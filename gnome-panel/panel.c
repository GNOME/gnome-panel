/* Gnome panel: Initialization routines
 * (C) 1997,1998,1999,2000 the Free Software Foundation
 * (C) 2000 Eazel, Inc.
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 *          George Lebl
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <libgnome/libgnome.h>
#include <libbonobo.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-file-info.h>

#include "panel.h"

#include "applet.h"
#include "button-widget.h"
#include "distribution.h"
#include "drawer-widget.h"
#include "edge-widget.h"
#include "floating-widget.h"
#include "foobar-widget.h"
#include "gnome-run.h"
#include "launcher.h"
#include "logout.h"
#include "menu-fentry.h"
#include "menu-util.h"
#include "menu.h"
#include "panel-util.h"
#include "panel-config.h"
#include "panel-config-global.h"
#include "panel-gconf.h"
#include "session.h"
#include "status.h"
#include "swallow.h"
#include "panel-applet-frame.h"
#include "global-keys.h"

#define PANEL_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

#undef PANEL_SESSION_DEBUG

/*list of all panel widgets created*/
GSList *panel_list = NULL;

static gboolean panel_dragged = FALSE;
static int panel_dragged_timeout = -1;
static gboolean panel_been_moved = FALSE;

/*the number of base panels (corner/snapped) out there, never let it
  go below 1*/
int base_panels = 0;

extern GSList *applets;

extern int applets_to_sync;
extern int panels_to_sync;

extern gboolean commie_mode;

extern GtkTooltips *panel_tooltips;

extern char *kde_menudir;




extern GlobalConfig global_config;

/*the types of stuff we accept*/

enum {
	TARGET_URL,
	TARGET_NETSCAPE_URL,
	TARGET_DIRECTORY,
	TARGET_COLOR,
	TARGET_APPLET,
	TARGET_APPLET_INTERNAL,
	TARGET_ICON_INTERNAL,
	TARGET_BGIMAGE,
	TARGET_BACKGROUND_RESET,
};

/* FIXME : Need to do some good error checking on all of these variables */

static GConfEnumStringPair panel_type_type_enum_map [] = {
	{ EDGE_PANEL,      "edge-panel" },
	{ DRAWER_PANEL,    "drawer-panel" },
	{ ALIGNED_PANEL,   "aligned-panel" },
	{ SLIDING_PANEL,   "sliding-panel" },
	{ FLOATING_PANEL,  "floating-panel" },
	{ FOOBAR_PANEL,	   "menu-panel" },
};

static GConfEnumStringPair background_type_enum_map [] = {
	{ PANEL_BACK_NONE,   "no-background" },
	{ PANEL_BACK_COLOR,  "color-background" },
	{ PANEL_BACK_PIXMAP, "pixmap-background" },
};

static GConfEnumStringPair panel_size_type_enum_map [] = {
	{ PANEL_SIZE_XX_SMALL, "panel-size-xx-small" },
	{ PANEL_SIZE_X_SMALL,  "panel-size-x-small" },
	{ PANEL_SIZE_SMALL,    "panel-size-small" },
	{ PANEL_SIZE_MEDIUM,   "panel-size-medium" },
	{ PANEL_SIZE_LARGE,    "panel-size-large" },
	{ PANEL_SIZE_X_LARGE,  "panel-size-x-large" },
	{ PANEL_SIZE_XX_LARGE, "panel-size-xx-large" },
};

static GConfEnumStringPair panel_edge_type_enum_map [] = {
	{ BORDER_TOP,    "panel-edge-top" },
	{ BORDER_RIGHT,  "panel-edge-right" },
	{ BORDER_BOTTOM, "panel-edge-bottom" },
	{ BORDER_LEFT,   "panel-edge-left" },
};

static GConfEnumStringPair panel_alignment_type_enum_map [] = {
	{ ALIGNED_LEFT,   "panel-alignment-left" },
	{ ALIGNED_CENTER, "panel-alignment-center" },
	{ ALIGNED_RIGHT,  "panel-alignment-right" },
};

static GConfEnumStringPair panel_anchor_type_enum_map [] = {
	{ SLIDING_ANCHOR_LEFT,  "panel-anchor-left" },
	{ SLIDING_ANCHOR_RIGHT, "panel-anchor-right" },
};

static GConfEnumStringPair panel_orient_type_enum_map [] = {
	{ PANEL_ORIENT_UP, "panel-orient-up" },
	{ PANEL_ORIENT_DOWN, "panel-orient-down" },
	{ PANEL_ORIENT_LEFT, "panel-orient-left" },
	{ PANEL_ORIENT_RIGHT, "panel-orient-right" },
};

static GConfEnumStringPair panel_orientation_type_enum_map [] = {
	{ GTK_ORIENTATION_HORIZONTAL, "panel-orientation-horizontal" },
	{ GTK_ORIENTATION_VERTICAL, "panel-orientation-vertical" },
};

static GConfEnumStringPair panel_speed_type_enum_map [] = {
	{ PANEL_SPEED_MEDIUM, "panel-speed-medium" },
	{ PANEL_SPEED_SLOW,   "panel-speed-slow" },
	{ PANEL_SPEED_FAST,   "panel-speed-fast" },
};

static GConfEnumStringPair panel_layer_type_enum_map [] = {
	{ LAYER_NORMAL, "panel-normal-layer" },
	{ LAYER_BELOW, "panel-below-layer" },
	{ LAYER_ABOVE, "panel-above-layer" },
};

static void
change_window_cursor(GdkWindow *window, GdkCursorType cursor_type)
{
	GdkCursor *cursor = gdk_cursor_new (cursor_type);
	gdk_window_set_cursor (window, cursor);
	gdk_cursor_unref (cursor);
}

static void
panel_realize (GtkWidget *widget, gpointer data)
{
	change_window_cursor (widget->window, GDK_LEFT_PTR);
	
	if (BASEP_IS_WIDGET (widget))
		basep_widget_enable_buttons(BASEP_WIDGET(widget), TRUE);
	else if (FOOBAR_IS_WIDGET (widget))
		foobar_widget_update_winhints (FOOBAR_WIDGET(widget));

	/*FIXME: this seems to fix the panel size problems on startup
	  (from a report) but I don't think it's right*/
	gtk_widget_queue_resize (GTK_WIDGET (widget));
}

PanelOrient
get_applet_orient (PanelWidget *panel)
{
	GtkWidget *panelw;
	g_return_val_if_fail(panel,PANEL_ORIENT_UP);
	g_return_val_if_fail(PANEL_IS_WIDGET(panel),PANEL_ORIENT_UP);
	g_return_val_if_fail(panel->panel_parent,PANEL_ORIENT_UP);
	panelw = panel->panel_parent;

	if (BASEP_IS_WIDGET(panelw))
		return basep_widget_get_applet_orient (BASEP_WIDGET(panelw));
	else
		return PANEL_ORIENT_DOWN;
}

/*we call this recursively*/
static void orient_change_foreach(GtkWidget *w, gpointer data);

void
orientation_change (AppletInfo  *info,
		    PanelWidget *panel)
{

	switch (info->type) {
	case APPLET_BONOBO:
		panel_applet_frame_change_orient (PANEL_APPLET_FRAME (info->widget),
						  get_applet_orient (panel));
		break;
	case APPLET_MENU:
		set_menu_applet_orient ((Menu *)info->data,
					get_applet_orient (panel));
		break;
	case APPLET_DRAWER: {
		Drawer      *drawer = info->data;
		BasePWidget *basep = BASEP_WIDGET (drawer->drawer);

		set_drawer_applet_orient (drawer,
					  get_applet_orient (panel));
		gtk_widget_queue_resize (drawer->drawer);
		gtk_container_foreach (GTK_CONTAINER (basep->panel),
				       orient_change_foreach,
				       (gpointer)basep->panel);
		}
		break;
	case APPLET_SWALLOW: {
		Swallow *swallow = info->data;

		if (panel->orient == GTK_ORIENTATION_VERTICAL)
			set_swallow_applet_orient (swallow,
						   SWALLOW_VERTICAL);
		else
			set_swallow_applet_orient (swallow,
						   SWALLOW_HORIZONTAL);
		}
		break;
	case APPLET_STATUS: {
		StatusApplet *status = info->data;

		if (status->orient != panel->orient) {
			status->orient = panel->orient;
			status_applet_update (status);
		}
		}
		break;
	default:
		break;
	}
}

static void
orient_change_foreach(GtkWidget *w, gpointer data)
{
	AppletInfo *info = g_object_get_data (G_OBJECT (w), "applet_info");
	PanelWidget *panel = data;
	
	orientation_change(info,panel);
}


static void
panel_orient_change(GtkWidget *widget,
		    GtkOrientation orient,
		    gpointer data)
{
	gtk_container_foreach(GTK_CONTAINER(widget),
			      orient_change_foreach,
			      widget);

	if (FLOATING_IS_WIDGET (PANEL_WIDGET (widget)->panel_parent))
		update_config_floating_orient (FLOATING_WIDGET (PANEL_WIDGET (widget)->panel_parent));

	panels_to_sync = TRUE;
}

static void
border_edge_change (BorderPos *border,
		    BorderEdge edge,
		    gpointer data)
{
	BasePWidget *basep = BASEP_WIDGET (data);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);
	gtk_container_foreach (GTK_CONTAINER (panel),
			       orient_change_foreach,
			       panel);
	panels_to_sync = TRUE;
	update_config_edge (basep);
}

/*we call this recursively*/
static void size_change_foreach(GtkWidget *w, gpointer data);

void
size_change (AppletInfo  *info,
	     PanelWidget *panel)
{
	PanelSize size = panel->sz;
	
	switch (info->type) {
	case APPLET_BONOBO:
		panel_applet_frame_change_size (
			PANEL_APPLET_FRAME (info->widget), size);
		break;
	case APPLET_STATUS: {
		StatusApplet *status = info->data;

		status->size = size;

		status_applet_update (status);
		}
		break;
	default:
		break;
	}
}

static void
size_change_foreach(GtkWidget *w, gpointer data)
{
	AppletInfo *info = g_object_get_data (G_OBJECT (w), "applet_info");
	PanelWidget *panel = data;
	
	size_change(info,panel);
}


static void
panel_size_change(GtkWidget *widget,
		  int sz,
		  gpointer data)
{
	gtk_container_foreach(GTK_CONTAINER(widget), size_change_foreach,
			      widget);
	panels_to_sync = TRUE;
	/*update the configuration box if it is displayed*/
	update_config_size (PANEL_WIDGET (widget)->panel_parent);
}

void
back_change (AppletInfo  *info,
	     PanelWidget *panel)
{
	if (info->type == APPLET_BONOBO) {
		PanelAppletFrame *frame = PANEL_APPLET_FRAME (info->widget);

		switch (panel->back_type) {
		case PANEL_BACK_PIXMAP:
			panel_applet_frame_change_background_pixmap (frame);
			break;
		case PANEL_BACK_COLOR:
			panel_applet_frame_change_background_color (frame,
								    panel->back_color.red,
								    panel->back_color.green,
								    panel->back_color.blue);
			break;
		case PANEL_BACK_NONE:
			panel_applet_frame_clear_background (frame);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}
}

static void
back_change_foreach (GtkWidget   *widget,
		     PanelWidget *panel)
{
	AppletInfo *info;

	info = g_object_get_data (G_OBJECT (widget), "applet_info");

	back_change (info, panel);
}

static void
panel_back_change(GtkWidget *widget,
		  PanelBackType type,
		  char *pixmap,
		  GdkColor *color)
{
	gtk_container_foreach (GTK_CONTAINER (widget),
			       (GtkCallback) back_change_foreach,
			       widget);

	panels_to_sync = TRUE;
	/*update the configuration box if it is displayed*/
	update_config_back(PANEL_WIDGET(widget));
}

static void state_hide_foreach(GtkWidget *w, gpointer data);

static void
state_restore_foreach(GtkWidget *w, gpointer data)
{
	AppletInfo *info = g_object_get_data (G_OBJECT (w), "applet_info");
	
	if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		BasePWidget *basep = BASEP_WIDGET(drawer->drawer);

		DRAWER_POS (basep->pos)->temp_hidden = FALSE;
		gtk_widget_queue_resize (GTK_WIDGET (basep));

		gtk_container_foreach (GTK_CONTAINER (basep->panel),
				       (basep->state == BASEP_SHOWN)
				       ? state_restore_foreach
				       : state_hide_foreach,
				       NULL);
	}
}

static void
state_hide_foreach(GtkWidget *w, gpointer data)
{
	AppletInfo *info = g_object_get_data (G_OBJECT(w), "applet_info");
	if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		BasePWidget *basep = BASEP_WIDGET(drawer->drawer);
		GtkWidget *widget = GTK_WIDGET(basep);

		DRAWER_POS (basep->pos)->temp_hidden = TRUE;
		gtk_container_foreach(GTK_CONTAINER(basep->panel),
				      state_hide_foreach,
				      NULL);

		gtk_widget_queue_resize (widget);

		/* quickly hide the window from sight, the allocation
		   and all that will get updated in the main loop */
		if(widget->window) {
			gdk_window_move(widget->window,
					-widget->allocation.width - 1,
					-widget->allocation.height - 1);
		}
	}
}

static void
queue_resize_foreach(GtkWidget *w, gpointer data)
{
	AppletInfo *info = g_object_get_data (G_OBJECT (w), "applet_info");

	if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		BasePWidget *basep = BASEP_WIDGET(drawer->drawer);
		
		if(basep->state == BASEP_SHOWN) {
			gtk_widget_queue_resize(w);
			gtk_container_foreach(GTK_CONTAINER(basep->panel),
					       queue_resize_foreach,
					       NULL);
		}
	}
}

static void
basep_state_change(BasePWidget *basep,
		   BasePState old_state,
		   gpointer data)
{
	gtk_container_foreach (GTK_CONTAINER (basep->panel),
			       (basep->state == BASEP_SHOWN)
			       ? state_restore_foreach
			       : state_hide_foreach,
			       (gpointer)basep);
}

#ifdef FIXME
/* Is this even needed anymore - remove?
 */
static void
basep_type_change(BasePWidget *basep,
		  PanelType type,
		  gpointer data)
{
	update_config_type(basep);
	panels_to_sync = TRUE;
}
#endif

static void
panel_applet_added(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	AppletInfo *info = g_object_get_data (G_OBJECT (applet),
					       "applet_info");
	GtkWidget *panelw = PANEL_WIDGET(widget)->panel_parent;
	
	/*
	 * on a real add the info will be NULL as the 
	 * only adding is done in panel_applet_register 
	 * and that doesn't add the info to the array until 
	 * after the add, so we can be sure this was 
	 * generated on a reparent.
	 */
	if((BASEP_IS_WIDGET(panelw) &&
	    !DRAWER_IS_WIDGET(panelw)) &&
	   info && info->type == APPLET_DRAWER) {
	        Drawer *drawer = info->data;
		BasePWidget *basep = BASEP_WIDGET(drawer->drawer);
		if(basep->state == BASEP_SHOWN ||
		   basep->state == BASEP_AUTO_HIDDEN) {
			BASEP_WIDGET(panelw)->drawers_open++;
			basep_widget_autoshow(BASEP_WIDGET(panelw));
		}
	}
	
	/*pop the panel up on addition*/
	if(BASEP_IS_WIDGET(panelw)) {
		basep_widget_autoshow(BASEP_WIDGET(panelw));
		/*try to pop down though if the mouse is out*/
		basep_widget_queue_autohide(BASEP_WIDGET(panelw));
	}

	orientation_change(info,PANEL_WIDGET(widget));
	size_change(info,PANEL_WIDGET(widget));
	back_change(info,PANEL_WIDGET(widget));

	/*we will need to save this applet's config now*/
	applets_to_sync = TRUE;
}

static void
panel_applet_removed(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	GtkWidget *parentw = PANEL_WIDGET(widget)->panel_parent;
	AppletInfo *info = g_object_get_data (G_OBJECT (applet),
					      "applet_info");

	/*we will need to save this applet's config now*/
	applets_to_sync = TRUE;
 
	if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		if((drawer->drawer) && (
			(BASEP_WIDGET(drawer->drawer)->state == BASEP_SHOWN) ||
			(BASEP_WIDGET(drawer->drawer)->state == BASEP_AUTO_HIDDEN))) {
			if(BASEP_IS_WIDGET(parentw)) {
				BASEP_WIDGET(parentw)->drawers_open--;
				basep_widget_queue_autohide(BASEP_WIDGET(parentw));
			}
		}
		/*it was a drawer so we need to save panels as well*/
		panels_to_sync = TRUE;
	}
}

static void
menu_deactivate(GtkWidget *w, PanelData *pd)
{
	pd->menu_age = 0;
	if(BASEP_IS_WIDGET(pd->panel))
		BASEP_WIDGET(pd->panel)->autohide_inhibit = FALSE;
}

static void
move_panel_to_cursor(GtkWidget *w)
{
	int x,y;
	gdk_window_get_pointer(NULL,&x,&y,NULL);
	if(BASEP_IS_WIDGET(w))
		basep_widget_set_pos(BASEP_WIDGET(w),x,y);
}

static gboolean
panel_move_timeout(gpointer data)
{
	if(panel_dragged && panel_been_moved)
		move_panel_to_cursor(data);
	
	panel_been_moved = FALSE;
	panel_dragged_timeout = -1;

	return FALSE;
}

static void
panel_remove_applets (PanelWidget *panel)
{
	GList *l;

	for (l = panel->applet_list; l; l = l->next) {
		AppletData *ad = l->data;
		AppletInfo *info;

		info = g_object_get_data (G_OBJECT (ad->applet),
					  "applet_info");

		panel_applet_save_position (info, info->gconf_key);

		if (info->type == APPLET_SWALLOW) {
			Swallow *swallow = info->data;

			swallow->clean_remove = TRUE;
		}
	}

	gnome_config_sync ();
}

static void
panel_destroy (GtkWidget *widget, gpointer data)
{
	PanelData *pd = g_object_get_data (G_OBJECT (widget), "PanelData");
	PanelWidget *panel = NULL;

	if (BASEP_IS_WIDGET (widget))
		panel = PANEL_WIDGET(BASEP_WIDGET(widget)->panel);
	else if (FOOBAR_IS_WIDGET (widget))
		panel = PANEL_WIDGET (FOOBAR_WIDGET (widget)->panel);

	panel_remove_applets (panel);
		
	kill_config_dialog(widget);

	if (DRAWER_IS_WIDGET(widget)) {
		GtkWidget *master_widget = panel->master_widget;

		if (master_widget != NULL) {
			AppletInfo *info =
				g_object_get_data (G_OBJECT (master_widget),
						   "applet_info");
			Drawer *drawer = info->data;
			drawer->drawer = NULL;
			panel_applet_clean (info);
			g_object_set_data (G_OBJECT (master_widget),
					   "applet_info", NULL);
		}
	} else if ((BASEP_IS_WIDGET(widget)
		    && !DRAWER_IS_WIDGET(widget))
		   || FOOBAR_IS_WIDGET (widget)) {
		/*this is a base panel and we just lost it*/
		base_panels--;
	}

	if (pd->menu != NULL)
		gtk_widget_unref (pd->menu);
	pd->menu = NULL;

	pd->panel = NULL;

	panel_list = g_slist_remove (panel_list, pd);
	g_free (pd);
}

static void
panel_applet_move(PanelWidget *panel, GtkWidget *widget, gpointer data)
{
	applets_to_sync = TRUE;
}

static void
panel_applet_about_to_die (GtkWidget *panel,
			   GtkWidget *widget,
			   gpointer   data)
{
	AppletInfo *info;

	info = g_object_get_data (G_OBJECT (widget), "applet_info");

	g_return_if_fail (info);

	/*
	 * FIXME: we need to implement an applet died dialog box
	 */
}

static GtkWidget *
panel_menu_get (PanelWidget *panel, PanelData *pd)
{
	if (pd->menu != NULL)
		return pd->menu;
	
	pd->menu = create_panel_context_menu (panel);
	g_signal_connect (G_OBJECT (pd->menu), "deactivate",
			  G_CALLBACK (menu_deactivate), pd);
	return pd->menu;
}

GtkWidget *
make_popup_panel_menu (PanelWidget *panel)
{
	GtkWidget *panelw;
	PanelData *pd;
	GtkWidget *menu;

	if (!panel) {
		panelw = ((PanelData *)panel_list->data)->panel;
		if (BASEP_IS_WIDGET (panelw))
			panel = PANEL_WIDGET (BASEP_WIDGET (panelw)->panel);
		else if (FOOBAR_IS_WIDGET (panelw))
			panel = PANEL_WIDGET (FOOBAR_WIDGET (panelw)->panel);
	} else
		panelw = panel->panel_parent;

	pd = g_object_get_data (G_OBJECT (panelw), "PanelData");
	menu = panel_menu_get (panel, pd);
	g_object_set_data (G_OBJECT (menu), "menu_panel", panel);

	pd->menu_age = 0;
	return menu;
}

static gboolean
panel_initiate_move (GtkWidget *widget, guint32 event_time)
{
	PanelWidget *panel = NULL;
	BasePWidget *basep = NULL;

	if (BASEP_IS_WIDGET (widget)) {
		basep = BASEP_WIDGET (widget);
		panel = PANEL_WIDGET (basep->panel);
	} else if (FOOBAR_IS_WIDGET (widget)) {
		panel = PANEL_WIDGET (FOOBAR_WIDGET (widget)->panel);
	}

	/*this should probably be in snapped widget*/
	if(!panel_dragged &&
	   !DRAWER_IS_WIDGET (widget) &&
	   !FOOBAR_IS_WIDGET (widget)) {
		GdkCursor *cursor = gdk_cursor_new (GDK_FLEUR);
		gtk_grab_add(widget);
		gdk_pointer_grab (widget->window,
				  FALSE,
				  PANEL_EVENT_MASK,
				  NULL,
				  cursor,
				  event_time);
		gdk_cursor_unref (cursor);

		if (basep) {
			basep->autohide_inhibit = TRUE;
			basep_widget_init_offsets (basep);
		}

		panel_dragged = TRUE;
		return TRUE;
	} if(DRAWER_IS_WIDGET(widget) &&
	     !panel_applet_in_drag) {
		panel_widget_applet_drag_start (
						PANEL_WIDGET(panel->master_widget->parent),
						panel->master_widget,
						PW_DRAG_OFF_CURSOR);
		return TRUE;
	}

	return FALSE;
}

static gboolean
panel_do_popup_menu (PanelWidget *panel, BasePWidget *basep, GtkWidget *widget, guint button, guint32 activate_time)
{
	if(!panel_applet_in_drag) {
		GtkWidget *menu;

		menu = make_popup_panel_menu (panel);
		if (basep) {
                        basep->autohide_inhibit = TRUE;
			basep_widget_autohide (basep);
		}

		gtk_menu_popup (GTK_MENU (menu),
                                NULL,
                                NULL,
                                panel_menu_position,
				widget,
				button,
                                activate_time);
		return TRUE;
	}
	return FALSE;
}

static gboolean
panel_popup_menu (GtkWidget *widget, gpointer data)
{
 	PanelWidget *panel = NULL;
	BasePWidget *basep = NULL;

	panel = PANEL_WIDGET (widget);
	if (BASEP_IS_WIDGET (data)) {
		basep = BASEP_WIDGET (data);
	}
	return panel_do_popup_menu (panel, basep, widget, 3, GDK_CURRENT_TIME);
}

static gboolean pointer_in_widget (GtkWidget *widget, GdkEventButton *event)
{
	int x, y;
	int wx, wy;
	int width, height;

	x = (int) event->x_root;
	y = (int) event->y_root;
	gdk_window_get_origin (widget->window, &wx, &wy);
	width = widget->allocation.width;
	height = widget->allocation.height;
	if ((x < wx || x >= wx + width) ||
            (y < wy || y >= wy + height))
		return FALSE;
	else
		return TRUE;
}

static gboolean
panel_event(GtkWidget *widget, GdkEvent *event)
{
	PanelWidget *panel = NULL;
	BasePWidget *basep = NULL;
	GdkEventButton *bevent;

	if (BASEP_IS_WIDGET (widget)) {
		basep = BASEP_WIDGET (widget);
		panel = PANEL_WIDGET (basep->panel);
	} else if (FOOBAR_IS_WIDGET (widget)) {
		panel = PANEL_WIDGET (FOOBAR_WIDGET (widget)->panel);
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		bevent = (GdkEventButton *) event;
		switch(bevent->button) {
		case 3:
			if (panel_do_popup_menu (panel, basep, widget, bevent->button, bevent->time))
				return TRUE;
			break;
		case 2:
			if ( ! commie_mode)
				return panel_initiate_move (widget,
							    bevent->time);
			break;
		default: break;
		}
		break;

	case GDK_BUTTON_RELEASE:
		bevent = (GdkEventButton *) event;
		if(panel_dragged) {
			if (basep) {
				basep_widget_set_pos(basep,
						     (gint16)bevent->x_root, 
						     (gint16)bevent->y_root);
				basep->autohide_inhibit = FALSE;
				basep_widget_queue_autohide(BASEP_WIDGET(widget));

				gdk_pointer_ungrab(bevent->time);
				gtk_grab_remove(widget);
				panel_dragged = FALSE;
				panel_dragged_timeout = -1;
				panel_been_moved = FALSE;
			}
			if (pointer_in_widget (basep->panel, bevent) &&
			    !gtk_widget_is_focus (basep->panel))
				panel_widget_focus (panel);
			return TRUE;
		}
		break;
	case GDK_MOTION_NOTIFY:
		if (panel_dragged) {
			if(panel_dragged_timeout==-1) {
				panel_been_moved = FALSE;
				move_panel_to_cursor(widget);
				panel_dragged_timeout = gtk_timeout_add (30,panel_move_timeout,widget);
			} else
				panel_been_moved = TRUE;
		}
		break;

	default:
		break;
	}

	return FALSE;
}

static gboolean
panel_widget_event (GtkWidget *widget, GdkEvent *event, GtkWidget *panelw)
{
	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton *bevent = (GdkEventButton *) event;

		if (bevent->button == 1 ||
		    bevent->button == 2)
			return panel_initiate_move (panelw, bevent->time);
	}

	return FALSE;
}

static gboolean
panel_sub_event_handler(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GdkEventButton *bevent;
	switch (event->type) {
		/*pass these to the parent!*/
		case GDK_BUTTON_PRESS:
	        case GDK_BUTTON_RELEASE:
	        case GDK_MOTION_NOTIFY:
			bevent = (GdkEventButton *) event;
			/*if the widget is a button we want to keep the
			  button 1 events*/
			if(!GTK_IS_BUTTON(widget) || bevent->button!=1)
				return gtk_widget_event(data, event);

			break;

		default:
			break;
	}

	return FALSE;
}

static void
drop_url(PanelWidget *panel, int pos, const char *url)
{
	char *p;

	g_return_if_fail (url != NULL);

	p = g_strdup_printf (_("Open URL: %s"), url);
	load_launcher_applet_from_info_url (url, p, url, "gnome-globe.png",
					    panel, pos, TRUE);
	g_free (p);
}

static void
drop_menu (PanelWidget *panel, int pos, const char *dir)
{
	int flags = MAIN_MENU_SYSTEM;
	DistributionType distribution = get_distribution_type ();

	/*guess distribution menus*/
	if(distribution != DISTRIBUTION_UNKNOWN)
		flags |= MAIN_MENU_DISTRIBUTION_SUB;
	/* Guess KDE menus */
	if (g_file_test (kde_menudir, G_FILE_TEST_IS_DIR))
		flags |= MAIN_MENU_KDE_SUB;
	/* FIXME: checkout gnome-vfs stuff for drop, this should be
	 * a uri */
	load_menu_applet (dir, FALSE /* main_menu */, flags, TRUE, FALSE, NULL, panel, pos, TRUE, NULL);
}

static gboolean
uri_exists (const char *uri)
{
	gboolean ret;
	GnomeVFSURI *vfs_uri = gnome_vfs_uri_new (uri);
	ret = gnome_vfs_uri_exists (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);
	return ret;
}

static void
drop_nautilus_uri (PanelWidget *panel,
		   int pos,
		   const char *uri,
		   const char *icon)
{
	char *quoted = g_shell_quote (uri);
	char *exec = g_strdup_printf ("nautilus %s",
				      quoted);
	char *base;
	g_free (quoted);

	base = g_path_get_basename (uri);

	load_launcher_applet_from_info (base,
					uri,
					exec,
					icon,
					panel,
					pos,
					TRUE);
	g_free (exec);
	g_free (base);
}

static void
drop_directory (PanelWidget *panel, int pos, const char *dir)
{
	char *tmp;

	/* not filename, but path, these are uris, not local
	 * files */
	tmp = g_build_path ("/", dir, ".directory", NULL);
	if (uri_exists (tmp)) {
		g_free (tmp);
		drop_menu (panel, pos, dir);
		return;
	}
	g_free (tmp);

	tmp = g_build_path ("/", dir, ".order", NULL);
	if (uri_exists (tmp)) {
		g_free (tmp);
		drop_menu (panel, pos, dir);
		return;
	}
	g_free (tmp);

	if (panel_is_program_in_path ("nautilus")) {
		/* nautilus */
		drop_nautilus_uri (panel, pos, dir, "gnome-folder.png");
	} else {
		if (panel_is_program_in_path  ("gmc-client")) {
			/* gmc */
			char *name;
			char *quoted = g_shell_quote (dir);
			char *exec = g_strdup_printf ("gmc-client "
						      "--create-window=%s",
						      quoted);

			g_free (quoted);

			name = g_path_get_basename (dir);
			load_launcher_applet_from_info (name,
							dir,
							exec,
							"gnome-folder.png",
							panel,
							pos,
							TRUE);
			g_free (exec);
			g_free (name);
		} else {
			drop_menu (panel, pos, dir);
		}
	}
}

static void
drop_urilist (PanelWidget *panel, int pos, char *urilist,
	      gboolean background_drops)
{
	GList *li, *files;

	files = gnome_vfs_uri_list_parse (urilist);

	for (li = files; li; li = li->next) {
		GnomeVFSURI *vfs_uri = li->data;
		gchar *uri = gnome_vfs_uri_to_string (vfs_uri, GNOME_VFS_URI_HIDE_NONE);
		const char *mimetype;
		char *basename;
		char *dirname;
		char *filename;
		GnomeVFSFileInfo *info;

		if (strncmp (uri, "http:", strlen ("http:")) == 0 ||
		    strncmp (uri, "https:", strlen ("https:")) == 0 ||
		    strncmp (uri, "ftp:", strlen ("ftp:")) == 0 ||
		    strncmp (uri, "gopher:", strlen ("gopher:")) == 0 ||
		    strncmp (uri, "ghelp:", strlen ("ghelp:")) == 0 ||
		    strncmp (uri, "man:", strlen ("man:")) == 0 ||
		    strncmp (uri, "info:", strlen ("info:")) == 0) {
			/* FIXME: probably do this only on link,
			 * in fact, on link always set up a link,
			 * on copy do all the other stuff.  Or something. */
			drop_url (panel, pos, uri);
			continue;
		}

		mimetype = gnome_vfs_mime_type_from_name (uri);
		basename = gnome_vfs_uri_extract_short_path_name (vfs_uri);
		dirname = gnome_vfs_uri_extract_dirname (vfs_uri);
		info = gnome_vfs_file_info_new ();

		if (gnome_vfs_get_file_info_uri (vfs_uri, info,
						 GNOME_VFS_FILE_INFO_DEFAULT) != GNOME_VFS_OK) {
			gnome_vfs_file_info_unref (info);
			info = NULL;
		}

		if (background_drops &&
		    mimetype != NULL &&
		    strncmp(mimetype, "image", sizeof("image")-1) == 0 &&
		    /* FIXME: We should probably use a gnome-vfs function here instead. */
		    /* FIXME: probably port the whole panel background stuff to gnome-vfs */
		    (filename = g_filename_from_uri (uri, NULL, NULL)) != NULL) {
			panel_widget_set_back_pixmap (panel, filename);
			g_free (filename);
		} else if (basename != NULL &&
			   strcmp (basename, ".directory") == 0 &&
			   dirname != NULL) {
			/* This is definately a menu */
			char *menu_uri = g_strconcat (vfs_uri->method_string, ":",
						      dirname, NULL);
			drop_menu (panel, pos, menu_uri);
			g_free (menu_uri);
		} else if (mimetype != NULL &&
			   (strcmp(mimetype, "application/x-gnome-app-info") == 0 ||
			    strcmp(mimetype, "application/x-kde-app-info") == 0)) {
			Launcher *launcher;
			
			launcher = load_launcher_applet (uri, panel, pos, TRUE, NULL);
			
			if (launcher != NULL)
				launcher_hoard (launcher);
		} else if (info != NULL &&
			   info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			drop_directory (panel, pos, uri);
		} else if (info != NULL &&
			   info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS &&
			   info->permissions &
			     (GNOME_VFS_PERM_USER_EXEC |
			      GNOME_VFS_PERM_GROUP_EXEC |
			      GNOME_VFS_PERM_OTHER_EXEC) &&
			   (filename = g_filename_from_uri (uri, NULL, NULL)) != NULL) {
			/* executable and local, so add a launcher with
			 * it */
			ask_about_launcher (filename, panel, pos, TRUE);
			g_free (filename);
		} else {
			/* FIXME: add a launcher that will launch the app
			 * associated with this file */
			/* FIXME: For now just add a launcher that launches
			 * nautilus on this uri */
			const char *icon = NULL;
			if (mimetype != NULL)
		        	icon = gnome_vfs_mime_get_icon (mimetype);
			if (icon == NULL)
				icon = "gnome-unknown.png";
			drop_nautilus_uri (panel, pos, uri, icon);
		}
		if (info != NULL)
			gnome_vfs_file_info_unref (info);
		g_free (basename);
		g_free (dirname);
		g_free (uri);
	}

	gnome_vfs_uri_list_free (files);
}

static void
drop_background_reset (PanelWidget *panel)
{
	panel_widget_change_params (panel,
				    panel->orient,
				    panel->sz,
				    PANEL_BACK_NONE,
				    panel->back_pixmap,
				    panel->fit_pixmap_bg,
				    panel->stretch_pixmap_bg,
				    panel->rotate_pixmap_bg,
				    &panel->back_color);
}

static void
drop_bgimage (PanelWidget *panel, const char *bgimage)
{
	char *filename;

	filename = g_filename_from_uri (bgimage, NULL, NULL);
	if (filename != NULL) {
		panel_widget_set_back_pixmap (panel, filename);

		g_free (filename);
	}
}

static void
drop_internal_icon (PanelWidget *panel, int pos, const char *icon_name,
		    int action)
{
	Launcher *old_launcher, *launcher;

	if (icon_name == NULL)
		return;

	if (action == GDK_ACTION_MOVE) {
		old_launcher = find_launcher (icon_name);
	} else {
		old_launcher = NULL;
	}

	launcher = load_launcher_applet (icon_name, panel, pos, TRUE, NULL);

	if (launcher != NULL) {
		launcher_hoard (launcher);

		if (old_launcher != NULL &&
		    old_launcher->button != NULL)
			gtk_widget_destroy (old_launcher->button);
	}
}

static void
move_applet (PanelWidget *panel, int pos, int applet_num)
{
	AppletInfo *info = g_slist_nth_data (applets, applet_num);

	if (pos < 0)
		pos = 0;

	if (info != NULL &&
	    info->widget != NULL &&
	    info->widget->parent != NULL &&
	    PANEL_IS_WIDGET (info->widget->parent)) {
		GSList *forb;
		forb = g_object_get_data (G_OBJECT (info->widget),
					  PANEL_APPLET_FORBIDDEN_PANELS);
		if ( ! g_slist_find (forb, panel))
			panel_widget_reparent (PANEL_WIDGET (info->widget->parent),
					       panel,
					       info->widget,
					       pos);
	}
}

static void
drop_internal_applet (PanelWidget *panel, int pos, const char *applet_type,
		      int action)
{
	int applet_num = -1;
	gboolean remove_applet = FALSE;

	if (applet_type == NULL)
		return;

	if (sscanf (applet_type, "MENU:%d", &applet_num) == 1 ||
	    sscanf (applet_type, "DRAWER:%d", &applet_num) == 1 ||
	    sscanf (applet_type, "SWALLOW:%d", &applet_num) == 1) {
		if (action != GDK_ACTION_MOVE)
			g_warning ("Only MOVE supported for menus/drawers/swallows");
		move_applet (panel, pos, applet_num);

	} else if (strncmp (applet_type, "MENU:", strlen("MENU:")) == 0) {
		const char *menu = &applet_type[strlen ("MENU:")];
		if (strcmp (menu, "MAIN") == 0)
			drop_menu (panel, pos, NULL);
		else
			drop_menu (panel, pos, menu);

	} else if (strcmp(applet_type,"DRAWER:NEW")==0) {
		load_drawer_applet(NULL, NULL, NULL, panel, pos, TRUE, NULL);

	} else if (strcmp (applet_type, "LOGOUT:NEW") == 0) {
		load_logout_applet (panel, pos, TRUE, FALSE);

	} else if (sscanf (applet_type, "LOGOUT:%d", &applet_num) == 1) {
		load_logout_applet (panel, pos, TRUE, FALSE);
		remove_applet = TRUE;

	} else if (strcmp (applet_type, "LOCK:NEW") == 0) {
		load_lock_applet (panel, pos, TRUE, FALSE);

	} else if (sscanf (applet_type, "LOCK:%d", &applet_num) == 1) {
		load_lock_applet (panel, pos, TRUE, FALSE);
		remove_applet = TRUE;

	} else if (strcmp (applet_type, "SWALLOW:ASK") == 0) {
		ask_about_swallowing(panel, pos, TRUE);

	} else if(strcmp(applet_type,"LAUNCHER:ASK")==0) {
		ask_about_launcher(NULL, panel, pos, TRUE);

	} else if(strcmp(applet_type,"STATUS:TRY")==0) {
		load_status_applet(panel, pos, TRUE, FALSE);

	} 

	if (remove_applet &&
	    action == GDK_ACTION_MOVE) {
		AppletInfo *info = g_slist_nth_data (applets, applet_num);

		if (info != NULL)
			panel_applet_clean (info);
	}
}

static void
drop_color(PanelWidget *panel, int pos, guint16 *dropped)
{
	GdkColor c;

	if(!dropped) return;

	c.red = dropped[0];
	c.green = dropped[1];
	c.blue = dropped[2];
	c.pixel = 0;

	panel_widget_set_back_color(panel, &c);
}

static GtkTargetList *
get_target_list (void)
{
	static GtkTargetEntry drop_types [] = {
		{ "text/uri-list",                       0, TARGET_URL },
		{ "x-url/http",                          0, TARGET_NETSCAPE_URL },
		{ "x-url/ftp",                           0, TARGET_NETSCAPE_URL },
		{ "_NETSCAPE_URL",                       0, TARGET_NETSCAPE_URL },
		{ "application/x-panel-directory",       0, TARGET_DIRECTORY },
		{ "application/x-panel-applet-iid",      0, TARGET_APPLET },
		{ "application/x-panel-applet-internal", 0, TARGET_APPLET_INTERNAL },
		{ "application/x-panel-icon-internal",   0, TARGET_ICON_INTERNAL },
		{ "application/x-color",                 0, TARGET_COLOR },
		{ "property/bgimage",                    0, TARGET_BGIMAGE },
		{ "x-special/gnome-reset-background",    0, TARGET_BACKGROUND_RESET },
	};
	static GtkTargetList *target_list = NULL;

	if (!target_list) {
		gint length = sizeof (drop_types) / sizeof (drop_types [0]);

		target_list = gtk_target_list_new (drop_types, length);
	}

	return target_list;
}

static gboolean
is_this_drop_ok (GtkWidget      *widget,
		 GdkDragContext *context,
		 guint          *ret_info,
		 GdkAtom        *ret_atom)
{
	GtkWidget *panel;
	GList     *l;

	g_return_val_if_fail (widget, FALSE);

	if (!BASEP_IS_WIDGET (widget) && !FOOBAR_IS_WIDGET (widget))
		return FALSE;

	if (!(context->actions & (GDK_ACTION_COPY|GDK_ACTION_MOVE)))
		return FALSE;

	if (BASEP_IS_WIDGET (widget))
		panel = BASEP_WIDGET (widget)->panel;
	else
		panel = FOOBAR_WIDGET (widget)->panel;

	for (l = context->targets; l; l = l->next) {
		GdkAtom atom;
		guint   info;

		atom = GDK_POINTER_TO_ATOM (l->data);

		if (gtk_target_list_find (get_target_list (), atom, &info)) {

			if (FOOBAR_IS_WIDGET (widget) &&
			    (info == TARGET_COLOR || info == TARGET_BGIMAGE))
				return FALSE;

			if (ret_info)
				*ret_info = info;

			if (ret_atom)
				*ret_atom = atom;
			break;
		}
	}

	return l ? TRUE : FALSE;
}

static void
do_highlight (GtkWidget *widget, gboolean highlight)
{
	gboolean have_drag;
	have_drag = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
							"have-drag"));
	if(highlight) {
		if(!have_drag) {
			g_object_set_data (G_OBJECT (widget), "have-drag",
					   GINT_TO_POINTER (TRUE));
			gtk_drag_highlight (widget);
		}
	} else {
		if(have_drag) {
			g_object_set_data (G_OBJECT (widget),
					   "have-drag", NULL);
			gtk_drag_unhighlight (widget);
		}
	}
}


static gboolean
drag_motion_cb (GtkWidget	   *widget,
		GdkDragContext     *context,
		gint                x,
		gint                y,
		guint               time)
{
	guint info;

	if ( ! is_this_drop_ok (widget, context, &info, NULL))
		return FALSE;

	/* check forbiddenness */
	if (info == TARGET_APPLET_INTERNAL) {
		GtkWidget *source_widget;

		source_widget = gtk_drag_get_source_widget (context);
		if (source_widget != NULL &&
		    BUTTON_IS_WIDGET (source_widget)) {
			GSList *forb;
			PanelWidget *panel = NULL;

			if (BASEP_IS_WIDGET (widget)) {
				BasePWidget *basep =
					BASEP_WIDGET (widget);
				panel = PANEL_WIDGET (basep->panel);
			} else if (FOOBAR_IS_WIDGET (widget)) {
				panel = PANEL_WIDGET (FOOBAR_WIDGET (widget)->panel);
			}
			forb = g_object_get_data (G_OBJECT (source_widget),
						  PANEL_APPLET_FORBIDDEN_PANELS);
			if (panel != NULL &&
			    g_slist_find (forb, panel) != NULL)
				return FALSE;
		}
	}

	/* always prefer copy, except for internal icons/applets,
	 * where we prefer move */
	if (info == TARGET_ICON_INTERNAL ||
	    info == TARGET_APPLET_INTERNAL) {
		if (context->actions & GDK_ACTION_MOVE) {
			gdk_drag_status (context, GDK_ACTION_MOVE, time);
		} else {
			gdk_drag_status (context, context->suggested_action, time);
		}
	} else if (context->actions & GDK_ACTION_COPY) {
		gdk_drag_status (context, GDK_ACTION_COPY, time);
	} else {
		gdk_drag_status (context, context->suggested_action, time);
	}

	do_highlight (widget, TRUE);

	if (BASEP_IS_WIDGET (widget)) {
		basep_widget_autoshow (BASEP_WIDGET (widget));
		basep_widget_queue_autohide (BASEP_WIDGET (widget));
	}

	return TRUE;
}

static gboolean
drag_drop_cb (GtkWidget	        *widget,
	      GdkDragContext    *context,
	      gint               x,
	      gint               y,
	      guint              time,
	      Launcher          *launcher)
{
	GdkAtom ret_atom = 0;

	if ( ! is_this_drop_ok (widget, context, NULL, &ret_atom))
		return FALSE;

	gtk_drag_get_data(widget, context,
			  ret_atom, time);

	return TRUE;
}

static void  
drag_leave_cb (GtkWidget	*widget,
	       GdkDragContext   *context,
	       guint             time,
	       Launcher         *launcher)
{
	do_highlight (widget, FALSE);
}

static void
drag_data_recieved_cb (GtkWidget	*widget,
		       GdkDragContext   *context,
		       gint              x,
		       gint              y,
		       GtkSelectionData *selection_data,
		       guint             info,
		       guint             time)
{
	PanelWidget *panel;
	int pos;

	g_return_if_fail(widget!=NULL);
	g_return_if_fail (BASEP_IS_WIDGET (widget) ||
			  FOOBAR_IS_WIDGET (widget));

	/* we use this only to really find out the info, we already
	   know this is an ok drop site and the info that got passed
	   to us is bogus (it's always 0 in fact) */
	if ( ! is_this_drop_ok (widget, context, &info, NULL)) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	if (BASEP_IS_WIDGET (widget))
		panel = PANEL_WIDGET (BASEP_WIDGET (widget)->panel);
	else
		panel = PANEL_WIDGET (FOOBAR_WIDGET (widget)->panel);

	pos = panel_widget_get_cursorloc(panel);
	
	/* 
	 * -1 passed to panel_applet_register will turn on 
	 * the insert_at_pos flag for panel_widget_add_full,
	 * which will not place it after the first applet.
	 */
	if(pos < 0)
		pos = -1;
	else if(pos > panel->size)
		pos = panel->size;

	switch (info) {
	case TARGET_URL:
		drop_urilist (panel, pos, (char *)selection_data->data,
			      FOOBAR_IS_WIDGET(widget) ? FALSE : TRUE);
		break;
	case TARGET_NETSCAPE_URL:
		drop_url (panel, pos, (char *)selection_data->data);
		break;
	case TARGET_COLOR:
		drop_color (panel, pos, (guint16 *)selection_data->data);
		break;
	case TARGET_BGIMAGE:
		if ( ! FOOBAR_IS_WIDGET(widget))
			drop_bgimage (panel, (char *)selection_data->data);
		break;
	case TARGET_BACKGROUND_RESET:
		if ( ! FOOBAR_IS_WIDGET(widget))
			drop_background_reset (panel);
		break;
	case TARGET_DIRECTORY:
		drop_directory (panel, pos, (char *)selection_data->data);
		break;
	case TARGET_APPLET:
		if ( ! selection_data->data) {
			gtk_drag_finish (context, FALSE, FALSE, time);
			return;
		}
		panel_applet_frame_load ((char *)selection_data->data,
					 panel, pos, NULL);
		break;
	case TARGET_APPLET_INTERNAL:
		drop_internal_applet (panel, pos, (char *)selection_data->data,
				      context->action);
		break;
	case TARGET_ICON_INTERNAL:
		drop_internal_icon (panel, pos, (char *)selection_data->data,
				    context->action);
		break;
	default:
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
panel_widget_setup(PanelWidget *panel)
{
	g_signal_connect (G_OBJECT(panel),
			  "applet_added",
			  G_CALLBACK(panel_applet_added),
			  NULL);
	g_signal_connect (G_OBJECT(panel),
			  "applet_removed",
			  G_CALLBACK(panel_applet_removed),
			  NULL);
	g_signal_connect (G_OBJECT(panel),
			  "applet_move",
			  G_CALLBACK(panel_applet_move),
			  NULL);
	g_signal_connect (G_OBJECT(panel),
			  "applet_about_to_die",
			  G_CALLBACK(panel_applet_about_to_die),
			  NULL);
	g_signal_connect (G_OBJECT(panel),
			  "back_change",
			  G_CALLBACK(panel_back_change),
			  NULL);
	g_signal_connect (G_OBJECT(panel),
			  "size_change",
			  G_CALLBACK(panel_size_change),
			  NULL);
	g_signal_connect (G_OBJECT (panel),
			  "orient_change",
			  G_CALLBACK (panel_orient_change),
			  NULL);
}

void
basep_pos_connect_signals (BasePWidget *basep)
{
	if (BORDER_IS_WIDGET (basep)) {
		g_signal_connect (G_OBJECT (basep->pos),
				  "edge_change",
				  G_CALLBACK (border_edge_change),
				  basep);
	}

	if (ALIGNED_IS_WIDGET (basep))
		g_signal_connect_swapped (G_OBJECT (basep->pos),
					  "align_change",
					  G_CALLBACK (update_config_align),
					  G_OBJECT (basep));
	else if (FLOATING_IS_WIDGET (basep))
		g_signal_connect_swapped (G_OBJECT (basep->pos),
					  "floating_coords_change",
					  G_CALLBACK (update_config_floating_pos),
					  G_OBJECT(basep));
	else if (SLIDING_IS_WIDGET (basep)) {
		g_signal_connect_swapped (G_OBJECT (basep->pos),
					  "anchor_change",
					  G_CALLBACK (update_config_anchor),
					  G_OBJECT(basep));
		g_signal_connect_swapped (G_OBJECT (basep->pos),
					  "offset_change",
					  G_CALLBACK (update_config_offset),
					  G_OBJECT (basep));
	}
}

static void
drawer_orient_change_foreach(GtkWidget *w, gpointer data)
{
	AppletInfo *info = g_object_get_data (G_OBJECT (w), "applet_info");
	PanelWidget *panel = data;
	
	if(info->type == APPLET_DRAWER)
		orientation_change(info, panel);
}

static void
panelw_size_alloc(BasePWidget *basep, GtkAllocation *alloc, gpointer data)
{
	if(!GTK_WIDGET_REALIZED(basep))
		return;

	if(DRAWER_IS_WIDGET(basep)) {
		gtk_container_foreach(GTK_CONTAINER(basep->panel),
				      orient_change_foreach,
				      basep->panel);
	} else if(FLOATING_IS_WIDGET(basep)) {
		gtk_container_foreach(GTK_CONTAINER(basep->panel),
				      orient_change_foreach,
				      basep->panel);
		update_config_floating_pos_limits(basep);
	} else if(ALIGNED_IS_WIDGET(basep)) {
		gtk_container_foreach(GTK_CONTAINER(basep->panel),
				      drawer_orient_change_foreach,
				      basep->panel);
	} else if(SLIDING_IS_WIDGET(basep)) {
		gtk_container_foreach(GTK_CONTAINER(basep->panel),
				      drawer_orient_change_foreach,
				      basep->panel);
		update_config_offset_limit(basep);
	}
}

void
panel_setup(GtkWidget *panelw)
{
	PanelData *pd;
	BasePWidget *basep = NULL; 
	PanelWidget *panel = NULL;

	g_return_if_fail(panelw);

	if (BASEP_IS_WIDGET (panelw)) {
		basep = BASEP_WIDGET(panelw);
		panel = PANEL_WIDGET(basep->panel);
	} else if (FOOBAR_IS_WIDGET (panelw)) {
		panel = PANEL_WIDGET (FOOBAR_WIDGET (panelw)->panel);
	}

	pd = g_new(PanelData,1);
	pd->menu = NULL;
	pd->menu_age = 0;
	pd->panel = panelw;

	if (FOOBAR_IS_WIDGET (panelw) || 
	    (BASEP_IS_WIDGET (panelw) &&
	     !DRAWER_IS_WIDGET (panelw)))
		base_panels++;
	
	if(EDGE_IS_WIDGET(panelw))
		pd->type = EDGE_PANEL;
	else if(DRAWER_IS_WIDGET(panelw))
		pd->type = DRAWER_PANEL;
	else if(ALIGNED_IS_WIDGET(panelw))
		pd->type = ALIGNED_PANEL;
	else if(SLIDING_IS_WIDGET(panelw))
		pd->type = SLIDING_PANEL;
	else if(FLOATING_IS_WIDGET(panelw))
		pd->type = FLOATING_PANEL;
	else if(FOOBAR_IS_WIDGET(panelw))
		pd->type = FOOBAR_PANEL;
	else
		g_warning("unknown panel type");
	
	panel_list = g_slist_append(panel_list,pd);
	
	g_object_set_data (G_OBJECT (panelw), "PanelData", pd);

	panel_widget_setup(panel);

	if (basep != NULL) {
		g_signal_connect (G_OBJECT(basep->hidebutton_e), "event",
				  G_CALLBACK (panel_sub_event_handler),
				  panelw);
		g_signal_connect (G_OBJECT(basep->hidebutton_w), "event",
				  G_CALLBACK (panel_sub_event_handler),
				  panelw);
		g_signal_connect (G_OBJECT(basep->hidebutton_n), "event",
				  G_CALLBACK (panel_sub_event_handler),
				  panelw);
		g_signal_connect (G_OBJECT(basep->hidebutton_s), "event",
				  G_CALLBACK (panel_sub_event_handler),
				  panelw);
		g_signal_connect (G_OBJECT (basep), "state_change",
				  G_CALLBACK (basep_state_change),
				  NULL);
		basep_pos_connect_signals (basep);
		basep_widget_enable_buttons(basep, FALSE);

		g_signal_connect_after (G_OBJECT(panelw), "size_allocate",
					G_CALLBACK(panelw_size_alloc),
					NULL);
	}

	g_signal_connect (G_OBJECT(panelw), "drag_data_received",
			  G_CALLBACK(drag_data_recieved_cb),
			  NULL);
	g_signal_connect (G_OBJECT(panelw), "drag_motion",
			  G_CALLBACK(drag_motion_cb),
			  NULL);
	g_signal_connect (G_OBJECT(panelw), "drag_leave",
			  G_CALLBACK(drag_leave_cb),
			  NULL);
	g_signal_connect (G_OBJECT(panelw), "drag_drop",
			  G_CALLBACK(drag_drop_cb),
			  NULL);

	gtk_drag_dest_set (GTK_WIDGET (panelw),
			   0, NULL, 0, 0);

	g_signal_connect (G_OBJECT (panelw), "event",
			  G_CALLBACK (panel_event), NULL);
	g_signal_connect (G_OBJECT (panel), "popup_menu",
                          G_CALLBACK (panel_popup_menu), panelw);
	g_signal_connect (G_OBJECT (panel), "event",
			  G_CALLBACK (panel_widget_event), panelw);
	
	gtk_widget_set_events(panelw,
			      gtk_widget_get_events(panelw) |
			      PANEL_EVENT_MASK);
 
	g_signal_connect (G_OBJECT (panelw), "destroy",
			  G_CALLBACK (panel_destroy), NULL);


	if(GTK_WIDGET_REALIZED(GTK_WIDGET(panelw)))
		panel_realize(GTK_WIDGET(panelw),NULL);
	else
		g_signal_connect_after(G_OBJECT(panelw), "realize",
				       G_CALLBACK(panel_realize),
				       NULL);
}

/*send state change to all the panels*/
void
send_state_change(void)
{
	GSList *list;
	for(list = panel_list; list != NULL; list = g_slist_next(list)) {
		PanelData *pd = list->data;
		if(BASEP_IS_WIDGET (pd->panel) && !DRAWER_IS_WIDGET(pd->panel))
			basep_state_change(BASEP_WIDGET(pd->panel),
					   BASEP_WIDGET(pd->panel)->state,
					   NULL);
	}
}

PanelData *
panel_data_by_id (const char *id)
{
	GSList *list;
	if (id == NULL)
		return NULL;
	for (list = panel_list; list != NULL; list = list->next) {
		PanelData *pd = list->data;
		const char *pd_id = NULL;

		if (BASEP_IS_WIDGET (pd->panel))
		       pd_id = PANEL_WIDGET (BASEP_WIDGET (pd->panel)->panel)->unique_id;
		else if (FOOBAR_IS_WIDGET (pd->panel))
		       pd_id = PANEL_WIDGET (FOOBAR_WIDGET (pd->panel)->panel)->unique_id;

		if (pd_id != NULL && strcmp (id, pd_id) == 0) {
			return pd;
		}
	}
	return NULL;
}

void
status_unparent (GtkWidget *widget)
{
	GList *li;
	PanelWidget *panel = NULL;
	if (BASEP_IS_WIDGET (widget))
		panel = PANEL_WIDGET(BASEP_WIDGET(widget)->panel);
	else if (FOOBAR_IS_WIDGET (widget))
		panel = PANEL_WIDGET (FOOBAR_WIDGET (widget)->panel);
	for(li=panel->applet_list;li;li=li->next) {
		AppletData *ad = li->data;
		AppletInfo *info = g_object_get_data (G_OBJECT (ad->applet),
						      "applet_info");
		if(info->type == APPLET_STATUS) {
			status_applet_put_offscreen(info->data);
		} else if(info->type == APPLET_DRAWER) {
			Drawer *dr = info->data;
			status_unparent(dr->drawer);
		}
	}
}


void
panel_session_init_global_config (void)
{
	GSList *li, *list;

	list = panel_gconf_all_global_entries ();

	for (li = list; li != NULL ; li = li->next) {
		GConfEntry *entry;
		GConfValue *value;
		gchar      *key;

		entry = (GConfEntry *)li->data;

		value = gconf_entry_get_value (entry);

		key = g_path_get_basename (gconf_entry_get_key (entry));

#ifdef PANEL_SESSION_DEBUG
		printf ("Checking global config for %s\n", key);
#endif
		/* FIXME: We need to do something more user friendly here */
		if (!strcmp (key, "panel-animation-speed")) {
			gconf_string_to_enum (panel_speed_type_enum_map,
					      gconf_value_get_string (value),
					      (gint *) &global_config.animation_speed);
		}

		else if (!strcmp (key, "panel-minimized-speed"))
			global_config.minimized_size =
				gconf_value_get_int (value);

		else if (!strcmp (key, "panel-minimized-size"))
			global_config.minimized_size =
				gconf_value_get_int (value);

		else if (!strcmp (key, "panel-hide-delay"))
			global_config.hide_delay =
				gconf_value_get_int (value);

		else if (!strcmp (key, "panel-show-delay"))
			global_config.show_delay =
				gconf_value_get_int (value);

		else if (!strcmp (key, "tooltips-enabled"))
			global_config.tooltips_enabled =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "keep-menus-in-memory"))
			global_config.keep_menus_in_memory =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "enable-animations"))
			global_config.enable_animations =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "autoraise-panel"))
			global_config.autoraise =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "panel-window-layer"))
			gconf_string_to_enum (panel_layer_type_enum_map,
					      gconf_value_get_string (value),
					      (gint *) &global_config.layer);

		else if (!strcmp (key, "drawer-autoclose"))
			global_config.drawer_auto_close =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "auto-update-menus"))
			;

		else if (!strcmp (key, "highlight-launchers-on-mouseover"))
			global_config.highlight_when_over =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "confirm-panel-remove"))
			global_config.confirm_panel_remove =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "enable-key-bindings"))
			global_config.keys_enabled =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "menu-key")) { 
			if (global_config.menu_key )
				g_free (global_config.menu_key );

			global_config.menu_key =
				g_strdup (gconf_value_get_string (value));

			convert_string_to_keysym_state (global_config.menu_key,
							&global_config.menu_keysym,
							&global_config.menu_state);
		} else if (!strcmp (key, "run-key")) {
			if (global_config.run_key )
				g_free (global_config.run_key );

			global_config.run_key =
				 g_strdup (gconf_value_get_string (value));

			convert_string_to_keysym_state (global_config.run_key,
							&global_config.run_keysym,
							&global_config.run_state);
		} else if (!strcmp (key, "screenshot-key")) {
			if (global_config.screenshot_key )
				g_free (global_config.screenshot_key );

			global_config.screenshot_key =
				g_strdup (gconf_value_get_string (value));

			convert_string_to_keysym_state (global_config.screenshot_key,
							&global_config.screenshot_keysym,
							&global_config.screenshot_state);

		} else if (!strcmp (key, "window-screenshot-key")) {
			if (global_config.window_screenshot_key )
				g_free (global_config.window_screenshot_key );

			global_config.window_screenshot_key =
				g_strdup (gconf_value_get_string (value));

			convert_string_to_keysym_state (global_config.window_screenshot_key,
							&global_config.window_screenshot_keysym,
							&global_config.window_screenshot_state);

		} else  {
			g_warning ("%s not handled", key);
		}

		g_free (key);
		gconf_entry_free (entry);
	}

	g_slist_free (list);

	/* FIXME STUFF THAT IS BORKED */
	global_config.menu_check = TRUE;
	global_config.menu_flags = get_default_menu_flags();

	panel_session_apply_global_config ();
}

void
panel_session_save_global_config (void)
{
	GConfChangeSet *global_config_cs;
	gchar *full_key;

	global_config_cs = gconf_change_set_new ();

	/* FIXME STUFF THAT IS BORKED 
	panel_gconf_global_config_set_int ("menu-flags", global_config.menu_flags);
	panel_gconf_global_config_set_bool ("menu-check", global_config.menu_check);
	*/

	/* FIXME: Make this more generic - this currently sucks */

	full_key = panel_gconf_global_config_get_full_key ("panel-minimized-speed");
	gconf_change_set_set_int (global_config_cs, full_key, global_config.minimized_size);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("panel-hide-delay");
	gconf_change_set_set_int (global_config_cs, full_key, global_config.hide_delay);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("panel-show-delay");
	gconf_change_set_set_int (global_config_cs, full_key, global_config.show_delay);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("tooltips-enabled");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.tooltips_enabled);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("enable-animations");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.enable_animations);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("autoraise-panel");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.autoraise);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("drawer-autoclose");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.drawer_auto_close);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("highlight-launchers-on-mouseover");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.highlight_when_over);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("confirm-panel-remove");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.confirm_panel_remove);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("keep-menus-in-memory");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.keep_menus_in_memory);
	g_free (full_key);
	
	full_key = panel_gconf_global_config_get_full_key ("panel-animation-speed");
	gconf_change_set_set_string (global_config_cs, full_key , gconf_enum_to_string (panel_speed_type_enum_map, global_config.animation_speed));
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("menu-key");
	gconf_change_set_set_string (global_config_cs, full_key, global_config.menu_key);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("run-key");
	gconf_change_set_set_string (global_config_cs, full_key, global_config.run_key);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("screenshot-key");
	gconf_change_set_set_string (global_config_cs, full_key, global_config.screenshot_key);
	g_free (full_key);
	
	full_key = panel_gconf_global_config_get_full_key ("window-screenshot-key");
	gconf_change_set_set_string (global_config_cs, full_key, global_config.window_screenshot_key);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("panel-window-layer");
	gconf_change_set_set_string (global_config_cs, full_key, gconf_enum_to_string (panel_layer_type_enum_map, global_config.layer));
	g_free (full_key);

	gconf_client_commit_change_set (panel_gconf_get_client (), global_config_cs, FALSE, NULL);

	gconf_change_set_unref (global_config_cs);
			     
}

void
panel_session_apply_global_config (void)
{
	static int layer_old = -1;
	static int menu_flags_old = -1;
	static int old_menu_check = -1;
	GSList *li;

	if (global_config.tooltips_enabled)
		gtk_tooltips_enable (panel_tooltips);
	else
		gtk_tooltips_disable (panel_tooltips);
	/* not incredibly efficent way to do this, we just make
	 * sure that all directories are reread */
	if (old_menu_check != global_config.menu_check) {
		fr_force_reread ();
	}
	if(old_menu_check != global_config.menu_check) {
		GSList *li;
		for(li = applets; li != NULL; li = g_slist_next(li)) {
			AppletInfo *info = li->data;
			if(info->menu != NULL) {
				gtk_widget_unref(info->menu);
				info->menu = NULL;
				info->menu_age = 0;
			}
			if(info->type == APPLET_MENU) {
				Menu *menu = info->data;
				if(menu->menu != NULL) {
					gtk_widget_unref(menu->menu);
					menu->menu = NULL;
					menu->age = 0;
				}
			}
		}
		for(li = panel_list; li != NULL; li = g_slist_next(li)) {
			PanelData *pd = li->data;
			if(pd->menu != NULL) {
				gtk_widget_unref(pd->menu);
				pd->menu = NULL;
				pd->menu_age = 0;
			}
		}
		foobar_widget_force_menu_remake();
	}
	old_menu_check = global_config.menu_check;

	/* if we changed global menu flags, cmark all main menus that use
	 * the global setting as dirty */
	if(menu_flags_old != global_config.menu_flags) {
		GSList *li;
		for(li = applets; li != NULL; li = g_slist_next(li)) {
			AppletInfo *info = li->data;
			if(info->type == APPLET_MENU) {
				Menu *menu = info->data;
				if(menu->menu != NULL &&
				   menu->global_main) {
					gtk_widget_unref(menu->menu);
					menu->menu = NULL;
					menu->age = 0;
				}
			}
		}
	}

	if (layer_old != global_config.layer) {
		 for (li = panel_list; li != NULL; li = li->next) {
			PanelData *pd = li->data;
			if ( ! GTK_WIDGET_REALIZED (pd->panel))
				continue;
			if (BASEP_IS_WIDGET (pd->panel))
				basep_widget_update_winhints (BASEP_WIDGET (pd->panel));
			else if (FOOBAR_IS_WIDGET (pd->panel))
				foobar_widget_update_winhints (FOOBAR_WIDGET (pd->panel));
		}
	}
	layer_old = global_config.layer;
	
	for (li = panel_list; li != NULL; li = li->next) {
		PanelData *pd = li->data;
		if (BASEP_IS_WIDGET (pd->panel)) {
			basep_update_frame (BASEP_WIDGET (pd->panel));

			if ((menu_flags_old != global_config.menu_flags) &&
			    pd->menu) {
				gtk_widget_unref (pd->menu);
				pd->menu = NULL;
				pd->menu_age = 0;
			}
				
		}
	}
	menu_flags_old = global_config.menu_flags;

	panel_global_keys_setup();
}


static gchar *
panel_gconf_profile_get_string (const gchar *profile,
				const gchar *panel_id,
				const gchar *key,
				gboolean     use_default,
				const gchar *default_val)
{
	gchar *full_key;
	gchar *retval;

	/* FIXME: Make this check screen sizes and stuff */
	if (use_default)
		full_key = panel_gconf_panel_default_profile_get_full_key (
						"medium", panel_id, key);
	else
		full_key = panel_gconf_panel_profile_get_full_key (
						profile, panel_id, key);

	retval = panel_gconf_get_string (full_key, default_val);
	g_free (full_key);

	return retval;
}

static gint
panel_gconf_profile_get_int (const gchar *profile,
			     const gchar *panel_id,
			     const gchar *key,
			     gboolean     use_default,
			     gint default_val)
{
	gchar *full_key;
	gint   retval;

	/* FIXME: Make this check screen sizes and stuff */
	if (use_default)
		full_key = panel_gconf_panel_default_profile_get_full_key (
						"medium", panel_id, key);
	else
		full_key = panel_gconf_panel_profile_get_full_key (
						profile, panel_id, key);
	
	retval = panel_gconf_get_int (full_key, default_val);
	g_free (full_key);

	return retval;
}

static gboolean
panel_gconf_profile_get_bool (const gchar *profile,
			      const gchar *panel_id,
			      const gchar *key,
			      gboolean     use_default,
			      gboolean     default_val)
{
	gchar    *full_key;
	gboolean  retval;

	/* FIXME: Make this check screen sizes and stuff */
	if (use_default)
		full_key = panel_gconf_panel_default_profile_get_full_key (
						"medium", panel_id, key);
	else
		full_key = panel_gconf_panel_profile_get_full_key (
						profile, panel_id, key);

	retval = panel_gconf_get_bool (full_key, default_val);
	g_free (full_key);

	return retval;
}

static void
panel_gconf_profile_set_string (const gchar *profile,
				const gchar *panel_id,
				const gchar *key,
				const gchar *value)
{
	gchar *full_key;

	full_key = panel_gconf_panel_profile_get_full_key (profile, panel_id, key);

#ifdef PANEL_SESSION_DEBUG
	printf ("Saving to %s\n", full_key);
#endif

	panel_gconf_set_string (full_key, value);	

	g_free (full_key);
}

static void
panel_gconf_profile_set_int (const gchar *profile,
			     const gchar *panel_id,
			     const gchar *key,
			     gint         value)
{
	gchar *full_key;

	full_key = panel_gconf_panel_profile_get_full_key (profile, panel_id, key);

#ifdef PANEL_SESSION_DEBUG
	printf ("Saving to %s\n", full_key);
#endif

	panel_gconf_set_int (full_key, value);	

	g_free (full_key);
}

static void
panel_gconf_profile_set_bool (const gchar *profile,
			      const gchar *panel_id,
			      const gchar *key,
			      gboolean     value)
{
	gchar *full_key;

	full_key = panel_gconf_panel_profile_get_full_key (profile, panel_id, key);

#ifdef PANEL_SESSION_DEBUG
	printf ("Saving to %s\n", full_key);
#endif

	panel_gconf_set_bool (full_key, value);	

	g_free (full_key);
}

void
panel_session_init_panels(void)
{
	gchar *panel_default_profile;

	gchar *panel_profile_key;
	gboolean use_default = FALSE;

	GtkWidget *panel = NULL;
	GSList *panel_ids;
	GSList *temp;
	gchar *timestring;
	
	panel_profile_key = g_strdup_printf ("/apps/panel/profiles/%s/panels", session_get_current_profile ());

#ifdef PANEL_SESSION_DEBUG
	printf ("Current profile is %s\n", session_get_current_profile ());
	printf ("We are checking for dir %s\n", panel_profile_key);
#endif

	if (panel_gconf_dir_exists (panel_profile_key) == FALSE) {
		/* FIXME: We need to resort to another fallback default panel config 
		          and do some sort of check for screen sizes */
		use_default = TRUE;
		panel_default_profile = PANEL_GCONF_DEFAULT_PROFILE;
		g_free (panel_profile_key);
		panel_profile_key = panel_gconf_general_default_profile_get_full_key (panel_default_profile, "panel-id-list");
	} else {
		g_free (panel_profile_key);
		panel_profile_key = panel_gconf_general_profile_get_full_key (session_get_current_profile (), "panel-id-list");
	}

	panel_ids = gconf_client_get_list (panel_gconf_get_client (),
					   panel_profile_key,
					   GCONF_VALUE_STRING,
					   NULL);

	g_free (panel_profile_key);
					  
	for (temp = panel_ids; temp; temp = temp->next) {
		PanelType type;
		PanelBackType back_type;
		gchar *panel_id;
		int sz;
		BasePState state;
		BasePMode mode;
		BorderEdge edge;
		char *back_pixmap, *color;
		GdkColor back_color = {0,0,0,1};
		gboolean fit_pixmap_bg;
		gboolean stretch_pixmap_bg;
		gboolean rotate_pixmap_bg;
		int hidebuttons_enabled;
		int hidebutton_pixmaps_enabled;
		int screen;
		char *tmp_str;

		panel_id = temp->data;

#ifdef PANEL_SESSION_DEBUG
		printf ("Loading panel id %s\n", panel_id);
#endif
		back_pixmap = panel_gconf_profile_get_string (session_get_current_profile (), 
							      panel_id, "panel-background-pixmap",
							      use_default, NULL);
		if (string_empty (back_pixmap)) {
			g_free (back_pixmap);
			back_pixmap = NULL;
		}

		color = panel_gconf_profile_get_string (session_get_current_profile (),
							panel_id, "panel-background-color",
							use_default, NULL);
		if ( ! string_empty (color))
			gdk_color_parse (color, &back_color);

		tmp_str = panel_gconf_profile_get_string (session_get_current_profile (),
							  panel_id, "panel-background-type",
							  use_default, "no-background");
		gconf_string_to_enum (background_type_enum_map, tmp_str, (gint *) &back_type);
		g_free (tmp_str);
		
		fit_pixmap_bg = panel_gconf_profile_get_bool (session_get_current_profile (),
							      panel_id, "panel-background-pixmap-fit",
							      use_default, FALSE);

		stretch_pixmap_bg = panel_gconf_profile_get_bool (session_get_current_profile (),
								  panel_id, "panel-background-pixmap-stretch",
								  use_default, FALSE);

		rotate_pixmap_bg = panel_gconf_profile_get_bool (session_get_current_profile (),
								 panel_id, "panel-background-pixmap-rotate",
								 use_default, FALSE);
	
		tmp_str = panel_gconf_profile_get_string (session_get_current_profile (),
							  panel_id, "panel-size",
							  use_default, "panel-size-small");
		gconf_string_to_enum (panel_size_type_enum_map, tmp_str, &sz);
		g_free (tmp_str);
		
		/* Now for type specific config */


		tmp_str = panel_gconf_profile_get_string (session_get_current_profile (),
							  panel_id, "panel-type",
							  use_default, "edge-panel");
		gconf_string_to_enum (panel_type_type_enum_map, tmp_str, (gint *) &type);
		g_free (tmp_str);
		
		hidebuttons_enabled = panel_gconf_profile_get_bool (session_get_current_profile (),
								    panel_id, "hide-buttons-enabled",
								    use_default, TRUE);
		
		hidebutton_pixmaps_enabled = panel_gconf_profile_get_bool (session_get_current_profile (),
									   panel_id, "hide-button-pixmaps-enabled",
									   use_default, TRUE);

		state = panel_gconf_profile_get_int (session_get_current_profile (),
						     panel_id, "panel-hide-state",
						     use_default, 0);

		mode = panel_gconf_profile_get_int (session_get_current_profile (),
						    panel_id, "panel-hide-mode",
						    use_default, 0);

		screen = panel_gconf_profile_get_int (session_get_current_profile (),
						      panel_id, "screen-id",
						      use_default, 0);

		switch (type) {
			
		case EDGE_PANEL:
			tmp_str = panel_gconf_profile_get_string (session_get_current_profile (),
								  panel_id, "screen-edge",
								  use_default, "panel-edge-bottom");
			gconf_string_to_enum (panel_edge_type_enum_map, tmp_str, (gint *) &edge);
			g_free (tmp_str);
			
			panel = edge_widget_new (panel_id,
						 screen,
						 edge, 
						 mode, state,
						 sz,
						 hidebuttons_enabled,
						 hidebutton_pixmaps_enabled,
						 back_type, back_pixmap,
						 fit_pixmap_bg,
						 stretch_pixmap_bg,
						 rotate_pixmap_bg,
						 &back_color);
			break;
		case ALIGNED_PANEL: {
			AlignedAlignment align;

			tmp_str = panel_gconf_profile_get_string (session_get_current_profile (),
								  panel_id, "screen-edge",
								  use_default, "panel-edge-bottom");
			gconf_string_to_enum (panel_edge_type_enum_map, tmp_str, (gint *) &edge);
			g_free (tmp_str);

			tmp_str = panel_gconf_profile_get_string (session_get_current_profile (),
								  panel_id, "panel-align",
								  use_default, "panel-alignment-left");
			gconf_string_to_enum (panel_alignment_type_enum_map, tmp_str, (gint *) &align);
			g_free (tmp_str);

			panel = aligned_widget_new (panel_id,
						    screen,
						    align,
						    edge,
						    mode,
						    state,
						    sz,
						    hidebuttons_enabled,
						    hidebutton_pixmaps_enabled,
						    back_type,
						    back_pixmap,
						    fit_pixmap_bg,
						    stretch_pixmap_bg,
						    rotate_pixmap_bg,
						    &back_color);
			break;
		}
		case SLIDING_PANEL: {
			gint16 offset;
			SlidingAnchor anchor;

			tmp_str = panel_gconf_profile_get_string (session_get_current_profile (),
								  panel_id, "screen-edge",
								  use_default, "panel-edge_bottom");
			gconf_string_to_enum (panel_edge_type_enum_map, tmp_str, (gint *) &edge);
			g_free (tmp_str);

			tmp_str = panel_gconf_profile_get_string (session_get_current_profile (),
								  panel_id, "panel-anchor",
								  use_default, "panel-anchor-left");
			gconf_string_to_enum (panel_anchor_type_enum_map, tmp_str, (gint *) &anchor);
			g_free (tmp_str);
			
			offset = panel_gconf_profile_get_int (session_get_current_profile (),
							      panel_id, "panel-offset",
							      use_default, 0);
	
			panel = sliding_widget_new (panel_id,
						    screen,
						    anchor,
						    offset,
						    edge,
						    mode,
						    state,
						    sz,
						    hidebuttons_enabled,
						    hidebutton_pixmaps_enabled,
						    back_type,
						    back_pixmap,
						    fit_pixmap_bg,
						    stretch_pixmap_bg,
						    rotate_pixmap_bg,
						    &back_color);
			break;
		}
		case DRAWER_PANEL: {
			int orient;

			tmp_str = panel_gconf_profile_get_string (session_get_current_profile (),
								  panel_id, "panel-orient",
								  use_default, "panel-orient-up");
			gconf_string_to_enum (panel_orient_type_enum_map, tmp_str, (gint *) &orient);
			g_free (tmp_str);


			panel = drawer_widget_new (panel_id,
						   (PanelOrient) orient,
						   BASEP_EXPLICIT_HIDE, 
						   state,
						   sz,
						   hidebuttons_enabled,
						   hidebutton_pixmaps_enabled,
						   back_type,
						   back_pixmap,
						   fit_pixmap_bg,
						   stretch_pixmap_bg,
						   rotate_pixmap_bg,
						   &back_color);
			break;
		}
		case FLOATING_PANEL: {
			GtkOrientation orient;
			int x, y;

			tmp_str = panel_gconf_profile_get_string (session_get_current_profile (),
								  panel_id, "panel-orient ",
								  use_default, "panel-orientation-horizontal"),
			gconf_string_to_enum (panel_orientation_type_enum_map, tmp_str, (gint *) &orient);
			g_free (tmp_str);

			x = panel_gconf_profile_get_int (session_get_current_profile (),
							 panel_id, "panel-x-position",
							 use_default, 0);

			y = panel_gconf_profile_get_int  (session_get_current_profile (),
							  panel_id, "panel-y-position",
							  use_default, 0);
			
			panel = floating_widget_new (panel_id,
						     screen,
						     x,
						     y,
						     orient,
						     mode,
						     state,
						     sz,
						     hidebuttons_enabled,
						     hidebutton_pixmaps_enabled,
						     back_type,
						     back_pixmap,
						     fit_pixmap_bg,
						     stretch_pixmap_bg,
						     rotate_pixmap_bg,
						     &back_color);
			break;
		}
		case FOOBAR_PANEL:
			panel = foobar_widget_new (panel_id, screen);

			timestring = panel_gconf_profile_get_string (session_get_current_profile (),
								     panel_id, "clock-format",
								     use_default, _("%I:%M:%S %p"));
			if (timestring) {
				foobar_widget_set_clock_format (FOOBAR_WIDGET (panel), timestring);
				g_free (timestring);
			}

			break;
		default:
			panel = NULL;
			g_warning ("Unkown panel type: %d; ignoring.", type);
			break;
		}

		g_free (color);
		g_free (back_pixmap);

		if (panel != NULL) {
			panel_setup (panel);
			gtk_widget_show (panel);
		}
	}

	/* Failsafe! */
	if (panel_ids == NULL) {
		panel_error_dialog ("no_panels_found",
				    _("No panels were found in your "
				      "configuration.  I will create "
				      "a menu panel for you"));

		panel = foobar_widget_new (NULL, 0);
		panel_setup (panel);
		gtk_widget_show (panel);
	}

	panel_g_slist_deep_free (panel_ids);
}

void
panel_session_save_panel (PanelData *pd)
{
	BasePWidget *basep = NULL; 
	PanelWidget *panel = NULL;
	GSList      *panel_id_list;
	GSList      *temp;
	gboolean     exists= FALSE;
	const gchar *panel_profile;
	gchar       *panel_id_key;
	char	    *color;
	
	panel_profile = session_get_current_profile ();
#ifdef PANEL_SESSION_DEBUG
	printf ("Saving to %s profile\n", panel_profile);
#endif
	panel_id_key = panel_gconf_general_profile_get_full_key (panel_profile, "panel-id-list");

	if (BASEP_IS_WIDGET (pd->panel)) {
		basep = BASEP_WIDGET (pd->panel);
		panel = PANEL_WIDGET (basep->panel);
	} else if (FOOBAR_IS_WIDGET (pd->panel)) {
		panel = PANEL_WIDGET (FOOBAR_WIDGET(pd->panel)->panel);
	}

#ifdef PANEL_SESSION_DEBUG
	printf ("Saving unique panel id %s\n", panel->unique_id);
#endif
	/* Get the current list from gconf */	
	panel_id_list = gconf_client_get_list (panel_gconf_get_client (),
					   panel_id_key,
					   GCONF_VALUE_STRING,
					   NULL);

	
	/* We need to go through the panel-id-list and add stuff to the panel */
	
	for (temp = panel_id_list; temp; temp = temp->next) {
#ifdef PANEL_SESSION_DEBUG
	printf ("Found ID: %s\n", (gchar *)temp->data);
#endif
		if (strcmp (panel->unique_id, (gchar *)temp->data) == 0)
			exists = TRUE;
	}	
	
	/* We need to add this key */
	if (exists == FALSE) {
#ifdef PANEL_SESSION_DEBUG
	printf ("Adding a new panel to id-list: %s\n", panel->unique_id);
#endif
		panel_id_list = g_slist_append (panel_id_list, g_strdup (panel->unique_id));	

		gconf_client_set_list (panel_gconf_get_client (),
				       panel_id_key,
				       GCONF_VALUE_STRING,
				       panel_id_list,
				       NULL);
	}

	g_free (panel_id_key);
	panel_g_slist_deep_free (panel_id_list);

	panel_gconf_profile_set_string (panel_profile,
					      panel->unique_id,
					      "panel-type", 
					      gconf_enum_to_string (panel_type_type_enum_map, pd->type));

	if (basep != NULL) {
		panel_gconf_profile_set_bool (panel_profile, panel->unique_id,
					      "hide-buttons-enabled", basep->hidebuttons_enabled);
		panel_gconf_profile_set_bool (panel_profile, panel->unique_id,
					      "hide-button-pixmaps-enabled", basep->hidebutton_pixmaps_enabled);
		panel_gconf_profile_set_int (panel_profile, panel->unique_id,
					     "panel-hide-mode", basep->mode);
		panel_gconf_profile_set_int (panel_profile, panel->unique_id,
					     "panel-hide-state", basep->state);
		panel_gconf_profile_set_int (panel_profile, panel->unique_id,
					     "screen-id", basep->screen);
	}

	panel_gconf_profile_set_string (panel_profile, panel->unique_id,
					"panel-size", 
					gconf_enum_to_string (panel_size_type_enum_map, panel->sz));
	panel_gconf_profile_set_bool (panel_profile, panel->unique_id,
				      "panel-backgroun-pixmap-fit", panel->fit_pixmap_bg);
	panel_gconf_profile_set_bool (panel_profile, panel->unique_id,
				      "panel-background-pixmap-stretch", panel->stretch_pixmap_bg);
	panel_gconf_profile_set_bool (panel_profile, panel->unique_id,
				      "panel-background-pixmap-rotate", panel->rotate_pixmap_bg);
	panel_gconf_profile_set_string (panel_profile, panel->unique_id,
					"panel-background-pixmap", sure_string (panel->back_pixmap));


	color = g_strdup_printf ("#%02x%02x%02x",
			 (guint)panel->back_color.red/256,
			 (guint)panel->back_color.green/256,
			 (guint)panel->back_color.blue/256);
	
	panel_gconf_profile_set_string (panel_profile, panel->unique_id,
					"panel-background-color", color);
	g_free (color);

	panel_gconf_profile_set_string (panel_profile, panel->unique_id,
					"panel-background-type", 
					gconf_enum_to_string (background_type_enum_map, panel->back_type));
	
	/* now do different types */
	if (BORDER_IS_WIDGET(pd->panel))
		panel_gconf_profile_set_string (panel_profile, panel->unique_id,
						"screen-edge", 
						gconf_enum_to_string (panel_edge_type_enum_map, BORDER_POS (basep->pos)->edge));
	switch (pd->type) {
	case ALIGNED_PANEL:
		panel_gconf_profile_set_string (panel_profile, panel->unique_id,
						"panel-align", 
						 gconf_enum_to_string (panel_alignment_type_enum_map, ALIGNED_POS (basep->pos)->align));
		break;
	case SLIDING_PANEL:
		panel_gconf_profile_set_int (panel_profile, panel->unique_id,
					     "panel-offset", SLIDING_POS (basep->pos)->offset);
		panel_gconf_profile_set_string (panel_profile, panel->unique_id,
	      					"panel-anchor", 
						 gconf_enum_to_string (panel_anchor_type_enum_map, SLIDING_POS (basep->pos)->anchor));
		break;
	case FLOATING_PANEL:
		panel_gconf_profile_set_string (panel_profile, panel->unique_id,
						"panel-orient", 
						 gconf_enum_to_string (panel_orientation_type_enum_map, PANEL_WIDGET (basep->pos)->orient));

		panel_gconf_profile_set_int (panel_profile, panel->unique_id,
					     "panel-x-position", FLOATING_POS (basep->pos)->x);
		panel_gconf_profile_set_int (panel_profile, panel->unique_id,
					     "panel-y-position", FLOATING_POS (basep->pos)->y);
		break;
	case DRAWER_PANEL:
		panel_gconf_profile_set_string (panel_profile, panel->unique_id,
						"panel-orient", 
						gconf_enum_to_string (panel_orient_type_enum_map, DRAWER_POS (basep->pos)->orient));
		break;
	case FOOBAR_PANEL:
		panel_gconf_profile_set_string (panel_profile, panel->unique_id,
						"clock-format", FOOBAR_WIDGET (pd->panel)->clock_format);
		panel_gconf_profile_set_int (panel_profile, panel->unique_id, 
					     "screen_id", FOOBAR_WIDGET (pd->panel)->screen);
		break;
	default:
		break;
	}

#ifdef PANEL_SESSION_DEBUG
	printf ("Done saving\n");
#endif
}

void
panel_session_remove_panel_from_config (PanelWidget *panel) {
/* We need to take in a panel widget, get it's ID, then remove it from the current profile */
        gchar *panel_profile_key, *panel_profile_directory;
	GSList *panel_ids, *temp, *new_panel_ids = NULL;

#ifdef PANEL_SESSION_DEBUG
	printf ("We are removing the configuration for panel id : %s\n", panel->unique_id);
#endif
        panel_profile_key = g_strdup_printf ("/apps/panel/profiles/%s/panels", session_get_current_profile ());

        if (panel_gconf_dir_exists (panel_profile_key) == FALSE) {
#ifdef PANEL_SESSION_DEBUG
	  printf ("We have no configuration to remove!\n");
#endif
                /* We haven't saved any profile, so don't remove anything */
                g_free (panel_profile_key);
                return;
        } else {
                g_free (panel_profile_key);
                panel_profile_key = panel_gconf_general_profile_get_full_key (session_get_current_profile (), "panel-id-list");
        }

        panel_ids = gconf_client_get_list (panel_gconf_get_client (),
                                           panel_profile_key,
                                           GCONF_VALUE_STRING,
                                           NULL);

        /* We now have to go through the GSList and remove panel->unique_id */

        for (temp = panel_ids; temp; temp = temp->next) {
                if (strcmp (panel->unique_id, (gchar *)temp->data) == 0) {
                        /* We now start removing configuration */
                        panel_profile_directory = g_strdup_printf ("/apps/panel/profiles/%s/panels/%s",
                                                                   session_get_current_profile (),
                                                                   PANEL_WIDGET (panel)->unique_id);
#ifdef PANEL_SESSION_DEBUG
	printf ("We are removing the configuration for gconf key : %s\n", panel_profile_directory);
#endif
                        panel_gconf_directory_recursive_clean (panel_gconf_get_client (),
                                                               (const gchar *) panel_profile_directory);
                        g_free (panel_profile_directory);
                } else {
                        new_panel_ids = g_slist_prepend (new_panel_ids, (gchar *)temp->data);
                }
        }

        gconf_client_set_list (panel_gconf_get_client (),
                               panel_profile_key,
                               GCONF_VALUE_STRING,
                               new_panel_ids,
                               NULL);
	if (new_panel_ids != NULL) {	
		panel_g_slist_deep_free (new_panel_ids);
	}
	gconf_client_suggest_sync (panel_gconf_get_client (), NULL);
}
