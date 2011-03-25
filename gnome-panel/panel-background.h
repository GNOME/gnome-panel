/*
 * panel-background.h: panel background rendering
 *
 * Copyright (C) 2002, 2003 Sun Microsystems, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_BACKGROUND_H__
#define __PANEL_BACKGROUND_H__

#include <glib.h>
#include <gtk/gtk.h>

#include "panel-enums.h"
#include "panel-types.h"
#include "panel-background-monitor.h"

typedef struct _PanelBackground PanelBackground;

typedef void (*PanelBackgroundChangedNotify) 
				(PanelBackground *background,
				 gpointer         user_data);

struct _PanelBackground {
	GSettings              *settings;

	PanelBackgroundType     type;

	PanelBackgroundChangedNotify notify_changed;
	gpointer                user_data;

	GdkRGBA                 color;
	char                   *image;
	GdkPixbuf              *loaded_image; 

	GtkOrientation          orientation;
	GdkRectangle            region;
        GdkPixbuf              *transformed_image;
	cairo_pattern_t        *composited_pattern;

	PanelBackgroundMonitor *monitor;
	GdkPixbuf              *desktop;
	gulong                  monitor_signal;

	GdkWindow              *window;

	cairo_pattern_t        *default_pattern;
	GdkRGBA                 default_color;

        PanelBackgroundImageStyle style_image;
        guint                   rotate_image : 1;

	guint                   has_alpha : 1;

	guint                   loaded : 1;
	guint                   transformed : 1;
	guint                   composited : 1;
};

void  panel_background_init              (PanelBackground     *background,
					  PanelBackgroundChangedNotify notify_changed,
					  gpointer             user_data);
void  panel_background_settings_init     (PanelBackground     *background,
					  GSettings           *settings);
void  panel_background_free              (PanelBackground     *background);

void  panel_background_set_default_style (PanelBackground     *background,
					  GdkRGBA             *color,
					  cairo_pattern_t     *pattern);
void  panel_background_realized          (PanelBackground     *background,
					  GdkWindow           *window);
void  panel_background_unrealized        (PanelBackground     *background);
void  panel_background_change_region     (PanelBackground     *background,
					  GtkOrientation       orientation,
					  int                  x,
					  int                  y,
					  int                  width,
					  int                  height);
char *panel_background_make_string       (PanelBackground     *background,
					  int                  x,
					  int                  y);

PanelBackgroundType  panel_background_get_type   (PanelBackground *background);
const GdkRGBA       *panel_background_get_color  (PanelBackground *background);

PanelBackgroundType
      panel_background_effective_type    (PanelBackground     *background);

void panel_background_change_background_on_widget (PanelBackground *background,
						   GtkWidget       *widget);

#endif /* __PANEL_BACKGROUND_H__ */
