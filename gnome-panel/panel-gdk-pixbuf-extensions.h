/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* panel-gdk-pixbuf-extensions.h: Routines to augment what's in gdk-pixbuf
                                  copied from eel and should go away in time
                                  if they get into gdk-pixbuf or we depend
                                  on eel.

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

#ifndef PANEL_GDK_PIXBUF_EXTENSIONS_H
#define PANEL_GDK_PIXBUF_EXTENSIONS_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_rect.h>

/* Fill an area of a pixbuf with a tile. */
void panel_gdk_pixbuf_draw_to_pixbuf_tiled (const GdkPixbuf       *pixbuf,
					    GdkPixbuf             *destination_pixbuf,
					    ArtIRect               destination_area,
					    int                    tile_width,
					    int                    tile_height,
					    int                    tile_origin_x,
					    int                    tile_origin_y,
					    int                    opacity,
					    GdkInterpType          interpolation_mode);

#endif /* PANEL_GDK_PIXBUF_EXTENSIONS_H */
