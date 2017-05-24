/*
 * panel-enums.h:
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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

 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_ENUMS_H__
#define __PANEL_ENUMS_H__

#include <glib.h>
#include "panel-enums-gsettings.h"

G_BEGIN_DECLS

#define PANEL_HORIZONTAL_MASK (PANEL_ORIENTATION_TOP  | PANEL_ORIENTATION_BOTTOM)
#define PANEL_VERTICAL_MASK   (PANEL_ORIENTATION_LEFT | PANEL_ORIENTATION_RIGHT)

typedef enum {
	PANEL_OBJECT_MENU,
	PANEL_OBJECT_LAUNCHER,
	PANEL_OBJECT_APPLET,
	PANEL_OBJECT_ACTION,
	PANEL_OBJECT_MENU_BAR,
	PANEL_OBJECT_USER_MENU
} PanelObjectType;

typedef enum {
        PANEL_ACTION_NONE = 0,
        PANEL_ACTION_LOCK,
        PANEL_ACTION_LOGOUT,
        PANEL_ACTION_RUN,
        PANEL_ACTION_SEARCH,
	PANEL_ACTION_FORCE_QUIT,
	PANEL_ACTION_HIBERNATE,
	PANEL_ACTION_SUSPEND,
	PANEL_ACTION_HYBRID_SLEEP,
	PANEL_ACTION_REBOOT,
	PANEL_ACTION_SHUTDOWN,
        PANEL_ACTION_LAST
} PanelActionButtonType;

G_END_DECLS

#endif /* __PANEL_ENUMS_H__ */
