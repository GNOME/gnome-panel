/*global type definitions*/
#ifndef PANEL_TYPES_H
#define PANEL_TYPES_H

typedef enum {
	ORIENT_UP=0,
	ORIENT_DOWN,
	ORIENT_LEFT,
	ORIENT_RIGHT
} PanelOrientType;

typedef enum {
	SIZE_TINY=0,
	SIZE_STANDARD,
	SIZE_LARGE,
	SIZE_HUGE
} PanelSizeType;

typedef enum {
	LAUNCHER_TILE=0,
	DRAWER_TILE,
	MENU_TILE,
	MISC_TILE,
	LAST_TILE
} PanelTileType;

#endif
