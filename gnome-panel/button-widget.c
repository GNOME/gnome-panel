#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_rgb_pixbuf_affine.h>
#include "button-widget.h"
#include "panel-widget.h"
#include "panel-types.h"
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

static int  button_widget_button_press	(GtkWidget         *widget,
					 GdkEventButton    *event);
static int  button_widget_button_release(GtkWidget         *widget,
					 GdkEventButton    *event);
static int  button_widget_enter_notify	(GtkWidget         *widget,
					 GdkEventCrossing  *event);
static int  button_widget_leave_notify	(GtkWidget         *widget,
					 GdkEventCrossing  *event);
static void button_widget_pressed	(ButtonWidget *button);
static void button_widget_unpressed	(ButtonWidget *button);

typedef void (*VoidSignal) (GtkObject * object,
			    gpointer data);

/*list of all the button widgets*/
static GList *buttons=NULL;

/*the tiles go here*/
struct {
	GdkPixBuf *tiles_up[LAST_TILE];
	GdkPixBuf *tiles_down[LAST_TILE];
} tiles = {{NULL}}; /*ansi C trick to make it all 0*/

static int tile_border[LAST_TILE]={0,0,0,0};
static int tile_depth[LAST_TILE]={0,0,0,0};

/*are tiles enabled*/
static int tiles_enabled[LAST_TILE]={FALSE,FALSE,FALSE,FALSE};

static int pixmaps_enabled[LAST_TILE] = {TRUE,TRUE,TRUE,TRUE};
static int always_text[LAST_TILE] = {FALSE,FALSE,FALSE,FALSE}; /*text always displayed*/

static GtkWidgetClass *parent_class;

guint
button_widget_get_type ()
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

static int button_widget_signals[LAST_SIGNAL] = {0};

static void
marshal_signal_void (GtkObject * object,
		     GtkSignalFunc func,
		     gpointer func_data,
		     GtkArg * args)
{
	VoidSignal rfunc;

	rfunc = (VoidSignal) func;

	(*rfunc) (object, func_data);
}


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
			       marshal_signal_void,
			       GTK_TYPE_NONE,
			       0);
	button_widget_signals[PRESSED_SIGNAL] =
		gtk_signal_new("pressed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(ButtonWidgetClass,
			       			 pressed),
			       marshal_signal_void,
			       GTK_TYPE_NONE,
			       0);
	button_widget_signals[UNPRESSED_SIGNAL] =
		gtk_signal_new("unpressed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(ButtonWidgetClass,
			       			 unpressed),
			       marshal_signal_void,
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

/*static int
get_size(ButtonWidget *button)
{
	int sizes[SIZE_HUGE+1]={24,48,64,80};
	return sizes[size];
}*/

static void
button_widget_realize(GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	ButtonWidget *button_widget;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_BUTTON_WIDGET (widget));

	button_widget = BUTTON_WIDGET (widget);

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

	widget->window = gtk_widget_get_parent_window(widget);
	gdk_window_ref(widget->window);
      
	button_widget->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
						      &attributes, attributes_mask);
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

	if(button->pixbuf)
		gdk_pixbuf_destroy(button->pixbuf);
	button->pixbuf = NULL;

	g_free(button->text);

	buttons = g_list_remove(buttons,button);
}
/*
static GdkPixBuf *
loadup_file(char *file, int size)
{
	GdkPixBuf *pb = NULL;
	int w,h;
	
	if(!file) {
		return NULL;
	}

	if(*file!='/') {
		char *f;
		f = gnome_pixmap_file (file);
		if(f) {
			pb = gdk_pixbuf_load_image(f);
			g_free(f);
		}
	} else
		pb = gdk_pixbuf_load_image (file);
	if(!pb) {
		return NULL;
	}

	w = pb->art_pixbuf->width;
	h = pb->art_pixbuf->height;
	if(w>h) {
		h = h*((double)size/w);
		w = size;
	} else {
		w = w*((double)size/h);
		h = size;
	}
	w = w>0?w:1;
	h = h>0?h:1;
	pb2 = gdk_pixbuf_scale(pb, w, h);
	gdk_imlib_render (im, w, h);

	*pixmap = gdk_imlib_copy_image (im);
	*mask = gdk_imlib_copy_mask (im);

	gdk_imlib_destroy_image (im);
}*/

static GdkPixBuf *
loadup_file(char *file)
{
	GdkPixBuf *pb = NULL;
	
	if(!file) {
		return NULL;
	}

	if(*file!='/') {
		char *f;
		f = gnome_pixmap_file (file);
		if(f) {
			pb = gdk_pixbuf_load_image(f);
			g_free(f);
		}
	} else
		pb = gdk_pixbuf_load_image (file);
	return pb;
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

static void
button_widget_real_draw(GtkWidget *widget, GdkRectangle *area)
{
	if(widget->parent && IS_PANEL_WIDGET(widget->parent))
		panel_widget_draw_icon(PANEL_WIDGET(widget->parent), BUTTON_WIDGET(widget));
}

void
button_widget_draw(ButtonWidget *button, guchar *rgb, int rowstride)
{
	GtkWidget *widget = GTK_WIDGET(button);
	GtkWidget *pwidget;
	PanelWidget *panel = PANEL_WIDGET(widget->parent);
	int size = panel_widget_get_pixel_size(panel);
	/*offset for pressed buttons*/
	int off = button->in_button&&button->pressed?SCALE(tile_depth[button->tile]):0;
	/*border to not draw when drawing a tile*/
	int border = tiles_enabled[button->tile]?SCALE(tile_border[button->tile]):0;
	 
	button->size = size;
	
	pwidget = widget->parent;
	
	if(tiles_enabled[button->tile]) {
		if(button->pressed && button->in_button) {
			if(tiles.tiles_down[button->tile]) {
				double affine[6];
				make_scale_affine(affine,
						  tiles.tiles_down[button->tile]->art_pixbuf->width,
						  tiles.tiles_down[button->tile]->art_pixbuf->height,
						  size);
				art_rgb_pixbuf_affine (rgb, 0, 0, size, size, rowstride,
						       tiles.tiles_down[button->tile]->art_pixbuf,
						       affine, ART_FILTER_NEAREST, NULL);
			}
		} else if (!global_config.tile_when_over || button->in_button) {
			if(tiles.tiles_up[button->tile]) {
				double affine[6];
				make_scale_affine(affine,
						  tiles.tiles_up[button->tile]->art_pixbuf->width,
						  tiles.tiles_up[button->tile]->art_pixbuf->height,
						  size);
				art_rgb_pixbuf_affine (rgb, 0, 0, size, size, rowstride,
						       tiles.tiles_up[button->tile]->art_pixbuf,
						       affine, ART_FILTER_NEAREST, NULL);
			}
		}
	}

	if (pixmaps_enabled[button->tile]) {
		if(button->pixbuf) {
			double affine[6];
			double transl[6];
			make_scale_affine(affine,
					  button->pixbuf->art_pixbuf->width,
					  button->pixbuf->art_pixbuf->height,
					  size);
			art_affine_translate(transl,-border+off,-border+off);
			art_affine_multiply(affine,affine,transl);

			art_rgb_pixbuf_affine((rgb+border*rowstride+border*3),
					      0, 0,
					      size-2*border, size-2*border,
					      rowstride,
					      button->pixbuf->art_pixbuf,
					      affine, ART_FILTER_NEAREST, NULL);
		}
	}
}

/* draw the xlib part (arrow/text) */
void
button_widget_draw_xlib(ButtonWidget *button, GdkPixmap *pixmap, int offx, int offy)
{
	GtkWidget *widget = GTK_WIDGET(button);
	GtkWidget *pwidget;
	GdkGC *gc;
	int size = button->size;
	/*offset for pressed buttons*/
	int off = button->in_button&&button->pressed?SCALE(tile_depth[button->tile]):0;
	/*border to not draw when drawing a tile*/
	 
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
	         
	        rect.x = widget->allocation.x-offx;
	        rect.y = widget->allocation.y-offy;
	        rect.width = widget->allocation.width;
	        rect.height = widget->allocation.height;

		if(!text) text = g_strdup("XXX");
		
		font = gdk_font_load("-*-helvetica-medium-r-normal-*-8-*-*-*-*-*-*-*");
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
				   widget->allocation.x+(widget->allocation.width/2)-(twidth/2)+off-1-offx,
				   widget->allocation.y+(widget->allocation.height/2)-(theight/2)-1+off-offy,
				   twidth+2,
				   theight+2);
		gdk_gc_set_foreground(gc,&widget->style->white);
		gdk_draw_string(pixmap,font,gc,
				widget->allocation.x+(widget->allocation.width/2)-(twidth/2)+off-offx,
				widget->allocation.y+(widget->allocation.height/2)+(theight/2)+off-offy,
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
			points[i].x+=widget->allocation.x+off-offx;
			points[i].y+=widget->allocation.y+off-offy;
		}
		gdk_gc_set_foreground(gc,&pwidget->style->white);
		gdk_draw_polygon(pixmap,gc,TRUE,points,3);
		gdk_gc_set_foreground(gc,&pwidget->style->black);
		gdk_draw_polygon(pixmap,gc,FALSE,points,3);
	}
	
	gdk_gc_destroy(gc);
}

static void
button_widget_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	PanelWidget *panel = PANEL_WIDGET(widget->parent);
	requisition->width = requisition->height = 
		panel_widget_get_pixel_size(panel);
}
static void
button_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	ButtonWidget *button_widget;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_BUTTON_WIDGET (widget));

	button_widget = BUTTON_WIDGET (widget);

	widget->allocation = *allocation;
	if (GTK_WIDGET_REALIZED (widget))
		gdk_window_move_resize (button_widget->event_window,
					allocation->x, allocation->y,
					allocation->width, allocation->height);
}



static void
button_widget_init (ButtonWidget *button)
{
	buttons = g_list_prepend(buttons,button);

	GTK_WIDGET_SET_FLAGS (button, GTK_NO_WINDOW);

	button->pixbuf = NULL;
	
	button->tile = 0;
	button->arrow = 0;
	button->orient = ORIENT_UP;
	
	button->pressed = FALSE;
	button->in_button = FALSE;
	button->ignore_leave = FALSE;
	
	button->pressed_timeout = 0;
	
	gtk_signal_connect(GTK_OBJECT(button),"destroy",
			   GTK_SIGNAL_FUNC(button_widget_destroy),
			   NULL);
}

static int
pressed_timeout_func(gpointer data)
{
	ButtonWidget *button;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (IS_BUTTON_WIDGET (data), FALSE);

	button = BUTTON_WIDGET (data);
	
	button->pressed_timeout=0;
	
	return FALSE;
}

static int
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
			gtk_timeout_add(400,pressed_timeout_func,button);
	}
	return TRUE;
}

static int
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

static int
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
		panel_widget_draw_icon(PANEL_WIDGET(widget->parent),
				       button);
	}

	return FALSE;
}

static int
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
		panel_widget_draw_icon(PANEL_WIDGET(widget->parent),
				       button);
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
	button->pressed = TRUE;
	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);
}
static void
button_widget_unpressed(ButtonWidget *button)
{
	button->pressed = FALSE;
	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);
	if(button->in_button)
		button_widget_clicked(button);
}

void
button_widget_down(ButtonWidget *button)
{
	if(!button->pressed)
		gtk_signal_emit(GTK_OBJECT(button),
				button_widget_signals[PRESSED_SIGNAL]);
}
void
button_widget_up(ButtonWidget *button)
{
	if(button->pressed)
		gtk_signal_emit(GTK_OBJECT(button),
				button_widget_signals[UNPRESSED_SIGNAL]);
}


GtkWidget*
button_widget_new(char *filename,
		  int size,
		  guint tile,
		  guint arrow,
		  PanelOrientType orient,
		  char *text)
{
	ButtonWidget *button;

	button = BUTTON_WIDGET (gtk_type_new (button_widget_get_type ()));
	
	button->pixbuf = loadup_file(filename);
	button->filename = g_strdup(filename);
	button->size = size;
	button->tile = tile;
	button->arrow = arrow;
	button->orient = orient;
	button->text = text?g_strdup(text):NULL;
	
	return GTK_WIDGET(button);
}

int
button_widget_set_pixmap(ButtonWidget *button, char *pixmap, int size)
{
	if(size<0)
		size = panel_widget_get_pixel_size(PANEL_WIDGET(GTK_WIDGET(button)->parent));
	
	button->pixbuf = loadup_file(pixmap);
	g_free(button->filename);
	button->filename = g_strdup(pixmap);
	button->size = size;

	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);

	if(!button->pixbuf)
		return FALSE;
	
	return TRUE;
}

void
button_widget_set_text(ButtonWidget *button, char *text)
{
	g_free(button->text);
	button->text = text?g_strdup(text):NULL;
	
	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);
}

void
button_widget_set_params(ButtonWidget *button,
			 guint tile,
			 guint arrow,
			 PanelOrientType orient)
{
	button->tile = tile;
	button->arrow = arrow;
	button->orient = orient;
	
	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);
}

void
button_widget_load_tile(int tile, char *tile_up, char *tile_down,
			int border, int depth)
{
	GList *list;

	if(tiles.tiles_up[tile])
		gdk_pixbuf_destroy(tiles.tiles_up[tile]);
	tiles.tiles_up[tile] = loadup_file(tile_up);

	if(tiles.tiles_down[tile])
		gdk_pixbuf_destroy(tiles.tiles_down[tile]);
	tiles.tiles_down[tile] = loadup_file(tile_down);

	tile_border[tile] = border;
	tile_depth[tile] = depth;
	
	for(list = buttons;list!=NULL;list=g_list_next(list)) {
		ButtonWidget *button = list->data;
		if(button->tile == tile)
			panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
					       button);
	}
}

void
button_widget_set_flags(int type, int _tiles_enabled, int _pixmaps_enabled, int _always_text)
{
	if(tiles_enabled[type] != _tiles_enabled ||
	   pixmaps_enabled[type] != _pixmaps_enabled ||
	   always_text[type] != _always_text) {
		GList *list;

		tiles_enabled[type] = _tiles_enabled;
		pixmaps_enabled[type] = _pixmaps_enabled;
		always_text[type] = _always_text;

		for(list = buttons;list!=NULL;list=g_list_next(list))
			panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(list->data)->parent),
					       list->data);
	}
}
