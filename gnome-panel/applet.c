/* Gnome panel: general applet functionality
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Federico Mena
 *           Miguel de Icaza
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gdk/gdkx.h>

#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-show.h>

#include "button-widget.h"
#include "launcher.h"
#include "panel.h"
#include "panel-addto.h"
#include "panel-bindings.h"
#include "panel-applet-frame.h"
#include "panel-action-button.h"
#include "panel-menu-bar.h"
#include "panel-separator.h"
#include "panel-toplevel.h"
#include "panel-util.h"
#include "panel-menu-button.h"
#include "panel-globals.h"
#include "panel-properties-dialog.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-schemas.h"

static GSList *registered_applets = NULL;
static GSList *queued_position_saves = NULL;
static guint   queued_position_source = 0;

static GtkWidget *panel_applet_get_menu (AppletInfo *info);
static void applet_menu_show       (GtkWidget *w, AppletInfo *info);
static void applet_menu_deactivate (GtkWidget *w, AppletInfo *info);

static inline PanelWidget *
panel_applet_get_panel_widget (AppletInfo *info)
{
	return PANEL_WIDGET (gtk_widget_get_parent (info->widget));
}

static void
panel_applet_set_dnd_enabled (AppletInfo *info,
			      gboolean    dnd_enabled)
{
	switch (info->type) {
	case PANEL_OBJECT_MENU:
		panel_menu_button_set_dnd_enabled (PANEL_MENU_BUTTON (info->widget),
						   dnd_enabled);
		break;
	case PANEL_OBJECT_LAUNCHER:
		panel_launcher_set_dnd_enabled (info->data, dnd_enabled);
		break;
	case PANEL_OBJECT_APPLET:
		break;
	case PANEL_OBJECT_ACTION:
		panel_action_button_set_dnd_enabled (PANEL_ACTION_BUTTON (info->widget),
						     dnd_enabled);
		break;
	case PANEL_OBJECT_MENU_BAR:
	case PANEL_OBJECT_SEPARATOR:
	case PANEL_OBJECT_USER_MENU:
		break;
	default:
		g_assert_not_reached ();
		break;
	}

}

static void
move_applet_callback (GtkWidget *widget, AppletInfo *info)
{
	GtkWidget   *parent;
	PanelWidget *panel;

	g_return_if_fail (info != NULL);
	g_return_if_fail (info->widget != NULL);

	parent = gtk_widget_get_parent (info->widget);

	g_return_if_fail (parent != NULL);
	g_return_if_fail (PANEL_IS_WIDGET (parent));

	panel = PANEL_WIDGET (parent);

	panel_widget_applet_drag_start (panel, info->widget,
					GDK_CURRENT_TIME);
}

/* permanently remove an applet - all non-permanent
 * cleanups should go in panel_applet_destroy()
 */
void
panel_applet_clean (AppletInfo *info)
{
	g_return_if_fail (info != NULL);

	if (info->type == PANEL_OBJECT_LAUNCHER)
		panel_launcher_delete (info->data);

	if (info->widget) {
		GtkWidget *widget = info->widget;

		info->widget = NULL;
		gtk_widget_destroy (widget);
	}
}

static void
panel_applet_recreate_menu (AppletInfo *info)
{
	if (info->menu) {
		GList *l;

		for (l = info->user_menu; l; l = l->next) {
			AppletUserMenu *menu = l->data;

			menu->menuitem = NULL;
			menu->submenu = NULL;
		}

		if (gtk_widget_get_visible (info->menu))
			gtk_menu_shell_deactivate (GTK_MENU_SHELL (info->menu));

		g_signal_handlers_disconnect_by_func (info->menu,
						      G_CALLBACK (applet_menu_show), info);
		g_signal_handlers_disconnect_by_func (info->menu,
						      G_CALLBACK (applet_menu_deactivate), info);

		g_object_unref (info->menu);
		info->menu = NULL;
	}

	panel_applet_get_menu (info);
}

static void
applet_remove_callback (GtkWidget  *widget,
			AppletInfo *info)
{

	panel_layout_delete_object (info->id);
}

static inline GdkScreen *
applet_user_menu_get_screen (AppletUserMenu *menu)
{
	PanelWidget *panel_widget;

	panel_widget = panel_applet_get_panel_widget (menu->info);

	return gtk_window_get_screen (GTK_WINDOW (panel_widget->toplevel));
}

static void
applet_callback_callback (GtkWidget      *widget,
			  AppletUserMenu *menu)
{
	g_return_if_fail (menu->info != NULL);

	switch (menu->info->type) {
	case PANEL_OBJECT_LAUNCHER:
		if (!strcmp (menu->name, "launch"))
			launcher_launch (menu->info->data, widget);
		else if (!strcmp (menu->name, "properties"))
			launcher_properties (menu->info->data);
		break;
	case PANEL_OBJECT_MENU:
		panel_menu_button_invoke_menu (
			PANEL_MENU_BUTTON (menu->info->widget), menu->name);
		break;
	case PANEL_OBJECT_ACTION:
		panel_action_button_invoke_menu (
			PANEL_ACTION_BUTTON (menu->info->widget), menu->name);
		break;
	case PANEL_OBJECT_MENU_BAR:
		panel_menu_bar_invoke_menu (
			PANEL_MENU_BAR (menu->info->widget), menu->name);
		break;

	case PANEL_OBJECT_APPLET:
		/*
		 * Applet's menu's are handled differently
		 */
		break;
	case PANEL_OBJECT_SEPARATOR:
	case PANEL_OBJECT_USER_MENU:
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
applet_menu_show (GtkWidget *w,
		  AppletInfo *info)
{
	PanelWidget *panel_widget;

	panel_widget = panel_applet_get_panel_widget (info);

	panel_toplevel_push_autohide_disabler (panel_widget->toplevel);
}


static void
applet_menu_deactivate (GtkWidget *w,
			AppletInfo *info)
{
	PanelWidget *panel_widget;

	panel_widget = panel_applet_get_panel_widget (info);

	panel_toplevel_pop_autohide_disabler (panel_widget->toplevel);
}

AppletUserMenu *
panel_applet_get_callback (GList      *user_menu,
			   const char *name)
{
	GList *l;

	for (l = user_menu; l; l = l->next) {
		AppletUserMenu *menu = l->data;

		if (strcmp (menu->name, name) == 0)
			return menu;
	}

	return NULL;	
}

void
panel_applet_add_callback (AppletInfo          *info,
			   const char          *callback_name,
			   const char          *menuitem_text,
			   CallbackEnabledFunc  is_enabled_func)
{
	AppletUserMenu *menu;

	g_return_if_fail (info != NULL);
	g_return_if_fail (panel_applet_get_callback (info->user_menu,
						     callback_name) == NULL);

	menu                  = g_new0 (AppletUserMenu, 1);
	menu->name            = g_strdup (callback_name);
	menu->text            = g_strdup (menuitem_text);
	menu->is_enabled_func = is_enabled_func;
	menu->sensitive       = TRUE;
	menu->info            = info;
	menu->menuitem        = NULL;
	menu->submenu         = NULL;

	info->user_menu = g_list_append (info->user_menu, menu);

	panel_applet_recreate_menu (info);
}

static void
setup_an_item (AppletUserMenu *menu,
	       GtkWidget      *submenu,
	       int             is_submenu)
{
	menu->menuitem = gtk_menu_item_new_with_mnemonic (menu->text);
	gtk_widget_show (menu->menuitem);

	g_signal_connect (G_OBJECT (menu->menuitem), "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &menu->menuitem);

	if(submenu)
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menu->menuitem);

	/*if an item not a submenu*/
	if (!is_submenu) {
		g_signal_connect (menu->menuitem, "activate",
				  G_CALLBACK (applet_callback_callback),
				  menu);
		g_signal_connect (submenu, "destroy",
				  G_CALLBACK (gtk_widget_destroyed),
				  &menu->submenu);
	/* if the item is a submenu and doesn't have it's menu
	   created yet*/
	} else if (!menu->submenu) {
		menu->submenu = gtk_menu_new ();
	}

	if(menu->submenu) {
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu->menuitem),
					  menu->submenu);
		g_signal_connect (G_OBJECT (menu->submenu), "destroy",
				    G_CALLBACK (gtk_widget_destroyed),
				    &menu->submenu);
	}
	
	gtk_widget_set_sensitive(menu->menuitem,menu->sensitive);
}

static void
add_to_submenus (AppletInfo *info,
		 const char *path,
		 const char *name,
		 AppletUserMenu *menu,
		 GtkWidget *submenu,
		 GList *user_menu)
{
	char *n = g_strdup (name);
	char *p = strchr (n, '/');
	char *t;
	AppletUserMenu *s_menu;

	/*this is the last one*/
	if (p == NULL) {
		g_free (n);
		setup_an_item (menu, submenu, FALSE);
		return;
	}
	
	/*this is the last one and we are a submenu, we have already been
	  set up*/
	if(p==(n + strlen(n) - 1)) {
		g_free(n);
		return;
	}
	
	*p = '\0';
	p++;
	
	t = g_strconcat (path, n, "/", NULL);
	s_menu = panel_applet_get_callback (user_menu, t);
	/*the user did not give us this sub menu, whoops, will create an empty
	  one then*/
	if (s_menu == NULL) {
		s_menu = g_new0 (AppletUserMenu,1);
		s_menu->name = g_strdup (t);
		s_menu->text = g_strdup (_("???"));
		s_menu->sensitive = TRUE;
		s_menu->info = info;
		s_menu->menuitem = NULL;
		s_menu->submenu = NULL;
		info->user_menu = g_list_append (info->user_menu,s_menu);
		user_menu = info->user_menu;
	}
	
	if (s_menu->submenu == NULL) {
		s_menu->submenu = gtk_menu_new ();
		/*a more elegant way to do this should be done
		  when I don't want to go to sleep */
		if (s_menu->menuitem != NULL) {
			gtk_widget_destroy (s_menu->menuitem);
			s_menu->menuitem = NULL;
		}
	}
	if (s_menu->menuitem == NULL)
		setup_an_item (s_menu, submenu, TRUE);
	
	add_to_submenus (info, t, p, menu, s_menu->submenu, user_menu);
	
	g_free(t);
	g_free(n);
}

static GtkWidget *
panel_applet_create_bare_menu (AppletInfo *info)
{
	GtkWidget *menu;

	menu = g_object_ref_sink (gtk_menu_new ());

	/* connect the show & deactivate signal, so that we can "disallow" and
	 * "re-allow" autohide when the menu is shown/deactivated.
	 */
	g_signal_connect (menu, "show",
			  G_CALLBACK (applet_menu_show), info);
	g_signal_connect (menu, "deactivate",
			  G_CALLBACK (applet_menu_deactivate), info);

	return menu;
}

static void
panel_applet_menu_lockdown_changed (PanelLockdown *lockdown,
				    gpointer       user_data)
{
	AppletInfo *info = user_data;

	panel_applet_recreate_menu (info);
}

static GtkWidget *
panel_applet_get_menu (AppletInfo *info)
{
	GtkWidget   *menu;
	GList       *l;
	gboolean     added_anything = FALSE;

	if (info->menu)
		return info->menu;

	menu = panel_applet_create_bare_menu (info);

	for (l = info->user_menu; l; l = l->next) {
		AppletUserMenu *user_menu = l->data;

		if (user_menu->is_enabled_func && !user_menu->is_enabled_func ())
			continue;

		add_to_submenus (info, "", user_menu->name, user_menu, 
				 menu, info->user_menu);

		added_anything = TRUE;
	}

	if ( ! added_anything) {
                g_signal_handlers_disconnect_by_func (menu,
						      G_CALLBACK (applet_menu_show), info);
                g_signal_handlers_disconnect_by_func (menu,
						      G_CALLBACK (applet_menu_deactivate), info);

		g_object_unref (menu);
		return NULL;
	}

	info->menu = menu;

	panel_lockdown_on_notify (panel_lockdown_get (),
				  NULL,
				  G_OBJECT (info->menu),
				  panel_applet_menu_lockdown_changed,
				  info);

	return info->menu;
}

static void
panel_applet_edit_menu_lockdown_changed (PanelLockdown *lockdown,
					 gpointer       user_data)
{
	AppletInfo *info = user_data;

	if (!panel_lockdown_get_panels_locked_down (lockdown))
		return;

	if (info->edit_menu) {
		if (gtk_widget_get_visible (info->edit_menu))
			gtk_menu_shell_deactivate (GTK_MENU_SHELL (info->edit_menu));

                g_signal_handlers_disconnect_by_func (info->edit_menu,
						      G_CALLBACK (applet_menu_deactivate), info);
		gtk_widget_destroy (info->edit_menu);
		info->edit_menu = NULL;
	}
}

static GtkWidget *
panel_applet_get_edit_menu (AppletInfo *info)
{
	GtkWidget   *menu;
	GtkWidget   *menuitem;
	gboolean     movable;
	gboolean     removable;

	if (info->edit_menu)
		return info->edit_menu;

	if (panel_lockdown_get_panels_locked_down_s ())
		return NULL;

	menu = panel_applet_create_bare_menu (info);

	movable = panel_applet_can_freely_move (info);
	removable = panel_layout_is_writable ();

	menuitem = gtk_menu_item_new_with_mnemonic (_("_Move"));
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (move_applet_callback), info);
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	gtk_widget_set_sensitive (menuitem, movable);

	menuitem = gtk_menu_item_new_with_mnemonic (_("_Remove From Panel"));
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (applet_remove_callback), info);
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	gtk_widget_set_sensitive (menuitem, removable);

	info->edit_menu = menu;

	panel_lockdown_on_notify (panel_lockdown_get (),
				  "panels-locked-down",
				  G_OBJECT (info->edit_menu),
				  panel_applet_edit_menu_lockdown_changed,
				  info);

	return info->edit_menu;
}


void
panel_applet_menu_set_recurse (GtkMenu     *menu,
			       const gchar *key,
			       gpointer     data)
{
	GList *children;
	GList *l;

	g_object_set_data (G_OBJECT (menu), key, data);

	children = gtk_container_get_children (GTK_CONTAINER (menu));

	for (l = children; l; l = l->next) {
		GtkWidget *submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (l->data));

		if (submenu)
			panel_applet_menu_set_recurse (
				GTK_MENU (submenu), key, data);
	}

	g_list_free (children);
}

void
panel_applet_position_menu (GtkMenu   *menu,
			    int       *x,
			    int       *y,
			    gboolean  *push_in,
			    GtkWidget *applet)
{
	GtkAllocation   allocation;
	GtkRequisition  requisition;
	GdkDevice      *device;
	GdkScreen      *screen;
	GtkWidget      *parent;
	int             menu_x = 0;
	int             menu_y = 0;
	int             pointer_x;
	int             pointer_y;

	parent = gtk_widget_get_parent (applet);

	g_return_if_fail (PANEL_IS_WIDGET (parent));

	screen = gtk_widget_get_screen (applet);

	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &requisition, NULL);

	gdk_window_get_origin (gtk_widget_get_window (applet), &menu_x, &menu_y);
	device = gdk_device_manager_get_client_pointer (gdk_display_get_device_manager (gtk_widget_get_display (applet)));
	gdk_window_get_device_position(gtk_widget_get_window (applet), device, &pointer_x, &pointer_y, NULL);

	gtk_widget_get_allocation (applet, &allocation);

	if (!gtk_widget_get_has_window (applet)) {
		menu_x += allocation.x;
		menu_y += allocation.y;
	}

	if (PANEL_WIDGET (parent)->orient == GTK_ORIENTATION_HORIZONTAL) {
		if (gtk_widget_get_direction (GTK_WIDGET (menu)) != GTK_TEXT_DIR_RTL) {
			if (pointer_x < allocation.width &&
			    requisition.width < pointer_x)
				menu_x += MIN (pointer_x,
					       allocation.width - requisition.width);
		} else {
			menu_x += allocation.width - requisition.width;
			if (pointer_x > 0 && pointer_x < allocation.width &&
			    pointer_x < allocation.width - requisition.width) {
				menu_x -= MIN (allocation.width - pointer_x,
					       allocation.width - requisition.width);
			}
		}
		menu_x = MIN (menu_x, gdk_screen_get_width (screen) - requisition.width);

		if (menu_y > gdk_screen_get_height (screen) / 2)
			menu_y -= requisition.height;
		else
			menu_y += allocation.height;
	} else {
		if (pointer_y < allocation.height &&
		    requisition.height < pointer_y)
			menu_y += MIN (pointer_y, allocation.height - requisition.height);
		menu_y = MIN (menu_y, gdk_screen_get_height (screen) - requisition.height);

		if (menu_x > gdk_screen_get_width (screen) / 2)
			menu_x -= requisition.width;
		else
			menu_x += allocation.width;
	}

	*x = menu_x;
	*y = menu_y;
	*push_in = FALSE;
}

static void
applet_show_menu (AppletInfo     *info,
		  GtkWidget      *menu,
		  gboolean        custom_position,
		  GdkEventButton *event)
{
	PanelWidget *panel_widget;

	g_return_if_fail (info != NULL);

	if (menu == NULL)
		return;

	panel_widget = panel_applet_get_panel_widget (info);

	panel_applet_menu_set_recurse (GTK_MENU (menu),
				       "menu_panel",
				       panel_widget);

	gtk_menu_set_screen (GTK_MENU (menu),
			     gtk_window_get_screen (GTK_WINDOW (panel_widget->toplevel)));

	if (!gtk_widget_get_realized (menu))
		gtk_widget_show (menu);

	gtk_menu_popup (GTK_MENU (menu),
			NULL,
			NULL,
			custom_position ?
				(GtkMenuPositionFunc) panel_applet_position_menu : NULL,
			info->widget,
			event->button,
			event->time);
}

static gboolean
applet_must_skip_menu (AppletInfo *info)
{
	if (panel_applet_is_in_drag ())
		return TRUE;

	if (info->type == PANEL_OBJECT_APPLET)
		return TRUE;

	return FALSE;
}

static gboolean
applet_button_press (GtkWidget      *widget,
		     GdkEventButton *event,
		     AppletInfo     *info)
{
	guint modifiers;

	if (event->button != 3)
		return FALSE;

	if (applet_must_skip_menu (info))
		return FALSE;

	modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

	if (modifiers == panel_bindings_get_mouse_button_modifier_keymask ())
		applet_show_menu (info, panel_applet_get_edit_menu (info), FALSE, event);
	else
		applet_show_menu (info, panel_applet_get_menu (info), TRUE, event);

	return TRUE;
}

static gboolean
applet_key_press (GtkWidget   *widget,
		  GdkEventKey *event,
		  AppletInfo  *info)
{
	GdkEventButton eventbutton;
	gboolean is_popup = FALSE;
	gboolean is_edit_popup = FALSE;

	if (applet_must_skip_menu (info))
		return FALSE;

	eventbutton.button = 3;
	eventbutton.time = event->time;

	/* We're not connecting to the popup-menu signal since we want to be
	 * able to deal with two cases:
	 *  - exact keybinding of popup-menu => we open the context menu
	 *  - keybinding of popup-menu + modifier from metacity => we open menu
	 *    to "edit"
	 */
	panel_util_key_event_is_popup (event, &is_popup, &is_edit_popup);

	if (is_edit_popup)
		applet_show_menu (info, panel_applet_get_edit_menu (info), FALSE, &eventbutton);
	else if (is_popup)
		applet_show_menu (info, panel_applet_get_menu (info), TRUE, &eventbutton);

	return (is_popup || is_edit_popup);
}

static void
panel_applet_destroy (GtkWidget  *widget,
		      AppletInfo *info)
{
	GList *l;

	g_return_if_fail (info != NULL);

	info->widget = NULL;

	registered_applets = g_slist_remove (registered_applets, info);

	queued_position_saves =
		g_slist_remove (queued_position_saves, info);

	if (info->menu) {
		if (gtk_widget_get_visible (info->menu))
			gtk_menu_shell_deactivate (GTK_MENU_SHELL (info->menu));

                g_signal_handlers_disconnect_by_func (info->menu,
						      G_CALLBACK (applet_menu_show), info);
                g_signal_handlers_disconnect_by_func (info->menu,
						      G_CALLBACK (applet_menu_deactivate), info);
		g_object_unref (info->menu);
        }
	info->menu = NULL;

	if (info->edit_menu) {
		if (gtk_widget_get_visible (info->edit_menu))
			gtk_menu_shell_deactivate (GTK_MENU_SHELL (info->edit_menu));

                g_signal_handlers_disconnect_by_func (info->edit_menu,
						      G_CALLBACK (applet_menu_show), info);
                g_signal_handlers_disconnect_by_func (info->edit_menu,
						      G_CALLBACK (applet_menu_deactivate), info);
		g_object_unref (info->edit_menu);
        }
	info->edit_menu = NULL;

	if (info->data_destroy)
		info->data_destroy (info->data);
	info->data = NULL;

	for (l = info->user_menu; l != NULL; l = l->next) {
		AppletUserMenu *umenu = l->data;

		g_free (umenu->name);
		g_free (umenu->text);

		g_free (umenu);
	}

	g_list_free (info->user_menu);
	info->user_menu = NULL;

	if (info->settings)
		g_object_unref (info->settings);
	info->settings = NULL;

	g_free (info->id);
	info->id = NULL;

	g_free (info);
}

static const char *
panel_applet_get_toplevel_id (AppletInfo *applet)
{
	PanelWidget *panel_widget;

	g_return_val_if_fail (applet != NULL, NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (applet->widget), NULL);

	panel_widget = panel_applet_get_panel_widget (applet);
	if (!panel_widget)
		return NULL;

	return panel_toplevel_get_id (panel_widget->toplevel);
}

static gboolean
panel_applet_position_save_timeout (gpointer dummy)
{
	GSList *l;

	queued_position_source = 0;

	for (l = queued_position_saves; l; l = l->next) {
		AppletInfo *info = l->data;

		panel_applet_save_position (info, info->id, TRUE);
	}

	g_slist_free (queued_position_saves);
	queued_position_saves = NULL;

	return FALSE;
}

void
panel_applet_save_position (AppletInfo *applet_info,
			    const char *id,
			    gboolean    immediate)
{
	const char  *toplevel_id;
	AppletData  *applet_data;

	g_return_if_fail (applet_info != NULL);
	g_return_if_fail (G_IS_OBJECT (applet_info->widget));

	if (!immediate) {
		if (!queued_position_source)
			queued_position_source =
				g_timeout_add_seconds (1,
						       (GSourceFunc) panel_applet_position_save_timeout,
						       NULL);

		if (!g_slist_find (queued_position_saves, applet_info))
			queued_position_saves =
				g_slist_prepend (queued_position_saves, applet_info);

		return;
	}

	if (!(toplevel_id = panel_applet_get_toplevel_id (applet_info)))
		return;

	applet_data = g_object_get_data (G_OBJECT (applet_info->widget),
					 PANEL_APPLET_DATA);

	g_settings_set_string (applet_info->settings,
			       PANEL_OBJECT_TOPLEVEL_ID_KEY,
			       toplevel_id);
	g_settings_set_enum (applet_info->settings,
			     PANEL_OBJECT_PACK_TYPE_KEY,
			     applet_data->pack_type);
	g_settings_set_int (applet_info->settings,
			    PANEL_OBJECT_PACK_INDEX_KEY,
			    applet_data->pack_index);
}

const char *
panel_applet_get_id (AppletInfo *info)
{
	if (!info)
		return NULL;

	return info->id;
}

const char *
panel_applet_get_id_by_widget (GtkWidget *applet_widget)
{
	GSList *l;

	if (!applet_widget)
		return NULL;

	for (l = registered_applets; l; l = l->next) {
		AppletInfo *info = l->data;

		if (info->widget == applet_widget)
			return info->id;
	}

	return NULL;
}

AppletInfo *
panel_applet_get_by_id (const char *id)
{
	GSList *l;

	for (l = registered_applets; l; l = l->next) {
		AppletInfo *info = l->data;

		if (!strcmp (info->id, id))
			return info;
	}

	return NULL;
}

GSList *
panel_applet_list_applets (void)
{
	return registered_applets;
}

AppletInfo *
panel_applet_get_by_type (PanelObjectType object_type, GdkScreen *screen)
{
	GSList *l;

	for (l = registered_applets; l; l = l->next) {
		AppletInfo *info = l->data;

		if (info->type == object_type) {
			if (screen) {
				if (screen == gtk_widget_get_screen (info->widget))
					return info;
			} else
				return info;
		}
	}

	return NULL;
}

AppletInfo *
panel_applet_register (GtkWidget       *applet,
		       PanelWidget     *panel,
		       PanelObjectType  type,
		       const char      *id,
		       GSettings       *settings,
		       gpointer         data,
		       GDestroyNotify   data_destroy)
{
	AppletInfo          *info;
	PanelObjectPackType  pack_type;
	int                  pack_index;
	
	g_return_val_if_fail (applet != NULL && panel != NULL, NULL);

	if (gtk_widget_get_has_window (applet))
		gtk_widget_set_events (applet, (gtk_widget_get_events (applet) |
						APPLET_EVENT_MASK) &
				       ~( GDK_POINTER_MOTION_MASK |
					  GDK_POINTER_MOTION_HINT_MASK));

	info = g_new0 (AppletInfo, 1);
	info->type         = type;
	info->widget       = applet;
	info->settings     = g_object_ref (settings);
	info->menu         = NULL;
	info->edit_menu    = NULL;
	info->data         = data;
	info->data_destroy = data_destroy;
	info->user_menu    = NULL;
	info->id           = g_strdup (id);

	g_object_set_data (G_OBJECT (applet), "applet_info", info);

	registered_applets = g_slist_append (registered_applets, info);

	/* Find where to insert the applet */
        pack_type = g_settings_get_enum (info->settings, PANEL_OBJECT_PACK_TYPE_KEY);
        pack_index = g_settings_get_int (info->settings, PANEL_OBJECT_PACK_INDEX_KEY);

	/* Insert it */
	panel_widget_add (panel, applet, pack_type, pack_index, TRUE);

	if (BUTTON_IS_WIDGET (applet) ||
	    gtk_widget_get_has_window (applet)) {
		g_signal_connect (applet, "button_press_event",
				  G_CALLBACK (applet_button_press),
				  info);

		g_signal_connect (applet, "key_press_event",
				  G_CALLBACK (applet_key_press),
				  info);
	}

	g_signal_connect (applet, "destroy",
			  G_CALLBACK (panel_applet_destroy),
			  info);

	panel_applet_set_dnd_enabled (info, TRUE);

	gtk_widget_show (applet);

	orientation_change (info, panel);
	back_change (info, panel);

	if (type != PANEL_OBJECT_APPLET)
		gtk_widget_grab_focus (applet);
	else
		gtk_widget_child_focus (applet, GTK_DIR_TAB_FORWARD);

	return info;
}

GSettings *
panel_applet_get_settings (AppletInfo *applet)
{
	g_return_val_if_fail (applet != NULL, NULL);

	return applet->settings;
}

gboolean
panel_applet_can_freely_move (AppletInfo *applet)
{
	/* if we check for more lockdown than this, then we'll need to update
	 * callers that use panel_lockdown_on_notify() */
	if (panel_lockdown_get_panels_locked_down_s ())
		return FALSE;

	return (g_settings_is_writable (applet->settings,
					PANEL_OBJECT_TOPLEVEL_ID_KEY) &&
	        g_settings_is_writable (applet->settings,
					PANEL_OBJECT_PACK_TYPE_KEY) &&
	        g_settings_is_writable (applet->settings,
					PANEL_OBJECT_PACK_INDEX_KEY));
}
