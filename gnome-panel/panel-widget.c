#include <gtk/gtk.h>
#include <gnome.h>
#include "panel-widget.h"
#include "gdkextra.h"

static void panel_widget_class_init	(PanelWidgetClass *klass);
static void panel_widget_init		(PanelWidget      *panel_widget);

static GdkCursor *fleur_cursor;

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

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

static void
panel_widget_class_init (PanelWidgetClass *class)
{
}

static void
panel_widget_init (PanelWidget *panel_widget)
{
	panel_widget->applet_count = 0;
	panel_widget->orient= PANEL_HORIZONTAL;
	panel_widget->snapped = PANEL_BOTTOM;
	panel_widget->mode = PANEL_EXPLICIT_HIDE;
	panel_widget->state = PANEL_SHOWN;
	panel_widget->size = 0;
	panel_widget->leave_notify_timer_tag = 0;
	panel_widget->currently_dragged_applet = NULL;
	panel_widget->drawer_drop_zone = NULL;
	panel_widget->drawer_drop_zone_pos = DROP_ZONE_LEFT;
}

static void
move_window(GtkWidget *widget, int x, int y)
{
	gdk_window_set_hints(widget->window, x, y, 0, 0, 0, 0, GDK_HINT_POS);
	gdk_window_move(widget->window, x, y);
	gtk_widget_draw(widget, NULL); /* FIXME: this should draw only the newly exposed area! */
}



/*this is only for snapped panels!*/
static void
panel_widget_set_position(PanelWidget *panel)
{
	gint xcor = 0;
	gint ycor = 0;
	gint x,y;
	gint newx,newy;

	if(panel->snapped == PANEL_FREE ||
	   panel->snapped == PANEL_DRAWER ||
	   panel->state == PANEL_MOVING)
		return;

	gdk_window_get_origin(GTK_WIDGET(panel)->window,&x,&y);
	newx = x;
	newy = y;

	if(panel->mode == PANEL_AUTO_HIDE && panel->state == PANEL_HIDDEN)
		ycor = panel->thick - panel->minimized_size;
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
			newy = gdk_screen_height() - panel->thick + ycor;
			break;
		case PANEL_LEFT:
			newx = -ycor;
			newy = xcor;
			break;
		case PANEL_RIGHT:
			newx = gdk_screen_width() - panel->thick + ycor;
			newy = xcor;
			break;
		default: break; /*to get rid of a warning*/
	}
	if(newx != x || newy != y)
		move_window(GTK_WIDGET(panel),newx,newy);
}

static void
drawer_resize_drop_zone(PanelWidget *panel)
{
	if(panel->orient == PANEL_HORIZONTAL)
		gtk_widget_set_usize(panel->drawer_drop_zone,
				     PANEL_DRAWER_DROP_TARGET_SIZE*
				     	PANEL_CELL_SIZE,
				     panel->thick);
	else
		gtk_widget_set_usize(panel->drawer_drop_zone, panel->thick,
				     PANEL_DRAWER_DROP_TARGET_SIZE*
				     	PANEL_CELL_SIZE);
}

static void
panel_widget_set_size(PanelWidget *panel, gint size)
{
	if(size == 0)
		size = panel->size;
	switch(panel->snapped) {
		case PANEL_DRAWER:
			drawer_resize_drop_zone(panel);
		case PANEL_FREE:
			if(panel->orient == PANEL_HORIZONTAL)
				gtk_widget_set_usize(GTK_WIDGET(panel),
						     size*PANEL_CELL_SIZE,
						     panel->thick);
			else
				gtk_widget_set_usize(GTK_WIDGET(panel),
						     panel->thick,
						     size*PANEL_CELL_SIZE);
			break;
		case PANEL_TOP:
		case PANEL_BOTTOM:
			panel->orient = PANEL_HORIZONTAL;
			gtk_widget_set_usize(GTK_WIDGET(panel),
					     gdk_screen_width(),
					     panel->thick);
			break;
		case PANEL_LEFT:
		case PANEL_RIGHT:
			panel->orient = PANEL_VERTICAL;
			gtk_widget_set_usize(GTK_WIDGET(panel),
					     panel->thick,
					     gdk_screen_height());
			break;
	}
}

static void
panel_widget_applet_put(PanelWidget *panel,gint pos)
{
	g_return_if_fail(panel->applets[pos].applet!=NULL);
	if(panel->orient == PANEL_HORIZONTAL)
		gtk_fixed_move(GTK_FIXED(panel->fixed),
			       panel->applets[pos].applet,
			       pos*PANEL_CELL_SIZE,
			       panel->applets[pos].applet->allocation.y);
	else
		gtk_fixed_move(GTK_FIXED(panel->fixed),
			       panel->applets[pos].applet,
			       panel->applets[pos].applet->allocation.x,
			       pos*PANEL_CELL_SIZE);
}


static void
panel_widget_pack_applets(PanelWidget *panel)
{
	gint i;
	gint x;

	for(i=0,x=0;i<PANEL_MAX;i++) {
		panel->applets[x].applet = panel->applets[i].applet;
		panel->applets[x].cells = panel->applets[i].cells;
		if(panel->applets[i].applet!=NULL)
			x++;
	}
	panel->size = x;
	for(;x<PANEL_MAX;x++) {
		panel->applets[x].applet = NULL;
		panel->applets[x].cells = 1;
	}
	for(i=0;i<panel->size;i+=panel->applets[i].cells)
		panel_widget_applet_put(panel,i);
}


static void
panel_widget_shrink_wrap(PanelWidget *panel,
			 gint width,
			 gint pos)
{
	gint i;

	g_return_if_fail(pos>=0 && pos<PANEL_MAX);

	if(width%PANEL_CELL_SIZE) width--; /*just so that I get
					     the right size*/
	/*convert width from pixels to cells*/
	width = (width/PANEL_CELL_SIZE) + 1;

	if(width >= panel->applets[pos].cells)
		return;

	for(i=pos+width;i<(pos+panel->applets[pos].cells);i++) {
		panel->applets[i].applet=NULL;
		panel->applets[i].cells=1;
	}
	for(i=pos;i<(pos+width);i++) {
		panel->applets[i].cells=width;
	}
}


static gint
panel_widget_push_left(PanelWidget *panel,gint pos)
{
	gint i;
	gint freepos;

	for(i=0;pos-i>=0 && panel->applets[pos-i].applet;i++)
		;
	if(pos-i < 0)
		return FALSE;

	freepos=i;

	for(;i>0;i--) {
		panel->applets[pos-i].applet=
			panel->applets[pos-i+1].applet;
		panel->applets[pos-i].cells =
			panel->applets[pos-i+1].cells;
	}

	for(i=pos-freepos;i<pos;i+=panel->applets[i].cells)
		panel_widget_applet_put(panel,i);
	return TRUE;
}


static gint
panel_widget_push_right(PanelWidget *panel,gint pos)
{
	gint i;
	gint freepos;

	if(panel->snapped == PANEL_DRAWER) {
		if(panel->size+1>=PANEL_MAX)
			return FALSE;
		if(panel->applets[pos].applet==NULL) {
			/*it must be the right end*/
			panel_widget_set_size(panel,++panel->size);
			return TRUE;
		}
		for(i=panel->size;i>pos;i--) {
			panel->applets[i].applet=
				panel->applets[i-1].applet;
			panel->applets[i].cells =
				panel->applets[i-1].cells;
		}
		panel->applets[pos].applet=NULL;
		panel->applets[pos].cells=1;
		panel->size++;
		for(i=pos+1;i<panel->size;i+=panel->applets[i].cells)
			panel_widget_applet_put(panel,i);
		panel_widget_set_size(panel,panel->size);
		return TRUE;
	}
	

	for(i=0;pos+i<panel->size && panel->applets[pos+i].applet;i++)
		;
	if(pos+i >= panel->size)
		return FALSE;

	freepos=i;

	for(;i>0;i--) {
		panel->applets[pos+i].applet=
			panel->applets[pos+i-1].applet;
		panel->applets[pos+i].cells =
			panel->applets[pos+i-1].cells;
	}

	for(i=0;i<freepos;i+=panel->applets[pos+i].cells)
		panel_widget_applet_put(panel,pos+i);
	return TRUE;
}

static gint
panel_widget_seize_space(PanelWidget *panel,
			 gint width,
			 gint pos)
{
	gint allocated=1;
	gint i;
	GtkWidget *applet;

	applet = panel->applets[pos].applet;

	if(width%PANEL_CELL_SIZE) width--; /*just so that I get
					     the right size*/
	/*convert width from pixels to cells*/
	width = (width/PANEL_CELL_SIZE) + 1;

	if(panel->snapped == PANEL_DRAWER)
		panel_widget_pack_applets(panel);


	for(i=1;(pos+i<(panel->snapped==PANEL_DRAWER?PANEL_MAX:panel->size)) &&
		(allocated < width) &&
		(panel->applets[pos+i].applet == applet ||
		 panel->applets[pos+i].applet == NULL);i++)
		allocated++;
	if(pos+i>panel->size) {
		panel->size = pos+i;
		panel_widget_set_size(panel,panel->size);
	}
	for(i=1;(pos-i >= 0) &&
		(allocated < width) &&
		(panel->applets[pos-i].applet == NULL);i++)
		allocated++;
	pos = pos-i+1;

	if(allocated < width) {
		while(allocated < width &&
		      panel_widget_push_right(panel,pos+allocated))
			allocated++;
		if(panel->snapped != PANEL_DRAWER) {
			while(allocated < width &&
			      panel_widget_push_left(panel,pos-1)) {
				pos--;
				allocated++;
			}
		}
	}

	for(i=0;i<allocated;i++) {
		panel->applets[pos+i].applet = applet;
		panel->applets[pos+i].cells = allocated;
	}
	return pos;
}

static void
panel_widget_adjust_applet(PanelWidget *panel, GtkWidget *applet)
{
	gint width, height;
	gint x,y;
	gint oldx,oldy;
	gint pos;

	width = applet->allocation.width;
	height = applet->allocation.height;
	oldx = applet->allocation.x;
	oldy = applet->allocation.y;
	pos = panel_widget_get_pos(panel,applet);

	/*don't adjust applets out of range, wait for
	  then to be pushed into range*/
	if(panel->snapped != PANEL_DRAWER && pos>=panel->size)
		return;

	g_return_if_fail(pos>=0 && pos<PANEL_MAX);

	if(panel->orient==PANEL_HORIZONTAL) {
		if(height > panel->thick) {
			panel->thick = height;
			panel_widget_set_size(panel,panel->size);
		}

		/*if smaller then it's allocation, we are OK*/
		if(width<=(PANEL_CELL_SIZE*panel->applets[pos].cells)) {
			panel_widget_shrink_wrap(panel,width,pos);
			x = (PANEL_CELL_SIZE*pos) +
			    ((PANEL_CELL_SIZE*panel->applets[pos].cells)/2) -
			    (width/2);
		        y = (panel->thick/2) - (height/2);
		} else {
			pos = panel_widget_seize_space(panel,width,pos);
			x = (PANEL_CELL_SIZE*pos) +
			    ((PANEL_CELL_SIZE*panel->applets[pos].cells)/2) -
			    (width/2);
		        y = (panel->thick/2) - (height/2);
		}
	} else { /* panel->orient==PANEL_VERTICAL */
		if(width > panel->thick) {
			panel->thick = width;
			panel_widget_set_size(panel,panel->size);
		}

		/*if smaller then it's allocation, we are OK*/
		if(height<=(PANEL_CELL_SIZE*panel->applets[pos].cells)) {
			panel_widget_shrink_wrap(panel,height,pos);
		        x = (panel->thick/2) - (width/2);
			y = (PANEL_CELL_SIZE*pos) +
			    ((PANEL_CELL_SIZE*panel->applets[pos].cells)/2) -
			    (height/2);
		} else {
			pos = panel_widget_seize_space(panel,height,pos);
		        x = (panel->thick/2) - (width/2);
			y = (PANEL_CELL_SIZE*pos) +
			    ((PANEL_CELL_SIZE*panel->applets[pos].cells)/2) -
			    (height/2);
		}
	}
	if(oldx!=x || oldy!=y)
		gtk_fixed_move(GTK_FIXED(panel->fixed),applet,x,y);
}


static gint
panel_widget_switch_applet_right(PanelWidget *panel, gint pos)
{
	gint i;
	gint rightn;
	AppletRecord tmp;

	tmp.applet = panel->applets[pos].applet;
	tmp.cells = panel->applets[pos].cells;

	rightn = pos + panel->applets[pos].cells;

	for(i=0;i<panel->applets[rightn].cells;i++) {
		panel->applets[pos+i].applet = panel->applets[rightn+i].applet;
		panel->applets[pos+i].cells = panel->applets[rightn+i].cells;
	}

	rightn = pos;
	pos = pos + i;

	for(i=0;i<tmp.cells;i++) {
		panel->applets[pos+i].applet = tmp.applet;
		panel->applets[pos+i].cells = tmp.cells;
	}


	if(panel->applets[rightn].applet)
		panel_widget_adjust_applet(panel,panel->applets[rightn].applet);
	if(panel->applets[pos].applet)
		panel_widget_adjust_applet(panel,panel->applets[pos].applet);

	return pos;
}

static gint
panel_widget_switch_applet_left(PanelWidget *panel, gint pos)
{
	pos -= panel->applets[pos-1].cells;
	panel_widget_switch_applet_right(panel,pos);
	return pos;
}

static gint
panel_widget_is_right_drop_zone(PanelWidget *panel, gint pos)
{
	if(panel->applets[pos + panel->applets[pos].cells].applet ==
	   panel->drawer_drop_zone && panel->drawer_drop_zone)
	   	return TRUE;
	return FALSE;
}

static gint
panel_widget_is_left_drop_zone(PanelWidget *panel, gint pos)
{
	if(panel->applets[pos - 1].applet ==
	   panel->drawer_drop_zone && panel->drawer_drop_zone)
	   	return TRUE;
	return FALSE;
}

static gint
panel_widget_get_right_switch_pos(PanelWidget *panel, gint pos)
{
	pos+=panel->applets[pos + panel->applets[pos].cells].cells;
	return pos;
}

static gint
panel_widget_get_left_switch_pos(PanelWidget *panel, gint pos)
{
	pos -= panel->applets[pos-1].cells;
	panel_widget_get_right_switch_pos(panel,pos);
	return pos;
}


static gint
panel_widget_switch_move(PanelWidget *panel, gint pos, gint moveby)
{
	gint width;
	gint finalpos;

	g_return_val_if_fail(pos>=0,-1);
	g_return_val_if_fail(panel,-1);

	width = panel->applets[pos].cells;
	finalpos = pos+moveby;

	if(finalpos >= panel->size)
		finalpos = panel->size-1;
	else if(finalpos < 0)
		finalpos = 0;

	while((pos+width-1)<finalpos) {
		if(panel->snapped == PANEL_DRAWER &&
		   panel_widget_is_right_drop_zone(panel,pos))
		   	return pos;
		if(panel_widget_get_right_switch_pos(panel,pos) > finalpos)
			return pos;
		pos = panel_widget_switch_applet_right(panel,pos);
	}
	while(pos>finalpos) {
		if(panel->snapped == PANEL_DRAWER &&
		   panel_widget_is_left_drop_zone(panel,pos))
		   	return pos;
		if((panel_widget_get_left_switch_pos(panel,pos)+width-1) < 
		   finalpos)
			return pos;
		pos = panel_widget_switch_applet_left(panel,pos);
	}

	return pos;
}



static gint
panel_widget_applet_size_allocate (GtkWidget *widget,
				   GdkEvent *event,
				   gpointer data)
{
	PanelWidget *panel;

	panel = gtk_object_get_data(GTK_OBJECT(widget),PANEL_APPLET_PARENT_KEY);
	panel_widget_adjust_applet(panel,widget);

	return TRUE;
}

static void
move_horiz(PanelWidget *panel, int src_x, int dest_x)
{
	gint x, y;

	gdk_window_get_origin(GTK_WIDGET(panel)->window,&x,&y);

	if (panel->step_size != 0) {
		if (src_x < dest_x) {
			for (x = src_x; x < dest_x; x += panel->step_size)
				move_window(GTK_WIDGET(panel), x, y);
		} else {
			for (x = src_x; x > dest_x; x -= panel->step_size)
				move_window(GTK_WIDGET(panel), x, y);
		}
	}
	
	move_window(GTK_WIDGET(panel), dest_x, y);
}


static void
move_vert(PanelWidget *panel, int src_y, int dest_y)
{
	gint x, y;

	gdk_window_get_origin(GTK_WIDGET(panel)->window,&x,&y);

	if (panel->step_size != 0) {
		if (src_y < dest_y) {
			for (y = src_y; y < dest_y; y += panel->step_size)
				move_window(GTK_WIDGET(panel),x,y);
		} else {
			for (y = src_y; y > dest_y; y -= panel->step_size)
				move_window(GTK_WIDGET(panel),x,y);
		}
	}

	move_window(GTK_WIDGET(panel), x, dest_y);
}



static void
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
		        move_vert(panel, -height + panel->minimized_size, 0);
			break;

		case PANEL_BOTTOM:
			move_vert(panel, sheight - panel->minimized_size, 
				  sheight - height);
			break;

		case PANEL_LEFT:
			move_horiz(panel, -width + panel->minimized_size, 0);
			break;

		case PANEL_RIGHT:
			move_horiz(panel, swidth - panel->minimized_size, 
				   swidth - width);
			break;
		default: break; /*to get rid of a warning*/
	}

	panel->state = PANEL_SHOWN;
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
	/*we are moving, so wait with the pop_down*/
	if(panel->currently_dragged_applet) {
		panel->leave_notify_timer_tag =
			gtk_timeout_add (panel->minimize_delay,
					 panel_widget_pop_down, panel);
		return FALSE;
	}

	panel->state = PANEL_MOVING;

	width   = GTK_WIDGET(panel)->allocation.width;
	height  = GTK_WIDGET(panel)->allocation.height;
	swidth  = gdk_screen_width();
	sheight = gdk_screen_height();

	switch (panel->snapped) {
		case PANEL_TOP:
			move_vert(panel, 0, -height + panel->minimized_size);
			break;

		case PANEL_BOTTOM:
			move_vert(panel, sheight - height, 
				  sheight - panel->minimized_size);
			break;

		case PANEL_LEFT:
			move_horiz(panel, 0, -width + panel->minimized_size);
			break;

		case PANEL_RIGHT:
			move_horiz(panel, swidth - width, 
				   swidth - panel->minimized_size);
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
				   panel->hidebutton_w->allocation.width, 0);
		else
			move_horiz(panel, width -
				   panel->hidebutton_e->allocation.width, 0);
	} else {
		if(fromright)
			move_vert(panel, -height +
				  panel->hidebutton_s->allocation.height, 0);
		else
			move_vert(panel, height -
				  panel->hidebutton_n->allocation.height, 0);
	}

	panel->state = PANEL_SHOWN;
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

	panel->state = PANEL_MOVING;

	width   = GTK_WIDGET(panel)->allocation.width;
	height  = GTK_WIDGET(panel)->allocation.height;

	if(panel->orient == PANEL_HORIZONTAL) {
		if(fromright)
			move_horiz(panel, 0, -width +
				   panel->hidebutton_w->allocation.width);
		else
			move_horiz(panel, 0, width -
				   panel->hidebutton_e->allocation.width);
	} else {
		if(fromright)
			move_vert(panel, 0, -height +
				  panel->hidebutton_s->allocation.height);
		else
			move_vert(panel, 0, height -
				  panel->hidebutton_n->allocation.height);
	}

	if(fromright)
		panel->state = PANEL_HIDDEN_LEFT;
	else
		panel->state = PANEL_HIDDEN_RIGHT;
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
		gtk_timeout_add (panel->minimize_delay,
				 panel_widget_pop_down, panel);
	
	return FALSE;
}

static void
panel_widget_set_hidebuttons(PanelWidget *panel)
{
	if(panel->snapped == PANEL_FREE ||
	   panel->snapped == PANEL_DRAWER)
		return;
	if(panel->orient == PANEL_HORIZONTAL) {
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
	gint i;
	gint length;

	g_return_if_fail(panel);
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(panel)));

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

	for(i=panel->size;i<PANEL_MAX;i += panel->applets[i].cells)
		if(panel->applets[i].applet)
			panel_widget_move(panel, i, panel->size-1);

	for(i=panel->size;i<PANEL_MAX;i++) {
		if(panel->applets[i].applet != NULL ||
		   panel->applets[i].cells != 1) {
		   	g_warning("cell %d messed up, not cleared by move\n",i);
		}
	}
}

static gint
panel_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation,
			   gpointer data)
{
	PanelWidget *panel = PANEL_WIDGET(widget);
	panel_widget_set_position(panel);
	panel_widget_apply_size_limit(panel);
	return FALSE;
}

GtkWidget*
panel_widget_new (gint size,
		  PanelOrientation orient,
		  PanelSnapped snapped,
		  PanelMode mode,
		  PanelState state,
		  gint step_size,
		  gint minimized_size,
		  gint minimize_delay,
		  gint pos_x,
		  gint pos_y,
		  DrawerDropZonePos drop_zone_pos)
{
	PanelWidget *panel;
	gint i;
	gchar *pixmap_name;
	GtkWidget *pixmap;

	if(snapped == PANEL_FREE)
		g_return_val_if_fail(size>=0,NULL);

	panel = gtk_type_new(panel_widget_get_type());
	if(snapped == PANEL_FREE)
		GTK_WINDOW(panel)->type = GTK_WINDOW_DIALOG;
	else
		GTK_WINDOW(panel)->type = GTK_WINDOW_POPUP;

	/*this makes the popup "pop down" once the button is released*/
	gtk_widget_set_events(GTK_WIDGET(panel),
			      gtk_widget_get_events(GTK_WIDGET(panel)) |
			      GDK_BUTTON_RELEASE_MASK);


	panel->table = gtk_table_new(3,3,FALSE);
	gtk_container_add(GTK_CONTAINER(panel),panel->table);
	gtk_widget_show(panel->table);

	panel->fixed = gtk_fixed_new();
	gtk_table_attach(GTK_TABLE(panel->table),panel->fixed,1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);
	gtk_widget_show(panel->fixed);

	/*EAST*/
	panel->hidebutton_e=gtk_button_new();
	pixmap_name=gnome_unconditional_pixmap_file("panel-arrow-left.xpm");
	pixmap = gnome_create_pixmap_widget(GTK_WIDGET(panel),
					    panel->hidebutton_e,
					    pixmap_name);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(panel->hidebutton_e),pixmap);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(panel->hidebutton_e), "clicked",
			   GTK_SIGNAL_FUNC(panel_show_hide_right),panel);
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_e,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	/*NORTH*/
	panel->hidebutton_n=gtk_button_new();
	pixmap_name=gnome_unconditional_pixmap_file("panel-arrow-up.xpm");
	pixmap = gnome_create_pixmap_widget(GTK_WIDGET(panel),
					    panel->hidebutton_n,
					    pixmap_name);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(panel->hidebutton_n),pixmap);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(panel->hidebutton_n), "clicked",
			   GTK_SIGNAL_FUNC(panel_show_hide_right),panel);
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_n,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);
	/*WEST*/
	panel->hidebutton_w=gtk_button_new();
	pixmap_name=gnome_unconditional_pixmap_file("panel-arrow-right.xpm");
	pixmap = gnome_create_pixmap_widget(GTK_WIDGET(panel),
					    panel->hidebutton_w,
					    pixmap_name);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(panel->hidebutton_w),pixmap);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(panel->hidebutton_w), "clicked",
			   GTK_SIGNAL_FUNC(panel_show_hide_left),panel);
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_w,
			 2,3,1,2,GTK_FILL,GTK_FILL,0,0);
	/*SOUTH*/
	panel->hidebutton_s=gtk_button_new();
	pixmap_name=gnome_unconditional_pixmap_file("panel-arrow-down.xpm");
	pixmap = gnome_create_pixmap_widget(GTK_WIDGET(panel),
					    panel->hidebutton_s,
					    pixmap_name);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(panel->hidebutton_s),pixmap);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(panel->hidebutton_s), "clicked",
			   GTK_SIGNAL_FUNC(panel_show_hide_left),panel);
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_s,
			 1,2,2,3,GTK_FILL,GTK_FILL,0,0);

	panel->snapped = snapped;
	panel->orient = orient;
	panel->mode = mode;
	panel->state = state;
	panel->step_size = step_size;
	panel->minimized_size = minimized_size;
	panel->minimize_delay = minimize_delay;

	panel->thick = PANEL_CELL_SIZE;

	/*sanity sets, ignore settings that would/might cause bad behaviour*/
	if(snapped == PANEL_FREE) {
		panel->size = size;
		panel->mode = PANEL_EXPLICIT_HIDE;
		if(panel->state == PANEL_HIDDEN_LEFT ||
		   panel->state == PANEL_HIDDEN_RIGHT)
			panel->state = PANEL_HIDDEN;
	} else if(snapped == PANEL_DRAWER) {
		panel->size = PANEL_DRAWER_DROP_TARGET_SIZE;
		panel->mode = PANEL_EXPLICIT_HIDE;
		if(panel->state == PANEL_HIDDEN_LEFT ||
		   panel->state == PANEL_HIDDEN_RIGHT)
			panel->state = PANEL_HIDDEN;
	} else {
		panel->size = PANEL_MAX;
		/*sanity check*/
		if(panel->mode == PANEL_EXPLICIT_HIDE &&
		   panel->state == PANEL_HIDDEN)
			panel->state = PANEL_SHOWN;
	}


	panel_widget_set_size(panel,panel->size);

	panel_widget_set_hidebuttons(panel);

	for(i=0;i<PANEL_MAX;i++) {
		panel->applets[i].applet = NULL;
		panel->applets[i].cells = 1;
	}

	if(panel->snapped == PANEL_DRAWER) {
		GtkWidget *frame;

		panel->drawer_drop_zone_pos = drop_zone_pos;
		panel->drawer_drop_zone = gtk_event_box_new();
		gtk_widget_show(panel->drawer_drop_zone);
		frame = gtk_frame_new(NULL);
		gtk_container_add(GTK_CONTAINER(panel->drawer_drop_zone),
				  frame);
		gtk_widget_show(frame);
		for(i=0;i<PANEL_DRAWER_DROP_TARGET_SIZE;i++) {
			panel->applets[i].applet = panel->drawer_drop_zone;
			panel->applets[i].cells = PANEL_DRAWER_DROP_TARGET_SIZE;
		}
		gtk_fixed_put(GTK_FIXED(panel->fixed),panel->drawer_drop_zone,
			      0,0);
		drawer_resize_drop_zone(panel);
	}

	if(!fleur_cursor)
		fleur_cursor = gdk_cursor_new(GDK_FLEUR);

	gtk_signal_connect(GTK_OBJECT(panel), "enter_notify_event",
			   GTK_SIGNAL_FUNC(panel_enter_notify),
			   panel);
	gtk_signal_connect(GTK_OBJECT(panel), "leave_notify_event",
			   GTK_SIGNAL_FUNC(panel_leave_notify),
			   panel);
	gtk_signal_connect_after(GTK_OBJECT(panel),
				 "size_allocate",
				 GTK_SIGNAL_FUNC(panel_widget_size_allocate),
				 panel);

	if(snapped == PANEL_FREE)
		move_window(GTK_WIDGET(panel),pos_x,pos_y);

	if(panel->mode == PANEL_AUTO_HIDE)
		panel_widget_pop_down(panel);

	return GTK_WIDGET(panel);
}

void
panel_widget_applet_drag_start_no_grab(PanelWidget *panel, GtkWidget *applet)
{
	panel->currently_dragged_applet = applet;
	panel->currently_dragged_applet_pos =
		panel_widget_get_pos(panel,applet);
}

void
panel_widget_applet_drag_end_no_grab(PanelWidget *panel)
{
	panel->currently_dragged_applet = NULL;
}

void
panel_widget_applet_drag_start(PanelWidget *panel, GtkWidget *applet)
{
	panel_widget_applet_drag_start_no_grab(panel,applet);

	/*if (warp)
	gdk_pointer_warp(NULL, applet->window,
			 0, 0, 0, 0,
			 applet->allocation.width / 2,
			 applet->allocation.height / 2);*/
	
	gtk_grab_add(applet);
	gdk_pointer_grab(applet->window,
			 TRUE,
			 APPLET_EVENT_MASK,
			 NULL,
			 fleur_cursor,
			 GDK_CURRENT_TIME);
}

static void
panel_widget_applet_drag_end(PanelWidget *panel)
{
	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gtk_grab_remove(panel->currently_dragged_applet);
	panel_widget_applet_drag_end_no_grab(panel);
}

int
panel_widget_applet_move_to_cursor(PanelWidget *panel)
{
	if (panel->currently_dragged_applet) {
		gint x,y;
		gint moveby;
		gint pos = panel->currently_dragged_applet_pos;

		gtk_widget_get_pointer(panel->fixed, &x, &y);

		printf("%d %d\n",x,y);

		if(panel->orient == PANEL_HORIZONTAL)
			moveby = (x/PANEL_CELL_SIZE)- pos;
		else
			moveby = (y/PANEL_CELL_SIZE)- pos;

		panel->currently_dragged_applet_pos =
			panel_widget_switch_move(panel, pos, moveby);
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
	PanelWidget *panel = data;
	GdkEventButton *bevent;


	switch (event->type) {
		case GDK_BUTTON_PRESS:
			bevent = (GdkEventButton *) event;

			if (panel->currently_dragged_applet) {
				panel_widget_applet_drag_end(panel);
				return TRUE;
			}

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
			return panel_widget_applet_move_to_cursor(panel);

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
		case GDK_MOTION_NOTIFY:
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
	/*if(!GTK_WIDGET_REALIZED(widget)) {
		gtk_signal_connect_after(GTK_OBJECT(widget),
					 "realize",
					 GTK_SIGNAL_FUNC(bind_applet_events),
					 NULL);
		return;
	}*/
	/* XXX: This is more or less a hack.  We need to be able to
	 * capture events over applets so that we can drag them with
	 * the mouse and such.  So we need to force the applet's
	 * widgets to recursively send the events back to their parent
	 * until the event gets to the applet wrapper (the
	 * GtkEventBox) for processing by us.
	 */
	
	if (!GTK_WIDGET_NO_WINDOW(widget)) {
		/*FIXME: can't be realized!*/
		/*gtk_widget_set_events(widget, gtk_widget_get_events(widget) |
				      APPLET_EVENT_MASK);*/
		gtk_signal_connect(GTK_OBJECT(widget), "event",
				   (GtkSignalFunc) panel_sub_event_handler,
				   NULL);
	}
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, 0);
}

static void
bind_top_applet_events(PanelWidget *panel, GtkWidget *widget)
{
	/*if(!GTK_WIDGET_REALIZED(widget)) {
		gtk_signal_connect_after(GTK_OBJECT(widget),
					 "realize",
					 GTK_SIGNAL_FUNC(
					 	bind_top_applet_events),
					 NULL);
		return;
	}*/
	gtk_signal_connect_after(GTK_OBJECT(widget),
			   	 "size_allocate",
			   	 GTK_SIGNAL_FUNC(
			   	 	panel_widget_applet_size_allocate),
				 panel);

	gtk_signal_connect(GTK_OBJECT(widget),
			   "event",
			   GTK_SIGNAL_FUNC(panel_widget_applet_event),
			   panel);

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


gint
panel_widget_add (PanelWidget *panel, GtkWidget *applet, gint pos)
{
	gint i;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(applet,-1);
	g_return_val_if_fail(pos>=0,-1);

	if(panel->snapped == PANEL_DRAWER) {
		if(pos >= panel->size &&
		   panel->drawer_drop_zone_pos == DROP_ZONE_LEFT) {
			i = panel->size++;
			panel_widget_set_size(panel,panel->size);
		} else {
			if(pos >= panel->size)
				pos = panel->size-1;
			if(panel->drawer_drop_zone_pos == DROP_ZONE_LEFT)
				while(pos<PANEL_MAX &&
				      panel->applets[pos].applet ==
				      panel->drawer_drop_zone)
					pos++;
			for(i=pos;i>=0 && panel->applets[pos].applet ==
			    panel->applets[i].applet;i--)
				;
			i++;
			panel_widget_push_right(panel,i);
		}
	} else {
		if(pos>=panel->size)
			pos = panel->size - 1;

		for(i=pos;i<panel->size;i += panel->applets[i].cells)
			if(!panel->applets[i].applet)
				break;

		/*panel is full to the right*/
		if(i==panel->size) {
			for(i=pos-1;i>=0;i -= panel->applets[i].cells)
				if(!panel->applets[i].applet)
					break;
			/*panel is full!*/
			if(i<=0)
				return -1;
		}
	}

	/*this will get done on size allocate!*/
	gtk_fixed_put(GTK_FIXED(panel->fixed),applet,0,0);
	panel->applets[i].applet = applet;
	panel->applets[i].cells = 1;

	gtk_object_set_data(GTK_OBJECT(applet),PANEL_APPLET_PARENT_KEY,panel);

	bind_top_applet_events(panel,applet);

	return i;
}

gint
panel_widget_move (PanelWidget *panel, gint oldpos, gint pos)
{
	gint i;
	AppletRecord tmp;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(pos>=0,-1);
	g_return_val_if_fail(oldpos>=0,-1);

	tmp.applet = panel->applets[oldpos].applet;
	tmp.cells = panel->applets[oldpos].cells;

	for(i=oldpos;i<oldpos+tmp.cells;i++) {
		panel->applets[i].applet = NULL;
		panel->applets[i].cells = 1;
	}
	
	if(pos>=panel->size)
		pos = panel->size - 1;

	for(i=pos;i<panel->size;i += panel->applets[i].cells)
		if(!panel->applets[i].applet)
			break;

	/*panel is full to the right*/
	if(i>=panel->size) {
		for(i=pos-1;i>=0;i -= panel->applets[i].cells)
			if(!panel->applets[i].applet)
				break;
		/*panel is full!*/
		if(i<=0)
			return -1;
	}

	/*reset size to 1 and adjust the applet*/
	panel->applets[i].applet = tmp.applet;
	panel->applets[i].cells = 1;
	panel_widget_adjust_applet(panel,tmp.applet);

	return i;
}

gint
panel_widget_remove (PanelWidget *panel, GtkWidget *applet)
{
	gint i,n,w;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(applet,-1);

	for(i=0;i<panel->size;i++)
		if(panel->applets[i].applet == applet)
			break;

	/*applet not found*/
	if(i==panel->size)
		return -1;

	w = panel->applets[i].cells;
	for(n=0;n<w;n++) {
		panel->applets[i+n].applet = NULL;
		panel->applets[i+n].cells = 1;
	}

	/*remove applet*/
	gtk_container_remove(GTK_CONTAINER(panel->fixed),applet);

	/*this will trigger size_Allocate of all applets and thus the
	  panel will again be set to the largest thickness*/
	panel->thick = PANEL_CELL_SIZE;
	panel_widget_set_size(panel,panel->size);

	return i;
}

gint
panel_widget_get_pos(PanelWidget *panel, GtkWidget *applet)
{
	gint i;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(applet,-1);

	/*there should never be anything left over panel->size except when
	  doing a resize*/
	for(i=0;i<PANEL_MAX;i++)
		if(panel->applets[i].applet == applet)
			break;

	/*applet not found*/
	if(i==PANEL_MAX)
		return -1;

	return i;
}

GList*
panel_widget_get_applets(PanelWidget *panel)
{
	GList *list=NULL;
	gint i;

	g_return_val_if_fail(panel,NULL);

	for(i=0;i<panel->size;i += panel->applets[i].cells)
		if(panel->applets[i].applet)
			list = g_list_prepend(list,panel->applets[i].applet);

	return list;
}

void
panel_widget_foreach(PanelWidget *panel, GFunc func, gpointer user_data)
{
	gint i;

	g_return_if_fail(panel);
	g_return_if_fail(func);

	for(i=0;i<panel->size;i += panel->applets[i].cells)
		if(panel->applets[i].applet)
			(*func)(panel->applets[i].applet,user_data);
}

/*static void
panel_widget_switch_orient(PanelWidget *panel)
{
	panel->thick = PANEL_CELL_SIZE;
	panel_widget_set_size(panel,panel->size*PANEL_CELL_SIZE);
}*/

void
panel_widget_change_params(PanelWidget *panel,
			   PanelOrientation orient,
			   PanelSnapped snapped,
			   PanelMode mode,
			   PanelState state,
			   gint step_size,
			   gint minimized_size,
			   gint minimize_delay,
			   DrawerDropZonePos drop_zone_pos)
{
	/*FIXME: change drop_zone_pos!!!!!!!!*/
	PanelOrientation oldorient;

	g_return_if_fail(panel);
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(panel)));

	/*the set_size will make it shown, this may change!  it would
	  require more work to keep the state to be persistent accross
	  sessions or even reconfigurations*/
	panel->state = state;
	panel->mode = mode;
	oldorient = panel->orient;

	/*so that there are no shifts necessary before size_allocate*/
	panel->snapped = snapped;
	switch(panel->snapped) {
		case PANEL_FREE:
			panel->orient = orient;
			break;
		case PANEL_TOP:
		case PANEL_BOTTOM:
			panel->size = PANEL_MAX;
			panel->orient = PANEL_HORIZONTAL;
			break;
		case PANEL_LEFT:
		case PANEL_RIGHT:
			panel->size = PANEL_MAX;
			panel->orient = PANEL_VERTICAL;
			break;
	}
	panel_widget_set_hidebuttons(panel);
	panel->thick = PANEL_CELL_SIZE;
	if(panel->mode == PANEL_EXPLICIT_HIDE && panel->state == PANEL_HIDDEN)
		panel->state = PANEL_SHOWN;

	if(panel->snapped == PANEL_DRAWER &&
	   drop_zone_pos != panel->drawer_drop_zone_pos) {
		int i;
		if(drop_zone_pos == DROP_ZONE_LEFT) {
			for(i=PANEL_DRAWER_DROP_TARGET_SIZE;i<panel->size;i++) {
				panel->applets[i-PANEL_DRAWER_DROP_TARGET_SIZE].
					applet = panel->applets[i].applet;
				panel->applets[i-PANEL_DRAWER_DROP_TARGET_SIZE].
					cells = panel->applets[i].cells;
			}
			for(i=panel->size - PANEL_DRAWER_DROP_TARGET_SIZE;
			    i<panel->size;i++) {
				panel->applets[i].applet =
					panel->drawer_drop_zone;
				panel->applets[i].cells =
					PANEL_DRAWER_DROP_TARGET_SIZE;
			}
		} else {
			for(i=panel->size-1-PANEL_DRAWER_DROP_TARGET_SIZE;
			    i>=0;i++) {
				panel->applets[i].applet =
					panel->applets[i+
					PANEL_DRAWER_DROP_TARGET_SIZE].applet;
				panel->applets[i].cells =
					panel->applets[i+
					PANEL_DRAWER_DROP_TARGET_SIZE].cells;
			}
			for(i=0;i<PANEL_DRAWER_DROP_TARGET_SIZE;i++) {
				panel->applets[i].applet =
					panel->drawer_drop_zone;
				panel->applets[i].cells =
					PANEL_DRAWER_DROP_TARGET_SIZE;
			}
		}
		for(i=0;i<panel->size;i+=panel->applets[i].cells)
			panel_widget_applet_put(panel,i);
	}
	panel->drawer_drop_zone_pos = drop_zone_pos;

	panel_widget_set_size(panel,panel->size);

	/*if(oldorient != panel->orient)
		panel_widget_switch_orient(panel);*/

	/*FIXME: notify each applet that we're changing orientation!*/
	/*NOTE: this will probably be handeled by the app itself since
	  there should be no applet<->panel configuration in the widget!
	  we should issue a change orient signal*/

	panel->step_size = step_size;
	panel->minimize_delay = minimize_delay;
	panel->minimized_size = minimized_size;

	if(panel->mode == PANEL_AUTO_HIDE)
		panel_widget_pop_down(panel);
}

#if 0

int
main(int argc, char **argv)
{
	GtkWidget *panel;
	GtkWidget *button;

	gnome_init("panel-widget", &argc, &argv);
	textdomain(PACKAGE);

	panel = panel_widget_new(0,PANEL_HORIZONTAL,PANEL_BOTTOM,PANEL_EXPLICIT_HIDE,PANEL_SHOWN,5,5,100);

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
