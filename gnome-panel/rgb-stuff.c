/*
 * RGB code for drawing stuff on the panel
 *
 * Author:
 *   George Lebl <jirka@5z.com>
 */
#include <config.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>
#include "rgb-stuff.h"

void
transform_pixbuf(guchar *dst, int x0, int y0, int x1, int y1, int drs,
		 GdkPixbuf *pixbuf, double affine[6],
		 int level, ArtAlphaGamma *ag)
{
	gint w, h, rs;

	rs = gdk_pixbuf_get_rowstride(pixbuf);
	h =  gdk_pixbuf_get_height(pixbuf);
	w =  gdk_pixbuf_get_width(pixbuf);

        if (gdk_pixbuf_get_has_alpha(pixbuf)) { 
	        art_rgb_rgba_affine(dst, x0, y0, x1, y1, drs,
                                    gdk_pixbuf_get_pixels(pixbuf),
                                    w, h, rs, affine, level, ag);
        } else {
                art_rgb_affine(dst, x0, y0, x1, y1, drs,
                               gdk_pixbuf_get_pixels(pixbuf),
                               w, h, rs, affine, level, ag);
	}
}
