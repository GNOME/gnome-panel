/*global type definitions*/
#ifndef PANEL_TYPES_H
#define PANEL_TYPES_H

typedef enum {
	ORIENT_UP,
	ORIENT_DOWN,
	ORIENT_LEFT,
	ORIENT_RIGHT
} PanelOrientType;

typedef enum {
	LAUNCHER_TILE=0,
	DRAWER_TILE,
	MENU_TILE,
	MISC_TILE,
	LAST_TILE
} PanelTileType;

#endif
