#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gnome.h>
#include "gwmh.h"

/* so this stuff goes pixmap -> pixbuf -> pixmap, which sucks.
 * it doesn't need to, except for visuals etc.
 * someone who understands that stuff better should fix it
 */

static GdkPixbuf *tasklist_icon_check_mini (GwmhTask *task, GtkWidget *widget);
static GdkPixbuf *tasklist_icon_check_x (GwmhTask *task, GtkWidget *widget);

#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)

/* Shamelessly stolen from gwmh.c by Tim Janik */

Pixmap
tasklist_icon_get_pixmap (GwmhTask *task)
{
	XWMHints *wmhints;
	Pixmap pixmap;
	
	wmhints = XGetWMHints (GDK_DISPLAY (), task->xwin);

	if (!wmhints)
		return 0;
	
	if (!(wmhints->flags & IconPixmapHint)) {
		XFree (wmhints);
		return 0;
	}

	pixmap = wmhints->icon_pixmap;

	XFree (wmhints);

	return pixmap;
}

static gpointer
get_typed_property_data (Display *xdisplay,
			 Window   xwindow,
			 Atom     property,
			 Atom     requested_type,
			 gint    *size_p,
			 guint    expected_format)
{
	static const guint prop_buffer_lengh = 1024 * 1024;
	unsigned char *prop_data = NULL;
	Atom type_returned = 0;
	unsigned long nitems_return = 0, bytes_after_return = 0;
	int format_returned = 0;
	gpointer data = NULL;
	gboolean abort = FALSE;
	
	g_return_val_if_fail (size_p != NULL, NULL);
	*size_p = 0;
	
	gdk_error_trap_push ();
	
	abort = XGetWindowProperty (xdisplay,
				    xwindow,
				    property,
				    0, prop_buffer_lengh,
				    False,
				    requested_type,
				    &type_returned, &format_returned,
				    &nitems_return,
				    &bytes_after_return,
				    &prop_data) != Success;
	if (gdk_error_trap_pop () ||
	    type_returned == None)
		abort++;
	if (!abort &&
	    requested_type != AnyPropertyType &&
	    requested_type != type_returned) {
		g_warning (G_GNUC_PRETTY_FUNCTION "(): Property has wrong type, probably on crack");
		abort++;
	}
	if (!abort && bytes_after_return) {
			g_warning (G_GNUC_PRETTY_FUNCTION "(): Eeek, property has more than %u bytes, stored on harddisk?",
				   prop_buffer_lengh);
			abort++;
	}
	if (!abort && expected_format && expected_format != format_returned) {
		g_warning (G_GNUC_PRETTY_FUNCTION "(): Expected format (%u) unmatched (%d), programmer was drunk?",
			   expected_format, format_returned);
		abort++;
	}
	if (!abort && prop_data && nitems_return && format_returned) {
		switch (format_returned) {
		case 32:
			*size_p = nitems_return * 4;
			if (sizeof (gulong) == 8) {
				guint32 i, *mem = g_malloc0 (*size_p + 1);
				gulong *prop_longs = (gulong*) prop_data;
				
				for (i = 0; i < *size_p / 4; i++)
					mem[i] = prop_longs[i];
				data = mem;
			}
			break;
		case 16:
			*size_p = nitems_return * 2;
			break;
		case 8:
			*size_p = nitems_return;
			break;
		default:
			g_warning ("Unknown property data format with %d bits (extraterrestrial?)",
				   format_returned);
			break;
		}
		if (!data && *size_p) {
			guint8 *mem = g_malloc (*size_p + 1);
			
			memcpy (mem, prop_data, *size_p);
			mem[*size_p] = 0;
			data = mem;
		}
	}

	if (prop_data)
		XFree (prop_data);
	
	return data;
}

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
	GdkPixbuf *pixbuf;
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
	
	if (!atomdata[0])
		return NULL;

	/* Get icon size and depth */
	XGetGeometry (xdisplay, (Drawable)atomdata[0], &root, &x, &y,
		      &width, &height, &b, &depth);
	
	if (width > 65535 || height > 65535)
		return FALSE;


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
	
	if (atomdata[1]) {
		mask = gdk_pixmap_new (NULL, width, height, depth);
		gc = gdk_gc_new (mask);
		gdk_gc_set_background (gc, &widget->style->black);
		gdk_gc_set_foreground (gc, &widget->style->white);
		XCopyPlane (GDK_DISPLAY (), atomdata[1], GDK_WINDOW_XWINDOW (mask),
			    GDK_GC_XGC (gc), 0, 0, width, height, 0, 0, 1);
		gdk_gc_unref (gc);
		
		image = gdk_image_get (mask, 0, 0, width, height);
		g_return_val_if_fail (image != NULL, FALSE);
		
		pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
		
		data = gdk_pixbuf_get_pixels (pixbuf);
		for (y = 0; y < gdk_pixbuf_get_height (pixbuf); y++) 
			for (x = 0; x < gdk_pixbuf_get_width (pixbuf); x++) 
			{
				data += 3;
				*data++ = gdk_image_get_pixel (image, x, y) == 0 ? 0 : 255;
			}
		
		gdk_pixmap_unref (mask);
	}
	
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
	
	wmhints = XGetWMHints (GDK_DISPLAY (), task->xwin);

	if (!wmhints)
		return NULL;
	
	if (!(wmhints->flags & IconPixmapHint)) {
		XFree (wmhints);
		return NULL;
	}
	
	XGetGeometry (GDK_DISPLAY (), wmhints->icon_pixmap, &root,
		      &x, &y, &width, &height,
		      &border_width, &depth);
	
	if (width > 65535 || height > 65535) {
		XFree (wmhints);
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
		pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
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

	return scaled;
}

GtkWidget *
get_task_icon (GwmhTask *task, GtkWidget *widget)
{
	GtkPixmap *pixmap2;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GdkPixbuf *pixbuf;

	pixbuf = tasklist_icon_check_x (task, widget);
	if (!pixbuf) {
		pixbuf = tasklist_icon_check_mini (task, widget);

		if (!pixbuf)
			return NULL;
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
