/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2001 Free Software Foundation, Inc.
 * Copyright (C) 2000 Helix Code, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 *      Glynn Foster <glynn.foster@sun.com>
 *      George Lebl <jirka@5z.com>
 *      Jacob Berkman <jacob@helixcode.com>
 */

#ifndef WINDOW_MENU_APPLET_H
#define WINDOW_MENU_APPLET_H

#include <libgnome-panel/gp-applet.h>

G_BEGIN_DECLS

#define WINDOW_MENU_TYPE_APPLET window_menu_applet_get_type ()
G_DECLARE_FINAL_TYPE (WindowMenuApplet, window_menu_applet,
                      WINDOW_MENU, APPLET, GpApplet)

G_END_DECLS

#endif
