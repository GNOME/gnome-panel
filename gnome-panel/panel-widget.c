#include <gtk/gtk.h>
#include <gnome.h>
#include "panel-widget.h"
#include "panel-util.h"
#include "gdkextra.h"

GList *panels=NULL; /*other panels we might want to move the applet to*/

/*there  can universally be only one applet being dragged since we assume
we only have one mouse :) */
gint panel_applet_in_drag = FALSE;

static void panel_widget_class_init	(PanelWidgetClass *klass);
static void panel_widget_init		(PanelWidget      *panel_widget);
static int  panel_try_to_set_pixmap     (PanelWidget *panel, char *pixmap);

static GdkCursor *fleur_cursor;

/*global settings*/
static gint pw_explicit_step = 50;
static gint pw_drawer_step = 20;
static gint pw_auto_step = 10;
static gint pw_minimized_size = 6;
static gint pw_minimize_delay = 300;
static gint pw_disable_animations = FALSE;
static PanelMovementType pw_movement_type = PANEL_SWITCH_MOVE;

/* FIXME: real DND is disabled, needs fixing(??), or might be left for dead
   really ... if an applet wants to do dnd it will do it on it's own and
   the panel will do interpanel drags differently
static char *applet_drop_types[]={"internal/applet-widget-pointer"};
*/
static char *image_drop_types[] = {"url:ALL", "application/x-color"};

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

/*
#define DRAG_BUFFER 50
*/

typedef void (*PanelWidgetOrientSignal) (GtkObject * object,
					 PanelOrientation orient,
					 PanelSnapped snapped,
					 gpointer data);

typedef void (*PanelWidgetStateSignal) (GtkObject * object,
				        PanelState state,
				        gpointer data);

typedef void (*PanelWidgetAppletSignal) (GtkObject * object,
				         GtkWidget * applet,
				         gpointer data);

/************************
 debugging
 ************************/
static void
debug_dump_panel_list(PanelWidget *panel)
{
	GList *list;
	puts("\nDUMP START\n");
	for(list = panel->applet_list;list!=NULL;list=g_list_next(list)) {
		AppletData *ad = list->data;
		printf("pos: %d cells: %d\n",ad->pos,ad->cells);
	}
	puts("\nDUMP END\n");
}

/************************
 convenience functions
 ************************/

static gint
applet_data_compare(AppletData *ad1, AppletData *ad2)
{
	return ad1->pos - ad2->pos;
}

static GList *
my_g_list_swap_next(GList *list, GList *dl)
{
	GList *t;

	if(!dl->next)
		return list;
	if(dl->prev)
		dl->prev->next = dl->next;
	t = dl->prev;
	dl->prev = dl->next;
	dl->next->prev = t;
	if(dl->next->next)
		dl->next->next->prev = dl;
	t = dl->next->next;
	dl->next->next = dl;
	dl->next = t;

	if(list == dl)
		return dl->prev;
	return list;
}

static GList *
my_g_list_swap_prev(GList *list, GList *dl)
{
	GList *t;

	if(!dl->prev)
		return list;
	if(dl->next)
		dl->next->prev = dl->prev;
	t = dl->next;
	dl->next = dl->prev;
	dl->prev->next = t;
	if(dl->prev->prev)
		dl->prev->prev->next = dl;
	t = dl->prev->prev;
	dl->prev->prev = dl;
	dl->prev = t;

	if(list == dl->next)
		return dl;
	return list;
}

/*maybe this should be a glib function?*/
static GList *
my_g_list_resort(GList *list, gpointer data, GCompareFunc func)
{
	GList *dl;

	if(!list)
		return NULL;

	dl = g_list_find(list,data);

	g_return_val_if_fail(dl!=NULL,list);

	while(dl->next &&
	      (*func)(dl->data,dl->next->data)>0)
		list=my_g_list_swap_next(list,dl);
	while(dl->prev &&
	      (*func)(dl->data,dl->prev->data)<0)
		list=my_g_list_swap_prev(list,dl);
	return list;
}

/*this is used to do an immediate move instead of set_uposition, which
queues one*/
void
move_resize_window(GtkWidget *widget, int x, int y, int w, int h)
{
	/*printf("%d x %d x %d x %d\n",x,y,w,h);*/
	gdk_window_set_hints(widget->window, x, y, w, h, w, h,
			     GDK_HINT_POS|GDK_HINT_MIN_SIZE|GDK_HINT_MAX_SIZE);
	gdk_window_move_resize(widget->window, x, y, w, h);
	/* FIXME: this should draw only the newly exposed area! */
	gtk_widget_draw(widget, NULL);
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

		panel_widget_type = gtk_type_unique (gtk_window_get_type (), &panel_widget_info);
	}

	return panel_widget_type;
}

enum {
	ORIENT_CHANGE_SIGNAL,
	STATE_CHANGE_SIGNAL,
	APPLET_MOVE_SIGNAL,
	APPLET_ADDED_SIGNAL,
	APPLET_REMOVED_SIGNAL,
	LAST_SIGNAL
};

static gint panel_widget_signals[LAST_SIGNAL] = {0,0,0};

static void
gtk_panel_widget_marshal_signal_state (GtkObject * object,
				       GtkSignalFunc func,
				       gpointer func_data,
				       GtkArg * args)
{
	PanelWidgetStateSignal rfunc;

	rfunc = (PanelWidgetStateSignal) func;

	(*rfunc) (object, GTK_VALUE_ENUM (args[0]),
		  func_data);
}

static void
gtk_panel_widget_marshal_signal_orient (GtkObject * object,
					GtkSignalFunc func,
					gpointer func_data,
					GtkArg * args)
{
	PanelWidgetOrientSignal rfunc;

	rfunc = (PanelWidgetOrientSignal) func;

	(*rfunc) (object, GTK_VALUE_ENUM (args[0]),
		  GTK_VALUE_ENUM (args[1]),
		  func_data);
}

static void
gtk_panel_widget_marshal_signal_applet (GtkObject * object,
					GtkSignalFunc func,
					gpointer func_data,
					GtkArg * args)
{
	PanelWidgetAppletSignal rfunc;

	rfunc = (PanelWidgetAppletSignal) func;

	(*rfunc) (object, GTK_VALUE_POINTER (args[0]),
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
			       gtk_panel_widget_marshal_signal_orient,
			       GTK_TYPE_NONE,
			       2,
			       GTK_TYPE_ENUM,
			       GTK_TYPE_ENUM);
	panel_widget_signals[STATE_CHANGE_SIGNAL] =
		gtk_signal_new("state_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 state_change),
			       gtk_panel_widget_marshal_signal_state,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_ENUM);
	panel_widget_signals[APPLET_MOVE_SIGNAL] =
		gtk_signal_new("applet_move",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 applet_move),
			       gtk_panel_widget_marshal_signal_applet,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_POINTER);
	panel_widget_signals[APPLET_ADDED_SIGNAL] =
		gtk_signal_new("applet_added",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 applet_added),
			       gtk_panel_widget_marshal_signal_applet,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_POINTER);
	panel_widget_signals[APPLET_REMOVED_SIGNAL] =
		gtk_signal_new("applet_removed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(PanelWidgetClass,
			       			 applet_removed),
			       gtk_panel_widget_marshal_signal_applet,
			       GTK_TYPE_NONE,
			       0);

	gtk_object_class_add_signals(object_class,panel_widget_signals,
				     LAST_SIGNAL);

	class->orient_change = NULL;
	class->state_change = NULL;
	class->applet_move = NULL;
	class->applet_added = NULL;
	class->applet_removed = NULL;
}

static void
panel_widget_init (PanelWidget *panel_widget)
{
	panel_widget->orient= PANEL_HORIZONTAL;
	panel_widget->snapped = PANEL_BOTTOM;
	panel_widget->mode = PANEL_EXPLICIT_HIDE;
	panel_widget->state = PANEL_SHOWN;
	panel_widget->drawer_drop_zone_pos = DROP_ZONE_LEFT;
}


/*this is only for snapped panels! it sets their position, the positions
of drawer and free are handeled by the app not panel-widget*/
static void
panel_widget_set_position(PanelWidget *panel)
{
	gint xcor = 0;
	gint ycor = 0;
	gint x,y;
	gint newx,newy;
	gint width,height;
	gint thick;

	if(panel->snapped == PANEL_FREE ||
	   panel->snapped == PANEL_DRAWER ||
	   panel->state == PANEL_MOVING)
		return;

	if(GTK_WIDGET(panel)->window)
		gdk_window_get_geometry(GTK_WIDGET(panel)->window,&x,&y,
					&width,&height,NULL);
	else
		x=y=-1;
	newx = x;
	newy = y;

	if(panel->orient == PANEL_VERTICAL)
		thick = width;
	else
		thick = height;


	if(panel->mode == PANEL_AUTO_HIDE && panel->state == PANEL_HIDDEN)
		ycor = thick - pw_minimized_size;
	if(panel->orient == PANEL_VERTICAL) {
		if(panel->state == PANEL_HIDDEN_LEFT)
			xcor = - gdk_screen_height() +
			       panel->hidebutton_s->allocation.height;
		else if(panel->state == PANEL_HIDDEN_RIGHT)
			xcor = gdk_screen_height() -
			       panel->hidebutton_n->allocation.height;
	} else if(panel->orient == PANEL_HORIZONTAL) {
		if(panel->state == PANEL_HIDDEN_LEFT)
			xcor = - gdk_screen_width() +
			       panel->hidebutton_w->allocation.width;
		else if(panel->state == PANEL_HIDDEN_RIGHT)
			xcor = gdk_screen_width() -
			       panel->hidebutton_w->allocation.width;
	}

	switch(panel->snapped) {
		case PANEL_TOP:
			newx = xcor;
			newy = -ycor;
			break;
		case PANEL_BOTTOM:
			newx = xcor;
			newy = gdk_screen_height() - thick + ycor;
			break;
		case PANEL_LEFT:
			newx = -ycor;
			newy = xcor;
			break;
		case PANEL_RIGHT:
			newx = gdk_screen_width() - thick + ycor;
			newy = xcor;
			break;
		default: break; /*to get rid of a warning*/
	}
	if(newx != x || newy != y)
		gtk_widget_set_uposition(GTK_WIDGET(panel),newx,newy);
}

static void
panel_widget_set_drop_zone(PanelWidget *panel)
{
	if(panel->orient == PANEL_HORIZONTAL) {
		if(panel->drawer_drop_zone_pos == DROP_ZONE_LEFT) {
			gtk_widget_hide(panel->hidebutton_n);
			gtk_widget_show(panel->hidebutton_e);
			gtk_widget_hide(panel->hidebutton_w);
			gtk_widget_hide(panel->hidebutton_s);
		} else {
			gtk_widget_hide(panel->hidebutton_n);
			gtk_widget_hide(panel->hidebutton_e);
			gtk_widget_show(panel->hidebutton_w);
			gtk_widget_hide(panel->hidebutton_s);
		}
	} else {
		if(panel->drawer_drop_zone_pos == DROP_ZONE_LEFT) {
			gtk_widget_show(panel->hidebutton_n);
			gtk_widget_hide(panel->hidebutton_e);
			gtk_widget_hide(panel->hidebutton_w);
			gtk_widget_hide(panel->hidebutton_s);
		} else {
			gtk_widget_hide(panel->hidebutton_n);
			gtk_widget_hide(panel->hidebutton_e);
			gtk_widget_hide(panel->hidebutton_w);
			gtk_widget_show(panel->hidebutton_s);
		}
	}
}

static void
panel_widget_set_size(PanelWidget *panel, gint size)
{
	GList *list;

	if(size == 0)
		size = panel->size;
	switch(panel->snapped) {
		case PANEL_DRAWER:
		case PANEL_FREE:
			if(panel->orient == PANEL_HORIZONTAL)
				gtk_widget_set_usize(GTK_WIDGET(panel->fixed),
						     size*PANEL_CELL_SIZE,
						     panel->thick);
			else
				gtk_widget_set_usize(GTK_WIDGET(panel->fixed),
						     panel->thick,
						     size*PANEL_CELL_SIZE);
			break;
		case PANEL_TOP:
		case PANEL_BOTTOM:
			panel->orient = PANEL_HORIZONTAL;
			gtk_widget_set_usize(GTK_WIDGET(panel->fixed),
					     0,
					     panel->thick);
			gtk_widget_set_usize(GTK_WIDGET(panel),
					     gdk_screen_width(),
					     0);
			break;
		case PANEL_LEFT:
		case PANEL_RIGHT:
			panel->orient = PANEL_VERTICAL;
			gtk_widget_set_usize(GTK_WIDGET(panel->fixed),
					     panel->thick,
					     0);
			gtk_widget_set_usize(GTK_WIDGET(panel),
					     0,
					     gdk_screen_height());
			break;
	}
	for(list = panel->applet_list;list!=NULL;list=g_list_next(list)) {
		AppletData *ad = list->data;
		/*postion now unknown*/
		ad->prevx = -1;
		ad->prevy = -1;
		ad->prevwidth = -1;
		ad->prevheight = -1;
	}
}

/*get the number of applets*/
gint
panel_widget_get_applet_count(PanelWidget *panel)
{
	gint i;
	GList *list;

	for(i=0,list=panel->applet_list;list!=NULL;list=g_list_next(list),i++)
		;
	return i;
}

static AppletData *
get_applet_data_pos(PanelWidget *panel, gint pos)
{
	GList *list;

	for(list=panel->applet_list;list!=NULL;list=g_list_next(list)) {
		AppletData *ad = list->data;
		if(ad->pos <= pos && ad->pos+ad->cells > pos)
			return ad;
	}
	return NULL;
}

/*temporairly using _, we will switch over to this function completely
  in time*/
static void
panel_widget_applet_put(PanelWidget *panel,AppletData *ad)
{
	gint width, height;
	gint x,y;
	gint oldx,oldy;

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

	if(oldx!=x || oldy!=y) {
		gtk_fixed_move(GTK_FIXED(panel->fixed),ad->applet,x,y);

		/*postion now unknown*/
		ad->prevx = -1;
		ad->prevy = -1;
		ad->prevwidth = -1;
		ad->prevheight = -1;
	}
}

static void
panel_widget_put_all(PanelWidget *panel)
{
	GList *list;

	for(list=panel->applet_list;list!=NULL;list=g_list_next(list))
		panel_widget_applet_put(panel,list->data);
}

static void
panel_widget_pack_applets(PanelWidget *panel)
{
	gint i;
	GList *list;

	for(list=panel->applet_list,i=0;list!=NULL;list=g_list_next(list)) {
		AppletData *ad = list->data;
		ad->pos = i;
		i+=ad->cells;
	}

	panel->size = i;

	panel_widget_put_all(panel);
}


static void
panel_widget_shrink_wrap(PanelWidget *panel,
			 gint width,
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
}

/*this is a special function and may fail if called improperly, it works
only under special circumstance when we know there is nothing from
old_size to panel->size*/
static void
panel_widget_right_stick(PanelWidget *panel,gint old_size)
{
	gint i,pos;
	GList *list,*prev;
	AppletData *ad;

	if(old_size>=panel->size ||
	   panel->snapped == PANEL_DRAWER)
	   	return;

	ad = get_applet_data_pos(panel,old_size-1);

	if(!ad)
		return;

	list = g_list_find(panel->applet_list,ad);

	g_return_if_fail(list!=NULL);

	pos = panel->size-1;

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
		panel_widget_applet_put(panel,list->data);
}

static gint
panel_widget_push_left(PanelWidget *panel,AppletData *oad)
{
	gint i;
	GList *list,*prev;
	AppletData *ad;

	if(!panel->applet_list)
		return FALSE;

	g_return_val_if_fail(panel->snapped != PANEL_DRAWER,FALSE);

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

		panel_widget_applet_put(panel,ad);
		if(ad == oad)
			break;
	}

	return TRUE;
}

static gint
panel_widget_push_right(PanelWidget *panel,AppletData *oad)
{
	gint i;
	GList *list,*prev;
	AppletData *ad;

	if(!panel->applet_list) {
		if(panel->snapped == PANEL_DRAWER) {
			panel_widget_set_size(panel,++panel->size);
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
		if(panel->snapped == PANEL_DRAWER)
			panel_widget_set_size(panel,++panel->size);
		else
			return FALSE;
	}
	for(list=prev;list!=NULL;list=g_list_previous(list)) {
		ad=list->data;
		ad->pos++;

		panel_widget_applet_put(panel,ad);
		if(ad == oad)
			break;
	}

	return TRUE;
}

static void
panel_widget_seize_space(PanelWidget *panel,
			 gint width,
			 AppletData *ad)
{
	GList *list,*rlist,*llist;

	if(width%PANEL_CELL_SIZE) width--; /*just so that I get
					     the right size*/
	/*convert width from pixels to cells*/
	width = (width/PANEL_CELL_SIZE) + 1;

	if(panel->snapped == PANEL_DRAWER)
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
		if(panel->snapped == PANEL_DRAWER ||
		   ad->pos+width-1 < panel->size) {
			ad->cells = width;
			if(ad->pos+width>panel->size) {
				panel->size = ad->pos+width;
				panel_widget_set_size(panel,panel->size);
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
			panel_widget_applet_put(panel,ad);
			return;
		}
		ad->cells += ad->pos-(wad->pos+wad->cells);
		ad->pos = wad->pos+wad->cells;
	} else {
		if(0 <= ad->pos+ad->cells-width) {
			ad->pos = ad->pos+ad->cells-width;
			ad->cells = width;
			panel_widget_applet_put(panel,ad);
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

	if(llist && panel->snapped!=PANEL_DRAWER) {
		AppletData *wad = llist->data;
		while(ad->cells<width && panel_widget_push_left(panel,wad)) {
			ad->cells++;
			ad->pos--;
		}
	}

	panel_widget_applet_put(panel,ad);
}

static void
panel_widget_adjust_applet(PanelWidget *panel, AppletData *ad)
{
	gint width, height;

	g_return_if_fail(ad!=NULL);

	gdk_window_get_size(ad->applet->window,&width,&height);

	/*don't adjust applets out of range, wait for
	  then to be pushed into range*/
	if(panel->snapped != PANEL_DRAWER && ad->pos>=panel->size)
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
	panel_widget_applet_put(panel,ad);
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
		panel_widget_applet_put(panel,ad);
		return;
	}
	nad->pos = ad->pos;
	ad->pos = nad->pos+nad->cells;
	panel->applet_list = my_g_list_swap_prev(panel->applet_list,nlist);

	panel_widget_applet_put(panel,ad);
	panel_widget_applet_put(panel,nad);
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
		panel_widget_applet_put(panel,ad);
		return;
	}
	ad->pos = pad->pos;
	pad->pos = ad->pos+ad->cells;
	panel->applet_list = my_g_list_swap_next(panel->applet_list,nlist);

	panel_widget_applet_put(panel,ad);
	panel_widget_applet_put(panel,pad);
}

static gint
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

static gint
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
panel_widget_switch_move(PanelWidget *panel, AppletData *ad, gint moveby)
{
	gint finalpos;
	gint pos;
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

static gint
panel_widget_get_thick(PanelWidget *panel)
{
	GList *list;
	gint thick=0;

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



static gint
panel_widget_applet_size_allocate (GtkWidget *widget,
				   GdkEvent *event,
				   gpointer data)
{
	PanelWidget *panel;
	AppletData *ad;
	gint thick;

	panel = gtk_object_get_data(GTK_OBJECT(widget),PANEL_APPLET_PARENT_KEY);
	ad = gtk_object_get_data(GTK_OBJECT(widget),PANEL_APPLET_DATA);

	if(ad->pos == -1)
		return TRUE;

	if(ad->prevwidth!=widget->allocation.width ||
	   ad->prevheight!=widget->allocation.height||
	   ad->prevx!=widget->allocation.x ||
	   ad->prevy!=widget->allocation.y) {
		thick = panel_widget_get_thick(panel);
		if(panel->thick != thick) {
			panel->thick = thick;
			panel_widget_set_size(panel, panel->size);
		}
		panel_widget_adjust_applet(panel,ad);

		gtk_signal_emit(GTK_OBJECT(panel),
				panel_widget_signals[APPLET_MOVE_SIGNAL],
				widget);
		ad->prevwidth = widget->allocation.width;
		ad->prevheight = widget->allocation.height;
		ad->prevx = widget->allocation.x;
		ad->prevy = widget->allocation.y;
	}

	return TRUE;
}

static gint
move_step(gint src, gint dest, gint pos, gint step)
{
	gint range = abs(src-dest);
	gint diff = abs(range-abs(pos-src));
	gint percentage = (diff*100)/range;

	if(percentage>50)
		percentage = 100-percentage;

	return ((step>>1)*log((percentage/10.0)+1))+1;
}

static void
move_horiz_d(PanelWidget *panel, gint src_x, gint dest_x, gint step, gint hide)
{
	gint orig_x, x, y;
	gint orig_w, w, h;

	x = panel->x;
	y = panel->y;
	gdk_window_get_size(GTK_WIDGET(panel)->window,&w,&h);
	if(!hide)
		w = 0;

	orig_x = x;
	orig_w = w;

	if (!pw_disable_animations && step != 0) {
		if (src_x < dest_x) {
			for( x = src_x; x < dest_x;
			     x+= move_step(src_x,dest_x,x,step)) {
				if(hide) {
					move_resize_window(GTK_WIDGET(panel),
							   x, y, w, h);
					w-=move_step(src_x,dest_x,x,step);
				} else {
					move_resize_window(GTK_WIDGET(panel),
							   orig_x, y, w, h);
					w+=move_step(src_x,dest_x,x,step);
				}
			}
		} else {
			for (x = src_x; x > dest_x;
			     x-= move_step(src_x,dest_x,x,step)) {
				if(hide) {
					move_resize_window(GTK_WIDGET(panel),
							   orig_x, y, w, h);
					w-=move_step(src_x,dest_x,x,step);
				} else {
					move_resize_window(GTK_WIDGET(panel),
							   x, y, w, h);
					w+=move_step(src_x,dest_x,x,step);
				}
			}
		}
	}
	
	if(hide)
		w = orig_w - abs(src_x-dest_x);
	else
		w = orig_w + abs(src_x-dest_x);

	move_resize_window(GTK_WIDGET(panel), dest_x, panel->y,w,h);
}

static void
move_vert_d(PanelWidget *panel, gint src_y, gint dest_y, gint step, gint hide)
{
	gint orig_y, x, y;
	gint orig_h, w, h;

	x = panel->x;
	y = panel->y;
	gdk_window_get_size(GTK_WIDGET(panel)->window,&w,&h);
	if(!hide)
		h = 0;

	orig_y = y;
	orig_h = h;

	if (!pw_disable_animations && step != 0) {
		if (src_y < dest_y) {
			for( y = src_y; y < dest_y;
			     y+= move_step(src_y,dest_y,y,step)) {
				if(hide) {
					move_resize_window(GTK_WIDGET(panel),
							   x, y, w, h);
					h-=move_step(src_y,dest_y,y,step);
				} else {
					move_resize_window(GTK_WIDGET(panel),
							   x, orig_y, w, h);
					h+=move_step(src_y,dest_y,y,step);
				}
			}
		} else {
			for (y = src_y; y > dest_y;
			     y-= move_step(src_y,dest_y,y,step)) {
				if(hide) {
					move_resize_window(GTK_WIDGET(panel),
							   x, orig_y, w, h);
					h-=move_step(src_y,dest_y,y,step);
				} else {
					move_resize_window(GTK_WIDGET(panel),
							   x, y, w, h);
					h+=move_step(src_y,dest_y,y,step);
				}
			}
		}
	}
	
	if(hide)
		h = orig_h - abs(src_y-dest_y);
	else
		h = orig_h + abs(src_y-dest_y);

	move_resize_window(GTK_WIDGET(panel), panel->x, dest_y, w,h);
}

static void
move_horiz(PanelWidget *panel, gint src_x, gint dest_x, gint step)
{
	gint x, y;

	gdk_window_get_position(GTK_WIDGET(panel)->window,&x,&y);

	if (!pw_disable_animations && step != 0) {
		if (src_x < dest_x) {
			for( x = src_x; x < dest_x;
			     x+= move_step(src_x,dest_x,x,step))
				move_window(GTK_WIDGET(panel),x,y);
		} else {
			for (x = src_x; x > dest_x;
			     x-= move_step(src_x,dest_x,x,step))
				move_window(GTK_WIDGET(panel),x,y);
		}
	}
	
	move_window(GTK_WIDGET(panel), dest_x, y);
}


static void
move_vert(PanelWidget *panel, gint src_y, gint dest_y, gint step)
{
	gint x, y;

	gdk_window_get_position(GTK_WIDGET(panel)->window,&x,&y);

	if (!pw_disable_animations && step != 0) {
		if (src_y < dest_y) {
                        for (y = src_y; y < dest_y;
			     y+= move_step(src_y,dest_y,y,step))
				move_window(GTK_WIDGET(panel),x,y);
		} else {
                        for (y = src_y; y > dest_y;
			     y-= move_step(src_y,dest_y,y,step))
				move_window(GTK_WIDGET(panel),x,y);
		}
	}

	move_window(GTK_WIDGET(panel), x, dest_y);
}



void
panel_widget_pop_up(PanelWidget *panel)
{
	int width, height;
	int swidth, sheight;

	if ((panel->state == PANEL_MOVING) ||
	    (panel->state == PANEL_SHOWN) ||
	    (panel->snapped == PANEL_DRAWER) ||
	    (panel->snapped == PANEL_FREE))
		return;

	panel->state = PANEL_MOVING;

	width   = GTK_WIDGET(panel)->allocation.width;
	height  = GTK_WIDGET(panel)->allocation.height;
	swidth  = gdk_screen_width();
	sheight = gdk_screen_height();

	switch (panel->snapped) {
		case PANEL_TOP:
		        move_vert(panel, -height + pw_minimized_size, 0,
				  pw_auto_step);
			break;

		case PANEL_BOTTOM:
			move_vert(panel, sheight - pw_minimized_size, 
				  sheight - height,pw_auto_step);
			break;

		case PANEL_LEFT:
			move_horiz(panel, -width + pw_minimized_size, 0,
				   pw_auto_step);
			break;

		case PANEL_RIGHT:
			move_horiz(panel, swidth - pw_minimized_size, 
				   swidth - width,pw_auto_step);
			break;
		default: break; /*to get rid of a warning*/
	}

	panel->state = PANEL_SHOWN;

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[STATE_CHANGE_SIGNAL],
			PANEL_SHOWN);
}

static gint
panel_widget_pop_down(gpointer data)
{
	PanelWidget *panel = data;
	int width, height;
	int swidth, sheight;

	if((panel->state != PANEL_SHOWN) ||
	   (panel->snapped == PANEL_DRAWER) ||
	   (panel->snapped == PANEL_FREE))
		return FALSE;
	/*we are moving, or have drawers open, so wait with the
	  pop_down*/
	if(panel->currently_dragged_applet || panel->drawers_open>0)
		return TRUE;

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[STATE_CHANGE_SIGNAL],
			PANEL_HIDDEN);

	panel->state = PANEL_MOVING;

	width   = GTK_WIDGET(panel)->allocation.width;
	height  = GTK_WIDGET(panel)->allocation.height;
	swidth  = gdk_screen_width();
	sheight = gdk_screen_height();

	switch (panel->snapped) {
		case PANEL_TOP:
			move_vert(panel, 0, -height + pw_minimized_size,
				  pw_auto_step);
			break;

		case PANEL_BOTTOM:
			move_vert(panel, sheight - height, 
				  sheight - pw_minimized_size,
				  pw_auto_step);
			break;

		case PANEL_LEFT:
			move_horiz(panel, 0, -width + pw_minimized_size,
				   pw_auto_step);
			break;

		case PANEL_RIGHT:
			move_horiz(panel, swidth - width, 
				   swidth - pw_minimized_size,
				   pw_auto_step);
			break;
		default: break; /*to get rid of a warning*/
	}

	panel->state = PANEL_HIDDEN;

	panel->leave_notify_timer_tag = 0;
  
	return FALSE;
}


static void
panel_widget_pop_show(PanelWidget *panel, int fromright)
{
	int width, height;

	if ((panel->state == PANEL_MOVING) ||
	    (panel->state == PANEL_SHOWN) ||
	    (panel->snapped == PANEL_DRAWER) ||
	    (panel->snapped == PANEL_FREE))
		return;

	panel->state = PANEL_MOVING;

	width   = GTK_WIDGET(panel)->allocation.width;
	height  = GTK_WIDGET(panel)->allocation.height;

	if(panel->orient == PANEL_HORIZONTAL) {
		if(fromright)
			move_horiz(panel, -width +
				   panel->hidebutton_w->allocation.width, 0,
				   pw_explicit_step);
		else
			move_horiz(panel, width -
				   panel->hidebutton_e->allocation.width, 0,
				   pw_explicit_step);
	} else {
		if(fromright)
			move_vert(panel, -height +
				  panel->hidebutton_s->allocation.height, 0,
				  pw_explicit_step);
		else
			move_vert(panel, height -
				  panel->hidebutton_n->allocation.height, 0,
				  pw_explicit_step);
	}

	panel->state = PANEL_SHOWN;

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[STATE_CHANGE_SIGNAL],
			PANEL_SHOWN);
}

static void
panel_widget_pop_hide(PanelWidget *panel, int fromright)
{
	int width, height;

	if((panel->state != PANEL_SHOWN) ||
	   (panel->snapped == PANEL_DRAWER) ||
	   (panel->snapped == PANEL_FREE))
		return;

	if (panel->leave_notify_timer_tag != 0) {
		gtk_timeout_remove (panel->leave_notify_timer_tag);
		panel->leave_notify_timer_tag = 0;
	}

	if(fromright)
	   	gtk_signal_emit(GTK_OBJECT(panel),
	   			panel_widget_signals[STATE_CHANGE_SIGNAL],
	   			PANEL_HIDDEN_LEFT);
	else
	   	gtk_signal_emit(GTK_OBJECT(panel),
	   			panel_widget_signals[STATE_CHANGE_SIGNAL],
	   			PANEL_HIDDEN_RIGHT);

	panel->state = PANEL_MOVING;

	width   = GTK_WIDGET(panel)->allocation.width;
	height  = GTK_WIDGET(panel)->allocation.height;

	if(panel->orient == PANEL_HORIZONTAL) {
		if(fromright)
			move_horiz(panel, 0, -width +
				   panel->hidebutton_w->allocation.width,
				   pw_explicit_step);
		else
			move_horiz(panel, 0, width -
				   panel->hidebutton_e->allocation.width,
				   pw_explicit_step);
	} else {
		if(fromright)
			move_vert(panel, 0, -height +
				  panel->hidebutton_s->allocation.height,
				  pw_explicit_step);
		else
			move_vert(panel, 0, height -
				  panel->hidebutton_n->allocation.height,
				  pw_explicit_step);
	}

	if(fromright)
		panel->state = PANEL_HIDDEN_LEFT;
	else
		panel->state = PANEL_HIDDEN_RIGHT;
}


void
panel_widget_open_drawer(PanelWidget *panel)
{
	gint x,y;
	gint width, height;

	if((panel->state == PANEL_SHOWN) ||
	   (panel->snapped != PANEL_DRAWER) ||
	   (panel->state == PANEL_MOVING))
		return;

	panel->state = PANEL_MOVING;

	width   = GTK_WIDGET(panel)->allocation.width;
	height  = GTK_WIDGET(panel)->allocation.height;
	x = panel->x;
	y = panel->y;

	gdk_window_move(GTK_WIDGET(panel)->window, -1000,-1000);
	gtk_widget_show(GTK_WIDGET(panel));
	if(panel->orient == PANEL_HORIZONTAL) {
		if(panel->drawer_drop_zone_pos==DROP_ZONE_LEFT) {
			move_resize_window(GTK_WIDGET(panel),x+width,y,
					   0,height);
			move_horiz_d(panel, x+width, x,
				     pw_drawer_step,FALSE);
		} else {
			move_resize_window(GTK_WIDGET(panel),x-width,y,
					   0,height);
			move_horiz_d(panel, x-width, x,
				     pw_drawer_step,FALSE);
		}
	} else {
		if(panel->drawer_drop_zone_pos==DROP_ZONE_LEFT) {
			move_resize_window(GTK_WIDGET(panel),x,y+height,
					   width,0);
			move_vert_d(panel, y+height, y,
				  pw_drawer_step,FALSE);
		} else {
			move_resize_window(GTK_WIDGET(panel),x,y-height,
					   width,0);
			move_vert_d(panel, y-height, y,
				    pw_drawer_step,FALSE);
		}
	}
	/*move_resize_window(GTK_WIDGET(panel),x,y,width,height);*/

	panel->state = PANEL_SHOWN;

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[STATE_CHANGE_SIGNAL],
			PANEL_SHOWN);

}

void
panel_widget_close_drawer(PanelWidget *panel)
{
	gint x,y;
	gint width, height;

	if((panel->state != PANEL_SHOWN) ||
	   (panel->snapped != PANEL_DRAWER) ||
	   (panel->state == PANEL_MOVING))
		return;

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[STATE_CHANGE_SIGNAL],
			PANEL_HIDDEN);

	panel->state = PANEL_MOVING;

	width   = GTK_WIDGET(panel)->allocation.width;
	height  = GTK_WIDGET(panel)->allocation.height;
	gdk_window_get_position(GTK_WIDGET(panel)->window,&x,&y);

	if(!pw_disable_animations) {
		if(panel->orient == PANEL_HORIZONTAL) {
			if(panel->drawer_drop_zone_pos==DROP_ZONE_LEFT)
				move_horiz_d(panel, x, x+width,
					     pw_drawer_step, TRUE);
			else
				move_horiz_d(panel, x, x-width,
					     pw_drawer_step, TRUE);
		} else {
			if(panel->drawer_drop_zone_pos==DROP_ZONE_LEFT)
				move_vert_d(panel, y, y+height,
					    pw_drawer_step, TRUE);
			else
				move_vert_d(panel, y, y-height,
					    pw_drawer_step, TRUE);
		}
	}

	gtk_widget_hide(GTK_WIDGET(panel));

	move_window(GTK_WIDGET(panel),x,y);

	panel->state = PANEL_HIDDEN;

	move_resize_window(GTK_WIDGET(panel),x,y,width,height);
}



static gint
panel_show_hide_right(GtkWidget *widget, gpointer data)
{
	PanelWidget *panel = data;
	if(panel->state == PANEL_MOVING) 
		return FALSE;
	else if(panel->state == PANEL_SHOWN)
		panel_widget_pop_hide(panel,TRUE);
	else
		panel_widget_pop_show(panel,FALSE);
	return FALSE;
}

static gint
panel_show_hide_left(GtkWidget *widget, gpointer data)
{
	PanelWidget *panel = data;
	if(panel->state == PANEL_MOVING) 
		return FALSE;
	else if(panel->state == PANEL_SHOWN)
		panel_widget_pop_hide(panel,FALSE);
	else
		panel_widget_pop_show(panel,TRUE);
	return FALSE;
}

static gint
panel_enter_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	PanelWidget *panel=data;
	/*FIXME: do we want this autoraise piece?*/
	gdk_window_raise(GTK_WIDGET(panel)->window);

	if ((panel->mode == PANEL_EXPLICIT_HIDE) ||
	    (event->detail == GDK_NOTIFY_INFERIOR) ||
	    (panel->state == PANEL_HIDDEN_LEFT) ||
	    (panel->state == PANEL_HIDDEN_RIGHT))
		return FALSE;

	if (panel->leave_notify_timer_tag != 0) {
		gtk_timeout_remove (panel->leave_notify_timer_tag);
		panel->leave_notify_timer_tag = 0;
	}
 
	panel_widget_pop_up(panel);
	
	return FALSE;
}


static gint
panel_leave_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	PanelWidget *panel = data;

	if ((panel->mode == PANEL_EXPLICIT_HIDE) ||
	    (event->detail == GDK_NOTIFY_INFERIOR) ||
	    (panel->state == PANEL_HIDDEN_LEFT) ||
	    (panel->state == PANEL_HIDDEN_RIGHT))
		return FALSE;
	
	/* check if there's already a timeout set, and delete it if 
	 * there was */
	if (panel->leave_notify_timer_tag != 0) {
		gtk_timeout_remove (panel->leave_notify_timer_tag);
	}
	
	/* set up our delay for popup. */
	panel->leave_notify_timer_tag =
		gtk_timeout_add (pw_minimize_delay,
				 panel_widget_pop_down, panel);
	
	return FALSE;
}

static void
panel_widget_set_hidebuttons(PanelWidget *panel)
{
	if(panel->snapped == PANEL_FREE)
		return;
	if(panel->snapped == PANEL_DRAWER) {
		panel_widget_set_drop_zone(panel);
	} else if(panel->orient == PANEL_HORIZONTAL) {
		gtk_widget_hide(panel->hidebutton_n);
		gtk_widget_show(panel->hidebutton_e);
		gtk_widget_show(panel->hidebutton_w);
		gtk_widget_hide(panel->hidebutton_s);
	} else {
		gtk_widget_show(panel->hidebutton_n);
		gtk_widget_hide(panel->hidebutton_e);
		gtk_widget_hide(panel->hidebutton_w);
		gtk_widget_show(panel->hidebutton_s);
	}
}

static void
panel_widget_apply_size_limit(PanelWidget *panel)
{
	gint length;
	gint old_size;
	GList *list;

	g_return_if_fail(panel);
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(panel)));

	old_size = panel->size;

	switch(panel->snapped) {
		case PANEL_DRAWER:
		case PANEL_FREE:
			break;
		case PANEL_TOP:
		case PANEL_BOTTOM:
			length = gdk_screen_width() -
				 (panel->hidebutton_w->allocation.width +
				 panel->hidebutton_e->allocation.width);
			/*just so that I get size*/
			if(length%PANEL_CELL_SIZE) length--;
			panel->size = length/PANEL_CELL_SIZE;
			break;
		case PANEL_LEFT:
		case PANEL_RIGHT:
			length = gdk_screen_height() -
				 (panel->hidebutton_n->allocation.height +
				 panel->hidebutton_s->allocation.height);
			/*just so that I get size*/
			if(length%PANEL_CELL_SIZE) length--;
			panel->size = length/PANEL_CELL_SIZE;
			break;
	}

	panel_widget_right_stick(panel,old_size);

	for(list=panel->applet_list;list!=NULL;list=g_list_next(list)) {
		AppletData *ad = list->data;
		if(ad->pos+ad->cells > panel->size)
			panel_widget_move(panel, ad->applet, panel->size-1);
	}
}

static gint
panel_widget_fixed_size_allocate(GtkWidget *widget, GtkAllocation *allocation,
			         gpointer data)
{
	PanelWidget *panel = data;

	if(!GTK_WIDGET_REALIZED(widget))
		return FALSE;

	panel_widget_set_position(panel);
	panel_widget_apply_size_limit(panel);
	return TRUE;
}


/*static gint
panel_widget_dnd_drop_internal(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GtkWidget *applet;
	PanelWidget *panel = data;
	gint pos;
	gint x,y;
	PanelWidget *old_panel;
	
	gtk_widget_get_pointer(panel->fixed, &x, &y);

	if(panel->orient == PANEL_HORIZONTAL)
		pos = (x/PANEL_CELL_SIZE);
	else
		pos = (y/PANEL_CELL_SIZE);

	if(!(event->dropdataavailable.data))
		return FALSE;

	applet = GTK_WIDGET(*((GtkWidget **)event->dropdataavailable.data));

	if(!applet)
		return FALSE;

	old_panel = gtk_object_get_data(GTK_OBJECT(applet),
					PANEL_APPLET_PARENT_KEY);

	gtk_widget_ref(applet);
	panel_widget_remove(old_panel,applet);
	panel_widget_add(panel,applet,pos);
	gtk_widget_unref(applet);

	return TRUE;
}*/

static void
panel_widget_dnd_dropped_filename (GtkWidget *widget, GdkEventDropDataAvailable *event, PanelWidget *panel)
{
	if (panel_try_to_set_pixmap (panel, event->data)){
		if (panel->back_pixmap)
			g_free (panel->back_pixmap);
		panel->back_pixmap = g_strdup (event->data);
		gtk_widget_queue_draw (widget);
	}
}

static void
panel_try_to_set_back_color(PanelWidget *panel, GdkColor *color)
{
  GtkStyle *ns;

  ns = gtk_style_copy(panel->fixed->style);
  gtk_style_ref(ns);

  ns->bg[GTK_STATE_NORMAL] = panel->back_color = *color;
  ns->bg[GTK_STATE_NORMAL].pixel = panel->back_color.pixel = 1; /* bogus */

  gtk_widget_set_style(panel->fixed, ns);

  if(ns->bg_pixmap[GTK_STATE_NORMAL]) {
	  gdk_imlib_free_pixmap(ns->bg_pixmap[GTK_STATE_NORMAL]);
	  ns->bg_pixmap[GTK_STATE_NORMAL] = NULL;
  }
  g_free(panel->back_pixmap); panel->back_pixmap = NULL;
  gtk_style_unref(ns);
}

static void
panel_widget_dnd_dropped_color (GtkWidget *widget, GdkEventDropDataAvailable *event, PanelWidget *panel)
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
panel_widget_dnd_drop_internal(GtkWidget *widget, GdkEventDropDataAvailable *event, gpointer data)
{
	/* Test for the type that was dropped */
	if (strcmp (event->data_type, "url:ALL") == 0)
		panel_widget_dnd_dropped_filename (widget, event, PANEL_WIDGET (data));
	else if(!strcmp(event->data_type, "application/x-color"))
	  panel_widget_dnd_dropped_color(widget, event, PANEL_WIDGET(data));
	return;
}

static int
panel_try_to_set_pixmap (PanelWidget *panel, char *pixmap)
{
	GdkImlibImage *im;
	GdkPixmap *p;
	GtkStyle *ns;

	if(!pixmap || strcmp(pixmap,"")==0) {
		ns = gtk_style_copy(panel->fixed->style);
		gtk_style_ref(ns);

		p = ns->bg_pixmap[GTK_STATE_NORMAL];
		gdk_imlib_free_pixmap (p);
		ns->bg_pixmap[GTK_STATE_NORMAL] = NULL;

		gtk_widget_set_style(panel->fixed, ns);
	
		gtk_style_unref(ns);
		return 1;
	}

	if (panel->back_pixmap && pixmap &&
	    strcmp (panel->back_pixmap, pixmap) == 0)
		return 1;
	
	if (!g_file_exists (pixmap))
		return 0;
	
	panel->back_color.pixel = 0;

	im = gdk_imlib_load_image (pixmap);
	if (!im)
		return 0;
	
	gdk_imlib_render (im, im->rgb_width, im->rgb_height);
	p = gdk_imlib_move_image (im);

	ns = gtk_style_copy(panel->fixed->style);
	gtk_style_ref(ns);

	ns->bg_pixmap[GTK_STATE_NORMAL] = p;

	gtk_widget_set_style(panel->fixed, ns);
	
	gtk_style_unref(ns);
	gdk_imlib_destroy_image (im);
	return 1;
}

static GtkWidget *
make_hidebutton(PanelWidget *panel,
		char *pixmaparrow,
		char *pixmaphandle,
		PanelSnapped snapped,
		gint wi, gint he,
		GtkSignalFunc hidefunc)
{
	GtkWidget *w;
	GtkWidget *pixmap;
	char *pixmap_name;

	if(snapped != PANEL_DRAWER) {
		w=gtk_button_new();
		pixmap_name=gnome_unconditional_pixmap_file(pixmaparrow);
		pixmap = gnome_pixmap_new_from_file(pixmap_name);
		g_free(pixmap_name);
		gtk_container_add(GTK_CONTAINER(w),pixmap);
		gtk_widget_show(pixmap);
		gtk_signal_connect(GTK_OBJECT(w), "clicked",
				   GTK_SIGNAL_FUNC(hidefunc),
				   panel);
	} else {
		GtkWidget *frame = gtk_frame_new(NULL);
		gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
		gtk_widget_show(frame);
		pixmap_name=gnome_unconditional_pixmap_file(pixmaphandle);
		pixmap = gnome_pixmap_new_from_file(pixmap_name);
		gtk_widget_show(pixmap);
		g_free(pixmap_name);
		gtk_container_add(GTK_CONTAINER(frame),pixmap);
		w = gtk_event_box_new();
		gtk_widget_show(w);
		gtk_container_add(GTK_CONTAINER(w),frame);
		gtk_widget_set_usize(w,wi,he);
	}
	return w;
}

GtkWidget*
panel_widget_new (gint size,
		  PanelOrientation orient,
		  PanelSnapped snapped,
		  PanelMode mode,
		  PanelState state,
		  gint pos_x,
		  gint pos_y,
		  DrawerDropZonePos drop_zone_pos,
		  char *back_pixmap,
		  GdkColor *back_color)
{
	PanelWidget *panel;

	if(snapped == PANEL_FREE)
		g_return_val_if_fail(size>=0,NULL);

	panel = gtk_type_new(panel_widget_get_type());

	if(snapped == PANEL_FREE)
		GTK_WINDOW(panel)->type = GTK_WINDOW_DIALOG;
	else
		GTK_WINDOW(panel)->type = GTK_WINDOW_POPUP;
	GTK_WINDOW(panel)->allow_shrink = TRUE;
	GTK_WINDOW(panel)->allow_grow = TRUE;
	GTK_WINDOW(panel)->auto_shrink = TRUE;

	/*this makes the popup "pop down" once the button is released*/
	gtk_widget_set_events(GTK_WIDGET(panel),
			      gtk_widget_get_events(GTK_WIDGET(panel)) |
			      GDK_BUTTON_RELEASE_MASK);

	panel->table = gtk_table_new(3,3,FALSE);
	gtk_container_add(GTK_CONTAINER(panel),panel->table);
	gtk_widget_show(panel->table);

	panel->frame = gtk_frame_new(NULL);
	gtk_widget_show(panel->frame);
	gtk_frame_set_shadow_type(GTK_FRAME(panel->frame),GTK_SHADOW_OUT);
	gtk_table_attach(GTK_TABLE(panel->table),panel->frame,1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);


	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());
	panel->fixed = gtk_fixed_new();
	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();
	gtk_container_add(GTK_CONTAINER(panel->frame),panel->fixed);
	gtk_widget_show(panel->fixed);
	gtk_widget_realize (panel->fixed);

	if (!panel_try_to_set_pixmap (panel, back_pixmap))
		back_pixmap = 0;

	panel->back_pixmap = back_pixmap;

	if(back_color && back_color->pixel)
		panel_try_to_set_back_color(panel, back_color);
	
	
	/*we add all the hide buttons to the table here*/
	/*EAST*/
	panel->hidebutton_e = make_hidebutton(panel,"panel-arrow-left.xpm",
					      "panel-menu-main.xpm",snapped,
					      40,0,
					      GTK_SIGNAL_FUNC(
					      	panel_show_hide_right));
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_e,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	/*NORTH*/
	panel->hidebutton_n = make_hidebutton(panel,"panel-arrow-up.xpm",
					      "panel-menu-main.xpm",snapped,
					      0,40,GTK_SIGNAL_FUNC(
					      	panel_show_hide_right));
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_n,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);
	/*WEST*/
	panel->hidebutton_w = make_hidebutton(panel,"panel-arrow-right.xpm",
					      "panel-menu-main.xpm",snapped,
					      40,0,GTK_SIGNAL_FUNC(
					      	panel_show_hide_left));
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_w,
			 2,3,1,2,GTK_FILL,GTK_FILL,0,0);
	/*SOUTH*/
	panel->hidebutton_s = make_hidebutton(panel,"panel-arrow-down.xpm",
					      "panel-menu-main.xpm",snapped,
					      0,40,GTK_SIGNAL_FUNC(
					      	panel_show_hide_left));
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_s,
			 1,2,2,3,GTK_FILL,GTK_FILL,0,0);

	panel->snapped = snapped;
	panel->orient = orient;
	panel->mode = mode;
	panel->state = state;

	panel->master_widget = NULL;

	panel->thick = PANEL_MINIMUM_WIDTH;

	/*sanity sets, ignore settings that would/might cause bad behaviour*/
	if(snapped == PANEL_FREE) {
		panel->size = size;
		panel->mode = PANEL_EXPLICIT_HIDE;
		if(panel->state == PANEL_HIDDEN_LEFT ||
		   panel->state == PANEL_HIDDEN_RIGHT)
			panel->state = PANEL_HIDDEN;
	} else if(snapped == PANEL_DRAWER) {
		panel->size = 0;
		panel->mode = PANEL_EXPLICIT_HIDE;
		if(panel->state == PANEL_HIDDEN_LEFT ||
		   panel->state == PANEL_HIDDEN_RIGHT)
			panel->state = PANEL_HIDDEN;
	} else {
		if(snapped == PANEL_BOTTOM ||
		   snapped == PANEL_TOP)
		   	panel->size = gdk_screen_width()/PANEL_CELL_SIZE;
		else
		   	panel->size = gdk_screen_width()/PANEL_CELL_SIZE;
		if(panel->mode == PANEL_EXPLICIT_HIDE &&
		   panel->state == PANEL_HIDDEN)
			panel->state = PANEL_SHOWN;
	}

	panel->applet_list = NULL;

	if(!fleur_cursor)
		fleur_cursor = gdk_cursor_new(GDK_FLEUR);

	gtk_signal_connect(GTK_OBJECT(panel), "enter_notify_event",
			   GTK_SIGNAL_FUNC(panel_enter_notify),
			   panel);
	gtk_signal_connect(GTK_OBJECT(panel), "leave_notify_event",
			   GTK_SIGNAL_FUNC(panel_leave_notify),
			   panel);
	gtk_signal_connect_after(GTK_OBJECT(panel->fixed),
				 "size_allocate",
				 GTK_SIGNAL_FUNC(
				 	panel_widget_fixed_size_allocate),
				 panel);

	panel_widget_set_size(panel,panel->size);
	panel_widget_set_position(panel);

	panel_widget_set_hidebuttons(panel);

	if(snapped == PANEL_FREE) {
		/*these are drawer positions, but what the hell, we can use
		  em for free panels as well, though free panels can be
		  moved by the em so we shouldn't realy on this*/
		panel->x = pos_x;
		panel->y = pos_y;
		move_window(GTK_WIDGET(panel),pos_x,pos_y);
	} else if(snapped == PANEL_DRAWER) {
		/*FIXME: slightly ugly hack to avoid flashing the drawer*/
		panel->x = -100;
		panel->y = -100;
		move_window(GTK_WIDGET(panel),-100,-100);
	} else {
		panel->x = 0;
		panel->y = 0;
	}


	if(panel->mode == PANEL_AUTO_HIDE)
		panel_widget_pop_down(panel);

	/* Ok, cool hack begins: drop image files on the panel */
	gtk_signal_connect (GTK_OBJECT (panel->fixed),
			    "drop_data_available_event",
			    GTK_SIGNAL_FUNC (panel_widget_dnd_drop_internal),
			    panel);
	
	gtk_widget_dnd_drop_set (GTK_WIDGET(panel->fixed), TRUE,
				 image_drop_types,
				 sizeof(image_drop_types)/sizeof(char *),
				 FALSE);
	
	/*FIXME: ???? will we delete this or make it work*/
	/*set up drag'n'drop (the drop)*/
	/*gtk_signal_connect (GTK_OBJECT (panel->fixed), 
			    "drop_data_available_event",
			    GTK_SIGNAL_FUNC(panel_widget_dnd_drop),
			    panel);
	gtk_widget_dnd_drop_set (GTK_WIDGET(panel->fixed), TRUE,
				 applet_drop_types, 1, FALSE);

	gtk_signal_connect (GTK_OBJECT (panel->drawer_drop_zone), 
			    "drop_data_available_event",
			    GTK_SIGNAL_FUNC(panel_widget_dnd_drop),
			    panel);
	gtk_widget_dnd_drop_set (GTK_WIDGET(panel->drawer_drop_zone), TRUE,
				 applet_drop_types, 1, FALSE);
	*/


  return GTK_WIDGET(panel);
}

static void
_panel_widget_applet_drag_start_no_grab(PanelWidget *panel, GtkWidget *applet)
{
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
	_panel_widget_applet_drag_start_no_grab(panel,applet);

	gtk_grab_add(applet);
	gdk_pointer_grab(applet->window,
			 TRUE,
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
}

void
panel_widget_applet_drag_end(PanelWidget *panel)
{
	_panel_widget_applet_drag_end(panel);
	panel_applet_in_drag = FALSE;
}

/*DND ... commented out*/
#if 0
/*
 * Hackisigh routine taken from GDK
 */
static void
gdk_dnd_drag_begin (GdkWindow *initial_window)
{
  GdkEventDragBegin tev;
  tev.type = GDK_DRAG_BEGIN;
  tev.window = initial_window;
  tev.u.allflags = 0;
  tev.u.flags.protocol_version = DND_PROTOCOL_VERSION;

  gdk_event_put ((GdkEvent *) &tev);
}

static void
drag_addwindow_recurse(GtkWidget *widget,gpointer data)
{
	if (!GTK_WIDGET_NO_WINDOW(widget))
		gdk_dnd_drag_addwindow (widget->window);
		
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       drag_addwindow_recurse, 0);
}

static void
artificial_drag_start (GtkWidget *widget, int x, int y)
{
	GdkWindow *window = widget->window;
	GdkWindowPrivate *wp = (GdkWindowPrivate *) window;

	if (!wp->dnd_drag_enabled)
		return;
	/*if (!gdk_dnd.drag_perhaps)
		return;*/
	if (gdk_dnd.dnd_grabbed)
		return;
	if (gdk_dnd.drag_really)
		return;

	printf ("Pushing artificial drag\n");
	/*gdk_dnd_drag_addwindow (window);*/
	drag_addwindow_recurse(widget,0);
	gdk_dnd_drag_begin (window);
	XGrabPointer (gdk_display, wp->xwindow, False,
		      ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
		      GrabModeAsync, GrabModeAsync, gdk_root_window,
		      None, CurrentTime);
	gdk_dnd.dnd_grabbed = TRUE;
	gdk_dnd.drag_perhaps = 1;
	gdk_dnd.drag_really = 1;
	gdk_dnd_display_drag_cursor (x, y, FALSE, TRUE);
}

#endif

static gint
panel_widget_is_cursor(PanelWidget *panel, int overlap)
{
	gint x,y;
	gint w,h;

	if(!GTK_WIDGET_VISIBLE(GTK_WIDGET(panel)))
		return FALSE;

	gtk_widget_get_pointer(GTK_WIDGET(panel), &x, &y);
	gdk_window_get_size(GTK_WIDGET(panel)->window, &w, &h);

	if((x+overlap)>=0 &&
	   (x-overlap)<=w &&
	   (y+overlap)>=0 &&
	   (y-overlap)<=h)
		return TRUE;
	return FALSE;
}

/*calculates the value to move the applet by*/
static gint
panel_widget_get_moveby(PanelWidget *panel, gint pos)
{
	gint x,y;

	gtk_widget_get_pointer(panel->fixed, &x, &y);

	if(panel->orient == PANEL_HORIZONTAL)
		return (x/PANEL_CELL_SIZE)- pos;
	else
		return (y/PANEL_CELL_SIZE)- pos;
}

static GList *
walk_up_to(gint pos, GList *list)
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
is_in_applet(gint pos, AppletData *ad)
{
	if(ad->pos <= pos && ad->pos+ad->cells > pos)
		return ad->applet;
	return NULL;
}

static gint
panel_widget_get_free_spot(PanelWidget *panel, AppletData *ad)
{
	int i,e;
	gint x,y;
	gint place;
	gint start;
	gint right=-1,left=-1;
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
panel_widget_nice_move(PanelWidget *panel, AppletData *ad, gint pos)
{
	if(pos==ad->pos)
		return;

	ad->pos = pos;

	panel->applet_list =
		my_g_list_resort(panel->applet_list,ad,
				 (GCompareFunc)applet_data_compare);

	panel_widget_applet_put(panel,ad);
}

static gint
panel_widget_is_on_list(PanelWidget *panel, GList *list)
{
	for(;list!=NULL;list=g_list_next(list))
		if(panel == list->data)
			return TRUE;
	return FALSE;
}


/*find the cursor position and move the applet to that position*/
gint
panel_widget_applet_move_to_cursor(PanelWidget *panel)
{
	if (panel->currently_dragged_applet) {
		gint moveby;
		gint pos = panel->currently_dragged_applet->pos;
		GtkWidget *applet = panel->currently_dragged_applet->applet;
		/*PanelWidget *assoc = gtk_object_get_data(GTK_OBJECT(applet),
						PANEL_APPLET_ASSOC_PANEL_KEY);*/
		GList *forb = gtk_object_get_data(GTK_OBJECT(applet),
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
				   (!panel_widget_is_on_list(new_panel,forb))) {
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
		   panel->snapped == PANEL_DRAWER) {
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

static gint
move_timeout_handler(gpointer data)
{
	return panel_widget_applet_move_to_cursor(PANEL_WIDGET(data));
}

void
panel_widget_applet_move_use_idle(PanelWidget *panel)
{
	gtk_timeout_add (30,move_timeout_handler,panel);
}



static gint
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

static gint
panel_sub_event_handler(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	switch (event->type) {
		/*pass these to the parent!*/
		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
			return gtk_widget_event(
				listening_parent(widget->parent), event);

			break;

		default:
			break;
	}

	return FALSE;
}

/*static gint
panel_widget_dnd_drag_request(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	PanelWidget *panel = data;
	
	gtk_widget_dnd_data_set (widget, event, &widget, sizeof(widget));
	gtk_widget_ref(widget);
	panel_widget_remove(panel,widget);

	return TRUE;
}*/


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

	g_return_if_fail(panel != r);

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

static gint
panel_widget_applet_destroy(GtkWidget *applet, gpointer data)
{
	PanelWidget *panel;
	PanelWidget *p;
	AppletData *ad;
	int thick;

	panel = gtk_object_get_data(GTK_OBJECT(applet),PANEL_APPLET_PARENT_KEY);

	g_return_val_if_fail(panel!=NULL,FALSE);

	ad = gtk_object_get_data(GTK_OBJECT(applet), PANEL_APPLET_DATA);
	/*we already "took care" of this applet*/
	if(!ad)
		return FALSE;

	panel->applet_list = g_list_remove(panel->applet_list,ad);
	g_free(ad);

	gtk_object_set_data(GTK_OBJECT(applet), PANEL_APPLET_DATA, NULL);

	p = gtk_object_get_data(GTK_OBJECT(applet),
				PANEL_APPLET_ASSOC_PANEL_KEY);
	if(p)
		remove_panel_from_forbidden(panel,p);

	if(panel->snapped==PANEL_DRAWER)
		panel_widget_pack_applets(panel);

	thick = panel_widget_get_thick(panel);
	if(panel->thick != thick) {
		panel->thick = thick;
		panel_widget_set_size(panel, panel->size);
	}

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_REMOVED_SIGNAL]);

	return FALSE;
}



static void
bind_top_applet_events(GtkWidget *widget)
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

	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, 0);
}

static gint
panel_widget_make_empty_pos(PanelWidget *panel, gint pos)
{
	AppletData *ad;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(pos>=0,-1);

	if(panel->snapped == PANEL_DRAWER) {
		if(pos >= panel->size) {
			panel_widget_set_size(panel,++panel->size);
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



static gint
panel_widget_find_empty_pos(PanelWidget *panel, gint pos)
{
	int i;
	gint right=-1,left=-1;
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

void
panel_widget_add_forbidden(PanelWidget *panel)
{
	add_panel_to_forbidden(panel,panel);
}

gint
panel_widget_add (PanelWidget *panel, GtkWidget *applet, gint pos)
{
	AppletData *ad;

	g_return_val_if_fail(panel!=NULL,-1);
	g_return_val_if_fail(applet!=NULL,-1);
	g_return_val_if_fail(pos>=0,-1);

	if(pw_movement_type == PANEL_SWITCH_MOVE ||
	   panel->snapped == PANEL_DRAWER)
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

	bind_top_applet_events(applet);

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_ADDED_SIGNAL],
			applet);

	/*NOTE: forbidden list is not updated on addition, use the
	function above for the panel*/

	return pos;
}

gint
panel_widget_reparent (PanelWidget *old_panel,
		       PanelWidget *new_panel,
		       GtkWidget *applet,
		       gint pos)
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
	   new_panel->snapped == PANEL_DRAWER)
		pos = panel_widget_make_empty_pos(new_panel,pos);
	else
		pos = panel_widget_find_empty_pos(new_panel,pos);

	if(pos==-1) return -1;

	p = gtk_object_get_data(GTK_OBJECT(applet),
				PANEL_APPLET_ASSOC_PANEL_KEY);
	if(p) {
		remove_panel_from_forbidden(old_panel,p);
		add_panel_to_forbidden(new_panel,p);
	}

	gtk_object_set_data(GTK_OBJECT(applet),
			    PANEL_APPLET_PARENT_KEY,
			    new_panel);

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

	/*reparent applet*/
	gtk_widget_reparent(applet,new_panel->fixed);

	/*it will get moved to the right position on size_allocate*/
	if(new_panel->orient == PANEL_HORIZONTAL)
		gtk_fixed_move(GTK_FIXED(new_panel->fixed),applet,
			       pos*PANEL_CELL_SIZE,0);
	else
		gtk_fixed_move(GTK_FIXED(new_panel->fixed),applet,
			       0,pos*PANEL_CELL_SIZE);

	gtk_widget_show(applet);

	if(old_panel->snapped==PANEL_DRAWER)
		panel_widget_pack_applets(old_panel);

	thick = panel_widget_get_thick(old_panel);
	if(old_panel->thick != thick)
		old_panel->thick = thick;
	panel_widget_set_size(old_panel, old_panel->size);

	gtk_signal_emit(GTK_OBJECT(old_panel),
			panel_widget_signals[APPLET_REMOVED_SIGNAL]);

	gtk_signal_emit(GTK_OBJECT(new_panel),
			panel_widget_signals[APPLET_ADDED_SIGNAL],
			applet);

	return pos;
}

gint
panel_widget_move (PanelWidget *panel, GtkWidget *applet, gint pos)
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

	panel_widget_applet_put(panel,ad);

	return pos;
}

gint
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
	if(p)
		remove_panel_from_forbidden(panel,p);

	/*remove applet*/
	gtk_container_remove(GTK_CONTAINER(panel->fixed),applet);


	if(panel->snapped==PANEL_DRAWER)
		panel_widget_pack_applets(panel);


	thick = panel_widget_get_thick(panel);
	if(panel->thick != thick) {
		panel->thick = thick;
		panel_widget_set_size(panel, panel->size);
	}

	gtk_signal_emit(GTK_OBJECT(panel),
			panel_widget_signals[APPLET_REMOVED_SIGNAL]);

	return i;
}

gint
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
			   PanelSnapped snapped,
			   PanelMode mode,
			   PanelState state,
			   DrawerDropZonePos drop_zone_pos,
			   char *pixmap)
{
	PanelOrientation oldorient;
	PanelSnapped oldsnapped;
	PanelState oldstate;

	g_return_if_fail(panel);
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(panel)));

	oldorient = panel->orient;
	oldsnapped = panel->snapped;
	oldstate = panel->state;

	panel->state = state;
	panel->mode = mode;

	/*so that there are no shifts necessary before size_allocate*/
	panel->snapped = snapped;
	switch(panel->snapped) {
		case PANEL_DRAWER:
			/*fallthrough, so orient MUST be right, can we
			  assume that???*/
		case PANEL_FREE:
			panel->orient = orient;
			break;
		case PANEL_TOP:
		case PANEL_BOTTOM:
			panel->orient = PANEL_HORIZONTAL;
			panel_widget_apply_size_limit(panel);
			break;
		case PANEL_LEFT:
		case PANEL_RIGHT:
			panel->orient = PANEL_VERTICAL;
			panel_widget_apply_size_limit(panel);
			break;
	}

	panel_widget_set_hidebuttons(panel);

	panel->thick = panel_widget_get_thick(panel);

	if(panel->mode == PANEL_EXPLICIT_HIDE && panel->state == PANEL_HIDDEN)
		panel->state = PANEL_SHOWN;

	panel->drawer_drop_zone_pos = drop_zone_pos;
	if(panel->snapped == PANEL_DRAWER)
	   	panel_widget_set_drop_zone(panel);

	panel_widget_set_size(panel,panel->size);
	panel_widget_set_position(panel);

	if(oldorient != panel->orient ||
	   oldsnapped != panel->snapped)
	   	gtk_signal_emit(GTK_OBJECT(panel),
	   			panel_widget_signals[ORIENT_CHANGE_SIGNAL],
	   			panel->orient,
	   			panel->snapped);
	if(oldstate != panel->state)
	   	gtk_signal_emit(GTK_OBJECT(panel),
	   			panel_widget_signals[STATE_CHANGE_SIGNAL],
	   			panel->state);

	if(panel->mode == PANEL_AUTO_HIDE)
		panel_widget_pop_down(panel);
	if (panel_try_to_set_pixmap (panel, pixmap)){
		if (panel->back_pixmap)
			g_free (panel->back_pixmap);
		panel->back_pixmap = g_strdup (pixmap);
	}
}

void
panel_widget_change_orient(PanelWidget *panel,
			   PanelOrientation orient)
{
	panel_widget_change_params(panel,
				   orient,
				   panel->snapped,
				   panel->mode,
				   panel->state,
				   panel->drawer_drop_zone_pos,
				   panel->back_pixmap);
}

void
panel_widget_change_drop_zone_pos(PanelWidget *panel,
				  DrawerDropZonePos drop_zone_pos)
{
	panel_widget_change_params(panel,
				   panel->orient,
				   panel->snapped,
				   panel->mode,
				   panel->state,
				   drop_zone_pos,
				   panel->back_pixmap);
}


void
panel_widget_restore_state(PanelWidget *panel)
{
	gtk_widget_show(GTK_WIDGET(panel));
	/*is this needed, probably ... in case we move the panel, this
	  function should do a complete restore*/
	panel_widget_set_size(panel,panel->size);
}


/*change global params*/
void
panel_widget_change_global(gint explicit_step,
			   gint auto_step,
			   gint drawer_step,
			   gint minimized_size,
			   gint minimize_delay,
			   PanelMovementType move_type,
			   gint disable_animations)
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

void
panel_widget_enable_buttons(PanelWidget *panel)
{
	gtk_widget_set_sensitive(panel->hidebutton_n,TRUE);
	gtk_widget_set_sensitive(panel->hidebutton_e,TRUE);
	gtk_widget_set_sensitive(panel->hidebutton_w,TRUE);
	gtk_widget_set_sensitive(panel->hidebutton_s,TRUE);
}


void
panel_widget_disable_buttons(PanelWidget *panel)
{
	gtk_widget_set_sensitive(panel->hidebutton_n,FALSE);
	gtk_widget_set_sensitive(panel->hidebutton_e,FALSE);
	gtk_widget_set_sensitive(panel->hidebutton_w,FALSE);
	gtk_widget_set_sensitive(panel->hidebutton_s,FALSE);
}

void
panel_widget_set_drawer_pos(PanelWidget *panel, gint x, gint y)
{
	if(panel->x!=x || panel->y!=y) {
		panel->x = x;
		panel->y = y;
		move_window(GTK_WIDGET(panel),x,y);
	}
}

#if 0
/*this does not really work, it's old test code, in the unlikely event
of a water landing, you can use this piece of code as a floatation device*/

int
main(int argc, char **argv)
{
	GtkWidget *panel;
	GtkWidget *button;

	gnome_init("panel-widget", &argc, &argv);
	textdomain(PACKAGE);

	panel = panel_widget_new(0,PANEL_HORIZONTAL,PANEL_BOTTOM,
				 PANEL_EXPLICIT_HIDE,PANEL_SHOWN,5,5,100);

	button = gtk_button_new_with_label("TEST");
	panel_widget_add(PANEL_WIDGET(panel),button,1);
	gtk_widget_show(button);

	button = gtk_button_new_with_label("TEST2");
	panel_widget_add(PANEL_WIDGET(panel),button,8);
	gtk_widget_show(button);

	gtk_widget_show(panel);

	gtk_main();
	return 0;
}

#endif
