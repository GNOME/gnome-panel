/* Gnome panel: panel widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
#include <limits.h>
#include <math.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include "panel-widget.h"
#include "panel-util.h"
#include "gdkextra.h"

GList *panels=NULL; /*other panels we might want to move the applet to*/

#define DEBUG 1

/*there  can universally be only one applet being dragged since we assume
we only have one mouse :) */
int panel_applet_in_drag = FALSE;

static void panel_widget_class_init	(PanelWidgetClass *klass);
static void panel_widget_init		(PanelWidget      *panel_widget);
static int  panel_try_to_set_pixmap     (PanelWidget *panel, char *pixmap);
static void panel_resize_pixmap         (PanelWidget *panel);
static void panel_try_to_set_back_color (PanelWidget *panel, GdkColor *color);

GdkCursor *fleur_cursor;

/*global settings*/
int pw_explicit_step = 50;
int pw_drawer_step = 20;
int pw_auto_step = 10;
int pw_minimized_size = 6;
int pw_minimize_delay = 300;
int pw_disable_animations = FALSE;
PanelMovementType pw_movement_type = PANEL_SWITCH_MOVE;

static char *image_drop_types[] = {"url:ALL", "application/x-color"};

/*this is a queue of panels we want to send applet_move to all it's applets,
  it's done in a queue and not immediately so that we don't do it too many
  times*/
static int send_move_idle_id = 0;
static GList *send_panels = NULL;

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

typedef void (*OrientSignal) (GtkObject * object,
			      PanelOrientation orient,
			      gpointer data);

typedef void (*BackSignal) (GtkObject * object,
			    PanelBackType type,
			    char *pixmap,
			    GdkColor *color,
			    gpointer data);

typedef void (*AppletSignal) (GtkObject * object,
			      GtkWidget * applet,
			      gpointer data);

typedef void (*VoidSignal) (GtkObject * object,
			    gpointer data);

/************************
 debugging
 ************************/
/*static void
debug_dump_panel_list(PanelWidget *panel)
{
	GList *list;
	puts("\nDUMP START\n");
	for(list = panel->applet_list;list!=NULL;list=g_list_next(list)) {
		AppletData *ad = list->data;
		printf("pos: %d cells: %d\n",ad->pos,ad->cells);
	}
	puts("\nDUMP END\n");
}*/

/************************
 convenience functions
 ************************/
static int
applet_data_compare(AppletData *ad1, AppletData *ad2)
{
	return ad1->pos - ad2->pos;
}

/************************
 widget core
 ************************/

guint
panel_widget_get_type ()
{
	static guint panel_widget_type = 0;

	if (!panel_widget_type) {
		GtkTypeInfo panel_widget_info = {
			"PanelWidget",
			sizeof (PanelWidget),
			sizeof (PanelWidgetClass),
			(GtkClassInitFunc) panel_widget_class_init,
			(GtkObjectInitFunc) panel_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};

		panel_widget_type = gtk_type_unique (gtk_event_box_get_type (), &panel_widget_info);
	}

	return panel_widget_type;
}

enum {
	ORIENT_CHANGE_SIGNAL,
	APPLET_MOVE_SIGNAL,
	APPLET_ADDED_SIGNAL,
	APPLET_REMOVED_SIGNAL,
	BACK_CHANGE_SIGNAL,
	LAST_SIGNAL
};

static int panel_widget_signals[LAST_SIGNAL] = {0,0,0,0,0};

static void
marshal_signal_orient (GtkObject * object,
		       GtkSignalFunc func,
		       gpointer func_data,
		       GtkArg * args)
{
	OrientSignal rfunc;

	rfunc = (OrientSignal) func;

	(*rfunc) (object, GTK_VALUE_ENUM (args[0]),
		  func_data);
}

static void
marshal_signal_applet (GtkObject * object,
		       GtkSignalFunc func,
		       gpointer func_data,
		       GtkArg * args)
{
	AppletSignal rfunc;

	rfunc = (AppletSignal) func;

	(*rfunc) (object, GTK_VALUE_POINTER (args[0]),
		  func_data);
}

static void
marshal_signal_back (GtkObject * object,
		     GtkSignalFunc func,
		     gpointer func_data,
		     GtkArg * args)
{
	BackSignal rfunc;

	rfunc = (BackSignal) func;

	(*rfunc) (object, GTK_VALUE_ENUM (args[0]),
		  GTK_VALUE_POINTER (args[1]),
		  GTK_VALUE_POINTER (args[2]),
		  func_data);
}


static void
panel_widget_class_init (PanelWidgetClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) class;

	panel_widget_signals[ORIENT_CHANGE_SIGNAL] =
		gtk_signal_new("orient_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 orient_change),
			       marshal_signal_orient,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_ENUM);
	panel_widget_signals[APPLET_MOVE_SIGNAL] =
		gtk_signal_new("applet_move",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 applet_move),
			       marshal_signal_applet,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_POINTER);
	panel_widget_signals[APPLET_ADDED_SIGNAL] =
		gtk_signal_new("applet_added",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 applet_added),
			       marshal_signal_applet,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_POINTER);
	panel_widget_signals[APPLET_REMOVED_SIGNAL] =
		gtk_signal_new("applet_removed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 applet_removed),
			       marshal_signal_applet,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_POINTER);
	panel_widget_signals[BACK_CHANGE_SIGNAL] =
		gtk_signal_new("back_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 back_change),
			       marshal_signal_back,
			       GTK_TYPE_NONE,
			       3,
			       GTK_TYPE_ENUM,
			       GTK_TYPE_POINTER,
			       GTK_TYPE_POINTER);
	gtk_object_class_add_signals(object_class,panel_widget_signals,
				     LAST_SIGNAL);

	class->orient_change = NULL;
	class->applet_move = NULL;
	class->applet_added = NULL;
	class->applet_removed = NULL;
	class->back_change = NULL;
}

void
panel_widget_applet_put(PanelWidget *panel,AppletData *ad, int force)
{
	int width, height;
	int x,y;
	int oldx,oldy;

	g_return_if_fail(ad->applet!=NULL);

	gdk_window_get_geometry(ad->applet->window,&oldx,&oldy,	
				&width,&height,NULL);

	if(panel->orient==PANEL_HORIZONTAL) {
		x = (PANEL_CELL_SIZE*ad->pos) +
		    ((PANEL_CELL_SIZE*ad->cells)/2) -
		    (width/2);
		y = (panel->thick/2) - (height/2);
	} else { /* panel->orient==PANEL_VERTICAL */
		x = (panel->thick/2) - (width/2);
		y = (PANEL_CELL_SIZE*ad->pos) +
		    ((PANEL_CELL_SIZE*ad->cells)/2) -
		    (height/2);
	}

	if(oldx!=x || oldy!=y || force)
		gtk_fixed_move(GTK_FIXED(panel->fixed),ad->applet,x,y);
}

void
panel_widget_put_all(PanelWidget *panel,int force)
{
	GList *list;

	for(list=panel->applet_list;list!=NULL;list=g_list_next(list))
		panel_widget_applet_put(panel,list->data,force);
}

/*get the number of applets*/
int
panel_widget_get_applet_count(PanelWidget *panel)
{
	int i;
	GList *list;

	for(i=0,list=panel->applet_list;list!=NULL;list=g_list_next(list),i++)
		;
	return i;
}

/*get the list item of the data on the position pos*/
static GList *
get_applet_list_pos(PanelWidget *panel, int pos)
{
	GList *list;
	
	for(list=panel->applet_list;list!=NULL;list=g_list_next(list)) {
		AppletData *ad = list->data;
		if(ad->pos <= pos) {
		       if(ad->pos+ad->cells > pos)
			       return list;
		} else
			return NULL;
	}
	return NULL;
}

AppletData *
get_applet_data_pos(PanelWidget *panel, int pos)
{
	GList *list = get_applet_list_pos(panel,pos);
	if(list)
		return list->data;
	return NULL;
}

void
panel_widget_pack_applets(PanelWidget *panel)
{
	int i;
	GList *list;

	for(list=panel->applet_list,i=0;list!=NULL;list=g_list_next(list)) {
		AppletData *ad = list->data;
		ad->pos = i;
		i+=ad->cells;
	}

	panel->size = i;
	panel_widget_put_all(panel,FALSE);
}


/*tells us if an applet is "stuck" on the right side*/
int
panel_widget_is_applet_stuck(PanelWidget *panel, GtkWidget *applet)
{
	AppletData *ad;
	GList *list;
	int i;

	g_return_val_if_fail(panel!=NULL,FALSE);
	g_return_val_if_fail(applet!=NULL,FALSE);

	ad = gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_DATA);

	g_return_val_if_fail(ad!=NULL,FALSE);

	list = g_list_find(panel->applet_list,ad);

	g_return_val_if_fail(list!=NULL,FALSE);
	
	do {
		i=ad->pos+ad->cells;
		if(i==panel->size)
			return TRUE;
		list = g_list_next(list);
		if(!list)
			break;
		ad = list->data;
	} while(ad->pos==i);
	return FALSE;
}

static void
panel_widget_switch_applet_right(PanelWidget *panel, GList *list)
{
	AppletData *ad = list->data;
	AppletData *nad;
	GList *nlist;

	nlist = g_list_next(list);
	if(nlist)
		nad = nlist->data;
	if(!nlist || nad->pos > ad->pos+ad->cells) {
		ad->pos++;
		panel_widget_applet_put(panel,ad,FALSE);
		return;
	}
	nad->pos = ad->pos;
	ad->pos = nad->pos+nad->cells;
	panel->applet_list = my_g_list_swap_prev(panel->applet_list,nlist);

	panel_widget_applet_put(panel,ad,FALSE);
	panel_widget_applet_put(panel,nad,FALSE);
}

static void
panel_widget_switch_applet_left(PanelWidget *panel, GList *list)
{
	AppletData *ad = list->data;
	AppletData *pad;
	GList *nlist;

	nlist = g_list_previous(list);
	if(nlist)
		pad = nlist->data;
	if(!nlist || pad->pos+pad->cells < ad->pos) {
		ad->pos--;
		panel_widget_applet_put(panel,ad,FALSE);
		return;
	}
	ad->pos = pad->pos;
	pad->pos = ad->pos+ad->cells;
	panel->applet_list = my_g_list_swap_next(panel->applet_list,nlist);

	panel_widget_applet_put(panel,ad,FALSE);
	panel_widget_applet_put(panel,pad,FALSE);
}

static int
panel_widget_get_right_switch_pos(PanelWidget *panel, GList *list)
{
	AppletData *ad = list->data;
	AppletData *nad;
	list = g_list_next(list);
	if(list)
		nad = list->data;
	if(!list || nad->pos > ad->pos+ad->cells)
		return ad->pos+1;
	return nad->pos+nad->cells-ad->cells;
}

static int
panel_widget_get_left_switch_pos(PanelWidget *panel, GList *list)
{
	AppletData *ad = list->data;
	AppletData *pad;
	list = g_list_previous(list);
	if(list)
		pad = list->data;
	if(!list || pad->pos+pad->cells < ad->pos)
		return ad->pos-1;
	return pad->pos;
}


static void
panel_widget_switch_move(PanelWidget *panel, AppletData *ad, int moveby)
{
	int finalpos;
	int pos;
	GList *list;

	g_return_if_fail(ad!=NULL);
	g_return_if_fail(panel!=NULL);

	if(moveby==0)
		return;

	list = g_list_find(panel->applet_list,ad);
	g_return_if_fail(list!=NULL);

	finalpos = ad->pos+moveby;

	if(finalpos >= panel->size)
		finalpos = panel->size-1;
	else if(finalpos < 0)
		finalpos = 0;

	while((ad->pos+ad->cells-1)<finalpos) {
		pos = panel_widget_get_right_switch_pos(panel,list);
		if(pos > finalpos || pos+ad->cells-1 >= panel->size)
			return;
		panel_widget_switch_applet_right(panel,list);
	}
	while(ad->pos>finalpos) {
		if((panel_widget_get_left_switch_pos(panel,list)+ad->cells-1) < 
		   finalpos)
			return;
		panel_widget_switch_applet_left(panel,list);
	}

	return;
}

static int
panel_widget_get_thick(PanelWidget *panel)
{
	GList *list;
	int thick=0;

	g_return_val_if_fail(panel,PANEL_MINIMUM_WIDTH);

	if(panel->orient==PANEL_HORIZONTAL) {
		for(list=panel->applet_list;list!=NULL;list=g_list_next(list)) {
			AppletData *ad = list->data;
			int height = ad->applet->allocation.height;
			if(height > thick)
				thick = height;
		}
	} else { /* panel->orient==PANEL_VERTICAL */
		for(list=panel->applet_list;list!=NULL;list=g_list_next(list)) {
			AppletData *ad = list->data;
			int width = ad->applet->allocation.width;
			if(width > thick)
				thick = width;
		}
	}
	return thick;
}

static void
panel_widget_shrink_wrap(PanelWidget *panel,
			 int width,
			 AppletData *ad)
{
	g_return_if_fail(ad!=NULL);

	if(width%PANEL_CELL_SIZE) width--; /*just so that I get
					     the right size*/
	/*convert width from pixels to cells*/
	width = (width/PANEL_CELL_SIZE) + 1;

	if(width >= ad->cells)
		return;

	ad->cells = width;

	if(panel->packed) {
		panel_widget_pack_applets(panel);
	}
}

/*this is a special function and may fail if called improperly, it works
only under special circumstance when we know there is nothing from
old_size to panel->size*/
static void
panel_widget_right_stick(PanelWidget *panel,int old_size)
{
	int i,pos;
	GList *list,*prev;
	AppletData *ad;

	if(old_size>=panel->size ||
	   panel->packed)
	   	return;
	
	list = get_applet_list_pos(panel,old_size-1);

	if(!list)
		return;
	
	pos = panel->size-1;

	ad = list->data;
	do { 
		i = ad->pos;
		ad->pos = pos--;
		ad->cells = 1;
		prev = list;
		list = g_list_previous(list);
		if(!list)
			break;
		ad = list->data;
	} while(ad->pos+ad->cells == i);

	for(list = prev;list!=NULL;list=g_list_next(list))
		panel_widget_applet_put(panel,list->data,FALSE);
}

static int
panel_widget_push_left(PanelWidget *panel,AppletData *oad)
{
	int i;
	GList *list,*prev;
	AppletData *ad;

	if(!panel->applet_list)
		return FALSE;

	g_return_val_if_fail(panel->packed == FALSE,FALSE);

	list = g_list_find(panel->applet_list,oad);

	g_return_val_if_fail(list!=NULL,FALSE);

	ad = oad;

	do { 
		i = ad->pos;
		prev = list;
		list = g_list_previous(list);
		if(!list)
			break;
		ad = list->data;
	} while(ad->pos+ad->cells == i);

	ad=prev->data;
	if(ad->pos<=0)
		return FALSE;
	for(list = prev;list!=NULL;list=g_list_next(list)) {
		ad=list->data;
		ad->pos--;

		panel_widget_applet_put(panel,ad,FALSE);
		if(ad == oad)
			break;
	}

	return TRUE;
}

static int
panel_widget_push_right(PanelWidget *panel,AppletData *oad)
{
	int i;
	GList *list,*prev;
	AppletData *ad;

	if(!panel->applet_list) {
		if(panel->packed) {
			panel->size++;
			return TRUE;
		}
		return FALSE;
	}

	list = g_list_find(panel->applet_list,oad);

	g_return_val_if_fail(list!=NULL,FALSE);

	ad = oad;

	do { 
		i = ad->pos+ad->cells;
		prev = list;
		list = g_list_next(list);
		if(!list)
			break;
		ad = list->data;
	} while(ad->pos == i);

	ad=prev->data;
	if(ad->pos+ad->cells>=panel->size) {
		if(panel->packed) {
			panel->size++;
		} else
			return FALSE;
	}
	for(list=prev;list!=NULL;list=g_list_previous(list)) {
		ad=list->data;
		ad->pos++;

		panel_widget_applet_put(panel,ad,FALSE);
		if(ad == oad)
			break;
	}

	return TRUE;
}

static void
panel_widget_seize_space(PanelWidget *panel,
			 int width,
			 AppletData *ad)
{
	GList *list,*rlist,*llist;

	if(width%PANEL_CELL_SIZE) width--; /*just so that I get
					     the right size*/
	/*convert width from pixels to cells*/
	width = (width/PANEL_CELL_SIZE) + 1;

	if(panel->packed)
		panel_widget_pack_applets(panel);

	/*we already have enough space*/
	if(width <= ad->cells)
		return;

	list = g_list_find(panel->applet_list,ad);
	g_return_if_fail(list!=NULL);

	/*try how much free space is on the right*/
	rlist = g_list_next(list);
	if(rlist) {
		AppletData *wad = rlist->data;
		if(ad->pos+width-1 < wad->pos) {
			ad->cells = width;
			return;
		}
		ad->cells = wad->pos - ad->pos;
	} else {
		if(panel->packed ||
		   ad->pos+width-1 < panel->size) {
			ad->cells = width;
			if(ad->pos+width>panel->size) {
				panel->size = ad->pos+width;
			}
			return;
		}
		ad->cells = panel->size - ad->pos;
	}

	/*try how much free space is on the left*/
	llist = g_list_previous(list);
	if(llist) {
		AppletData *wad = llist->data;
		if(wad->pos+wad->cells-1 < ad->pos+ad->cells-width) {
			ad->pos = ad->pos+ad->cells-width;
			ad->cells = width;
			panel_widget_applet_put(panel,ad,FALSE);
			return;
		}
		ad->cells += ad->pos-(wad->pos+wad->cells);
		ad->pos = wad->pos+wad->cells;
	} else {
		if(0 <= ad->pos+ad->cells-width) {
			ad->pos = ad->pos+ad->cells-width;
			ad->cells = width;
			panel_widget_applet_put(panel,ad,FALSE);
			return;
		}
		ad->cells += ad->pos;
		ad->pos = 0;
	}

	if(rlist) {
		AppletData *wad = rlist->data;
		while(ad->cells<width && panel_widget_push_right(panel,wad)) {
			ad->cells++;
		}
	}

	if(llist && !panel->packed) {
		AppletData *wad = llist->data;
		while(ad->cells<width && panel_widget_push_left(panel,wad)) {
			ad->cells++;
			ad->pos--;
		}
	}

	panel_widget_applet_put(panel,ad,FALSE);
}

static void
panel_widget_apply_size_limit(PanelWidget *panel)
{
	int length;
	int old_size;
	GList *list;

	g_return_if_fail(panel);

	old_size = panel->size;
	
	if(panel->orient == PANEL_HORIZONTAL) {
		length = panel->fixed->allocation.width;
		/*just so that I get size*/
		if(length%PANEL_CELL_SIZE) length--;
		panel->size = length/PANEL_CELL_SIZE;
	} else {
		length = panel->fixed->allocation.height;
		/*just so that I get size*/
		if(length%PANEL_CELL_SIZE) length--;
		panel->size = length/PANEL_CELL_SIZE;
	}

	if(old_size < panel->size)
		panel_widget_right_stick(panel,old_size);
	else if(old_size > panel->size)
		for(list=panel->applet_list;
		    list!=NULL;
		    list=g_list_next(list)) {
			AppletData *ad = list->data;
			if(ad->pos+ad->cells > panel->size)
				panel_widget_move(panel, ad->applet,
						  panel->size-1);
		}
}


static void
panel_widget_adjust_applet(PanelWidget *panel, AppletData *ad)
{
	int width, height;

	if(panel->postpone_adjust)
		return;
	
	g_return_if_fail(ad!=NULL);
	
	gdk_window_get_size(ad->applet->window,&width,&height);

	/*don't adjust applets out of range, wait for
	  then to be pushed into range*/
	if(panel->packed && ad->pos>=panel->size)
		return;

	if(panel->orient==PANEL_HORIZONTAL) {
		/*if smaller then it's allocation, we are OK*/
		if(width<=(PANEL_CELL_SIZE*ad->cells))
			panel_widget_shrink_wrap(panel,width,ad);
		else
			panel_widget_seize_space(panel,width,ad);
	} else { /* panel->orient==PANEL_VERTICAL */
		/*if smaller then it's allocation, we are OK*/
		if(height<=(PANEL_CELL_SIZE*ad->cells))
			panel_widget_shrink_wrap(panel,height,ad);
		else
			panel_widget_seize_space(panel,height,ad);
	}
	panel_widget_applet_put(panel,ad,FALSE);
}

static void
send_applet_move(PanelWidget *panel, AppletData *ad)
{
	int x,y,w,h;
	if(!ad->applet->window)
		return;
	gdk_window_get_origin(ad->applet->window,&x,&y);
	gdk_window_get_size(ad->applet->window,&w,&h);
	
	if(ad->prevwidth!=w ||
	   ad->prevheight!=h) {
		int thick;

		ad->prevwidth = w;
		ad->prevheight = h;
		thick = panel_widget_get_thick(panel);
		if(panel->thick != thick) {
			panel->thick = thick;
		}
		panel_widget_adjust_applet(panel,ad);
	}
	if(ad->prevx!=x ||
	   ad->prevy!=y) {
		ad->prevx = x;
		ad->prevy = y;
		gtk_signal_emit(GTK_OBJECT(panel),
				panel_widget_signals[APPLET_MOVE_SIGNAL],
				ad->applet);
	}
}


static int
panel_widget_applet_size_allocate (GtkWidget *widget,
				   GtkAllocation *allocation,
				   gpointer data)
{
	PanelWidget *panel;
	AppletData *ad;
	
	panel = gtk_object_get_data(GTK_OBJECT(widget),PANEL_APPLET_PARENT_KEY);
	ad = gtk_object_get_data(GTK_OBJECT(widget),PANEL_APPLET_DATA);

	if(ad->pos == -1)
		return FALSE;

	send_applet_move(panel,ad);

	return FALSE;
}

int
panel_widget_is_cursor(PanelWidget *panel, int overlap)
{
	int x,y;
	int w,h;
	GtkWidget *widget = panel->drop_widget;
	
	if(!w || !GTK_WIDGET_VISIBLE(widget))
		return FALSE;

	gtk_widget_get_pointer(widget, &x, &y);
	gdk_window_get_size(widget->window, &w, &h);

	if((x+overlap)>=0 &&
	   (x-overlap)<=w &&
	   (y+overlap)>=0 &&
	   (y-overlap)<=h)
		return TRUE;
	return FALSE;
}
static int
adjust_applets_idle(gpointer data)
{
	PanelWidget *panel = data;
	GList *list;
	panel->postpone_adjust = FALSE;
	for(list = panel->applet_list;
	    list != NULL;
	    list = g_list_next(list))
		panel_widget_adjust_applet(panel, list->data);
	panel->adjust_applet_idle = 0;
	return FALSE;
}

static int
send_move_idle(gpointer data)
{
	while(send_panels) {
		GList *list;
		PanelWidget *panel = send_panels->data;

		for(list = panel->applet_list;
		    list != NULL;
		    list = g_list_next(list)) {
			AppletData *ad = list->data;
			send_applet_move(panel,ad);
		}
		send_panels = g_list_remove_link(send_panels,send_panels);
	}
	
	send_move_idle_id = 0;
	return FALSE;
}

/*slightly hackish, the widget that owns the widget will call this
  function when it's window changes position or size ...*/
void
panel_widget_send_move(PanelWidget *panel)
{
	int x,y,w,h;
	if(!GTK_WIDGET_REALIZED(GTK_WIDGET(panel)))
		return;
	
	gdk_window_get_origin(GTK_WIDGET(panel)->window,&x,&y);
	gdk_window_get_size(GTK_WIDGET(panel)->window,&w,&h);
	
	if(x != panel->last_x ||
	   y != panel->last_y ||
	   w != panel->last_w ||
	   h != panel->last_h) {
		if(g_list_find(send_panels,panel)==NULL)
			send_panels = g_list_prepend(send_panels,panel);
		if(!send_move_idle_id)
			send_move_idle_id = gtk_idle_add(send_move_idle,NULL);
	}
	panel->last_x = x;
	panel->last_y = y;
	panel->last_w = w;
	panel->last_h = h;
}

static int
panel_widget_fixed_size_allocate(GtkWidget *widget,
				 GtkAllocation *allocation,
			         gpointer data)
{
	PanelWidget *panel = data;
	
	if(!GTK_WIDGET_REALIZED(widget))
		return FALSE;

	if(!panel->packed)
		panel_widget_apply_size_limit(panel);
	
	/*adjust all applets make it run at idle time, so that
	  we know that all events have been processed so this
	  would be the last fixed size_allocate for a while*/
	if(!panel->adjust_applet_idle)
		panel->adjust_applet_idle =
			gtk_idle_add_priority(GTK_PRIORITY_LOW,
					      adjust_applets_idle, panel);

	if(panel->fit_pixmap_bg && panel->back_type == PANEL_BACK_PIXMAP)
		panel_resize_pixmap(panel);
	
	panel_widget_send_move(panel);
	
	return FALSE;
}

/*FIXME: we need to somehow get the size_allocates of the toplevel window*/
static int
panel_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation,
			   gpointer data)
{
	/*PanelWidget *panel = PANEL_WIDGET(widget);
	if(!panel->packed)
		panel_widget_apply_size_limit(panel);*/
	/*GList *list;

	if(!GTK_WIDGET_REALIZED(widget))
		return FALSE;

	for(list = panel->applet_list;list != NULL;list = g_list_next(list)) {
		AppletData *ad = list->data;
		gtk_signal_emit(GTK_OBJECT(panel),
				panel_widget_signals[APPLET_MOVE_SIGNAL],
				ad->applet);
	}*/

	return FALSE;
}

static void
panel_widget_dnd_dropped_filename (GtkWidget *widget,
				   GdkEventDropDataAvailable *event,
				   PanelWidget *panel)
{
	if (panel_try_to_set_pixmap (panel, event->data)) {
		if (panel->back_pixmap)
			g_free (panel->back_pixmap);
		panel->back_pixmap = g_strdup (event->data);
		gtk_widget_queue_draw (widget);
		panel->back_type = PANEL_BACK_PIXMAP;
		gtk_signal_emit(GTK_OBJECT(panel),
				panel_widget_signals[BACK_CHANGE_SIGNAL],
				panel->back_type,
				panel->back_pixmap,
				&panel->back_color);
	}
}

static void
panel_try_to_set_default_back(PanelWidget *panel)
{
	GtkStyle *ns;

	ns = gtk_style_new();

	gtk_style_ref(ns);
	gtk_widget_set_style(panel->fixed, ns);
	gtk_style_unref(ns);

	gtk_widget_queue_draw(GTK_WIDGET(panel));
}

static void
panel_try_to_set_back_color(PanelWidget *panel, GdkColor *color)
{
	GtkStyle *ns;

	ns = gtk_style_copy(panel->fixed->style);
	gtk_style_ref(ns);

	ns->bg[GTK_STATE_NORMAL] = panel->back_color = *color;
	ns->bg[GTK_STATE_NORMAL].pixel =
		panel->back_color.pixel = 1; /* bogus */

	if(ns->bg_pixmap[GTK_STATE_NORMAL]) {
		gdk_imlib_free_pixmap(ns->bg_pixmap[GTK_STATE_NORMAL]);
		ns->bg_pixmap[GTK_STATE_NORMAL] = NULL;
	}

	gtk_widget_set_style(panel->fixed, ns);

	gtk_style_unref(ns);

	gtk_widget_queue_draw(GTK_WIDGET(panel));
}

static void
panel_widget_dnd_dropped_color (GtkWidget *widget,
				GdkEventDropDataAvailable *event,
				PanelWidget *panel)
{
	gdouble *dropped = (gdouble *)event->data;
	GdkColor c;

	c.red = (dropped[1]*65535);
	c.green = (dropped[2]*65535);
	c.blue = (dropped[3]*65535);
	c.pixel = 0;

	panel_try_to_set_back_color(panel, &c);
}

static void
panel_widget_dnd_drop_internal(GtkWidget *widget,
			       GdkEventDropDataAvailable *event,
			       gpointer data)
{
	PanelWidget *panel = data;
	/* Test for the type that was dropped */
	if (strcmp (event->data_type, "url:ALL") == 0) {
		panel_widget_dnd_dropped_filename (widget, event, panel);
	} else if(!strcmp(event->data_type, "application/x-color")) {
		panel_widget_dnd_dropped_color(widget, event, panel);
		panel->back_type = PANEL_BACK_COLOR;
		gtk_signal_emit(GTK_OBJECT(panel),
				panel_widget_signals[BACK_CHANGE_SIGNAL],
				panel->back_type,
				panel->back_pixmap,
				&panel->back_color);
	}
	return;
}

static void
panel_resize_pixmap(PanelWidget *panel)
{
	GdkImlibImage *im;
	GdkPixmap *p;
	GtkStyle *ns;
	int w, h;

	im = gtk_object_get_data(GTK_OBJECT(panel),"gdk_image");
	if(!im) return;

	w = im->rgb_width;
	h = im->rgb_height;

	switch (panel->orient) {
	case PANEL_HORIZONTAL:
		gdk_imlib_render (im, w * panel->thick / h, panel->thick);
		break;

	case PANEL_VERTICAL:
		gdk_imlib_render (im, panel->thick, h * panel->thick / w);
		break;

	default:
		g_assert_not_reached ();
	}

	p = gdk_imlib_move_image (im);

	ns = gtk_style_copy(panel->fixed->style);
	gtk_style_ref(ns);

	if(ns->bg_pixmap[GTK_STATE_NORMAL])
		gdk_imlib_free_pixmap (ns->bg_pixmap[GTK_STATE_NORMAL]);
	ns->bg_pixmap[GTK_STATE_NORMAL] = p;

	gtk_widget_set_style(panel->fixed, ns);
	
	gtk_style_unref(ns);
}

static int
panel_try_to_set_pixmap (PanelWidget *panel, char *pixmap)
{
	GdkImlibImage *im;
	GdkImlibImage *im2;
	GdkPixmap *p;
	GtkStyle *ns;

	if(!pixmap || strcmp(pixmap,"")==0) {
		ns = gtk_style_copy(panel->fixed->style);
		gtk_style_ref(ns);

		p = ns->bg_pixmap[GTK_STATE_NORMAL];
		if(p)
			gdk_imlib_free_pixmap (p);
		ns->bg_pixmap[GTK_STATE_NORMAL] = NULL;

		gtk_widget_set_style(panel->fixed, ns);
	
		gtk_style_unref(ns);
		return 1;
	}

	if (!g_file_exists (pixmap))
		return 0;
	
	im = gdk_imlib_load_image (pixmap);
	if (!im)
		return 0;

	im2 = gtk_object_get_data(GTK_OBJECT(panel),"gdk_image");
	if(im2) gdk_imlib_destroy_image (im2);
	
	if (panel->fit_pixmap_bg) {
		int w, h;

		w = im->rgb_width;
		h = im->rgb_height;

		switch (panel->orient) {
		case PANEL_HORIZONTAL:
			gdk_imlib_render (im, w * panel->thick / h, panel->thick);
			break;

		case PANEL_VERTICAL:
			gdk_imlib_render (im, panel->thick, h * panel->thick / w);
			break;

		default:
			g_assert_not_reached ();
		}
	} else
		gdk_imlib_render (im, im->rgb_width, im->rgb_height);

	p = gdk_imlib_move_image (im);

	ns = gtk_style_copy(panel->fixed->style);
	gtk_style_ref(ns);

	if(ns->bg_pixmap[GTK_STATE_NORMAL])
		gdk_imlib_free_pixmap (ns->bg_pixmap[GTK_STATE_NORMAL]);
	ns->bg_pixmap[GTK_STATE_NORMAL] = p;

	gtk_widget_set_style(panel->fixed, ns);
	
	gtk_style_unref(ns);
	gtk_object_set_data(GTK_OBJECT(panel),"gdk_image",im);
	return 1;
}

static int
panel_widget_realize(GtkWidget *w, gpointer data)
{
	PanelWidget *panel = PANEL_WIDGET(w);

	if(panel->back_type == PANEL_BACK_PIXMAP) {
		if (!panel_try_to_set_pixmap (panel, panel->back_pixmap))
			panel->back_type = PANEL_BACK_NONE;
	} else if(panel->back_type == PANEL_BACK_COLOR) {
		panel_try_to_set_back_color(panel, &panel->back_color);
	}

	gtk_widget_dnd_drop_set (GTK_WIDGET(panel->fixed), TRUE,
				 image_drop_types,
				 sizeof(image_drop_types)/sizeof(char *),
				 FALSE);
	

	return FALSE;
}

static int
panel_widget_destroy(GtkWidget *w, gpointer data)
{
	GdkImlibImage *im;

	im = gtk_object_get_data(GTK_OBJECT(w),"gdk_image");
	if(im) gdk_imlib_destroy_image (im);
	gtk_object_set_data(GTK_OBJECT(w),"gdk_image",NULL);
	
	/*remove from panels list*/
	panels = g_list_remove(panels,w);
	send_panels = g_list_remove(send_panels,w);
	
	return FALSE;
}

static void
panel_widget_init (PanelWidget *panel)
{
	/*this makes the popup "pop down" once the button is released*/
	gtk_widget_set_events(GTK_WIDGET(panel),
			      gtk_widget_get_events(GTK_WIDGET(panel)) |
			      GDK_BUTTON_RELEASE_MASK);

	panel->frame = gtk_frame_new(NULL);
	gtk_widget_show(panel->frame);
	gtk_frame_set_shadow_type(GTK_FRAME(panel->frame),GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(panel),panel->frame);

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());
	panel->fixed = gtk_fixed_new();
	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();
	gtk_container_add(GTK_CONTAINER(panel->frame),panel->fixed);
	gtk_widget_show(panel->fixed);
	/*gtk_widget_realize (panel->fixed);*/
	
	panel->back_type =PANEL_BACK_NONE;
	panel->fit_pixmap_bg = FALSE;
	panel->back_pixmap = NULL;
	panel->back_color.red = 0;
	panel->back_color.green = 0;
	panel->back_color.blue = 0;
	panel->back_color.pixel = 1;
	panel->postpone_adjust = FALSE;
	panel->adjust_applet_idle = 0;
	panel->packed = FALSE;
	panel->orient = PANEL_HORIZONTAL;
	panel->thick = PANEL_MINIMUM_WIDTH;
	panel->size = INT_MAX;
	panel->applet_list = NULL;
	panel->master_widget = NULL;
	panel->drop_widget = GTK_WIDGET(panel);
	panel->last_x = panel->last_y = panel->last_w = panel->last_h = -1;

	if(!fleur_cursor)
		fleur_cursor = gdk_cursor_new(GDK_FLEUR);

	gtk_signal_connect_after(GTK_OBJECT(panel->fixed),
				 "size_allocate",
				 GTK_SIGNAL_FUNC(
				 	panel_widget_fixed_size_allocate),
				 panel);
	gtk_signal_connect_after(GTK_OBJECT(panel),
				 "size_allocate",
				 GTK_SIGNAL_FUNC(panel_widget_size_allocate),
				 panel);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "destroy",
			   GTK_SIGNAL_FUNC(panel_widget_destroy),
			   NULL);

	/* Ok, cool hack begins: drop image files on the panel */
	gtk_signal_connect (GTK_OBJECT (panel->fixed),
			    "drop_data_available_event",
			    GTK_SIGNAL_FUNC (panel_widget_dnd_drop_internal),
			    panel);
	
	panels = g_list_append(panels,panel);
}

GtkWidget *
panel_widget_new (int packed,
		  PanelOrientation orient,
		  PanelBackType back_type,
		  char *back_pixmap,
		  int fit_pixmap_bg,
		  GdkColor *back_color)
{
	PanelWidget *panel;

	panel = gtk_type_new(panel_widget_get_type());

	panel->back_type = back_type;

	panel->fit_pixmap_bg = fit_pixmap_bg;
	if(back_pixmap)
		panel->back_pixmap = g_strdup(back_pixmap);
	else
		panel->back_pixmap = NULL;
	
	if(back_color)
		panel->back_color = *back_color;
	else {
		panel->back_color.red = 0;
		panel->back_color.green = 0;
		panel->back_color.blue = 0;
		panel->back_color.pixel = 1;
	}	

	panel->orient = orient;

	panel->packed = packed;
	if(packed)
		panel->size = 0;
	else
		panel->size = INT_MAX;
	
	if(GTK_WIDGET_REALIZED(GTK_WIDGET(panel))) {
		if(panel->back_type == PANEL_BACK_PIXMAP) {
			if (!panel_try_to_set_pixmap (panel, back_pixmap))
				panel->back_type = PANEL_BACK_NONE;
		} else if(panel->back_type == PANEL_BACK_COLOR) {
			panel_try_to_set_back_color(panel, &panel->back_color);
		}
	} else
		gtk_signal_connect_after(GTK_OBJECT(panel),
					 "realize",
					 GTK_SIGNAL_FUNC(panel_widget_realize),
					 panel);
	return GTK_WIDGET(panel);
}

static void
_panel_widget_applet_drag_start_no_grab(PanelWidget *panel, GtkWidget *applet)
{
#ifdef DEBUG
  g_message("Starting drag on a %s at %p\n",
	    gtk_type_name(GTK_OBJECT(applet)->klass->type), applet);
#endif
	panel->currently_dragged_applet =
		gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_DATA);
}


void
panel_widget_applet_drag_start_no_grab(PanelWidget *panel, GtkWidget *applet)
{
	panel_applet_in_drag = TRUE;
	_panel_widget_applet_drag_start_no_grab(panel,applet);
}

void
_panel_widget_applet_drag_end_no_grab(PanelWidget *panel)
{
#ifdef DEBUG
  g_message("Ending drag\n");
#endif
	panel->currently_dragged_applet = NULL;
}

void
panel_widget_applet_drag_end_no_grab(PanelWidget *panel)
{
	_panel_widget_applet_drag_end_no_grab(panel);
	panel_applet_in_drag = FALSE;
}

static void
_panel_widget_applet_drag_start(PanelWidget *panel, GtkWidget *applet)
{
#ifdef DEBUG
  g_message("Starting drag [grabbed] on a %s at %p\n",
	    gtk_type_name(GTK_OBJECT(applet)->klass->type), applet);
#endif
	_panel_widget_applet_drag_start_no_grab(panel,applet);

	gtk_grab_add(applet);
	gdk_pointer_grab(applet->window,
			 FALSE,
			 APPLET_EVENT_MASK,
			 NULL,
			 fleur_cursor,
			 GDK_CURRENT_TIME);
}

void
panel_widget_applet_drag_start(PanelWidget *panel, GtkWidget *applet)
{
	panel_applet_in_drag = TRUE;
	_panel_widget_applet_drag_start(panel, applet);
}

static void
_panel_widget_applet_drag_end(PanelWidget *panel)
{
	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gtk_grab_remove(panel->currently_dragged_applet->applet);
	_panel_widget_applet_drag_end_no_grab(panel);
	panel->currently_dragged_applet = NULL;
}

void
panel_widget_applet_drag_end(PanelWidget *panel)
{
	_panel_widget_applet_drag_end(panel);
	panel_applet_in_drag = FALSE;
}

/*calculates the value to move the applet by*/
static int
panel_widget_get_moveby(PanelWidget *panel, int pos)
{
	int x,y;

	gtk_widget_get_pointer(panel->fixed, &x, &y);

	if(panel->orient == PANEL_HORIZONTAL)
		return (x/PANEL_CELL_SIZE)- pos;
	else
		return (y/PANEL_CELL_SIZE)- pos;
}

static GList *
walk_up_to(int pos, GList *list)
{
	AppletData *ad = list->data;
	if(ad->pos <= pos && ad->pos+ad->cells > pos)
		return list;
	while(g_list_next(list) && ad->pos+ad->cells <= pos) {
		list = g_list_next(list);
		ad = list->data;
	}
	while(g_list_previous(list) && ad->pos > pos) {
		list = g_list_previous(list);
		ad = list->data;
	}
	return list;
}

static GtkWidget *
is_in_applet(int pos, AppletData *ad)
{
	if(ad->pos <= pos && ad->pos+ad->cells > pos)
		return ad->applet;
	return NULL;
}

static int
panel_widget_get_free_spot(PanelWidget *panel, AppletData *ad)
{
	int i,e;
	int x,y;
	int place;
	int start;
	int right=-1,left=-1;
	GList *list;

	gtk_widget_get_pointer(panel->fixed, &x, &y);

	if(panel->orient == PANEL_HORIZONTAL)
		place = x/PANEL_CELL_SIZE;
	else
		place = y/PANEL_CELL_SIZE;

	if(ad->pos>=panel->size)
		return -1;

	if(!panel->applet_list) {
		if(place+ad->cells>panel->size)
			return panel->size-ad->cells;
		else
			return place;
	}

	list = panel->applet_list;

	start = place-(ad->cells/2);
	if(start<0)
		start = 0;
	for(e=0,i=start;i<panel->size;i++) {
		GtkWidget *applet;
		list = walk_up_to(i,list);
		applet = is_in_applet(i,list->data);
		if(!applet || applet == ad->applet) {
			e++;
			if(e>=ad->cells) {
				right = i-e+1;
				break;
			}
		} else
			e=0;
	}

	start = place+(ad->cells/2);
	if(start>=panel->size)
		start = panel->size-1;
	for(e=0,i=start;i>=0;i--) {
		GtkWidget *applet;
		list = walk_up_to(i,list);
		applet = is_in_applet(i,list->data);
		if(!applet || applet == ad->applet) {
			e++;
			if(e>=ad->cells) {
				left = i;
				break;
			}
		} else
			e=0;
	}

	start = place-(ad->cells/2);

	if(left==-1) {
		if(right==-1)
			return -1;
		else
			return right;
	} else {
		if(right==-1)
			return left;
		else
			return abs(left-start)>abs(right-start)?right:left;
	}
}

/*to call this function we MUST know that there is at least
ad->cells free at pos otherwise we will mess up the panel*/
static void
panel_widget_nice_move(PanelWidget *panel, AppletData *ad, int pos)
{
	if(pos==ad->pos)
		return;

	ad->pos = pos;

	panel->applet_list =
		my_g_list_resort_item(panel->applet_list,ad,
				      (GCompareFunc)applet_data_compare);

	panel_widget_applet_put(panel,ad,FALSE);
}

/*find the cursor position and move the applet to that position*/
int
panel_widget_applet_move_to_cursor(PanelWidget *panel)
{
	if (panel->currently_dragged_applet) {
		int moveby;
		int pos = panel->currently_dragged_applet->pos;
		GtkWidget *applet;
		GList *forb;

		g_assert(panel->currently_dragged_applet);
		applet = panel->currently_dragged_applet->applet;
		g_assert(GTK_IS_WIDGET(applet));
		forb = gtk_object_get_data(GTK_OBJECT(applet),
					   PANEL_APPLET_FORBIDDEN_PANELS);

		if(!panel_widget_is_cursor(panel,10)) {
			GList *list;
			for(list=panels;
			    list!=NULL;
			    list=g_list_next(list)) {
			    	PanelWidget *new_panel =
			    		PANEL_WIDGET(list->data);

			    	if(panel != new_panel &&
			    	   panel_widget_is_cursor(new_panel,10) &&
				   (!g_list_find(forb,new_panel))) {
					pos = panel_widget_get_moveby(
						new_panel,0);
					if(pos<0)
						pos = 0;
					/*disable reentrancy into this
					  function*/
					if(panel_widget_reparent(panel,
							         new_panel,
							         applet,
							         pos)==-1)
					/*can't find a free pos
					  so cancel the reparent*/
						continue;
					_panel_widget_applet_drag_end(panel);
					_panel_widget_applet_drag_start(
						new_panel, applet);
					panel_widget_applet_move_use_idle(
						new_panel);
			    	   	return FALSE;
			    	}
			}
			/*FIXME: without this it's sometimes hard to get
			  applets onto drawers, but it's an annoying
			  behaviour*/
			/*return TRUE;*/
		}

		if(pw_movement_type == PANEL_SWITCH_MOVE ||
		   panel->packed) {
			moveby = panel_widget_get_moveby(panel,pos);
			if(moveby != 0)
				panel_widget_switch_move(panel,
					panel->currently_dragged_applet,
					moveby);
		} else {
			pos = panel_widget_get_free_spot(panel,
					panel->currently_dragged_applet);

			if(pos>=0)
				panel_widget_nice_move(panel,
					panel->currently_dragged_applet,
					pos);
		}
		return TRUE;
	}
	return FALSE;
}

static int
move_timeout_handler(gpointer data)
{
	return panel_widget_applet_move_to_cursor(PANEL_WIDGET(data));
}

void
panel_widget_applet_move_use_idle(PanelWidget *panel)
{
	gtk_timeout_add (30,move_timeout_handler,panel);
}



static int
panel_widget_applet_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	PanelWidget *panel;
	GdkEventButton *bevent;

	panel = gtk_object_get_data(GTK_OBJECT(widget),PANEL_APPLET_PARENT_KEY);

	g_return_val_if_fail(panel!=NULL,TRUE);

	switch (event->type) {
		case GDK_BUTTON_PRESS:
			bevent = (GdkEventButton *) event;

			/* don't propagate this event */
			if (panel->currently_dragged_applet)
				return TRUE;

			if(bevent->button == 2) {
				/* Start drag */
				panel_widget_applet_drag_start(panel, widget);
				panel_widget_applet_move_use_idle(panel);
				return TRUE;
			}

			break;

		case GDK_BUTTON_RELEASE:
			if (panel->currently_dragged_applet) {
				panel_widget_applet_drag_end(panel);
				return TRUE;
			}

			break;

		default:
			break;
	}

	return FALSE;
}


static GtkWidget *
listening_parent(GtkWidget *widget)
{
	if (GTK_WIDGET_NO_WINDOW(widget))
		return listening_parent(widget->parent);

	return widget;
}

static int
panel_sub_event_handler(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	switch (event->type) {
		GdkEventButton *bevent;
		/*pass these to the parent!*/
		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
			bevent = (GdkEventButton *)event;
			if(bevent->button != 1)
				return gtk_widget_event(
				    listening_parent(widget->parent), event);

			break;

		default:
			break;
	}

	return FALSE;
}


static void
bind_applet_events(GtkWidget *widget, gpointer data)
{
	/* XXX: This is more or less a hack.  We need to be able to
	 * capture events over applets so that we can drag them with
	 * the mouse and such.  So we need to force the applet's
	 * widgets to recursively send the events back to their parent
	 * until the event gets to the applet wrapper (the
	 * GtkEventBox) for processing by us.
	 */
	
	if (!GTK_WIDGET_NO_WINDOW(widget))
		gtk_signal_connect(GTK_OBJECT(widget), "event",
				   (GtkSignalFunc) panel_sub_event_handler,
				   NULL);
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, 0);
}

static void
remove_panel_from_forbidden(PanelWidget *panel, PanelWidget *r)
{
	GList *list;
	if(!panel->master_widget)
		return;

	list = gtk_object_get_data(GTK_OBJECT(panel->master_widget),
				   PANEL_APPLET_FORBIDDEN_PANELS);
	if(list) {
		list = g_list_remove(list,r);
		gtk_object_set_data(GTK_OBJECT(panel->master_widget),
				    PANEL_APPLET_FORBIDDEN_PANELS,
				    list);
	}
	panel = gtk_object_get_data(GTK_OBJECT(panel->master_widget),
				    PANEL_APPLET_PARENT_KEY);
	if(panel)
		remove_panel_from_forbidden(panel, r);
}

static void
add_panel_to_forbidden(PanelWidget *panel, PanelWidget *r)
{
	GList *list;
	if(!panel->master_widget)
		return;

	list = gtk_object_get_data(GTK_OBJECT(panel->master_widget),
				   PANEL_APPLET_FORBIDDEN_PANELS);
	if(g_list_find(list,r)==NULL) {
		list = g_list_prepend(list,r);

		gtk_object_set_data(GTK_OBJECT(panel->master_widget),
				    PANEL_APPLET_FORBIDDEN_PANELS,
				    list);
	}

	panel = gtk_object_get_data(GTK_OBJECT(panel->master_widget),
				    PANEL_APPLET_PARENT_KEY);
	if(panel)
		add_panel_to_forbidden(panel, r);
}

static int
panel_widget_applet_destroy(GtkWidget *applet, gpointer data)
{
	PanelWidget *panel;
	PanelWidget *p;
	AppletData *ad;
	int thick;

	panel = gtk_object_get_data(GTK_OBJECT(applet),PANEL_APPLET_PARENT_KEY);

	g_return_val_if_fail(panel!=NULL,FALSE);

	ad = gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_DATA);

	p = gtk_object_get_data(GTK_OBJECT(applet),
				PANEL_APPLET_ASSOC_PANEL_KEY);

	/*we already "took care" of this applet*/
	if(!ad)
		return FALSE;

	panel->applet_list = g_list_remove(panel->applet_list,ad);
	g_free(ad);

	if(p)
		remove_panel_from_forbidden(panel,p);

	if(panel->packed)
		panel_widget_pack_applets(panel);

	thick = panel_widget_get_thick(panel);
	if(panel->thick != thick || panel->packed) {
		panel->thick = thick;
	}
	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_REMOVED_SIGNAL],
			applet);


	return FALSE;
}



static void
bind_top_applet_events(GtkWidget *widget, int bind_lower)
{
	gtk_signal_connect_after(GTK_OBJECT(widget),
			   	 "size_allocate",
			   	 GTK_SIGNAL_FUNC(
			   	 	panel_widget_applet_size_allocate),
				 NULL);

	gtk_signal_connect(GTK_OBJECT(widget), "destroy",
			   GTK_SIGNAL_FUNC(panel_widget_applet_destroy),
			   NULL);

	gtk_signal_connect(GTK_OBJECT(widget),
			   "event",
			   GTK_SIGNAL_FUNC(panel_widget_applet_event),
			   NULL);

	/* XXX: This is more or less a hack.  We need to be able to
	 * capture events over applets so that we can drag them with
	 * the mouse and such.  So we need to force the applet's
	 * widgets to recursively send the events back to their parent
	 * until the event gets to the applet wrapper (the
	 * GtkEventBox) for processing by us.
	 */

	if (bind_lower && GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, 0);
}

static int
panel_widget_find_empty_pos(PanelWidget *panel, int pos)
{
	int i;
	int right=-1,left=-1;
	GList *list;

	if(pos>=panel->size)
		pos = panel->size-1;

	if(!panel->applet_list)
		return pos;

	list = panel->applet_list;

	for(i=pos;i<panel->size;i++) {
		list = walk_up_to(i,list);
		if(!is_in_applet(i,list->data)) {
			right = i;
			break;
		}
	}

	for(i=pos;i>=0;i--) {
		list = walk_up_to(i,list);
		if(!is_in_applet(i,list->data)) {
			left = i;
			break;
		}
	}

	if(left==-1) {
		if(right==-1)
			return -1;
		else
			return right;
	} else {
		if(right==-1)
			return left;
		else
			return abs(left-pos)>abs(right-pos)?right:left;
	}
}

static int
panel_widget_make_empty_pos(PanelWidget *panel, int pos)
{
	AppletData *ad;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(pos>=0,-1);

	if(panel->packed) {
		if(pos >= panel->size) {
			panel->size++;
			return panel->size-1;
		} else {
			ad = get_applet_data_pos(panel,pos);
			g_return_val_if_fail(ad!=NULL,pos);
			if(panel_widget_push_right(panel,ad))
				return ad->pos-1;
			return -1;
		}
	} else {
		GList *list;
		AppletData *rad;

		if(pos>=panel->size)
			pos = panel->size - 1;

		ad = get_applet_data_pos(panel,pos);
		if(!ad)
			return pos;

		rad = ad;
		list = g_list_find(panel->applet_list,ad);
		g_return_val_if_fail(list!=NULL,-1);
		list = g_list_next(list);
		if(list)
			rad = list->data;

		if((!list || ad->pos+ad->cells < rad->pos) &&
		   ad->pos+ad->cells < panel->size)
			return ad->pos+ad->cells;

		if(panel_widget_push_right(panel,rad))
			return rad->pos-1;
		if(panel_widget_push_left(panel,ad))
			return ad->pos+ad->cells;

		/*panel is full!*/
		return -1;
	}
}



void
panel_widget_add_forbidden(PanelWidget *panel)
{
	add_panel_to_forbidden(panel,panel);
}

int
panel_widget_add_full (PanelWidget *panel, GtkWidget *applet, int pos, int bind_lower_events)
{
	AppletData *ad;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(applet!=NULL,-1);
	g_return_val_if_fail(pos>=0,-1);

	if(pw_movement_type == PANEL_SWITCH_MOVE ||
	   panel->packed)
		pos = panel_widget_make_empty_pos(panel,pos);
	else
		pos = panel_widget_find_empty_pos(panel,pos);

	if(pos==-1) return -1;

	/*this will get done right on size allocate!*/
	if(panel->orient == PANEL_HORIZONTAL)
		gtk_fixed_put(GTK_FIXED(panel->fixed),applet,
			      pos*PANEL_CELL_SIZE,0);
	else
		gtk_fixed_put(GTK_FIXED(panel->fixed),applet,
			      0,pos*PANEL_CELL_SIZE);

	gtk_object_set_data(GTK_OBJECT(applet),PANEL_APPLET_PARENT_KEY,panel);
	ad = g_new(AppletData,1);
	ad->applet = applet;
	ad->cells = 1;
	ad->pos = pos;
	ad->prevwidth = -1;
	ad->prevheight = -1;
	ad->prevx = -1;
	ad->prevx = -1;

	panel->applet_list = g_list_insert_sorted(panel->applet_list,ad,
					  (GCompareFunc)applet_data_compare);
	gtk_object_set_data(GTK_OBJECT(applet),PANEL_APPLET_DATA,ad);

	bind_top_applet_events(applet,bind_lower_events);

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_ADDED_SIGNAL],
			applet);
	
	/*NOTE: forbidden list is not updated on addition, use the
	function above for the panel*/

	return pos;
}

static void
run_up_forbidden(PanelWidget *panel,
		 void (*runfunc)(PanelWidget *,PanelWidget *))
{
	GList *list;
	for(list = panel->applet_list;list!=NULL;list = g_list_next(list)) {
		AppletData *ad = list->data;
		PanelWidget *p =
			gtk_object_get_data(GTK_OBJECT(ad->applet),
					    PANEL_APPLET_ASSOC_PANEL_KEY);
		if(p)
			run_up_forbidden(p,runfunc);
	}
	(*runfunc)(panel,panel);
}

int
panel_widget_reparent (PanelWidget *old_panel,
		       PanelWidget *new_panel,
		       GtkWidget *applet,
		       int pos)
{
	int thick;
	AppletData *ad;
	PanelWidget *p;

	g_return_val_if_fail(old_panel!=NULL,-1);
	g_return_val_if_fail(new_panel!=NULL,-1);
	g_return_val_if_fail(applet!=NULL,-1);
	g_return_val_if_fail(pos>=0,-1);

	ad = gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_DATA);
	g_return_val_if_fail(ad!=NULL,-1);

	if(pw_movement_type == PANEL_SWITCH_MOVE ||
	   new_panel->packed)
		pos = panel_widget_make_empty_pos(new_panel,pos);
	else
		pos = panel_widget_find_empty_pos(new_panel,pos);

	if(pos==-1) return -1;

	gtk_signal_emit(GTK_OBJECT(old_panel),
			panel_widget_signals[APPLET_REMOVED_SIGNAL],
			applet);

	p = gtk_object_get_data(GTK_OBJECT(applet),
				PANEL_APPLET_ASSOC_PANEL_KEY);
	if(p)
		run_up_forbidden(p,remove_panel_from_forbidden);

	gtk_object_set_data(GTK_OBJECT(applet),
			    PANEL_APPLET_PARENT_KEY,
			    new_panel);
	if(p)
		run_up_forbidden(p,add_panel_to_forbidden);

	ad->pos = pos;
	ad->cells = 1;
	ad->prevwidth = -1;
	ad->prevheight = -1;
	ad->prevx = -1;
	ad->prevy = -1;

	old_panel->applet_list = g_list_remove(old_panel->applet_list,
					       ad);
	new_panel->applet_list =
		g_list_insert_sorted(new_panel->applet_list,ad,
				     (GCompareFunc)applet_data_compare);

	gtk_widget_hide(applet);

/*this should have worked I guess*/
#if 0
	/*reparent applet*/
	gtk_widget_reparent(applet,new_panel->fixed);

	/*it will get moved to the right position on size_allocate*/
	if(new_panel->orient == PANEL_HORIZONTAL)
		gtk_fixed_move(GTK_FIXED(new_panel->fixed),applet,
			       pos*PANEL_CELL_SIZE,0);
	else
		gtk_fixed_move(GTK_FIXED(new_panel->fixed),applet,
			       0,pos*PANEL_CELL_SIZE);
#else
	/*remove applet from the old panel applet*/
	gtk_widget_ref(applet);
	gtk_container_remove(GTK_CONTAINER(old_panel->fixed),applet);

	/*add it to the new panel*/
	/*it will get moved to the right position on size_allocate*/
	if(new_panel->orient == PANEL_HORIZONTAL)
		gtk_fixed_put(GTK_FIXED(new_panel->fixed),applet,
			      pos*PANEL_CELL_SIZE,0);
	else
		gtk_fixed_put(GTK_FIXED(new_panel->fixed),applet,
			      0,pos*PANEL_CELL_SIZE);
	gtk_widget_unref(applet);
#endif

	gtk_widget_show(applet);

	if(old_panel->packed)
		panel_widget_pack_applets(old_panel);

	thick = panel_widget_get_thick(old_panel);
	if(old_panel->thick != thick)
		old_panel->thick = thick;

	gtk_signal_emit(GTK_OBJECT(new_panel),
			panel_widget_signals[APPLET_ADDED_SIGNAL],
			applet);
	return pos;
}

int
panel_widget_move (PanelWidget *panel, GtkWidget *applet, int pos)
{
	AppletData *ad;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(applet!=NULL,-1);
	g_return_val_if_fail(pos>=0,-1);

	ad = gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_DATA);
	ad->pos = -1;
	panel->applet_list = g_list_remove(panel->applet_list,ad);

	pos = panel_widget_make_empty_pos(panel,pos);
	if(pos==-1) return -1;

	ad->prevx = -1;
	ad->prevy = -1;
	ad->prevwidth = -1;
	ad->prevheight = -1;
	ad->pos = pos;
	/*reset size to 1*/
	ad->cells = 1;
	panel->applet_list =
		g_list_insert_sorted(panel->applet_list,ad,
				     (GCompareFunc)applet_data_compare);

	panel_widget_applet_put(panel,ad,FALSE);

	return pos;
}

int
panel_widget_remove (PanelWidget *panel, GtkWidget *applet)
{
	AppletData *ad;
	int i,thick;
	PanelWidget *p;

	g_return_val_if_fail(panel!=NULL,FALSE);
	g_return_val_if_fail(applet!=NULL,FALSE);

	ad = gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_DATA);
	/*this applet must be dead already*/
	if(!ad)
		return -1;

	i = ad->pos;

	panel->applet_list = g_list_remove(panel->applet_list,ad);
	g_free(ad);

	gtk_object_set_data(GTK_OBJECT(applet), PANEL_APPLET_DATA, NULL);

	p = gtk_object_get_data(GTK_OBJECT(applet),
				PANEL_APPLET_ASSOC_PANEL_KEY);
	/*remove applet*/
	gtk_widget_ref(applet);
	gtk_container_remove(GTK_CONTAINER(panel->fixed),applet);

	if(p)
		remove_panel_from_forbidden(panel,p);

	if(panel->packed)
		panel_widget_pack_applets(panel);


	thick = panel_widget_get_thick(panel);
	if(panel->thick != thick || panel->packed) {
		panel->thick = thick;
	}

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_REMOVED_SIGNAL],
			applet);
	gtk_widget_unref(applet);

	return i;
}

int
panel_widget_get_pos(PanelWidget *panel, GtkWidget *applet)
{
	AppletData *ad;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(applet,-1);

	ad = gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_DATA);

	g_return_val_if_fail(ad,-1);

	if(panel !=
	   gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_PARENT_KEY))
		return -1;

	return ad->pos;
}

GList*
panel_widget_get_applets(PanelWidget *panel)
{
	GList *rlist=NULL;
	GList *list;

	g_return_val_if_fail(panel,NULL);

	for(list = panel->applet_list;list!=NULL;list = g_list_next(list)) {
		AppletData *ad = list->data;
		rlist = g_list_prepend(rlist,ad->applet);
	}

	return rlist;
}

void
panel_widget_foreach(PanelWidget *panel, GFunc func, gpointer user_data)
{
	GList *list;

	g_return_if_fail(panel);
	g_return_if_fail(func);

	for(list = panel->applet_list;list!=NULL;list = g_list_next(list)) {
		AppletData *ad = list->data;
		(*func)(ad->applet,user_data);
	}
}

void
panel_widget_change_params(PanelWidget *panel,
			   PanelOrientation orient,
			   PanelBackType back_type,
			   char *pixmap,
			   int fit_pixmap_bg,
			   GdkColor *back_color)
{
	PanelOrientation oldorient;
	
	int change_back = FALSE;

	g_return_if_fail(panel);
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(panel)));

	oldorient = panel->orient;

	panel->orient = orient;

	panel->thick = panel_widget_get_thick(panel);


	if(oldorient != panel->orient) {
		/*postpone all adjustments until fixed is size_allocated*/
		panel->postpone_adjust = TRUE;
	   	gtk_signal_emit(GTK_OBJECT(panel),
	   			panel_widget_signals[ORIENT_CHANGE_SIGNAL],
	   			panel->orient);
	}
	if(back_color) {
		/*this will allways trigger, but so what*/
		if(back_type == PANEL_BACK_COLOR)
			change_back = TRUE;
		panel->back_color = *back_color;
	} /*if we didn't pass a color, then don't set a new color!*/

	/*only change the pixmap name if we passed a non-null value*/
	if(pixmap && pixmap != panel->back_pixmap) {
		if(back_type == PANEL_BACK_PIXMAP)
			change_back = TRUE;
		if (panel->back_pixmap)
			g_free (panel->back_pixmap);

		panel->back_pixmap = g_strdup (pixmap);
	}

	/*clearly a signal should be sent*/
	if(panel->back_type != back_type)
		change_back = TRUE;
	
	/*this bit is not optimal, it allways sets the pixmap etc etc ...
	  but this function isn't called too often*/
	panel->back_type = back_type;
	panel->fit_pixmap_bg = fit_pixmap_bg;
	if(back_type == PANEL_BACK_PIXMAP) {
		panel_try_to_set_pixmap (panel, panel->back_pixmap);
	} else if(back_type == PANEL_BACK_COLOR) {
		panel_try_to_set_back_color(panel, &panel->back_color);
	} else {
		panel_try_to_set_default_back(panel);
	}

	/* let the applets know we changed the background */
	if(change_back) {
		gtk_signal_emit(GTK_OBJECT(panel),
				panel_widget_signals[BACK_CHANGE_SIGNAL],
				panel->back_type,
				panel->back_pixmap,
				&panel->back_color);
	}
}

void
panel_widget_change_orient(PanelWidget *panel,
			   PanelOrientation orient)
{
	panel_widget_change_params(panel,
				   orient,
				   panel->back_type,
				   panel->back_pixmap,
				   panel->fit_pixmap_bg,
				   &panel->back_color);
}


/*change global params*/
void
panel_widget_change_global(int explicit_step,
			   int auto_step,
			   int drawer_step,
			   int minimized_size,
			   int minimize_delay,
			   PanelMovementType move_type,
			   int disable_animations)
{
	if(explicit_step>0)
		pw_explicit_step=explicit_step;
	if(auto_step>0)
		pw_auto_step=auto_step;
	if(drawer_step>0)
		pw_drawer_step=drawer_step;
	if(minimized_size>0)
		pw_minimized_size=minimized_size;
	if(minimize_delay>=0)
		pw_minimize_delay=minimize_delay;
	pw_movement_type = move_type;
	pw_disable_animations = disable_animations;
}
