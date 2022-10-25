/*
 * panel-xdg.c: miscellaneous XDG-related functions.
 *
 * Copyright (C) 2010 Novell, Inc.
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
 *	Vincent Untz <vuntz@gnome.org>
 */

#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "panel-xdg.h"

/*
 * Originally based on code from panel-util.c. This part of the code was:
 * Copyright (C) 2006 Vincent Untz <vuntz@gnome.org>
 */

char *
panel_xdg_icon_remove_extension (const char *icon)
{
	char *icon_no_extension;
	char *p;

	icon_no_extension = g_strdup (icon);
	p = strrchr (icon_no_extension, '.');
	if (p &&
	    (strcmp (p, ".png") == 0 ||
	     strcmp (p, ".xpm") == 0 ||
	     strcmp (p, ".svg") == 0)) {
	    *p = 0;
	}

	return icon_no_extension;
}
