#include <config.h>
#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_filterlevel.h>
#include "button-widget.h"
#include "panel-widget.h"
#include "basep-widget.h"
#include "panel-types.h"
#include "panel-util.h"
#include "panel-config-global.h"
#include "rgb-stuff.h"

#undef BUTTON_WIDGET_DEBUG

#define BUTTON_WIDGET_DISPLACEMENT 5

extern GlobalConfig global_config;

static void     button_widget_class_init     (ButtonWidgetClass *klass);
static void     button_widget_instance_init  (ButtonWidget      *button);
static void     button_widget_size_request   (GtkWidget         *widget,
					      GtkRequisition    *requisition);
static void     button_widget_size_allocate  (GtkWidget         *widget,
					      GtkAllocation     *allocation);
static void     button_widget_realize        (GtkWidget         *widget);
static void     button_widget_unrealize      (GtkWidget         *widget);
static void     button_widget_map            (GtkWidget         *widget);
static void     button_widget_unmap          (GtkWidget         *widget);
static gboolean button_widget_expose         (GtkWidget         *widget,
					      GdkEventExpose    *event);
static void     button_widget_destroy        (GtkObject         *obj);
static gboolean button_widget_button_press   (GtkWidget         *widget,
					      GdkEventButton    *event);
static gboolean button_widget_button_release (GtkWidget         *widget,
					      GdkEventButton    *event);
static gboolean button_widget_enter_notify   (GtkWidget         *widget,
					      GdkEventCrossing  *event);
static gboolean button_widget_leave_notify   (GtkWidget         *widget,
					      GdkEventCrossing  *event);
static void     button_widget_pressed        (ButtonWidget      *button);
static void     button_widget_unpressed      (ButtonWidget      *button);


static GtkWidgetClass *button_widget_parent_class;

GType
button_widget_get_type (void)
{
	static GType object_type = 0;

	if (object_type == 0) {
		static const GTypeInfo object_info = {
			sizeof (ButtonWidgetClass),
                    	(GBaseInitFunc)         NULL,
                    	(GBaseFinalizeFunc)     NULL,
                    	(GClassInitFunc)        button_widget_class_init,
                    	NULL,                   /* class_finalize */
                    	NULL,                   /* class_data */
                    	sizeof (ButtonWidget),
                    	0,                      /* n_preallocs */
                    	(GInstanceInitFunc)     button_widget_instance_init

		};

		object_type = g_type_register_static (GTK_TYPE_WIDGET, "ButtonWidget", &object_info, 0);
		button_widget_parent_class = g_type_class_ref (GTK_TYPE_WIDGET);
	}

	return object_type;
}

enum {
	CLICKED_SIGNAL,
	PRESSED_SIGNAL,
	UNPRESSED_SIGNAL,
	LAST_SIGNAL
};

static guint button_widget_signals[LAST_SIGNAL] = {0};

static void
button_widget_class_init (ButtonWidgetClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	button_widget_signals[CLICKED_SIGNAL] =
		g_signal_new ("clicked",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (ButtonWidgetClass, clicked),
                              NULL,
                              NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	button_widget_signals[PRESSED_SIGNAL] =
		g_signal_new ("pressed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (ButtonWidgetClass, pressed),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);

	button_widget_signals[UNPRESSED_SIGNAL] =
		g_signal_new ("unpressed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (ButtonWidgetClass, unpressed),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
	
	class->clicked = NULL;
	class->pressed = button_widget_pressed;
	class->unpressed = button_widget_unpressed;

	gtk_object_class->destroy = button_widget_destroy;
	  
	widget_class->realize = button_widget_realize;
	widget_class->unrealize = button_widget_unrealize;
	widget_class->map = button_widget_map;
	widget_class->unmap = button_widget_unmap;
	widget_class->size_allocate = button_widget_size_allocate;
	widget_class->size_request = button_widget_size_request;
	widget_class->button_press_event = button_widget_button_press;
	widget_class->button_release_event = button_widget_button_release;
	widget_class->enter_notify_event = button_widget_enter_notify;
	widget_class->leave_notify_event = button_widget_leave_notify;
	widget_class->expose_event = button_widget_expose;
}
 
static void
translate_to(GtkWidget *from, GtkWidget *to, int *x, int *y)
{
	while (from != to) {
		if (!GTK_WIDGET_NO_WINDOW (from)) {
			*x += from->allocation.x;
			*y += from->allocation.y;
		}
		from = from->parent;
	}
}

static GtkWidget *
get_frame(BasePWidget *basep)
{
	if (GTK_WIDGET_VISIBLE (basep->frame)) {
		return basep->frame;
	} else {
		return basep->innerebox;
	}
}

static void
calculate_overlay_geometry (PanelWidget *panel, GtkWidget *parent,
			    GtkWidget *applet, int *x, int *y, int *w, int *h)
{
	*x = applet->allocation.x;
	*y = applet->allocation.y;
	*w = applet->allocation.width;
	*h = applet->allocation.height;

	translate_to (GTK_WIDGET(panel), parent, x, y);

	/* when not shown, things are somewhat weird, and we try to put the
	 * window completely off as it can't be clickable anyway */
	/* XXX: These window thingies should really be unmapped in hidden
	 * case, or something like that, this is ugly, but who gives a fuck,
	 * this is all going to be rewritten soon (famous last words?) */
	if(BASEP_IS_WIDGET(parent) &&
	   BASEP_WIDGET(parent)->state != BASEP_SHOWN &&
	   BASEP_WIDGET(parent)->state != BASEP_AUTO_HIDDEN) {
		*x = parent->requisition.width + 1;
		*y = parent->requisition.height + 1;
		return;
	}

	if(panel->orient == GTK_ORIENTATION_HORIZONTAL) {
		if (applet->allocation.x > panel->size) {
			*x = parent->requisition.width + 1;
			*y = parent->requisition.height + 1;
			return;
		}

		*y = 0;
		/* we use the requisition, since allocation might have not
		   yet happened if we are inside the allocation, anyway
		   they are the same for basep */
		*h = parent->requisition.height;

		if ((*w + applet->allocation.x) > panel->size) {
			*w = panel->size - applet->allocation.x;
		}

		if ( ! BASEP_IS_WIDGET(parent)) {
			/*don't do the edge flushing on foobar*/
			return;
		}

		/* if on the edge (only if padding is 0)
		   then make the thing flush with the innerebox or frame
		   of the basep */
		if(applet->allocation.x == 0) {
			GtkWidget *frame = get_frame(BASEP_WIDGET(parent));
			*w += (*x - frame->allocation.x);
			*x = frame->allocation.x;
		} else if(applet->allocation.x + *w == panel->size) {
			GtkWidget *frame = get_frame(BASEP_WIDGET(parent));
			*w = frame->allocation.width + frame->allocation.x - *x;
		}
	} else {
		if (applet->allocation.y > panel->size) {
			*x = parent->requisition.width + 1;
			*y = parent->requisition.height + 1;
			return;
		}

		*x = 0;
		*w = parent->requisition.width;

		if ((*h + applet->allocation.y) > panel->size) {
			*h = panel->size - applet->allocation.y;
		}

		if ( ! BASEP_IS_WIDGET(parent)) {
			/*don't do the edge flushing on foobar*/
			return;
		}

		/* if on the edge (only if padding is 0)
		   then make the thing flush with the innerbox of frame
		   of the basep */
		if(applet->allocation.y == 0) {
			GtkWidget *frame = get_frame(BASEP_WIDGET(parent));
			*h += (*y - frame->allocation.y);
			*y = frame->allocation.y;
		} else if(applet->allocation.y + *h == panel->size) {
			GtkWidget *frame = get_frame(BASEP_WIDGET(parent));
			*h = frame->allocation.height + frame->allocation.y - *y;
		}
	}
}

static void
button_widget_realize(GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	ButtonWidget *button_widget;
	PanelWidget *panel;
	GtkWidget *parent;
	int x,y,w,h;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (BUTTON_IS_WIDGET (widget));

	panel = PANEL_WIDGET(widget->parent);
	parent = panel->panel_parent;

	calculate_overlay_geometry(panel, parent, widget, &x, &y, &w, &h);

	button_widget = BUTTON_WIDGET (widget);

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = x;
	attributes.y = y;
	attributes.width = w;
	attributes.height = h;
	attributes.wclass = GDK_INPUT_ONLY;
	attributes.event_mask = (GDK_BUTTON_PRESS_MASK |
				 GDK_BUTTON_RELEASE_MASK |
				 GDK_POINTER_MOTION_MASK |
				 GDK_POINTER_MOTION_HINT_MASK |
				 GDK_KEY_PRESS_MASK |
				 GDK_ENTER_NOTIFY_MASK |
				 GDK_LEAVE_NOTIFY_MASK);
	attributes_mask = GDK_WA_X | GDK_WA_Y;

	widget->window = gtk_widget_get_parent_window(widget);
	g_object_ref (G_OBJECT (widget->window));
      
	button_widget->event_window = gdk_window_new (parent->window,
						      &attributes,
						      attributes_mask);
	gdk_window_set_user_data (button_widget->event_window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
button_widget_unrealize (GtkWidget *widget)
{
	ButtonWidget *button_widget;
  
	g_return_if_fail (widget != NULL);
	g_return_if_fail (BUTTON_IS_WIDGET (widget));

	button_widget = BUTTON_WIDGET (widget);
	
	gdk_window_set_user_data (button_widget->event_window, NULL);
	gdk_window_destroy (button_widget->event_window);
	button_widget->event_window = NULL;
	
	if (GTK_WIDGET_CLASS (button_widget_parent_class)->unrealize)
		(* GTK_WIDGET_CLASS (button_widget_parent_class)->unrealize) (widget);
}

static void
button_widget_map (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (BUTTON_IS_WIDGET (widget));

	if (GTK_WIDGET_REALIZED (widget) && !GTK_WIDGET_MAPPED (widget))
		gdk_window_show (BUTTON_WIDGET (widget)->event_window);

	GTK_WIDGET_CLASS (button_widget_parent_class)->map (widget);
}

static void
button_widget_unmap (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (BUTTON_IS_WIDGET (widget));

	if (GTK_WIDGET_MAPPED (widget))
		gdk_window_hide (BUTTON_WIDGET (widget)->event_window);

	GTK_WIDGET_CLASS (button_widget_parent_class)->unmap (widget);
}

static void
button_widget_destroy (GtkObject *obj)
{
	ButtonWidget *button = BUTTON_WIDGET(obj);

	if (button->pressed_timeout != 0)
		gtk_timeout_remove(button->pressed_timeout);
	button->pressed_timeout = 0;

	if (button->pixbuf)
		g_object_unref (G_OBJECT (button->pixbuf));
	button->pixbuf = NULL;
	
	if (button->scaled)
		g_object_unref (G_OBJECT (button->scaled));
	button->scaled = NULL;
	
	if (button->scaled_hc)
		g_object_unref (G_OBJECT (button->scaled_hc));
	button->scaled_hc = NULL;
	
	g_free (button->filename);
	button->filename = NULL;
	
	g_free (button->text);
	button->text = NULL;

	if (GTK_OBJECT_CLASS (button_widget_parent_class)->destroy)
		GTK_OBJECT_CLASS (button_widget_parent_class)->destroy (obj);
}

static GdkPixbuf *
loadup_file(const char *file)
{
	GdkPixbuf *pb = NULL;
	
	if (string_empty (file))
		return NULL;

	if (!g_path_is_absolute (file)) {
		char *f;

		f = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
					       file, TRUE, NULL);
		if (f) {
			pb = gdk_pixbuf_new_from_file (f, NULL);
			g_free (f);
		}
	} else {
		pb = gdk_pixbuf_new_from_file (file, NULL);
	}
	return pb;
}

#if 0
/* #define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11) */
#define INTENSITY(r, g, b) (((r)*77 + (g)*150 + (b)*28)>>8)

/* saturation is 0-255, darken is 0-255 */
static void
do_saturate_darken (GdkPixbuf *dest, GdkPixbuf *src,
		    int saturation, int darken)
{
	gint i, j;
	gint width, height, has_alpha, srcrowstride, destrowstride;
	guchar *target_pixels;
	guchar *original_pixels;
	guchar *pixsrc;
	guchar *pixdest;
	int intensity;
	int alpha;
	int negalpha;
	int val;
	guchar r,g,b;

	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	srcrowstride = gdk_pixbuf_get_rowstride (src);
	destrowstride = gdk_pixbuf_get_rowstride (dest);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i*destrowstride;
		pixsrc = original_pixels + i*srcrowstride;
		for (j = 0; j < width; j++) {
			r = *(pixsrc++);
			g = *(pixsrc++);
			b = *(pixsrc++);
			intensity = INTENSITY(r,g,b);
			negalpha = ((255 - saturation)*darken)>>8;
			alpha = (saturation*darken)>>8;
			val = (negalpha * intensity + alpha * r) >> 8;
			*(pixdest++) = MIN(val, 255);
			val = (negalpha * intensity + alpha * g) >> 8;
			*(pixdest++) = MIN(val, 255);
			val = (negalpha * intensity + alpha * b) >> 8;
			*(pixdest++) = MIN(val, 255);
			if (has_alpha)
				*(pixdest++) = *(pixsrc++);
		}
	}
}
#undef INTENSITY
#endif

/* colorshift a pixbuf */
static void
do_colorshift (GdkPixbuf *dest, GdkPixbuf *src, int shift)
{
	gint i, j;
	gint width, height, has_alpha, srcrowstride, destrowstride;
	guchar *target_pixels;
	guchar *original_pixels;
	guchar *pixsrc;
	guchar *pixdest;
	int val;
	guchar r,g,b;

	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	srcrowstride = gdk_pixbuf_get_rowstride (src);
	destrowstride = gdk_pixbuf_get_rowstride (dest);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i*destrowstride;
		pixsrc = original_pixels + i*srcrowstride;
		for (j = 0; j < width; j++) {
			r = *(pixsrc++);
			g = *(pixsrc++);
			b = *(pixsrc++);
			val = r + shift;
			*(pixdest++) = CLAMP(val, 0, 255);
			val = g + shift;
			*(pixdest++) = CLAMP(val, 0, 255);
			val = b + shift;
			*(pixdest++) = CLAMP(val, 0, 255);
			if (has_alpha)
				*(pixdest++) = *(pixsrc++);
		}
	}
}



#define SCALE(x) (((x)*size)/48.0)

static void
draw_arrow(GdkPoint *points, PanelOrient orient, int size)
{
	switch(orient) {
	case PANEL_ORIENT_UP:
		points[0].x = SCALE(48-12);
		points[0].y = SCALE(10);
		points[1].x = SCALE(48-4);
		points[1].y = SCALE(10);
		points[2].x = SCALE(48-8);
		points[2].y = SCALE(3);
		break;
	case PANEL_ORIENT_DOWN:
		points[0].x = SCALE(4);
		points[0].y = SCALE(48 - 10);
		points[1].x = SCALE(12);
		points[1].y = SCALE(48 - 10);
		points[2].x = SCALE(8);
		points[2].y = SCALE(48 - 3);
		break;
	case PANEL_ORIENT_LEFT:
		points[0].x = SCALE(10);
		points[0].y = SCALE(4);
		points[1].x = SCALE(10);
		points[1].y = SCALE(12);
		points[2].x = SCALE(3);
		points[2].y = SCALE(8);
		break;
	case PANEL_ORIENT_RIGHT:
		points[0].x = SCALE(48 - 10);
		points[0].y = SCALE(48 - 12);
		points[1].x = SCALE(48 - 10);
		points[1].y = SCALE(48 - 4);
		points[2].x = SCALE(48 - 3);
		points[2].y = SCALE(48 - 8);
		break;
	}
}

void
button_widget_set_dnd_highlight(ButtonWidget *button, gboolean highlight)
{
	g_return_if_fail (button != NULL);
	g_return_if_fail (BUTTON_IS_WIDGET (button));

	if(button->dnd_highlight != highlight) {
		button->dnd_highlight = highlight;
		gtk_widget_queue_draw (GTK_WIDGET (button));
	}
}

static gboolean
button_widget_expose (GtkWidget         *widget,
		      GdkEventExpose    *event)
{
	ButtonWidget *button;
	GdkRectangle area, image_bound;
	int off, size;
	int x, y, w, h;
	GdkPixbuf *pb = NULL;
  
	g_return_val_if_fail (BUTTON_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	button = BUTTON_WIDGET (widget);
	
	if (!GTK_WIDGET_VISIBLE (widget) || !GTK_WIDGET_MAPPED (widget)) {
		return FALSE;
	}

	size = widget->allocation.height;
	/* offset for pressed buttons */
	off = (button->in_button && button->pressed) ?
		SCALE(BUTTON_WIDGET_DISPLACEMENT) : 0;
	
	if (!global_config.highlight_when_over || !button->in_button) {
		pb = button->scaled;
	} else {
		pb = button->scaled_hc;
	}
	
	w = gdk_pixbuf_get_width (pb);
	h = gdk_pixbuf_get_height (pb);
	x = widget->allocation.x + off + (widget->allocation.width - w)/2;
	y = widget->allocation.y + off + (widget->allocation.height - h)/2;
	
	image_bound.x = x;
	image_bound.y = y;      
	image_bound.width = w;
	image_bound.height = h;
	
	area = event->area;
	
	if (gdk_rectangle_intersect (&area, &widget->allocation, &area) &&
	    gdk_rectangle_intersect (&image_bound, &area, &image_bound))  {
		gdk_pixbuf_render_to_drawable_alpha (pb,
						     widget->window,
						     image_bound.x - x, image_bound.y - y,
						     image_bound.x, image_bound.y,
						     image_bound.width, image_bound.height,
						     GDK_PIXBUF_ALPHA_FULL,
						     128,
						     GDK_RGB_DITHER_NORMAL,
						     0, 0);
	}
	
	if(button->arrow) {
		int i;
		GdkPoint points[3];
		draw_arrow (points, button->orient, widget->allocation.height);
		for (i = 0; i < 3; i++) {
			points[i].x += off + widget->allocation.x;
			points[i].y += off + widget->allocation.y;
		}
		gdk_draw_polygon (widget->window, widget->style->white_gc, TRUE, points, 3);
		gdk_draw_polygon (widget->window, widget->style->black_gc, FALSE, points, 3);
	}

	if (button->dnd_highlight) {
		gdk_draw_rectangle(widget->window, widget->style->black_gc, FALSE,
				   widget->allocation.x, widget->allocation.x,
				   widget->allocation.width - 1,
				   widget->allocation.height - 1);
	}
	
	return FALSE;
}

static GdkPixbuf *
make_hc_pixbuf(GdkPixbuf *pb)
{
	GdkPixbuf *new;
	
	if(!pb)
		return NULL;
#ifdef BUTTON_WIDGET_DEBUG
	printf ("Creating highlight pixbuf\n");
#endif
	new = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pb),
			      gdk_pixbuf_get_has_alpha (pb),
			      gdk_pixbuf_get_bits_per_sample (pb),
			      gdk_pixbuf_get_width (pb),
			      gdk_pixbuf_get_height (pb));
	do_colorshift (new, pb, 30);
	/*do_saturate_darken (new, pb, (int)(1.00*255), (int)(1.15*255));*/

	return new;
}

static void
button_widget_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	PanelWidget *panel = PANEL_WIDGET(widget->parent);
	requisition->width = requisition->height = panel->sz;
}

static void
button_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	ButtonWidget *button_widget;
	int w, h;
	double scale, scale_x, scale_y;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (BUTTON_IS_WIDGET (widget));

	button_widget = BUTTON_WIDGET (widget);

	widget->allocation = *allocation;

	w = gdk_pixbuf_get_width (button_widget->pixbuf);
	h = gdk_pixbuf_get_height (button_widget->pixbuf);

	scale_x = (double) allocation->width / w;
	scale_y = (double) allocation->height / h;

	scale = MIN (scale_x, scale_y);

	w *= scale;
	h *= scale;

	if (button_widget->scaled) {
		g_object_unref (button_widget->scaled);
	}

	/* TODO: Don't scale if already the same size */
	button_widget->scaled = gdk_pixbuf_scale_simple (button_widget->pixbuf,
							 w, h, GDK_INTERP_BILINEAR);
	if (button_widget->scaled_hc) {
		g_object_unref (button_widget->scaled_hc);
	}
	
	button_widget->scaled_hc = make_hc_pixbuf (button_widget->scaled);
	
	if (GTK_WIDGET_REALIZED (widget)) {
		PanelWidget *panel;
		int x,y,w,h;

		panel = PANEL_WIDGET(widget->parent);

		calculate_overlay_geometry (panel, panel->panel_parent,
					    widget, &x, &y, &w, &h);
		gdk_window_move_resize (button_widget->event_window,
					x, y, w, h);
	}
}

static void
button_widget_instance_init (ButtonWidget *button)
{
	GTK_WIDGET_SET_FLAGS (button, GTK_NO_WINDOW);

	button->pixbuf = NULL;
	
	button->pobject = 0;
	button->arrow = 0;
	button->orient = PANEL_ORIENT_UP;
	
	button->pressed = FALSE;
	button->in_button = FALSE;
	button->ignore_leave = FALSE;
	button->dnd_highlight = FALSE;
	
	button->pressed_timeout = 0;
}

static gboolean
pressed_timeout_func(gpointer data)
{
	ButtonWidget *button;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (BUTTON_IS_WIDGET (data), FALSE);

	button = BUTTON_WIDGET (data);
	
	button->pressed_timeout = 0;
	
	return FALSE;
}

static gboolean
button_widget_button_press (GtkWidget *widget, GdkEventButton *event)
{
	ButtonWidget *button;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (BUTTON_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	button = BUTTON_WIDGET (widget);

	if (button->pressed_timeout)
		return TRUE;

	if (event->button == 1) {
		gtk_grab_add(widget);
		button_widget_down (button);
		button->pressed_timeout =
			gtk_timeout_add(400, pressed_timeout_func, button);
	}
	return TRUE;
}

static gboolean
button_widget_button_release (GtkWidget *widget, GdkEventButton *event)
{
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (BUTTON_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (event->button == 1) {
		ButtonWidget *button = BUTTON_WIDGET (widget);
		gtk_grab_remove (widget);
		button_widget_up (button);
	}

	return TRUE;
}

static gboolean
button_widget_enter_notify (GtkWidget *widget, GdkEventCrossing *event)
{
	GtkWidget *event_widget;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (BUTTON_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	event_widget = gtk_get_event_widget ((GdkEvent*) event);

	if ((event_widget == widget) &&
	    (event->detail != GDK_NOTIFY_INFERIOR)) {
		ButtonWidget *button = BUTTON_WIDGET (widget);
#ifdef BUTTON_WIDGET_DEBUG
	printf ("We should be getting events here!!\n");
#endif
		button->in_button = TRUE;
		gtk_widget_queue_draw (widget);
	}

	return FALSE;
}

static gboolean
button_widget_leave_notify (GtkWidget *widget, GdkEventCrossing *event)
{
	GtkWidget *event_widget;
	ButtonWidget *button;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (BUTTON_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	event_widget = gtk_get_event_widget ((GdkEvent*) event);

	button = BUTTON_WIDGET (widget);

	if ((event_widget == widget) &&
	    (event->detail != GDK_NOTIFY_INFERIOR) &&
	    (!button->ignore_leave)) {
		button->in_button = FALSE;
		if (global_config.highlight_when_over) {
			gtk_widget_queue_draw (widget);
		}
	}

	return FALSE;
}

void
button_widget_clicked(ButtonWidget *button)
{
	g_signal_emit (G_OBJECT(button),
		       button_widget_signals[CLICKED_SIGNAL], 0);
}

static void
button_widget_pressed(ButtonWidget *button)
{
	g_return_if_fail(button != NULL);
	g_return_if_fail(BUTTON_IS_WIDGET(button));

	button->pressed = TRUE;
	gtk_widget_queue_draw (GTK_WIDGET (button));
}

static void
button_widget_unpressed(ButtonWidget *button)
{
	g_return_if_fail(button != NULL);
	g_return_if_fail(BUTTON_IS_WIDGET(button));

	button->pressed = FALSE;
	gtk_widget_queue_draw (GTK_WIDGET (button));
	
	if(button->in_button)
		button_widget_clicked(button);
}

void
button_widget_down(ButtonWidget *button)
{
	g_return_if_fail(button != NULL);
	g_return_if_fail(BUTTON_IS_WIDGET(button));

	if(!button->pressed)
		g_signal_emit(G_OBJECT(button),
			      button_widget_signals[PRESSED_SIGNAL], 0);
}
void
button_widget_up(ButtonWidget *button)
{
	g_return_if_fail(button != NULL);
	g_return_if_fail(BUTTON_IS_WIDGET(button));

	if(button->pressed)
		g_signal_emit(G_OBJECT(button),
			      button_widget_signals[UNPRESSED_SIGNAL], 0);
}

GtkWidget*
button_widget_new(const char *filename,
		  int size,
		  int pobject,
		  gboolean arrow,
		  PanelOrient orient,
		  const char *text)
{
	ButtonWidget *button;

	g_return_val_if_fail(pobject >= 0, NULL);
	g_return_val_if_fail(pobject < LAST_POBJECT, NULL);

	button = BUTTON_WIDGET (g_object_new (button_widget_get_type (), NULL));
	
	button->pixbuf = loadup_file (filename);
	button->scaled = NULL;
	button->scaled_hc = NULL;
	button->filename = g_strdup (filename);
	button->size = size;
	button->pobject = pobject;
	button->arrow = arrow ? 1 : 0;
	button->orient = orient;
	button->text = text ? g_strdup (text) : NULL;

	return GTK_WIDGET(button);
}

gboolean
button_widget_set_pixmap(ButtonWidget *button, const char *pixmap, int size)
{
	g_return_val_if_fail(button != NULL, FALSE);
	g_return_val_if_fail(BUTTON_IS_WIDGET(button), FALSE);

	if (size < 0)
		size = PANEL_WIDGET(GTK_WIDGET(button)->parent)->sz;
	
	if (button->pixbuf != NULL)
		g_object_unref (G_OBJECT (button->pixbuf));

	button->pixbuf = loadup_file(pixmap);

	
	g_free(button->filename);
	button->filename = g_strdup(pixmap);
	button->size = size;

	/* TODO: Is this guaranteed to trigger the size_allocate call to
	   update scale + scale_hc???? */
	gtk_widget_queue_resize (GTK_WIDGET (button));
	gtk_widget_queue_draw (GTK_WIDGET (button));

	if (button->pixbuf == NULL)
		return FALSE;
	
	return TRUE;
}

void
button_widget_set_text(ButtonWidget *button, const char *text)
{
	g_return_if_fail(button != NULL);
	g_return_if_fail(BUTTON_IS_WIDGET(button));

	g_free(button->text);
	button->text = text?g_strdup(text):NULL;

	gtk_widget_queue_draw (GTK_WIDGET (button));
}

void
button_widget_set_params(ButtonWidget *button,
			 int pobject,
			 gboolean arrow,
			 PanelOrient orient)
{
	g_return_if_fail(button != NULL);
	g_return_if_fail(BUTTON_IS_WIDGET(button));
	g_return_if_fail(pobject >= 0);
	g_return_if_fail(pobject < LAST_POBJECT);

	button->pobject = pobject;
	button->arrow = arrow?1:0;
	button->orient = orient;

	gtk_widget_queue_draw (GTK_WIDGET (button));
}

