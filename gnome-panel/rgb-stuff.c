/*
 * RGB code for drawing stuff on the panel
 *
 * Code taken from gdk-pixbuf, so parts of it are authored by:
 *   Cody Russell <bratsche@dfw.net>
 * Actually I don't think it contains much if any of this code any more.
 * Rest is:
 *   George Lebl <jirka@5z.com>
 */
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_filterlevel.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>
#include "rgb-stuff.h"

void
combine_rgb_rgba(guchar *dest, int dx, int dy, int dw, int dh, int drs,
		 guchar *rgba, int rw, int rh, int rrs)
{
	int r, g, b, a;
	int i,j;
	guchar *imgrow;
	guchar *dstrow;

	for(j=0;j<dh;j++) {
		imgrow = rgba + j * rrs;
		dstrow = dest + (dy+j) * drs + (dx*3);
		for(i=0;i<dw;i++) {
			r = *(imgrow++);
			g = *(imgrow++);
			b = *(imgrow++);
			a = *(imgrow++);
			*dstrow = (r*a + *dstrow * (255-a))>>8;
			dstrow++;
			*dstrow = (g*a + *dstrow * (255-a))>>8;
			dstrow++;
			*dstrow = (b*a + *dstrow * (255-a))>>8;
			dstrow++;
		}
	}
}

void
tile_rgb(guchar *dest, int dw, int dh, int offx, int offy, int drs,
	 guchar *tile, int w, int h, int rowstride, int has_alpha)
{
	guchar *p;
	int i,j,x,y,a;
	guchar *imgrow;
	int off;

	p = dest;
	y = offy % h;
	off = drs - (dw*3); /*the space after width ends until next row*/
	for(j=0;j<dh;j++) {
		x = offx % w;
		imgrow = tile + y * rowstride + (has_alpha?/*x*4*/x<<2:x*3);
		for(i=0;i<dw;i++) {
			*(p++) = *(imgrow++);
			*(p++) = *(imgrow++);
			*(p++) = *(imgrow++);
			if(has_alpha) {
				a = *(imgrow++);
				if(a!=255) {
					guchar *pp = p-3;
					pp[0] = (pp[0]*a)>>8;
					pp[1] = (pp[1]*a)>>8;
					pp[2] = (pp[2]*a)>>8;
				}
			}
			x++;
			if(x>=w) {
				x = 0;
				imgrow = tile + y * rowstride;
			}
		}
		p += off;
		y++;
		if(y>=h)
			y = 0;
	}
}

void
tile_rgb_pixbuf(guchar *dest, int dw, int dh, int offx, int offy, int drs,
		GdkPixbuf *pbuf, int scale_w, int scale_h, int rotate)
{
	int i,j;

	gdouble scaleaff[6];
	gdouble affine[6];
	
	scaleaff[1] = scaleaff[2] = scaleaff[4] = scaleaff[5] = 0;

	scaleaff[0] = scale_w / (double)(gdk_pixbuf_get_width(pbuf));
	scaleaff[3] = scale_h / (double)(gdk_pixbuf_get_height(pbuf));

	if(rotate) {
		int tmp;

		art_affine_rotate(affine,270);
		art_affine_multiply(scaleaff,scaleaff,affine);
		art_affine_translate(affine,0,scale_w);
		art_affine_multiply(scaleaff,scaleaff,affine);
		
		tmp = scale_h;
		scale_h = scale_w;
		scale_w = tmp;
	}
	
	for(i=-(offx%scale_w);i<dw;i+=scale_w) {
		for(j=-(offy%scale_h);j<dh;j+=scale_h) {
			art_affine_translate(affine,i,j);
			art_affine_multiply(affine,scaleaff,affine);
			if(gdk_pixbuf_get_has_alpha(pbuf))
				art_rgb_rgba_affine(dest,
					       0,0,dw,dh,drs,
					       gdk_pixbuf_get_pixels(pbuf),
					       gdk_pixbuf_get_width(pbuf),
					       gdk_pixbuf_get_height(pbuf),
					       gdk_pixbuf_get_rowstride(pbuf),
					       affine,ART_FILTER_NEAREST,NULL);
			else
				art_rgb_affine(dest,
					       0,0,dw,dh,drs,
					       gdk_pixbuf_get_pixels(pbuf),
					       gdk_pixbuf_get_width(pbuf),
					       gdk_pixbuf_get_height(pbuf),
					       gdk_pixbuf_get_rowstride(pbuf),
					       affine,ART_FILTER_NEAREST,NULL);

		}
	}
}

#if 0
void
make_scale_affine(double affine[], int w, int h, int size, int *outw, int *outh)
{
	int oh, ow;
	oh = h;
	ow = w;
	if(w > h) {
		h = h*((double)size/w);
		w = size;
	} else {
		w = w*((double)size/h);
		h = size;
	}
	w = w > 0 ? w : 1;
	h = h > 0 ? h : 1;

	affine[1] = affine[2] = affine[4] = affine[5] = 0;

	affine[0] = w / (double)ow;
	affine[3] = h / (double)oh;

	if(outw)
		*outw = w;
	if(outh)
		*outh = h;
}
#endif

GdkPixbuf *
scale_pixbuf_to_square (GdkPixbuf *pb, int size, int *outw, int *outh, GdkInterpType interp)
{
	GdkPixbuf *new_pb;
	int width, height;
	int new_width, new_height;

	new_width = width = gdk_pixbuf_get_width (pb);
	new_height = height = gdk_pixbuf_get_height (pb);

	if(new_width > new_height) {
		new_height = new_height * (size / (double)new_width);
		new_width = size;
	} else {
		new_width = new_width * (size / (double)new_height);
		new_height = size;
	}

	new_width = new_width > 0 ? new_width : 1;
	new_height = new_height > 0 ? new_height : 1;

	if(outw)
		*outw = new_width;
	if(outh)
		*outh = new_height;

	if (new_width == width && new_height == height)
		return gdk_pixbuf_ref (pb);

	new_pb = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(pb),
				gdk_pixbuf_get_has_alpha(pb),
				gdk_pixbuf_get_bits_per_sample(pb),
				new_width,
				new_height);

	gdk_pixbuf_scale (pb, new_pb, 0, 0, new_width, new_height, 0.0, 0.0,
			  new_width / (double)width,
			  new_height / (double)height,
			  interp);

	return new_pb;
}

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



#if 0
void
rgb_rotate270(guchar *dest, int drs, guchar *src, int w, int h, int srs)
{
	guchar *p;
	guchar *sp;

	int i,j;

	for(j=0;j<h;j++) {
		sp = src + j*srs;
		p = dest + j*3;
		for(i=0;i<w;i++) {
			*(p++) = *(sp++);
			*(p++) = *(sp++);
			*p = *(sp++);
			p += drs-2;
		}
	}
}

void
rgba_rotate270(guchar *dest, int drs, guchar *src, int w, int h, int srs)
{
	guchar *p;
	guchar *sp;

	int i,j;

	for(j=0;j<h;j++) {
		sp = src + j*srs;
		p = dest + j*4;
		for(i=0;i<w;i++) {
			*(p++) = *(sp++);
			*(p++) = *(sp++);
			*(p++) = *(sp++);
			*p = *(sp++);
			p += drs-3;
		}
	}
}
#endif
