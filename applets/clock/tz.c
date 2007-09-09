/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Generic timezone utilities.
 *
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2004 Sun Microsystems, Inc.
 *
 * Authors: Hans Petter Jansson <hpj@ximian.com>
 *	    additional functions by Erwann Chenede <erwann.chenede@sun.com>
 *	    reworked by Vincent Untz <vuntz@gnome.org>
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

#include <config.h>

#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "tz.h"

#ifdef HAVE_SOLARIS
#define TZ_DATA_FILE "/usr/share/lib/zoneinfo/tab/zone_sun.tab"
#define ZONE_DIR     "/usr/share/lib/zoneinfo"
#define SYS_TZ_FILE  "/etc/TIMEZONE"
#else
#define TZ_DATA_FILE "/usr/share/zoneinfo/zone.tab"
#define ZONE_DIR     "/usr/share/zoneinfo"
#endif

struct _TzDB
{
        int ref;
	GPtrArray *locations;
};

struct _TzLocation
{
	char *country;
	gdouble latitude;
	gdouble longitude;
	char *zone;
#ifdef TZ_USE_COMMENT
	char *comment;
#endif
};

extern char **environ;

static TzDB *global_tz_db = NULL;

static TzDB *tz_build_db (void);
static void  tz_free_db  (void);
static void  tz_location_free (TzLocation *tz);

static float
convert_pos (char *pos,
             int   digits)
{
  char whole[10];
  char *fraction;
  int i;
  float t1, t2;

  if (!pos || strlen (pos) < 4 || digits > 9)
    return 0.0;

  for (i = 0; i < digits + 1; i++)
    whole[i] = pos[i];
  whole[i] = '\0';
  fraction = pos + digits + 1;

  t1 = g_strtod (whole, NULL);
  t2 = g_strtod (fraction, NULL);

  if (t1 >= 0.0)
    return t1 + t2 / pow (10.0, strlen (fraction));
  else
    return t1 - t2 / pow (10.0, strlen (fraction));
}

static int
tz_location_cmp_zone (gconstpointer a,
                      gconstpointer b)
{
  const TzLocation *tza = *(TzLocation **) a;
  const TzLocation *tzb = *(TzLocation **) b;

  return strcmp (tza->zone, tzb->zone);
}

/* ---------------- *
 * Public interface *
 * ---------------- */

void
tz_ref_db (void)
{
  if (!global_tz_db) 
    {
      tz_build_db ();
      if (global_tz_db)
        global_tz_db->ref = 1;
    }
  else
    global_tz_db->ref++;
}

void
tz_unref_db (void)
{
  if (!global_tz_db)
    return;

  global_tz_db->ref--;

  if (global_tz_db->ref <= 0)
    tz_free_db ();
}

static void
tz_free_db (void)
{
  int i;

  if (!global_tz_db)
    return;

  for (i = 0; i < global_tz_db->locations->len; i++)
    {
      TzLocation *loc = g_ptr_array_index (global_tz_db->locations, i);
      tz_location_free (loc);
    }
  g_ptr_array_free (global_tz_db->locations, FALSE);

  g_free (global_tz_db);

  global_tz_db = NULL;
}

static TzDB *
tz_build_db (void)
{
  FILE *tzfile;
  char buf[4096];

  g_assert (global_tz_db == NULL);

  global_tz_db = g_new0 (TzDB, 1);
  global_tz_db->locations = g_ptr_array_new ();

  tzfile = g_fopen (TZ_DATA_FILE, "r");
  if (!tzfile)
    {
      g_warning ("Cannot open \"%s\".\n", TZ_DATA_FILE);
      return global_tz_db;
    }

  while (fgets (buf, sizeof (buf), tzfile))
    {
      char **tmpstrarr;
      char *latstr, *lngstr, *p;
      TzLocation *loc;

      if (buf[0] == '#')
	continue;

      g_strchomp (buf);
      tmpstrarr = g_strsplit (buf, "\t", 6);
      if (!tmpstrarr[0] || !tmpstrarr[1] || !tmpstrarr[2])
        {
          g_strfreev (tmpstrarr);
          continue;
        }

      latstr = g_strdup (tmpstrarr[1]);
      p = latstr + 1;
      while (*p != '-' && *p != '+' && *p)
	p++;
      lngstr = g_strdup (p);
      *p = '\0';

      loc = g_slice_new (TzLocation);
      loc->country = g_strdup (tmpstrarr[0]);
      loc->zone = g_strdup (tmpstrarr[2]);
      loc->latitude = convert_pos (latstr, 2);
      loc->longitude = convert_pos (lngstr, 3);
#ifdef HAVE_SOLARIS
      if (tmpstrarr[3] && *tmpstrarr[3] == '-')
#ifdef TZ_USE_COMMENT
        loc->comment = tmpstrarr[4] ? g_strdup (tmpstrarr[4]) : NULL;
#else
        ;
#endif
      else if (tmpstrarr[3] && *tmpstrarr[3] != '-' &&
               !g_ascii_islower (loc->zone[0]))
        {
          TzLocation *locgrp;

          /* alias entry */
          locgrp = g_slice_new (TzLocation);
          locgrp->country = g_strdup (tmpstrarr[0]);
          locgrp->zone = g_strdup (tmpstrarr[3]);
          locgrp->latitude  = convert_pos (latstr, 2);
          locgrp->longitude = convert_pos (lngstr, 3);
#ifdef TZ_USE_COMMENT
          locgrp->comment = tmpstrarr[4] ? g_strdup (tmpstrarr[4]) : NULL;
#endif

          g_ptr_array_add (global_tz_db->locations, locgrp);
        }
#ifdef TZ_USE_COMMENT
      else
        loc->comment = NULL;
#endif
#else
#ifdef TZ_USE_COMMENT
      loc->comment = (tmpstrarr[3]) ? g_strdup (tmpstrarr[3]) : NULL;
#endif
#endif

      g_ptr_array_add (global_tz_db->locations, loc);

      g_free (latstr);
      g_free (lngstr);
      g_strfreev (tmpstrarr);
    }

  fclose (tzfile);

  /* now sort by country */
  g_ptr_array_sort (global_tz_db->locations, tz_location_cmp_zone);

  return global_tz_db;
}

static void
tz_location_free (TzLocation *tz)
{

  if (tz->country)
    g_free (tz->country);
  if (tz->zone)
    g_free (tz->zone);
#ifdef TZ_USE_COMMENT
  if (tz->comment)
    g_free (tz->comment);
#endif

  g_slice_free (TzLocation, tz);
}

GPtrArray *
tz_get_locations (void)
{
  g_return_val_if_fail (global_tz_db != NULL, NULL);

  return global_tz_db->locations;
}

const char *
tz_location_get_country (TzLocation *loc)
{
  return loc->country;
}

const char *
tz_location_get_zone (TzLocation *loc)
{
  return loc->zone;
}

#ifdef TZ_USE_COMMENT
const char *
tz_location_get_comment (TzLocation *loc)
{
  return loc->comment;
}
#endif

void
tz_location_get_position (TzLocation *loc,
                          double     *longitude,
			  double     *latitude)
{
  *longitude = loc->longitude;
  *latitude = loc->latitude;
}

glong
tz_location_get_utc_offset (TzLocation *loc)
{
  TzInfo *tz_info;
  glong   offset;

  tz_info = tz_info_from_location (loc);
  offset = tz_info->utc_offset;
  tz_info_free (tz_info);

  return offset;
}

void
tz_info_free (TzInfo *tzinfo)
{
  g_return_if_fail (tzinfo != NULL);

  if (tzinfo->tzname_normal)
    g_free (tzinfo->tzname_normal);
  tzinfo->tzname_normal = NULL;

  g_slice_free (TzInfo, tzinfo);
}

TzInfo *
tz_info_from_location (TzLocation *loc)
{
  TzInfo    *tzinfo;
  time_t     curtime;
  struct tm *curzone;

  g_return_val_if_fail (loc != NULL, NULL);
  g_return_val_if_fail (loc->zone != NULL, NULL);

  tzinfo = g_slice_new0 (TzInfo);

  curtime = time (NULL);
  curzone = tz_get_localtime_at (loc->zone, &curtime);

#ifdef HAVE_SOLARIS
  tzinfo->tzname_normal = g_strdup (loc->zone);
  tzinfo->utc_offset = 0;
#else
  tzinfo->tzname_normal = g_strdup (curzone->tm_zone);
  tzinfo->utc_offset = curzone->tm_gmtoff;
#endif

  tzinfo->daylight = curzone->tm_isdst;

  return tzinfo;
}

#ifdef HAVE_SOLARIS		  
char *
tz_get_system_timezone_solaris (void)
{
  char *contents;
  char *tz_token;
  char *retval;
  char *tmpfilebuf;

  if (!g_file_get_contents (SYS_TZ_FILE, contents, -1, NULL))
    return NULL;

  retval = NULL;

  tz_token = g_strrstr (tmpfilebuf, "TZ=");
  if (tz_token)
    {
      char **toks;

      toks = g_strsplit (tz_token, "\n", 2);
      if (toks[0])
        {
          retval = g_strdup (toks[0] + strlen ("TZ="));
        }
      g_strfreev (toks);
    }

  g_free (contents);

  return retval;
}
#endif

/* This tries to get the system timezone from:
 *  + TZ environment variable
 *  + OS specific stuff
 *  + /etc/timezone
 *  + /etc/localtime
 * If everything fails, it returns UTC.
 */
char *
tz_get_system_timezone (void)
{
  const char  *const_tz;
  FILE        *etc_timezone;
  char        *tz;
  struct stat  stat_localtime;
  struct stat  stat_buf;

  //TODO make sure this returns something valid
  const_tz = g_getenv ("TZ");
  if (const_tz && const_tz[0] != '\0')
    return g_strdup (const_tz);

#ifdef HAVE_SOLARIS		  
  tz = tz_get_system_timezone_solaris ();
  if (tz)
    return tz;
#endif

  etc_timezone = g_fopen ("/etc/timezone", "r");
  if (etc_timezone)
    {
      GString *reading;
      int      c;

      reading = g_string_new ("");

      c = fgetc (etc_timezone);
      while (c != EOF && (g_ascii_isalnum (c) || c == '/' || c == '-' || c == '_'))
        {
          reading = g_string_append_c (reading, c);
          c = fgetc (etc_timezone);
        }

      fclose (etc_timezone);

      if (reading->str && reading->str[0] != '\0')
        return g_string_free (reading, FALSE);
      else
        g_string_free (reading, TRUE);
    }

  if (g_file_test ("/etc/localtime", G_FILE_TEST_IS_SYMLINK))
    {
      char *file;

      file = g_file_read_link ("/etc/localtime", NULL);
      if (strncmp (file, ZONE_DIR"/", strlen (ZONE_DIR"/")))
        tz = g_strdup (file + strlen (ZONE_DIR"/"));
      g_free (file);

      if (tz && tz[0] != '\0')
        return tz;

      g_free (tz);
    }

  if (g_stat ("/etc/localtime", &stat_localtime) == 0)
    {
      GPtrArray *locs;
      TzLocation *tz_loc;
      char *filename;
      int i;
      int fd;
      char *localtime_content;
      char *buf_content;

      locs = tz_get_locations ();

      tz_loc = NULL;

      /* try to locate by inode */
      for (i = 0; i < locs->len; i++)
        {
          tz_loc = g_ptr_array_index (locs, i);

          filename = g_build_filename (ZONE_DIR, tz_loc->zone, NULL);
          if (g_stat (filename, &stat_buf) != 0)
            {
              g_free (filename);
              continue;
            }
          g_free (filename);

          /* same inode => hardlink */
          if (stat_localtime.st_ino == stat_buf.st_ino)
            break;
        }

      if (i < locs->len)
        {
          g_assert (tz_loc != NULL);
          return g_strdup (tz_loc->zone);
        }

      /* try to locate by content */
      localtime_content = g_slice_alloc (stat_localtime.st_size);
      buf_content = g_slice_alloc (stat_localtime.st_size);

      fd = g_open ("/etc/localtime", O_RDONLY, 0);
      read (fd, localtime_content, stat_localtime.st_size);
      close (fd);

      for (i = 0; i < locs->len; i++)
        {
          tz_loc = g_ptr_array_index (locs, i);

          filename = g_build_filename (ZONE_DIR, tz_loc->zone, NULL);
          if (g_stat (filename, &stat_buf) != 0)
            {
              g_free (filename);
              continue;
            }

          if (stat_localtime.st_size != stat_buf.st_size)
            {
              g_free (filename);
              continue;
            }

          fd = g_open (filename, O_RDONLY, 0);
          read (fd, buf_content, stat_localtime.st_size);
          close (fd);

          g_free (filename);

          if (memcmp (localtime_content, buf_content, stat_localtime.st_size) == 0)
            break;
        }

      g_slice_free1 (stat_localtime.st_size, localtime_content);
      g_slice_free1 (stat_localtime.st_size, buf_content);

      if (i < locs->len)
        {
          g_assert (tz_loc != NULL);
          return g_strdup (tz_loc->zone);
        }
    }

  return g_strdup ("UTC");
}

TzLocation *
tz_get_location_by_name (const char *name)
{
  GPtrArray *locs;
  int i;

  g_return_val_if_fail (name != NULL, NULL);

  locs = tz_get_locations ();

  for (i = 0; i < locs->len; i++)
    {
      TzLocation *tz_loc;

      tz_loc = g_ptr_array_index (locs, i);

      if (tz_loc && !g_utf8_collate (tz_location_get_zone (tz_loc), name))
        return tz_loc;
    }

  return NULL;
}

struct tm *
tz_get_localtime_at (const char   *tz,
		     const time_t *now)
{
        char      **environ_old;
        char      **envp;
        struct tm  *retval;
        int         i, env_len, tz_index;
                                                                                
        tz_index = -1;
        for (env_len = 0; environ [env_len]; env_len++)
                if (!strncmp (environ [env_len], "TZ=", strlen ("TZ=")))
                        tz_index = env_len;
                                                                                
        if (tz_index == -1)
                tz_index = env_len++;
                                                                                
        envp = g_new0 (char *, env_len + 1);
                                                                                
        for (i = 0; i < env_len; i++)
                if (i == tz_index)
                        envp [i] = g_strconcat ("TZ=", tz, NULL);
                else
                        envp [i] = g_strdup (environ [i]);
                                                                                
        environ_old = environ;
        environ = envp;

        retval = localtime (now);

        environ = environ_old;
        g_strfreev (envp);
                                                                                
        return retval;
}
