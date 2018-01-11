/*
 * clock-utils.c
 *
 * Copyright (C) 2007 Vincent Untz <vuntz@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Vincent Untz <vuntz@gnome.org>
 *
 * Most of the original code comes from clock.c
 */

#include "config.h"

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "clock-applet.h"
#include "clock-utils.h"

gboolean
clock_locale_supports_am_pm (void)
{
#ifdef HAVE_NL_LANGINFO
        const char *am;

        am = nl_langinfo (AM_STR);
        return (am[0] != '\0');
#else
	return TRUE;
#endif
}

GDesktopClockFormat
clock_locale_format (void)
{
	return clock_locale_supports_am_pm () ?
		G_DESKTOP_CLOCK_FORMAT_12H :
		G_DESKTOP_CLOCK_FORMAT_24H;
}
