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

#include <gdk/gdkkeysyms.h>

#include <libgnome/libgnome.h>
#include <libbonobo.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomeui/gnome-window-icon.h>

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
#include "menu-fentry.h"
#include "menu-util.h"
#include "menu.h"
#include "multiscreen-stuff.h"
#include "panel-util.h"
#include "panel-config.h"
#include "panel-config-global.h"
#include "panel-gconf.h"
#include "session.h"
#include "panel-applet-frame.h"
#include "global-keys.h"
#include "panel-action-button.h"

#define PANEL_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

/*list of all panel widgets created*/
GSList *panel_list = NULL;

static gboolean panel_dragged = FALSE;
static guint panel_dragged_timeout = 0;
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
	{ PANEL_BACK_IMAGE,  "pixmap-background" },
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

static void
panel_realize (GtkWidget *widget, gpointer data)
{
	if (BASEP_IS_WIDGET (widget))
		basep_widget_enable_buttons(BASEP_WIDGET(widget), TRUE);
	else if (FOOBAR_IS_WIDGET (widget))
		foobar_widget_update_winhints (FOOBAR_WIDGET(widget));

	/*FIXME: this seems to fix the panel size problems on startup
	  (from a report) but I don't think it's right*/
	gtk_widget_queue_resize (GTK_WIDGET (widget));
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
						  panel_widget_get_applet_orient (panel));
		break;
	case APPLET_MENU:
		set_menu_applet_orient ((Menu *)info->data,
					panel_widget_get_applet_orient (panel));
		break;
	case APPLET_DRAWER: {
		Drawer      *drawer = info->data;
		BasePWidget *basep = BASEP_WIDGET (drawer->drawer);

		set_drawer_applet_orient (drawer,
					  panel_widget_get_applet_orient (panel));
		gtk_widget_queue_resize (drawer->drawer);
		gtk_container_foreach (GTK_CONTAINER (basep->panel),
				       orient_change_foreach,
				       (gpointer)basep->panel);
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
panel_orient_change (GtkWidget *widget, gpointer data)
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
	if (info->type == APPLET_BONOBO)
		panel_applet_frame_change_size (
			PANEL_APPLET_FRAME (info->widget), panel->sz);
}

static void
size_change_foreach(GtkWidget *w, gpointer data)
{
	AppletInfo *info = g_object_get_data (G_OBJECT (w), "applet_info");
	PanelWidget *panel = data;
	
	size_change(info,panel);
}


static void
panel_size_change (GtkWidget *widget, gpointer data)
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
	if (info->type == APPLET_BONOBO)
		panel_applet_frame_change_background (
			PANEL_APPLET_FRAME (info->widget), panel->background.type);
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
panel_back_change (GtkWidget *widget, gpointer data)
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
					-ABS (widget->allocation.width) - 1,
					-ABS (widget->allocation.height) - 1);
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

static gboolean
deactivate_idle (gpointer data)
{
	PanelData *pd = data;
	pd->deactivate_idle = 0;

	pd->insertion_pos = -1;

	return FALSE;
}

static void
menu_deactivate(GtkWidget *w, PanelData *pd)
{
	pd->menu_age = 0;
	if (pd->deactivate_idle == 0)
		pd->deactivate_idle = g_idle_add (deactivate_idle, pd);
	if(BASEP_IS_WIDGET(pd->panel))
		BASEP_WIDGET(pd->panel)->autohide_inhibit = FALSE;
}

static void
move_panel_to_cursor (GtkWidget *panel)
{
	GdkScreen *screen;
	GdkWindow *root_window;
	int        x, y;

	g_return_if_fail (GTK_IS_WINDOW (panel));

	screen = gtk_window_get_screen (GTK_WINDOW (panel));
	root_window = gdk_screen_get_root_window (screen);

	gdk_window_get_pointer (root_window, &x, &y, NULL);

	if (BASEP_IS_WIDGET (panel))
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);
}

static gboolean
panel_move_timeout (GtkWidget *panel)
{
	if (panel_dragged && panel_been_moved)
		move_panel_to_cursor (panel);
	
	panel_been_moved = FALSE;
	panel_dragged_timeout = 0;

	return FALSE;
}

static void
panel_remove_applets (PanelWidget *panel)
{
	GList *l;

	for (l = panel->applet_list; l; l = l->next) {
		AppletData *applet_data = l->data;
		AppletInfo *info;

		info = g_object_get_data (
				G_OBJECT (applet_data->applet), "applet_info");

		if (info && info->type == APPLET_LAUNCHER)
			launcher_properties_destroy (info->data);
			
	}
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
		
	kill_config_dialog (widget);

	if ((BASEP_IS_WIDGET (widget) && !DRAWER_IS_WIDGET (widget)) ||
	    FOOBAR_IS_WIDGET (widget))
		base_panels--;

	if (pd->menu)
		g_object_unref (pd->menu);
	pd->menu = NULL;

	pd->panel = NULL;

	if (pd->deactivate_idle != 0)
		g_source_remove (pd->deactivate_idle);
	pd->deactivate_idle = 0;

	g_object_set_data (G_OBJECT (widget), "PanelData", NULL);

	panel_list = g_slist_remove (panel_list, pd);
	g_free (pd);
}

static void
panel_applet_move(PanelWidget *panel, GtkWidget *widget, gpointer data)
{
	AppletInfo *info;

	info = g_object_get_data (G_OBJECT (widget), "applet_info");

	g_return_if_fail (info);

	panel_applet_save_position (info, info->gconf_key, FALSE);
}

static GtkWidget *
panel_menu_get (PanelWidget *panel, PanelData *pd)
{
	if (pd->menu != NULL)
		return pd->menu;
	
	pd->menu = g_object_ref (create_panel_context_menu (panel));
	gtk_object_sink (GTK_OBJECT (pd->menu));
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
panel_initiate_move (GtkWidget *widget,
		     guint32    event_time)
{
	PanelWidget *panel;
	BasePWidget *basep;
	GdkCursor   *cursor;

	if (FOOBAR_IS_WIDGET (widget))
		return FALSE;

	g_assert (BASEP_IS_WIDGET (widget));

	basep = BASEP_WIDGET (widget);
	panel = PANEL_WIDGET (basep->panel);

	if (DRAWER_IS_WIDGET (widget) && !panel_applet_in_drag) {
		panel_widget_applet_drag_start (
			PANEL_WIDGET (panel->master_widget->parent),
			panel->master_widget,
			PW_DRAG_OFF_CURSOR);
		return TRUE;
	}

	if (panel_dragged)
		return FALSE;

	gtk_grab_add (widget);

	cursor = gdk_cursor_new (GDK_FLEUR);
	gdk_pointer_grab (widget->window, FALSE, PANEL_EVENT_MASK,
			  NULL, cursor, event_time);
	gdk_cursor_unref (cursor);

	basep->autohide_inhibit = TRUE;
	basep_widget_init_offsets (basep);
	panel_dragged = TRUE;

	return TRUE;
}

static gboolean
panel_do_popup_menu (PanelWidget *panel,
		     BasePWidget *basep,
		     GtkWidget   *widget,
		     int          screen,
		     guint        button,
		     guint32      activate_time)
{
	if (!panel_applet_in_drag) {
		GtkWidget *menu;

		menu = make_popup_panel_menu (panel);
		if (basep) {
                        basep->autohide_inhibit = TRUE;
			basep_widget_queue_autohide (basep);
		}

		gtk_menu_set_screen (GTK_MENU (menu),
				     panel_screen_from_number (screen));

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
	int          screen = 0;

	panel = PANEL_WIDGET (widget);
	if (BASEP_IS_WIDGET (data)) {
		basep = BASEP_WIDGET (data);
		screen = basep->screen;
	} else if (FOOBAR_IS_WIDGET (data))
		screen = FOOBAR_WIDGET (data)->screen;

	return panel_do_popup_menu (
			panel, basep, widget, screen, 3, GDK_CURRENT_TIME);
}

static gboolean
panel_end_move (GtkWidget *widget, GdkEventButton *bevent)
{
	BasePWidget *basep;

	if (!panel_dragged || !BASEP_IS_WIDGET (widget))
		return FALSE;

	basep = BASEP_WIDGET (widget);

	basep_widget_set_pos (basep, bevent->x_root, bevent->y_root);
	basep->autohide_inhibit = FALSE;
	basep_widget_queue_autohide (basep);

	gtk_grab_remove (widget);
	gdk_pointer_ungrab (bevent->time);
	panel_dragged = FALSE;

	if (panel_dragged_timeout != 0)
		g_source_remove (panel_dragged_timeout);
	panel_dragged_timeout = 0;
	panel_been_moved = FALSE;

	return TRUE;
}

static gboolean
panel_event(GtkWidget *widget, GdkEvent *event)
{
	PanelData *pd;
	PanelWidget *panel = NULL;
	BasePWidget *basep = NULL;
	GdkEventButton *bevent;
	GdkEventKey *kevent;
	int x, y;
	int screen = 0;

	if (BASEP_IS_WIDGET (widget)) {
		basep = BASEP_WIDGET (widget);
		panel = PANEL_WIDGET (basep->panel);
		screen = basep->screen;
	} else if (FOOBAR_IS_WIDGET (widget)) {
		panel = PANEL_WIDGET (FOOBAR_WIDGET (widget)->panel);
		screen= FOOBAR_WIDGET (widget)->screen;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		bevent = (GdkEventButton *) event;
		if (panel_dragged) {
			return panel_end_move (widget, bevent);
		}
		switch (bevent->button) {
		case 3:
			/* Store the point where the popup menu was started to
			 * insert applets at that point */
			pd = g_object_get_data (G_OBJECT (widget), "PanelData");
			gtk_widget_get_pointer (GTK_WIDGET (panel), &x, &y);
			if (panel->orient == GTK_ORIENTATION_VERTICAL)
				pd->insertion_pos = y;
			else
				pd->insertion_pos = x;

			if (panel_do_popup_menu (panel, basep, widget, screen,
						 bevent->button, bevent->time))
				return TRUE;
			break;
		case 2:
			if (!commie_mode)
				return panel_initiate_move (widget,
							    bevent->time);
			break;
		default: break;
		}
		break;

	case GDK_BUTTON_RELEASE:
		bevent = (GdkEventButton *) event;
		if (panel_dragged)
			return panel_end_move (widget, bevent);
		break;
	case GDK_MOTION_NOTIFY:
		if (panel_dragged) {
			if (!panel_dragged_timeout) {
				panel_been_moved = FALSE;
				move_panel_to_cursor (widget);
				panel_dragged_timeout = g_timeout_add (30, (GSourceFunc) panel_move_timeout, widget);
			} else
				panel_been_moved = TRUE;
		}
		break;
	case GDK_KEY_PRESS:
		kevent = (GdkEventKey *)event;
		if (GTK_IS_SOCKET (GTK_WINDOW (widget)->focus_widget)) {
			/*
		  	 * If the focus widget is a GtkSocket, i.e. the
			 * focus is in an applet in another process, then key 
			 * bindings do not work. We get around this by
			 * activating the key binding we require here.
			 */ 
			if (kevent->keyval == GDK_F10 && kevent->state == GDK_CONTROL_MASK)
				return gtk_bindings_activate (GTK_OBJECT (panel), kevent->keyval, kevent->state);
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
	if (commie_mode)
		return FALSE;

	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton *bevent = (GdkEventButton *) event;

		if (bevent->button == 1 ||
		    bevent->button == 2) {
			if (panel_dragged) {
				return panel_end_move (widget, bevent);
			} else {
				return panel_initiate_move (panelw, bevent->time);
			}
		}
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
	if (panel_uri_exists (tmp)) {
		g_free (tmp);
		drop_menu (panel, pos, dir);
		return;
	}
	g_free (tmp);

	tmp = g_build_path ("/", dir, ".order", NULL);
	if (panel_uri_exists (tmp)) {
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
			    strcmp(mimetype, "application/x-desktop") == 0 ||
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
				    panel->background.image,
				    panel->background.fit_image,
				    panel->background.stretch_image,
				    panel->background.rotate_image,
				    &panel->background.color);

	if (BASEP_IS_WIDGET (panel->panel_parent))
		basep_update_frame (BASEP_WIDGET (panel->panel_parent));
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

	if (BASEP_IS_WIDGET (panel->panel_parent))
		basep_update_frame (BASEP_WIDGET (panel->panel_parent));
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
		    old_launcher->button != NULL) {
			if (old_launcher->prop_dialog != NULL) {
				g_signal_handler_disconnect ( old_launcher->button,
						old_launcher->destroy_handler);
				launcher_properties_destroy(old_launcher);
			}
			panel_applet_remove_in_idle (old_launcher->info);
		}
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
	    sscanf (applet_type, "DRAWER:%d", &applet_num) == 1) {
		if (action != GDK_ACTION_MOVE)
			g_warning ("Only MOVE supported for menus/drawers");
		move_applet (panel, pos, applet_num);

	} else if (strncmp (applet_type, "MENU:", strlen("MENU:")) == 0) {
		const char *menu = &applet_type[strlen ("MENU:")];
		if (strcmp (menu, "MAIN") == 0)
			drop_menu (panel, pos, NULL);
		else
			drop_menu (panel, pos, menu);

	} else if (!strcmp (applet_type, "DRAWER:NEW"))
		load_drawer_applet (NULL, NULL, NULL, panel, pos, TRUE, NULL);

	else if (!strncmp (applet_type, "ACTION:", strlen ("ACTION:")))
		remove_applet = panel_action_button_load_from_drag (
					applet_type, panel, pos,
					TRUE, NULL, &applet_num);

	else if (!strcmp(applet_type,"LAUNCHER:ASK"))
		ask_about_launcher (NULL, panel, pos, TRUE);

	if (remove_applet &&
	    action == GDK_ACTION_MOVE) {
		AppletInfo *info = g_slist_nth_data (applets, applet_num);

		if (info != NULL)
			panel_applet_clean (info, TRUE);
	}
}

static void
drop_color (PanelWidget *panel,
	    int          pos,
	    guint16     *dropped)
{
	PanelColor color;

	if (!dropped)
		return;

	color.gdk.red   = dropped [0];
	color.gdk.green = dropped [1];
	color.gdk.blue  = dropped [2];
	color.alpha     = 65535;

	panel_widget_set_back_color (panel, &color);

	if (BASEP_IS_WIDGET (panel->panel_parent))
		basep_update_frame (BASEP_WIDGET (panel->panel_parent));
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

gboolean
panel_check_dnd_target_data (GtkWidget      *widget,
			     GdkDragContext *context,
			     guint          *ret_info,
			     GdkAtom        *ret_atom)
{
	GList *l;

	g_return_val_if_fail (widget, FALSE);

	if (!BASEP_IS_WIDGET  (widget) &&
	    !FOOBAR_IS_WIDGET (widget) &&
	    !BUTTON_IS_WIDGET (widget))
		return FALSE;

	if (!(context->actions & (GDK_ACTION_COPY|GDK_ACTION_MOVE)))
		return FALSE;

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

gboolean
panel_check_drop_forbidden (PanelWidget    *panel,
			    GdkDragContext *context,
			    guint           info,
			    guint           time_)
{
	if (!panel)
		return FALSE;

	if (info == TARGET_APPLET_INTERNAL) {
		GtkWidget *source_widget;

		source_widget = gtk_drag_get_source_widget (context);

		if (BUTTON_IS_WIDGET (source_widget)) {
			GSList *forb;

			forb = g_object_get_data (G_OBJECT (source_widget),
						  PANEL_APPLET_FORBIDDEN_PANELS);

			if (g_slist_find (forb, panel))
				return FALSE;
		}
	}

	if (info == TARGET_ICON_INTERNAL ||
	    info == TARGET_APPLET_INTERNAL) {
		if (context->actions & GDK_ACTION_MOVE)
			gdk_drag_status (context, GDK_ACTION_MOVE, time_);
		else
			gdk_drag_status (context, context->suggested_action, time_);

	} else if (context->actions & GDK_ACTION_COPY)
		gdk_drag_status (context, GDK_ACTION_COPY, time_);
	else
		gdk_drag_status (context, context->suggested_action, time_);

	return TRUE;

}

static gboolean
drag_motion_cb (GtkWidget	   *widget,
		GdkDragContext     *context,
		gint                x,
		gint                y,
		guint               time)
{
	PanelWidget *panel = NULL;
	guint        info;

	if (!panel_check_dnd_target_data (widget, context, &info, NULL))
		return FALSE;

	if (BASEP_IS_WIDGET (widget)) {
		BasePWidget *basep = BASEP_WIDGET (widget);

		panel = PANEL_WIDGET (basep->panel);

	} else if (FOOBAR_IS_WIDGET (widget))
		panel = PANEL_WIDGET (FOOBAR_WIDGET (widget)->panel);

	if (!panel_check_drop_forbidden (panel, context, info, time))
		return FALSE;

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

	if (!panel_check_dnd_target_data (widget, context, NULL, &ret_atom))
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

void
panel_receive_dnd_data (PanelWidget      *panel,
			guint             info,
			int               pos,
			GtkSelectionData *selection_data,
			GdkDragContext   *context,
			guint             time_,
			gboolean          is_foobar)
{
	switch (info) {
	case TARGET_URL:
		drop_urilist (panel, pos, (char *)selection_data->data, !is_foobar);
		break;
	case TARGET_NETSCAPE_URL:
		drop_url (panel, pos, (char *)selection_data->data);
		break;
	case TARGET_COLOR:
		drop_color (panel, pos, (guint16 *)selection_data->data);
		break;
	case TARGET_BGIMAGE:
		if (is_foobar)
			drop_bgimage (panel, (char *)selection_data->data);
		break;
	case TARGET_BACKGROUND_RESET:
		if (is_foobar)
			drop_background_reset (panel);
		break;
	case TARGET_DIRECTORY:
		drop_directory (panel, pos, (char *)selection_data->data);
		break;
	case TARGET_APPLET:
		if (!selection_data->data) {
			gtk_drag_finish (context, FALSE, FALSE, time_);
			return;
		}
		panel_applet_frame_load ((char *)selection_data->data,
					 panel, pos, TRUE, NULL);
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
		gtk_drag_finish (context, FALSE, FALSE, time_);
		return;
	}

	gtk_drag_finish (context, TRUE, FALSE, time_);
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
	if (!panel_check_dnd_target_data (widget, context, &info, NULL)) {
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

	panel_receive_dnd_data (
		panel, info, pos, selection_data, context,
		time, FOOBAR_IS_WIDGET (widget));
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
	g_signal_connect (G_OBJECT (panel),
			  "back_change",
			  G_CALLBACK (panel_back_change),
			  NULL);
	g_signal_connect (G_OBJECT (panel),
			  "size_change",
			  G_CALLBACK (panel_size_change),
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

PanelData *
panel_setup (GtkWidget *panelw)
{
	PanelData *pd;
	BasePWidget *basep = NULL; 
	PanelWidget *panel = NULL;

	g_return_val_if_fail (panelw != NULL, NULL);

	if (BASEP_IS_WIDGET (panelw)) {
		basep = BASEP_WIDGET(panelw);
		panel = PANEL_WIDGET(basep->panel);
	} else if (FOOBAR_IS_WIDGET (panelw)) {
		panel = PANEL_WIDGET (FOOBAR_WIDGET (panelw)->panel);
	}

	pd = g_new0 (PanelData,1);
	pd->menu = NULL;
	pd->menu_age = 0;
	pd->panel = panelw;
	pd->insertion_pos = -1;
	pd->deactivate_idle = 0;

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
	if ( ! FOOBAR_IS_WIDGET(panelw))
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

	return pd;
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
panel_register_window_icon (void)
{
	char *panel_icon;

	panel_icon = panel_pixmap_discovery ("gnome-panel.png", FALSE);

	if (panel_icon) {
		gnome_window_icon_set_default_from_file (panel_icon);
		g_free (panel_icon);
	}
}

GdkScreen *
panel_screen_from_toplevel (GtkWidget *panel)
{
	GdkScreen  *retval = NULL;
	GdkDisplay *display;

	display = gdk_display_get_default ();

	if (BASEP_IS_WIDGET (panel))
	        retval = gdk_display_get_screen (
	                        display, BASEP_WIDGET (panel)->screen);

	else if (FOOBAR_IS_WIDGET (panel))
	        retval = gdk_display_get_screen (
	                        display, FOOBAR_WIDGET (panel)->screen);

	return retval;
}

int
panel_monitor_from_toplevel (GtkWidget *panel)
{
	int retval = -1;

	if (BASEP_IS_WIDGET (panel))
	        retval = BASEP_WIDGET (panel)->monitor;

	else if (FOOBAR_IS_WIDGET (panel))
	        retval = FOOBAR_WIDGET (panel)->monitor;

	return retval;
}

int
panel_screen_from_panel_widget (PanelWidget *panel)
{
	g_return_val_if_fail (PANEL_IS_WIDGET (panel), 0);
	g_return_val_if_fail (panel->panel_parent != NULL, 0);
	g_return_val_if_fail (BASEP_IS_WIDGET (panel->panel_parent) ||
			      FOOBAR_IS_WIDGET (panel->panel_parent), 0);

	if (BASEP_IS_WIDGET (panel->panel_parent))
		return BASEP_WIDGET (panel->panel_parent)->screen;
	else
		return FOOBAR_WIDGET (panel->panel_parent)->screen;
}

int
panel_monitor_from_panel_widget (PanelWidget *panel)
{
	g_return_val_if_fail (PANEL_IS_WIDGET (panel), 0);
	g_return_val_if_fail (panel->panel_parent != NULL, 0);
	g_return_val_if_fail (BASEP_IS_WIDGET (panel->panel_parent) ||
			      FOOBAR_IS_WIDGET (panel->panel_parent), 0);

	if (BASEP_IS_WIDGET (panel->panel_parent))
		return BASEP_WIDGET (panel->panel_parent)->monitor;
	else
		return FOOBAR_WIDGET (panel->panel_parent)->monitor;
}

gboolean
panel_is_applet_right_stick (GtkWidget *applet)
{
        PanelWidget *panel;

        g_return_val_if_fail (GTK_IS_WIDGET (applet), FALSE);
        g_return_val_if_fail (PANEL_IS_WIDGET (applet->parent), FALSE);

        panel = PANEL_WIDGET (applet->parent);

        /* These types of panels are *always* packed */
        if (ALIGNED_IS_WIDGET (panel->panel_parent) ||
            BORDER_IS_WIDGET (panel->panel_parent)  ||
            SLIDING_IS_WIDGET (panel->panel_parent) ||
            FLOATING_IS_WIDGET (panel->panel_parent))
                return FALSE;

        return panel_widget_is_applet_stuck (panel, applet);
}

static char *
panel_get_string (const char *profile,
		  const char *panel_id,
		  const char *key,
		  const char *default_val)
{
	const char *full_key;
	char       *retval;

	full_key = panel_gconf_full_key (
			PANEL_GCONF_PANELS, profile, panel_id, key);
	retval = panel_gconf_get_string (full_key, default_val);

	return retval;
}

static int
panel_get_int (const char *profile,
	       const char *panel_id,
	       const char *key,
	       gint        default_val)
{
	const char *full_key;
	int         retval;

	full_key = panel_gconf_full_key (
			PANEL_GCONF_PANELS, profile, panel_id, key);
	retval = panel_gconf_get_int (full_key, default_val);

	return retval;
}

static gboolean
panel_get_bool (const char *profile,
		const char *panel_id,
		const char *key,
		gboolean    default_val)
{
	const char *full_key;
	gboolean    retval;

	full_key = panel_gconf_full_key (
			PANEL_GCONF_PANELS, profile, panel_id, key);
	retval = panel_gconf_get_bool (full_key, default_val);

	return retval;
}

static void
panel_set_string (const char *profile,
		  const char *panel_id,
		  const char *key,
		  const char *value)
{
	const char *full_key;

	full_key = panel_gconf_full_key (
			PANEL_GCONF_PANELS, profile, panel_id, key);
	panel_gconf_set_string (full_key, value);	
}

static void
panel_set_int (const char *profile,
	       const char *panel_id,
	       const char *key,
	       int         value)
{
	const char *full_key;

	full_key = panel_gconf_full_key (
			PANEL_GCONF_PANELS, profile, panel_id, key);
	panel_gconf_set_int (full_key, value);	
}

static void
panel_set_bool (const char *profile,
		const char *panel_id,
		const char *key,
		gboolean    value)
{
	const char *full_key;

	full_key = panel_gconf_full_key (
			PANEL_GCONF_PANELS, profile, panel_id, key);
	panel_gconf_set_bool (full_key, value);	
}

static void
panel_load_fallback_default_panel (int screen)
{
	GtkWidget *panel;

	panel = foobar_widget_new (NULL, screen, 0);
	panel_save_to_gconf (panel_setup (panel));
	gtk_widget_show (panel);
}

void
panel_load_global_config (void)
{
	GSList *li, *list;

	list = panel_gconf_all_global_entries ();

	for (li = list; li != NULL; li = li->next) {
		GConfEntry *entry = li->data;
		li->data = NULL;
		panel_global_config_set_entry (entry);
		gconf_entry_free (entry);
	}

	g_slist_free (list);

	panel_apply_global_config ();
}

void
panel_save_global_config (void)
{
	GConfChangeSet *change_set;
	const char     *full_key;

	change_set = gconf_change_set_new ();

	/*
	 * keep in sync with panel-config-global.h and
	 * panel-global-config.schemas
	 */

	full_key = panel_gconf_global_key ("tooltips_enabled");
	gconf_change_set_set_bool (change_set, full_key, global_config.tooltips_enabled);

	full_key = panel_gconf_global_key ("keep_menus_in_memory");
	gconf_change_set_set_bool (change_set, full_key, global_config.keep_menus_in_memory);

	full_key = panel_gconf_global_key ("enable_animations");
	gconf_change_set_set_bool (change_set, full_key, global_config.enable_animations);

	full_key = panel_gconf_global_key ("panel_minimized_size");
	gconf_change_set_set_int (change_set, full_key, global_config.minimized_size);

	full_key = panel_gconf_global_key ("panel_show_delay");
	gconf_change_set_set_int (change_set, full_key, global_config.show_delay);

	full_key = panel_gconf_global_key ("panel_animation_speed");
	gconf_change_set_set_string (
			change_set, full_key ,
			gconf_enum_to_string (panel_speed_type_enum_map,
					      global_config.animation_speed));

	full_key = panel_gconf_global_key ("panel_hide_delay");
	gconf_change_set_set_int (change_set, full_key, global_config.hide_delay);

	full_key = panel_gconf_global_key ("enable_key_bindings");
	gconf_change_set_set_bool (change_set, full_key, global_config.keys_enabled);

	full_key = panel_gconf_global_key ("menu_key");
	gconf_change_set_set_string (change_set, full_key, global_config.menu_key.str);

	full_key = panel_gconf_global_key ("run_key");
	gconf_change_set_set_string (change_set, full_key, global_config.run_key.str);

	full_key = panel_gconf_global_key ("screenshot-key");
	gconf_change_set_set_string (change_set, full_key, global_config.screenshot_key.str);
	
	full_key = panel_gconf_global_key ("window_screenshot_key");
	gconf_change_set_set_string (change_set, full_key, global_config.window_screenshot_key.str);

	full_key = panel_gconf_global_key ("drawer_autoclose");
	gconf_change_set_set_bool (change_set, full_key, global_config.drawer_auto_close);

	full_key = panel_gconf_global_key ("confirm_panel_remove");
	gconf_change_set_set_bool (change_set, full_key, global_config.confirm_panel_remove);

	full_key = panel_gconf_global_key ("highlight_launchers_on_mouseover");
	gconf_change_set_set_bool (change_set, full_key, global_config.highlight_when_over);

	gconf_client_commit_change_set (panel_gconf_get_client (), change_set, FALSE, NULL);

	gconf_change_set_unref (change_set);
}

void
panel_apply_global_config (void)
{
	GSList *l;

	if (global_config.tooltips_enabled)
		gtk_tooltips_enable (panel_tooltips);
	else
		gtk_tooltips_disable (panel_tooltips);

	for (l = panel_list; l; l = l->next) {
		PanelData *pd = l->data;

		if (BASEP_IS_WIDGET (pd->panel))
			basep_update_frame (BASEP_WIDGET (pd->panel));
	}

	panel_global_keys_setup ();
}


static GtkWidget *
panel_load_edge_panel_from_gconf (const char          *profile,
				  const char          *panel_id,
				  int                  screen,
				  int                  monitor,
				  BasePMode            mode,
				  BasePState           state,
				  int                  size,
				  gboolean             hidebuttons_enabled,
				  gboolean             hidebutton_pixmaps_enabled,
				  PanelBackgroundType  back_type,
				  const char          *back_pixmap,
				  gboolean             fit_pixmap_bg,
				  gboolean             stretch_pixmap_bg,
				  gboolean             rotate_pixmap_bg,
				  PanelColor          *back_color)
{
	BorderEdge  edge = BORDER_BOTTOM;
	char       *tmp_str;

	tmp_str = panel_get_string (profile, panel_id,
				    "screen_edge", "panel-edge-bottom");
	gconf_string_to_enum (
		panel_edge_type_enum_map, tmp_str, (int *) &edge);
	g_free (tmp_str);
			
	return edge_widget_new (panel_id,
				screen,
				monitor,
				edge, 
				mode, state,
				size,
				hidebuttons_enabled,
				hidebutton_pixmaps_enabled,
				back_type, back_pixmap,
				fit_pixmap_bg,
				stretch_pixmap_bg,
				rotate_pixmap_bg,
				back_color);
	
}

static GtkWidget *
panel_load_aligned_panel_from_gconf (const char          *profile,
				     const char          *panel_id,
				     int                  screen,
				     int                  monitor,
				     BasePMode            mode,
				     BasePState           state,
				     int                  size,
				     gboolean             hidebuttons_enabled,
				     gboolean             hidebutton_pixmaps_enabled,
				     PanelBackgroundType  back_type,
				     const char          *back_pixmap,
				     gboolean             fit_pixmap_bg,
				     gboolean             stretch_pixmap_bg,
				     gboolean             rotate_pixmap_bg,
				     PanelColor          *back_color)
{
	AlignedAlignment  align = ALIGNED_LEFT;
	BorderEdge        edge = BORDER_BOTTOM;
	char             *tmp_str;

	tmp_str = panel_get_string (profile, panel_id,
				    "screen_edge", "panel-edge-bottom");
	gconf_string_to_enum (
		panel_edge_type_enum_map, tmp_str, (int *) &edge);
	g_free (tmp_str);

	tmp_str = panel_get_string (profile, panel_id,
				    "panel_align", "panel-alignment-left");
	gconf_string_to_enum (
		panel_alignment_type_enum_map, tmp_str, (int *) &align);
	g_free (tmp_str);

	return aligned_widget_new (panel_id,
				   screen,
				   monitor,
				   align,
				   edge,
				   mode,
				   state,
				   size,
				   hidebuttons_enabled,
				   hidebutton_pixmaps_enabled,
				   back_type, back_pixmap,
				   fit_pixmap_bg,
				   stretch_pixmap_bg,
				   rotate_pixmap_bg,
				   back_color);
}

static GtkWidget *
panel_load_sliding_panel_from_gconf (const char          *profile,
				     const char          *panel_id,
				     int                  screen,
				     int                  monitor,
				     BasePMode            mode,
				     BasePState           state,
				     int                  size,
				     gboolean             hidebuttons_enabled,
				     gboolean             hidebutton_pixmaps_enabled,
				     PanelBackgroundType  back_type,
				     const char          *back_pixmap,
				     gboolean             fit_pixmap_bg,
				     gboolean             stretch_pixmap_bg,
				     gboolean             rotate_pixmap_bg,
				     PanelColor          *back_color)
{
	SlidingAnchor  anchor = SLIDING_ANCHOR_LEFT;
	BorderEdge     edge = BORDER_BOTTOM;
	gint16         offset;
	char          *tmp_str;

	tmp_str = panel_get_string (profile, panel_id,
				    "screen_edge", "panel-edge-bottom");
	gconf_string_to_enum (
		panel_edge_type_enum_map, tmp_str, (int *) &edge);
	g_free (tmp_str);

	tmp_str = panel_get_string (profile, panel_id,
				    "panel_anchor", "panel-anchor-left");
	gconf_string_to_enum (
		panel_anchor_type_enum_map, tmp_str, (int *) &anchor);
	g_free (tmp_str);
			
	offset = panel_get_int (profile, panel_id, "panel_offset", 0);
	
	return sliding_widget_new (panel_id,
				   screen,
				   monitor,
				   anchor,
				   offset,
				   edge,
				   mode,
				   state,
				   size,
				   hidebuttons_enabled,
				   hidebutton_pixmaps_enabled,
				   back_type, back_pixmap,
				   fit_pixmap_bg,
				   stretch_pixmap_bg,
				   rotate_pixmap_bg,
				   back_color);
}

static GtkWidget *
panel_load_drawer_panel_from_gconf (const char          *profile,
				    const char          *panel_id,
				    int                  screen,
				    int                  monitor,
				    BasePState           state,
				    int                  size,
				    gboolean             hidebuttons_enabled,
				    gboolean             hidebutton_pixmaps_enabled,
				    PanelBackgroundType  back_type,
				    const char          *back_pixmap,
				    gboolean             fit_pixmap_bg,
				    gboolean             stretch_pixmap_bg,
				    gboolean             rotate_pixmap_bg,
				    PanelColor          *back_color)
{
	int   orient = PANEL_ORIENT_UP;
	char *tmp_str;

	tmp_str = panel_get_string (profile, panel_id,
				    "panel_orient", "panel-orient-up");
	gconf_string_to_enum (
		panel_orient_type_enum_map, tmp_str, (int *) &orient);
	g_free (tmp_str);

	return drawer_widget_new (panel_id,
				  screen,
				  monitor,
				  (PanelOrient) orient,
				  BASEP_EXPLICIT_HIDE, 
				  state,
				  size,
				  hidebuttons_enabled,
				  hidebutton_pixmaps_enabled,
				  back_type, back_pixmap,
				  fit_pixmap_bg,
				  stretch_pixmap_bg,
				  rotate_pixmap_bg,
				  back_color);
}

static GtkWidget *
panel_load_floating_panel_from_gconf (const char          *profile,
				      const char          *panel_id,
				      int                  screen,
				      int                  monitor,
				      BasePMode            mode,
				      BasePState           state,
				      int                  size,
				      gboolean             hidebuttons_enabled,
				      gboolean             hidebutton_pixmaps_enabled,
				      PanelBackgroundType  back_type,
				      const char          *back_pixmap,
				      gboolean             fit_pixmap_bg,
				      gboolean             stretch_pixmap_bg,
				      gboolean             rotate_pixmap_bg,
				      PanelColor          *back_color)
{
	GtkOrientation  orient = GTK_ORIENTATION_HORIZONTAL;
	int             x, y;
	char           *tmp_str;

	tmp_str = panel_get_string (profile, panel_id,
				    "panel_orient", "panel-orientation-horizontal");
	gconf_string_to_enum (
		panel_orientation_type_enum_map, tmp_str, (int *) &orient);
	g_free (tmp_str);

	x = panel_get_int (profile, panel_id, "panel_x_position", 0);
	y = panel_get_int (profile, panel_id, "panel_y_position", 0);
			
	return floating_widget_new (panel_id,
				    screen,
				    monitor,
				    x,
				    y,
				    orient,
				    mode,
				    state,
				    size,
				    hidebuttons_enabled,
				    hidebutton_pixmaps_enabled,
				    back_type, back_pixmap,
				    fit_pixmap_bg,
				    stretch_pixmap_bg,
				    rotate_pixmap_bg,
				    back_color);
}
	
static void
panel_load_panel_from_gconf (const char *profile,
			     const char *panel_id)
{
	GtkWidget           *panel = NULL;
	PanelType            type = EDGE_PANEL;
	PanelBackgroundType  back_type = PANEL_BACK_NONE;
	BasePState           state;
	BasePMode            mode;
	PanelColor           back_color = { { 0, 0, 0, 0 }, 0xffff};
	GdkColor             gdkcolor = {0, 0, 0, 1};
	gboolean             fit_pixmap_bg;
	gboolean             stretch_pixmap_bg;
	gboolean             rotate_pixmap_bg;
	gboolean             hidebuttons_enabled;
	gboolean             hidebutton_pixmaps_enabled;
	int                  screen;
	int                  monitor;
	int                  size = PANEL_SIZE_SMALL;
	char                *back_pixmap;
	char                *tmp_str;

	screen  = panel_get_int (profile, panel_id, "screen", -1);
	monitor = panel_get_int (profile, panel_id, "monitor", -1);
	if (screen == -1 || monitor == -1) {
		/* Backwards compat:
		 *   In 2.0.0, we only had Xinerama support and the
		 *   monitor number was saved as "screen_id".
		 */
		screen = 0;
		monitor = panel_get_int (profile, panel_id, "screen_id", 0);
	}

	if (screen >= multiscreen_screens () ||
	    monitor >= multiscreen_monitors (screen))
		return;

	back_pixmap = panel_get_string (profile, panel_id,
					"panel_background_pixmap", NULL);
	if (string_empty (back_pixmap)) {
		g_free (back_pixmap);
		back_pixmap = NULL;
	}

	tmp_str = panel_get_string (profile, panel_id, "panel_background_color", NULL);
	if (!tmp_str || !tmp_str [0]) {
		gdk_color_parse (tmp_str, &gdkcolor);
		back_color.gdk = gdkcolor;
	}
	g_free (tmp_str);

	back_color.alpha = panel_get_int (
				profile, panel_id, "panel_background_color_alpha", 0xFFFF);

	tmp_str = panel_get_string (
			profile, panel_id, "panel_background_type", "no-background");
	gconf_string_to_enum (background_type_enum_map, tmp_str, (int *) &back_type);
	g_free (tmp_str);
		
	fit_pixmap_bg = panel_get_bool (profile, panel_id,
					"panel_background_pixmap_fit", FALSE);

	stretch_pixmap_bg = panel_get_bool (profile, panel_id,
					    "panel_background_pixmap_stretch", FALSE);

	rotate_pixmap_bg = panel_get_bool (profile, panel_id,
					   "panel_background_pixmap_rotate", FALSE);
	
	tmp_str = panel_get_string (profile, panel_id, "panel_size", "panel-size-small");
	gconf_string_to_enum (panel_size_type_enum_map, tmp_str, &size);
	g_free (tmp_str);
		
	hidebuttons_enabled =
		panel_get_bool (profile, panel_id, "hide_buttons_enabled", TRUE);
		
	hidebutton_pixmaps_enabled =
		panel_get_bool (profile, panel_id, "hide_button_pixmaps_enabled", TRUE);

	state   = panel_get_int (profile, panel_id, "panel_hide_state", 0);
	mode    = panel_get_int (profile, panel_id, "panel_hide_mode", 0);

	tmp_str = panel_get_string (profile, panel_id, "panel_type", "edge-panel");
	gconf_string_to_enum (panel_type_type_enum_map, tmp_str, (int *) &type);
	g_free (tmp_str);

	switch (type) {
	case EDGE_PANEL:
		panel = panel_load_edge_panel_from_gconf (
				profile, panel_id, screen, monitor,
				mode, state, size, hidebuttons_enabled,
				hidebutton_pixmaps_enabled,
				back_type, back_pixmap,
				fit_pixmap_bg, stretch_pixmap_bg,
				rotate_pixmap_bg, &back_color);
		break;
	case ALIGNED_PANEL:
		panel = panel_load_aligned_panel_from_gconf (
				profile, panel_id, screen, monitor,
				mode, state, size, hidebuttons_enabled,
				hidebutton_pixmaps_enabled,
				back_type, back_pixmap,
				fit_pixmap_bg, stretch_pixmap_bg,
				rotate_pixmap_bg, &back_color);
		break;

	case SLIDING_PANEL:
		panel = panel_load_sliding_panel_from_gconf (
				profile, panel_id, screen, monitor,
				mode, state, size, hidebuttons_enabled,
				hidebutton_pixmaps_enabled,
				back_type, back_pixmap,
				fit_pixmap_bg, stretch_pixmap_bg,
				rotate_pixmap_bg, &back_color);
		break;
	case DRAWER_PANEL:
		panel = panel_load_drawer_panel_from_gconf (
				profile, panel_id, screen, monitor,
				state, size, hidebuttons_enabled,
				hidebutton_pixmaps_enabled,
				back_type, back_pixmap,
				fit_pixmap_bg, stretch_pixmap_bg,
				rotate_pixmap_bg, &back_color);
		break;
	case FLOATING_PANEL:
		panel = panel_load_floating_panel_from_gconf (
				profile, panel_id, screen, monitor,
				mode, state, size, hidebuttons_enabled,
				hidebutton_pixmaps_enabled,
				back_type, back_pixmap,
				fit_pixmap_bg, stretch_pixmap_bg,
				rotate_pixmap_bg, &back_color);
	
		break;
	case FOOBAR_PANEL:
		panel = foobar_widget_new (panel_id, screen, monitor);
		break;
	default:
		g_warning ("Unkown panel type: %d; ignoring.", type);
		break;
	}

	g_free (back_pixmap);

	if (panel) {
		panel_setup (panel);
		gtk_widget_show (panel);
	}
}

static gboolean
panel_load_default_panel_for_screen (const char *profile,
				     const char *panel_id,
				     int         screen)
{
	GConfClient *client;
	GError      *error = NULL;
	GSList      *panels, *l;
	const char  *key;
	char        *new_panel_id;

	new_panel_id = panel_gconf_load_default_config_for_screen (
				PANEL_GCONF_PANELS, profile, panel_id, screen, &error);
	if (error) {
		g_warning ("Could not load default config for panel '%s' on screen %d: '%s'\n",
			   panel_id, screen, error->message);
		g_error_free (error);
		return FALSE;
	}

	panel_set_int (profile, new_panel_id, "screen", screen);

	panel_load_panel_from_gconf (profile, new_panel_id);

	client = panel_gconf_get_client ();

	key = panel_gconf_general_key (profile, "panel_id_list");
	panels = gconf_client_get_list (
				client, key, GCONF_VALUE_STRING, NULL);

	for (l = panels; l; l = l->next)
		if (!strcmp (new_panel_id, l->data))
			break;
	
	if (!l) {
		panels = g_slist_append (panels, new_panel_id);	
		new_panel_id = NULL;

		gconf_client_set_list (
			client, key, GCONF_VALUE_STRING, panels, NULL);
	}

	panel_g_slist_deep_free (panels);
	g_free (new_panel_id);

	return TRUE;
}

static gboolean
panel_load_default_panels_for_screen (const char *profile,
				      int         screen)
{
	GConfClient *client;
	GSList      *panels, *l;
	GError      *error = NULL;
	int          loaded_panels;

	client = panel_gconf_get_client ();

	/* FIXME: "medium" shouldn't be hardcoded.
	 */
	panels = gconf_client_all_dirs (
			client,
			"/schemas/apps/panel/default_profiles/medium/panels",
			&error);
	if (error) {
		g_warning ("Cannot list default panels: '%s'\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	loaded_panels = 0;

	for (l = panels; l; l = l->next) {
		char *panel_id;

		panel_id = g_path_get_basename (l->data);

		if (panel_load_default_panel_for_screen (profile, panel_id, screen))
			loaded_panels++;

		g_free (panel_id);
		g_free (l->data);
	}

	g_slist_free (panels);

	panel_applet_load_defaults_for_screen (PANEL_GCONF_APPLETS, profile, screen);
	panel_applet_load_defaults_for_screen (PANEL_GCONF_OBJECTS, profile, screen);

	return loaded_panels > 0;
}

static void
panel_ensure_panel_per_screen (const char *profile,
			       int         screen)
{
	GSList *l;

	for (l = panel_list; l; l = l->next) {
		PanelData *pdata = l->data;

		if (BASEP_IS_WIDGET (pdata->panel) &&
		    BASEP_WIDGET (pdata->panel)->screen == screen)
			return;

		else if (FOOBAR_IS_WIDGET (pdata->panel) &&
			 FOOBAR_WIDGET (pdata->panel)->screen == screen)
			return;
	}

	if (!panel_load_default_panels_for_screen (profile, screen))
		panel_load_fallback_default_panel (screen);
}

void
panel_load_panels_from_gconf (void)
{
	GSList     *panel_ids;
	GSList     *l;
	const char *profile;
	const char *key;
	int         i;

	profile = panel_gconf_get_profile ();
	
	key = panel_gconf_general_key (profile, "panel_id_list");

	panel_ids = gconf_client_get_list (
			panel_gconf_get_client (), key, GCONF_VALUE_STRING, NULL);

	for (l = panel_ids; l; l = l->next)
		panel_load_panel_from_gconf (profile, l->data);

	if (!panel_ids) {
		panel_error_dialog (
			gdk_screen_get_default (),
			"no_panels_found",
			_("No panels were found in your configuration.  "
			  "I will create a menu panel for you"));

		panel_load_fallback_default_panel (0);
	}

	for (i = 0; i < multiscreen_screens (); i++)
		panel_ensure_panel_per_screen (profile, i);

	panel_g_slist_deep_free (panel_ids);
}

void
panel_save_to_gconf (PanelData *pd)
{
	BasePWidget *basep = NULL; 
	PanelWidget *panel = NULL;
	GConfClient *client;
	GSList      *panel_id_list;
	GSList      *l;
	const gchar *profile;
	const char  *key;
	char	    *color;
	int          screen = 0;
	int          monitor = 0;

	g_return_if_fail (pd != NULL);
	
	if (BASEP_IS_WIDGET (pd->panel)) {
		basep   = BASEP_WIDGET (pd->panel);
		panel   = PANEL_WIDGET (basep->panel);
		screen  = basep->screen;
		monitor = basep->monitor;

	} else if (FOOBAR_IS_WIDGET (pd->panel)) {
		panel   = PANEL_WIDGET (FOOBAR_WIDGET (pd->panel)->panel);
		screen  = FOOBAR_WIDGET (pd->panel)->screen;
		monitor = FOOBAR_WIDGET (pd->panel)->monitor;
	}

	profile = panel_gconf_get_profile ();
	client  = panel_gconf_get_client ();

	key = panel_gconf_general_key (profile, "panel_id_list");
	panel_id_list = gconf_client_get_list (
				client, key, GCONF_VALUE_STRING, NULL);

	for (l = panel_id_list; l; l = l->next)
		if (strcmp (panel->unique_id, l->data) == 0)
			break;
	
	if (!l) {
		panel_id_list = 
			g_slist_append (panel_id_list,
					g_strdup (panel->unique_id));	

		gconf_client_set_list (client, key, GCONF_VALUE_STRING,
				       panel_id_list, NULL);
	}

	panel_g_slist_deep_free (panel_id_list);

	panel_set_string (profile, panel->unique_id, "panel_type", 
			  gconf_enum_to_string (panel_type_type_enum_map, pd->type));

	panel_set_int (profile, panel->unique_id, "screen", screen);
	panel_set_int (profile, panel->unique_id, "monitor", monitor);

	if (basep) {
		panel_set_bool (profile, panel->unique_id,
				"hide_buttons_enabled",
				basep->hidebuttons_enabled);

		panel_set_bool (profile, panel->unique_id,
				"hide_button_pixmaps_enabled",
				basep->hidebutton_pixmaps_enabled);

		panel_set_int (profile, panel->unique_id,
			       "panel_hide_mode", basep->mode);

		panel_set_int (profile, panel->unique_id,
			       "panel_hide_state", basep->state);
	}

	panel_set_string (profile, panel->unique_id, "panel_size", 
			  gconf_enum_to_string (panel_size_type_enum_map, panel->sz));

	panel_set_bool (profile, panel->unique_id,
			"panel_background_pixmap_fit",
			panel->background.fit_image);

	panel_set_bool (profile, panel->unique_id,
			"panel_background_pixmap_stretch",
			panel->background.stretch_image);

	panel_set_bool (profile, panel->unique_id,
			"panel_background_pixmap_rotate",
			panel->background.rotate_image);

	panel_set_string (profile, panel->unique_id,
			  "panel_background_pixmap",
			  sure_string (panel->background.image));

	color = g_strdup_printf ("#%02x%02x%02x",
			 (guint) panel->background.color.gdk.red   / 256,
			 (guint) panel->background.color.gdk.green / 256,
			 (guint) panel->background.color.gdk.blue  / 256);
	
	panel_set_string (profile, panel->unique_id,
			  "panel_background_color", color);
	g_free (color);

	panel_set_int (profile, panel->unique_id,
		       "panel_background_color_alpha",
		       panel->background.color.alpha);

	panel_set_string (profile, panel->unique_id,
			  "panel_background_type", 
			  gconf_enum_to_string (background_type_enum_map,
			  panel->background.type));
	
	if (BORDER_IS_WIDGET (pd->panel))
		panel_set_string (profile, panel->unique_id,
				  "screen_edge", 
				  gconf_enum_to_string (panel_edge_type_enum_map,
							BORDER_POS (basep->pos)->edge));

	switch (pd->type) {
	case ALIGNED_PANEL:
		panel_set_string (profile, panel->unique_id,
				  "panel_align", 
				  gconf_enum_to_string (panel_alignment_type_enum_map,
							ALIGNED_POS (basep->pos)->align));
		break;
	case SLIDING_PANEL:
		panel_set_int (profile, panel->unique_id,
			       "panel_offset", SLIDING_POS (basep->pos)->offset);

		panel_set_string (profile, panel->unique_id,
				  "panel_anchor", 
				  gconf_enum_to_string (panel_anchor_type_enum_map,
							SLIDING_POS (basep->pos)->anchor));
		break;
	case FLOATING_PANEL:
		panel_set_string (profile, panel->unique_id,
				  "panel_orient", 
				  gconf_enum_to_string (panel_orientation_type_enum_map,
							PANEL_WIDGET (basep->panel)->orient));

		panel_set_int (profile, panel->unique_id,
			       "panel_x_position", FLOATING_POS (basep->pos)->x);

		panel_set_int (profile, panel->unique_id,
			       "panel_y_position", FLOATING_POS (basep->pos)->y);
		break;
	case DRAWER_PANEL:
		panel_set_string (profile, panel->unique_id,
				  "panel_orient", 
				  gconf_enum_to_string (panel_orient_type_enum_map,
							DRAWER_POS (basep->pos)->orient));
		break;
	default:
		break;
	}
}

void
panel_remove_from_gconf (PanelWidget *panel)
{
	const char *key;
	GSList     *new_panels = NULL;
	GSList     *panel_ids;
	GSList     *l;

	key = panel_gconf_general_key (panel_gconf_get_profile (), "panel_id_list");

	panel_ids = gconf_client_get_list (
			panel_gconf_get_client (), key, GCONF_VALUE_STRING, NULL);

	for (l = panel_ids; l; l = l->next) {
		char *id = l->data;
		l->data = NULL;
		if (strcmp (panel->unique_id, id) == 0) {
			char *dir;

			dir = g_strdup_printf ("/apps/panel/profiles/%s/panels/%s",
					       panel_gconf_get_profile (),
					       PANEL_WIDGET (panel)->unique_id);

			panel_gconf_clean_dir (panel_gconf_get_client (), dir);
			g_free (dir);
			g_free (id);
		} else {
			new_panels = g_slist_prepend (new_panels, id);
		}
        }

	g_slist_free (panel_ids);

        gconf_client_set_list (panel_gconf_get_client (), key,
			       GCONF_VALUE_STRING, new_panels, NULL);

	if (new_panels)
		panel_g_slist_deep_free (new_panels);
}
