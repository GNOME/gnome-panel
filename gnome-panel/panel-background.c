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

#include "panel-background-monitor.h"
#include "panel-gdk-pixbuf-extensions.h"


static gboolean panel_background_composite (PanelBackground *background);


static void
free_prepared_resources (PanelBackground *background)
{
	background->prepared = FALSE;

	switch (background->type) {
	case PANEL_BACK_NONE:
		break;
	case PANEL_BACK_COLOR:
		if (!background->has_alpha) {
			if (background->colormap && background->color.gdk.pixel)
				gdk_colormap_free_colors (
					background->colormap,
					&background->color.gdk, 1);
			background->color.gdk.pixel = 0;
		}
		break;
	case PANEL_BACK_IMAGE:
		if (background->pixmap)
			g_object_unref (background->pixmap);
		background->pixmap = NULL;
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
set_pixbuf_background (PanelBackground *background)
{
	g_assert (background->composited_image != NULL);

	gdk_pixbuf_render_pixmap_and_mask_for_colormap (
		background->composited_image,
		background->colormap,
		&background->pixmap, NULL, 128);

	gdk_window_set_back_pixmap (
		background->window, background->pixmap, FALSE);
}

static gboolean
panel_background_prepare (PanelBackground *background)
{
	PanelBackgroundType  effective_type;
	GtkWidget           *widget = NULL;

	if (!background->colormap || !background->transformed)
		return FALSE;

	free_prepared_resources (background);

	effective_type = panel_background_effective_type (background);

	switch (effective_type) {
	case PANEL_BACK_NONE:
		if (background->default_pixmap)
			gdk_window_set_back_pixmap (
				background->window, background->default_pixmap, FALSE);
		else
			gdk_window_set_background (
				background->window, &background->default_color);
		break;
	case PANEL_BACK_COLOR:
		if (background->has_alpha &&
		    background->composited_image)
			set_pixbuf_background (background);
		else {
			gdk_colormap_alloc_color (
				background->colormap,
				&background->color.gdk,
				FALSE, TRUE);
			gdk_window_set_background (
				background->window, &background->color.gdk);
		}
		break;
	case PANEL_BACK_IMAGE:
		set_pixbuf_background (background);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	gdk_window_get_user_data (GDK_WINDOW (background->window),
				  (gpointer *) &widget);

	if (GTK_IS_WIDGET (widget))
	  gtk_widget_queue_draw (widget);

	background->prepared = TRUE;

	return TRUE;
}

static void
free_composited_resources (PanelBackground *background)
{
	free_prepared_resources (background);

	background->composited = FALSE;

	if (background->composited_image)
		g_object_unref (background->composited_image);
	background->composited_image = NULL;
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

	g_object_unref (tmp);

	panel_background_composite (background);
}

static GdkPixbuf *
get_desktop_pixbuf (PanelBackground *background)
{
	GdkPixbuf *desktop;

	if (!background->monitor) {
		background->monitor =
			panel_background_monitor_get_for_screen (
				gdk_drawable_get_screen (background->window));

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

static GdkPixbuf *
composite_image_onto_desktop (PanelBackground *background)
{
	GdkPixbuf *retval;
	ArtIRect   rect;
	int        width, height;
	int        tilewidth, tileheight;

	if (!background->desktop)
		background->desktop = get_desktop_pixbuf (background);

	retval = gdk_pixbuf_copy (background->desktop);

	width  = gdk_pixbuf_get_width  (background->desktop);
	height = gdk_pixbuf_get_height (background->desktop);

	tilewidth  = gdk_pixbuf_get_width (background->transformed_image);
	tileheight = gdk_pixbuf_get_height (background->transformed_image);

	rect.x0 = 0;
	rect.y0 = 0;
	rect.x1 = width;
	rect.y1 = height;

	panel_gdk_pixbuf_draw_to_pixbuf_tiled (
		background->transformed_image,
		retval, rect, tilewidth, tileheight,
		0, 0, 255, GDK_INTERP_NEAREST);

	return retval;
}

static GdkPixbuf *
composite_color_onto_desktop (PanelBackground *background)
{
	guint32 color;

	if (!background->desktop)
		background->desktop = get_desktop_pixbuf (background);

	color = ((background->color.gdk.red & 0xff00) << 8) +
		 (background->color.gdk.green & 0xff00) +
		 (background->color.gdk.blue >> 8);

	return gdk_pixbuf_composite_color_simple (
			background->desktop, 
			gdk_pixbuf_get_width (background->desktop),
			gdk_pixbuf_get_height (background->desktop),
			GDK_INTERP_NEAREST,
			(255 - (background->color.alpha >> 8)),
			255, color, color);
}

static GdkPixbuf *
get_composited_pixbuf (PanelBackground *background)
{
	GdkPixbuf *retval = NULL;

	switch (background->type) {
	case PANEL_BACK_NONE:
		break;
	case PANEL_BACK_COLOR:
		retval = composite_color_onto_desktop (background);
		break;
	case PANEL_BACK_IMAGE:
		retval = composite_image_onto_desktop (background);
		if (!retval)
			retval = g_object_ref (background->transformed_image);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return retval;
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
			background->composited_image =
				get_composited_pixbuf (background);
		break;
	case PANEL_BACK_IMAGE:
		if (background->transformed_image) {
			if (background->has_alpha)
				background->composited_image =
					get_composited_pixbuf (background);
			else
				background->composited_image =
					g_object_ref (background->transformed_image);
		}
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

	if (!background->loaded_image)
		return NULL;

	orig_width  = gdk_pixbuf_get_width  (background->loaded_image);
	orig_height = gdk_pixbuf_get_height (background->loaded_image);

	panel_width  = background->region.width;
	panel_height = background->region.height;

	width  = orig_width;
	height = orig_height;

	if (background->fit_image) {
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
	} else if (background->stretch_image) {
		if (background->orientation == GTK_ORIENTATION_VERTICAL &&
		    background->rotate_image) {
			width  = panel_height;
			height = panel_width;
		} else {
			width  = panel_width;
			height = panel_height;
		}
	}

	scaled = gdk_pixbuf_scale_simple (
			background->loaded_image,
			width, height,
			GDK_INTERP_BILINEAR);

	if (background->rotate_image &&
	    background->orientation == GTK_ORIENTATION_VERTICAL) {
		gulong *dest;
		gulong *src;
		int     x, y;

		/* FIXME: horrible lazy hack.
		 *        The rotation loop below was optimised
		 *        for 32 bit pixel format pixbufs. Need
		 *        a version that can handle pixbufs without
		 *        and alpha channel.
		 */
		if (!background->has_alpha) {
			GdkPixbuf *tmp;

			tmp = gdk_pixbuf_add_alpha (scaled, FALSE, 0, 0, 0);
			g_object_unref (scaled);
			scaled = tmp;
		}

		retval = gdk_pixbuf_new (
				GDK_COLORSPACE_RGB, TRUE, 8, height, width);

		dest = (gulong *) gdk_pixbuf_get_pixels (retval);
		src  = (gulong *) gdk_pixbuf_get_pixels (scaled);

		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
				dest [y + height * (width - x - 1)] =
					src [y * width + x];

		g_object_unref (scaled);
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
		has_alpha = (background->color.alpha != 0xffff);

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

	background->loaded_image = 
		gdk_pixbuf_new_from_file (background->image, &error);
	if (!background->loaded_image) {
		g_assert (error != NULL);
		g_warning (G_STRLOC ": unable to open '%s' : %s",
			   background->image, error->message);
		g_error_free (error);
	}
}

void
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

void
panel_background_set_pango_color (PanelBackground *background,
				  PangoColor      *pango_color)
{
	if (background->color.gdk.red   == pango_color->red &&
	    background->color.gdk.green == pango_color->green &&
	    background->color.gdk.blue  == pango_color->blue)
		return;

	free_transformed_resources (background);

	background->color.gdk.red   = pango_color->red;
	background->color.gdk.green = pango_color->green;
	background->color.gdk.blue  = pango_color->blue;

	panel_background_transform (background);
}

void
panel_background_set_opacity (PanelBackground *background,
			      guint16          opacity)
{
	if (background->color.alpha == opacity)
		return;

	free_transformed_resources (background);

	background->color.alpha = opacity;

	panel_background_update_has_alpha (background);

	panel_background_transform (background);
}

void
panel_background_set_color (PanelBackground *background,
			    PanelColor      *color)
{
	PangoColor pango_color;

	pango_color.red   = color->gdk.red;
	pango_color.green = color->gdk.green;
	pango_color.blue  = color->gdk.blue;

	panel_background_set_pango_color (background, &pango_color);
	panel_background_set_opacity (background, color->alpha);
}

void
panel_background_set_image (PanelBackground *background,
			    const char      *image)
{
	if (!background->image && !image)
		return;

	if (background->image && image && !strcmp (background->image, image))
		return;

	free_transformed_resources (background);

	if (background->loaded_image)
		g_object_unref (background->loaded_image);
	background->loaded_image = NULL;

	if (background->image)
		g_free (background->image);
	background->image = NULL;

	if (image && image [0]) {
		background->image = g_strdup (image);
		load_background_file (background);
	}

	panel_background_update_has_alpha (background);
}

void
panel_background_set_fit (PanelBackground *background,
			  gboolean         fit_image)
{
	fit_image = fit_image != FALSE;

	if (background->fit_image == fit_image)
		return;

	free_transformed_resources (background);

	background->fit_image = fit_image;

	panel_background_transform (background);
}

void
panel_background_set_stretch (PanelBackground *background,
			      gboolean         stretch_image)
{
	stretch_image = stretch_image != FALSE;

	if (background->stretch_image == stretch_image)
		return;

	free_transformed_resources (background);

	background->stretch_image = stretch_image;

	panel_background_transform (background);
}

void
panel_background_set_rotate (PanelBackground *background,
			     gboolean         rotate_image)
{
	rotate_image = rotate_image != FALSE;

	if (background->rotate_image == rotate_image)
		return;

	free_transformed_resources (background);

	background->rotate_image = rotate_image;

	panel_background_transform (background);
}

void
panel_background_set (PanelBackground     *background,
		      PanelBackgroundType  type,
		      PanelColor          *color,
		      const char          *image,
		      gboolean             fit_image,
		      gboolean             stretch_image,
		      gboolean             rotate_image)
{
	panel_background_set_color (background, color);
	panel_background_set_image (background, image);
	panel_background_set_fit (background, fit_image);
	panel_background_set_stretch (background, stretch_image);
	panel_background_set_rotate (background, rotate_image);
	panel_background_set_type (background, type);
}

void
panel_background_set_default_style (PanelBackground *background,
				    GdkColor        *color,
				    GdkPixmap       *pixmap)
{
	background->default_color = *color;

	if (pixmap)
		g_object_ref (pixmap);

	if (background->default_pixmap)
		g_object_unref (background->default_pixmap);

	background->default_pixmap = pixmap;

	if (background->type == PANEL_BACK_NONE)
		panel_background_prepare (background);
}

void
panel_background_realized (PanelBackground *background,
			   GdkWindow       *window)
{
	if (background->window && background->colormap && background->gc)
		return;

	if (!background->window)
		background->window = g_object_ref (window);

	if (!background->colormap)
		background->colormap =
			g_object_ref (gdk_drawable_get_colormap (window));

	if (!background->gc)
		background->gc = gdk_gc_new (window);

	panel_background_prepare (background);
}

void
panel_background_unrealized (PanelBackground *background)
{
	free_prepared_resources (background);

	if (background->window)
		g_object_unref (background->window);
	background->window = NULL;

	if (background->colormap)
		g_object_unref (background->colormap);
	background->colormap = NULL;

	if (background->gc)
		g_object_unref (background->gc);
	background->gc = NULL;
}

void
panel_background_change_region (PanelBackground *background,
				GtkOrientation   orientation,
				int              x,
				int              y,
				int              width,
				int              height)
{
	if (background->region.x == x &&
	    background->region.y == y &&
	    background->region.width == width &&
	    background->region.height == height &&
	    background->orientation == orientation)
		return;

	background->region.x      = x;
	background->region.y      = y;
	background->region.width  = width;
	background->region.height = height;

	background->orientation = orientation;

	if (background->desktop)
		g_object_unref (background->desktop);
	background->desktop = NULL;

	panel_background_transform (background);
}

void
panel_background_init (PanelBackground *background)
{
	background->type = PANEL_BACK_NONE;

	background->color.gdk.red   = 0;
	background->color.gdk.blue  = 0;
	background->color.gdk.green = 0;
	background->color.gdk.pixel = 0;
	background->color.alpha     = 0xffff;

	background->image        = NULL;
	background->loaded_image = NULL;

	background->orientation       = GTK_ORIENTATION_HORIZONTAL;
	background->region.x          = -1;
	background->region.y          = -1;
	background->region.width      = -1;
	background->region.height     = -1;
	background->transformed_image = NULL;
	background->composited_image  = NULL;

	background->monitor        = NULL;
	background->desktop        = NULL;
	background->monitor_signal = -1;

	background->pixmap   = NULL;
	background->window   = NULL;
	background->colormap = NULL;
	background->gc       = NULL;

	background->default_pixmap      = NULL;
	background->default_color.red   = 0;
	background->default_color.green = 0;
	background->default_color.blue  = 0;
	background->default_color.pixel = 0;

	background->fit_image     = FALSE;
	background->stretch_image = FALSE;
	background->rotate_image  = FALSE;

	background->has_alpha = FALSE;

	background->transformed = FALSE;
	background->composited  = FALSE;
	background->prepared    = FALSE;
}

void
panel_background_free (PanelBackground *background)
{
	disconnect_background_monitor (background);

	free_transformed_resources (background);

	if (background->image)
		g_free (background->image);
	background->image = NULL;

	if (background->loaded_image)
		g_object_unref (background->loaded_image);
	background->loaded_image = NULL;

	if (background->window)
		g_object_unref (background->window);
	background->window = NULL;

	if (background->colormap)
		g_object_unref (background->colormap);
	background->colormap = NULL;

	if (background->gc)
		g_object_unref (background->gc);
	background->gc = NULL;

	if (background->default_pixmap)
		g_object_unref (background->default_pixmap);
	background->default_pixmap = NULL;
}

char *
panel_background_make_string (PanelBackground *background,
			      int              x,
			      int              y)
{
	char *retval = NULL;

	if (background->type == PANEL_BACK_IMAGE ||
	    (background->type == PANEL_BACK_COLOR && background->has_alpha) ||
	    (background->type == PANEL_BACK_NONE && background->default_pixmap)) {
		GdkNativeWindow pixmap_xid;

		if (!background->window)
			return NULL;

		pixmap_xid = gdk_x11_drawable_get_xid (
				GDK_DRAWABLE (background->window));

		retval = g_strdup_printf ("pixmap:%d,%d,%d", pixmap_xid, x, y);

	} else if (background->type == PANEL_BACK_COLOR)
		retval = g_strdup_printf (
				"color:%.4x%.4x%.4x",
				background->color.gdk.red,
				background->color.gdk.green,
				background->color.gdk.blue);
	else
		retval = g_strdup ("none:");

        return retval;

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
	if (background->type == PANEL_BACK_IMAGE && !background->composited_image)
		retval = PANEL_BACK_NONE;

	return retval;
}
