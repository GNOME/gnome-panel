/*
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2003-2006 Vincent Untz
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
 */


#ifndef NA_APPLET_H
#define NA_APPLET_H

#include <libgnome-panel/gp-applet.h>

G_BEGIN_DECLS

#define NA_TYPE_APPLET na_applet_get_type ()
G_DECLARE_FINAL_TYPE (NaApplet, na_applet, NA, APPLET, GpApplet)

G_END_DECLS

#endif
