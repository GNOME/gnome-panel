/*
 * clock-utils.h
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      Vincent Untz <vuntz@gnome.org>
 *
 * Most of the original code comes from clock.c
 */

#ifndef __CLOCK_UTILS_H__
#define __CLOCK_UTILS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Needs to match the indices in the combo of the prefs dialog */
typedef enum {
	CLOCK_FORMAT_INVALID = 0,
	CLOCK_FORMAT_12,
	CLOCK_FORMAT_24,
	CLOCK_FORMAT_UNIX,
	CLOCK_FORMAT_INTERNET,
	CLOCK_FORMAT_CUSTOM
} ClockFormat;

gboolean clock_locale_supports_am_pm (void);
ClockFormat clock_locale_format (void);

void clock_utils_display_help (GtkWidget  *widget,
			       const char *doc_id,
			       const char *link_id);

G_END_DECLS

#endif /* __CLOCK_UTILS_H__ */
