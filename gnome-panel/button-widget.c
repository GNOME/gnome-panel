#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include "button-widget.h"
#include "panel-widget.h"

/*FIXME: make this global, and use it everywhere*/
#define SMALL_ICON_SIZE 20
#define BIG_ICON_SIZE   48

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
static GdkPixmap *tiles_up[LAST_TILE]={NULL,NULL,NULL,NULL};
static GdkBitmap *tiles_up_mask[LAST_TILE]={NULL,NULL,NULL,NULL};
static GdkPixmap *tiles_down[LAST_TILE]={NULL,NULL,NULL,NULL};
static GdkBitmap *tiles_down_mask[LAST_TILE]={NULL,NULL,NULL,NULL};
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

	if(button->pixmap)
		gdk_pixmap_unref(button->pixmap);
	button->pixmap = NULL;
	if(button->mask)
		gdk_bitmap_unref(button->mask);
	button->mask = NULL;

	g_free(button->text);

	buttons = g_list_remove(buttons,button);
}

static void
draw_arrow(GdkPoint *points, PanelOrientType orient)
{
	switch(orient) {
	case ORIENT_UP:
		points[0].x = BIG_ICON_SIZE-12;
		points[0].y = 10;
		points[1].x = BIG_ICON_SIZE-4;
		points[1].y = 10;
		points[2].x = BIG_ICON_SIZE-8;
		points[2].y = 3;
		break;
	case ORIENT_DOWN:
		points[0].x = 4;
		points[0].y = BIG_ICON_SIZE - 10;
		points[1].x = 12;
		points[1].y = BIG_ICON_SIZE - 10;
		points[2].x = 8;
		points[2].y = BIG_ICON_SIZE - 3;
		break;
	case ORIENT_LEFT:
		points[0].x = 10;
		points[0].y = 4;
		points[1].x = 10;
		points[1].y = 12;
		points[2].x = 3;
		points[2].y = 8;
		break;
	case ORIENT_RIGHT:
		points[0].x = BIG_ICON_SIZE - 10;
		points[0].y = BIG_ICON_SIZE - 12;
		points[1].x = BIG_ICON_SIZE - 10;
		points[1].y = BIG_ICON_SIZE - 4;
		points[2].x = BIG_ICON_SIZE - 3;
		points[2].y = BIG_ICON_SIZE - 8;
		break;
	}
}

void
button_widget_draw(ButtonWidget *button, GdkPixmap *pixmap)
{
	GtkWidget *widget = GTK_WIDGET(button);
	GtkWidget *pwidget;
	GdkPixmap *tile = NULL;
	GdkBitmap *tile_mask = NULL;
	GdkGC *gc;
	/*offset for pressed buttons*/
	int off = button->in_button&&button->pressed?tile_depth[button->tile]:0;
	/*border to not draw when drawing a tile*/
	int border = tiles_enabled[button->tile]?tile_border[button->tile]:0;
	int i;
	 
	if(!GTK_WIDGET_REALIZED(button))
		return;
	
	pwidget = widget->parent;
	
	gc = gdk_gc_new(pixmap);

	if(tiles_enabled[button->tile]) {
		if(button->pressed && button->in_button) {
			tile = tiles_down[button->tile];
			tile_mask = tiles_down_mask[button->tile];
		} else {
			tile = tiles_up[button->tile];
			tile_mask = tiles_up_mask[button->tile];
		}
	}

	if(tiles_enabled[button->tile] && tile) {
		if (tile_mask) {
			gdk_gc_set_clip_mask (gc, tile_mask);
			gdk_gc_set_clip_origin (gc, widget->allocation.x,
						widget->allocation.y);
		}
		gdk_draw_pixmap(pixmap, gc, tile, 0,0,
				widget->allocation.x, widget->allocation.y,
				BIG_ICON_SIZE, BIG_ICON_SIZE);
		if (tile_mask) {
			gdk_gc_set_clip_mask (gc, NULL);
			gdk_gc_set_clip_origin (gc, 0, 0);
		}
	}

	if (pixmaps_enabled[button->tile]) {
		if (button->mask) {
			gdk_gc_set_clip_mask (gc, button->mask);
			gdk_gc_set_clip_origin (gc,
						widget->allocation.x+off,
						widget->allocation.y+off);
		}


		i = MAX(border-off,0);
		if(button->pixmap) {
			gdk_draw_pixmap (pixmap, gc, button->pixmap,
					 i, i,
					 widget->allocation.x+i+off,
					 widget->allocation.y+i+off,
					 BIG_ICON_SIZE-i-off-border,
					 BIG_ICON_SIZE-i-off-border);
		}

		if (button->mask) {
			gdk_gc_set_clip_mask (gc, NULL);
			gdk_gc_set_clip_origin (gc, 0, 0);
		}
	}

	/*draw text*/
	if (!pixmaps_enabled[button->tile] ||
	  always_text[button->tile] ||
	  !button->pixmap) {
		char *text = g_strdup(button->text);
		int twidth,theight;
		GdkFont *font;
		GdkRectangle rect = {
			widget->allocation.x,
			widget->allocation.y,
			widget->allocation.width,
			widget->allocation.height
		};
		if(!text) text = g_strdup("XXX");
		
		font = gdk_font_load("-*-helvetica-medium-r-normal-*-8-*-*-*-*-*-*-*");
		if(!font)
			font = gdk_font_load("fixed");
		if(!font)
			font = widget->style->font;
		

		gdk_gc_set_clip_rectangle (gc, &rect);

		twidth = gdk_string_width(font,text);
		theight = gdk_string_height(font,text);
		if(twidth>BIG_ICON_SIZE)
			twidth = BIG_ICON_SIZE;
		
		gdk_gc_set_foreground(gc,&widget->style->black);
		gdk_draw_rectangle(pixmap,gc,TRUE,
				   widget->allocation.x+(widget->allocation.width/2)-(twidth/2)+off-1,
				   widget->allocation.y+(widget->allocation.height/2)-(theight/2)-1+off,
				   twidth+2,
				   theight+2);
		gdk_gc_set_foreground(gc,&widget->style->white);
		gdk_draw_string(pixmap,font,gc,
				widget->allocation.x+(widget->allocation.width/2)-(twidth/2)+off,
				widget->allocation.y+(widget->allocation.height/2)+(theight/2)+off,
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
		draw_arrow(points,button->orient);
		for(i=0;i<3;i++) {
			points[i].x+=widget->allocation.x+off;
			points[i].y+=widget->allocation.y+off;
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
	requisition->width = requisition->height = BIG_ICON_SIZE;
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

	button->pixmap = NULL;
	button->mask = NULL;
	
	button->tile = 0;
	button->arrow = 0;
	button->orient = ORIENT_UP;
	
	button->pressed = FALSE;
	button->in_button = FALSE;
	button->ignore_leave = FALSE;
	
	gtk_signal_connect(GTK_OBJECT(button),"destroy",
			   GTK_SIGNAL_FUNC(button_widget_destroy),
			   NULL);
}

static int
button_widget_button_press (GtkWidget *widget, GdkEventButton *event)
{
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (IS_BUTTON_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (event->button == 1) {
		ButtonWidget *button = BUTTON_WIDGET (widget);
		gtk_grab_add(widget);
		button_widget_down (button);
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
	gtk_signal_emit(GTK_OBJECT(button),
			button_widget_signals[PRESSED_SIGNAL]);
}
void
button_widget_up(ButtonWidget *button)
{
	gtk_signal_emit(GTK_OBJECT(button),
			button_widget_signals[UNPRESSED_SIGNAL]);
}


GtkWidget*
button_widget_new(GdkPixmap *pixmap,
		  GdkBitmap *mask,
		  guint tile,
		  guint arrow,
		  PanelOrientType orient,
		  char *text)
{
	ButtonWidget *button;

	button = BUTTON_WIDGET (gtk_type_new (button_widget_get_type ()));
	
	button->pixmap = pixmap;
	button->mask = mask;
	button->tile = tile;
	button->arrow = arrow;
	button->orient = orient;
	button->text = text?g_strdup(text):NULL;
	
	return GTK_WIDGET(button);
}

static void
loadup_file(GdkPixmap **pixmap, GdkBitmap **mask, char *file)
{
	GdkImlibImage *im = NULL;
	int w,h;
	
	if(!file) {
		*pixmap = NULL;
		*mask = NULL;
		return;
	}

	if(*file!='/') {
		char *f;
		f = gnome_pixmap_file (file);
		if(f) {
			im = gdk_imlib_load_image (f);
			g_free(f);
		}
	} else
		im = gdk_imlib_load_image (file);
	if(!im) {
		*pixmap = NULL;
		*mask = NULL;
		return;
	}

	w = im->rgb_width;
	h = im->rgb_height;
	if(w>h) {
		if(w>BIG_ICON_SIZE) {
			h = h*((double)BIG_ICON_SIZE/w);
			w = BIG_ICON_SIZE;
		}
	} else {
		if(h>BIG_ICON_SIZE) {
			w = w*((double)BIG_ICON_SIZE/h);
			h = BIG_ICON_SIZE;
		}
	}
	w = w>0?w:1;
	h = h>0?h:1;
	gdk_imlib_render (im, w, h);

	*pixmap = gdk_imlib_copy_image (im);
	*mask = gdk_imlib_copy_mask (im);

	gdk_imlib_destroy_image (im);
}

GtkWidget*
button_widget_new_from_file(char *pixmap,
			    guint tile,
			    guint arrow,
			    PanelOrientType orient,
			    char *text)
{
	GdkPixmap *_pixmap;
	GdkBitmap *mask;
	
	loadup_file(&_pixmap,&mask,pixmap);
	
	return button_widget_new(_pixmap,mask,tile,arrow,orient,text);
}

void
button_widget_set_pixmap(ButtonWidget *button, GdkPixmap *pixmap, GdkBitmap *mask)
{
	if(button->pixmap)
		gdk_pixmap_unref(button->pixmap);
	button->pixmap = NULL;
	if(button->mask)
		gdk_bitmap_unref(button->mask);
	button->mask = NULL;
	
	button->pixmap = pixmap;
	button->mask = mask;
	
	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);
}

int
button_widget_set_pixmap_from_file(ButtonWidget *button, char *pixmap)
{
	GdkPixmap *_pixmap;
	GdkBitmap *mask;
	
	loadup_file(&_pixmap,&mask,pixmap);
	button_widget_set_pixmap(button,_pixmap,mask);

	if(!_pixmap)
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

	if(tiles_up[tile])
		gdk_pixmap_unref(tiles_up[tile]);
	tiles_up[tile] = NULL;
	if(tiles_up_mask[tile])
		gdk_bitmap_unref(tiles_up_mask[tile]);
	tiles_up_mask[tile] = NULL;

	if(tiles_down[tile])
		gdk_pixmap_unref(tiles_down[tile]);
	tiles_down[tile] = NULL;
	if(tiles_down_mask[tile])
		gdk_bitmap_unref(tiles_down_mask[tile]);
	tiles_down_mask[tile] = NULL;

	loadup_file(&tiles_up[tile],&tiles_up_mask[tile],tile_up);
	loadup_file(&tiles_down[tile],&tiles_down_mask[tile],tile_down);
	
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
