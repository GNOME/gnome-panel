/*global type definitions*/
#ifndef PANEL_TYPES_H
#define PANEL_TYPES_H

#include <gtk/gtktypeutils.h>
#include "gnome-panel.h"

typedef GNOME_Panel_OrientType PanelOrientType;
#define ORIENT_UP GNOME_Panel_ORIENT_UP
#define ORIENT_DOWN GNOME_Panel_ORIENT_DOWN
#define ORIENT_LEFT GNOME_Panel_ORIENT_LEFT
#define ORIENT_RIGHT GNOME_Panel_ORIENT_RIGHT

enum {
	SIZE_ULTRA_TINY = 12,
	SIZE_TINY = 24,
	SIZE_SMALL = 36,
	SIZE_STANDARD = 48,
	SIZE_LARGE = 64,
	SIZE_HUGE = 80,
	SIZE_RIDICULOUS = 128
};

typedef GNOME_Panel_BackType PanelBackType;
#define PANEL_BACK_NONE GNOME_Panel_BACK_NONE
#define PANEL_BACK_COLOR GNOME_Panel_BACK_COLOR
#define PANEL_BACK_PIXMAP GNOME_Panel_BACK_PIXMAP
#define PANEL_BACK_TRANSLUCENT GNOME_Panel_BACK_TRANSLUCENT



typedef enum {
	LAUNCHER_TILE=0,
	DRAWER_TILE,
	MENU_TILE,
	MISC_TILE,
	LAST_TILE
} PanelTileType;
/* XXX: if you add any here you need to make the tile type larger
 * for button-widget, as it's 2 bits only for now */

typedef enum {
	EDGE_PANEL,
	DRAWER_PANEL,
	ALIGNED_PANEL,
	SLIDING_PANEL,
	FLOATING_PANEL,
	FOOBAR_PANEL
} PanelType;

typedef enum {
	BORDER_TOP,
	BORDER_RIGHT,
	BORDER_BOTTOM,
	BORDER_LEFT
} BorderEdge;

#endif
