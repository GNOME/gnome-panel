/*global type definitions*/
#ifndef PANEL_TYPES_H
#define PANEL_TYPES_H

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "GNOME_Panel.h"

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
	PANEL_SPEED_SLOW,
	PANEL_SPEED_MEDIUM,
	PANEL_SPEED_FAST
} PanelSpeed;

typedef struct {
	GdkColor gdk;
	guint16  alpha;
} PanelColor;

#endif
