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
static int  button_widget_expose	(GtkWidget         *widget,
					 GdkEventExpose    *event);
static void button_widget_size_request  (GtkWidget         *widget,
					 GtkRequisition    *requisition);
static void button_widget_size_allocate (GtkWidget         *widget,
					 GtkAllocation     *allocation);
static int  button_widget_pressed	(ButtonWidget *button);
static void button_widget_unpressed	(ButtonWidget *button);

typedef int (*IntSignal) (GtkObject * object,
			  gpointer data);
typedef void (*VoidSignal) (GtkObject * object,
			    gpointer data);

/*list of all the button widgets*/
static GList *buttons=NULL;

/*the tiles go here*/
static GdkPixmap *tiles_up[MAX_TILES]={NULL,NULL,NULL,NULL};
static GdkBitmap *tiles_up_mask[MAX_TILES]={NULL,NULL,NULL,NULL};
static GdkPixmap *tiles_down[MAX_TILES]={NULL,NULL,NULL,NULL};
static GdkBitmap *tiles_down_mask[MAX_TILES]={NULL,NULL,NULL,NULL};

/*are tiles enabled*/
static int tiles_enabled = FALSE;

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
marshal_signal_int (GtkObject * object,
		    GtkSignalFunc func,
		    gpointer func_data,
		    GtkArg * args)
{
	IntSignal rfunc;
	int *retval;

	rfunc = (IntSignal) func;

	retval = GTK_RETLOC_BOOL(args[0]);

	*retval = (*rfunc) (object, func_data);
}

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
			       marshal_signal_int,
			       GTK_TYPE_BOOL,
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
	class->pressed = NULL; /*FIXME:button_widget_pressed;*/
	class->unpressed = button_widget_unpressed;

	widget_class->size_allocate = button_widget_size_allocate;
	widget_class->size_request = button_widget_size_request;
}


static int
button_widget_destroy(GtkWidget *w, gpointer data)
{
	ButtonWidget *button = BUTTON_WIDGET(w);

	if(button->pixmap)
		gdk_pixmap_unref(button->pixmap);
	button->pixmap = NULL;
	if(button->mask)
		gdk_bitmap_unref(button->mask);
	button->mask = NULL;

	buttons = g_list_remove(buttons,button);

	return FALSE;
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
	GdkPixmap *tile;
	GdkBitmap *tile_mask;
	GdkGCValues values;
	GdkPoint points[3];
	GdkGC *gc;
	
	if(!GTK_WIDGET_REALIZED(button))
		return;
	
	gc = gdk_gc_new(pixmap);

	if(button->arrow) {
		int i;
		draw_arrow(points,button->orient);
		for(i=0;i<3;i++) {
			points[i].x+=widget->allocation.x;
			points[i].y+=widget->allocation.y;
		}
	}
	
	if(tiles_enabled) {
		if(button->pressed) {
			tile = tiles_down[button->tile];
			tile_mask = tiles_down_mask[button->tile];
		} else {
			tile = tiles_up[button->tile];
			tile_mask = tiles_up_mask[button->tile];
		}
	}

	if(tiles_enabled) {
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

	if (button->mask) {
		gdk_gc_set_clip_mask (gc, button->mask);
		gdk_gc_set_clip_origin (gc, widget->allocation.x,
					widget->allocation.y);
	}

	gdk_draw_pixmap (pixmap, gc, button->pixmap, 0, 0,
			 widget->allocation.x, widget->allocation.y,
			 BIG_ICON_SIZE, BIG_ICON_SIZE);

	/*stripe a pressed button if we have no tiles, to provide some sort of
	  feedback*/
	if(!tiles_enabled && button->pressed) {
		int i;
		gdk_gc_set_foreground(gc,&widget->style->black);
		for(i=0;i<BIG_ICON_SIZE;i+=2)
			gdk_draw_line(pixmap,gc,
				      widget->allocation.x,widget->allocation.y+i,
				      widget->allocation.x+BIG_ICON_SIZE,widget->allocation.y+i);
	}

	if (button->mask) {
		gdk_gc_set_clip_mask (gc, NULL);
		gdk_gc_set_clip_origin (gc, 0, 0);
	}
	
	if(button->arrow) {
		gdk_gc_set_foreground(gc,&widget->style->white);
		gdk_draw_polygon(pixmap,gc,TRUE,points,3);
		gdk_gc_set_foreground(gc,&widget->style->black);
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
	widget->allocation = *allocation;
}

static void
button_widget_init (ButtonWidget *button)
{
	GTK_WIDGET_SET_FLAGS (button, GTK_NO_WINDOW);

	buttons = g_list_prepend(buttons,button);

	button->pixmap = NULL;
	button->mask = NULL;
	
	button->tile = 0;
	button->arrow = 0;
	button->orient = ORIENT_UP;
	
	button->pressed = FALSE;
	
	gtk_signal_connect(GTK_OBJECT(button),"destroy",
			   GTK_SIGNAL_FUNC(button_widget_destroy),
			   NULL);
}

void
button_widget_clicked(ButtonWidget *button)
{
	gtk_signal_emit(GTK_OBJECT(button),
			button_widget_signals[CLICKED_SIGNAL]);
}

static int
button_widget_pressed(ButtonWidget *button)
{
	button->pressed = TRUE;
	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);
	return FALSE;
}
static void
button_widget_unpressed(ButtonWidget *button)
{
	button->pressed = FALSE;
	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);
}

int
button_widget_down(ButtonWidget *button)
{
	int retval=FALSE;

	/*FIXME:*/
	button_widget_pressed(button);

	gtk_signal_emit(GTK_OBJECT(button),
			button_widget_signals[PRESSED_SIGNAL],
			&retval);
	printf ("retval: %d\n",retval);
	return retval;
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
		  int tile,
		  int arrow,
		  PanelOrientType orient)
{
	ButtonWidget *button;

	button = BUTTON_WIDGET (gtk_type_new (button_widget_get_type ()));
	
	button->pixmap = pixmap;
	button->mask = mask;
	button->tile = tile;
	button->arrow = arrow;
	button->orient = orient;
	
	return GTK_WIDGET(button);
}

static void
loadup_file(GdkPixmap **pixmap, GdkBitmap **mask, char *file)
{
	GdkImlibImage *im = NULL;

	if(*file!='/') {
		char *f;
		f = gnome_unconditional_pixmap_file (file);
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

	gdk_imlib_render (im, BIG_ICON_SIZE, BIG_ICON_SIZE);

	*pixmap = gdk_imlib_copy_image (im);
	*mask = gdk_imlib_copy_mask (im);

	gdk_imlib_destroy_image (im);
}

GtkWidget*
button_widget_new_from_file(char *pixmap,
			    int tile,
			    int arrow,
			    PanelOrientType orient)
{
	GdkPixmap *_pixmap;
	GdkBitmap *mask;
	
	loadup_file(&_pixmap,&mask,pixmap);
	if(!_pixmap)
		return NULL;
	
	return button_widget_new(_pixmap,mask,tile,arrow,orient);
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
	if(!_pixmap)
		return FALSE;
	
	button_widget_set_pixmap(button,_pixmap,mask);
	return TRUE;
}

void
button_widget_set_params(ButtonWidget *button,
			 int tile,
			 int arrow,
			 PanelOrientType orient)
{
	button->tile = tile;
	button->arrow = arrow;
	button->orient = orient;
	
	panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
			       button);
}

void
button_widget_load_tile(int tile, char *tile_up, char *tile_down)
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
	
	for(list = buttons;list!=NULL;list=g_list_next(list)) {
		ButtonWidget *button = list->data;
		if(button->tile == tile)
			panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(button)->parent),
					       button);
	}
}

void
button_widget_tile_enable(int enabled)
{
	if(tiles_enabled != enabled) {
		GList *list;
		tiles_enabled = enabled;

		for(list = buttons;list!=NULL;list=g_list_next(list))
			panel_widget_draw_icon(PANEL_WIDGET(GTK_WIDGET(list->data)->parent),
					       list->data);
	}
}
