#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_filterlevel.h>
#include "button-widget.h"
#include "panel-widget.h"
#include "basep-widget.h"
#include "panel-types.h"
#include "panel-util.h"
#include "panel_config_global.h"
#include "rgb-stuff.h"

extern GlobalConfig global_config;

static void button_widget_class_init	(ButtonWidgetClass *klass);
static void button_widget_init		(ButtonWidget      *button);
static void button_widget_size_request  (GtkWidget         *widget,
					 GtkRequisition    *requisition);
static void button_widget_size_allocate (GtkWidget         *widget,
					 GtkAllocation     *allocation);
static void button_widget_realize	(GtkWidget         *widget);
static void button_widget_unrealize     (GtkWidget         *widget);
static void button_widget_map           (GtkWidget         *widget);
static void button_widget_unmap         (GtkWidget         *widget);
static void button_widget_real_draw     (GtkWidget         *widget,
					 GdkRectangle      *area);

static gboolean  button_widget_button_press	(GtkWidget         *widget,
						 GdkEventButton    *event);
static gboolean  button_widget_button_release(GtkWidget         *widget,
					      GdkEventButton    *event);
static gboolean  button_widget_enter_notify	(GtkWidget         *widget,
						 GdkEventCrossing  *event);
static gboolean  button_widget_leave_notify	(GtkWidget         *widget,
						 GdkEventCrossing  *event);
static void button_widget_pressed	(ButtonWidget *button);
static void button_widget_unpressed	(ButtonWidget *button);

/*list of all the button widgets*/
static GList *buttons = NULL;

/*the tiles go here*/
static struct {
	GdkPixbuf *tiles_up[LAST_TILE];
	GdkPixbuf *tiles_up_hc[LAST_TILE];
	GdkPixbuf *tiles_down[LAST_TILE];
	GdkPixbuf *tiles_down_hc[LAST_TILE];
} tiles = {{NULL}}; /*ansi C trick to make it all 0*/

static int tile_border[LAST_TILE]={0,0,0,0};
static int tile_depth[LAST_TILE]={0,0,0,0};

/*are tiles enabled*/
static gboolean tiles_enabled[LAST_TILE] = {FALSE,FALSE,FALSE,FALSE};

static gboolean pixmaps_enabled[LAST_TILE] = {TRUE,TRUE,TRUE,TRUE};
static gboolean always_text[LAST_TILE] = {FALSE,FALSE,FALSE,FALSE}; /*text always displayed*/

static GtkWidgetClass *parent_class;

guint
button_widget_get_type (void)
{
	static guint button_widget_type = 0;

	if (!button_widget_type) {
		GtkTypeInfo button_widget_info = {
			"ButtonWidget",
			sizeof (ButtonWidget),
			sizeof (ButtonWidgetClass),
			(GtkClassInitFunc) button_widget_class_init,
			(GtkObjectInitFunc) button_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};

		button_widget_type = gtk_type_unique (GTK_TYPE_WIDGET,
						      &button_widget_info);
	}

	return button_widget_type;
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
	GtkObjectClass *object_class = (GtkObjectClass*) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass*) class;

	parent_class = gtk_type_class (GTK_TYPE_WIDGET);

	button_widget_signals[CLICKED_SIGNAL] =
		gtk_signal_new("clicked",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(ButtonWidgetClass,
			       			 clicked),
			       gtk_signal_default_marshaller,
			       GTK_TYPE_NONE,
			       0);
	button_widget_signals[PRESSED_SIGNAL] =
		gtk_signal_new("pressed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(ButtonWidgetClass,
			       			 pressed),
			       gtk_signal_default_marshaller,
			       GTK_TYPE_NONE,
			       0);
	button_widget_signals[UNPRESSED_SIGNAL] =
		gtk_signal_new("unpressed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(ButtonWidgetClass,
			       			 unpressed),
			       gtk_signal_default_marshaller,
			       GTK_TYPE_NONE,
			       0);
	gtk_object_class_add_signals(object_class,button_widget_signals,
				     LAST_SIGNAL);
	
	class->clicked = NULL;
	class->pressed = button_widget_pressed;
	class->unpressed = button_widget_unpressed;

	widget_class->realize = button_widget_realize;
	widget_class->unrealize = button_widget_unrealize;
	widget_class->draw = button_widget_real_draw;
	widget_class->map = button_widget_map;
	widget_class->unmap = button_widget_unmap;
	widget_class->size_allocate = button_widget_size_allocate;
	widget_class->size_request = button_widget_size_request;
	widget_class->button_press_event = button_widget_button_press;
	widget_class->button_release_event = button_widget_button_release;
	widget_class->enter_notify_event = button_widget_enter_notify;
	widget_class->leave_notify_event = button_widget_leave_notify;
	
}

static void
setup_no_alpha(ButtonWidget *button)
{
	button->no_alpha = 0;
	if(!tiles_enabled[button->tile] ||
	   global_config.tile_when_over)
		return;
	if(button->pressed) {
		if(tiles.tiles_down[button->tile] &&
		   !gdk_pixbuf_get_has_alpha(tiles.tiles_down[button->tile]))
			button->no_alpha = 1;
	} else {
		if(tiles.tiles_up[button->tile] &&
		   !gdk_pixbuf_get_has_alpha(tiles.tiles_up[button->tile]))
			button->no_alpha = 1;
	}
}

static void
translate_to(GtkWidget *from, GtkWidget *to, int *x, int *y)
{
	while (from != to) {
		if(!GTK_WIDGET_NO_WINDOW(from)) {
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
	if(IS_BASEP_WIDGET(parent) &&
	   BASEP_WIDGET(parent)->state != BASEP_SHOWN &&
	   BASEP_WIDGET(parent)->state != BASEP_AUTO_HIDDEN) {
		*x = parent->requisition.width + 1;
		*y = parent->requisition.height + 1;
		return;
	}

	if(panel->orient == PANEL_HORIZONTAL) {
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

		if ( ! IS_BASEP_WIDGET(parent)) {
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

		if ( ! IS_BASEP_WIDGET(parent)) {
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
	g_return_if_fail (IS_BUTTON_WIDGET (widget));

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
	gdk_window_ref(widget->window);
      
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
	g_return_if_fail (IS_BUTTON_WIDGET (widget));

	button_widget = BUTTON_WIDGET (widget);
	
	gdk_window_set_user_data (button_widget->event_window, NULL);
	gdk_window_destroy (button_widget->event_window);
	button_widget->event_window = NULL;
	
	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
button_widget_map (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_BUTTON_WIDGET (widget));

	if (GTK_WIDGET_REALIZED (widget) && !GTK_WIDGET_MAPPED (widget))
		gdk_window_show (BUTTON_WIDGET (widget)->event_window);

	GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
button_widget_unmap (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_BUTTON_WIDGET (widget));

	if (GTK_WIDGET_MAPPED (widget))
		gdk_window_hide (BUTTON_WIDGET (widget)->event_window);

	GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
button_widget_destroy(GtkWidget *w, gpointer data)
{
	ButtonWidget *button = BUTTON_WIDGET(w);

	if(button->pressed_timeout != 0)
		gtk_timeout_remove(button->pressed_timeout);

	if(button->pixbuf)
		gdk_pixbuf_unref(button->pixbuf);
	button->pixbuf = NULL;
	if(button->pixbuf_hc)
		gdk_pixbuf_unref(button->pixbuf_hc);
	button->pixbuf_hc = NULL;
	if(button->cache)
		gdk_pixmap_unref(button->cache);
	button->cache = NULL;

	g_free(button->filename);
	g_free(button->text);

	buttons = g_list_remove(buttons,button);
}

static GdkPixbuf *
loadup_file(const char *file)
{
	GdkPixbuf *pb = NULL;
	
	if (string_empty (file))
		return NULL;

	if ( ! g_path_is_absolute(file)) {
		char *f;
		f = gnome_pixmap_file (file);
		if (f != NULL) {
			pb = gdk_pixbuf_new_from_file(f);
			g_free (f);
		}
	} else {
		pb = gdk_pixbuf_new_from_file (file);
	}
	return pb;
}

#if 0
/*#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)*/
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
draw_arrow(GdkPoint *points, PanelOrientType orient, int size)
{
	switch(orient) {
	case ORIENT_UP:
		points[0].x = SCALE(48-12);
		points[0].y = SCALE(10);
		points[1].x = SCALE(48-4);
		points[1].y = SCALE(10);
		points[2].x = SCALE(48-8);
		points[2].y = SCALE(3);
		break;
	case ORIENT_DOWN:
		points[0].x = SCALE(4);
		points[0].y = SCALE(48 - 10);
		points[1].x = SCALE(12);
		points[1].y = SCALE(48 - 10);
		points[2].x = SCALE(8);
		points[2].y = SCALE(48 - 3);
		break;
	case ORIENT_LEFT:
		points[0].x = SCALE(10);
		points[0].y = SCALE(4);
		points[1].x = SCALE(10);
		points[1].y = SCALE(12);
		points[2].x = SCALE(3);
		points[2].y = SCALE(8);
		break;
	case ORIENT_RIGHT:
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
	g_return_if_fail (IS_BUTTON_WIDGET (button));

	if(button->dnd_highlight != highlight) {
		button->dnd_highlight = highlight;
		if(button->cache)
			gdk_pixmap_unref(button->cache);
		button->cache = NULL;

		panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
				       button);
	}
}

static void
button_widget_real_draw(GtkWidget *widget, GdkRectangle *area)
{
	if(widget->parent && IS_PANEL_WIDGET(widget->parent))
		panel_widget_draw_icon(PANEL_WIDGET(widget->parent), BUTTON_WIDGET(widget));

}

void
button_widget_draw(ButtonWidget *button, guchar *rgb, int rowstride)
{
	GtkWidget *widget, *pwidget;
	PanelWidget *panel;
	int size, off, border;

	g_return_if_fail(button != NULL);
	g_return_if_fail(IS_BUTTON_WIDGET(button));
	g_return_if_fail(rgb != NULL);
	g_return_if_fail(rowstride >= 0);

	widget = GTK_WIDGET(button);
	panel = PANEL_WIDGET(widget->parent);
	size = panel->sz;
	/*offset for pressed buttons*/
	off = (button->in_button && button->pressed) ?
		SCALE(tile_depth[button->tile]) : 0;
	/*border to not draw when drawing a tile*/
	border = tiles_enabled[button->tile] ?
		SCALE(tile_border[button->tile]) : 0;
	 
	button->size = size;
	
	pwidget = widget->parent;
	
	if(tiles_enabled[button->tile]) {
		GdkPixbuf *pb = NULL;
		if (button->pressed &&
		    button->in_button) {
			/* Pressed down */
			if(!global_config.saturate_when_over ||
			   !button->in_button) {
				pb = tiles.tiles_down[button->tile];
			} else {
				pb = tiles.tiles_down_hc[button->tile];
			}
		} else if ( ! global_config.tile_when_over ||
			    button->in_button) {
			/* not pressed */
			if( ! global_config.saturate_when_over ||
			    ! button->in_button) {
				pb = tiles.tiles_up[button->tile];
			} else {
				pb = tiles.tiles_up_hc[button->tile];
			}
		}

		if(pb != NULL) {
			GdkPixbuf *scaled_pb;
			double affine[6];
			GdkInterpType interp;

			if (global_config.fast_button_scaling)
				interp = GDK_INTERP_NEAREST;
			else
				interp = GDK_INTERP_HYPER;

			scaled_pb = scale_pixbuf_to_square (pb, size,
							    NULL, NULL,
							    interp);

			art_affine_identity (affine);

			transform_pixbuf (rgb, 0, 0, size, size,
					  rowstride, scaled_pb, affine,
					  ART_FILTER_NEAREST, NULL);

			gdk_pixbuf_unref (scaled_pb);
		}
	}

	if (pixmaps_enabled[button->tile]) {
		GdkPixbuf *pb = NULL;
		if(!global_config.saturate_when_over || !button->in_button) {
			pb = button->pixbuf;
		} else {
			pb = button->pixbuf_hc;
		}
		if(pb != NULL) {
			double affine[6];
			int w, h;
			GdkPixbuf *scaled_pb;
			GdkInterpType interp;

			if (global_config.fast_button_scaling)
				interp = GDK_INTERP_NEAREST;
			else
				interp = GDK_INTERP_HYPER;

			scaled_pb = scale_pixbuf_to_square (pb, size, &w, &h,
							    interp);

			art_affine_translate(affine,
					     -border+off + (size-w)/2,
					     -border+off + (size-h)/2);

			transform_pixbuf((rgb+border*rowstride+border*3),
				         0, 0,
				         size-2*border, size-2*border,
				         rowstride,
				         scaled_pb,
				         affine, ART_FILTER_NEAREST, NULL);

			gdk_pixbuf_unref (scaled_pb);
		}
	}
}

/* draw the xlib part (arrow/text/dndhighlight) */
void
button_widget_draw_xlib(ButtonWidget *button, GdkPixmap *pixmap)
{
	GtkWidget *widget, *pwidget;
	GdkGC *gc;
	int size, off;

	g_return_if_fail(button != NULL);
	g_return_if_fail(IS_BUTTON_WIDGET(button));
	g_return_if_fail(pixmap != NULL);

	widget = GTK_WIDGET(button);
	size = button->size;

	/*offset for pressed buttons*/
	off = (button->in_button && button->pressed) ?
		SCALE(tile_depth[button->tile]) : 0;
	 
	gc = gdk_gc_new(pixmap);
	
	pwidget = widget->parent;
	/*draw text*/
	if (!pixmaps_enabled[button->tile] ||
	    always_text[button->tile] ||
	    !button->pixbuf) {
		char *text = g_strdup(button->text);
		int twidth,theight;
		GdkFont *font;
		GdkRectangle rect;
	         
	        rect.x = 0;
	        rect.y = 0;
	        rect.width = widget->allocation.width;
	        rect.height = widget->allocation.height;

		if(!text) text = g_strdup("XXX");
		
		font = gdk_fontset_load(_("-*-helvetica-medium-r-normal-*-8-*-*-*-*-*-*-*"));
		if(!font)
			font = gdk_font_load("fixed");
		if(!font)
			font = widget->style->font;
		

		gdk_gc_set_clip_rectangle (gc, &rect);

		twidth = gdk_string_width(font,text);
		theight = gdk_string_height(font,text);
		if(twidth>size)
			twidth = size;
		
		gdk_gc_set_foreground(gc,&widget->style->black);
		gdk_draw_rectangle(pixmap,gc,TRUE,
				   (widget->allocation.width/2)-(twidth/2)+off-1,
				   (widget->allocation.height/2)-(theight/2)-1+off,
				   twidth+2,
				   theight+2);
		gdk_gc_set_foreground(gc,&widget->style->white);
		gdk_draw_string(pixmap,font,gc,
				(widget->allocation.width/2)-(twidth/2)+off,
				(widget->allocation.height/2)+(theight/2)+off,
				text);
		gdk_gc_set_foreground(gc,&widget->style->black);
		gdk_gc_set_clip_rectangle (gc, NULL);
		if(font!=widget->style->font)
			gdk_font_unref(font);
		g_free(text);
	}

	
	if(button->arrow) {
		int i;
		GdkPoint points[3];
		draw_arrow(points,button->orient,size);
		for(i=0;i<3;i++) {
			points[i].x+=off;
			points[i].y+=off;
		}
		gdk_gc_set_foreground(gc,&pwidget->style->white);
		gdk_draw_polygon(pixmap,gc,TRUE,points,3);
		gdk_gc_set_foreground(gc,&pwidget->style->black);
		gdk_draw_polygon(pixmap,gc,FALSE,points,3);
	}

	if (button->dnd_highlight) {
		gdk_gc_set_foreground(gc, &widget->style->black);
		gdk_draw_rectangle(pixmap, gc, FALSE,
				   0, 0,
				   widget->allocation.width - 1,
				   widget->allocation.height - 1);
	}
	
	gdk_gc_destroy(gc);
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

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_BUTTON_WIDGET (widget));

	button_widget = BUTTON_WIDGET (widget);

	widget->allocation = *allocation;
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
button_widget_init (ButtonWidget *button)
{
	buttons = g_list_prepend(buttons,button);

	GTK_WIDGET_SET_FLAGS (button, GTK_NO_WINDOW);

	button->pixbuf = NULL;
	button->pixbuf_hc = NULL;
	
	button->tile = 0;
	button->arrow = 0;
	button->orient = ORIENT_UP;
	
	button->pressed = FALSE;
	button->in_button = FALSE;
	button->ignore_leave = FALSE;
	button->dnd_highlight = FALSE;
	
	button->pressed_timeout = 0;
	
	gtk_signal_connect(GTK_OBJECT(button),"destroy",
			   GTK_SIGNAL_FUNC(button_widget_destroy),
			   NULL);
}

static gboolean
pressed_timeout_func(gpointer data)
{
	ButtonWidget *button;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (IS_BUTTON_WIDGET (data), FALSE);

	button = BUTTON_WIDGET (data);
	
	button->pressed_timeout = 0;
	
	return FALSE;
}

static gboolean
button_widget_button_press (GtkWidget *widget, GdkEventButton *event)
{
	ButtonWidget *button;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (IS_BUTTON_WIDGET (widget), FALSE);
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
	g_return_val_if_fail (IS_BUTTON_WIDGET (widget), FALSE);
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
	g_return_val_if_fail (IS_BUTTON_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	event_widget = gtk_get_event_widget ((GdkEvent*) event);

	if ((event_widget == widget) &&
	    (event->detail != GDK_NOTIFY_INFERIOR)) {
		ButtonWidget *button = BUTTON_WIDGET (widget);
		button->in_button = TRUE;
		if(global_config.tile_when_over ||
		   global_config.saturate_when_over) {
			if(button->cache)
				gdk_pixmap_unref(button->cache);
			button->cache = NULL;
			panel_widget_draw_icon(PANEL_WIDGET(widget->parent),
					       button);
		}
	}

	return FALSE;
}

static gboolean
button_widget_leave_notify (GtkWidget *widget, GdkEventCrossing *event)
{
	GtkWidget *event_widget;
	ButtonWidget *button;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (IS_BUTTON_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	event_widget = gtk_get_event_widget ((GdkEvent*) event);

	button = BUTTON_WIDGET (widget);

	if ((event_widget == widget) &&
	    (event->detail != GDK_NOTIFY_INFERIOR) &&
	    (!button->ignore_leave)) {
		button->in_button = FALSE;
		if(global_config.tile_when_over ||
		   global_config.saturate_when_over) {
			if(button->cache)
				gdk_pixmap_unref(button->cache);
			button->cache = NULL;
			panel_widget_draw_icon(PANEL_WIDGET(widget->parent),
					       button);
		}
	}

	return FALSE;
}


void
button_widget_clicked(ButtonWidget *button)
{
	gtk_signal_emit(GTK_OBJECT(button),
			button_widget_signals[CLICKED_SIGNAL]);
}

static void
button_widget_pressed(ButtonWidget *button)
{
	g_return_if_fail(button != NULL);
	g_return_if_fail(IS_BUTTON_WIDGET(button));

	button->pressed = TRUE;
	if(button->cache)
		gdk_pixmap_unref(button->cache);
	button->cache = NULL;
	setup_no_alpha(button);
	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);
}
static void
button_widget_unpressed(ButtonWidget *button)
{
	g_return_if_fail(button != NULL);
	g_return_if_fail(IS_BUTTON_WIDGET(button));

	button->pressed = FALSE;
	if(button->cache)
		gdk_pixmap_unref(button->cache);
	button->cache = NULL;
	setup_no_alpha(button);
	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);
	if(button->in_button)
		button_widget_clicked(button);
}

void
button_widget_down(ButtonWidget *button)
{
	g_return_if_fail(button != NULL);
	g_return_if_fail(IS_BUTTON_WIDGET(button));

	if(!button->pressed)
		gtk_signal_emit(GTK_OBJECT(button),
				button_widget_signals[PRESSED_SIGNAL]);
}
void
button_widget_up(ButtonWidget *button)
{
	g_return_if_fail(button != NULL);
	g_return_if_fail(IS_BUTTON_WIDGET(button));

	if(button->pressed)
		gtk_signal_emit(GTK_OBJECT(button),
				button_widget_signals[UNPRESSED_SIGNAL]);
}

static GdkPixbuf *
make_hc_pixbuf(GdkPixbuf *pb)
{
	GdkPixbuf *new;
	if(!pb)
		return NULL;

	new = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(pb),
			     gdk_pixbuf_get_has_alpha(pb),
			     gdk_pixbuf_get_bits_per_sample(pb),
			     gdk_pixbuf_get_width(pb),
			     gdk_pixbuf_get_height(pb));
	do_colorshift(new, pb, 30);
	/*do_saturate_darken (new, pb, (int)(1.00*255), (int)(1.15*255));*/

	return new;
}


GtkWidget*
button_widget_new(const char *filename,
		  int size,
		  int tile,
		  gboolean arrow,
		  PanelOrientType orient,
		  const char *text)
{
	ButtonWidget *button;

	g_return_val_if_fail(tile >= 0, NULL);
	g_return_val_if_fail(tile < LAST_TILE, NULL);

	button = BUTTON_WIDGET (gtk_type_new (button_widget_get_type ()));
	
	button->pixbuf = loadup_file (filename);
	button->pixbuf_hc = make_hc_pixbuf (button->pixbuf);
	button->filename = g_strdup (filename);
	button->size = size;
	button->tile = tile;
	button->arrow = arrow ? 1 : 0;
	button->orient = orient;
	button->text = text ? g_strdup (text) : NULL;

	setup_no_alpha(button);
	
	return GTK_WIDGET(button);
}

gboolean
button_widget_set_pixmap(ButtonWidget *button, const char *pixmap, int size)
{
	g_return_val_if_fail(button != NULL, FALSE);
	g_return_val_if_fail(IS_BUTTON_WIDGET(button), FALSE);

	if (size < 0)
		size = PANEL_WIDGET(GTK_WIDGET(button)->parent)->sz;
	
	if (button->pixbuf != NULL)
		gdk_pixbuf_unref(button->pixbuf);
	if (button->pixbuf_hc != NULL)
		gdk_pixbuf_unref(button->pixbuf_hc);

	button->pixbuf = loadup_file(pixmap);
	button->pixbuf_hc = make_hc_pixbuf(button->pixbuf);

	g_free(button->filename);
	button->filename = g_strdup(pixmap);
	button->size = size;
	if (button->cache != NULL)
		gdk_pixmap_unref(button->cache);
	button->cache = NULL;

	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);

	if (button->pixbuf == NULL)
		return FALSE;
	
	return TRUE;
}

void
button_widget_set_text(ButtonWidget *button, const char *text)
{
	g_return_if_fail(button != NULL);
	g_return_if_fail(IS_BUTTON_WIDGET(button));

	g_free(button->text);
	button->text = text?g_strdup(text):NULL;

	if (button->cache != NULL)
		gdk_pixmap_unref(button->cache);
	button->cache = NULL;
	
	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);
}

void
button_widget_set_params(ButtonWidget *button,
			 int tile,
			 gboolean arrow,
			 PanelOrientType orient)
{
	g_return_if_fail(button != NULL);
	g_return_if_fail(IS_BUTTON_WIDGET(button));
	g_return_if_fail(tile >= 0);
	g_return_if_fail(tile < LAST_TILE);

	button->tile = tile;
	button->arrow = arrow?1:0;
	button->orient = orient;

	if(button->cache)
		gdk_pixmap_unref(button->cache);
	button->cache = NULL;
	setup_no_alpha(button);
	
	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);
}

void
button_widget_load_tile(int tile, const char *tile_up, const char *tile_down,
			int border, int depth)
{
	GdkPixbuf *pb;

	g_return_if_fail(tile >= 0);
	g_return_if_fail(tile < LAST_TILE);
	g_return_if_fail(tile_up != NULL);
	g_return_if_fail(tile_down != NULL);

	if(tiles.tiles_up[tile])
		gdk_pixbuf_unref(tiles.tiles_up[tile]);
	if(tiles.tiles_up_hc[tile])
		gdk_pixbuf_unref(tiles.tiles_up_hc[tile]);

	pb = loadup_file(tile_up);
	tiles.tiles_up[tile] = pb;
	tiles.tiles_up_hc[tile] = make_hc_pixbuf(pb);

	if(tiles.tiles_down[tile])
		gdk_pixbuf_unref(tiles.tiles_down[tile]);
	if(tiles.tiles_down_hc[tile])
		gdk_pixbuf_unref(tiles.tiles_down_hc[tile]);

	pb = loadup_file(tile_down);
	tiles.tiles_down[tile] = pb;
	tiles.tiles_down_hc[tile] = make_hc_pixbuf(pb);

	tile_border[tile] = border;
	tile_depth[tile] = depth;

	button_widget_redo_all ();
}

void
button_widget_set_flags(int tile,
			gboolean enable_tiles,
			gboolean enable_pixmaps,
			gboolean text_always)
{
	g_return_if_fail (tile >= 0);
	g_return_if_fail (tile < LAST_TILE);

	if(tiles_enabled[tile] != enable_tiles ||
	   pixmaps_enabled[tile] != enable_pixmaps ||
	   always_text[tile] != text_always) {
		tiles_enabled[tile] = enable_tiles;
		pixmaps_enabled[tile] = enable_pixmaps;
		always_text[tile] = text_always;

		button_widget_redo_all ();
	}
}

void
button_widget_redo_all (void)
{
	GList *list;

	for(list = buttons; list != NULL; list = list->next) {
		ButtonWidget *button = list->data;
		if(button->cache != NULL)
			gdk_pixmap_unref(button->cache);
		button->cache = NULL;
		setup_no_alpha(button);
		gtk_widget_queue_draw (GTK_WIDGET (button));
	}
}
