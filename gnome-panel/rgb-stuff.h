#ifndef RGB_STUFF_H
#define RGB_STUFF_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_alphagamma.h>

/* A little function to help me with the right _affine call */
void transform_pixbuf(guchar *dst, int x0, int y0, int x1, int y1, int drs, 
		      GdkPixbuf *pixbuf, double affine[6],
		      int level, ArtAlphaGamma *ag);

#endif /* RGB_STUFF_H */
