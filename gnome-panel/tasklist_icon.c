/*
 * This has been stolen from the tasklist and then greatly wacked into
 * shape by jacob and me.  This code needs some nicer place to live then
 * like this
 */
#include "config.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gnome.h>
#include "gwmh.h"
#include "xstuff.h"

#include "tasklist_icon.h"

/* so this stuff goes pixmap -> pixbuf -> pixmap, which sucks.
 * it doesn't need to, except for visuals etc.
 * someone who understands that stuff better should fix it
 */

static GdkPixbuf *tasklist_icon_check_mini (GwmhTask *task, GtkWidget *widget);
static GdkPixbuf *tasklist_icon_check_x (GwmhTask *task, GtkWidget *widget);

#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)

/* Shamelessly stolen from gwmh.c by Tim Janik */

static GdkPixbuf *
tasklist_icon_check_mini (GwmhTask *task,  GtkWidget *widget)
{
	GdkGC *gc;
	int x, y, b, width, height, depth;
	guint32 *atomdata;
	Window root;
	GdkImage *image;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GdkPixbuf *pixbuf, *pixbuf2;
	gint size;
	static gulong KWM_WIN_ICON = 0;
	Display *xdisplay;
	guchar *data;
	
	if (!KWM_WIN_ICON) {
		KWM_WIN_ICON = gdk_atom_intern ("KWM_WIN_ICON", FALSE);
	}

	xdisplay = GDK_WINDOW_XDISPLAY (task->gdkwindow);
	
	atomdata = get_typed_property_data (xdisplay,
					    task->xwin,
					    KWM_WIN_ICON,
					    KWM_WIN_ICON,
					    &size,
					    32);
	if (!atomdata)
		return NULL;
	
	if (!atomdata[0]) {
		g_free (atomdata);
		return NULL;
	}

	gdk_error_trap_push ();

	/* Get icon size and depth */
	XGetGeometry (xdisplay, (Drawable)atomdata[0], &root, &x, &y,
		      &width, &height, &b, &depth);
	
	if (gdk_error_trap_pop () != 0 ||
	    width > 65535 || height > 65535)
		return FALSE;


	gdk_error_trap_push ();

	/* Create a new GdkPixmap and copy the mini icon pixmap to it */
	pixmap = gdk_pixmap_new (widget->window, width, height, depth);
	gc = gdk_gc_new (pixmap);
	XCopyArea (GDK_WINDOW_XDISPLAY (pixmap), atomdata[0], GDK_WINDOW_XWINDOW (pixmap),
		   GDK_GC_XGC (gc), 0, 0, width, height, 0, 0);
	gdk_gc_destroy (gc);
	
	pixbuf = gdk_pixbuf_get_from_drawable (NULL,
					       pixmap,
					       gtk_widget_get_colormap (widget),
					       0, 0,
					       0, 0,
					       width, height);
	gdk_pixmap_unref (pixmap);
	
	if (size > 1 && atomdata[1]) {
		mask = gdk_pixmap_new (widget->window, width, height, depth);
		gc = gdk_gc_new (mask);
		gdk_gc_set_background (gc, &widget->style->black);
		gdk_gc_set_foreground (gc, &widget->style->white);
		XCopyPlane (GDK_DISPLAY (), atomdata[1], GDK_WINDOW_XWINDOW (mask),
			    GDK_GC_XGC (gc), 0, 0, width, height, 0, 0, 1);
		gdk_gc_unref (gc);
		
		image = gdk_image_get (mask, 0, 0, width, height);
		g_return_val_if_fail (image != NULL, FALSE);
	
		pixbuf2 = pixbuf;
		pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
		gdk_pixbuf_unref(pixbuf2);
		
		data = gdk_pixbuf_get_pixels (pixbuf);
		for (y = 0; y < gdk_pixbuf_get_height (pixbuf); y++) 
			for (x = 0; x < gdk_pixbuf_get_width (pixbuf); x++) 
			{
				data += 3;
				*data++ = gdk_image_get_pixel (image, x, y) == 0 ? 0 : 255;
			}
		gdk_pixmap_unref (mask);
		gdk_image_destroy(image);	
	}

	gdk_flush ();
	gdk_error_trap_pop ();

	g_free (atomdata);
	
	return pixbuf;
}

static GdkPixbuf *
tasklist_icon_check_x (GwmhTask *task,  GtkWidget *widget)
{
	XWMHints * wmhints;
	GdkPixmap *pixmap, *mask;
	GdkPixbuf *pixbuf, *scaled;
	GdkImage *image;
	GdkGC *gc;
	Window root;
	int x, y;
	unsigned int width, height;
	unsigned int border_width;
	unsigned int depth;
	guchar *data;

	gdk_error_trap_push ();
	
	wmhints = XGetWMHints (GDK_DISPLAY (), task->xwin);

	if (!wmhints) {
		gdk_flush ();
		gdk_error_trap_pop ();
		return NULL;
	}
	
	if (!(wmhints->flags & IconPixmapHint)) {
		XFree (wmhints);
		gdk_flush ();
		gdk_error_trap_pop ();
		return NULL;
	}
	
	XGetGeometry (GDK_DISPLAY (), wmhints->icon_pixmap, &root,
		      &x, &y, &width, &height,
		      &border_width, &depth);
	
	if (width > 65535 || height > 65535) {
		XFree (wmhints);
		gdk_flush ();
		gdk_error_trap_pop ();
		return NULL;
	}
	
	pixmap = gdk_pixmap_new (widget->window, width, height, -1);	
	gc = gdk_gc_new (pixmap);

	if (depth == 1) {
		gdk_gc_set_background (gc, &widget->style->white);
		gdk_gc_set_foreground (gc, &widget->style->black);
		XCopyPlane (GDK_DISPLAY (), wmhints->icon_pixmap, GDK_WINDOW_XWINDOW (pixmap),
			   GDK_GC_XGC (gc), 0, 0, width, height, 0, 0, 1);
	}
	else {
		XCopyArea (GDK_DISPLAY (), wmhints->icon_pixmap, GDK_WINDOW_XWINDOW (pixmap),
			   GDK_GC_XGC (gc), 0, 0, width, height, 0, 0);
	}
	pixbuf = gdk_pixbuf_get_from_drawable (NULL, pixmap, 
					       gtk_widget_get_colormap (widget), 
					       0, 0,
					       0, 0, width, height);
	gdk_gc_destroy (gc);

	if (depth == 1) {
		scaled = pixbuf;
		pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
		gdk_pixbuf_unref(scaled);
	}
	
	if (wmhints->flags & IconMaskHint) {
	       mask = gdk_pixmap_new (widget->window, width, height, depth);
	       gc = gdk_gc_new (mask);
	       gdk_gc_set_background (gc, &widget->style->black);
	       gdk_gc_set_foreground (gc, &widget->style->white);
	       XCopyPlane (GDK_DISPLAY (), wmhints->icon_mask, GDK_WINDOW_XWINDOW (mask),
			   GDK_GC_XGC (gc), 0, 0, width, height, 0, 0, 1);
	       gdk_gc_unref (gc);

	       image = gdk_image_get (mask, 0, 0, width, height);
	       g_return_val_if_fail (image != NULL, FALSE);
	       
	       scaled = pixbuf;
	       pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
	       gdk_pixbuf_unref (scaled);

	       data = gdk_pixbuf_get_pixels (pixbuf);
	       for (y = 0; y < gdk_pixbuf_get_height (pixbuf); y++)
		 for (x = 0; x < gdk_pixbuf_get_width (pixbuf); x++) 
		   {
		     data += 3;
		     *data++ = gdk_image_get_pixel (image, x, y) == 0 ? 0 : 255;
		   }

	       gdk_pixmap_unref (mask);
	       gdk_image_destroy (image);
	}

	scaled = gdk_pixbuf_scale_simple (pixbuf, 20, 20,
					  GDK_INTERP_BILINEAR);
	gdk_pixbuf_unref (pixbuf);
	gdk_pixmap_unref (pixmap);
	
	XFree (wmhints);

	gdk_flush ();
	gdk_error_trap_pop ();

	return scaled;
}

static GdkPixbuf *
tasklist_icon_create_minimized_icon (GtkWidget *widget, GdkPixbuf *pixbuf)
{
        GdkPixbuf *target;
        gint i, j;
        gint width, height, has_alpha, rowstride;
        guchar *target_pixels;
        guchar *original_pixels;
        guchar *dest_pixel, *src_pixel;
        gint32 red, green, blue;
        GdkColor color;
        
        color = widget->style->bg[GTK_STATE_NORMAL];
        red = color.red / 255;
        blue = color.blue / 255;
        green = color.green / 255;
        
        has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
        width = gdk_pixbuf_get_width (pixbuf);
        height = gdk_pixbuf_get_height (pixbuf);
        rowstride = gdk_pixbuf_get_rowstride (pixbuf);

        target = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                 has_alpha,
                                 gdk_pixbuf_get_bits_per_sample (pixbuf),
                                 width, height);

        target_pixels = gdk_pixbuf_get_pixels (target);
        original_pixels = gdk_pixbuf_get_pixels (pixbuf);

        for (i = 0; i < height; i++) {
                for (j = 0; j < width; j++) {
                                src_pixel = original_pixels + i*rowstride + j*(has_alpha?4:3);
                                dest_pixel = target_pixels + i*rowstride + j*(has_alpha?4:3);

                                dest_pixel[0] = ((src_pixel[0] - red) >> 1) + red;
                                dest_pixel[1] = ((src_pixel[1] - green) >> 1) + green;
                                dest_pixel[2] = ((src_pixel[2] - blue) >> 1) + blue;
                                
                                if (has_alpha)
                                        dest_pixel[3] = src_pixel[3];

                }
        }
        
        return target;
}

GtkWidget *
get_task_icon (GwmhTask *task, GtkWidget *widget)
{
	GtkWidget *pixmap2;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GdkPixbuf *pixbuf;
	GdkPixbuf *pixbuf2;
	
	pixbuf = tasklist_icon_check_x (task, widget);
	if (!pixbuf) {
		pixbuf = tasklist_icon_check_mini (task, widget);

		if (!pixbuf)
			return NULL;
	}
	if (GWMH_TASK_ICONIFIED (task)) {
		pixbuf2 = tasklist_icon_create_minimized_icon (widget, 
								pixbuf);
		gdk_pixbuf_unref (pixbuf);
		pixbuf = pixbuf2;
	}

	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 128);

	gdk_pixbuf_unref (pixbuf);

	if (!pixmap)
		return NULL;
	
	pixmap2 = gtk_pixmap_new (pixmap, mask);
	gtk_widget_show (pixmap2);

	gdk_pixmap_unref (pixmap);
	if (mask)
		gdk_bitmap_unref (mask);

	return pixmap2;
}
