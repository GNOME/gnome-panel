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
static GdkPixbuf * get_missing (int preffered_size);

enum {
	PROP_0,
	PROP_ACTIVATABLE,
	PROP_HAS_ARROW,
	PROP_DND_HIGHLIGHT,
	PROP_ORIENTATION,
	PROP_ICON_NAME,
	PROP_STOCK_ID
};

#define BUTTON_WIDGET_DISPLACEMENT 2

static GObjectClass *parent_class;

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

	g_return_if_fail (widget != NULL);
	g_return_if_fail (BUTTON_IS_WIDGET (widget));

	button = GTK_BUTTON (widget);

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_ONLY;
	attributes.event_mask = (GDK_BUTTON_PRESS_MASK |
				 GDK_BUTTON_RELEASE_MASK |
				 GDK_POINTER_MOTION_MASK |
				 GDK_POINTER_MOTION_HINT_MASK |
				 GDK_KEY_PRESS_MASK |
				 GDK_ENTER_NOTIFY_MASK |
				 GDK_LEAVE_NOTIFY_MASK);
	attributes_mask = GDK_WA_X | GDK_WA_Y;

	widget->window = gtk_widget_get_parent_window (widget);
	g_object_ref (G_OBJECT (widget->window));
      
	button->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
					       &attributes,
					       attributes_mask);
	gdk_window_set_user_data (button->event_window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
button_widget_unrealize (GtkWidget *widget)
{
	GtkButton *button;
	PanelWidget *panel;
	GtkWidget *parent;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (BUTTON_IS_WIDGET (widget));

	panel  = PANEL_WIDGET (widget->parent);
	parent = GTK_WIDGET (panel->toplevel);
	button = GTK_BUTTON (widget);

	if (button->event_window != NULL) {
		gdk_window_set_user_data (button->event_window, NULL);
		gdk_window_destroy (button->event_window);
		button->event_window = NULL;
	}

	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
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
button_widget_load_pixbuf_and_scale (ButtonWidget *button)
{
	double scale;
	int    width;
	int    height;

	if (button->size <= 1)
		return;

	if (button->pixbuf == NULL) {
		if (!button->filename && !button->stock_id)
			return;

		if (button->filename != NULL) {
			char *error = NULL;

			button->pixbuf = button_load_pixbuf (button->filename, &error);
			if (error) {
				panel_error_dialog (gdk_screen_get_default (),
						    "cannot_load_pixbuf",
						    _("Failed to load image %s"),
						    "%s",
						    button->filename,
						    error);
				g_free (error);
			}
		}

		if (button->pixbuf == NULL &&
		    button->stock_id != NULL)
			button->pixbuf = gtk_widget_render_icon (
						GTK_WIDGET (button),
						button->stock_id,
						(GtkIconSize) -1,
						NULL);

/* FIXME: this should be based on the panel size */
#define PREFERRED_SIZE 48
		if (button->pixbuf == NULL)
			button->pixbuf = get_missing (PREFERRED_SIZE);
#undef PREFERRED_SIZE

		if (!button->pixbuf)
			return;
	}

	width  = gdk_pixbuf_get_width  (button->pixbuf);
	height = gdk_pixbuf_get_height (button->pixbuf);

	if (button->orientation & PANEL_HORIZONTAL_MASK)
		scale = (double) button->size / height;
	else
		scale = (double) button->size / width;

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

	gtk_widget_queue_resize (GTK_WIDGET (button));
}

static void
button_widget_reload_pixbuf (ButtonWidget *button)
{
	button_widget_unset_pixbufs (button);
	button_widget_load_pixbuf_and_scale (button);
}

static void
button_widget_icon_theme_changed (ButtonWidget *button)
{
	if (button->filename != NULL)
		button_widget_reload_pixbuf (button);
}

static void
button_widget_gtk_theme_changed (ButtonWidget *button)
{
	if (button->stock_id != NULL)
		button_widget_reload_pixbuf (button);
}

static void
button_widget_finalize (GObject *object)
{
	ButtonWidget *button = (ButtonWidget *) object;

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
	case PROP_ACTIVATABLE:
		g_value_set_boolean (value, button->activatable);
		break;
	case PROP_HAS_ARROW:
		g_value_set_boolean (value, button->arrow);
		break;
	case PROP_DND_HIGHLIGHT:
		g_value_set_boolean (value, button->dnd_highlight);
		break;
	case PROP_ORIENTATION:
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
	case PROP_ACTIVATABLE:
		button_widget_set_activatable (button, g_value_get_boolean (value));
		break;
	case PROP_HAS_ARROW:
		button_widget_set_has_arrow (button, g_value_get_boolean (value));
		break;
	case PROP_DND_HIGHLIGHT:
		button_widget_set_dnd_highlight (button, g_value_get_boolean (value));
		break;
	case PROP_ORIENTATION:
		button_widget_set_orientation (button, g_value_get_enum (value));
		break;
	case PROP_ICON_NAME:
		button_widget_set_icon_name (button, g_value_get_string (value));
		break;
	case PROP_STOCK_ID:
		button_widget_set_stock_id (button, g_value_get_string (value));
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
		default_pixmap = gnome_desktop_item_find_icon (panel_icon_theme, "gnome-unknown.png",
							       preffered_size, 0);
	if (default_pixmap != NULL)
		retval = gdk_pixbuf_new_from_file_at_size (default_pixmap,
							   preffered_size,
							   preffered_size,
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

	if (g_path_is_absolute (file))
		full = g_strdup (file);
	else
		full = gnome_desktop_item_find_icon (panel_icon_theme, file, preffered_size, 0);

	if (full) {
		retval = gdk_pixbuf_new_from_file_at_size (full,
							   preffered_size,
							   preffered_size,
							   &error);
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

	if (string_empty (file))
		return NULL;

	retval = load_pixbuf (file, PREFERRED_SIZE, NULL);
	if (!retval) {
		char *tmp;

		if (error && *error) {
			g_free (*error);
			*error = NULL;
		}

		tmp = g_path_get_basename (file);
		retval = load_pixbuf (tmp, PREFERRED_SIZE, error);
		g_free (tmp);
	}

	return retval;

#undef PREFERRED_SIZE
}

static GtkArrowType
calc_arrow (PanelOrientation  orientation,
	    int               button_width,
	    int               button_height,
	    int              *x,
	    int              *y,
	    int              *width,
	    int              *height)
{
	GtkArrowType retval = GTK_ARROW_UP;
	double scale;

	scale = (orientation & PANEL_HORIZONTAL_MASK ? button_height : button_width) / 48.0;

	*width  = 12 * scale;
	*height = 12 * scale;

	switch (orientation) {
	case PANEL_ORIENTATION_TOP:
		*x     = scale * 3;
		*y     = scale * (48 - 13);
		retval = GTK_ARROW_DOWN;
		break;
	case PANEL_ORIENTATION_BOTTOM:
		*x     = scale * (48 - 13);
		*y     = scale * 1;
		retval = GTK_ARROW_UP;
		break;
	case PANEL_ORIENTATION_LEFT:
		*x     = scale * (48 - 13);
		*y     = scale * 3;
		retval = GTK_ARROW_RIGHT;
		break;
	case PANEL_ORIENTATION_RIGHT:
		*x     = scale * 1;
		*y     = scale * 3;
		retval = GTK_ARROW_LEFT;
		break;
	}

	return retval;
}

static gboolean
button_widget_expose (GtkWidget         *widget,
		      GdkEventExpose    *event)
{
	ButtonWidget *button_widget;
	GtkButton *button;
	GdkRectangle area, image_bound;
	int off;
	int x, y, w, h;
	GdkPixbuf *pb = NULL;
  
	g_return_val_if_fail (BUTTON_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	button_widget = BUTTON_WIDGET (widget);
	button = GTK_BUTTON (widget);
	
	if (!GTK_WIDGET_VISIBLE (widget) || !GTK_WIDGET_MAPPED (widget))
		return FALSE;

	if (!button_widget->scaled_hc && !button_widget->scaled)
		return FALSE;

	/* offset for pressed buttons */
	off = (button_widget->activatable && button->in_button && button->button_down) ?
		BUTTON_WIDGET_DISPLACEMENT * widget->allocation.height / 48.0 : 0;

	if (!button_widget->activatable) {
		pb = gdk_pixbuf_copy (button_widget->scaled);
		gdk_pixbuf_saturate_and_pixelate (button_widget->scaled,
						  pb,
						  0.8,
						  TRUE);
	} else if (panel_global_config_get_highlight_when_over () && 
	    (button->in_button || GTK_WIDGET_HAS_FOCUS (widget)))
		pb = g_object_ref (button_widget->scaled_hc);
	else
		pb = g_object_ref (button_widget->scaled);

	g_assert (pb != NULL);

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

	g_object_unref (pb);
	
	if (button_widget->arrow) {
		GtkArrowType arrow_type;
		int          x, y, width, height;

		x = y = width = height = -1;

		arrow_type = calc_arrow (button_widget->orientation,
					 widget->allocation.width,
					 widget->allocation.height,
					 &x,
					 &y,
					 &width,
					 &height);

		gtk_paint_arrow (widget->style,
				 widget->window,
				 GTK_STATE_NORMAL,
				 GTK_SHADOW_NONE,
				 NULL,
				 widget,
				 NULL,
				 arrow_type,
				 TRUE,
				 widget->allocation.x + x,
				 widget->allocation.y + y,
				 width,
				 height);
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
	ButtonWidget *button_widget = BUTTON_WIDGET (widget);

	if (button_widget->scaled) {
		requisition->width  = gdk_pixbuf_get_width  (button_widget->scaled);
		requisition->height = gdk_pixbuf_get_height (button_widget->scaled);
	}
}

static void
button_widget_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
	ButtonWidget *button_widget = BUTTON_WIDGET (widget);
	GtkButton    *button = GTK_BUTTON (widget);
	int           size;

	if (button_widget->orientation & PANEL_HORIZONTAL_MASK)
		size = allocation->height;
	else
		size = allocation->width;

	if (button_widget->size != size) {
		button_widget->size = size;

		button_widget_load_pixbuf_and_scale (button_widget);
	}

	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED (widget)) {
		PanelWidget *panel;

		panel = PANEL_WIDGET (widget->parent);

		gdk_window_move_resize (button->event_window, 
					allocation->x,
					allocation->y,
					allocation->width,
					allocation->height);
	}
}

static void
button_widget_activate (GtkButton *button)
{
	ButtonWidget *button_widget = BUTTON_WIDGET (button);

	if (!button_widget->activatable)
		return;

	if (GTK_BUTTON_CLASS (parent_class)->activate)
		GTK_BUTTON_CLASS (parent_class)->activate (button);
}

static gboolean
button_widget_button_press (GtkWidget *widget, GdkEventButton *event)
{
	g_return_val_if_fail (BUTTON_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (event->button == 1 && BUTTON_WIDGET (widget)->activatable)
		return GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);

	return FALSE;
}

static gboolean
button_widget_enter_notify (GtkWidget *widget, GdkEventCrossing *event)
{
	gboolean in_button;

	g_return_val_if_fail (BUTTON_IS_WIDGET (widget), FALSE);

	in_button = GTK_BUTTON (widget)->in_button;
	GTK_WIDGET_CLASS (parent_class)->enter_notify_event (widget, event);
	if (in_button != GTK_BUTTON (widget)->in_button &&
	    panel_global_config_get_highlight_when_over ())
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
button_widget_instance_init (ButtonWidget *button)
{
	button->pixbuf    = NULL;
	button->scaled    = NULL;
	button->scaled_hc = NULL;
	
	button->arrow       = 0;
	button->orientation = PANEL_ORIENTATION_TOP;
	
	button->ignore_leave  = FALSE;
	button->dnd_highlight = FALSE;

	button->size = 0;

	g_signal_connect_object (panel_icon_theme, "changed",
				 G_CALLBACK (button_widget_icon_theme_changed),
				 button,
				 G_CONNECT_SWAPPED);

	g_signal_connect (button, "style-set", 
			  G_CALLBACK (button_widget_gtk_theme_changed), button);
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
	widget_class->unrealize          = button_widget_unrealize;
	widget_class->size_allocate      = button_widget_size_allocate;
	widget_class->size_request       = button_widget_size_request;
	widget_class->button_press_event = button_widget_button_press;
	widget_class->enter_notify_event = button_widget_enter_notify;
	widget_class->leave_notify_event = button_widget_leave_notify;
	widget_class->expose_event       = button_widget_expose;

	button_class->activate = button_widget_activate;

	g_object_class_install_property (
			gobject_class,
			PROP_ACTIVATABLE,
			g_param_spec_boolean ("activatable",
					      "Activatable",
					      "Whether the button is activatable",
					      TRUE,
					      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
			gobject_class,
			PROP_HAS_ARROW,
			g_param_spec_boolean ("has-arrow",
					      "Has Arrow",
					      "Whether or not to draw an arrow indicator",
					      FALSE,
					      G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_DND_HIGHLIGHT,
			g_param_spec_boolean ("dnd-highlight",
					      "Drag and drop Highlight",
					      "Whether or not to highlight the icon during drag and drop",
					      FALSE,
					      G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_ORIENTATION,
			g_param_spec_enum ("orientation",
					   "Orientation",
					   "The ButtonWidget orientation",
					   PANEL_TYPE_ORIENTATION,
					   PANEL_ORIENTATION_TOP,
					   G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_ICON_NAME,
			g_param_spec_string ("icon-name",
					     "Icon Name",
					     "The desired icon for the ButtonWidget",
					     NULL,
					     G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_STOCK_ID,
			g_param_spec_string ("stock-id",
					     "Stock Icon ID",
					     "The desired stock icon for the ButtonWidget",
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
			"orientation", orientation,
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
			"orientation", orientation,
			"stock-id", stock_id,
			NULL);
	
	return retval;
}

void
button_widget_set_activatable (ButtonWidget *button,
			       gboolean      activatable)
{
	g_return_if_fail (BUTTON_IS_WIDGET (button));

	activatable = activatable != FALSE;

	if (button->activatable != activatable) {
		button->activatable = activatable;

		if (GTK_WIDGET_DRAWABLE (button))
			gtk_widget_queue_draw (GTK_WIDGET (button));

		g_object_notify (G_OBJECT (button), "activatable");
	}
}

gboolean
button_widget_get_activatable (ButtonWidget *button)
{
	g_return_val_if_fail (BUTTON_IS_WIDGET (button), FALSE);

	return button->activatable;
}

void
button_widget_set_icon_name (ButtonWidget *button,
			     const char   *icon_name)
{
	g_return_if_fail (BUTTON_IS_WIDGET (button));

	if (button->filename && icon_name && !strcmp (button->filename, icon_name))
		return;

	if (button->filename)
		g_free (button->filename);
	button->filename = g_strdup (icon_name);

	button_widget_reload_pixbuf (button);

	g_object_notify (G_OBJECT (button), "icon-name");
}

const char *
button_widget_get_icon_name (ButtonWidget *button)
{
	g_return_val_if_fail (BUTTON_IS_WIDGET (button), NULL);

	return button->filename;
}

void
button_widget_set_stock_id (ButtonWidget *button,
			    const char   *stock_id)
{
	g_return_if_fail (BUTTON_IS_WIDGET (button));
	
	if (button->stock_id && stock_id && !strcmp (button->stock_id, stock_id))
		return;

	if (button->stock_id)
		g_free (button->stock_id);
	button->stock_id = g_strdup (stock_id);

	button_widget_reload_pixbuf (button);

	g_object_notify (G_OBJECT (button), "stock-id");
}

const char *
button_widget_get_stock_id (ButtonWidget *button)
{
	g_return_val_if_fail (BUTTON_IS_WIDGET (button), NULL);
	
	return button->stock_id;
}

void
button_widget_set_orientation (ButtonWidget     *button,
			       PanelOrientation  orientation)
{
	g_return_if_fail (BUTTON_IS_WIDGET (button));

	if (button->orientation == orientation)
		return;

	button->orientation = orientation;

	/* Force a re-scale */
	button->size = -1;

	gtk_widget_queue_resize (GTK_WIDGET (button));

	g_object_notify (G_OBJECT (button), "orientation");
}

PanelOrientation
button_widget_get_orientation (ButtonWidget *button)
{
	g_return_val_if_fail (BUTTON_IS_WIDGET (button), 0);

	return button->orientation;
}

void
button_widget_set_has_arrow (ButtonWidget *button,
			     gboolean      has_arrow)
{
	g_return_if_fail (BUTTON_IS_WIDGET (button));

	has_arrow = has_arrow != FALSE;

	if (button->arrow == has_arrow)
		return;

	button->arrow = has_arrow;

	gtk_widget_queue_draw (GTK_WIDGET (button));

	g_object_notify (G_OBJECT (button), "has-arrow");
}

gboolean
button_widget_get_has_arrow (ButtonWidget *button)
{
	g_return_val_if_fail (BUTTON_IS_WIDGET (button), FALSE);

	return button->arrow;
}

void
button_widget_set_dnd_highlight (ButtonWidget *button,
				 gboolean      dnd_highlight)
{
	g_return_if_fail (BUTTON_IS_WIDGET (button));

	dnd_highlight = dnd_highlight != FALSE;

	if (button->dnd_highlight == dnd_highlight)
		return;

	button->dnd_highlight = dnd_highlight;

	gtk_widget_queue_draw (GTK_WIDGET (button));

	g_object_notify (G_OBJECT (button), "dnd-highlight");
}

gboolean
button_widget_get_dnd_highlight (ButtonWidget *button)
{
	g_return_val_if_fail (BUTTON_IS_WIDGET (button), FALSE);

	return button->dnd_highlight;
}
