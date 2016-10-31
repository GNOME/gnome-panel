/*
 * Copyright (C) 2002 Red Hat, Inc.
 * Developed by Havoc Pennington
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

#ifndef SHOW_DESKTOP_APPLET_H
#define SHOW_DESKTOP_APPLET_H

#include <libgnome-panel/gp-applet.h>

G_BEGIN_DECLS

#define SHOW_DESKTOP_TYPE_APPLET show_desktop_applet_get_type ()
G_DECLARE_FINAL_TYPE (ShowDesktopApplet, show_desktop_applet,
                      SHOW_DESKTOP, APPLET, GpApplet)

G_END_DECLS

#endif
