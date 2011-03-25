/*
 * panel-background.c: panel background rendering
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

#include <config.h>

#include "panel-background.h"

#include <string.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <cairo.h>
#include <cairo-xlib.h>

#include <libpanel-util/panel-glib.h>

#include "panel-background-monitor.h"
#include "panel-schemas.h"
#include "panel-util.h"


static gboolean panel_background_composite (PanelBackground *background);
static void load_background_file (PanelBackground *background);


static gboolean
panel_background_prepare (PanelBackground *background)
{
	PanelBackgroundType  effective_type;
	GtkWidget           *widget = NULL;

	if (!background->transformed)
		return FALSE;

	effective_type = panel_background_effective_type (background);

	switch (effective_type) {
	case PANEL_BACK_NONE:
                if (background->default_pattern)
                        gdk_window_set_background_pattern (background->window,
                                                          background->default_pattern);
		else
			gdk_window_set_background_rgba (
				background->window, &background->default_color);
		break;
	case PANEL_BACK_COLOR:
		if (background->has_alpha &&
		    background->composited_pattern)
			gdk_window_set_background_pattern (background->window,
							   background->composited_pattern);
		else
			gdk_window_set_background_rgba (background->window,
                                                        &background->color);
		break;
	case PANEL_BACK_IMAGE:
		g_assert (background->composited_pattern != NULL);
		gdk_window_set_background_pattern (background->window,
						   background->composited_pattern);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	/* Panel applets may use the panel's background pixmap to
	 * decide how to draw themselves.  Therefore, we need to
	 * make sure that all drawing has been completed before
	 * the applet looks at the pixmap. */
	gdk_display_sync (gdk_window_get_display (background->window));

	gdk_window_get_user_data (GDK_WINDOW (background->window),
				  (gpointer) &widget);

	if (GTK_IS_WIDGET (widget))
	  gtk_widget_queue_draw (widget);

	background->notify_changed (background, background->user_data);

	return TRUE;
}

static void
free_composited_resources (PanelBackground *background)
{
	background->composited = FALSE;

	if (background->composited_pattern)
		cairo_pattern_destroy (background->composited_pattern);
	background->composited_pattern = NULL;
}

static void
background_changed (PanelBackgroundMonitor *monitor,
		    PanelBackground        *background)
{
	GdkPixbuf *tmp;

	tmp = background->desktop;

	background->desktop = panel_background_monitor_get_region (
					background->monitor,
					background->region.x,
					background->region.y,
					background->region.width,
					background->region.height);

	if (tmp)
		g_object_unref (tmp);

	panel_background_composite (background);
}

// FIXMEchpe make this a cairo_pattern_t*
static GdkPixbuf *
get_desktop_pixbuf (PanelBackground *background)
{
	GdkPixbuf *desktop;

	if (!background->monitor) {
		background->monitor =
			panel_background_monitor_get_for_screen (
				gdk_window_get_screen (background->window));

		background->monitor_signal =
			g_signal_connect (
			background->monitor, "changed",
                        G_CALLBACK (background_changed), background);
	}

	desktop = panel_background_monitor_get_region (
				background->monitor,
				background->region.x,
				background->region.y,
				background->region.width,
				background->region.height);

	return desktop;
}

static cairo_pattern_t *
composite_image_onto_desktop (PanelBackground *background)
{
	int              width, height;
	cairo_t         *cr;
	cairo_surface_t *surface;
	cairo_pattern_t *pattern;

	if (!background->desktop)
		background->desktop = get_desktop_pixbuf (background);

	if (!background->desktop)
		return NULL;

	width  = gdk_pixbuf_get_width  (background->desktop);
	height = gdk_pixbuf_get_height (background->desktop);

        surface = gdk_window_create_similar_surface (background->window,
                                                     CAIRO_CONTENT_COLOR_ALPHA,
                                                     width, height);
        if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS) {
                cairo_surface_destroy (surface);
                return NULL;
        }

	cr = cairo_create (surface);

        if (background->has_alpha) {
                cairo_set_source_rgb (cr, 1, 1, 1);
                cairo_paint (cr);

                gdk_cairo_set_source_pixbuf (cr, background->desktop, 0, 0);
                cairo_rectangle (cr, 0, 0, width, height);
                cairo_fill (cr);
        }

        gdk_cairo_set_source_pixbuf (cr, background->transformed_image, 0, 0);
        cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	cairo_destroy (cr);

        pattern = cairo_pattern_create_for_surface (surface);
	cairo_surface_destroy (surface);

        return pattern;
}

static cairo_pattern_t *
composite_color_onto_desktop (PanelBackground *background)
{
        cairo_surface_t *surface;
        cairo_pattern_t *pattern;
        cairo_t *cr;

        if (!background->desktop)
                background->desktop = get_desktop_pixbuf (background);
        if (!background->desktop)
                return NULL;

        surface = gdk_window_create_similar_surface (background->window,
                                                     CAIRO_CONTENT_COLOR_ALPHA,
                                                     background->region.width,
                                                     background->region.height);
        if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS) {
                cairo_surface_destroy (surface);
                return NULL;
        }

        cr = cairo_create (surface);
        gdk_cairo_set_source_pixbuf (cr, background->desktop, 0, 0);
        cairo_paint (cr);

        gdk_cairo_set_source_rgba (cr, &background->color);
        cairo_paint (cr);

        cairo_destroy (cr);

        pattern = cairo_pattern_create_for_surface (surface);
        cairo_surface_destroy (surface);

        return pattern;
}

static cairo_pattern_t *
get_composited_pattern (PanelBackground *background)
{
	cairo_pattern_t *pattern = NULL;

	switch (background->type) {
	case PANEL_BACK_NONE:
		break;
	case PANEL_BACK_COLOR:
		pattern = composite_color_onto_desktop (background);
		break;
	case PANEL_BACK_IMAGE:
		pattern = composite_image_onto_desktop (background);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return pattern;
}

static gboolean
panel_background_composite (PanelBackground *background)
{
	if (!background->transformed)
		return FALSE;

	free_composited_resources (background);

	switch (background->type) {
	case PANEL_BACK_NONE:
		break;
	case PANEL_BACK_COLOR:
		if (background->has_alpha)
			background->composited_pattern =
				get_composited_pattern (background);
		break;
	case PANEL_BACK_IMAGE:
		if (background->transformed_image)
                        background->composited_pattern =
                                get_composited_pattern (background);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	background->composited = TRUE;

	panel_background_prepare (background);

	return TRUE;
}

static void
free_transformed_resources (PanelBackground *background)
{
	free_composited_resources (background);

	background->transformed = FALSE;

	if (background->type != PANEL_BACK_IMAGE)
		return;

	if (background->transformed_image)
                g_object_unref (background->transformed_image);
	background->transformed_image = NULL;
}

static GdkPixbuf *
get_scaled_and_rotated_pixbuf (PanelBackground *background)
{
	GdkPixbuf *scaled;
	GdkPixbuf *retval;
	int        orig_width, orig_height;
	int        panel_width, panel_height;
	int        width, height;

	load_background_file (background);
	if (!background->loaded_image)
		return NULL;

	orig_width  = gdk_pixbuf_get_width  (background->loaded_image);
	orig_height = gdk_pixbuf_get_height (background->loaded_image);

	panel_width  = background->region.width;
	panel_height = background->region.height;

	width  = orig_width;
	height = orig_height;

	switch (background->style_image) {
	case PANEL_BACKGROUND_IMAGE_STYLE_NONE:
		if (background->orientation == GTK_ORIENTATION_VERTICAL &&
			   background->rotate_image) {
			int tmp = width;
			width = height;
			height = tmp;
		}
		break;
	case PANEL_BACKGROUND_IMAGE_STYLE_STRETCH:
		if (background->orientation == GTK_ORIENTATION_VERTICAL &&
		    background->rotate_image) {
			width  = panel_height;
			height = panel_width;
		} else {
			width  = panel_width;
			height = panel_height;
		}
		break;
	case PANEL_BACKGROUND_IMAGE_STYLE_FIT:
		switch (background->orientation) {
		case GTK_ORIENTATION_HORIZONTAL:
			width  = orig_width * panel_height / orig_height;
			height = panel_height;
			break;
		case GTK_ORIENTATION_VERTICAL:
			if (background->rotate_image) {
				width  = orig_width * panel_width / orig_height;
				height = panel_width;
			} else {
				width  = panel_width;
				height = orig_height * panel_width / orig_width;
			}
			break;
		default:
			g_assert_not_reached ();
			break;
		}
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	if (width == orig_width &&
	    height == orig_height) {
		scaled = background->loaded_image;
		g_object_ref (scaled);
	} else {
		scaled = gdk_pixbuf_scale_simple (
				background->loaded_image,
				width, height,
				GDK_INTERP_BILINEAR);
	}

	if (background->rotate_image &&
	    background->orientation == GTK_ORIENTATION_VERTICAL) {
		if (!background->has_alpha) {
			guchar *dest;
			guchar *src;
			int     x, y;
			int     destrowstride;
			int     srcrowstride;

			retval = gdk_pixbuf_new (
				GDK_COLORSPACE_RGB, FALSE, 8, height, width);

			dest          = gdk_pixbuf_get_pixels (retval);
			destrowstride = gdk_pixbuf_get_rowstride (retval);
			src           = gdk_pixbuf_get_pixels (scaled);
			srcrowstride  = gdk_pixbuf_get_rowstride (scaled);

			for (y = 0; y < height; y++)
				for (x = 0; x < width; x++) {
					guchar *dstptr = & ( dest [3*y + destrowstride * (width - x - 1)] );
					guchar *srcptr = & ( src [y * srcrowstride + 3*x] );
					dstptr[0] = srcptr[0];
					dstptr[1] = srcptr[1];
					dstptr[2] = srcptr[2];
				}

			g_object_unref (scaled);
		} else {
			guint32 *dest;
			guint32 *src;
			int     x, y;
			int     destrowstride;
			int     srcrowstride;

			retval = gdk_pixbuf_new (
				GDK_COLORSPACE_RGB, TRUE, 8, height, width);

			dest          = (guint32 *) gdk_pixbuf_get_pixels (retval);
			destrowstride =             gdk_pixbuf_get_rowstride (retval) / 4;
			src           = (guint32 *) gdk_pixbuf_get_pixels (scaled);
			srcrowstride  =             gdk_pixbuf_get_rowstride (scaled) / 4;

			for (y = 0; y < height; y++)
				for (x = 0; x < width; x++)
					dest [y + destrowstride * (width - x - 1)] =
						src [y * srcrowstride + x];

			g_object_unref (scaled);
		}
	} else
		retval = scaled;

	return retval;
}

static gboolean
panel_background_transform (PanelBackground *background)
{
	if (background->region.width == -1)
		return FALSE;

	free_transformed_resources (background);

	if (background->type == PANEL_BACK_IMAGE)
		background->transformed_image =
			get_scaled_and_rotated_pixbuf (background);

	background->transformed = TRUE;

	panel_background_composite (background);

	return TRUE;
}

static void
disconnect_background_monitor (PanelBackground *background)
{
	if (background->monitor) {
		g_signal_handler_disconnect (
			background->monitor, background->monitor_signal);
		background->monitor_signal = -1;
		g_object_unref (background->monitor);
	}
	background->monitor = NULL;

	if (background->desktop)
		g_object_unref (background->desktop);
	background->desktop = NULL;
}

static void
panel_background_update_has_alpha (PanelBackground *background)
{
	gboolean has_alpha = FALSE;

	if (background->type == PANEL_BACK_COLOR)
		has_alpha = (background->color.alpha < 1.);
	else if (background->type == PANEL_BACK_IMAGE &&
		 background->loaded_image)
		has_alpha = gdk_pixbuf_get_has_alpha (background->loaded_image);

	background->has_alpha = has_alpha;

	if (!has_alpha)
		disconnect_background_monitor (background);
}

static void
load_background_file (PanelBackground *background)
{
	GError *error = NULL;

	if (!g_file_test (background->image, G_FILE_TEST_IS_REGULAR))
		return;

	//FIXME add a monitor on the file so that we reload the background
	//when it changes
	background->loaded_image = 
		gdk_pixbuf_new_from_file (background->image, &error);
	if (!background->loaded_image) {
		g_assert (error != NULL);
		g_warning (G_STRLOC ": unable to open '%s': %s",
			   background->image, error->message);
		g_error_free (error);
	}

	panel_background_update_has_alpha (background);
}

static void
panel_background_set_type (PanelBackground     *background,
			   PanelBackgroundType  type)
{
	if (background->type == type)
		return;

	free_transformed_resources (background);

	background->type = type;

	panel_background_update_has_alpha (background);

	panel_background_transform (background);
}

static void
panel_background_set_color_no_update (PanelBackground *background,
				      const GdkRGBA   *color)
{
	g_return_if_fail (color != NULL);

        if (gdk_rgba_equal (color, &background->color))
                return;

        background->color = *color;
        panel_background_update_has_alpha (background);
}

static void
panel_background_set_color (PanelBackground *background,
                            const GdkRGBA   *color)
{
	g_return_if_fail (color != NULL);

        if (gdk_rgba_equal (color, &background->color))
                return;

	free_transformed_resources (background);
	panel_background_set_color_no_update (background, color);
	panel_background_transform (background);
}

static void
panel_background_set_image_uri_no_update (PanelBackground *background,
					  const char      *uri)
{
	if (background->loaded_image)
		g_object_unref (background->loaded_image);
	background->loaded_image = NULL;

	if (background->image)
		g_free (background->image);
	background->image = NULL;

	if (!PANEL_GLIB_STR_EMPTY (uri)) {
		GFile *file;
		file = g_file_new_for_uri (uri);

		if (g_file_is_native (file))
			background->image = g_file_get_path (file);

		g_object_unref (file);
	}

	panel_background_update_has_alpha (background);
}

static void
panel_background_set_image_uri (PanelBackground *background,
				const char      *uri)
{
	GFile *file;
	char  *current_uri = NULL;

	if (!background->image && !uri)
		return;

	if (background->image) {
		file = g_file_new_for_path (background->image);
		current_uri = g_file_get_uri (file);
		g_object_unref (file);
	}

	if (g_strcmp0 (current_uri, uri) == 0) {
		g_free (current_uri);
		return;
	}
	g_free (current_uri);

	free_transformed_resources (background);
	panel_background_set_image_uri_no_update (background, uri);
	panel_background_transform (background);
}

static void
panel_background_set_image_style_no_update (PanelBackground           *background,
					    PanelBackgroundImageStyle  style)
{
	background->style_image = style;
}

static void
panel_background_set_image_style (PanelBackground           *background,
				  PanelBackgroundImageStyle  style)
{
	if (background->style_image == style)
		return;

	free_transformed_resources (background);
	panel_background_set_image_style_no_update (background, style);
	panel_background_transform (background);
}

static void
panel_background_set_rotate_no_update (PanelBackground *background,
				       gboolean         rotate_image)
{
	background->rotate_image = rotate_image != FALSE;
}

static void
panel_background_set_rotate (PanelBackground *background,
			     gboolean         rotate_image)
{
	rotate_image = rotate_image != FALSE;

	if (background->rotate_image == rotate_image)
		return;

	free_transformed_resources (background);
	panel_background_set_rotate_no_update (background, rotate_image);
	panel_background_transform (background);
}

static void
panel_background_settings_changed (GSettings       *settings,
				   char            *key,
				   PanelBackground *background)
{
	char     *value_str;
	gboolean  value_boolean;

	if (g_strcmp0 (key, PANEL_BACKGROUND_TYPE_KEY) == 0) {
		PanelBackgroundType type;
		type = g_settings_get_enum (settings, key);
		panel_background_set_type (background, type);
	} else if (g_strcmp0 (key, PANEL_BACKGROUND_COLOR_KEY) == 0) {
		GdkRGBA color;
		value_str = g_settings_get_string (settings, key);
		if (gdk_rgba_parse (&color, value_str))
			panel_background_set_color (background, &color);
		g_free (value_str);
	} else if (g_strcmp0 (key, PANEL_BACKGROUND_IMAGE_URI_KEY) == 0) {
		value_str = g_settings_get_string (settings, key);
		panel_background_set_image_uri (background, value_str);
		g_free (value_str);
	} else if (g_strcmp0 (key, PANEL_BACKGROUND_IMAGE_STYLE_KEY) == 0) {
		PanelBackgroundImageStyle style;
		style = g_settings_get_enum (settings, key);
		panel_background_set_image_style (background, style);
	} else if (g_strcmp0 (key, PANEL_BACKGROUND_IMAGE_ROTATE_KEY) == 0) {
		value_boolean = g_settings_get_boolean (settings, key);
		panel_background_set_rotate (background, value_boolean);
	} else {
		g_assert_not_reached ();
	}
}

void
panel_background_settings_init (PanelBackground *background,
				GSettings       *settings)
{
	PanelBackgroundType  type;
	char                *color_str;
	GdkRGBA              color;
	char                *image;
	PanelBackgroundImageStyle style_image;
	gboolean             rotate_image;

	g_assert (background->settings == NULL);

	background->settings = g_object_ref (settings);
	g_signal_connect (background->settings, "changed",
			  G_CALLBACK (panel_background_settings_changed),
			  background);

	color_str = g_settings_get_string (background->settings,
					   PANEL_BACKGROUND_COLOR_KEY);
	if (!gdk_rgba_parse (&color, color_str))
		gdk_rgba_parse (&color, PANEL_BACKGROUND_COLOR_DEFAULT);
	panel_background_set_color_no_update (background, &color);
	g_free (color_str);

	image = g_settings_get_string (background->settings,
				       PANEL_BACKGROUND_IMAGE_URI_KEY);
	panel_background_set_image_uri_no_update (background, image);
	g_free (image);

	style_image = g_settings_get_enum (background->settings,
					   PANEL_BACKGROUND_IMAGE_STYLE_KEY);
	panel_background_set_image_style_no_update (background, style_image);

	rotate_image = g_settings_get_boolean (background->settings,
					       PANEL_BACKGROUND_IMAGE_ROTATE_KEY);
	panel_background_set_rotate_no_update (background, rotate_image);

	type = g_settings_get_enum (background->settings,
				    PANEL_BACKGROUND_TYPE_KEY);
	panel_background_set_type (background, type);
}

void
panel_background_set_default_style (PanelBackground *background,
				    GdkRGBA         *color,
				    cairo_pattern_t *pattern)
{
	g_return_if_fail (color != NULL);

	background->default_color = *color;

        if (pattern)
                cairo_pattern_reference (pattern);

        if (background->default_pattern)
                cairo_pattern_destroy (background->default_pattern);

        background->default_pattern = pattern;

	if (background->type == PANEL_BACK_NONE)
		panel_background_prepare (background);
}

void
panel_background_realized (PanelBackground *background,
			   GdkWindow       *window)
{
	g_return_if_fail (window != NULL);

	if (background->window)
		return;

        background->window = g_object_ref (window);

	panel_background_prepare (background);
}

void
panel_background_unrealized (PanelBackground *background)
{
	if (background->window)
		g_object_unref (background->window);
	background->window = NULL;
}

void
panel_background_change_region (PanelBackground *background,
				GtkOrientation   orientation,
				int              x,
				int              y,
				int              width,
				int              height)
{
	gboolean need_to_retransform = FALSE;
	gboolean need_to_reprepare = FALSE;

	if (background->region.x == x &&
	    background->region.y == y &&
	    background->region.width == width &&
	    background->region.height == height &&
	    background->orientation == orientation)
		return;

	/* we only need to retransform anything
	   on size/orientation changes if the
	   background is an image and some
	   conditions are met */
	if (background->type == PANEL_BACK_IMAGE) {
		if (background->orientation != orientation &&
		    background->rotate_image) {
			/* if orientation changes and we are rotating */
			need_to_retransform = TRUE;
		} else if ((background->region.width != width ||
			    background->region.height != height) &&
			   (background->style_image != PANEL_BACKGROUND_IMAGE_STYLE_NONE)) {
			/* or if the size changes and we are 
			   stretching or fitting the image */
			need_to_retransform = TRUE;
		}
	}

	/* if size changed, we at least need
	   to "prepare" the background again */
	if (background->region.width != width ||
	    background->region.height != height)
		need_to_reprepare = TRUE;

	background->region.x      = x;
	background->region.y      = y;
	background->region.width  = width;
	background->region.height = height;

	background->orientation = orientation;

	if (background->desktop)
		g_object_unref (background->desktop);
	background->desktop = NULL;

	if (need_to_retransform || ! background->transformed)
		/* only retransform the background if we have in
		   fact changed size/orientation */
		panel_background_transform (background);
	else if (background->has_alpha || ! background->composited)
		/* only do compositing if we have some alpha
		   value to worry about */
		panel_background_composite (background);
	else if (need_to_reprepare)
		/* at least we must prepare the background
		   if the size changed */
		panel_background_prepare (background);
}

void
panel_background_init (PanelBackground              *background,
		       PanelBackgroundChangedNotify  notify_changed,
		       gpointer                      user_data)
{
	background->settings = NULL;

	background->type = PANEL_BACK_NONE;
	background->notify_changed = notify_changed;
	background->user_data = user_data;

	background->color.red   = 0.;
	background->color.blue  = 0.;
	background->color.green = 0.;
	background->color.alpha = 1.;

	background->image        = NULL;
	background->loaded_image = NULL;

	background->orientation        = GTK_ORIENTATION_HORIZONTAL;
	background->region.x           = -1;
	background->region.y           = -1;
	background->region.width       = -1;
	background->region.height      = -1;
	background->transformed_image  = NULL;
	background->composited_pattern = NULL;

	background->monitor        = NULL;
	background->desktop        = NULL;
	background->monitor_signal = -1;

	background->window   = NULL;

	background->default_pattern     = NULL;
	background->default_color.red   = 0.;
	background->default_color.green = 0.;
	background->default_color.blue  = 0.;
	background->default_color.alpha = 1.;

	background->style_image   = PANEL_BACKGROUND_IMAGE_STYLE_NONE;
	background->rotate_image  = FALSE;

	background->has_alpha = FALSE;

	background->transformed = FALSE;
	background->composited  = FALSE;
}

void
panel_background_free (PanelBackground *background)
{
	disconnect_background_monitor (background);

	free_transformed_resources (background);

	if (background->settings)
		g_object_unref (background->settings);
	background->settings = NULL;

	if (background->image)
		g_free (background->image);
	background->image = NULL;

	if (background->loaded_image)
		g_object_unref (background->loaded_image);
	background->loaded_image = NULL;

	if (background->monitor)
		g_object_unref (background->monitor);
	background->monitor = NULL;

	if (background->window)
		g_object_unref (background->window);
	background->window = NULL;

	if (background->default_pattern)
                cairo_pattern_destroy (background->default_pattern);
	background->default_pattern = NULL;
}

char *
panel_background_make_string (PanelBackground *background,
			      int              x,
			      int              y)
{
	PanelBackgroundType  effective_type;
        GVariant            *variant;
        gchar               *retval;

	effective_type = panel_background_effective_type (background);

	if (effective_type == PANEL_BACK_IMAGE ||
	    (effective_type == PANEL_BACK_COLOR && background->has_alpha)) {
                cairo_surface_t *surface;

		if (!background->composited_pattern)
			return NULL;

                if (cairo_pattern_get_surface (background->composited_pattern, &surface) != CAIRO_STATUS_SUCCESS)
                        return NULL;

                g_assert (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_XLIB);

                variant = g_variant_new ("(uii)", (guint32)cairo_xlib_surface_get_drawable (surface),
                                         x, y);
	} else if (effective_type == PANEL_BACK_COLOR) {
                variant = g_variant_new ("(dddd)",
                                         background->color.red, background->color.green,
                                         background->color.blue, background->color.alpha);
	} else
		variant = g_variant_new ("()");

        retval = g_variant_print (variant, TRUE);
        g_variant_unref (variant);

        return retval;
}

PanelBackgroundType
panel_background_get_type (PanelBackground *background)
{
	return background->type;
}

const GdkRGBA *
panel_background_get_color (PanelBackground *background)
{
	return &(background->color);
}

/* What are we actually rendering - e.g. if we're supposed to
 * be rendering an image, but haven't got a valid image, then
 * we're rendering the default gtk background.
 */
PanelBackgroundType
panel_background_effective_type (PanelBackground *background)
{
	PanelBackgroundType retval;

	retval = background->type;
	if (background->type == PANEL_BACK_IMAGE && !background->composited_pattern)
		retval = PANEL_BACK_NONE;

	return retval;
}

static cairo_pattern_t *
panel_background_get_pattern_for_widget (PanelBackground *background,
                                         GtkWidget       *widget)
{
	GtkAllocation    allocation;
	cairo_t         *cr;
        cairo_surface_t *surface;
        cairo_surface_t *bg_surface;
	cairo_pattern_t *pattern;
        cairo_matrix_t   matrix;

        if (!background->composited_pattern)
                return NULL;

        if (cairo_pattern_get_surface (background->composited_pattern, &bg_surface) != CAIRO_STATUS_SUCCESS)
                return NULL;

	gtk_widget_get_allocation (widget, &allocation);
        surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                              allocation.width, allocation.height);

	cr = cairo_create (surface);
        cairo_set_source_surface (cr, bg_surface, -allocation.x, -allocation.y);
        cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
        cairo_fill (cr);
        cairo_destroy (cr);

        pattern = cairo_pattern_create_for_surface (surface);
        cairo_matrix_init_translate (&matrix, 0, 0);
        cairo_matrix_scale (&matrix, allocation.width, allocation.height);
        cairo_pattern_set_matrix (pattern, &matrix);
        cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

        cairo_surface_destroy (surface);

        return pattern;
}

static GtkStyleProperties *
_panel_background_get_widget_style_properties (GtkWidget *widget,
                                               gboolean   create_if_needed)
{
        GtkStyleProperties *properties;

        properties = g_object_get_data (G_OBJECT (widget),
                                        "panel-object-style-props");

        if (!properties && create_if_needed) {
                properties = gtk_style_properties_new ();
                g_object_set_data_full (G_OBJECT (widget),
                                        "panel-object-style-props",
                                        properties,
                                        (GDestroyNotify) g_object_unref);
        }

        return properties;
}

static void
_panel_background_reset_widget_style_properties (GtkWidget *widget)
{
        GtkStyleProperties *properties;

        properties = _panel_background_get_widget_style_properties (widget, FALSE);

        if (properties)
                gtk_style_context_remove_provider (gtk_widget_get_style_context (widget),
                                                   GTK_STYLE_PROVIDER (properties));


        g_object_set_data (G_OBJECT (widget), "panel-object-style-props", NULL);
}

void
panel_background_change_background_on_widget (PanelBackground *background,
					      GtkWidget       *widget)
{
        GtkStyleProperties *properties;

        gtk_widget_reset_style (widget);

	switch (panel_background_get_type (background)) {
	case PANEL_BACK_NONE:
                _panel_background_reset_widget_style_properties (widget);
                return;
	case PANEL_BACK_COLOR:
                if (!background->has_alpha) {
                        properties = _panel_background_get_widget_style_properties (widget, TRUE);
                        gtk_style_properties_set (properties, GTK_STATE_FLAG_NORMAL,
                                                  "background-color", &background->color,
                                                  "background-image", NULL,
                                                  NULL);
                        break;
                }
                // Color with alpha, fallback to image
	case PANEL_BACK_IMAGE: {
                cairo_pattern_t *pattern;

                properties = _panel_background_get_widget_style_properties (widget, TRUE);
                pattern = panel_background_get_pattern_for_widget (background, widget);
                if (pattern) {
                        gtk_style_properties_set (properties, GTK_STATE_FLAG_NORMAL,
                                                  /* background-color can't be
                                                   * NULL, but is ignored
                                                   * anyway */
                                                  "background-image", pattern,
                                                  NULL);
                        cairo_pattern_destroy (pattern);
                } else {
                        _panel_background_reset_widget_style_properties (widget);
                        return;
                }
        }
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	/* Note: this actually replaces the old properties, since it's the same
	 * pointer */
        gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
                                        GTK_STYLE_PROVIDER (properties),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
