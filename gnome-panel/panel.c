/* Gnome panel: Initialization routines
 * (C) 1997 the Free Software Foundation
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 *          George Lebl
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <gnome.h>

#include "panel-include.h"

#define PANEL_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

/*list of all panel widgets created*/
GList *panel_list = NULL;

/*the timeout handeler for panel dragging id,
  yes I am too lazy to get the events to work*/
static int panel_dragged = 0;

/*the number of base panels out there, never let it go below 1*/
int base_panels = 0;

extern int config_sync_timeout;
extern GList *applets_to_sync;
extern int panels_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

extern GArray *applets;
extern int applet_count;

extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

extern GtkTooltips *panel_tooltips;

extern GnomeClient *client;

extern GlobalConfig global_config;

/*???? this might be ugly, but I guess we can safely assume that we can only
  have one menu open and that nothing weird will happen to the panel that
  opened that menu whilethe user is looking over the choices*/
extern PanelWidget *current_panel;

/*a list of started extern applet child processes*/
extern GList * children;

/*get the default panel widget if the panel has more then one or
  just get the that one*/
PanelWidget *
get_def_panel_widget(GtkWidget *panel)
{
	g_return_val_if_fail(panel!=NULL,NULL);
	if(IS_SNAPPED_WIDGET(panel)) {
		return PANEL_WIDGET(SNAPPED_WIDGET(panel)->panel);
	} else if(IS_CORNER_WIDGET(panel)) {
		return PANEL_WIDGET(CORNER_WIDGET(panel)->panel);
	} else if(IS_DRAWER_WIDGET(panel)) {
		return PANEL_WIDGET(DRAWER_WIDGET(panel)->panel);
	}
	puts("get_def_panel");
	g_warning("unknown panel type");
	return NULL;
}

static PanelData *
get_lowest_level_master_pd(PanelWidget *panel)
{
	GtkObject *parent;
	PanelData *pd;

	while(panel->master_widget)
		panel = gtk_object_get_data(GTK_OBJECT(panel->master_widget),
					    PANEL_APPLET_PARENT_KEY);
	parent = gtk_object_get_data(GTK_OBJECT(panel),PANEL_PARENT);
	g_return_val_if_fail(parent!=NULL,NULL);
	
	pd = gtk_object_get_user_data(parent);
	g_return_val_if_fail(pd!=NULL,NULL);
	
	return pd;
}

/*whoa ... what an ugly function!, it gets the right orient type
 for an applet on the panel for pd*/
PanelOrientType
get_applet_orient(PanelWidget *panel)
{
	PanelOrientType orient=ORIENT_UP;
	PanelOrientation porient;
	GtkWidget *panelw = gtk_object_get_data(GTK_OBJECT(panel),
						PANEL_PARENT);
	PanelData *pd = gtk_object_get_user_data(GTK_OBJECT(panelw));
	PanelData *tpd;
	switch(pd->type) {
	/*case FREE_PANEL:
		orient = (panel->orient==PANEL_VERTICAL)?
			ORIENT_RIGHT:ORIENT_UP;
		break;*/
	case DRAWER_PANEL:
		porient = PANEL_WIDGET(DRAWER_WIDGET(panelw)->panel)->orient;
		tpd = get_lowest_level_master_pd(
			PANEL_WIDGET(DRAWER_WIDGET(panelw)->panel));
		switch(tpd->type){
		case FREE_PANEL:
		case DRAWER_PANEL:
			orient=(porient==PANEL_VERTICAL)?
				ORIENT_RIGHT:ORIENT_UP;
			break;
		case SNAPPED_PANEL:
			switch(SNAPPED_WIDGET(tpd->panel)->pos) {
			case SNAPPED_TOP:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_RIGHT:ORIENT_DOWN;
				break;
			case SNAPPED_BOTTOM:
			case SNAPPED_LEFT:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_RIGHT:ORIENT_UP;
				break;
			case SNAPPED_RIGHT:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_LEFT:ORIENT_UP;
				break;
			}
			break;
		case CORNER_PANEL:
			switch(CORNER_WIDGET(tpd->panel)->pos) {
			case CORNER_NE:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_LEFT:ORIENT_DOWN;
				break;
			case CORNER_SE:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_LEFT:ORIENT_UP;
				break;
			case CORNER_SW:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_RIGHT:ORIENT_UP;
				break;
			case CORNER_NW:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_RIGHT:ORIENT_DOWN;
				break;
			}
		default: break;
		}
		break;
	case SNAPPED_PANEL:
		switch(SNAPPED_WIDGET(panelw)->pos) {
		case SNAPPED_TOP: orient = ORIENT_DOWN; break;
		case SNAPPED_BOTTOM: orient = ORIENT_UP; break;
		case SNAPPED_LEFT: orient = ORIENT_RIGHT; break;
		case SNAPPED_RIGHT: orient = ORIENT_LEFT; break;
		}
		break;
	case CORNER_PANEL:
		if(PANEL_WIDGET(CORNER_WIDGET(panelw)->panel)->orient ==
		   PANEL_HORIZONTAL) {
			switch(CORNER_WIDGET(panelw)->pos) {
			case CORNER_SE: 
			case CORNER_SW:
				orient = ORIENT_UP;
				break;
			case CORNER_NE:
			case CORNER_NW:
				orient = ORIENT_DOWN;
				break;
			}
		} else { /*vertical*/
			switch(CORNER_WIDGET(panelw)->pos) {
			case CORNER_SE: 
			case CORNER_NE:
				orient = ORIENT_LEFT;
				break;
			case CORNER_SW:
			case CORNER_NW:
				orient = ORIENT_RIGHT;
				break;
			}
		}
	default: break;
	}
	return orient;
}

void
change_window_cursor(GdkWindow *window, GdkCursorType cursor_type)
{
	GdkCursor *cursor = gdk_cursor_new(cursor_type);
	gdk_window_set_cursor(window, cursor);
	gdk_cursor_destroy(cursor);
}

static void
panel_realize(GtkWidget *widget, gpointer data)
{
	change_window_cursor(widget->window, GDK_ARROW);
	
	if(IS_SNAPPED_WIDGET(widget))
		snapped_widget_enable_buttons(SNAPPED_WIDGET(widget));
	else if(IS_CORNER_WIDGET(widget))
		corner_widget_enable_buttons(CORNER_WIDGET(widget));
}

/*we call this recursively*/
static void orient_change_foreach(gpointer data, gpointer user_data);

void
orientation_change(int applet_id, PanelWidget *panel)
{
	AppletInfo *info = get_applet_info(applet_id);
	if(info->type == APPLET_EXTERN) {
		Extern *ext = info->data;
		g_assert(ext);
		/*ingore this until we get an ior*/
		if(ext->ior)
			send_applet_change_orient(ext->ior,info->applet_id,
						  get_applet_orient(panel));
	} else if(info->type == APPLET_MENU) {
		Menu *menu = info->data;
		set_menu_applet_orient(menu,get_applet_orient(panel));
	} else if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		DrawerWidget *dw = DRAWER_WIDGET(drawer->drawer);
		reposition_drawer(drawer);
		set_drawer_applet_orient(drawer,get_applet_orient(panel));
		panel_widget_foreach(PANEL_WIDGET(dw->panel),
				     orient_change_foreach,
				     (gpointer)dw->panel);
	} else if(info->type == APPLET_SWALLOW) {
		Swallow *swallow = info->data;

		if(panel->orient == PANEL_VERTICAL)
			set_swallow_applet_orient(swallow,SWALLOW_VERTICAL);
		else
			set_swallow_applet_orient(swallow,SWALLOW_HORIZONTAL);
	}
}

static void
orient_change_foreach(gpointer data, gpointer user_data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(data)));
	PanelWidget *panel = user_data;
	
	orientation_change(applet_id,panel);
}


static void
panel_orient_change(GtkWidget *widget,
		    PanelOrientation orient,
		    gpointer data)
{
	panel_widget_foreach(PANEL_WIDGET(widget),orient_change_foreach,
			     widget);
	panels_to_sync = TRUE;
	/*update the configuration box if it is displayed*/
	update_config_orient(gtk_object_get_data(GTK_OBJECT(widget),
						 PANEL_PARENT));
}

static void
snapped_pos_change(GtkWidget *widget,
		   SnappedPos pos,
		   gpointer data)
{
	panel_widget_foreach(PANEL_WIDGET(SNAPPED_WIDGET(widget)->panel),
			     orient_change_foreach,
			     SNAPPED_WIDGET(widget)->panel);
	panels_to_sync = TRUE;
	/*update the configuration box if it is displayed*/
	update_config_orient(widget);
}

static void
corner_pos_change(GtkWidget *widget,
		  CornerPos pos,
		  gpointer data)
{
	panel_widget_foreach(PANEL_WIDGET(CORNER_WIDGET(widget)->panel),
			     orient_change_foreach,
			     CORNER_WIDGET(widget)->panel);
	panels_to_sync = TRUE;
	/*update the configuration box if it is displayed*/
	update_config_orient(widget);
}

void
back_change(int applet_id,
	    PanelWidget *panel)
{
  
	AppletInfo *info = get_applet_info(applet_id);
	if(info->type == APPLET_EXTERN) {
		Extern *ext = info->data;
		g_assert(ext);
		/*ignore until we have a valid IOR*/
		if(ext->ior)
			send_applet_change_back(ext->ior, info->applet_id,
						panel->back_type,panel->back_pixmap,
						&panel->back_color);
	}
	/*FIXME: probably set other backgrounds as well*/
}


static void
back_change_foreach(gpointer data, gpointer user_data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(data)));
	PanelWidget *panel = user_data;

	back_change(applet_id,panel);
}

static void
panel_back_change(GtkWidget *widget,
		  PanelBackType type,
		  char *pixmap,
		  GdkColor *color)
{
	panel_widget_foreach(PANEL_WIDGET(widget),back_change_foreach,widget);
	panels_to_sync = TRUE;
	/*update the configuration box if it is displayed*/
	update_config_back(PANEL_WIDGET(widget));
}

static void state_hide_foreach(gpointer data, gpointer user_data);

static void
state_restore_foreach(gpointer data, gpointer user_data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(data)));
	AppletInfo *info = get_applet_info(applet_id);
	
	if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		DrawerWidget *dw = DRAWER_WIDGET(drawer->drawer);
		if(dw->state == DRAWER_SHOWN) {
			drawer_widget_restore_state(dw);
			panel_widget_foreach(PANEL_WIDGET(dw->panel),
					     state_restore_foreach,
					     NULL);
		} else { /*it's hidden*/
			gtk_widget_hide(GTK_WIDGET(dw));
			panel_widget_foreach(PANEL_WIDGET(dw->panel),
					     state_hide_foreach,
					     NULL);
		}
	}
}

static void
state_hide_foreach(gpointer data, gpointer user_data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(data)));
	AppletInfo *info = get_applet_info(applet_id);

	if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		DrawerWidget *dw = DRAWER_WIDGET(drawer->drawer);
		gtk_widget_hide(GTK_WIDGET(dw));
		panel_widget_foreach(PANEL_WIDGET(dw->panel),
				     state_hide_foreach,
				     NULL);
	}
}

static int
snapped_state_change(GtkWidget *widget,
		     SnappedState state,
		     gpointer data)
{
	if(state==SNAPPED_SHOWN)
		panel_widget_foreach(PANEL_WIDGET(SNAPPED_WIDGET(widget)->panel),
				     state_restore_foreach,
				     (gpointer)widget);
	else
		panel_widget_foreach(PANEL_WIDGET(SNAPPED_WIDGET(widget)->panel),
				     state_hide_foreach,
				     (gpointer)widget);

	panels_to_sync = TRUE;

	return TRUE;
}
static int
corner_state_change(GtkWidget *widget,
		    CornerState state,
		    gpointer data)
{
	if(state==CORNER_SHOWN)
		panel_widget_foreach(PANEL_WIDGET(CORNER_WIDGET(widget)->panel),
				     state_restore_foreach,
				     (gpointer)widget);
	else
		panel_widget_foreach(PANEL_WIDGET(CORNER_WIDGET(widget)->panel),
				     state_hide_foreach,
				     (gpointer)widget);

	panels_to_sync = TRUE;

	return TRUE;
}
static int
drawer_state_change(GtkWidget *widget,
		    DrawerState state,
		    gpointer data)
{
	if(state==DRAWER_SHOWN)
		panel_widget_foreach(PANEL_WIDGET(DRAWER_WIDGET(widget)->panel),
				     state_restore_foreach,
				     (gpointer)widget);
	else
		panel_widget_foreach(PANEL_WIDGET(DRAWER_WIDGET(widget)->panel),
				     state_hide_foreach,
				     (gpointer)widget);

	panels_to_sync = TRUE;

	return TRUE;
}

static int
panel_size_allocate(GtkWidget *widget, GtkAllocation *alloc, gpointer data)
{
	if(IS_DRAWER_WIDGET(widget)) {
		Drawer *drawer = gtk_object_get_data(GTK_OBJECT(widget),
						     DRAWER_PANEL_KEY);
		if(!drawer)
			return FALSE;

		if(DRAWER_WIDGET(widget)->state == DRAWER_SHOWN)
			reposition_drawer(drawer);
		panels_to_sync = TRUE;
	}
	return FALSE;
}

/*the following is slightly ugly .... but it works, I need to send the
  orient change in an idle handeler*/
static int
panel_applet_added_idle(gpointer data)
{
	int applet_id = GPOINTER_TO_INT(data);
	AppletInfo *info = get_applet_info(applet_id);
	PanelWidget *panel = gtk_object_get_data(GTK_OBJECT(info->widget),
						 PANEL_APPLET_PARENT_KEY);

	orientation_change(applet_id,panel);

	return FALSE;
}

static void
panel_applet_added(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(applet)));
	AppletInfo *info = get_applet_info(applet_id);
	GtkWidget *panelw = gtk_object_get_data(GTK_OBJECT(widget),
						PANEL_PARENT);
	
	/*on a real add the info will be NULL as the only adding
	  is done in register_toy and that doesn't add the info to the
	  array until after the add, so we can be sure this was
	  generated on a reparent*/
	if(IS_SNAPPED_WIDGET(panelw) &&
	   info && info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		DrawerWidget *dw = DRAWER_WIDGET(drawer->drawer);
		if(dw->state == DRAWER_SHOWN)
			SNAPPED_WIDGET(panelw)->drawers_open++;
	}

	/*pop the panel up on addition*/
	if(IS_SNAPPED_WIDGET(panelw)) {
		snapped_widget_pop_up(SNAPPED_WIDGET(panelw));
		/*try to pop down though if the mouse is out*/
		snapped_widget_queue_pop_down(SNAPPED_WIDGET(panelw));
	}

	gtk_idle_add(panel_applet_added_idle,GINT_TO_POINTER(applet_id));
}

static void
count_open_drawers(gpointer data, gpointer user_data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(data)));
	AppletInfo *info = get_applet_info(applet_id);
	int *count = user_data;
	if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		if(DRAWER_WIDGET(drawer->drawer)->state == DRAWER_SHOWN)
			(*count)++;
	}
}

static void
panel_applet_removed(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	GtkWidget *parentw = gtk_object_get_data(GTK_OBJECT(widget),
						 PANEL_PARENT);
	int applet_id =
		GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(applet)));

	if(IS_SNAPPED_WIDGET(parentw)) {
		int drawers_open = 0;

		panel_widget_foreach(PANEL_WIDGET(widget),
				     count_open_drawers,
				     &drawers_open);
		SNAPPED_WIDGET(parentw)->drawers_open = drawers_open;
		snapped_widget_queue_pop_down(SNAPPED_WIDGET(parentw));
	}

	/*we will need to save this applet's config now*/
	if(g_list_find(applets_to_sync, GINT_TO_POINTER(applet_id))==NULL)
		applets_to_sync = g_list_prepend(applets_to_sync,
						 GINT_TO_POINTER(applet_id));
}

static void
panel_menu_position (GtkMenu *menu, int *x, int *y, gpointer data)
{
	GtkWidget *w = data;
	int wx, wy;

	g_return_if_fail(w != NULL);

	gdk_window_get_origin (w->window, &wx, &wy);

	if(IS_DRAWER_WIDGET(w)) {
		PanelWidget *panel = PANEL_WIDGET(DRAWER_WIDGET(w)->panel);
		if(panel->orient==PANEL_VERTICAL) {
			gtk_widget_get_pointer(w, NULL, y);
			*x = wx + w->allocation.width;
			*y += wy;
		} else {
			gtk_widget_get_pointer(w, x, NULL);
			*x += wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
		}
	} else if(IS_SNAPPED_WIDGET(w)) {
		switch(SNAPPED_WIDGET(w)->pos) {
		case SNAPPED_BOTTOM:
			gtk_widget_get_pointer(w, x, NULL);
			*x += wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
			break;
		case SNAPPED_TOP:
			gtk_widget_get_pointer(w, x, NULL);
			*x += wx;
			*y = wy + w->allocation.height;
			break;
		case SNAPPED_LEFT:
			gtk_widget_get_pointer(w, NULL, y);
			*x = wx + w->allocation.width;
			*y += wy;
			break;
		case SNAPPED_RIGHT:
			gtk_widget_get_pointer(w, NULL, y);
			*x = wx - GTK_WIDGET (menu)->allocation.width;
			*y += wy;
			break;
		}
	} else if(IS_CORNER_WIDGET(w)) {
		PanelWidget *panel = PANEL_WIDGET(CORNER_WIDGET(w)->panel);
		if(panel->orient==PANEL_HORIZONTAL) {
			switch(CORNER_WIDGET(w)->pos) {
			case CORNER_NE:
			case CORNER_NW:
				gtk_widget_get_pointer(w, x, NULL);
				*x += wx;
				*y = wy + w->allocation.height;
				break;
			case CORNER_SE:
			case CORNER_SW:
				gtk_widget_get_pointer(w, x, NULL);
				*x += wx;
				*y = wy - GTK_WIDGET (menu)->allocation.height;
				break;
			}
		} else { /*vertical*/
			switch(CORNER_WIDGET(w)->pos) {
			case CORNER_NE:
			case CORNER_SE:
				gtk_widget_get_pointer(w, NULL, y);
				*x = wx - GTK_WIDGET (menu)->allocation.width;
				*y += wy;
				break;
			case CORNER_SW:
			case CORNER_NW:
				gtk_widget_get_pointer(w, NULL, y);
				*x = wx + w->allocation.width;
				*y += wy;
				break;
			}
		}
	}

	if(*x + GTK_WIDGET (menu)->allocation.width > gdk_screen_width())
		*x=gdk_screen_width() - GTK_WIDGET (menu)->allocation.width;
	if(*x < 0) *x =0;

	if(*y + GTK_WIDGET (menu)->allocation.height > gdk_screen_height())
		*y=gdk_screen_height() - GTK_WIDGET (menu)->allocation.height;
	if(*y < 0) *y =0;
}

static void
menu_deactivate(GtkWidget *w, GtkWidget *panel)
{
	if(IS_SNAPPED_WIDGET(panel))
		SNAPPED_WIDGET(panel)->autohide_inhibit = FALSE;
}

static void
snapped_panel_move(SnappedWidget *snapped, double x, double y)
{
	SnappedPos newloc;
	int minx, miny, maxx, maxy;

	gdk_window_get_geometry (GTK_WIDGET(snapped)->window,
				 &minx, &miny, &maxx, &maxy, NULL);
	gdk_window_get_origin (GTK_WIDGET(snapped)->window, &minx, &miny);
	maxx += minx;
	maxy += miny;
	if (x >= minx &&
	    x <= maxx &&
	    y >= miny &&
	    y <= maxy)
 	        return;

	if ((x) * gdk_screen_height() > y * gdk_screen_width() ) {
		if(gdk_screen_height() * (gdk_screen_width()-(x)) >
		   y * gdk_screen_width() )
			newloc = SNAPPED_TOP;
		else
			newloc = SNAPPED_RIGHT;
	} else {
		if(gdk_screen_height() * (gdk_screen_width()-(x)) >
		   y * gdk_screen_width() )
			newloc = SNAPPED_LEFT;
		else
			newloc = SNAPPED_BOTTOM;
	}
	if(newloc != snapped->pos)
		snapped_widget_change_pos(snapped, newloc);
}

static void
corner_panel_move(CornerWidget *corner, double x, double y)
{
	CornerPos newloc;
	PanelOrientation neworient;
	int minx, miny, maxx, maxy;

	gdk_window_get_geometry (GTK_WIDGET(corner)->window,
				 &minx, &miny, &maxx, &maxy, NULL);
	gdk_window_get_origin (GTK_WIDGET(corner)->window, &minx, &miny);
	maxx += minx;
	maxy += miny;
	if (x >= minx &&
	    x <= maxx &&
	    y >= miny &&
	    y <= maxy)
 	        return;

	if ((x) * gdk_screen_height() > y * gdk_screen_width() ) {
		if(gdk_screen_height() * (gdk_screen_width()-(x)) >
		   y * gdk_screen_width() ) {
			neworient = PANEL_HORIZONTAL;
			if(x<gdk_screen_width()/2)
				newloc = CORNER_NW;
			else
				newloc = CORNER_NE;
		} else {
			neworient = PANEL_VERTICAL;
			if(y<gdk_screen_height()/2)
				newloc = CORNER_NE;
			else
				newloc = CORNER_SE;
		}
	} else {
		if(gdk_screen_height() * (gdk_screen_width()-(x)) >
		   y * gdk_screen_width() ) {
			neworient = PANEL_VERTICAL;
			if(y<gdk_screen_height()/2)
				newloc = CORNER_NW;
			else
				newloc = CORNER_SW;
		} else {
			neworient = PANEL_HORIZONTAL;
			if(x<gdk_screen_width()/2)
				newloc = CORNER_SW;
			else
				newloc = CORNER_SE;
		}
	}
	if(newloc != corner->pos ||
	   neworient != PANEL_WIDGET(corner->panel)->orient)
		corner_widget_change_pos_orient(corner, newloc, neworient);
}

static int
snapped_panel_move_timeout(gpointer data)
{
	int x,y;

	gdk_window_get_pointer(NULL,&x,&y,NULL);
	snapped_panel_move(data,x,y);
	return TRUE;
}
static int
corner_panel_move_timeout(gpointer data)
{
	int x,y;

	gdk_window_get_pointer(NULL,&x,&y,NULL);
	corner_panel_move(data,x,y);
	return TRUE;
}

static int
panel_destroy(GtkWidget *widget, gpointer data)
{
	PanelData *pd = gtk_object_get_user_data(GTK_OBJECT(widget));
	GtkWidget *panel_menu = data;

	if(IS_DRAWER_WIDGET(widget)) {
		PanelWidget *panel = PANEL_WIDGET(DRAWER_WIDGET(widget)->panel);
		if(panel->master_widget) {
			int applet_id =
				GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(panel->master_widget)));
			AppletInfo *info = get_applet_info(applet_id);
			Drawer *drawer = info->data;
			drawer->drawer = NULL;
			panel_clean_applet(applet_id);
		}
	} else if(IS_SNAPPED_WIDGET(widget) ||
		  IS_CORNER_WIDGET(widget)) {
		/*this is a base panel and we just lost it*/
		base_panels--;
	}

	if(panel_menu)
		gtk_widget_unref(panel_menu);
	
	panel_list = g_list_remove(panel_list,pd);
	g_free(pd);

	return FALSE;
}

static void
panel_applet_move(GtkWidget *panel,GtkWidget *widget, gpointer data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(widget)));
	
	if(g_list_find(applets_to_sync, GINT_TO_POINTER(applet_id))==NULL)
		applets_to_sync = g_list_prepend(applets_to_sync,
						 GINT_TO_POINTER(applet_id));
}


static int
panel_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GdkEventButton *bevent;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		bevent = (GdkEventButton *) event;
		switch(bevent->button) {
		case 3: /* fall through */
		case 1:
			if(!panel_applet_in_drag) {
				GtkWidget *rem = 
					gtk_object_get_data(GTK_OBJECT(widget),
							    "remove_item");
				if(IS_SNAPPED_WIDGET(widget)) {
					SnappedWidget *snapped =
						SNAPPED_WIDGET(widget);
					current_panel =
						PANEL_WIDGET(snapped->panel);
					if(base_panels <= 1)
						gtk_widget_set_sensitive(rem,
									 FALSE);
					else
						gtk_widget_set_sensitive(rem,
									 TRUE);
					snapped->autohide_inhibit = TRUE;
					snapped_widget_queue_pop_down(snapped);
				} else if(IS_CORNER_WIDGET(widget)) {
					CornerWidget *corner =
						CORNER_WIDGET(widget);
					current_panel = PANEL_WIDGET(corner->panel);
					if(base_panels <= 1)
						gtk_widget_set_sensitive(rem,
									 FALSE);
					else
						gtk_widget_set_sensitive(rem,
									 TRUE);
				} else if(IS_DRAWER_WIDGET(widget)) {
					DrawerWidget *drawer =
						DRAWER_WIDGET(widget);
					current_panel = PANEL_WIDGET(drawer->panel);
					gtk_widget_set_sensitive(rem, TRUE);
				} else
					gtk_widget_set_sensitive(rem,
								 TRUE);
				gtk_menu_popup(GTK_MENU(data), NULL, NULL,
					       panel_menu_position,
					       widget, bevent->button,
					       bevent->time);
				return TRUE;
			}
			break;
		case 2:
			/*this should probably be in snapped widget*/
			if(!panel_dragged &&
			   (IS_SNAPPED_WIDGET(widget) ||
			    IS_CORNER_WIDGET(widget))) {
				GdkCursor *cursor = gdk_cursor_new (GDK_FLEUR);
				gtk_grab_add(widget);
				gdk_pointer_grab (widget->window,
						  FALSE,
						  PANEL_EVENT_MASK,
						  NULL,
						  cursor,
						  bevent->time);
				gdk_cursor_destroy (cursor);
				if(IS_SNAPPED_WIDGET(widget)) {
					SNAPPED_WIDGET(widget)->autohide_inhibit = TRUE;
					panel_dragged =
						gtk_timeout_add(30,
								snapped_panel_move_timeout,
								widget);
				} else { /*CORNER_WIDGET*/
					panel_dragged =
						gtk_timeout_add(30,
								corner_panel_move_timeout,
								widget);
				}
				return TRUE;
			}
			break;
		}
		break;

	case GDK_BUTTON_RELEASE:
		bevent = (GdkEventButton *) event;
		if(panel_dragged) {
			if(IS_SNAPPED_WIDGET(widget)) {
				snapped_panel_move(SNAPPED_WIDGET(widget),
					   bevent->x_root, bevent->y_root);
				SNAPPED_WIDGET(widget)->autohide_inhibit = FALSE;
				snapped_widget_queue_pop_down(SNAPPED_WIDGET(widget));
			} else
				corner_panel_move(CORNER_WIDGET(widget),
					   bevent->x_root, bevent->y_root);
			gdk_pointer_ungrab(bevent->time);
			gtk_grab_remove(widget);
			gtk_timeout_remove(panel_dragged);
			panel_dragged = 0;
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
	GdkEventButton *bevent;
	switch (event->type) {
		/*pass these to the parent!*/
		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
			bevent = (GdkEventButton *) event;
			/*if the widget is a button we want to keep the
			  button 1 events*/
			if(!GTK_IS_BUTTON(widget) || bevent->button!=1)
				return gtk_widget_event(
					listening_parent(widget->parent),
							 event);

			break;

		default:
			break;
	}

	return FALSE;
}


static void
bind_panel_events(GtkWidget *widget, gpointer data)
{
	if (!GTK_WIDGET_NO_WINDOW(widget))
		gtk_signal_connect(GTK_OBJECT(widget), "event",
				   (GtkSignalFunc) panel_sub_event_handler,
				   NULL);
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_panel_events, NULL);
}



static void
panel_widget_setup(PanelWidget *panel)
{
	gtk_signal_connect(GTK_OBJECT(panel),
			   "applet_added",
			   GTK_SIGNAL_FUNC(panel_applet_added),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "applet_removed",
			   GTK_SIGNAL_FUNC(panel_applet_removed),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "applet_move",
			   GTK_SIGNAL_FUNC(panel_applet_move),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "back_change",
			   GTK_SIGNAL_FUNC(panel_back_change),
			   NULL);
}

void
panel_setup(GtkWidget *panelw)
{
	GtkWidget *panel_menu;
	PanelData *pd;
	
	pd = g_new(PanelData,1);
	pd->panel = panelw;
	if(IS_DRAWER_WIDGET(panelw))
		pd->type = DRAWER_PANEL;
	else if(IS_SNAPPED_WIDGET(panelw))
		pd->type = SNAPPED_PANEL;
	else if(IS_CORNER_WIDGET(panelw))
		pd->type = CORNER_PANEL;
	else {
		puts("panel_setup");
		g_warning("unknown panel type");
	}
	
	panel_list = g_list_append(panel_list,pd);
	
	gtk_object_set_user_data(GTK_OBJECT(panelw),pd);

	panel_menu = create_panel_root_menu(panelw);

	if(IS_DRAWER_WIDGET(panelw)) {
		PanelWidget *panel = PANEL_WIDGET(DRAWER_WIDGET(panelw)->panel);
		panel_widget_setup(panel);
		gtk_signal_connect(GTK_OBJECT(panel),
				   "orient_change",
				   GTK_SIGNAL_FUNC(panel_orient_change),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(panelw),
				   "state_change",
				   GTK_SIGNAL_FUNC(drawer_state_change),
				   NULL);
	} else if(IS_SNAPPED_WIDGET(panelw)) {
		PanelWidget *panel =
			PANEL_WIDGET(SNAPPED_WIDGET(panelw)->panel);
		snapped_widget_disable_buttons(SNAPPED_WIDGET(panelw));
		panel_widget_setup(panel);
		gtk_signal_connect(GTK_OBJECT(panelw),
				   "pos_change",
				   GTK_SIGNAL_FUNC(snapped_pos_change),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(panelw),
				   "state_change",
				   GTK_SIGNAL_FUNC(snapped_state_change),
				   NULL);
		
		/*this is a base panel*/
		base_panels++;
	} else if(IS_CORNER_WIDGET(panelw)) {
		PanelWidget *panel =
			PANEL_WIDGET(CORNER_WIDGET(panelw)->panel);
		corner_widget_disable_buttons(CORNER_WIDGET(panelw));
		panel_widget_setup(panel);
		gtk_signal_connect(GTK_OBJECT(panel),
				   "orient_change",
				   GTK_SIGNAL_FUNC(panel_orient_change),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(panelw),
				   "pos_change",
				   GTK_SIGNAL_FUNC(corner_pos_change),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(panelw),
				   "state_change",
				   GTK_SIGNAL_FUNC(corner_state_change),
				   NULL);
		
		/*this is a base panel*/
		base_panels++;
	} else {
		puts("panel_setup later");
		g_warning("unknown panel type");
	}
	
	gtk_widget_set_events(panelw,
			      gtk_widget_get_events(panelw) |
			      PANEL_EVENT_MASK);

	gtk_signal_connect(GTK_OBJECT(panelw),
			   "size_allocate",
			   GTK_SIGNAL_FUNC(panel_size_allocate),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panelw),
			   "destroy",
			   GTK_SIGNAL_FUNC(panel_destroy),
			   panel_menu);

	/*with this we capture button presses throughout all the widgets of the
	  panel*/
	gtk_signal_connect(GTK_OBJECT(panelw),
			   "event",
			   GTK_SIGNAL_FUNC(panel_event),
			   panel_menu);
	if (GTK_IS_CONTAINER(panelw))
		gtk_container_foreach (GTK_CONTAINER (panelw),
				       bind_panel_events, NULL);

	gtk_signal_connect(GTK_OBJECT(panel_menu),
			   "deactivate",
			   GTK_SIGNAL_FUNC(menu_deactivate),
			   panelw);

	if(GTK_WIDGET_REALIZED(GTK_WIDGET(panelw)))
		panel_realize(GTK_WIDGET(panelw),NULL);
	else
		gtk_signal_connect_after(GTK_OBJECT(panelw), "realize",
					 GTK_SIGNAL_FUNC(panel_realize),
					 NULL);
}

/*send state change to all the panels*/
void
send_state_change(void)
{
	GList *list;
	for(list = panel_list; list != NULL; list = g_list_next(list)) {
		PanelData *pd = list->data;
		if(IS_SNAPPED_WIDGET(pd->panel))
			snapped_state_change(pd->panel,
					     SNAPPED_WIDGET(pd->panel)->state,
					     NULL);
		else if(IS_CORNER_WIDGET(pd->panel))
			corner_state_change(pd->panel,
					    CORNER_WIDGET(pd->panel)->state,
					    NULL);
	}
}
