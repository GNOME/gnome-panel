#include <config.h>
#include <math.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-desktop-item.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "button-widget.h"
#include "panel-widget.h"
#include "panel-types.h"
#include "panel-util.h"
#include "panel-config-global.h"
#include "panel-marshal.h"
#include "panel-typebuiltins.h"
#include "panel-globals.h"



static GdkPixbuf *button_load_pixbuf (const char  *file,
				      char       **error);

enum {
	PROP_0,
	PROP_HAS_ARROW,
	PROP_ORIENT,
	PROP_ICON_NAME,
	PROP_STOCK_ID,
};

#define BUTTON_WIDGET_DISPLACEMENT 2

static GObjectClass *parent_class;

static void
translate_to(GtkWidget *from, GtkWidget *to, int *x, int *y)
{
	while (from != to) {
		if (!GTK_WIDGET_NO_WINDOW (from)) {
			*x += MAX (from->allocation.x, 0);
			*y += MAX (from->allocation.y, 0);
		}
		from = from->parent;
	}
}

static void
calculate_overlay_geometry (PanelWidget *panel,
			    GtkWidget   *parent,
			    GtkWidget   *applet,
			    int         *x,
			    int         *y,
			    int         *w,
			    int         *h)
{
	*x = applet->allocation.x;
	*y = applet->allocation.y;
	*w = applet->allocation.width;
	*h = applet->allocation.height;

	translate_to (GTK_WIDGET(panel), parent, x, y);

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
		if(*h < parent->requisition.height)
			*h = parent->requisition.height;

		if ((*w + applet->allocation.x) > panel->size) {
			*w = panel->size - applet->allocation.x;
		}
	} else {
		if (applet->allocation.y > panel->size) {
			*x = parent->requisition.width + 1;
			*y = parent->requisition.height + 1;
			return;
		}

		*x = 0;
		if(*w < parent->requisition.width)
			*w = parent->requisition.width;

		if ((*h + applet->allocation.y) > panel->size) {
			*h = panel->size - applet->allocation.y;
		}
	}
}

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

static GdkPixbuf *
make_hc_pixbuf (GdkPixbuf *pb)
{
	GdkPixbuf *new;
	
	if (!pb)
		return NULL;

	new = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pb),
			      gdk_pixbuf_get_has_alpha (pb),
			      gdk_pixbuf_get_bits_per_sample (pb),
			      gdk_pixbuf_get_width (pb),
			      gdk_pixbuf_get_height (pb));
	do_colorshift (new, pb, 30);

	return new;
}

static void
button_widget_realize(GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	GtkButton *button;
	PanelWidget *panel;
	GtkWidget *parent;
	int x,y,w,h;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (BUTTON_IS_WIDGET (widget));

	panel  = PANEL_WIDGET(widget->parent);
	parent = GTK_WIDGET (panel->toplevel);

	calculate_overlay_geometry(panel, parent, widget, &x, &y, &w, &h);

	button = GTK_BUTTON (widget);

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
      
	button->event_window = gdk_window_new (parent->window,
					       &attributes,
					       attributes_mask);
	gdk_window_set_user_data (button->event_window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
button_widget_parent_set (GtkWidget *widget,
			  GtkWidget *previous_parent)
{
	GtkWidget *parent;
	int        x, y, w, h;

	g_return_if_fail (BUTTON_IS_WIDGET (widget));

	if (!GTK_WIDGET_REALIZED (widget)|| !widget->parent)
		return;

	parent = GTK_WIDGET (PANEL_WIDGET (widget->parent)->toplevel);

	calculate_overlay_geometry (
		PANEL_WIDGET (widget->parent), parent, widget, &x, &y, &w, &h);

	gdk_window_reparent (
		GTK_BUTTON (widget)->event_window, parent->window, x, y);
}

static void
button_widget_unset_pixbufs (ButtonWidget *button)
{
	if (button->pixbuf)
		g_object_unref (button->pixbuf);
	button->pixbuf = NULL;

	if (button->scaled)
		g_object_unref (button->scaled);
	button->scaled = NULL;

	if (button->scaled_hc)
		g_object_unref (button->scaled_hc);
	button->scaled_hc = NULL;
}

static void
button_widget_load_pixbuf_and_scale (ButtonWidget *button,
				     int           dest_width,
				     int           dest_height)
{
	double scale;
	double scale_x;
	double scale_y;
	int    width;
	int    height;

	if (!button->pixbuf) {
		g_assert (!button->filename || !button->stock_id);

		if (!button->filename && !button->stock_id)
			return;

		if (button->stock_id)
			button->pixbuf = gtk_widget_render_icon (
						GTK_WIDGET (button),
						button->stock_id,
						(GtkIconSize) -1,
						NULL);
		else {
			char *error = NULL;

			button->pixbuf = button_load_pixbuf (button->filename, &error);
			if (error) {
				panel_error_dialog (gdk_screen_get_default (),
						    "cannot_load_pixbuf",
						    _("Failed to load image %s\n\n"
						    "Details: %s"),
						    button->filename,
						    error);
				g_free (error);
			}
		}

		if (!button->pixbuf)
			return;
	}

	width  = gdk_pixbuf_get_width  (button->pixbuf);
	height = gdk_pixbuf_get_height (button->pixbuf);

	scale_x = (double) dest_width  / width;
	scale_y = (double) dest_height / height;

	scale = MIN (scale_x, scale_y);

	width  *= scale;
	height *= scale;

	if (button->scaled) {
		if (gdk_pixbuf_get_width  (button->scaled) == width &&
		    gdk_pixbuf_get_height (button->scaled) == height)
			return; /* no need to re-scale */

		g_object_unref (button->scaled);
	}

	button->scaled = gdk_pixbuf_scale_simple (
				button->pixbuf, width, height, GDK_INTERP_BILINEAR);

	if (button->scaled_hc)
		g_object_unref (button->scaled_hc);
	
	button->scaled_hc = make_hc_pixbuf (button->scaled);
}

static void
button_widget_reload_pixbuf (ButtonWidget *button)
{
	button_widget_unset_pixbufs (button);
	if (GTK_WIDGET (button)->allocation.width  <= 1 ||
	    GTK_WIDGET (button)->allocation.height <= 1)
		return;

	button_widget_load_pixbuf_and_scale (
		button,
		GTK_WIDGET (button)->allocation.width,
		GTK_WIDGET (button)->allocation.height);
	gtk_widget_queue_draw (GTK_WIDGET (button));
}

static void
button_widget_theme_changed (ButtonWidget *button)
{
	button_widget_reload_pixbuf (button);
}

static void
button_widget_finalize (GObject *object)
{
	ButtonWidget *button = (ButtonWidget *) object;

	if (button->pressed_timeout)
		g_source_remove (button->pressed_timeout);
	button->pressed_timeout = 0;

	button_widget_unset_pixbufs (button);

	g_free (button->filename);
	button->filename = NULL;

	g_free (button->stock_id);
	button->stock_id = NULL;
	
	parent_class->finalize (object);
}

static void
button_widget_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
	ButtonWidget *button;

	g_return_if_fail (BUTTON_IS_WIDGET (object));

	button = BUTTON_WIDGET (object);

	switch (prop_id) {
	case PROP_HAS_ARROW:
		g_value_set_boolean (value, button->arrow);
		break;
	case PROP_ORIENT:
		g_value_set_enum (value, button->orientation);
		break;
	case PROP_ICON_NAME:
		g_value_set_string (value, button->filename);
		break;
	case PROP_STOCK_ID:
		g_value_set_string (value, button->stock_id);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
button_widget_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
	ButtonWidget *button;

	g_return_if_fail (BUTTON_IS_WIDGET (object));

	button = BUTTON_WIDGET (object);

	switch (prop_id) {
		const char *icon_name;
		const char *stock_id;

	case PROP_HAS_ARROW:
		button->arrow = g_value_get_boolean (value) ? 1 : 0;
		gtk_widget_queue_draw (GTK_WIDGET (button));
		break;
	case PROP_ORIENT:
		button->orientation = g_value_get_enum (value);
		gtk_widget_queue_draw (GTK_WIDGET (button));
		break;
	case PROP_ICON_NAME:
		icon_name = g_value_get_string (value);

		g_assert (!button->filename || !button->stock_id);

		if (button->stock_id) {
			g_free (button->stock_id);
			button->stock_id = NULL;
		}

		if (button->filename)
			g_free (button->filename);
		button->filename = g_strdup (icon_name);

		button_widget_reload_pixbuf (button);
		break;
	case PROP_STOCK_ID:
		stock_id = g_value_get_string (value);

		g_assert (!button->filename || !button->stock_id);

		if (button->filename) {
			g_free (button->filename);
			button->filename = NULL;
		}

		if (button->stock_id)
			g_free (button->stock_id);
		button->stock_id = g_strdup (stock_id);

		button_widget_reload_pixbuf (button);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static char *default_pixmap = NULL;

static GdkPixbuf *
get_missing (int preffered_size)
{
	GdkPixbuf *retval = NULL;

	if (default_pixmap == NULL)
		default_pixmap = panel_pixmap_discovery ("gnome-unknown.png",
							 FALSE /* fallback */);
	if (default_pixmap != NULL)
		retval = gdk_pixbuf_new_from_file (default_pixmap,
						   NULL);
	if (retval == NULL)
		retval = missing_pixbuf (preffered_size);

	return retval;
}

static GdkPixbuf *
load_pixbuf (const char  *file,
	     int          preffered_size,
	     char       **str_error)
{
	GError    *error = NULL;
	GdkPixbuf *retval = NULL;
	char      *full;

	full = gnome_desktop_item_find_icon (panel_icon_theme, file, preffered_size, 0);

	if (full) {
		retval = gdk_pixbuf_new_from_file (full, &error);
		if (!retval) {
			if (str_error)
				*str_error = g_strdup (error ? error->message : _("none"));
			g_error_free (error);
		}
		g_free (full);
	} else if (str_error)
		*str_error = g_strdup (_("file not found"));

	return retval;


}

static GdkPixbuf *
button_load_pixbuf (const char  *file,
		    char       **error)
{
/* FIXME: this should be based on the panel size */
#define PREFERRED_SIZE 48

	GdkPixbuf *retval = NULL;
	char *tmp;

	if (string_empty (file))
		return get_missing (PREFERRED_SIZE);

	tmp = g_path_get_basename (file);
	retval = load_pixbuf (tmp, PREFERRED_SIZE, error);
	g_free (tmp);

	if (!retval && g_path_is_absolute (file)) {
		if (error && *error) {
			g_free (*error);
			*error = NULL;
		}

		retval = load_pixbuf (file, PREFERRED_SIZE, error);
	}

	if (!retval)
		retval = get_missing (PREFERRED_SIZE);

	return retval;

#undef PREFERRED_SIZE
}

#define SCALE(x) (((x)*size)/48.0)

static void
draw_arrow (GdkPoint         *points,
	    PanelOrientation  orientation,
	    int               size)
{
	switch (orientation) {
	case PANEL_ORIENTATION_TOP:
		points[0].x = SCALE(4);
		points[0].y = SCALE(48 - 10);
		points[1].x = SCALE(12);
		points[1].y = SCALE(48 - 10);
		points[2].x = SCALE(8);
		points[2].y = SCALE(48 - 3);
		break;
	case PANEL_ORIENTATION_BOTTOM:
		points[0].x = SCALE(48-12);
		points[0].y = SCALE(10);
		points[1].x = SCALE(48-4);
		points[1].y = SCALE(10);
		points[2].x = SCALE(48-8);
		points[2].y = SCALE(3);
		break;
	case PANEL_ORIENTATION_LEFT:
		points[0].x = SCALE(48 - 10);
		points[0].y = SCALE(48 - 12);
		points[1].x = SCALE(48 - 10);
		points[1].y = SCALE(48 - 4);
		points[2].x = SCALE(48 - 3);
		points[2].y = SCALE(48 - 8);
		break;
	case PANEL_ORIENTATION_RIGHT:
		points[0].x = SCALE(10);
		points[0].y = SCALE(4);
		points[1].x = SCALE(10);
		points[1].y = SCALE(12);
		points[2].x = SCALE(3);
		points[2].y = SCALE(8);
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
	ButtonWidget *button_widget;
	GtkButton *button;
	GdkRectangle area, image_bound;
	int off, size;
	int x, y, w, h;
	GdkPixbuf *pb = NULL;
  
	g_return_val_if_fail (BUTTON_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	button_widget = BUTTON_WIDGET (widget);
	button = GTK_BUTTON (widget);
	
	if (!GTK_WIDGET_VISIBLE (widget) || !GTK_WIDGET_MAPPED (widget)) {
		return FALSE;
	}

	size = widget->allocation.height;
	/* offset for pressed buttons */
	off = (button->in_button && button->button_down) ?
		SCALE(BUTTON_WIDGET_DISPLACEMENT) : 0;
	
	if (panel_global_config_get_highlight_when_over () && 
	    (button->in_button || GTK_WIDGET_HAS_FOCUS (widget)))
		pb = button_widget->scaled_hc;
	else
		pb = button_widget->scaled;
	
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
	    gdk_rectangle_intersect (&image_bound, &area, &image_bound))
		gdk_draw_pixbuf (widget->window, NULL, pb,
				 image_bound.x - x, image_bound.y - y,
				 image_bound.x, image_bound.y,
				 image_bound.width, image_bound.height,
				 GDK_RGB_DITHER_NORMAL,
				 0, 0);
	
	if (button_widget->arrow) {
		int i;
		GdkPoint points[3];
		draw_arrow (points, button_widget->orientation, widget->allocation.height);
		for (i = 0; i < 3; i++) {
			points[i].x += off + widget->allocation.x;
			points[i].y += off + widget->allocation.y;
		}
		gdk_draw_polygon (widget->window, widget->style->white_gc, TRUE, points, 3);
		gdk_draw_polygon (widget->window, widget->style->black_gc, FALSE, points, 3);
	}

	if (button_widget->dnd_highlight) {
		gdk_draw_rectangle(widget->window, widget->style->black_gc, FALSE,
				   widget->allocation.x, widget->allocation.y,
				   widget->allocation.width - 1,
				   widget->allocation.height - 1);
	}

	if (GTK_WIDGET_HAS_FOCUS (widget)) {
		gint focus_width, focus_pad;
		gint x, y, width, height;

		gtk_widget_style_get (widget,
				      "focus-line-width", &focus_width,
				      "focus-padding", &focus_pad,
				      NULL);
		x = widget->allocation.x + focus_pad;
		y = widget->allocation.y + focus_pad;
		width = widget->allocation.width -  2 * focus_pad;
		height = widget->allocation.height - 2 * focus_pad;
		gtk_paint_focus (widget->style, widget->window,
				 GTK_STATE_NORMAL,
				 &event->area, widget, "button",
				 x, y, width, height);
	}
	
	return FALSE;
}

static void
button_widget_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
	PanelWidget *panel = PANEL_WIDGET (widget->parent);

	requisition->width = requisition->height = panel->sz;
}

static void
button_widget_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
	ButtonWidget *button_widget;
	GtkButton    *button;

	g_return_if_fail (BUTTON_IS_WIDGET (widget));

	button_widget = BUTTON_WIDGET (widget);
	button        = GTK_BUTTON (widget);

	widget->allocation = *allocation;

	button_widget_load_pixbuf_and_scale (
		button_widget, allocation->width, allocation->height);

	if (GTK_WIDGET_REALIZED (widget)) {
		PanelWidget *panel;
		int          x, y, w, h;

		panel = PANEL_WIDGET (widget->parent);

		calculate_overlay_geometry (
			panel, GTK_WIDGET (panel->toplevel),
			widget, &x, &y, &w, &h);

		gdk_window_move_resize (button->event_window, x, y, w, h);
	}
}

static gboolean
pressed_timeout_func(gpointer data)
{
	ButtonWidget *button;

	g_return_val_if_fail (BUTTON_IS_WIDGET (data), FALSE);

	button = BUTTON_WIDGET (data);

	button->pressed_timeout = 0;

	return FALSE;
}

static gboolean
button_widget_button_press (GtkWidget *widget, GdkEventButton *event)
{
 	ButtonWidget *button;

	g_return_val_if_fail (BUTTON_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	button = BUTTON_WIDGET (widget);

	if (button->pressed_timeout)
		return TRUE;

	return GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);
}

static gboolean
button_widget_enter_notify (GtkWidget *widget, GdkEventCrossing *event)
{
	g_return_val_if_fail (BUTTON_IS_WIDGET (widget), FALSE);

	GTK_WIDGET_CLASS (parent_class)->enter_notify_event (widget, event);
	if (GTK_BUTTON (widget)->in_button)
		gtk_widget_queue_draw (widget);

	return FALSE;
}

static gboolean
button_widget_leave_notify (GtkWidget *widget, GdkEventCrossing *event)
{
	gboolean in_button;

	g_return_val_if_fail (BUTTON_IS_WIDGET (widget), FALSE);

	in_button = GTK_BUTTON (widget)->in_button;
	GTK_WIDGET_CLASS (parent_class)->leave_notify_event (widget, event);
	if (in_button != GTK_BUTTON (widget)->in_button &&
	    panel_global_config_get_highlight_when_over ())
		gtk_widget_queue_draw (widget);

	return FALSE;
}

static void
button_widget_button_pressed (GtkButton *button)
{
	ButtonWidget *button_widget;

	g_return_if_fail (BUTTON_IS_WIDGET (button));

	GTK_BUTTON_CLASS (parent_class)->pressed (button);

	button_widget = BUTTON_WIDGET (button);
	button_widget->pressed_timeout =
		g_timeout_add (400, pressed_timeout_func, button_widget);
        gtk_widget_queue_draw (GTK_WIDGET (button));
}

static void
button_widget_button_released (GtkButton *button)
{
	g_return_if_fail (BUTTON_IS_WIDGET (button));

	GTK_BUTTON_CLASS (parent_class)->released (button);
        gtk_widget_queue_draw (GTK_WIDGET (button));
}

static void
button_widget_instance_init (ButtonWidget *button)
{
	button->pixbuf    = NULL;
	button->scaled    = NULL;
	button->scaled_hc = NULL;
	
	button->arrow       = 0;
	button->orientation = PANEL_ORIENTATION_TOP;
	
	button->ignore_leave  = FALSE;
	button->dnd_highlight = FALSE;

	button->pressed_timeout = 0;

	panel_signal_connect_object_while_alive (
			panel_icon_theme, "changed",
			G_CALLBACK (button_widget_theme_changed), button);
}

static void
button_widget_class_init (ButtonWidgetClass *klass)
{
	GObjectClass *gobject_class   = (GObjectClass   *) klass;
	GtkWidgetClass *widget_class  = (GtkWidgetClass *) klass;
	GtkButtonClass *button_class  = (GtkButtonClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize     = button_widget_finalize;
	gobject_class->get_property = button_widget_get_property;
	gobject_class->set_property = button_widget_set_property;
	  
	widget_class->realize            = button_widget_realize;
	widget_class->parent_set         = button_widget_parent_set;
	widget_class->size_allocate      = button_widget_size_allocate;
	widget_class->size_request       = button_widget_size_request;
	widget_class->button_press_event = button_widget_button_press;
	widget_class->enter_notify_event = button_widget_enter_notify;
	widget_class->leave_notify_event = button_widget_leave_notify;
	widget_class->expose_event       = button_widget_expose;

	button_class->pressed  = button_widget_button_pressed;
	button_class->released = button_widget_button_released;

	g_object_class_install_property (
			gobject_class,
			PROP_HAS_ARROW,
			g_param_spec_boolean ("has-arrow",
					      _("Has Arrow"),
					      _("Whether or not to draw an arrow indicator"),
					      FALSE,
					      G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_ORIENT,
			g_param_spec_enum ("orient",
					   _("Orientation"),
					   _("The ButtonWidget orientation"),
					   PANEL_TYPE_ORIENTATION,
					   PANEL_ORIENTATION_TOP,
					   G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_ICON_NAME,
			g_param_spec_string ("icon-name",
					     _("Icon Name"),
					     _("The desired icon for the ButtonWidget"),
					     NULL,
					     G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_STOCK_ID,
			g_param_spec_string ("stock-id",
					     _("Stock Icon ID"),
					     _("The desired stock icon for the ButtonWidget"),
					     NULL,
					     G_PARAM_READWRITE));
}

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

		object_type = g_type_register_static (GTK_TYPE_BUTTON, "ButtonWidget", &object_info, 0);
	}

	return object_type;
}

GtkWidget *
button_widget_new (const char       *filename,
		   gboolean          arrow,
		   PanelOrientation  orientation)
{
	GtkWidget *retval;

	retval = g_object_new (
			BUTTON_TYPE_WIDGET,
			"has-arrow", arrow,
			"orient", orientation,
			"icon-name", filename,
			NULL);
	
	return retval;
}

GtkWidget *
button_widget_new_from_stock (const char       *stock_id,
			      gboolean          arrow,
			      PanelOrientation  orientation)
{
	GtkWidget *retval;

	retval = g_object_new (
			BUTTON_TYPE_WIDGET,
			"has-arrow", arrow,
			"orient", orientation,
			"stock-id", stock_id,
			NULL);
	
	return retval;
}

void
button_widget_set_pixmap (ButtonWidget *button,
			  const char   *pixmap)
{
	g_return_if_fail (BUTTON_IS_WIDGET (button));

	g_object_set (G_OBJECT (button), "icon-name", pixmap, NULL);
}

void
button_widget_set_stock_id (ButtonWidget *button,
			    const char   *stock_id)
{
	g_return_if_fail (BUTTON_IS_WIDGET (button));

	g_object_set (G_OBJECT (button), "stock-id", stock_id, NULL);
}

void
button_widget_set_params (ButtonWidget     *button,
			  gboolean          arrow,
			  PanelOrientation  orientation)
{
	g_return_if_fail (BUTTON_IS_WIDGET (button));

	g_object_set (G_OBJECT (button), "has-arrow", arrow, NULL);
	g_object_set (G_OBJECT (button), "orient", orientation, NULL);
}
