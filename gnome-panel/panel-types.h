/*global type definitions*/
#ifndef PANEL_TYPES_H
#define PANEL_TYPES_H

#include <gtk/gtktypeutils.h>
#include "GNOME_Panel.h"

typedef GNOME_Vertigo_PanelOrient PanelOrient;

#define PANEL_ORIENT_UP    GNOME_Vertigo_PANEL_ORIENT_UP
#define PANEL_ORIENT_DOWN  GNOME_Vertigo_PANEL_ORIENT_DOWN
#define PANEL_ORIENT_LEFT  GNOME_Vertigo_PANEL_ORIENT_LEFT
#define PANEL_ORIENT_RIGHT GNOME_Vertigo_PANEL_ORIENT_RIGHT

typedef enum {
	PANEL_SIZE_XX_SMALL = GNOME_Vertigo_PANEL_XX_SMALL,
	PANEL_SIZE_X_SMALL  = GNOME_Vertigo_PANEL_X_SMALL,
	PANEL_SIZE_SMALL    = GNOME_Vertigo_PANEL_SMALL,
	PANEL_SIZE_MEDIUM   = GNOME_Vertigo_PANEL_MEDIUM,
	PANEL_SIZE_LARGE    = GNOME_Vertigo_PANEL_LARGE,
	PANEL_SIZE_X_LARGE  = GNOME_Vertigo_PANEL_X_LARGE,
	PANEL_SIZE_XX_LARGE = GNOME_Vertigo_PANEL_XX_LARGE 
} PanelSize;

typedef enum {
	PANEL_BACK_NONE,
	PANEL_BACK_COLOR,
	PANEL_BACK_PIXMAP
} PanelBackType;

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

typedef enum {
	PANEL_SPEED_SLOW,
	PANEL_SPEED_MEDIUM,
	PANEL_SPEED_FAST
} PanelSpeed;

#endif
