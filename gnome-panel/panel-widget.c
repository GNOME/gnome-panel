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
#include "button-widget.h"
#include "panel-util.h"
#include "gdkextra.h"

GSList *panels=NULL; /*other panels we might want to move the applet to*/

/*define for some debug output*/
/*#define DEBUG 1*/

/*there  can universally be only one applet being dragged since we assume
we only have one mouse :) */
int panel_applet_in_drag = FALSE;

static void panel_widget_class_init	(PanelWidgetClass *klass);
static void panel_widget_init		(PanelWidget      *panel_widget);
static int  panel_try_to_set_pixmap     (PanelWidget      *panel,
					 char             *pixmap);
static void panel_resize_pixmap         (PanelWidget      *panel);
static void panel_try_to_set_back_color (PanelWidget      *panel,
					 GdkColor         *color);
static void panel_widget_size_request   (GtkWidget        *widget,
					 GtkRequisition   *requisition);
static void panel_widget_size_allocate  (GtkWidget        *widget,
					 GtkAllocation    *allocation);
static void panel_widget_cadd		(GtkContainer     *container,
					 GtkWidget        *widget);
static void panel_widget_cremove	(GtkContainer     *container,
					 GtkWidget        *widget);
static int  panel_widget_expose		(GtkWidget        *widget,
					 GdkEventExpose   *event);
static void panel_widget_draw		(GtkWidget        *widget,
					 GdkRectangle     *area);


/*global settings*/
int pw_explicit_step = 50;
int pw_drawer_step = 20;
int pw_auto_step = 10;
int pw_minimized_size = 6;
int pw_minimize_delay = 300;
int pw_disable_animations = FALSE;
PanelMovementType pw_movement_type = PANEL_SWITCH_MOVE;
int pw_applet_padding = 3;

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

		panel_widget_type =
			gtk_type_unique (gtk_fixed_get_type (),
					 &panel_widget_info);
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
static GtkFixedClass *parent_class = NULL;


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
	GtkObjectClass *object_class = (GtkObjectClass*) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass*) class;
	GtkContainerClass *container_class = (GtkContainerClass*) class;
	
	parent_class = gtk_type_class (gtk_fixed_get_type ());

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
	
	widget_class->size_request = panel_widget_size_request;
	widget_class->size_allocate = panel_widget_size_allocate;
	widget_class->expose_event = panel_widget_expose;
	widget_class->draw = panel_widget_draw;

	container_class->add = panel_widget_cadd;
	container_class->remove = panel_widget_cremove;
}

static void
remove_panel_from_forbidden(PanelWidget *panel, PanelWidget *r)
{
	GSList *list;
	GtkWidget *parent_panel;
	
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(r!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(r));

	if(!panel->master_widget)
		return;

	list = gtk_object_get_data(GTK_OBJECT(panel->master_widget),
				   PANEL_APPLET_FORBIDDEN_PANELS);
	if(list) {
		list = g_slist_remove(list,r);
		gtk_object_set_data(GTK_OBJECT(panel->master_widget),
				    PANEL_APPLET_FORBIDDEN_PANELS,
				    list);
	}
	parent_panel = panel->master_widget->parent;
	if (parent_panel)
		remove_panel_from_forbidden(PANEL_WIDGET(parent_panel), r);
}

static void
add_panel_to_forbidden(PanelWidget *panel, PanelWidget *r)
{
	GSList *list;
	GtkWidget *parent_panel;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(r!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(r));

	if(!panel->master_widget)
		return;

	list = gtk_object_get_data(GTK_OBJECT(panel->master_widget),
				   PANEL_APPLET_FORBIDDEN_PANELS);
	if(g_slist_find(list,r)==NULL) {
		list = g_slist_prepend(list,r);

		gtk_object_set_data(GTK_OBJECT(panel->master_widget),
				    PANEL_APPLET_FORBIDDEN_PANELS,
				    list);
	}
	parent_panel = panel->master_widget->parent;
	if (parent_panel)
		add_panel_to_forbidden(PANEL_WIDGET(parent_panel), r);
}

static void
run_up_forbidden(PanelWidget *panel,
		 void (*runfunc)(PanelWidget *,PanelWidget *))
{
	GList *list;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));

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


static void
panel_widget_cadd(GtkContainer *container, GtkWidget *widget)
{
	PanelWidget *p;

	g_return_if_fail (container != NULL);
	g_return_if_fail (IS_PANEL_WIDGET (container));
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_WIDGET (widget));

	panel_widget_add(PANEL_WIDGET(container),widget,0);
	p = gtk_object_get_data(GTK_OBJECT(widget),
				PANEL_APPLET_ASSOC_PANEL_KEY);
	if(p)
		run_up_forbidden(p,add_panel_to_forbidden);
}

static void
panel_widget_cremove(GtkContainer *container, GtkWidget *widget)
{
	AppletData *ad;
	PanelWidget *p;
	PanelWidget *panel;

	g_return_if_fail (container != NULL);
	g_return_if_fail (IS_PANEL_WIDGET (container));
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	
	panel = PANEL_WIDGET (container);
	
	ad = gtk_object_get_data(GTK_OBJECT(widget), PANEL_APPLET_DATA);
	p = gtk_object_get_data(GTK_OBJECT(widget),
				PANEL_APPLET_ASSOC_PANEL_KEY);

	if(p)
		run_up_forbidden(p,remove_panel_from_forbidden);

	gtk_widget_ref(widget);
	if (GTK_CONTAINER_CLASS (parent_class)->remove)
		(* GTK_CONTAINER_CLASS (parent_class)->remove) (container,
								widget);
	if(ad) {
		PanelWidget *panel = PANEL_WIDGET (container);

		panel->applet_list = g_list_remove (panel->applet_list, ad);
		panel->no_window_applet_list =
			g_list_remove (panel->no_window_applet_list, ad);
	}

	gtk_signal_emit(GTK_OBJECT(container),
			panel_widget_signals[APPLET_REMOVED_SIGNAL],
			widget);
	gtk_widget_unref(widget);
}


/*get the number of applets*/
int
panel_widget_get_applet_count(PanelWidget *panel)
{
	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),-1);

	return g_list_length(GTK_FIXED(panel)->children);
}

/*get the list item of the data on the position pos*/
static GList *
get_applet_list_pos(PanelWidget *panel, int pos)
{
	GList *list;

	g_return_val_if_fail(panel!=NULL,NULL);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),NULL);
	
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

/*tells us if an applet is "stuck" on the right side*/
int
panel_widget_is_applet_stuck(PanelWidget *panel, GtkWidget *applet)
{
	AppletData *ad;
	GList *list;
	int i;

	g_return_val_if_fail(panel!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),FALSE);
	g_return_val_if_fail(applet!=NULL,FALSE);
	g_return_val_if_fail(GTK_IS_WIDGET(applet),FALSE);

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
	AppletData *ad;
	AppletData *nad;
	GList *nlist;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(list!=NULL);
	
	ad = list->data;

	nlist = g_list_next(list);
	if(nlist)
		nad = nlist->data;
	if(!nlist || nad->pos > ad->pos+ad->cells) {
		ad->pos++;
		gtk_widget_queue_resize(GTK_WIDGET(panel));
		return;
	}
	nad->pos = ad->pos;
	ad->pos = nad->pos+nad->cells;
	panel->applet_list = my_g_list_swap_prev(panel->applet_list,nlist);

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_MOVE_SIGNAL],
			ad->applet);
	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_MOVE_SIGNAL],
			nad->applet);
	gtk_widget_queue_resize(GTK_WIDGET(panel));
}

static void
panel_widget_switch_applet_left(PanelWidget *panel, GList *list)
{
	AppletData *ad;
	AppletData *pad;
	GList *nlist;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(list!=NULL);
	
	ad = list->data;

	nlist = g_list_previous(list);
	if(nlist)
		pad = nlist->data;
	if(!nlist || pad->pos+pad->cells < ad->pos) {
		ad->pos--;
		gtk_widget_queue_resize(GTK_WIDGET(panel));
		return;
	}
	ad->pos = pad->pos;
	pad->pos = ad->pos+ad->cells;
	panel->applet_list = my_g_list_swap_next(panel->applet_list,nlist);

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_MOVE_SIGNAL],
			ad->applet);
	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_MOVE_SIGNAL],
			pad->applet);
	gtk_widget_queue_resize(GTK_WIDGET(panel));
}

static int
panel_widget_get_right_switch_pos(PanelWidget *panel, GList *list)
{
	AppletData *ad;
	AppletData *nad;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),-1);
	g_return_val_if_fail(list!=NULL,-1);
	
	ad = list->data;

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
	AppletData *ad;
	AppletData *pad;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),-1);
	g_return_val_if_fail(list!=NULL,-1);
	
	ad = list->data;

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
	g_return_if_fail(IS_PANEL_WIDGET(panel));

	if(moveby==0)
		return;

	list = g_list_find(panel->applet_list,ad);
	g_return_if_fail(list!=NULL);

	finalpos = ad->pos+moveby;

	if(finalpos >= panel->size)
		finalpos = panel->size-1;
	else if(finalpos < pw_applet_padding)
		finalpos = pw_applet_padding;

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

/*this is a special function and may fail if called improperly, it works
only under special circumstance when we know there is nothing from
old_size to panel->size*/
static void
panel_widget_right_stick(PanelWidget *panel,int old_size)
{
	int i,pos;
	GList *list,*prev;
	AppletData *ad;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(old_size>=0);
	
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
		gtk_signal_emit(GTK_OBJECT(panel),
				panel_widget_signals[APPLET_MOVE_SIGNAL],
				((AppletData *)list->data)->applet);
}

static void
panel_widget_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	PanelWidget *panel;
	GList *list;

	g_return_if_fail(widget!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(widget));
	g_return_if_fail(requisition!=NULL);

	panel = PANEL_WIDGET(widget);

	if(panel->orient == PANEL_HORIZONTAL) {
		requisition->width = pw_applet_padding;
		requisition->height = PANEL_MINIMUM_WIDTH;
	} else {
		requisition->height = pw_applet_padding;
		requisition->width = PANEL_MINIMUM_WIDTH;
	}

	for(list = panel->applet_list; list!=NULL; list = g_list_next(list)) {
		AppletData *ad = list->data;
		gtk_widget_size_request (ad->applet, &ad->applet->requisition);
		if(panel->orient == PANEL_HORIZONTAL) {
			if(requisition->height <
			   ad->applet->requisition.height) {
				requisition->height =
					ad->applet->requisition.height;
			}
			if(panel->packed)
				requisition->width +=
					ad->applet->requisition.width +
					pw_applet_padding;
		} else {
			if(requisition->width <
			   ad->applet->requisition.width) {
				requisition->width =
					ad->applet->requisition.width;
			}
			if(panel->packed)
				requisition->height +=
					ad->applet->requisition.height +
					pw_applet_padding;
		}
	}
	if(!panel->packed) {
		if(panel->orient == PANEL_HORIZONTAL) {
			requisition->width = panel->size;
		} else {
			requisition->height = panel->size;
		}
	}
}

void
panel_widget_draw_all(PanelWidget *panel)
{
	GList *li;
	GtkWidget *widget;
	GdkGC *gc;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	
	if(!panel->pixmap) /*it will get drawn soon anyhow*/
		return;

	widget = GTK_WIDGET(panel);
	
	gc = widget->style->bg_gc[GTK_WIDGET_STATE(widget)];

	if(widget->style->bg_pixmap[GTK_WIDGET_STATE(widget)]) {
		gdk_gc_set_fill(gc, GDK_TILED);
		gdk_gc_set_tile(gc, widget->style->bg_pixmap[GTK_WIDGET_STATE(widget)]);
	}
				
	gdk_draw_rectangle(panel->pixmap,gc, TRUE, 0,0,-1,-1);

	gdk_gc_set_fill(gc, GDK_SOLID);
/*	gdk_gc_set_tile(gc, NULL); */
	
	for(li = panel->applet_list; li != NULL;
	    li = g_list_next(li)) {
		AppletData *ad = li->data;
		if(IS_BUTTON_WIDGET(ad->applet))
			button_widget_draw(BUTTON_WIDGET(ad->applet),
					   panel->pixmap);
	}
	if(GTK_WIDGET_DRAWABLE(widget))
		gdk_draw_pixmap(widget->window,
				widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
				panel->pixmap,
				0,0,0,0,-1,-1);
}

void
panel_widget_draw_icon(PanelWidget *panel, ButtonWidget *button)
{
	GtkWidget *widget;
	GtkWidget *applet;
	GdkGC *gc;
	
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(button != NULL);
	g_return_if_fail(IS_BUTTON_WIDGET(button));

	if(!panel->pixmap) /*it will get drawn soon anyhow*/
		return;

	widget = GTK_WIDGET(panel);
	applet = GTK_WIDGET(button);
	
	gc = widget->style->bg_gc[GTK_WIDGET_STATE(widget)];

	if(widget->style->bg_pixmap[GTK_WIDGET_STATE(widget)]) {
		gdk_gc_set_fill(gc, GDK_TILED);
		gdk_gc_set_tile(gc, widget->style->bg_pixmap[GTK_WIDGET_STATE(widget)]);
	}
				
	gdk_draw_rectangle(panel->pixmap,gc, TRUE,
			   applet->allocation.x,applet->allocation.y,
			   applet->allocation.width,applet->allocation.height);

	gdk_gc_set_fill(gc, GDK_SOLID);
/*	gdk_gc_set_tile(gc, NULL); */

	button_widget_draw(button, panel->pixmap);

	if(GTK_WIDGET_DRAWABLE(widget))
		gdk_draw_pixmap(widget->window,
				widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
				panel->pixmap,
				applet->allocation.x,applet->allocation.y,
				applet->allocation.x,applet->allocation.y,
				applet->allocation.width,applet->allocation.height);

}

static void
panel_widget_draw(GtkWidget *widget, GdkRectangle *area)
{
	GList *li;
	PanelWidget *panel;

	g_return_if_fail(widget!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(widget));

	panel = PANEL_WIDGET(widget);

	if(!GTK_WIDGET_DRAWABLE(widget))
		return;
	if(panel->pixmap) {
		gdk_draw_pixmap(widget->window,
				widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
				panel->pixmap,
				area->x, area->y,
				area->x, area->y,
				area->width, area->height);
	}
	for(li = panel->applet_list;
	    li != NULL;
	    li = g_list_next(li)) {
		AppletData *ad = li->data;
		GdkRectangle ch_area;
		if(!IS_BUTTON_WIDGET(ad->applet) &&
		   gtk_widget_intersect(ad->applet, area, &ch_area))
			gtk_widget_draw(ad->applet, &ch_area);
	}
}

static int
panel_widget_expose(GtkWidget *widget, GdkEventExpose *event)
{
	GList *li;
	PanelWidget *panel;

	g_return_val_if_fail(widget!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(widget),FALSE);
	g_return_val_if_fail(event!=NULL,FALSE);

	panel = PANEL_WIDGET(widget);

	if(!GTK_WIDGET_DRAWABLE(widget))
		return FALSE;
	if(panel->pixmap) {
		gdk_draw_pixmap(widget->window,
				widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
				panel->pixmap,
				event->area.x, event->area.y,
				event->area.x, event->area.y,
				event->area.width, event->area.height);
	}
	for(li = panel->applet_list;
	    li != NULL;
	    li = g_list_next(li)) {
		AppletData *ad = li->data;
		GdkEventExpose ch_event = *event;

		if(!IS_BUTTON_WIDGET(ad->applet) &&
		   GTK_WIDGET_NO_WINDOW (ad->applet) &&
		   gtk_widget_intersect (ad->applet, &event->area, 
					 &ch_event.area))
			gtk_widget_event (ad->applet, (GdkEvent*) &ch_event);
	}

	return FALSE;
}

static void
panel_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	PanelWidget *panel;
	GList *list;
	GList *send_move = NULL;
	int i;
	int old_size;
	int old_thick;

	g_return_if_fail(widget!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(widget));
	g_return_if_fail(allocation!=NULL);

	panel = PANEL_WIDGET(widget);

	old_size = panel->size;
	old_thick = panel->thick;
	
	widget->allocation = *allocation;
	if (GTK_WIDGET_REALIZED (widget))
		gdk_window_move_resize (widget->window,
					allocation->x, 
					allocation->y,
					allocation->width, 
					allocation->height);

	if(panel->orient == PANEL_HORIZONTAL)
		panel->size = allocation->width;
	else
		panel->size = allocation->height;
	if(old_size<panel->size)
		panel_widget_right_stick(panel,old_size);

	if(panel->packed) {
		i = pw_applet_padding;
		for(list = panel->applet_list;
		    list!=NULL;
		    list = g_list_next(list)) {
			AppletData *ad = list->data;
			GtkAllocation challoc;
			
			if(i != ad->pos) {
				ad->pos = i;
				send_move = g_list_prepend(send_move,ad);
			}
			if(panel->orient == PANEL_HORIZONTAL) {
				ad->cells = ad->applet->requisition.width +
					pw_applet_padding;
				challoc.x = ad->pos;
				challoc.y = (allocation->height - ad->applet->requisition.height) / 2;
			} else {
				ad->cells = ad->applet->requisition.height +
					pw_applet_padding;
				challoc.x = (allocation->width - ad->applet->requisition.width) / 2;
				challoc.y = ad->pos;
			}
			challoc.width = ad->applet->requisition.width;
			challoc.height = ad->applet->requisition.height;
			gtk_widget_size_allocate(ad->applet,&challoc);
			i += ad->cells;
		}
	} else { /*not packed*/
		i = panel->size;
		for(list = g_list_last(panel->applet_list);
		    list!=NULL;
		    list = g_list_previous(list)) {
			AppletData *ad = list->data;
			
			if(panel->orient == PANEL_HORIZONTAL)
				ad->cells = ad->applet->requisition.width +
					pw_applet_padding;
			else
				ad->cells = ad->applet->requisition.height +
					pw_applet_padding;
			if(ad->pos+ad->cells > i) {
				ad->pos = i - ad->cells;
				send_move = g_list_prepend(send_move,ad);
			}
			i = ad->pos;
		}
		if(i<pw_applet_padding) {
			i = pw_applet_padding;
			for(list = panel->applet_list;
			    list!=NULL;
			    list = g_list_next(list)) {
				AppletData *ad = list->data;
				
				if(ad->pos < i) {
					ad->pos = i;
					if(!g_list_find(send_move,ad))
						send_move = g_list_prepend(send_move,ad);
				}
				
				i = ad->pos + ad->cells;
			}
		}

		for(list = panel->applet_list;
		    list!=NULL;
		    list = g_list_next(list)) {
			AppletData *ad = list->data;
			GtkAllocation challoc;
			if(panel->orient == PANEL_HORIZONTAL) {
				challoc.x = ad->pos;
				challoc.y = (allocation->height - ad->applet->requisition.height) / 2;
			} else {
				challoc.x = (allocation->width - ad->applet->requisition.width) / 2;
				challoc.y = ad->pos;
			}
			challoc.width = ad->applet->requisition.width;
			challoc.height = ad->applet->requisition.height;
			gtk_widget_size_allocate(ad->applet,&challoc);
		}
	}
	if(panel->orient == PANEL_HORIZONTAL)
		panel->thick = allocation->height;
	else
		panel->thick = allocation->width;

	if(old_thick != panel->thick &&
	   panel->fit_pixmap_bg &&
	   panel->back_type == PANEL_BACK_PIXMAP)
		panel_resize_pixmap(panel);

	
	while(send_move) {
		AppletData *ad = send_move->data;
		gtk_signal_emit(GTK_OBJECT(panel),
				panel_widget_signals[APPLET_MOVE_SIGNAL],
				ad->applet);
		send_move = my_g_list_pop_first(send_move);
	}
	
	if (GTK_WIDGET_REALIZED (widget)) {
		if(!panel->pixmap ||
		   allocation->width != panel->pixw ||
		   allocation->height != panel->pixh) {
			if(panel->pixmap)
				gdk_pixmap_unref(panel->pixmap);
			panel->pixw = allocation->width;
			panel->pixh = allocation->height;
			panel->pixmap = gdk_pixmap_new(widget->window,
						       panel->pixw,
						       panel->pixh,
						       gtk_widget_get_visual(widget)->depth);
		}
		panel_widget_draw_all(panel);
	}
}

int
panel_widget_is_cursor(PanelWidget *panel, int overlap)
{
	int x,y;
	int w,h;
	GtkWidget *widget;

	g_return_val_if_fail(panel!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),FALSE);

	widget = panel->drop_widget;
	
	if(!widget ||
	   !GTK_IS_WIDGET(widget) ||
	   !GTK_WIDGET_VISIBLE(widget))
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

void
panel_widget_set_back_pixmap (PanelWidget *panel, char *file)
{
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(file!=NULL);

	if (panel_try_to_set_pixmap (panel, file)) {
		if (panel->back_pixmap)
			g_free (panel->back_pixmap);
		panel->back_pixmap = g_strdup (file);
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

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));

	ns = gtk_rc_get_style(GTK_WIDGET(panel));
	if(!ns) ns = gtk_style_new();

	gtk_style_ref(ns);
	gtk_widget_set_style(GTK_WIDGET(panel), ns);
	gtk_style_unref(ns);

	panel_widget_draw_all(panel);
}

static void
panel_try_to_set_back_color(PanelWidget *panel, GdkColor *color)
{
	GtkStyle *ns;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(color!=NULL);

	ns = gtk_style_copy(GTK_WIDGET(panel)->style);
	gtk_style_ref(ns);

	ns->bg[GTK_STATE_NORMAL] = panel->back_color = *color;
	ns->bg[GTK_STATE_NORMAL].pixel =
		panel->back_color.pixel = 1; /* bogus */

	if(ns->bg_pixmap[GTK_STATE_NORMAL]) {
		gdk_imlib_free_pixmap(ns->bg_pixmap[GTK_STATE_NORMAL]);
		ns->bg_pixmap[GTK_STATE_NORMAL] = NULL;
	}

	gtk_widget_set_style(GTK_WIDGET(panel), ns);

	gtk_style_unref(ns);

	panel_widget_draw_all(panel);
}

void
panel_widget_set_back_color(PanelWidget *panel, GdkColor *color)
{
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(color!=NULL);

	panel_try_to_set_back_color(panel, color);

	panel->back_type = PANEL_BACK_COLOR;
	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[BACK_CHANGE_SIGNAL],
			panel->back_type,
			panel->back_pixmap,
			&panel->back_color);
}

static void
panel_resize_pixmap(PanelWidget *panel)
{
	GdkImlibImage *im;
	GdkPixmap *p;
	GtkStyle *ns;
	int w, h;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));

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

	ns = gtk_style_copy(GTK_WIDGET(panel)->style);
	gtk_style_ref(ns);

	if(ns->bg_pixmap[GTK_STATE_NORMAL])
		gdk_imlib_free_pixmap (ns->bg_pixmap[GTK_STATE_NORMAL]);
	ns->bg_pixmap[GTK_STATE_NORMAL] = p;

	gtk_widget_set_style(GTK_WIDGET(panel), ns);
	
	gtk_style_unref(ns);
}

static int
panel_try_to_set_pixmap (PanelWidget *panel, char *pixmap)
{
	GdkImlibImage *im;
	GdkImlibImage *im2;
	GdkPixmap *p;
	GtkStyle *ns;

	g_return_val_if_fail(panel!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),FALSE);

	if(!pixmap || strcmp(pixmap,"")==0) {
		ns = gtk_style_copy(GTK_WIDGET(panel)->style);
		gtk_style_ref(ns);

		p = ns->bg_pixmap[GTK_STATE_NORMAL];
		if(p)
			gdk_imlib_free_pixmap (p);
		ns->bg_pixmap[GTK_STATE_NORMAL] = NULL;

		gtk_widget_set_style(GTK_WIDGET(panel), ns);
	
		gtk_style_unref(ns);

		panel_widget_draw_all(panel);
		return TRUE;
	}

	if (!g_file_exists (pixmap))
		return FALSE;
	
	im = gdk_imlib_load_image (pixmap);
	if (!im)
		return FALSE;

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

	ns = gtk_style_copy(GTK_WIDGET(panel)->style);
	gtk_style_ref(ns);

	if(ns->bg_pixmap[GTK_STATE_NORMAL])
		gdk_imlib_free_pixmap (ns->bg_pixmap[GTK_STATE_NORMAL]);
	ns->bg_pixmap[GTK_STATE_NORMAL] = p;

	gtk_widget_set_style(GTK_WIDGET(panel), ns);
	
	gtk_style_unref(ns);
	gtk_object_set_data(GTK_OBJECT(panel),"gdk_image",im);
	
	panel_widget_draw_all(panel);
	return TRUE;
}

static void
panel_widget_realize(GtkWidget *w, gpointer data)
{
	PanelWidget *panel;

	g_return_if_fail(w!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(w));

	panel = PANEL_WIDGET(w);

	if(panel->back_type == PANEL_BACK_PIXMAP) {
		if (!panel_try_to_set_pixmap (panel, panel->back_pixmap))
			panel->back_type = PANEL_BACK_NONE;
	} else if(panel->back_type == PANEL_BACK_COLOR) {
		panel_try_to_set_back_color(panel, &panel->back_color);
	}

	if(panel->pixmap)
		gdk_pixmap_unref(panel->pixmap);
	panel->pixw = w->allocation.width;
	panel->pixh = w->allocation.height;
	panel->pixmap = gdk_pixmap_new(w->window,
				       panel->pixw,
				       panel->pixh,
				       gtk_widget_get_visual(w)->depth);
	panel_widget_draw_all(panel);
}

static int
panel_widget_destroy(GtkWidget *w, gpointer data)
{
	GdkImlibImage *im;

	g_return_val_if_fail(w!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(w),FALSE);

	im = gtk_object_get_data(GTK_OBJECT(w),"gdk_image");
	if(im) gdk_imlib_destroy_image (im);
	gtk_object_set_data(GTK_OBJECT(w),"gdk_image",NULL);
	
	/*remove from panels list*/
	panels = g_slist_remove(panels,w);
	
	return FALSE;
}

static int panel_widget_applet_event(GtkWidget *widget, GdkEvent *event, gpointer data);

static int
is_in_widget(GdkEventButton *bevent, GtkWidget *widget)
{
	g_return_val_if_fail(widget!=NULL,FALSE);
	g_return_val_if_fail(GTK_IS_WIDGET(widget),FALSE);
	g_return_val_if_fail(bevent!=NULL,FALSE);

	if(bevent->x >= widget->allocation.x &&
	   bevent->x < (widget->allocation.x + widget->allocation.width) &&
	   bevent->y >= widget->allocation.y &&
	   bevent->y < (widget->allocation.y + widget->allocation.height))
		return TRUE;
	return FALSE;
}

static int
panel_widget_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	PanelWidget *panel;
	GList *list;
	GdkEventButton *bevent;

	g_return_val_if_fail(widget!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(widget),FALSE);
	g_return_val_if_fail(event!=NULL,FALSE);

	panel = PANEL_WIDGET(widget);
	bevent = (GdkEventButton *) event;

	if((event->type != GDK_BUTTON_PRESS &&
	    event->type != GDK_BUTTON_RELEASE) ||
	   bevent->window != widget->window)
		return FALSE;

	for(list = panel->no_window_applet_list; list!=NULL;
	    list = g_list_next(list)) {
		AppletData *ad = list->data;
		if(is_in_widget(bevent,ad->applet) &&
		   bevent->button != 1)
			return panel_widget_applet_event(ad->applet,
							 event,data);
	}
	return FALSE;
}

static void
panel_widget_init (PanelWidget *panel)
{
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));

	/*this makes the popup "pop down" once the button is released*/
	gtk_widget_set_events(GTK_WIDGET(panel),
			      gtk_widget_get_events(GTK_WIDGET(panel)) |
			      GDK_BUTTON_RELEASE_MASK);
	
	panel->pixmap = NULL;
	panel->pixw = panel->pixh = -1;

	panel->back_type =PANEL_BACK_NONE;
	panel->fit_pixmap_bg = FALSE;
	panel->back_pixmap = NULL;
	panel->back_color.red = 0;
	panel->back_color.green = 0;
	panel->back_color.blue = 0;
	panel->back_color.pixel = 1;
	panel->packed = FALSE;
	panel->orient = PANEL_HORIZONTAL;
	panel->thick = PANEL_MINIMUM_WIDTH;
	panel->size = INT_MAX;
	panel->applet_list = NULL;
	panel->no_window_applet_list = NULL;
	panel->master_widget = NULL;
	panel->drop_widget = GTK_WIDGET(panel);

	gtk_signal_connect(GTK_OBJECT(panel),
			   "destroy",
			   GTK_SIGNAL_FUNC(panel_widget_destroy),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "event",
			   GTK_SIGNAL_FUNC(panel_widget_event),
			   NULL);

	panels = g_slist_append(panels,panel);
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

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());

	panel = gtk_type_new(panel_widget_get_type());

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

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
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(applet!=NULL);
	g_return_if_fail(GTK_IS_WIDGET(panel));

	panel_applet_in_drag = TRUE;
	_panel_widget_applet_drag_start_no_grab(panel,applet);
}

static void
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
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));

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
	if(applet->window) {
		GdkCursor *fleur_cursor = gdk_cursor_new(GDK_FLEUR);
		gdk_pointer_grab(applet->window,
				 FALSE,
				 APPLET_EVENT_MASK,
				 NULL,
				 fleur_cursor,
				 GDK_CURRENT_TIME);
		gdk_cursor_destroy(fleur_cursor);
	}
}

void
panel_widget_applet_drag_start(PanelWidget *panel, GtkWidget *applet)
{
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(applet!=NULL);
	g_return_if_fail(GTK_IS_WIDGET(panel));

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
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));

	_panel_widget_applet_drag_end(panel);
	panel_applet_in_drag = FALSE;
}

/*get pos of the cursor location*/
int
panel_widget_get_cursorloc(PanelWidget *panel)
{
	int x,y;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),-1);

	gtk_widget_get_pointer(GTK_WIDGET(panel), &x, &y);

	if(panel->orient == PANEL_HORIZONTAL)
		return x;
	else
		return y;
}

/*calculates the value to move the applet by*/
static int
panel_widget_get_moveby(PanelWidget *panel, int pos)
{
	int x,y;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),-1);

	gtk_widget_get_pointer(GTK_WIDGET(panel), &x, &y);

	if(panel->orient == PANEL_HORIZONTAL)
		return x - pos;
	else
		return y - pos;
}

static GList *
walk_up_to(int pos, GList *list)
{
	AppletData *ad = list->data;

	g_return_val_if_fail(list!=NULL,NULL);

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
	g_return_val_if_fail(ad!=NULL,NULL);

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

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),-1);
	g_return_val_if_fail(ad!=NULL,-1);

	gtk_widget_get_pointer(GTK_WIDGET(panel), &x, &y);

	if(panel->orient == PANEL_HORIZONTAL)
		place = x;
	else
		place = y;

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
	if(start<pw_applet_padding)
		start = pw_applet_padding;
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
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(ad!=NULL);

	if(pos==ad->pos)
		return;

	ad->pos = pos;

	panel->applet_list =
		my_g_list_resort_item(panel->applet_list,ad,
				      (GCompareFunc)applet_data_compare);

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_MOVE_SIGNAL],
			ad->applet);
	gtk_widget_queue_resize(GTK_WIDGET(panel));
}

static int moving_timeout = -1;
static int been_moved = FALSE;


/*find the cursor position and move the applet to that position*/
int
panel_widget_applet_move_to_cursor(PanelWidget *panel)
{
	g_return_val_if_fail(panel!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),FALSE);

	if (panel->currently_dragged_applet) {
		int moveby;
		int pos = panel->currently_dragged_applet->pos;
		GtkWidget *applet;
		GSList *forb;

		applet = panel->currently_dragged_applet->applet;
		g_assert(GTK_IS_WIDGET(applet));
		forb = gtk_object_get_data(GTK_OBJECT(applet),
					   PANEL_APPLET_FORBIDDEN_PANELS);

		if(!panel_widget_is_cursor(panel,10)) {
			GSList *list;
			for(list=panels;
			    list!=NULL;
			    list=g_slist_next(list)) {
			    	PanelWidget *new_panel =
			    		PANEL_WIDGET(list->data);

			    	if(panel != new_panel &&
			    	   panel_widget_is_cursor(new_panel,10) &&
				   (!g_slist_find(forb,new_panel))) {
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
			    	   	return FALSE;
			    	}
			}
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
	g_return_val_if_fail(data!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(data),FALSE);
	
	if(been_moved)
		panel_widget_applet_move_to_cursor(PANEL_WIDGET(data));
	been_moved = FALSE;
	moving_timeout = -1;
	
	return FALSE;
}



static int
panel_widget_applet_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	PanelWidget *panel;
	GdkEventButton *bevent;

	g_return_val_if_fail(widget!=NULL,FALSE);
	g_return_val_if_fail(GTK_IS_WIDGET(widget),FALSE);
	g_return_val_if_fail(widget->parent!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(widget->parent),FALSE);
	g_return_val_if_fail(event!=NULL,FALSE);

	panel = PANEL_WIDGET(widget->parent);

	g_return_val_if_fail(panel!=NULL,TRUE);
	
	switch (event->type) {
		case GDK_BUTTON_PRESS:
			bevent = (GdkEventButton *) event;
			printf("the appwidget %lX\n",(long)widget);

			/* don't propagate this event */
			if (panel->currently_dragged_applet)
				return TRUE;

			if(bevent->button == 2) {
				/* Start drag */
				panel_widget_applet_drag_start(panel, widget);
				return TRUE;
			}

			break;

		case GDK_BUTTON_RELEASE:
			if (panel->currently_dragged_applet) {
				panel_widget_applet_drag_end(panel);
				return TRUE;
			}

			break;
		case GDK_MOTION_NOTIFY:
			if (panel->currently_dragged_applet) {
				if(moving_timeout==-1) {
					been_moved = FALSE;
					panel_widget_applet_move_to_cursor(panel);
					moving_timeout = gtk_timeout_add (30,move_timeout_handler,panel);
				} else
					been_moved = TRUE;
			}
			break;
		default:
			break;
	}

	return FALSE;
}

static int
panel_sub_event_handler(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	g_return_val_if_fail(widget!=NULL,FALSE);
	g_return_val_if_fail(GTK_IS_WIDGET(widget),FALSE);
	g_return_val_if_fail(event!=NULL,FALSE);

	switch (event->type) {
		GdkEventButton *bevent;
		/*pass these to the parent!*/
		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
		case GDK_MOTION_NOTIFY:
			bevent = (GdkEventButton *)event;
			if(bevent->button != 1 || panel_applet_in_drag)
				return gtk_widget_event(data, event);

			break;

		default:
			break;
	}

	return FALSE;
}


static void
bind_applet_events(GtkWidget *widget, gpointer data)
{
	g_return_if_fail(widget!=NULL);
	g_return_if_fail(GTK_IS_WIDGET(widget));

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
				   data);
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, data);
}
static int
panel_widget_applet_destroy(GtkWidget *applet, gpointer data)
{
	AppletData *ad;

	g_return_val_if_fail(applet!=NULL,FALSE);
	g_return_val_if_fail(GTK_IS_WIDGET(applet),FALSE);

	ad = gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_DATA);

	/*if it wasn't yet removed*/
	if(applet->parent) {
		PanelWidget *panel = PANEL_WIDGET(applet->parent);

		if(panel->currently_dragged_applet == ad)
			panel->currently_dragged_applet = NULL;

		panel->applet_list = g_list_remove(panel->applet_list,ad);
		panel->no_window_applet_list =
			g_list_remove (panel->no_window_applet_list, ad);
	}

	g_free(ad);
	return FALSE;
}


static void
bind_top_applet_events(GtkWidget *widget, int bind_lower)
{
	g_return_if_fail(widget!=NULL);
	g_return_if_fail(GTK_IS_WIDGET(widget));

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
				       bind_applet_events, widget);
}

static int
panel_widget_find_empty_pos(PanelWidget *panel, int pos)
{
	int i;
	int right=-1,left=-1;
	GList *list;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),-1);
	g_return_val_if_fail(pos>=0,-1);

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

	for(i=pos;i>=pw_applet_padding;i--) {
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

void
panel_widget_add_forbidden(PanelWidget *panel)
{
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));

	add_panel_to_forbidden(panel,panel);
}

int
panel_widget_add_full (PanelWidget *panel, GtkWidget *applet, int pos, int bind_lower_events)
{
	AppletData *ad = NULL;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),-1);
	g_return_val_if_fail(applet!=NULL,-1);
	g_return_val_if_fail(GTK_IS_WIDGET(applet),-1);
	g_return_val_if_fail(pos>=0,-1);
	
	ad = gtk_object_get_data(GTK_OBJECT(applet),PANEL_APPLET_DATA);

	if(ad)
		pos = ad->pos;

	if(pos<pw_applet_padding)
		pos = pw_applet_padding;

	if(pw_movement_type == PANEL_SWITCH_MOVE ||
	   panel->packed) {
		if(get_applet_list_pos(panel,pos)) 
			/*this is a slight hack so that this applet is
			  inserted AFTER an applet with this pos number*/
			pos++;
	} else
		pos = panel_widget_find_empty_pos(panel,pos);

	if(pos==-1) return -1;

	if(!ad) {
		ad = g_new(AppletData,1);
		ad->applet = applet;
		ad->cells = 1;
		ad->pos = pos;
		gtk_object_set_data(GTK_OBJECT(applet),PANEL_APPLET_DATA,ad);
		
		/*this is a completely new applet, which was not yet bound*/
		bind_top_applet_events(applet,bind_lower_events);
	}

	panel->applet_list =
		g_list_insert_sorted(panel->applet_list,ad,
				     (GCompareFunc)applet_data_compare);
	if(GTK_WIDGET_NO_WINDOW(applet))
		panel->no_window_applet_list =
			g_list_prepend(panel->no_window_applet_list,ad);

	/*this will get done right on size allocate!*/
	if(panel->orient == PANEL_HORIZONTAL)
		gtk_fixed_put(GTK_FIXED(panel),applet,
			      pos,0);
	else
		gtk_fixed_put(GTK_FIXED(panel),applet,
			      0,pos);


	gtk_widget_queue_resize(GTK_WIDGET(panel));

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_ADDED_SIGNAL],
			applet);
	
	/*NOTE: forbidden list is not updated on addition, use the
	function above for the panel*/

	return pos;
}


int
panel_widget_reparent (PanelWidget *old_panel,
		       PanelWidget *new_panel,
		       GtkWidget *applet,
		       int pos)
{
	AppletData *ad;

	g_return_val_if_fail(old_panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(old_panel),-1);
	g_return_val_if_fail(new_panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(new_panel),-1);
	g_return_val_if_fail(applet!=NULL,-1);
	g_return_val_if_fail(GTK_IS_WIDGET(applet),-1);
	g_return_val_if_fail(pos>=0,-1);

	ad = gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_DATA);
	g_return_val_if_fail(ad!=NULL,-1);
	
	ad->pos = pos;

	/*reparent applet*/
	if (IS_BUTTON_WIDGET (applet)) {
		gtk_widget_ref (applet);
		gtk_container_remove (GTK_CONTAINER (applet->parent), applet);
		gtk_container_add (GTK_CONTAINER (new_panel), applet);
		gtk_widget_unref (applet);
	} else 
		gtk_widget_reparent(applet,GTK_WIDGET(new_panel));

	return pos;
}

int
panel_widget_move (PanelWidget *panel, GtkWidget *applet, int pos)
{
	AppletData *ad;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),-1);
	g_return_val_if_fail(applet!=NULL,-1);
	g_return_val_if_fail(GTK_IS_WIDGET(applet),-1);
	g_return_val_if_fail(pos>=0,-1);

	ad = gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_DATA);
	ad->pos = -1;
	panel->applet_list = g_list_remove(panel->applet_list,ad);
	panel->no_window_applet_list =
		g_list_remove(panel->no_window_applet_list,ad);
	
	if(pos<pw_applet_padding)
		pos = pw_applet_padding;

	if(get_applet_list_pos(panel,pos)) 
		/*this is a slight hack so that this applet is
		  inserted AFTER an applet with this pos number*/
		pos++;
	if(pos==-1) return -1;

	ad->pos = pos;
	/*reset size to 1*/
	ad->cells = 1;
	panel->applet_list =
		g_list_insert_sorted(panel->applet_list,ad,
				     (GCompareFunc)applet_data_compare);
	if(GTK_WIDGET_NO_WINDOW(ad->applet))
		panel->no_window_applet_list =
			g_list_remove(panel->no_window_applet_list,ad);

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_MOVE_SIGNAL],
			ad->applet);
	gtk_widget_queue_resize(GTK_WIDGET(panel));

	return pos;
}

int
panel_widget_get_pos(PanelWidget *panel, GtkWidget *applet)
{
	AppletData *ad;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),-1);
	g_return_val_if_fail(applet!=NULL,-1);
	g_return_val_if_fail(GTK_IS_WIDGET(applet),-1);

	ad = gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_DATA);

	g_return_val_if_fail(ad,-1);

	if(panel != PANEL_WIDGET(applet->parent))
		return -1;

	return ad->pos;
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

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(GTK_WIDGET_REALIZED(panel));

	oldorient = panel->orient;

	panel->orient = orient;

	if(oldorient != panel->orient) {
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
	gtk_widget_queue_resize(GTK_WIDGET(panel));
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
			   int disable_animations,
			   int applet_padding)
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

	/*change padding on all panels NOW*/
	if(pw_applet_padding != applet_padding) {
		GSList *li;
		pw_applet_padding = applet_padding;
		for(li=panels;li!=NULL;li=g_slist_next(li))
			gtk_widget_queue_resize(li->data);
	}
}
