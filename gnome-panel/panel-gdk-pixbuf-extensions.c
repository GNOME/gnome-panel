/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* panel-gdk-pixbuf-extensions.c: Routines to augment what's in gdk-pixbuf.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Darin Adler <darin@eazel.com>
            Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "panel-gdk-pixbuf-extensions.h"

#define PANEL_OPACITY_FULLY_TRANSPARENT 0
#define PANEL_OPACITY_FULLY_OPAQUE      255

typedef struct {
	GdkPixbuf *destination_pixbuf;
	int opacity;
	GdkInterpType interpolation_mode;
} PixbufTileData;

static const ArtIRect panel_art_irect_empty = { 0, 0, 0, 0 };

/**
 * panel_gdk_pixbuf_is_valid:
 * @pixbuf: A GdkPixbuf
 *
 * Return value: A boolean indicating whether the given pixbuf is valid.
 *
 * A pixbuf is valid if:
 * 
 *   1. It is non NULL
 *   2. It is has non NULL pixel data.
 *   3. It has width and height greater than 0.
 */
static gboolean
panel_gdk_pixbuf_is_valid (const GdkPixbuf *pixbuf)
{
	return ((pixbuf != NULL)
		&& (gdk_pixbuf_get_pixels (pixbuf) != NULL)
		&& (gdk_pixbuf_get_width (pixbuf) > 0)
		&& (gdk_pixbuf_get_height (pixbuf) > 0));
}

/**
 * panel_gdk_pixbuf_get_dimensions:
 * @pixbuf: A GdkPixbuf
 *
 * Return value: The dimensions of the pixbuf as a EelDimensions.
 *
 * This function is useful in code that uses libart rect 
 * intersection routines.
 */
static void
panel_gdk_pixbuf_get_dimensions (const GdkPixbuf *pixbuf,
				 int             *width,
				 int             *height)
{
	g_return_if_fail (panel_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (width != NULL);
	g_return_if_fail (height != NULL);

	*width  = gdk_pixbuf_get_width (pixbuf);
	*height = gdk_pixbuf_get_height (pixbuf);
}

/**
 * panel_gdk_pixbuf_intersect:
 * @pixbuf: A GdkPixbuf.
 * @pixbuf_x: X coordinate of pixbuf.
 * @pixbuf_y: Y coordinate of pixbuf.
 * @rectangle: An ArtIRect.
 *
 * Return value: The intersection of the pixbuf and the given rectangle.
 */
static ArtIRect
panel_gdk_pixbuf_intersect (const GdkPixbuf *pixbuf,
			    int pixbuf_x,
			    int pixbuf_y,
			    ArtIRect rectangle)
{
	ArtIRect intersection;
	ArtIRect bounds;
	int width = 0;
	int height = 0;

	g_return_val_if_fail (panel_gdk_pixbuf_is_valid (pixbuf), panel_art_irect_empty);

	panel_gdk_pixbuf_get_dimensions (pixbuf, &width, &height);

	bounds.x0 = pixbuf_x;
	bounds.y0 = pixbuf_y;
	bounds.x1 = pixbuf_x + width;
	bounds.y1 = pixbuf_y + height;

	art_irect_intersect (&intersection, &rectangle, &bounds);

	/* In theory, this is not needed because a rectangle is empty
	 * regardless of how MUCH negative the dimensions are.  
	 * However, to make debugging and self checks simpler, we
	 * consistenly return a standard empty rectangle.
	 */
	if (art_irect_empty (&intersection)) {
		return panel_art_irect_empty;
	}

	return intersection;
}

static void
pixbuf_destroy_callback (guchar  *pixels,
			 gpointer callback_data)
{
	g_return_if_fail (pixels != NULL);
	g_return_if_fail (callback_data != NULL);

	g_object_unref (callback_data);
}

/**
 * panel_gdk_pixbuf_new_from_pixbuf_sub_area:
 * @pixbuf: The source pixbuf.
 * @area: The area within the source pixbuf to use for the sub pixbuf.
 *        This area needs to be contained within the bounds of the 
 *        source pixbuf, otherwise it will be clipped to that. 
 *
 * Return value: A newly allocated pixbuf that shares the pixel data
 *               of the source pixbuf in order to represent a sub area.
 *
 * Create a pixbuf from a sub area of another pixbuf.  The resulting pixbuf
 * will share the pixel data of the source pixbuf.  Memory bookeeping is
 * all taken care for the caller.  All you need to do is gdk_pixbuf_unref()
 * the resulting pixbuf to properly free resources.
 */
static GdkPixbuf *
panel_gdk_pixbuf_new_from_pixbuf_sub_area (GdkPixbuf *pixbuf,
					   ArtIRect   area)
{
	GdkPixbuf *sub_pixbuf;
	ArtIRect target;
	guchar *pixels;
	
	g_return_val_if_fail (panel_gdk_pixbuf_is_valid (pixbuf), NULL);
	g_return_val_if_fail (!art_irect_empty (&area), NULL);

	/* Clip the pixbuf by the given area; bail if no work */
	target = panel_gdk_pixbuf_intersect (pixbuf, 0, 0, area);
	if (art_irect_empty (&target)) {
 		return NULL;
 	}

	/* Since we are going to be sharing the given pixbuf's data, we need 
	 * to ref it.  It will be unreffed in the destroy function above */
	g_object_ref (pixbuf);

	/* Compute the offset into the pixel data */
	pixels = 
		gdk_pixbuf_get_pixels (pixbuf)
		+ (target.y0 * gdk_pixbuf_get_rowstride (pixbuf))
		+ (target.x0 * (gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3));
	
	/* Make a pixbuf pretending its real estate is the sub area */
	sub_pixbuf = gdk_pixbuf_new_from_data (pixels,
					       GDK_COLORSPACE_RGB,
					       gdk_pixbuf_get_has_alpha (pixbuf),
					       8,
					       target.x1 - target.x0,
					       target.y1 - target.y0,
					       gdk_pixbuf_get_rowstride (pixbuf),
					       pixbuf_destroy_callback,
					       pixbuf);

	return sub_pixbuf;
}

/**
 * panel_gdk_pixbuf_draw_to_pixbuf:
 * @pixbuf: The source pixbuf to draw.
 * @destination_pixbuf: The destination pixbuf.
 * @source_x: The source pixbuf x coordiate to composite from.
 * @source_y: The source pixbuf y coordiate to composite from.
 * @destination_area: The destination area within the destination pixbuf.
 *                    This area will be clipped if invalid in any way.
 *
 * Copy one pixbuf onto another another..  This function has some advantages
 * over plain gdk_pixbuf_copy_area():
 *
 *   Composition paramters (source coordinate, destination area) are
 *   given in a way that is consistent with the rest of the extensions
 *   in this file.  That is, it matches the declaration of
 *   eel_gdk_pixbuf_draw_to_pixbuf_alpha() and 
 *   eel_gdk_pixbuf_draw_to_drawable() very closely.
 *
 *   All values are clipped to make sure they are valid.
 *
 */
static void
panel_gdk_pixbuf_draw_to_pixbuf (const GdkPixbuf *pixbuf,
				 GdkPixbuf       *destination_pixbuf,
				 int              source_x,
				 int              source_y,
				 ArtIRect         destination_area)
{
	ArtIRect target;
	ArtIRect source;
	int target_width;
	int target_height;
	int source_width;
	int source_height;
	int width = 0;
	int height = 0;
	
	g_return_if_fail (panel_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (panel_gdk_pixbuf_is_valid (destination_pixbuf));
	g_return_if_fail (!art_irect_empty (&destination_area));

	panel_gdk_pixbuf_get_dimensions (pixbuf, &width, &height);

	g_return_if_fail (source_x >= 0);
	g_return_if_fail (source_y >= 0);
	g_return_if_fail (source_x < width);
	g_return_if_fail (source_y < height);

	/* Clip the destination area to the pixbuf dimensions; bail if no work */
	target = panel_gdk_pixbuf_intersect (destination_pixbuf, 0, 0, destination_area);
	if (art_irect_empty (&target)) {
 		return;
 	}

	source.x0 = source_x;
	source.y0 = source_y;
	source.x1 = width;
	source.y1 = height;

	/* Adjust the target width if the source area is smaller than the
	 * source pixbuf dimensions */
	target_width = target.x1 - target.x0;
	target_height = target.y1 - target.y0;
	source_width = source.x1 - source.x0;
	source_height = source.y1 - source.y0;

	target.x1 = target.x0 + MIN (target_width, source_width);
	target.y1 = target.y0 + MIN (target_height, source_height);

	gdk_pixbuf_copy_area (pixbuf,
			      source.x0,
			      source.y0,
			      target.x1 - target.x0,
			      target.y1 - target.y0,
			      destination_pixbuf,
			      target.x0,
			      target.y0);
}

/**
 * panel_gdk_pixbuf_draw_to_pixbuf_alpha:
 * @pixbuf: The source pixbuf to draw.
 * @destination_pixbuf: The destination pixbuf.
 * @source_x: The source pixbuf x coordiate to composite from.
 * @source_y: The source pixbuf y coordiate to composite from.
 * @destination_area: The destination area within the destination pixbuf.
 *                    This area will be clipped if invalid in any way.
 * @opacity: The opacity of the drawn tiles where 0 <= opacity <= 255.
 * @interpolation_mode: The interpolation mode.  See <gdk-pixbuf.h>
 *
 * Composite one pixbuf over another.  This function has some advantages
 * over plain gdk_pixbuf_composite():
 *
 *   Composition paramters (source coordinate, destination area) are
 *   given in a way that is consistent with the rest of the extensions
 *   in this file.  That is, it matches the declaration of
 *   eel_gdk_pixbuf_draw_to_pixbuf() and 
 *   eel_gdk_pixbuf_draw_to_drawable() very closely.
 *
 *   All values are clipped to make sure they are valid.
 *
 *   Workaround a limitation in gdk_pixbuf_composite() that does not allow
 *   the source (x,y) to be greater than (0,0)
 * 
 */
static void
panel_gdk_pixbuf_draw_to_pixbuf_alpha (const GdkPixbuf *pixbuf,
				       GdkPixbuf *destination_pixbuf,
				       int source_x,
				       int source_y,
				       ArtIRect destination_area,
				       int opacity,
				       GdkInterpType interpolation_mode)
{
	ArtIRect target;
	ArtIRect source;
	int target_width;
	int target_height;
	int source_width;
	int source_height;
	int width = 0;
	int height = 0;

	g_return_if_fail (panel_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (panel_gdk_pixbuf_is_valid (destination_pixbuf));
	g_return_if_fail (!art_irect_empty (&destination_area));
	g_return_if_fail (opacity >= PANEL_OPACITY_FULLY_TRANSPARENT);
	g_return_if_fail (opacity <= PANEL_OPACITY_FULLY_OPAQUE);
	g_return_if_fail (interpolation_mode >= GDK_INTERP_NEAREST);
	g_return_if_fail (interpolation_mode <= GDK_INTERP_HYPER);
	
	panel_gdk_pixbuf_get_dimensions (pixbuf, &width, &height);

	g_return_if_fail (source_x >= 0);
	g_return_if_fail (source_y >= 0);
	g_return_if_fail (source_x < width);
	g_return_if_fail (source_y < height);

	/* Clip the destination area to the pixbuf dimensions; bail if no work */
	target = panel_gdk_pixbuf_intersect (destination_pixbuf, 0, 0, destination_area);
	if (art_irect_empty (&target)) {
 		return;
 	}

	source.x0 = source_x;
	source.y0 = source_y;
	source.x1 = width;
	source.y1 = height;
	
	/* Adjust the target width if the source area is smaller than the
	 * source pixbuf dimensions */
	target_width = target.x1 - target.x0;
	target_height = target.y1 - target.y0;
	source_width = source.x1 - source.x0;
	source_height = source.y1 - source.y0;

	target.x1 = target.x0 + MIN (target_width, source_width);
	target.y1 = target.y0 + MIN (target_height, source_height);
	
	/* If the source point is not (0,0), then we need to create a sub pixbuf
	 * with only the source area.  This is needed to work around a limitation
	 * in gdk_pixbuf_composite() that requires the source area to be (0,0). */
	if (source.x0 != 0 || source.y0 != 0) {
		ArtIRect area;

		width = width - source.x0;
		height = height - source.y0;
		
		area.x0 = source.x0;
		area.y0 = source.y0;
		area.x1 = area.x0 + width;
		area.y1 = area.y0 + height;
		
		pixbuf = panel_gdk_pixbuf_new_from_pixbuf_sub_area ((GdkPixbuf *) pixbuf, area);
	} else {
		g_object_ref (G_OBJECT (pixbuf));
	}
	
	gdk_pixbuf_composite (pixbuf,
			      destination_pixbuf,
			      target.x0,
			      target.y0,
			      target.x1 - target.x0,
			      target.y1 - target.y0,
			      target.x0,
			      target.y0,
			      1.0,
			      1.0,
			      interpolation_mode,
			      opacity);

	g_object_unref (G_OBJECT (pixbuf));
}

static void
draw_tile_to_pixbuf (const GdkPixbuf *pixbuf,
		     int              x,
		     int              y,
		     ArtIRect         area,
		     PixbufTileData  *pixbuf_tile_data)
{
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (pixbuf_tile_data != NULL);
	g_return_if_fail (!art_irect_empty (&area));

	if (pixbuf_tile_data->opacity == PANEL_OPACITY_FULLY_TRANSPARENT) {
		panel_gdk_pixbuf_draw_to_pixbuf (pixbuf,
						 pixbuf_tile_data->destination_pixbuf,
						 x,
						 y,
						 area);
	} else {
		panel_gdk_pixbuf_draw_to_pixbuf_alpha (pixbuf,
						       pixbuf_tile_data->destination_pixbuf,
						       x,
						       y,
						       area,
						       pixbuf_tile_data->opacity,
						       pixbuf_tile_data->interpolation_mode);
	}
}

static void
pixbuf_draw_tiled (const GdkPixbuf *pixbuf,
		   int width,
		   int height,
		   ArtIRect destination_area,
		   int tile_width,
		   int tile_height,
		   int tile_origin_x,
		   int tile_origin_y,
		   PixbufTileData *pixbuf_tile_data)
{
	ArtIRect target;
	int x;
	int y;
	int min_x;
	int min_y;
	int max_x;
	int max_y;
	int num_left;
	int num_above;
	ArtIRect clipped_destination_area;

	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (width > 0);
	g_return_if_fail (height > 0);
	g_return_if_fail (tile_width > 0);
	g_return_if_fail (tile_height > 0);
	g_return_if_fail (tile_width <= gdk_pixbuf_get_width (pixbuf));
	g_return_if_fail (tile_height <= gdk_pixbuf_get_height (pixbuf));
	g_return_if_fail (!art_irect_empty (&destination_area));

	/* FIXME: This is confusing.  Instead of passing in the destination_dimensions
	 *        I should just pass in the destination pixbuf, so that we can use
	 *        eel_gdk_pixbuf_intersect directly on that.
	 */
	clipped_destination_area.x0 = 0;
	clipped_destination_area.y0 = 0;
	clipped_destination_area.x1 = width;
	clipped_destination_area.y1 = height;

	art_irect_intersect (&target, &destination_area, &clipped_destination_area);
	if (art_irect_empty (&target)) {
		return;
	}

	/* The number of tiles left and above the target area */
	num_left = (target.x0 - tile_origin_x) / tile_width;
	num_above = (target.y0 - tile_origin_y) / tile_height;
	
	min_x = tile_origin_x - tile_width + (num_left * tile_width);
	min_y = tile_origin_y - tile_height + (num_above * tile_height);
	
	max_x = (target.x1 + 2 * tile_width);
	max_y = (target.y1 + 2 * tile_height);
	
	for (y = min_y; y <= max_y; y += tile_height) {
		for (x = min_x; x <= max_x; x += tile_width) {
			ArtIRect current;
			ArtIRect area;

			current.x0 = x;
			current.y0 = y;
			current.x1 = x + tile_width;
			current.y1 = y + tile_height;

			/* FIXME: A potential speed improvement here would be to clip only the
			 * first and last rectangles, not the ones in between.  
			 */
			art_irect_intersect (&area, &target, &current);

			if (!art_irect_empty (&area)) {
				g_assert (area.x0 >= x);
				g_assert (area.y0 >= y);

				draw_tile_to_pixbuf (pixbuf,
						     area.x0 - x,
						     area.y0 - y,
						     area,
						     pixbuf_tile_data);
			}
		}
	}
}

/**
 * panel_gdk_pixbuf_draw_to_pixbuf_tiled:
 * @pixbuf: Source tile pixbuf.
 * @destination_pixbuf: Destination pixbuf.
 * @destination_area: Area of the destination pixbuf to tile.
 * @tile_width: Width of the tile.  This can be less than width of the
 *              tile pixbuf, but not greater.
 * @tile_height: Height of the tile.  This can be less than width of the
 *               tile pixbuf, but not greater.
 * @tile_origin_x: The x coordinate of the tile origin.  Can be negative.
 * @tile_origin_y: The y coordinate of the tile origin.  Can be negative.
 * @opacity: The opacity of the drawn tiles where 0 <= opacity <= 255.
 * @interpolation_mode: The interpolation mode.  See <gdk-pixbuf.h>
 *
 * Fill an area of a GdkPixbuf with a tile.
 */
void
panel_gdk_pixbuf_draw_to_pixbuf_tiled (const GdkPixbuf *pixbuf,
				       GdkPixbuf       *destination_pixbuf,
				       ArtIRect         destination_area,
				       int              tile_width,
				       int              tile_height,
				       int              tile_origin_x,
				       int              tile_origin_y,
				       int              opacity,
				       GdkInterpType    interpolation_mode)
{
	PixbufTileData pixbuf_tile_data;
	int            width = 0;
	int            height = 0;

	g_return_if_fail (panel_gdk_pixbuf_is_valid (destination_pixbuf));
	g_return_if_fail (panel_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (tile_width > 0);
	g_return_if_fail (tile_height > 0);
	g_return_if_fail (tile_width <= gdk_pixbuf_get_width (pixbuf));
	g_return_if_fail (tile_height <= gdk_pixbuf_get_height (pixbuf));
	g_return_if_fail (opacity >= PANEL_OPACITY_FULLY_TRANSPARENT);
	g_return_if_fail (opacity <= PANEL_OPACITY_FULLY_OPAQUE);
	g_return_if_fail (interpolation_mode >= GDK_INTERP_NEAREST);
	g_return_if_fail (interpolation_mode <= GDK_INTERP_HYPER);

	panel_gdk_pixbuf_get_dimensions (destination_pixbuf, &width, &height);

	pixbuf_tile_data.destination_pixbuf = destination_pixbuf;
	pixbuf_tile_data.opacity = opacity;
	pixbuf_tile_data.interpolation_mode = interpolation_mode;

	pixbuf_draw_tiled (pixbuf,
			   width,
			   height,
			   destination_area,
			   tile_width,
			   tile_height,
			   tile_origin_x,
			   tile_origin_y,
			   &pixbuf_tile_data);
}
