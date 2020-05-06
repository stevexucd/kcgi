/*	$Id$ */
/*
 * Copyright (c) 2016, 2017, 2020 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kcgi.h"
#include "extern.h"

/* 
 * The following code is from newlib and is licensed as follows:
 *
 * Copyright (c) 1994-2009  Red Hat, Inc. All rights reserved.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the BSD License.   This program is distributed in the hope that
 * it will be useful, but WITHOUT ANY WARRANTY expressed or implied,
 * including the implied warranties of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  A copy of this license is available at
 * http://www.opensource.org/licenses. Any Red Hat trademarks that are
 * incorporated in the source code or documentation are not subject to
 * the BSD License and may only be used or replicated with the express
 * permission of Red Hat, Inc.
 */
#define _SEC_IN_MINUTE 60L
#define _SEC_IN_HOUR 3600L
#define _SEC_IN_DAY 86400L

static const int DAYS_IN_MONTH[12] =
	{31, 28, 31, 30, 31, 30, 
	 31, 31, 30, 31, 30, 31};

#define _DAYS_IN_MONTH(x) \
	((x == 1) ? days_in_feb : DAYS_IN_MONTH[x])

static const int _DAYS_BEFORE_MONTH[12] =
	{0, 31, 59, 90, 120, 151, 
	 181, 212, 243, 273, 304, 334};

#define _ISLEAP(y) \
	(((y) % 4) == 0 && \
	 (((y) % 100) != 0 || (((y)+1900) % 400) == 0))
#define _DAYS_IN_YEAR(year) \
	(_ISLEAP(year) ? 366 : 365)

static void 
khttp_validate_time(struct tm *tim_p)
{
	div_t	 res;
	int 	 days_in_feb = 28;

	if (tim_p->tm_sec < 0 || tim_p->tm_sec > 59) {
		res = div (tim_p->tm_sec, 60);
		tim_p->tm_min += res.quot;
		if ((tim_p->tm_sec = res.rem) < 0) {
			tim_p->tm_sec += 60;
			--tim_p->tm_min;
		}
	}

	if (tim_p->tm_min < 0 || tim_p->tm_min > 59) {
		res = div (tim_p->tm_min, 60);
		tim_p->tm_hour += res.quot;
		if ((tim_p->tm_min = res.rem) < 0) {
			tim_p->tm_min += 60;
			--tim_p->tm_hour;
		}
	}

	if (tim_p->tm_hour < 0 || tim_p->tm_hour > 23) {
		res = div (tim_p->tm_hour, 24);
		tim_p->tm_mday += res.quot;
		if ((tim_p->tm_hour = res.rem) < 0) {
			tim_p->tm_hour += 24;
			--tim_p->tm_mday;
		}
	}

	if (tim_p->tm_mon < 0 || tim_p->tm_mon > 11) {
		res = div (tim_p->tm_mon, 12);
		tim_p->tm_year += res.quot;
		if ((tim_p->tm_mon = res.rem) < 0) {
			tim_p->tm_mon += 12;
			--tim_p->tm_year;
		}
	}

	if (_DAYS_IN_YEAR (tim_p->tm_year) == 366)
		days_in_feb = 29;

	if (tim_p->tm_mday <= 0) {
		while (tim_p->tm_mday <= 0) {
			if (--tim_p->tm_mon == -1) {
				tim_p->tm_year--;
				tim_p->tm_mon = 11;
				days_in_feb =
					((_DAYS_IN_YEAR (tim_p->tm_year) == 366) ?
					 29 : 28);
			}
			tim_p->tm_mday += _DAYS_IN_MONTH (tim_p->tm_mon);
		}
	} else {
		while (tim_p->tm_mday > _DAYS_IN_MONTH (tim_p->tm_mon)) {
			tim_p->tm_mday -= _DAYS_IN_MONTH (tim_p->tm_mon);
			if (++tim_p->tm_mon == 12) {
				tim_p->tm_year++;
				tim_p->tm_mon = 0;
				days_in_feb =
					((_DAYS_IN_YEAR (tim_p->tm_year) == 366) ?
					 29 : 28);
			}
		}
	}
}

/*
 * See khttp_validate_time().
 */
static time_t 
khttp_mktime(struct tm *tim_p)
{
	time_t	tim = 0;
	long	days = 0;
	int	year;

	/* Validate structure. */

	khttp_validate_time(tim_p);

	/* Compute hours, minutes, seconds. */

	tim += tim_p->tm_sec + (tim_p->tm_min * _SEC_IN_MINUTE) +
		(tim_p->tm_hour * _SEC_IN_HOUR);

	/* Compute days in year. */

	days += tim_p->tm_mday - 1;
	days += _DAYS_BEFORE_MONTH[tim_p->tm_mon];

	if (tim_p->tm_mon > 1 && _DAYS_IN_YEAR(tim_p->tm_year) == 366)
		days++;

	/* Compute day of the year. */

	tim_p->tm_yday = days;

#if 0
	if (tim_p->tm_year > 10000 || tim_p->tm_year < -10000)
		return (time_t) -1;
#endif

	/* Compute days in other years. */

	if ((year = tim_p->tm_year) > 70) {
		for (year = 70; year < tim_p->tm_year; year++)
			days += _DAYS_IN_YEAR (year);
	} else if (year < 70) {
		for (year = 69; year > tim_p->tm_year; year--)
			days -= _DAYS_IN_YEAR(year);
		days -= _DAYS_IN_YEAR(year);
	}

	/* Compute total seconds. */

	tim += (time_t)days * _SEC_IN_DAY;

	/* Compute day of the week. */

	if ((tim_p->tm_wday = (days + 4) % 7) < 0)
		tim_p->tm_wday += 7;

	return tim;
}

/* 
 * Move epoch from 01.01.1970 to 01.03.0000 (yes, Year 0) - this is the
 * first day of a 400-year long "era", right after additional day of
 * leap year.  This adjustment is required only for date calculation, so
 * instead of modifying time_t value (which would require 64-bit
 * operations to work correctly) it's enough to adjust the calculated
 * number of days since epoch.
 */
#define EPOCH_ADJUSTMENT_DAYS	719468L

/* Year to which the adjustment was made. */
#define ADJUSTED_EPOCH_YEAR	0

/* 1st March of year 0 is Wednesday. */
#define ADJUSTED_EPOCH_WDAY	3

/* 
 * There are 97 leap years in 400-year periods. ((400 - 97) * 365 + 97 *
 * 366).
 */
#define DAYS_PER_ERA		146097L

/* 
 * There are 24 leap years in 100-year periods. ((100 - 24) * 365 + 24 *
 * 366).
 */
#define DAYS_PER_CENTURY	36524L

/* There is one leap year every 4 years. */
#define DAYS_PER_4_YEARS	(3 * 365 + 366)

/* Number of days in a non-leap year. */
#define DAYS_PER_YEAR		365

/* Number of days in January. */
#define DAYS_IN_JANUARY		31

/* Number of days in non-leap February. */
#define DAYS_IN_FEBRUARY	28

/* Number of years per era. */
#define YEARS_PER_ERA		400

/* Various constants. */
#define SECSPERMIN	60L
#define MINSPERHOUR	60L
#define HOURSPERDAY	24L
#define SECSPERHOUR	(SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY	(SECSPERHOUR * HOURSPERDAY)
#define YEAR_BASE	1900
#define DAYSPERWEEK	7
#define MONSPERYEAR	12

/* Self-explanatory. */
#define ISLEAP(y) 	((((y) % 4) == 0 && ((y) % 100) != 0) || \
			 ((y) % 400) == 0)

/*
 * Convert UNIX epoch to values in "res".
 * Never fails.
 */
static void
khttp_gmtime_r(int64_t lcltime, struct tm *res)
{
	int64_t		days, rem, era, weekday, year;
	uint64_t	erayear, yearday, month, day, eraday;

	memset(res, 0, sizeof(struct tm));

	days = lcltime / SECSPERDAY + EPOCH_ADJUSTMENT_DAYS;
	rem = lcltime % SECSPERDAY;

	if (rem < 0) {
		rem += SECSPERDAY;
		--days;
	}

	/* Compute hour, min, and sec. */

	res->tm_hour = (int)(rem / SECSPERHOUR);
	rem %= SECSPERHOUR;
	res->tm_min = (int)(rem / SECSPERMIN);
	res->tm_sec = (int)(rem % SECSPERMIN);

	/* Compute day of week. */

	if ((weekday = ((ADJUSTED_EPOCH_WDAY + days) % DAYSPERWEEK)) < 0)
		weekday += DAYSPERWEEK;

	res->tm_wday = weekday;

	/* 
	 * Compute year, month, day & day of year. 
	 * For description of this algorithm see
	 * http://howardhinnant.github.io/date_algorithms.html#civil_from_days
	 */

	era = (days >= 0 ? days : days - (DAYS_PER_ERA - 1)) / DAYS_PER_ERA;
	eraday = days - era * DAYS_PER_ERA;	/* [0, 146096] */
	erayear = (eraday - eraday / (DAYS_PER_4_YEARS - 1) + eraday / DAYS_PER_CENTURY -
			eraday / (DAYS_PER_ERA - 1)) / 365;	/* [0, 399] */
	yearday = eraday - (DAYS_PER_YEAR * erayear + erayear / 4 - erayear / 100);	/* [0, 365] */
	month = (5 * yearday + 2) / 153;	/* [0, 11] */
	day = yearday - (153 * month + 2) / 5 + 1;	/* [1, 31] */
	month += month < 10 ? 2 : -10;
	year = ADJUSTED_EPOCH_YEAR + erayear + era * YEARS_PER_ERA + (month <= 1);

	res->tm_yday = yearday >= DAYS_PER_YEAR - DAYS_IN_JANUARY - DAYS_IN_FEBRUARY ?
		yearday - (DAYS_PER_YEAR - DAYS_IN_JANUARY - DAYS_IN_FEBRUARY) :
		yearday + DAYS_IN_JANUARY + DAYS_IN_FEBRUARY + ISLEAP(erayear);
	res->tm_year = year - YEAR_BASE;
	res->tm_mon = month;
	res->tm_mday = day;
	res->tm_isdst = 0;
}

char *
khttp_epoch2str(int64_t tt, char *buf, size_t sz)
{
	struct tm	 tm;
	char		 rbuf[64];

	if (buf == NULL || sz == 0)
		return NULL;

	khttp_gmtime_r(tt, &tm);

	if (strftime(rbuf, sizeof(rbuf),
	    "%a, %d %b %Y %T GMT", &tm) == 0) {
		XWARNX("strftime");
		return NULL;
	}

	strlcpy(buf, rbuf, sz);
	return buf;
}

/*
 * Deprecated interface simply zeroes the time structure if it's
 * negative.  The original implementation did not do this (it set the
 * "struct tm" to zero), but the documentation still said otherwise.
 * This uses the original documented behaviour.
 */
char *
kutil_epoch2str(int64_t tt, char *buf, size_t sz)
{

	return khttp_epoch2str(tt < 0 ? 0 : tt, buf, sz);
}

char *
khttp_epoch2ustr(int64_t tt, char *buf, size_t sz)
{
	char		 rbuf[64];
	struct tm	 tm;

	if (buf == NULL || sz == 0)
		return NULL;

	khttp_gmtime_r(tt, &tm);

	if (snprintf(rbuf, sizeof(rbuf), 
	    "%.4d-%.2d-%.2dT%.2d:%.2d:%.2dZ",
	    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, 
	    tm.tm_hour, tm.tm_min, tm.tm_sec) == -1) {
		XWARNX("snprintf");
		return NULL;
	}

	strlcpy(buf, rbuf, sz);
	return buf;
}

/*
 * Deprecated.
 * See kutil_epoch2str() for behaviour notes.
 */
char *
kutil_epoch2utcstr(int64_t tt, char *buf, size_t sz)
{

	return khttp_epoch2ustr(tt < 0 ? 0 : tt, buf, sz);
}

int
khttp_epoch2tms(int64_t tt, int *tm_sec, int *tm_min, 
	int *tm_hour, int *tm_mday, int *tm_mon, 
	int *tm_year, int *tm_wday, int *tm_yday)
{
	struct tm	tm;

	khttp_gmtime_r(tt, &tm);

	if (tm_sec != NULL)
		*tm_sec = tm.tm_sec;
	if (tm_min != NULL)
		*tm_min = tm.tm_min;
	if (tm_hour != NULL)
		*tm_hour = tm.tm_hour;
	if (tm_mday != NULL)
		*tm_mday = tm.tm_mday;
	if (tm_mon != NULL)
		*tm_mon = tm.tm_mon;
	if (tm_year != NULL)
		*tm_year = tm.tm_year;
	if (tm_wday != NULL)
		*tm_wday = tm.tm_wday;
	if (tm_yday != NULL)
		*tm_yday = tm.tm_yday;

	return 1;
}

/*
 * Deprecated.
 * This has a bad corner case where gmtime() returns NULL and we just
 * assume the epoch---this wasn't present in the earlier version of this
 * because it was using a hand-rolled gmtime() that didn't fail.  This
 * is suitably unlikely that it's ok, as it's deprecated.
 */
void
kutil_epoch2tmvals(int64_t tt, int *tm_sec, int *tm_min, 
	int *tm_hour, int *tm_mday, int *tm_mon, 
	int *tm_year, int *tm_wday, int *tm_yday)
{
	struct tm	 tm;

	khttp_gmtime_r(tt < 0 ? 0 : tt, &tm);

	if (tm_sec != NULL)
		*tm_sec = tm.tm_sec;
	if (tm_min != NULL)
		*tm_min = tm.tm_min;
	if (tm_hour != NULL)
		*tm_hour = tm.tm_hour;
	if (tm_mday != NULL)
		*tm_mday = tm.tm_mday;
	if (tm_mon != NULL)
		*tm_mon = tm.tm_mon;
	if (tm_year != NULL)
		*tm_year = tm.tm_year;
	if (tm_wday != NULL)
		*tm_wday = tm.tm_wday;
	if (tm_yday != NULL)
		*tm_yday = tm.tm_yday;
}

static int
khttp_int_truncate(int64_t src, int *dst)
{

	if (src > INT_MAX || src < INT_MIN) {
		XWARNX("date conversion integer over/underflow");
		return 0;
	}
	*dst = src;
	return 1;
}

int
khttp_datetime2epoch(int64_t *res, int64_t day, int64_t mon,
	int64_t year, int64_t hour, int64_t min, int64_t sec)
{
	struct tm	 tm, test;
	int64_t		 val;

	if (res == NULL)
		res = &val;

	memset(&tm, 0, sizeof(struct tm));

	/* 
	 * Narrow these to integers to make khttp_mktime() less diverged
	 * from its origins.
	 * This is fine, as all of these need to be sane to start with,
	 * except we won't handle 64-bit years.
	 * No loss there.
	 */

	if (!khttp_int_truncate(sec, &tm.tm_sec))
		return 0;
	if (!khttp_int_truncate(min, &tm.tm_min))
		return 0;
	if (!khttp_int_truncate(hour, &tm.tm_hour))
		return 0;
	if (!khttp_int_truncate(day, &tm.tm_mday))
		return 0;
	if (!khttp_int_truncate(mon - 1, &tm.tm_mon))
		return 0;
	if (!khttp_int_truncate(year - 1900, &tm.tm_year))
		return 0;

	test = tm;

	*res = khttp_mktime(&tm);

	/* If we normalised any values, this will catch them. */

	if (test.tm_sec != tm.tm_sec ||
	    test.tm_min != tm.tm_min ||
	    test.tm_hour != tm.tm_hour ||
	    test.tm_mday != tm.tm_mday ||
	    test.tm_mon != tm.tm_mon ||
	    test.tm_year != tm.tm_year)
		return 0;

	return 1;
}

int
khttp_date2epoch(int64_t *res, int64_t day, int64_t mon, int64_t year)
{

	return khttp_datetime2epoch(res, day, mon, year, 0, 0, 0);
}

/*
 * Deprecated interface.
 */
int64_t
kutil_date2epoch(int64_t day, int64_t mon, int64_t year)
{
	int64_t	 res;

	if (!khttp_date2epoch(&res, day, mon, year))
		return -1;

	return res;
}

/*
 * Deprecated interface.
 */
int
kutil_datetime_check(int64_t day, int64_t mon, int64_t year,
	int64_t hour, int64_t min, int64_t sec)
{

	return khttp_datetime2epoch
		(NULL, day, mon, year, hour, min, sec);
}

/*
 * Deprecated interface.
 */
int
kutil_date_check(int64_t day, int64_t mon, int64_t year)
{

	return khttp_date2epoch(NULL, day, mon, year);
}

/*
 * Deprecated interface.
 */
int64_t
kutil_datetime2epoch(int64_t day, int64_t mon, int64_t year,
	int64_t hour, int64_t min, int64_t sec)
{
	int64_t	 res;

	if (!khttp_datetime2epoch
	    (&res, day, mon, year, hour, min, sec))
		return -1;

	return res;
}
