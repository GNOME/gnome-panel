/*
 * Copyright (C) 2001 Alexander Larsson
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
 * Authors: Alexander Larsson
 */

#ifndef WORKSPACE_SWITCHER_APPLET_H
#define WORKSPACE_SWITCHER_APPLET_H

#include <libgnome-panel/gp-applet.h>

G_BEGIN_DECLS

#define WORKSPACE_SWITCHER_TYPE_APPLET workspace_switcher_applet_get_type ()
G_DECLARE_FINAL_TYPE (WorkspaceSwitcherApplet, workspace_switcher_applet,
                      WORKSPACE_SWITCHER, APPLET, GpApplet)

G_END_DECLS

#endif
