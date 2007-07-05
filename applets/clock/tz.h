/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Generic timezone utilities.
 *
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2004 Sun Microsystems, Inc.
 *
 * Authors: Hans Petter Jansson <hpj@ximian.com>
 *	    Erwann Chenede <erwann.chenede@sun.com>
 *	    Vincent Untz <vuntz@gnome.org>
 * 
 * Largely based on Michael Fulbright's work on Anaconda.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef _E_TZ_H
#define _E_TZ_H

#include <time.h>

typedef struct _TzDB TzDB;
typedef struct _TzLocation TzLocation;
typedef struct _TzInfo TzInfo;

/* see the glibc info page information on time zone information */
/*  tzname_normal    is the default name for the timezone */
/*  utc_offset       is offset in seconds from utc */
/*  daylight         if non-zero then location obeys daylight savings */

struct _TzInfo
{
	char *tzname_normal;
	glong utc_offset;
	int daylight;
};

void	    tz_ref_db                 (void);
void	    tz_unref_db               (void);

GPtrArray  *tz_get_locations          (void);
TzLocation *tz_get_location_by_name   (const char *name);

void       tz_location_get_position   (TzLocation *loc,
				       double     *longitude,
				       double     *latitude);
const char *tz_location_get_country    (TzLocation *loc);
const char *tz_location_get_zone       (TzLocation *loc);
#ifdef TZ_USE_COMMENT
const char *tz_location_get_comment    (TzLocation *loc);
#endif
glong      tz_location_get_utc_offset (TzLocation *loc);

TzInfo    *tz_info_from_location      (TzLocation *loc);
void       tz_info_free               (TzInfo *tz_info);

struct tm *tz_get_localtime_at	      (const char *zone,
				       const time_t *now);
char 	  *tz_get_system_timezone     (void);
#endif

