/* Gnome panel: panel widget
 * (C) 1997,1998,1999,2000 the Free Software Foundation
 * (C) 2000 Eazel, Inc.
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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_filterlevel.h>
#include "rgb-stuff.h"
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

GSList *panels = NULL; /*other panels we might want to move the applet to*/

/*define for some debug output*/
/*#define PANEL_DEBUG 1*/

#define TRANSLUCENT_OPACITY 128

/*there  can universally be only one applet being dragged since we assume
we only have one mouse :) */
gboolean panel_applet_in_drag = FALSE;

/* Commie mode! */
extern gboolean commie_mode;

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
static void applet_move			(PanelWidget      *panel,
					 GtkWidget        *applet);


/*global settings*/
int pw_explicit_step = 50;
int pw_drawer_step = 20;
int pw_auto_step = 10;
int pw_minimized_size = 6;
int pw_minimize_delay = 300;
int pw_maximize_delay = 0;
gboolean pw_disable_animations = FALSE;
PanelMovementType pw_movement_type = PANEL_SWITCH_MOVE;
int pw_applet_padding = 3;
int pw_applet_border_padding = 0;

/* translucent panel variables */
GdkPixmap *desktop_pixmap = NULL;

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

typedef void (*BackSignal) (GtkObject * object,
			    PanelBackType type,
			    char *pixmap,
			    GdkColor *color,
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
applet_data_compare (AppletData *ad1, AppletData *ad2)
{
	return ad1->pos - ad2->pos;
}

/************************
 widget core
 ************************/

guint
panel_widget_get_type (void)
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
	SIZE_CHANGE_SIGNAL,
	APPLET_MOVE_SIGNAL,
	APPLET_ADDED_SIGNAL,
	APPLET_REMOVED_SIGNAL,
	BACK_CHANGE_SIGNAL,
	APPLET_DRAW_SIGNAL,
	APPLET_ABOUT_TO_DIE_SIGNAL,
	LAST_SIGNAL
};

static guint panel_widget_signals[LAST_SIGNAL] = {0};
static GtkFixedClass *parent_class = NULL;


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
			       gtk_marshal_NONE__ENUM,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_ENUM);
	panel_widget_signals[SIZE_CHANGE_SIGNAL] =
		gtk_signal_new("size_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 size_change),
			       gtk_marshal_NONE__INT,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_INT);
	panel_widget_signals[APPLET_MOVE_SIGNAL] =
		gtk_signal_new("applet_move",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 applet_move),
			       gtk_marshal_NONE__POINTER,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_POINTER);
	panel_widget_signals[APPLET_ADDED_SIGNAL] =
		gtk_signal_new("applet_added",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 applet_added),
			       gtk_marshal_NONE__POINTER,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_POINTER);
	panel_widget_signals[APPLET_REMOVED_SIGNAL] =
		gtk_signal_new("applet_removed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 applet_removed),
			       gtk_marshal_NONE__POINTER,
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
	panel_widget_signals[APPLET_DRAW_SIGNAL] =
		gtk_signal_new("applet_draw",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 applet_draw),
			       gtk_marshal_NONE__POINTER,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_POINTER);
	panel_widget_signals[APPLET_ABOUT_TO_DIE_SIGNAL] =
		gtk_signal_new("applet_about_to_die",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 applet_about_to_die),
			       gtk_marshal_NONE__POINTER,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_POINTER);
	gtk_object_class_add_signals(object_class,panel_widget_signals,
				     LAST_SIGNAL);

	class->orient_change = NULL;
	class->size_change = NULL;
	class->applet_move = applet_move;
	class->applet_added = NULL;
	class->applet_removed = NULL;
	class->back_change = NULL;
	class->applet_draw = NULL;
	class->applet_about_to_die = NULL;
	
	widget_class->size_request = panel_widget_size_request;
	widget_class->size_allocate = panel_widget_size_allocate;
	widget_class->expose_event = panel_widget_expose;
	widget_class->draw = panel_widget_draw;

	container_class->add = panel_widget_cadd;
	container_class->remove = panel_widget_cremove;
}

static void
applet_move(PanelWidget *panel, GtkWidget *applet)
{
	if(panel->back_type == PANEL_BACK_COLOR ||
	   (panel->back_type == PANEL_BACK_NONE &&
	    !GTK_WIDGET(panel)->style->bg_pixmap[GTK_WIDGET_STATE(panel)]))
		return;
	if(IS_BUTTON_WIDGET(applet)) {
		ButtonWidget *button = BUTTON_WIDGET(applet);
		if(!button->no_alpha && button->cache) {
			gdk_pixmap_unref(button->cache);
			button->cache = NULL;
		}
	}
	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_DRAW_SIGNAL],
			applet);
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
panel_widget_cremove (GtkContainer *container, GtkWidget *widget)
{
	AppletData *ad;
	PanelWidget *p;
	PanelWidget *panel;

	g_return_if_fail (container != NULL);
	g_return_if_fail (IS_PANEL_WIDGET (container));
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	
	panel = PANEL_WIDGET (container);
	
	ad = gtk_object_get_data (GTK_OBJECT (widget), PANEL_APPLET_DATA);
	p = gtk_object_get_data (GTK_OBJECT (widget),
				 PANEL_APPLET_ASSOC_PANEL_KEY);

	if (ad != NULL &&
	    ad->no_die == 0) {
		gtk_signal_emit (GTK_OBJECT (container),
				 panel_widget_signals[APPLET_ABOUT_TO_DIE_SIGNAL],
				 widget);
	}


	if (p != NULL)
		run_up_forbidden (p, remove_panel_from_forbidden);

	if(panel->currently_dragged_applet == ad)
		panel_widget_applet_drag_end(panel);

	gtk_widget_ref(widget);
	if (GTK_CONTAINER_CLASS (parent_class)->remove)
		(* GTK_CONTAINER_CLASS (parent_class)->remove) (container,
								widget);
	if (ad != NULL) {
		PanelWidget *panel = PANEL_WIDGET (container);

		panel->applet_list = g_list_remove (panel->applet_list, ad);
		panel->no_window_applet_list =
			g_list_remove (panel->no_window_applet_list, ad);
	}

	gtk_signal_emit (GTK_OBJECT (container),
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

static int
allocate_dirty_child(gpointer data)
{
	AppletData *ad = data;
	PanelWidget *panel;
	GtkAllocation challoc;
	GtkRequisition chreq;
	
	g_return_val_if_fail(ad != NULL,FALSE);
	g_return_val_if_fail(ad->applet != NULL,FALSE);
	g_return_val_if_fail(ad->applet->parent != NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(ad->applet->parent),FALSE);

	panel = PANEL_WIDGET(ad->applet->parent);
	
	if(!ad->dirty)
		return FALSE;
	
	gtk_widget_get_child_requisition(ad->applet,&chreq);

	if(panel->orient == PANEL_HORIZONTAL) {
		ad->cells = chreq.width + pw_applet_padding;
		challoc.x = ad->pos;
		challoc.y = (GTK_WIDGET(panel)->allocation.height -
			     chreq.height) / 2;
	} else {
		ad->cells = chreq.height + pw_applet_padding;
		challoc.x = (GTK_WIDGET(panel)->allocation.width -
			     chreq.width) / 2;
		challoc.y = ad->pos;
	}
	challoc.width = chreq.width;
	challoc.height = chreq.height;
	gtk_widget_size_allocate(ad->applet,&challoc);

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_MOVE_SIGNAL],
			ad->applet);

	return FALSE;
}

static void
panel_widget_queue_applet_for_resize(AppletData *ad)
{
	ad->dirty = TRUE;
	g_idle_add(allocate_dirty_child,ad);
}


static void
panel_widget_switch_applet_right(PanelWidget *panel, GList *list)
{
	AppletData *ad;
	AppletData *nad = NULL;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(list!=NULL);
	
	ad = list->data;

	if(list->next)
		nad = list->next->data;
	if(!nad || nad->pos > ad->pos+ad->cells) {
		ad->pos++;
		panel_widget_queue_applet_for_resize(ad);
		return;
	}

	nad->pos = ad->pos;
	ad->pos = nad->pos+nad->cells;
	panel->applet_list = my_g_list_swap_next(panel->applet_list,list);

	panel_widget_queue_applet_for_resize(ad);
	panel_widget_queue_applet_for_resize(nad);
}

static void
panel_widget_switch_applet_left(PanelWidget *panel, GList *list)
{
	AppletData *ad;
	AppletData *pad = NULL;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(list!=NULL);
	
	ad = list->data;

	if(list->prev)
		pad = list->prev->data;
	if(!pad || pad->pos+pad->cells < ad->pos) {
		ad->pos--;
		panel_widget_queue_applet_for_resize(ad);
		return;
	}
	ad->pos = pad->pos;
	pad->pos = ad->pos+ad->cells;
	panel->applet_list = my_g_list_swap_prev(panel->applet_list,list);

	panel_widget_queue_applet_for_resize(ad);
	panel_widget_queue_applet_for_resize(pad);
}

static int
panel_widget_get_right_switch_pos(PanelWidget *panel, GList *list)
{
	AppletData *ad;
	AppletData *nad = NULL;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),-1);
	g_return_val_if_fail(list!=NULL,-1);
	
	ad = list->data;

	if(list->next)
		nad = list->next->data;
	if(!nad || nad->pos > ad->pos+ad->cells)
		return ad->pos+1;
	return nad->pos+nad->cells-ad->cells;
}

static int
panel_widget_get_left_switch_pos(PanelWidget *panel, GList *list)
{
	AppletData *ad;
	AppletData *pad = NULL;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),-1);
	g_return_val_if_fail(list!=NULL,-1);
	
	ad = list->data;

	if(list->prev)
		pad = list->prev->data;
	if(!pad || pad->pos+pad->cells < ad->pos)
		return ad->pos-1;
	return pad->pos;
}


static void
panel_widget_switch_move (PanelWidget *panel, AppletData *ad, int moveby)
{
	int padding;
	int finalpos;
	int pos;
	GList *list;

	g_return_if_fail (ad != NULL);
	g_return_if_fail (panel != NULL);
	g_return_if_fail (IS_PANEL_WIDGET (panel));

	if (moveby == 0)
		return;

	list = g_list_find (panel->applet_list, ad);
	g_return_if_fail (list != NULL);

	finalpos = ad->pos + moveby;

	if (panel->no_padding_on_ends)
		padding = 0;
	else
		padding = pw_applet_padding;

	if (ad->pos < finalpos) {
		while (ad->pos < finalpos) {
			pos = panel_widget_get_right_switch_pos (panel, list);
			if (pos + ad->cells > panel->size ||
			    abs (pos - finalpos) > abs (ad->pos - finalpos))
				return;
			panel_widget_switch_applet_right (panel, list);
		}
	} else {
		while (ad->pos > finalpos) {
			pos = panel_widget_get_left_switch_pos (panel, list);
			if (pos < padding ||
			    abs (pos - finalpos) > abs (ad->pos - finalpos))
				return;
			panel_widget_switch_applet_left (panel, list);
		}
	}
}

static int
push_applet_right(PanelWidget *panel, GList *list)
{
	AppletData *ad = NULL;
	AppletData *nad = NULL;

	g_return_val_if_fail(panel!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),FALSE);
	
	if (list) {
		ad = list->data;
	
		if(ad->pos + ad->cells >= panel->size)
			return FALSE;

		if(list->next)
			nad = list->next->data;
	}

	if(!nad || nad->pos > ad->pos+ad->cells) {
		ad->pos++;
		panel_widget_queue_applet_for_resize(ad);
		return TRUE;
	}

	g_return_val_if_fail(list!=NULL,FALSE);
	
	if(!push_applet_right(panel,list->next)) {
		return FALSE;
	}
	ad->pos++;
	panel_widget_queue_applet_for_resize(ad);
	return TRUE;
}

static int
push_applet_left(PanelWidget *panel, GList *list)
{
	AppletData *ad = NULL;
	AppletData *pad = NULL;

	g_return_val_if_fail(panel!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),FALSE);

	if (list) {
		ad = list->data;

		if(ad->pos <= (panel->no_padding_on_ends?0:pw_applet_padding))
			return FALSE;

		if(list->prev)
			pad = list->prev->data;
	}

	if(!list || !pad || pad->pos+pad->cells < ad->pos) {
		ad->pos--;
		panel_widget_queue_applet_for_resize(ad);
		return TRUE;
	}

	g_return_val_if_fail(list!=NULL,FALSE);
	
	if(!push_applet_left(panel,list->prev)) {
		return FALSE;
	}
	ad->pos--;
	panel_widget_queue_applet_for_resize(ad);
	return TRUE;
}

static void
panel_widget_push_move (PanelWidget *panel, AppletData *ad, int moveby)
{
	int finalpos;
	GList *list;

	g_return_if_fail (ad != NULL);
	g_return_if_fail (panel != NULL);
	g_return_if_fail (IS_PANEL_WIDGET (panel));

	if (moveby == 0)
		return;

	list = g_list_find (panel->applet_list, ad);
	g_return_if_fail (list != NULL);
	
	finalpos = ad->pos + moveby;

	if (ad->pos < finalpos) {
		while (ad->pos < finalpos) {
			if ( ! push_applet_right (panel, list))
				return;
		}
	} else {
		while (ad->pos > finalpos) {
			if ( ! push_applet_left (panel, list))
				return;
		}
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
		requisition->height = panel->sz;
	} else {
		requisition->height = pw_applet_padding;
		requisition->width = panel->sz;
	}

	for(list = panel->applet_list; list!=NULL; list = g_list_next(list)) {
		AppletData *ad = list->data;
		GtkRequisition chreq;
		gtk_widget_size_request(ad->applet,&chreq);
		if(panel->orient == PANEL_HORIZONTAL) {
			if(requisition->height - 2*pw_applet_border_padding < chreq.height)
				requisition->height = chreq.height + 2*pw_applet_border_padding;
			if(panel->packed)
				requisition->width += chreq.width +
					pw_applet_padding;
		} else {
			if(requisition->width - 2*pw_applet_border_padding < chreq.width)
				requisition->width = chreq.width + 2*pw_applet_border_padding;
			if(panel->packed)
				requisition->height += chreq.height +
					pw_applet_padding;
		}
	}
	if(!panel->packed) {
		if(panel->orient == PANEL_HORIZONTAL) {
			requisition->width = panel->size;
		} else {
			requisition->height = panel->size;
		}
	} else if(/*panel->packed &&*/ panel->no_padding_on_ends) {
		if(panel->orient == PANEL_HORIZONTAL) {
			requisition->width -= pw_applet_padding*2;
		} else {
			requisition->height -= pw_applet_padding*2;
		}
	}
	requisition->width = CLAMP (requisition->width, 12, gdk_screen_width ());
	requisition->height = CLAMP (requisition->height, 12, gdk_screen_height ());
}

static void
make_background(PanelWidget *panel, guchar *rgb_buf,
		int x, int y, int w, int h,
		GdkPixbuf *pb, int scale_w, int scale_h,
		int rotate)
{
	if(pb) {
		if(scale_w == 0 || scale_h == 0) {
			tile_rgb(rgb_buf,w,h,x,y,w*3,
				 gdk_pixbuf_get_pixels(pb),
				 gdk_pixbuf_get_width(pb),
				 gdk_pixbuf_get_height(pb),
				 gdk_pixbuf_get_rowstride(pb),
				 gdk_pixbuf_get_has_alpha(pb));
		} else {
			tile_rgb_pixbuf(rgb_buf, w, h, x, y, w*3,
					pb, scale_w, scale_h,
					rotate);
		}
	} else {
		int i;
		int size;
		int r,g,b;
		guchar *p;
		if(panel->back_type != PANEL_BACK_COLOR) {
			GtkWidget *widget = GTK_WIDGET(panel);
			/* convert to 8 bit per channel */
			r = widget->style->bg[GTK_WIDGET_STATE(widget)].red>>8;
			g = widget->style->bg[GTK_WIDGET_STATE(widget)].green>>8;
			b = widget->style->bg[GTK_WIDGET_STATE(widget)].blue>>8;
		} else {
			/* convert to 8 bit per channel */
			r = panel->back_color.red>>8;
			g = panel->back_color.green>>8;
			b = panel->back_color.blue>>8;
		}
		size = w*h;
		p = rgb_buf;
		for(i=0;i<size;i++) {
			(*p++) = r;
			(*p++) = g;
			(*p++) = b;
		}
	}
}

static void
send_draw_to_all_applets(PanelWidget *panel)
{
	GList *li;
	for(li = panel->applet_list; li != NULL;
	    li = g_list_next(li)) {
		AppletData *ad = li->data;
		gtk_signal_emit(GTK_OBJECT(panel),
				panel_widget_signals[APPLET_DRAW_SIGNAL],
				ad->applet);
	}
}

static void
kill_cache_on_all_buttons(PanelWidget *panel, gboolean even_no_alpha)
{
	GList *li;
	for(li = panel->applet_list; li != NULL;
	    li = g_list_next(li)) {
		AppletData *ad = li->data;
		if(IS_BUTTON_WIDGET(ad->applet)) {
			ButtonWidget *button = BUTTON_WIDGET(ad->applet);
			if((even_no_alpha || !button->no_alpha)
			   && button->cache) {
				gdk_pixmap_unref(button->cache);
				button->cache = NULL;
			}
		}
	}
}

void
panel_widget_force_repaint (PanelWidget *panel)
{
	gtk_widget_queue_clear (GTK_WIDGET (panel));
	kill_cache_on_all_buttons(panel, TRUE);
	send_draw_to_all_applets(panel);
}

/* set up a pixbuf thats the background of the panel */
void
panel_widget_setup_translucent_background (PanelWidget *panel)
{
	GdkPixmap *pixmap = NULL;
	GdkBitmap *bitmap = NULL;
	GdkPixbuf *background = NULL;
	GdkPixbuf *background_shaded = NULL;
	GdkWindow *window = gtk_widget_get_parent_window (GTK_WIDGET (panel));

	if (desktop_pixmap && window != NULL) {
		background = gdk_pixbuf_get_from_drawable (NULL,
				desktop_pixmap, 
				gdk_window_get_colormap (window), 
				panel->x, panel->y,
				0, 0, panel->width, panel->height);

		background_shaded = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 
				8, panel->width, panel->height);

		gdk_pixbuf_composite_color (background, background_shaded,
				0, 0, panel->width, panel->height,
				0.0, 0.0, 1.0, 1.0, 
				GDK_INTERP_NEAREST, TRANSLUCENT_OPACITY,
				0, 0, 100, 
				0, 0);

		/* render it onto a pixmap to use to tile the background */
		gdk_pixbuf_render_pixmap_and_mask (background_shaded, &pixmap, 
				&bitmap, 0);

		/* and we're finished with it */
		if (panel->backpix != NULL) {
			gdk_pixbuf_unref (panel->backpix);
		}
		panel->backpix = background_shaded;
		gdk_pixbuf_unref (background);

		if (bitmap != NULL) {
			gdk_bitmap_unref (bitmap);
		}
		if (pixmap != NULL) {
			if (panel->backpixmap != NULL) {
				gdk_pixmap_unref (panel->backpixmap);
			}
			panel->backpixmap = pixmap;
		}
	}
}

static void
setup_background(PanelWidget *panel, GdkPixbuf **pb, int *scale_w, int *scale_h,
		 gboolean *rotate)
{
	GtkWidget *widget = GTK_WIDGET(panel);
	GdkPixmap *bg_pixmap;
	
	*pb = NULL;
	*scale_w = *scale_h = 0;
	*rotate = FALSE;

	bg_pixmap = widget->style->bg_pixmap[GTK_WIDGET_STATE(widget)];

	if(panel->back_type == PANEL_BACK_NONE) {
		if(bg_pixmap && !panel->backpix) {
			int w, h;
			gdk_window_get_size(bg_pixmap, &w, &h);
			panel->backpix = gdk_pixbuf_get_from_drawable
				(NULL, bg_pixmap, widget->style->colormap,
				 0, 0, 0, 0, w, h);
			kill_cache_on_all_buttons(panel, FALSE);
			send_draw_to_all_applets(panel);
		} else if(!bg_pixmap && panel->backpix) {
			gdk_pixbuf_unref(panel->backpix);
			panel->backpix = NULL;
			kill_cache_on_all_buttons(panel, FALSE);
			send_draw_to_all_applets(panel);
		}
		*pb = panel->backpix;
	} else if(panel->back_type == PANEL_BACK_PIXMAP) {
		if(!panel->backpixmap)
			panel_resize_pixmap(panel);

		*pb = panel->backpix;

		if(panel->fit_pixmap_bg ||
		   panel->strech_pixmap_bg) {
			*scale_w = panel->scale_w;
			*scale_h = panel->scale_h;
			if(panel->orient == PANEL_VERTICAL &&
			   panel->rotate_pixmap_bg)
				*rotate = TRUE;
		} else {
			if(panel->orient == PANEL_VERTICAL &&
			   panel->rotate_pixmap_bg) {
				/* we need to set scales to rotate*/
				*scale_w = gdk_pixbuf_get_width((*pb));
				*scale_h = gdk_pixbuf_get_height((*pb));
				*rotate = TRUE;
			}
		}
	} else if(panel->back_type == PANEL_BACK_TRANSLUCENT) {
		/* YAK: *shrug* */
		/* FIXME: make this returnt he right thing! */
		*pb = panel->backpix;
	} 
}

void
panel_widget_draw_all(PanelWidget *panel, GdkRectangle *area)
{
	GList *li;
	GtkWidget *widget;
	GdkGC *gc;
	GdkPixmap *pixmap;
	GdkRectangle da;
	GdkPixbuf *pb = NULL;
	int size;
	int scale_w = 0,scale_h = 0;
	int rotate = FALSE;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));

	widget = GTK_WIDGET(panel);

#ifdef PANEL_DEBUG
	puts("PANEL_WIDGET_DRAW_ALL");
#endif

	if(!GTK_WIDGET_DRAWABLE(widget) ||
	   widget->allocation.width <= 0 ||
	   widget->allocation.height <= 0)
		return;

	if(area) {
		if(area->width==0 || area->height==0)
			return;
		da = *area;
	} else {
		da.x = 0;
		da.y = 0;
		da.width = widget->allocation.width;
		da.height = widget->allocation.height;
	}

	setup_background(panel, &pb, &scale_w, &scale_h, &rotate);
	
	pixmap = gdk_pixmap_new(widget->window,
				da.width,da.height,
				gtk_widget_get_visual(widget)->depth);
	
	gc = gdk_gc_new(pixmap);
	gdk_gc_copy(gc,widget->style->bg_gc[GTK_WIDGET_STATE(widget)]);

	if(panel->back_type == PANEL_BACK_NONE) {
		if(widget->style->bg_pixmap[GTK_WIDGET_STATE(widget)]) {
			gdk_gc_set_fill(gc, GDK_TILED);
			gdk_gc_set_tile(gc,
					widget->style->bg_pixmap[GTK_WIDGET_STATE(widget)]);
			gdk_gc_set_ts_origin(gc,-da.x,-da.y);
		}
	} else if(panel->back_type == PANEL_BACK_PIXMAP) {
		if(panel->backpixmap) {
			gdk_gc_set_fill(gc, GDK_TILED);
			gdk_gc_set_tile(gc, panel->backpixmap);
			gdk_gc_set_ts_origin(gc,-da.x,-da.y);
		}
	} else if(panel->back_type == PANEL_BACK_TRANSLUCENT) {
		if(panel->backpixmap) {
			gdk_gc_set_fill(gc, GDK_TILED);
			gdk_gc_set_tile(gc, panel->backpixmap);
			gdk_gc_set_ts_origin(gc,-da.x,-da.y);
		}
	} else /*COLOR*/ {
		gdk_gc_set_foreground(gc, &panel->back_color);
	}


	gdk_draw_rectangle(pixmap,gc, TRUE, 0, 0, -1, -1);

	/* get pixel size of an icon */
	size = panel->sz;

	for(li = panel->applet_list; li != NULL;
	    li = g_list_next(li)) {
		AppletData *ad = li->data;
		if(IS_BUTTON_WIDGET(ad->applet) &&
		   ad->applet->allocation.x>=0 &&
		   (!area || gtk_widget_intersect(ad->applet, area, NULL))) {
			ButtonWidget *button = BUTTON_WIDGET(ad->applet);
			if(!button->cache) {
				guchar *rgb_buf;

				button->cache =
					gdk_pixmap_new(widget->window, size,size,
						       gtk_widget_get_visual(widget)->depth);
				rgb_buf = g_new0(guchar, size*size*3);
				/*if the icon doesn't have an opaque tile,
				  draw us a background*/
				if(!button->no_alpha)
					make_background(panel, rgb_buf,
							ad->applet->allocation.x,
							ad->applet->allocation.y,
							size,size, pb, scale_w, scale_h,
							rotate);
				button_widget_draw(button, rgb_buf, size*3);
				gdk_draw_rgb_image(button->cache, gc,
						   0,0, size, size,
						   GDK_RGB_DITHER_NORMAL,
						   rgb_buf, size*3);
				g_free(rgb_buf);
				button_widget_draw_xlib(button, button->cache);
			}
			gdk_draw_pixmap(pixmap,gc,
					button->cache,
					0,0,
					ad->applet->allocation.x - da.x,
					ad->applet->allocation.y - da.y,
					size, size);
		}
	}
	gdk_gc_unref(gc);

	gdk_draw_pixmap(widget->window,
			widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
			pixmap,
			0,0,da.x,da.y,da.width,da.height);
	gdk_pixmap_unref(pixmap);
}

void
panel_widget_draw_icon(PanelWidget *panel, ButtonWidget *button)
{
	GtkWidget *widget;
	GdkRectangle area;

	g_return_if_fail(button != NULL);
	g_return_if_fail(GTK_IS_WIDGET(button));
	g_return_if_fail(panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	
#ifdef PANEL_DEBUG
	puts("PANEL_WIDGET_DRAW_ICON");
#endif

	widget = GTK_WIDGET(button);
	
	area.x = widget->allocation.x;
	area.y = widget->allocation.y;
	area.width = widget->allocation.width;
	area.height = widget->allocation.height;

	panel_widget_draw_all(panel, &area);
}

static void
panel_widget_draw(GtkWidget *widget, GdkRectangle *area)
{
	GList *li;
	PanelWidget *panel;

	g_return_if_fail(widget!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(widget));
	
#ifdef PANEL_DEBUG
	puts("PANEL_WIDGET_DRAW");
	
	printf("allocation %d x %d\n",
	       widget->allocation.width,
	       widget->allocation.height);
#endif

	panel = PANEL_WIDGET(widget);

	if(!GTK_WIDGET_DRAWABLE(widget) ||
	   panel->inhibit_draw)
		return;

	panel_widget_draw_all(panel, area);

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
	
#ifdef PANEL_DEBUG
	puts("PANEL_WIDGET_EXPOSE");
#endif

	panel_widget_draw_all(panel, &event->area);

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
	GSList *send_move = NULL;
	GSList *li;
	int i;
	int old_size;
	int old_thick;
	GtkAllocation old_alloc;
	int side_padding;

	g_return_if_fail(widget!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(widget));
	g_return_if_fail(allocation!=NULL);
	
	panel = PANEL_WIDGET(widget);

#ifdef PANEL_DEBUG
	puts("PANEL_WIDGET_SIZE_ALLOCATE");
	printf("allocation %d x %d\n",
	       allocation->width,
	       allocation->height);
#endif

	/* allow drawing if it was inhibited */
	panel->inhibit_draw = FALSE;
	
	old_size = panel->size;
	old_thick = panel->thick;
	
	old_alloc = widget->allocation;

	if((widget->allocation.width != allocation->width ||
	    widget->allocation.height != allocation->height) &&
	   panel->back_type != PANEL_BACK_COLOR &&
	   (panel->back_type != PANEL_BACK_NONE ||
	    GTK_WIDGET(panel)->style->bg_pixmap[GTK_WIDGET_STATE(panel)])) {
		kill_cache_on_all_buttons(panel, FALSE);
		send_draw_to_all_applets(panel);
	}
	
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

	if(panel->no_padding_on_ends)
		side_padding = 0;
	else
		side_padding = pw_applet_padding;

	if(panel->packed) {
		i = side_padding;
		for(list = panel->applet_list;
		    list!=NULL;
		    list = g_list_next(list)) {
			AppletData *ad = list->data;
			GtkAllocation challoc;
			GtkRequisition chreq;
			gtk_widget_get_child_requisition(ad->applet,&chreq);
			
			if(i != ad->pos) {
				ad->pos = i;
				send_move = g_slist_prepend(send_move,ad);
			}
			if(panel->orient == PANEL_HORIZONTAL) {
				ad->cells = chreq.width;
				challoc.x = ad->pos;
				challoc.y = (allocation->height - chreq.height) / 2;
			} else {
				ad->cells = chreq.height;
				challoc.x = (allocation->width - chreq.width) / 2;
				challoc.y = ad->pos;
			}
			if(list->next)
				ad->cells += pw_applet_padding;
			else
				ad->cells += side_padding;
			challoc.width = chreq.width;
			challoc.height = chreq.height;
			ad->dirty = FALSE;
			gtk_widget_size_allocate(ad->applet,&challoc);
			i += ad->cells;
		}
	} else { /*not packed*/
		i = panel->size;
		for(list = g_list_last(panel->applet_list);
		    list!=NULL;
		    list = g_list_previous(list)) {
			AppletData *ad = list->data;
			GtkRequisition chreq;
			gtk_widget_get_child_requisition(ad->applet,&chreq);

			if(panel->orient == PANEL_HORIZONTAL)
				ad->cells = chreq.width;
			else
				ad->cells = chreq.height;
			
			if(list->next)
				ad->cells += pw_applet_padding;
			else
				ad->cells += side_padding;

			if(ad->pos+ad->cells > i) {
				ad->pos = i - ad->cells;
				send_move = g_slist_prepend(send_move,ad);
			}
			i = ad->pos;
		}
		if(i<side_padding) {
			i = side_padding;
			for(list = panel->applet_list;
			    list!=NULL;
			    list = g_list_next(list)) {
				AppletData *ad = list->data;
				
				if(ad->pos < i) {
					ad->pos = i;
					if(!g_slist_find(send_move,ad))
						send_move = g_slist_prepend(send_move,ad);
				}
				
				i = ad->pos + ad->cells;
			}
		}

		for(list = panel->applet_list;
		    list!=NULL;
		    list = g_list_next(list)) {
			AppletData *ad = list->data;
			GtkAllocation challoc;
			GtkRequisition chreq;
			gtk_widget_get_child_requisition(ad->applet,&chreq);
			if(panel->orient == PANEL_HORIZONTAL) {
				challoc.x = ad->pos;
				challoc.y = (allocation->height - chreq.height) / 2;
			} else {
				challoc.x = (allocation->width - chreq.width) / 2;
				challoc.y = ad->pos;
			}
			challoc.width = chreq.width;
			challoc.height = chreq.height;
			ad->dirty = FALSE;
			gtk_widget_size_allocate(ad->applet,&challoc);
		}
	}
	if(panel->orient == PANEL_HORIZONTAL)
		panel->thick = allocation->height;
	else
		panel->thick = allocation->width;

	if(panel->back_type == PANEL_BACK_PIXMAP &&
	   (panel->fit_pixmap_bg || panel->strech_pixmap_bg) &&
	   (old_alloc.width != allocation->width ||
	    old_alloc.height != allocation->height)) {
		if(panel->backpixmap) {
			gdk_pixmap_unref(panel->backpixmap);
			panel->backpixmap = NULL;
		}
	}

	if (panel->back_type == PANEL_BACK_TRANSLUCENT &&
			GTK_WIDGET(panel)->window != NULL) {
		panel->width = allocation->width;
		panel->height = allocation->height;
		gdk_window_get_deskrelative_origin (GTK_WIDGET(panel)->window,
				&panel->x, &panel->y);
		panel_widget_setup_translucent_background (panel);
	}

	for(li=send_move;li!=NULL;li=g_slist_next(li)) {
		AppletData *ad = li->data;
		gtk_signal_emit(GTK_OBJECT(panel),
				panel_widget_signals[APPLET_MOVE_SIGNAL],
				ad->applet);
	}
	g_slist_free(send_move);
	
	if (GTK_WIDGET_REALIZED (widget))
		panel_widget_draw_all(panel,NULL);
}

gboolean
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
	w = widget->allocation.width;
	h = widget->allocation.height;

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

#ifdef PANEL_DEBUG
	puts("PANEL_WIDGET_SET_BACK_PIXMAP");
#endif

	if (panel_try_to_set_pixmap (panel, file)) {
		g_free (panel->back_pixmap);
		panel->back_pixmap = g_strdup (file);
		panel->back_type = PANEL_BACK_PIXMAP;
		panel_widget_draw_all(panel,NULL);

		gtk_signal_emit(GTK_OBJECT(panel),
				panel_widget_signals[BACK_CHANGE_SIGNAL],
				panel->back_type,
				panel->back_pixmap,
				&panel->back_color);
	} else
		panel_widget_draw_all(panel,NULL);
}

static void
panel_try_to_set_back_color(PanelWidget *panel, GdkColor *color)
{
	GdkColormap *cmap;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(color!=NULL);
	
	kill_cache_on_all_buttons(panel, FALSE);
	send_draw_to_all_applets(panel);

	cmap = gtk_widget_get_colormap(GTK_WIDGET(panel));

	if(gdk_colormap_alloc_color(cmap,color,FALSE,TRUE)) {
		panel->back_color = *color;
	}
}

void
panel_widget_set_back_color(PanelWidget *panel, GdkColor *color)
{
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(color!=NULL);

	panel->back_type = PANEL_BACK_COLOR;
	panel_try_to_set_back_color(panel, color);
	panel_widget_draw_all(panel,NULL);

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[BACK_CHANGE_SIGNAL],
			panel->back_type,
			panel->back_pixmap,
			&panel->back_color);
}

static GdkPixmap *
get_pixmap_from_pixbuf(GtkWidget *w, GdkPixbuf *pb, int scale_w, int scale_h,
		       int rotate)
{
	GdkGC *gc;
	GdkPixmap *p;
	gdouble affine[6];
	guchar *rgb;
	
	affine[1] = affine[2] = affine[4] = affine[5] = 0;

	affine[0] = scale_w / (double)(gdk_pixbuf_get_width(pb));
	affine[3] = scale_h / (double)(gdk_pixbuf_get_height(pb));

	if(rotate) {
		int tmp;
		gdouble aff[6];

		art_affine_rotate(aff,270);
		art_affine_multiply(affine,affine,aff);
		art_affine_translate(aff,0,scale_w);
		art_affine_multiply(affine,affine,aff);
		
		tmp = scale_h;
		scale_h = scale_w;
		scale_w = tmp;
	}
	
	rgb = g_new0(guchar,scale_h*scale_w*3);
#ifdef PANEL_DEBUG
	printf("scale_w %d scale_h %d\n",scale_w,scale_h);
#endif
	transform_pixbuf(rgb,
		         0,0,scale_w,scale_h,scale_w*3,
		         pb,affine,ART_FILTER_NEAREST,NULL);
	p = gdk_pixmap_new(w->window, scale_w,scale_h,
			   gtk_widget_get_visual(GTK_WIDGET(w))->depth);
	gc = gdk_gc_new(p);
	gdk_draw_rgb_image(p,gc,0,0,
			   scale_w,scale_h,
			   GDK_RGB_DITHER_NORMAL,
			   rgb, scale_w*3);
	g_free(rgb);
	gdk_gc_destroy(gc);

	return p;
}


static void
panel_resize_pixmap(PanelWidget *panel)
{
	int w, h;
	int pw, ph;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));

	if(panel->backpixmap)
		gdk_pixmap_unref(panel->backpixmap);
	panel->backpixmap = NULL;
	
	if(!panel->backpix) return;
	
	panel->scale_w = w = gdk_pixbuf_get_width(panel->backpix);
	panel->scale_h = h = gdk_pixbuf_get_height(panel->backpix);
	
	pw = GTK_WIDGET(panel)->allocation.width;
	ph = GTK_WIDGET(panel)->allocation.height;

	if(panel->fit_pixmap_bg) {
		switch (panel->orient) {
		case PANEL_HORIZONTAL:
			panel->scale_w = w * ph / h;
			panel->scale_h = ph;
			break;

		case PANEL_VERTICAL:
			if(panel->rotate_pixmap_bg) {
				panel->scale_w = w * pw / h;
				panel->scale_h = pw;
			} else {
				panel->scale_w = pw;
				panel->scale_h = h * pw / w;
			}
			break;

		default:
			g_assert_not_reached ();
		}
	} else if(panel->strech_pixmap_bg) {
		if(panel->orient == PANEL_VERTICAL &&
		   panel->rotate_pixmap_bg) {
			panel->scale_w = ph;
			panel->scale_h = pw;
		} else {
			panel->scale_w = pw;
			panel->scale_h = ph;
		}
	}
	
	panel->backpixmap = get_pixmap_from_pixbuf(
					GTK_WIDGET(panel),
					panel->backpix,
					panel->scale_w,
					panel->scale_h,
					panel->orient == PANEL_VERTICAL &&
					panel->rotate_pixmap_bg);
}

static int
panel_try_to_set_pixmap (PanelWidget *panel, char *pixmap)
{
	g_return_val_if_fail(panel!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),FALSE);

	kill_cache_on_all_buttons(panel, FALSE);
	send_draw_to_all_applets(panel);

	if(panel->backpix)
		gdk_pixbuf_unref(panel->backpix);
	panel->backpix = NULL;
	if (panel->backpixmap != NULL)
		gdk_pixmap_unref (panel->backpixmap);
	panel->backpixmap = NULL;

	if (string_empty (pixmap)) {
		return TRUE;
	}

	if ( ! panel_file_exists (pixmap))
		return FALSE;
	
	panel->backpix = gdk_pixbuf_new_from_file (pixmap);
	if (panel->backpix == NULL)
		return FALSE;

	panel->scale_w = gdk_pixbuf_get_width (panel->backpix);
	panel->scale_h = gdk_pixbuf_get_height (panel->backpix);

	return TRUE;
}

static int
panel_try_to_set_translucent (PanelWidget *panel)
{
	g_return_val_if_fail(panel!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel),FALSE);

	kill_cache_on_all_buttons(panel, FALSE);
	send_draw_to_all_applets(panel);

	panel_widget_setup_translucent_background (panel);

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
	} else if(panel->back_type == PANEL_BACK_TRANSLUCENT) {
		/* YAK: hmm */
	} else if(panel->back_type == PANEL_BACK_COLOR) {
		panel_try_to_set_back_color(panel, &panel->back_color);
	}

#ifdef PANEL_DEBUG
	puts("PANEL_WIDGET_REALIZE");
#endif
	panel_widget_draw_all(panel,NULL);
}

static void
panel_widget_destroy(GtkWidget *w, gpointer data)
{
	PanelWidget *panel;

	g_return_if_fail(w!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(w));

	panel = PANEL_WIDGET(w);

	if(panel->backpix)
		gdk_pixbuf_unref(panel->backpix);
	panel->backpix = NULL;

	g_free (panel->back_pixmap);
	panel->back_pixmap = NULL;

	/*remove from panels list*/
	panels = g_slist_remove(panels,w);
}

static void
panel_widget_style_set(PanelWidget *panel)
{
	if(panel->back_type == PANEL_BACK_NONE) {
		if(panel->backpix)
			gdk_pixbuf_unref(panel->backpix);
		panel->backpix = NULL;
		kill_cache_on_all_buttons(panel, TRUE);
		send_draw_to_all_applets(panel);
	}
}

static gboolean panel_widget_applet_event(GtkWidget *widget, GdkEvent *event, gpointer data);

static gboolean
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

static gboolean
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

PanelWidget *
panel_widget_get_by_id (gint32 id)
{
	GSList *li;

	for (li = panels; li != NULL; li = li->next) {
		PanelWidget *panel = li->data;

		if (panel->unique_id == id)
			return panel;
	}

	return NULL;
}

void
panel_widget_set_id (PanelWidget *panel, gint32 id)
{
	panel->unique_id = id;
}

static gint32
generate_unique_id (void)
{
	gint32 id;
	GTimeVal tv;
	static int incr = -1;

	if (incr == -1)
		incr = (rand () >> 3) & 0xFF;

	id = (rand () >> 3) && 0xFFF;
	id += (incr << 12);

	g_get_current_time (&tv);
	id += (tv.tv_usec & 0x7FF) << 20;

	incr ++;

	if (panel_widget_get_by_id (id) != NULL)
		id = generate_unique_id ();

	return id;
}

static void
panel_widget_init (PanelWidget *panel)
{
	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));

	panel->unique_id = generate_unique_id ();

	/*this makes the popup "pop down" once the button is released*/
	gtk_widget_set_events(GTK_WIDGET(panel),
			      gtk_widget_get_events(GTK_WIDGET(panel)) |
			      GDK_BUTTON_RELEASE_MASK);
	
	panel->back_type =PANEL_BACK_NONE;
	panel->fit_pixmap_bg = FALSE;
	panel->strech_pixmap_bg = FALSE;
	panel->back_pixmap = NULL;
	panel->back_color.red = 0;
	panel->back_color.green = 0;
	panel->back_color.blue = 0;
	panel->back_color.pixel = 1;
	panel->packed = FALSE;
	panel->orient = PANEL_HORIZONTAL;
	panel->thick = PANEL_MINIMUM_WIDTH;
	panel->size = G_MAXINT;
	panel->applet_list = NULL;
	panel->no_window_applet_list = NULL;
	panel->master_widget = NULL;
	panel->drop_widget = GTK_WIDGET(panel);
	panel->backpix = NULL;
	panel->backpixmap = NULL;
	panel->inhibit_draw = FALSE;

	gtk_signal_connect(GTK_OBJECT(panel),
			   "destroy",
			   GTK_SIGNAL_FUNC(panel_widget_destroy),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "style_set",
			   GTK_SIGNAL_FUNC(panel_widget_style_set),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "event",
			   GTK_SIGNAL_FUNC(panel_widget_event),
			   NULL);

	panels = g_slist_append(panels,panel);
}

GtkWidget *
panel_widget_new (gboolean packed,
		  PanelOrientation orient,
		  int sz,
		  PanelBackType back_type,
		  char *back_pixmap,
		  gboolean fit_pixmap_bg,
		  gboolean strech_pixmap_bg,
		  gboolean rotate_pixmap_bg,
		  gboolean no_padding_on_ends,
		  GdkColor *back_color)
{
	PanelWidget *panel;

	panel = gtk_type_new(panel_widget_get_type());

	panel->back_type = back_type;

	panel->fit_pixmap_bg = fit_pixmap_bg;
	panel->strech_pixmap_bg = strech_pixmap_bg;
	panel->rotate_pixmap_bg = rotate_pixmap_bg;
	panel->back_pixmap = g_strdup (sure_string (back_pixmap));
	panel->no_padding_on_ends = no_padding_on_ends;
	
	if(back_color)
		panel->back_color = *back_color;
	else {
		panel->back_color.red = 0;
		panel->back_color.green = 0;
		panel->back_color.blue = 0;
		panel->back_color.pixel = 1;
	}	

	panel->orient = orient;
	panel->sz = sz;

#ifdef PANEL_DEBUG
	printf("GOT SIZE OF %d\n",sz);
#endif

	panel->packed = packed;
	if(packed)
		panel->size = 0;
	else
		panel->size = G_MAXINT;
	
	gtk_signal_connect_after(GTK_OBJECT(panel),
				 "realize",
				 GTK_SIGNAL_FUNC(panel_widget_realize),
				 panel);

	return GTK_WIDGET(panel);
}

static guint moving_timeout = 0;
static gboolean been_moved = FALSE;
static gboolean repeat_if_outside = FALSE;

void
panel_widget_applet_drag_start_no_grab (PanelWidget *panel,
					GtkWidget *applet,
					int drag_off)
{
	AppletData *ad;

	g_return_if_fail (panel != NULL);
	g_return_if_fail (IS_PANEL_WIDGET (panel));
	g_return_if_fail (applet != NULL);
	g_return_if_fail (GTK_IS_WIDGET (panel));

	ad = gtk_object_get_data (GTK_OBJECT (applet), PANEL_APPLET_DATA);
	g_return_if_fail (ad != NULL);

	if (moving_timeout != 0) {
		gtk_timeout_remove (moving_timeout);
		moving_timeout = 0;
		been_moved = FALSE;
	}

#ifdef PANEL_DEBUG
	g_message("Starting drag on a %s at %p\n",
		  gtk_type_name(GTK_OBJECT(applet)->klass->type), applet);
#endif
	panel->currently_dragged_applet = ad;
	if (drag_off == PW_DRAG_OFF_CURSOR)
		ad->drag_off = panel_widget_get_cursorloc (panel) - ad->pos;
	else if (drag_off == PW_DRAG_OFF_CENTER)
		ad->drag_off = ad->cells / 2;
	else
		ad->drag_off = drag_off;

	panel_applet_in_drag = TRUE;
}


void
panel_widget_applet_drag_end_no_grab (PanelWidget *panel)
{
	g_return_if_fail (panel != NULL);
	g_return_if_fail (IS_PANEL_WIDGET (panel));

#ifdef PANEL_DEBUG
	g_message("Ending drag\n");
#endif
	panel->currently_dragged_applet = NULL;
	panel_applet_in_drag = FALSE;

	if (moving_timeout != 0) {
		gtk_timeout_remove (moving_timeout);
		moving_timeout = 0;
		been_moved = FALSE;
	}
}

void
panel_widget_applet_drag_start (PanelWidget *panel,
				GtkWidget *applet,
				int drag_off)
{
	g_return_if_fail (panel !=NULL);
	g_return_if_fail (IS_PANEL_WIDGET (panel));
	g_return_if_fail (applet != NULL);
	g_return_if_fail (GTK_IS_WIDGET (panel));

#ifdef PANEL_DEBUG
	g_message("Starting drag [grabbed] on a %s at %p\n",
		  gtk_type_name(GTK_OBJECT(applet)->klass->type), applet);
#endif
	panel_widget_applet_drag_start_no_grab (panel, applet, drag_off);

	gtk_grab_add (applet);
	if (applet->window != NULL) {
		GdkCursor *fleur_cursor = gdk_cursor_new (GDK_FLEUR);
		gdk_pointer_grab (applet->window,
				  FALSE,
				  APPLET_EVENT_MASK,
				  NULL,
				  fleur_cursor,
				  GDK_CURRENT_TIME);
		gdk_cursor_destroy (fleur_cursor);
		gdk_flush ();
	}
}

void
panel_widget_applet_drag_end (PanelWidget *panel)
{
	g_return_if_fail (panel != NULL);
	g_return_if_fail (IS_PANEL_WIDGET (panel));

	if (panel->currently_dragged_applet == NULL)
		return;
	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gtk_grab_remove (panel->currently_dragged_applet->applet);
	panel_widget_applet_drag_end_no_grab (panel);
	gdk_flush ();
}

/*get pos of the cursor location*/
int
panel_widget_get_cursorloc (PanelWidget *panel)
{
	int x, y;

	g_return_val_if_fail (panel != NULL, -1);
	g_return_val_if_fail (IS_PANEL_WIDGET (panel), -1);

	gtk_widget_get_pointer (GTK_WIDGET (panel), &x, &y);

	if (panel->orient == PANEL_HORIZONTAL)
		return x;
	else
		return y;
}

/*get amount of free space around the applet (including the applet size),
  or return 0 on error or if the panel is packed*/
int
panel_widget_get_free_space(PanelWidget *panel, GtkWidget *applet)
{
	int right,left;
	GList *li;
	AppletData *ad;

	g_return_val_if_fail(panel!=NULL, 0);
	g_return_val_if_fail(IS_PANEL_WIDGET(panel), 0);
	g_return_val_if_fail(applet!=NULL, 0);
	g_return_val_if_fail(GTK_IS_WIDGET(applet), 0);
	
	/*this function doesn't make sense on packed panels*/
	if(panel->packed)
		return 0;
	
	if(panel->no_padding_on_ends) {
		right = panel->size;
		left = 0;
	} else {
		right = panel->size - pw_applet_padding;
		left = pw_applet_padding;
	}
	
	for(li = panel->applet_list; li; li = g_list_next(li)) {
		ad = li->data;
		if(ad->applet == applet)
			break;
	}
	/*the applet is not on this panel*/
	if(!li)
		return 0;
	
	if(li->prev) {
		AppletData *pad = li->prev->data;
		left = pad->pos+pad->cells + pw_applet_padding;
	}
	if(li->next) {
		AppletData *nad = li->next->data;
		right = nad->pos - pw_applet_padding;
	}
	
	return right - left;
}

/*calculates the value to move the applet by*/
static int
panel_widget_get_moveby (PanelWidget *panel, int pos, int offset)
{
	g_return_val_if_fail (panel != NULL, -1);
	g_return_val_if_fail (IS_PANEL_WIDGET (panel), -1);

	return panel_widget_get_cursorloc (panel) - offset - pos;
}

static GList *
walk_up_to (int pos, GList *list)
{
	AppletData *ad;

	g_return_val_if_fail (list != NULL, NULL);

	ad = list->data;

	if (ad->pos <= pos &&
	    ad->pos + ad->cells > pos)
		return list;
	while (list->next != NULL &&
	       ad->pos + ad->cells <= pos) {
		list = list->next;
		ad = list->data;
	}
	while (list->prev != NULL &&
	       ad->pos > pos) {
		list = list->prev;
		ad = list->data;
	}
	return list;
}

static GtkWidget *
is_in_applet (int pos, AppletData *ad)
{
	g_return_val_if_fail (ad != NULL, NULL);

	if (ad->pos <= pos &&
	    ad->pos + ad->cells > pos)
		return ad->applet;
	return NULL;
}

static int
panel_widget_get_free_spot (PanelWidget *panel, AppletData *ad)
{
	int i, e;
	int place;
	int start;
	int right = -1, left = -1;
	GList *list;
	int side_padding;

	g_return_val_if_fail (panel != NULL, -1);
	g_return_val_if_fail (IS_PANEL_WIDGET (panel), -1);
	g_return_val_if_fail (ad != NULL, -1);

	place = panel_widget_get_cursorloc (panel);

	if (ad->pos >= panel->size)
		return -1;

	if (panel->applet_list == NULL) {
		if (place + ad->cells>panel->size)
			return panel->size-ad->cells;
		else
			return place;
	}

	list = panel->applet_list;

	if (panel->no_padding_on_ends)
		side_padding = 0;
	else
		side_padding = pw_applet_padding;

	start = place - ad->drag_off;
	if (start < side_padding)
		start = side_padding;
	for (e = 0, i = start; i < panel->size; i++) {
		GtkWidget *applet;
		list = walk_up_to (i, list);
		applet = is_in_applet (i, list->data);
		if (applet == NULL ||
		    applet == ad->applet) {
			e++;
			if (e >= ad->cells) {
				right = i - e + 1;
				break;
			}
		} else {
			e = 0;
		}
	}

	start = place + ad->drag_off;
	if (start >= panel->size)
		start = panel->size - 1;
	for (e = 0, i = start; i >= 0; i--) {
		GtkWidget *applet;
		list = walk_up_to (i, list);
		applet = is_in_applet (i, list->data);
		if (applet == NULL ||
		    applet == ad->applet) {
			e++;
			if (e >= ad->cells) {
				left = i;
				break;
			}
		} else {
			e=0;
		}
	}

	start = place - ad->drag_off;

	if (left == -1) {
		if (right == -1)
			return -1;
		else
			return right;
	} else {
		if (right == -1)
			return left;
		else
			return abs (left - start) > abs (right - start) ?
				right : left;
	}
}

/*to call this function we MUST know that there is at least
  ad->cells free at pos otherwise we will mess up the panel*/
static void
panel_widget_nice_move (PanelWidget *panel, AppletData *ad, int pos)
{
	g_return_if_fail (panel != NULL);
	g_return_if_fail (IS_PANEL_WIDGET (panel));
	g_return_if_fail (ad != NULL);

	if (pos < 0 ||
	    pos == ad->pos)
		return;

	ad->pos = pos;

	panel->applet_list =
		my_g_list_resort_item (panel->applet_list, ad,
				       (GCompareFunc)applet_data_compare);

	panel_widget_queue_applet_for_resize (ad);
}

/* schedule to run the below function */
static void schedule_try_move (PanelWidget *panel, gboolean repeater);

/*find the cursor position and move the applet to that position*/
static void
panel_widget_applet_move_to_cursor (PanelWidget *panel)
{
	int moveby;
	int pos;
	int movement;
	GtkWidget *applet;
	GSList *forb;
	GdkModifierType mods;
	AppletData *ad;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));

	if (panel->currently_dragged_applet == NULL)
		return;

	ad = panel->currently_dragged_applet;

	pos = ad->pos;

	applet = ad->applet;
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
				pos = panel_widget_get_moveby (new_panel, 0,
							       ad->drag_off);
				if (pos < 0)
					pos = 0;
				panel_widget_applet_drag_end (panel);
				/*disable reentrancy into this
				  function*/
				if (panel_widget_reparent (panel,
							   new_panel,
							   applet,
							   pos) == -1) {
					panel_widget_applet_drag_start
						(panel, applet, ad->drag_off);
					/*can't find a free pos
					  so cancel the reparent*/
					continue;
				}
				panel_widget_applet_drag_start (new_panel,
								applet,
								ad->drag_off);
				/* schedule a move, the thing might have
				 * gone outside the cursor, thus we need to
				 * schedule a move */
				schedule_try_move(new_panel, TRUE);
				return;
			}
		}
	}

	gdk_window_get_pointer(GTK_WIDGET(panel)->window,
			       NULL,NULL,&mods);

	movement = pw_movement_type;

	if (panel->packed) {
		movement = PANEL_SWITCH_MOVE;
	} else {
		if (mods & GDK_CONTROL_MASK)
			movement = PANEL_SWITCH_MOVE;
		else if (mods & GDK_SHIFT_MASK)
			movement = PANEL_PUSH_MOVE;
		else if (mods & GDK_MOD1_MASK)
			movement = PANEL_FREE_MOVE;
	}

	switch(movement) {
	case PANEL_SWITCH_MOVE:
		moveby = panel_widget_get_moveby (panel, pos, ad->drag_off);
		panel_widget_switch_move (panel, ad, moveby);
		break;
	case PANEL_FREE_MOVE:
		pos = panel_widget_get_free_spot (panel, ad);
		panel_widget_nice_move (panel, ad, pos);
		break;
	case PANEL_PUSH_MOVE:
		moveby = panel_widget_get_moveby (panel, pos, ad->drag_off);
		panel_widget_push_move (panel, ad, moveby);
		break;
	}
}

static int
move_timeout_handler(gpointer data)
{
	PanelWidget *panel = data;
	g_return_val_if_fail(data!=NULL,FALSE);
	g_return_val_if_fail(IS_PANEL_WIDGET(data),FALSE);

	if(been_moved &&
	   panel->currently_dragged_applet) {
		panel_widget_applet_move_to_cursor(panel);
		been_moved = FALSE;
		return TRUE;
	}
	been_moved = FALSE;

	if(panel->currently_dragged_applet && repeat_if_outside) {
		int x,y;
		int w,h;
		GtkWidget *widget;

		widget = panel->currently_dragged_applet->applet;

		gtk_widget_get_pointer(widget, &x, &y);
		w = widget->allocation.width;
		h = widget->allocation.height;

		/* if NOT inside return TRUE, this means we will be
		 * kept inside the timeout until we hit the damn widget
		 * or the drag ends */
		if(!(x>=0 && x<=w && y>=0 && y<=h))
			return TRUE;
	}

	moving_timeout = 0;
	
	return FALSE;
}

static void
schedule_try_move(PanelWidget *panel, gboolean repeater)
{
	if (!panel->currently_dragged_applet)
		return;
	repeat_if_outside = repeater;
	if(moving_timeout == 0) {
		been_moved = FALSE;
		panel_widget_applet_move_to_cursor(panel);
		moving_timeout =
			gtk_timeout_add (50, move_timeout_handler, panel);
	} else
		been_moved = TRUE;
}

static gboolean
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
#ifdef PANEL_DEBUG
			printf("the appwidget %lX\n",(long)widget);
#endif

			/* don't propagate this event */
			if (panel->currently_dragged_applet) {
				gtk_signal_emit_stop_by_name
					(GTK_OBJECT (widget), "event");
				return TRUE;
			}

			if ( ! commie_mode &&
			    bevent->button == 2) {
				/* Start drag */
				panel_widget_applet_drag_start
					(panel, widget, PW_DRAG_OFF_CURSOR);
				return TRUE;
			}

			break;

		case GDK_BUTTON_RELEASE:
			if (panel->currently_dragged_applet) {
				gtk_signal_emit_stop_by_name
					(GTK_OBJECT (widget), "event");
				panel_widget_applet_drag_end(panel);
				return TRUE;
			}

			break;
		case GDK_MOTION_NOTIFY:
			schedule_try_move(panel, FALSE);
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

static void
panel_widget_applet_destroy (GtkWidget *applet, gpointer data)
{
	AppletData *ad;

	g_return_if_fail (applet != NULL);
	g_return_if_fail (GTK_IS_WIDGET (applet));

	ad = gtk_object_get_data (GTK_OBJECT (applet), PANEL_APPLET_DATA);

	/*if it wasn't yet removed*/
	if(applet->parent) {
		PanelWidget *panel = PANEL_WIDGET (applet->parent);

		gtk_signal_emit (GTK_OBJECT (panel),
				 panel_widget_signals[APPLET_ABOUT_TO_DIE_SIGNAL],
				 applet);

		if (panel->currently_dragged_applet == ad)
			panel_widget_applet_drag_end (panel);

		panel->applet_list = g_list_remove (panel->applet_list,ad);
		panel->no_window_applet_list =
			g_list_remove (panel->no_window_applet_list, ad);

	}

	g_free (ad);
}


static void
bind_top_applet_events(GtkWidget *widget, gboolean bind_lower)
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

	for (i = pos; i < panel->size; i++) {
		list = walk_up_to (i, list);
		if ( ! is_in_applet (i, list->data)) {
			right = i;
			break;
		}
	}

	for(i = pos;
	    i >= (panel->no_padding_on_ends ? 0 : pw_applet_padding);
	    i--) {
		list = walk_up_to (i, list);
		if ( ! is_in_applet (i, list->data)) {
			left = i;
			break;
		}
	}

	if (left == -1) {
		if (right == -1)
			return -1;
		else
			return right;
	} else {
		if (right == -1)
			return left;
		else
			return abs (left - pos) > abs (right - pos) ?
				right : left;
	}
}

void
panel_widget_add_forbidden (PanelWidget *panel)
{
	g_return_if_fail (panel != NULL);
	g_return_if_fail (IS_PANEL_WIDGET (panel));

	add_panel_to_forbidden (panel, panel);
}

int
panel_widget_add_full (PanelWidget *panel, GtkWidget *applet, int pos,
		       gboolean bind_lower_events, gboolean insert_at_pos)
{
	AppletData *ad = NULL;

	g_return_val_if_fail (panel != NULL, -1);
	g_return_val_if_fail (IS_PANEL_WIDGET (panel), -1);
	g_return_val_if_fail (applet != NULL, -1);
	g_return_val_if_fail (GTK_IS_WIDGET (applet), -1);
	g_return_val_if_fail (pos >= 0, -1);
	
	ad = gtk_object_get_data (GTK_OBJECT (applet), PANEL_APPLET_DATA);

	if (ad != NULL)
		pos = ad->pos;

	if (pos < 0)
		pos = 0;
	if ( ! panel->no_padding_on_ends &&
	     pos < pw_applet_padding)
		pos = pw_applet_padding;
	
	if ( ! insert_at_pos) {
		if (panel->packed) {
			if (get_applet_list_pos (panel, pos)) 
				/*this is a slight hack so that this applet
				  is inserted AFTER an applet with this pos
				  number*/
				pos++;
		} else {
			int newpos = panel_widget_find_empty_pos (panel, pos);
			if (newpos >= 0)
				pos = newpos;
			else if (get_applet_list_pos (panel, pos)) 
				/*this is a slight hack so that this applet
				  is inserted AFTER an applet with this pos
				  number*/
				pos++;
		}
	}

	if(pos==-1) return -1;

	if (ad == NULL) {
		ad = g_new (AppletData, 1);
		ad->applet = applet;
		ad->cells = 1;
		ad->pos = pos;
		ad->drag_off = 0;
		ad->dirty = FALSE;
		ad->no_die = 0;
		gtk_object_set_data (GTK_OBJECT (applet),
				     PANEL_APPLET_DATA, ad);
		
		/*this is a completely new applet, which was not yet bound*/
		bind_top_applet_events (applet, bind_lower_events);
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

gboolean
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
	
	/*we'll resize both panels anyway*/
	ad->dirty = FALSE;
	
	ad->pos = pos;

	ad->no_die++;

	/*reparent applet*/
	if (IS_BUTTON_WIDGET (applet)) {
		ButtonWidget *button = BUTTON_WIDGET(applet);
		if (button->cache != NULL)
			gdk_pixmap_unref (button->cache);
		button->cache = NULL;
		gtk_widget_ref (applet);
		gtk_container_remove (GTK_CONTAINER (applet->parent), applet);
		gtk_container_add (GTK_CONTAINER (new_panel), applet);
		gtk_widget_unref (applet);
	} else 
		gtk_widget_reparent (applet, GTK_WIDGET (new_panel));

	gdk_flush();

	ad->no_die--;

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
	panel->applet_list = g_list_remove (panel->applet_list, ad);
	panel->no_window_applet_list =
		g_list_remove (panel->no_window_applet_list, ad);
	
	if(pos < 0)
		pos = 0;
	if(!panel->no_padding_on_ends && pos < pw_applet_padding)
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
			   int sz,
			   PanelBackType back_type,
			   char *pixmap,
			   gboolean fit_pixmap_bg,
			   gboolean strech_pixmap_bg,
			   gboolean rotate_pixmap_bg,
			   gboolean no_padding_on_ends,
			   GdkColor *back_color)
{
	PanelOrientation oldorient;
	int oldsz;
	gboolean change_back = FALSE;

	g_return_if_fail(panel!=NULL);
	g_return_if_fail(IS_PANEL_WIDGET(panel));
	g_return_if_fail(GTK_WIDGET_REALIZED(panel));

	oldorient = panel->orient;
	panel->orient = orient;

	oldsz = panel->sz;
	panel->sz = sz;
	
#ifdef PANEL_DEBUG
	printf("GOT SIZE OF %d\n",sz);
#endif

	kill_cache_on_all_buttons(panel, TRUE);
	send_draw_to_all_applets(panel);

	if(oldorient != panel->orient) {
	   	gtk_signal_emit(GTK_OBJECT(panel),
	   			panel_widget_signals[ORIENT_CHANGE_SIGNAL],
	   			panel->orient);
	}
	if(oldsz != panel->sz) {
	   	gtk_signal_emit(GTK_OBJECT(panel),
	   			panel_widget_signals[SIZE_CHANGE_SIGNAL],
	   			panel->sz);
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

		g_free (panel->back_pixmap);
		panel->back_pixmap = g_strdup (pixmap);
	}

	/*clearly a signal should be sent*/
	if(panel->back_type != back_type ||
	   panel->fit_pixmap_bg != fit_pixmap_bg ||
	   panel->strech_pixmap_bg != strech_pixmap_bg ||
	   panel->rotate_pixmap_bg != rotate_pixmap_bg)
		change_back = TRUE;
	
	/*this bit is not optimal, it allways sets the pixmap etc etc ...
	  but this function isn't called too often*/
	panel->back_type = back_type;
	panel->fit_pixmap_bg = fit_pixmap_bg;
	panel->strech_pixmap_bg = strech_pixmap_bg;
	panel->rotate_pixmap_bg = rotate_pixmap_bg;
	if(back_type == PANEL_BACK_PIXMAP) {
		panel_try_to_set_pixmap (panel, panel->back_pixmap);
		/* we will queue resize and redraw anyhow */
	} else if(back_type == PANEL_BACK_COLOR) {
		panel_try_to_set_back_color(panel, &panel->back_color);
	} else if(back_type == PANEL_BACK_TRANSLUCENT) {
		panel_try_to_set_translucent (panel);
	}

	/* let the applets know we changed the background */
	if (change_back) {
		gtk_signal_emit (GTK_OBJECT (panel),
				 panel_widget_signals[BACK_CHANGE_SIGNAL],
				 panel->back_type,
				 panel->back_pixmap,
				 &panel->back_color);
	}

	panel->no_padding_on_ends = no_padding_on_ends;

	/* inhibit draws until we resize */
	panel->inhibit_draw = TRUE;
	gtk_widget_queue_resize (GTK_WIDGET (panel));
}

void
panel_widget_change_orient(PanelWidget *panel,
			   PanelOrientation orient)
{
	panel_widget_change_params(panel,
				   orient,
				   panel->sz,
				   panel->back_type,
				   panel->back_pixmap,
				   panel->fit_pixmap_bg,
				   panel->strech_pixmap_bg,
				   panel->rotate_pixmap_bg,
				   panel->no_padding_on_ends,
				   &panel->back_color);
}


/*change global params*/
void
panel_widget_change_global (int explicit_step,
			    int auto_step,
			    int drawer_step,
			    int minimized_size,
			    int minimize_delay,
			    int maximize_delay,
			    PanelMovementType move_type,
			    gboolean disable_animations,
			    int applet_padding,
			    int applet_border_padding)
{
	if (explicit_step > 0)
		pw_explicit_step=explicit_step;
	if (auto_step > 0)
		pw_auto_step=auto_step;
	if (drawer_step > 0)
		pw_drawer_step=drawer_step;
	if (minimized_size > 0)
		pw_minimized_size=minimized_size;
	if (minimize_delay >= 0)
		pw_minimize_delay=minimize_delay;
	if (maximize_delay >= 0)
		pw_maximize_delay=maximize_delay;
	pw_movement_type = move_type;
	pw_disable_animations = disable_animations;

	/*change padding on all panels NOW*/
	if( (pw_applet_padding != applet_padding) ||
	    (pw_applet_border_padding != applet_border_padding)) {
		GSList *li;
		pw_applet_padding = applet_padding;
		pw_applet_border_padding = applet_border_padding;
		for (li = panels; li != NULL; li = li->next)
			gtk_widget_queue_resize (li->data);
	}
}

/* when we get color_only, we also optionally set r, g, b to the
   color and w, and h to the area if the background is one color
   only, otherwise normally return an rgb and set r, g, b to -1 */
void
panel_widget_get_applet_rgb_bg (PanelWidget *panel,
				GtkWidget *applet,
				guchar **rgb,
				int *w, int *h,
				int *rowstride,
				gboolean color_only,
				int *r, int *g, int *b)
{
	GtkWidget *widget;

	GdkPixbuf *pb = NULL;

	int scale_w = 0,scale_h = 0;
	int rotate = FALSE;
	
	*rgb = NULL;
	*w = *h = *rowstride = 0;
	*r = *g = *b = -1;

	g_return_if_fail (panel != NULL);
	g_return_if_fail (IS_PANEL_WIDGET (panel));
	g_return_if_fail (applet != NULL);
	g_return_if_fail (GTK_IS_WIDGET (applet));

	widget = GTK_WIDGET (panel);

	if( ! GTK_WIDGET_DRAWABLE (widget) ||
	   widget->allocation.width <= 0 ||
	   widget->allocation.height <= 0)
		return;

	setup_background (panel, &pb, &scale_w, &scale_h, &rotate);
	
	*w = applet->allocation.width;
	*h = applet->allocation.height;
	
	if (color_only &&
	    pb == NULL) {
		if(panel->back_type != PANEL_BACK_COLOR) {
			GtkWidget *widget = GTK_WIDGET(panel);
			/* convert to 8 bit per channel */
			*r = widget->style->bg[GTK_WIDGET_STATE(widget)].red>>8;
			*g = widget->style->bg[GTK_WIDGET_STATE(widget)].green>>8;
			*b = widget->style->bg[GTK_WIDGET_STATE(widget)].blue>>8;
		} else {
			/* convert to 8 bit per channel */
			*r = panel->back_color.red>>8;
			*g = panel->back_color.green>>8;
			*b = panel->back_color.blue>>8;
		}
		return;
	}
	*rowstride = applet->allocation.width * 3;

	*rgb = g_new0(guchar,
		      (*h) * (*rowstride));

	make_background (panel, *rgb,
			 applet->allocation.x,
			 applet->allocation.y,
			 *w, *h, pb, scale_w, scale_h, rotate);
}
