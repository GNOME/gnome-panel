/*global type definitions*/
#ifndef PANEL_TYPES_H
#define PANEL_TYPES_H

#include "gnome-panel.h"

typedef GNOME_Panel_OrientType PanelOrientType;
#define ORIENT_UP GNOME_Panel_ORIENT_UP
#define ORIENT_DOWN GNOME_Panel_ORIENT_DOWN
#define ORIENT_LEFT GNOME_Panel_ORIENT_LEFT
#define ORIENT_RIGHT GNOME_Panel_ORIENT_RIGHT

typedef GNOME_Panel_BackType PanelBackType;
#define PANEL_BACK_NONE GNOME_Panel_BACK_NONE
#define PANEL_BACK_COLOR GNOME_Panel_BACK_COLOR
#define PANEL_BACK_PIXMAP GNOME_Panel_BACK_PIXMAP

typedef enum {
	LAUNCHER_TILE=0,
	DRAWER_TILE,
	MENU_TILE,
	LOGOUT_TILE,
	LAST_TILE
} PanelTileType;

#endif
