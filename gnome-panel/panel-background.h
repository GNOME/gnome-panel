/*
 * panel-background.h:
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_BACKGROUND_H__
#define __PANEL_BACKGROUND_H__

#include <glib.h>
#include <gtk/gtk.h>

#include "panel-types.h"
#include "panel-background-monitor.h"

typedef struct _PanelBackground PanelBackground;

struct _PanelBackground {
	PanelBackgroundType     type;

	PanelColor              color;
	char                   *image;
	GdkPixbuf              *loaded_image; 

	GtkOrientation          orientation;
	GdkRectangle            region;
	GdkPixbuf              *transformed_image;
	GdkPixbuf              *composited_image;

	PanelBackgroundMonitor *monitor;
	GdkPixbuf              *desktop;
	gulong                  monitor_signal;

	GdkPixmap              *pixmap;
	GdkWindow              *window;
	GdkColormap            *colormap;
	GdkGC                  *gc;

	GdkPixmap              *default_pixmap;
	GdkColor                default_color;

        guint                   fit_image : 1;
        guint                   stretch_image : 1;
        guint                   rotate_image : 1;

	guint                   has_alpha : 1;

	guint                   loaded : 1;
	guint                   transformed : 1;
	guint                   composited : 1;
	guint                   prepared : 1;
};

void     panel_background_init (PanelBackground     *background);
void     panel_background_free (PanelBackground     *background);

gboolean panel_background_set  (PanelBackground     *background,
				PanelBackgroundType  type,
				PanelColor          *color,
				const char          *image,
				gboolean             fit_image,
				gboolean             stretch_image,
				gboolean             rotate_image);

gboolean panel_background_set_none  (PanelBackground *background);
gboolean panel_background_set_color (PanelBackground *background,
				     PanelColor      *color);
gboolean panel_background_set_image (PanelBackground *background,
				     const char      *image,
				     gboolean         fit_image,
				     gboolean         stretch_image,
				     gboolean         rotate_image);

void panel_background_set_default_style (PanelBackground *background,
					 GdkColor        *color,
					 GdkPixmap       *pixmap);

void panel_background_realized   (PanelBackground *background,
				  GdkWindow       *window);
void panel_background_unrealized (PanelBackground *background);

void panel_background_change_region (PanelBackground *background,
				     GtkOrientation   orientation,
				     int              x,
				     int              y,
				     int              width,
				     int              height);

char *panel_background_make_string  (PanelBackground *background,
				     int              x,
				     int              y);

#endif /* __PANEL_BACKGROUND_H__ */
