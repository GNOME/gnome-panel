#ifndef RGB_STUFF_H
#define RGB_STUFF_H

#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_alphagamma.h>

/* combine rgba onto the dest (given the dest has something in it already) */
void combine_rgb_rgba(guchar *dest, int dx, int dy, int dw, int dh, int drs,
		      guchar *rgba, int rw, int rh, int rrs);
/* tile an rgb onto dest */
void tile_rgb(guchar *dest, int dw, int dh, int offx, int offy, int drs,
	      guchar *tile, int w, int h, int rowstride, int has_alpha);
/* tile an GdkPixBuf onto an rgb buffer*/
void tile_rgb_pixbuf(guchar *dest, int dw, int dh, int offx, int offy, int drs,
		     GdkPixbuf *pbuf, int scale_w, int scale_h, int rotate);

#if 0
/* scale a w by h pixmap into a square of size 'size', optionally rerurn
   outw and outh */
void make_scale_affine(double affine[], int w, int h, int size,
		       int *outw, int *outh);
#endif

GdkPixbuf * scale_pixbuf_to_square (GdkPixbuf *pb, int size, int *outw, int *outh, GdkInterpType interp);

#if 0
void rgb_rotate270(guchar *dest, int drs, guchar *src, int w, int h, int srs);
void rgba_rotate270(guchar *dest, int drs, guchar *src, int w, int h, int srs);
#endif

/* A little function to help me with the right _affine call */
void transform_pixbuf(guchar *dst, int x0, int y0, int x1, int y1, int drs, 
		      GdkPixbuf *pixbuf, double affine[6],
		      int level, ArtAlphaGamma *ag);

#endif /* RGB_STUFF_H */
