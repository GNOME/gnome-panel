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
#include <libgnome/libgnome.h>
#include <libbonobo.h>
#include <gdk/gdkx.h>

#include "button-widget.h"
#include "drawer.h"
#include "launcher.h"
#include "menu-util.h"
#include "menu.h"
#include "panel-config.h"
#include "panel-gconf.h"
#include "panel-config-global.h"
#include "panel-applet-frame.h"
#include "egg-screen-exec.h"
#include "panel-action-button.h"
#include "panel-menu-bar.h"
#include "panel-compatibility.h"
#include "panel-toplevel.h"
#include "panel-util.h"
#include "panel-profile.h"
#include "panel-menu-button.h"
#include "panel-globals.h"

#define SMALL_ICON_SIZE 20

static int applet_count = 0;

static GConfEnumStringPair object_type_enum_map [] = {
	{ APPLET_DRAWER,   "drawer-object" },
	{ APPLET_MENU,     "menu-object" },
	{ APPLET_LAUNCHER, "launcher-object" }, 
	{ APPLET_BONOBO,   "bonobo-applet" },
	{ APPLET_ACTION,   "action-applet" },
	{ APPLET_MENU_BAR, "menu-bar" },
	{ APPLET_LOCK,     "lock-object" },   /* FIXME:                           */
	{ APPLET_LOGOUT,   "logout-object" }, /*   Both only for backwards compat */
};

static GSList *queued_position_saves = NULL;
static guint   queued_position_source = 0;

static void
move_applet_callback (GtkWidget *widget, AppletInfo *info)
{
	PanelWidget *panel;

	g_return_if_fail (info != NULL);
	g_return_if_fail (info->widget != NULL);
	g_return_if_fail (info->widget->parent != NULL);
	g_return_if_fail (PANEL_IS_WIDGET (info->widget->parent));

	panel = PANEL_WIDGET (info->widget->parent);

	panel_widget_applet_drag_start (panel, info->widget,
					PW_DRAG_OFF_CENTER);
}

void
panel_applet_clean_gconf (AppletType  type,
			  const char *id,
			  gboolean    clean_gconf)
{
	GConfClient *client;
	GSList      *id_list, *l;
	const char  *key;
	const char  *profile;

	g_return_if_fail (id != NULL);

	client  = gconf_client_get_default ();
	profile = panel_profile_get_name ();

	key = panel_gconf_general_key (profile,
				       type == APPLET_BONOBO ? "applet_id_list" : "object_id_list");

        id_list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

        for (l = id_list; l; l = l->next)
                if (!strcmp (id, (char *) l->data))
                        break;

        if (l) {
		g_free (l->data);
		l->data = NULL;
                id_list = g_slist_delete_link (id_list, l);

                gconf_client_set_list (client, key, GCONF_VALUE_STRING, id_list, NULL);

		key = panel_gconf_sprintf ("/apps/panel/profiles/%s/%s/%s",
					   profile,
					   type == APPLET_BONOBO ? "applets" : "objects",
					   id);

		if (clean_gconf)
			panel_gconf_clean_dir (client, key);
        }

	for (l = id_list; l; l = l->next)
		g_free (l->data);
	g_slist_free (id_list);

	g_object_unref (client);
}

/* permanently remove an applet - all non-permanent
 * cleanups should go in panel_applet_destroy()
 */
void
panel_applet_clean (AppletInfo *info,
		    gboolean    clean_gconf)
{
	g_return_if_fail (info != NULL);

	if (info->type == APPLET_LAUNCHER) {
		Launcher   *launcher = info->data;
		const char *location;

		location = gnome_desktop_item_get_location (launcher->ditem);

		if (location)
			 unlink (location);
	}

	panel_applet_clean_gconf (info->type, info->id, clean_gconf);

	if (info->widget) {
		GtkWidget *widget = info->widget;

		info->widget = NULL;
		gtk_widget_destroy (widget);
	}
}

static gboolean
applet_idle_remove (gpointer data)
{
	AppletInfo *info = data;

	info->remove_idle = 0;

	panel_applet_clean (info, TRUE);

	return FALSE;
}

void
panel_applet_remove_in_idle (AppletInfo *info)
{
	if (info->remove_idle == 0)
		info->remove_idle = g_idle_add (applet_idle_remove, info);
}

static void
applet_remove_callback (GtkWidget  *widget,
			AppletInfo *info)
{
	panel_applet_remove_in_idle (info);
}

static inline GdkScreen *
applet_user_menu_get_screen (AppletUserMenu *menu)
{
	PanelWidget *panel_widget;

	panel_widget = PANEL_WIDGET (menu->info->widget->parent);

	return gtk_window_get_screen (GTK_WINDOW (panel_widget->toplevel));
}

static void
applet_callback_callback (GtkWidget      *widget,
			  AppletUserMenu *menu)
{
	GdkScreen *screen;

	g_return_if_fail (menu->info != NULL);

	screen = applet_user_menu_get_screen (menu);

	switch (menu->info->type) {
	case APPLET_LAUNCHER:
		if (!strcmp (menu->name, "properties"))
			launcher_properties (
				menu->info->data, screen);

		else if (!strcmp (menu->name, "help"))
			panel_show_help (screen, "wgospanel.xml", "gospanel-16");

		else if (!strcmp (menu->name, "help_on_app"))
			launcher_show_help (menu->info->data, screen);
		break;
	case APPLET_DRAWER: 
		if (strcmp (menu->name, "properties")==0) {
			Drawer *drawer = menu->info->data;
			g_assert(drawer);
#ifdef FIXME_FOR_NEW_CONFIG
			panel_config (drawer->toplevel);
#endif
		} else if (strcmp (menu->name, "help") == 0) {
			panel_show_help (screen, "wgospanel.xml", "gospanel-18");
		}
		break;
	case APPLET_MENU:
		panel_menu_button_invoke_menu (
			PANEL_MENU_BUTTON (menu->info->widget), menu->name);
		break;
	case APPLET_ACTION:
	case APPLET_LOGOUT:
	case APPLET_LOCK:
		panel_action_button_invoke_menu (
			PANEL_ACTION_BUTTON (menu->info->widget), menu->name);
		break;
	case APPLET_MENU_BAR:
		panel_menu_bar_invoke_menu (
			PANEL_MENU_BAR (menu->info->widget), menu->name);
		break;

	case APPLET_BONOBO:
		/*
		 * Applet's menu's are handled differently
		 */
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
applet_menu_deactivate (GtkWidget *w, AppletInfo *info)
{
	PanelWidget *panel_widget;

	panel_widget = PANEL_WIDGET (info->widget->parent);

	panel_toplevel_unblock_auto_hide (panel_widget->toplevel);
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
panel_applet_add_callback (AppletInfo *info,
			   const char *callback_name,
			   const char *stock_item,
			   const char *menuitem_text)
{
	AppletUserMenu *menu;

	g_return_if_fail (info != NULL);

	menu = panel_applet_get_callback (info->user_menu, callback_name);
	if (menu == NULL) {
		menu = g_new0 (AppletUserMenu, 1);
		menu->name = g_strdup (callback_name);
		menu->stock_item = g_strdup (stock_item);
		menu->text = g_strdup (menuitem_text);
		menu->sensitive = TRUE;
		menu->info = info;
		menu->menuitem = NULL;
		menu->submenu = NULL;
		info->user_menu = g_list_append (info->user_menu, menu);
	} else {
		g_free (menu->stock_item);
		menu->stock_item = NULL;
		g_free (menu->text);
		menu->text = NULL;
		menu->text = g_strdup (menuitem_text);
		menu->stock_item = g_strdup (stock_item);
	}

	/*make sure the menu is rebuilt*/
	if(info->menu) {
		GList *list;
		for(list=info->user_menu;list!=NULL;list=g_list_next(list)) {
			AppletUserMenu *menu = list->data;
			menu->menuitem=NULL;
			menu->submenu=NULL;
		}
		gtk_widget_unref(info->menu);
		info->menu = NULL;
	}
}

void
panel_applet_remove_callback (AppletInfo *info,
			      const char *callback_name)
{
	AppletUserMenu *menu;

	g_return_if_fail (info != NULL);
	
	menu = panel_applet_get_callback (info->user_menu, callback_name);
	if (menu != NULL) {
		info->user_menu = g_list_remove (info->user_menu, menu);
		g_free(menu->name);
		menu->name = NULL;
		g_free(menu->stock_item);
		menu->stock_item = NULL;
		g_free(menu->text);
		menu->text = NULL;
		g_free(menu);
	} else
		return; /*it just isn't there*/

	/*make sure the menu is rebuilt*/
	if (info->menu != NULL) {
		GList *list;
		for (list = info->user_menu; list != NULL; list = list->next) {
			AppletUserMenu *menu = list->data;
			menu->menuitem = NULL;
			menu->submenu = NULL;
		}
		gtk_widget_unref (info->menu);
		info->menu = NULL;
	}
}

static void
setup_an_item (AppletUserMenu *menu,
	       GtkWidget      *submenu,
	       int             is_submenu)
{
	GtkWidget *image = NULL;

	menu->menuitem = gtk_image_menu_item_new ();

	g_signal_connect (G_OBJECT (menu->menuitem), "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &menu->menuitem);

	if (menu->stock_item && menu->stock_item [0])
		image = gtk_image_new_from_stock (menu->stock_item, GTK_ICON_SIZE_MENU);

	setup_menuitem (menu->menuitem, GTK_ICON_SIZE_MENU, image, menu->text);

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
		menu->submenu = panel_menu_new ();
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
		s_menu->stock_item = NULL;
		s_menu->text = g_strdup (_("???"));
		s_menu->sensitive = TRUE;
		s_menu->info = info;
		s_menu->menuitem = NULL;
		s_menu->submenu = NULL;
		info->user_menu = g_list_append (info->user_menu,s_menu);
		user_menu = info->user_menu;
	}
	
	if (s_menu->submenu == NULL) {
		s_menu->submenu = panel_menu_new ();
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
panel_applet_create_menu (AppletInfo *info)
{
	GtkWidget *menu;
	GtkWidget *menuitem;
	GList     *l;

	menu = g_object_ref (panel_menu_new ());
	gtk_object_sink (GTK_OBJECT (menu));

	/* connect the deactivate signal, so that we can "re-allow" 
	 * autohide when the menu is deactivated.
	 */
	g_signal_connect (menu, "deactivate",
			  G_CALLBACK (applet_menu_deactivate), info);

	if (info->user_menu) {
		for (l = info->user_menu; l; l = l->next) {
			AppletUserMenu *user_menu = (AppletUserMenu *)l->data;

			add_to_submenus (info, "", user_menu->name, user_menu, 
					 menu, info->user_menu);
		}
	}

	if (!commie_mode) {
		GtkWidget *image;

		menuitem = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
		gtk_widget_show (menuitem);

		menuitem = gtk_image_menu_item_new ();

		image = gtk_image_new_from_stock (GTK_STOCK_REMOVE,
						  GTK_ICON_SIZE_MENU);

		setup_menuitem (menuitem, GTK_ICON_SIZE_MENU, image , _("_Remove From Panel"));

		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (applet_remove_callback), info);

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

		menuitem = gtk_image_menu_item_new ();

		/* FIXME: should have a "Move" pixmap.
		 */
		image = gtk_image_new ();
		setup_menuitem (menuitem, GTK_ICON_SIZE_MENU, image, _("_Move"));

		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (move_applet_callback), info);

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	}

	return menu;
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
		GtkWidget *submenu = GTK_MENU_ITEM (l->data)->submenu;

		if (submenu)
			panel_applet_menu_set_recurse (
				GTK_MENU (submenu), key, data);
	}

	g_list_free (children);
}

static void
applet_show_menu (AppletInfo     *info,
		  GdkEventButton *event)
{
	PanelWidget *panel_widget;

	g_return_if_fail (info != NULL);

	panel_widget = PANEL_WIDGET (info->widget->parent);

	if (!info->menu)
		info->menu = panel_applet_create_menu (info);

	g_assert (info->menu);

	panel_toplevel_block_auto_hide (panel_widget->toplevel);

	panel_applet_menu_set_recurse (GTK_MENU (info->menu),
				       "menu_panel",
				       info->widget->parent);

	gtk_menu_set_screen (GTK_MENU (info->menu),
			     gtk_window_get_screen (GTK_WINDOW (panel_widget->toplevel)));

	if (!GTK_WIDGET_REALIZED (info->menu))
		gtk_widget_show (info->menu);

	gtk_menu_popup (GTK_MENU (info->menu),
			NULL,
			NULL,
			(GtkMenuPositionFunc) panel_position_applet_menu,
			info->widget,
			event->button,
			event->time);
}

static gboolean
applet_do_popup_menu (GtkWidget      *widget,
		      GdkEventButton *event,
		      AppletInfo     *info)
{
	if (panel_applet_is_in_drag ())
		return FALSE;

	if (info->type == APPLET_BONOBO)
		return FALSE;

	applet_show_menu (info, event);

	return TRUE;
}

static gboolean
applet_popup_menu (GtkWidget      *widget,
		   AppletInfo     *info)
{
	GdkEventButton event;

	event.button = 3;
	event.time = GDK_CURRENT_TIME;

	return applet_do_popup_menu (widget, &event, info);
}

static gboolean
applet_button_press (GtkWidget      *widget,
		     GdkEventButton *event,
		     AppletInfo     *info)
{
	if (event->button == 3)
		return applet_do_popup_menu (widget, event, info);

	return FALSE;
}

static void
panel_applet_destroy (GtkWidget  *widget,
		      AppletInfo *info)
{
	GList *l;

	g_return_if_fail (info != NULL);

	info->widget = NULL;

	applets = g_slist_remove (applets, info);

	queued_position_saves =
		g_slist_remove (queued_position_saves, info);

	if (info->remove_idle) {
		g_source_remove (info->remove_idle);
		info->remove_idle = 0;
	}

	if (info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;

		if (drawer->toplevel) {
			PanelWidget *panel_widget;

			panel_widget = panel_toplevel_get_panel_widget (
							drawer->toplevel);
			panel_widget->master_widget = NULL;

			gtk_widget_destroy (GTK_WIDGET (drawer->toplevel));
			drawer->toplevel = NULL;
		}
	}

	if (info->menu)
		gtk_widget_unref (info->menu);
	info->menu = NULL;

	if (info->data_destroy)
		info->data_destroy (info->data);
	info->data = NULL;

	for (l = info->user_menu; l != NULL; l = l->next) {
		AppletUserMenu *umenu = l->data;

		g_free (umenu->name);
		g_free (umenu->stock_item);
		g_free (umenu->text);

		g_free (umenu);
	}

	g_list_free (info->user_menu);
	info->user_menu = NULL;

	g_free (info->id);
	info->id = NULL;

	g_free (info);
}

typedef struct {
	AppletType   type;
	PanelWidget *panel_widget;
	int          position;
	char        *id;
	gulong       destroy_handler;
} PanelAppletToLoad;

static GSList *panel_applets_to_load = NULL;

static void
free_applet_to_load (PanelAppletToLoad *applet)
{
	if (applet->destroy_handler)
		g_signal_handler_disconnect (applet->panel_widget,
					     applet->destroy_handler);
	applet->destroy_handler = 0;

	g_free (applet->id);
	applet->id = NULL;

	g_free (applet);
}

static gboolean
panel_applet_load_idle_handler (gpointer dummy)
{
	PanelAppletToLoad *applet;

	if (!panel_applets_to_load) {
		panel_compatibility_load_applets ();
		return FALSE;
	}

	applet = (PanelAppletToLoad *) panel_applets_to_load->data;
	panel_applets_to_load->data = NULL;
	panel_applets_to_load =
		g_slist_delete_link (panel_applets_to_load,
				     panel_applets_to_load);

	switch (applet->type) {
	case APPLET_BONOBO:
		panel_applet_frame_load_from_gconf (
					applet->panel_widget,
					applet->position,
					applet->id);
		break;
	case APPLET_DRAWER:
		drawer_load_from_gconf (applet->panel_widget,
					applet->position,
					applet->id);
		break;
	case APPLET_MENU:
		panel_menu_button_load_from_gconf (applet->panel_widget,
						   applet->position,
						   TRUE,
						   applet->id);
		break;
	case APPLET_LAUNCHER:
		launcher_load_from_gconf (applet->panel_widget,
					  applet->position,
					  applet->id);
		break;
	case APPLET_LOGOUT:  /* FIXME: This is backward compatibility only. */
	case APPLET_LOCK:    /*        Remove at some time in the future    */
		panel_action_button_load (
				applet->type == APPLET_LOGOUT ? PANEL_ACTION_LOGOUT :
								PANEL_ACTION_LOCK,
				applet->panel_widget,
				applet->position,
				TRUE,
				applet->id,
				TRUE);
		break;
	case APPLET_ACTION:
		panel_action_button_load_from_gconf (
				applet->panel_widget,
				applet->position,
				TRUE,
				applet->id);
		break;
	case APPLET_MENU_BAR:
		panel_menu_bar_load_from_gconf (
				applet->panel_widget,
				applet->position,
				TRUE,
				applet->id);
		break;
	default:
		break;
	}

	free_applet_to_load (applet);

	return TRUE;
}

static void
panel_destroyed_while_loading (GtkWidget *panel, gpointer data)
{
	PanelAppletToLoad *applet = data;

	applet->destroy_handler = 0;

	panel_applets_to_load =
		g_slist_remove (panel_applets_to_load, applet);

	free_applet_to_load (applet);
}

static void
panel_applet_load_with_id (PanelGConfKeyType  type,
			   GConfClient       *client,
			   const char        *profile,
			   const char        *id)
{
	PanelAppletToLoad *applet;
	PanelToplevel     *toplevel;
	PanelWidget       *panel_widget;
	int                applet_type = 0;
	const char        *key;
	char              *type_string;
	char              *toplevel_id;
	int                position;
	gboolean           right_stick;

	key = panel_gconf_full_key (type, profile, id, "object_type");
	type_string = gconf_client_get_string (client, key, NULL);
	if (!type_string) {
		g_printerr (_("No object_type set for panel object with ID %s\n"), id);
		return;
	}
        
	if (!gconf_string_to_enum (object_type_enum_map,
				   type_string,
				   &applet_type)) {
		g_free (type_string);
		g_warning ("Unkown applet type %s from %s", type_string, key);
		return;
	}
	
	g_free (type_string);

	key = panel_gconf_full_key (type, profile, id, "position");
	position = gconf_client_get_int (client, key, NULL);
	
	key = panel_gconf_full_key (type, profile, id, "panel_id");
	toplevel_id = gconf_client_get_string (client, key, NULL);
	if (!toplevel_id) {
		g_printerr (_("No panel_id set for panel object with ID %s\n"), id);
		return;
	}

	key = panel_gconf_full_key (type, profile, id, "panel_right_stick");
	right_stick = gconf_client_get_bool (client, key, NULL);

	toplevel = panel_profile_get_toplevel_by_id (toplevel_id);
	g_free (toplevel_id);

	if (!toplevel)
		return;

	panel_widget = panel_toplevel_get_panel_widget (toplevel);

	if (right_stick && !panel_widget->packed)
		position = panel_widget->size - position;

	applet = g_new0 (PanelAppletToLoad, 1);

	applet->type            = applet_type;
	applet->panel_widget    = panel_widget;
	applet->position        = position;
	applet->id              = g_strdup (id);
	applet->destroy_handler =
			g_signal_connect (panel_widget, "destroy",
					  G_CALLBACK (panel_destroyed_while_loading),
					  applet);

	panel_applets_to_load = g_slist_prepend (panel_applets_to_load, applet);
}

static void
panel_applet_load_list (PanelGConfKeyType type)
{
	GConfClient *client;
	GSList      *id_list, *l;
	const char  *profile;
	const char  *key;
	
	client  = gconf_client_get_default ();
	profile = panel_profile_get_name ();

	key = panel_gconf_general_key (profile,
				       type == PANEL_GCONF_APPLETS ? "applet_id_list" :
								     "object_id_list");
        id_list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

        for (l = id_list; l; l = l->next) {
		panel_applet_load_with_id (type, client, profile, l->data);
		g_free (l->data);
	}
	g_slist_free (id_list);

	g_object_unref (client);
}

static int
panel_applet_compare (const PanelAppletToLoad *a,
		      const PanelAppletToLoad *b)
{
	if (a->panel_widget != b->panel_widget)
		return a->panel_widget - b->panel_widget;
	else
		return a->position - b->position;
}

void
panel_applet_load_applets_from_gconf (void)
{
	panel_applet_load_list (PANEL_GCONF_APPLETS);
	panel_applet_load_list (PANEL_GCONF_OBJECTS);

	if (!panel_applets_to_load)
		return;

	panel_applets_to_load = g_slist_sort (panel_applets_to_load,
					      (GCompareFunc) panel_applet_compare);

	g_idle_add (panel_applet_load_idle_handler, NULL);
}

static G_CONST_RETURN char *
panel_applet_get_toplevel_id (AppletInfo *applet)
{
	PanelWidget *panel_widget;

	g_return_val_if_fail (applet != NULL, NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (applet->widget), NULL);

	panel_widget = PANEL_WIDGET (applet->widget->parent);
	if (!panel_widget)
		return NULL;

	return panel_profile_get_toplevel_id (panel_widget->toplevel);
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
	PanelGConfKeyType  key_type;
	GConfClient       *client;
	const char        *profile;
	const char        *key;
	const char        *toplevel_id;
	gboolean           right_stick;
	int                position;

	g_return_if_fail (applet_info != NULL);

	if (!immediate) {
		if (!queued_position_source)
			queued_position_source =
				g_timeout_add (1000,
					       (GSourceFunc) panel_applet_position_save_timeout,
					       NULL);

		if (!g_slist_find (queued_position_saves, applet_info))
			queued_position_saves =
				g_slist_prepend (queued_position_saves, applet_info);

		return;
	}

	if (!(toplevel_id = panel_applet_get_toplevel_id (applet_info)))
		return;

	client  = gconf_client_get_default ();
	profile = panel_profile_get_name ();

	key_type = applet_info->type == APPLET_BONOBO ? PANEL_GCONF_APPLETS : PANEL_GCONF_OBJECTS;

	key = panel_gconf_full_key (key_type, profile, id, "panel_id");
	gconf_client_set_string (client, key, toplevel_id, NULL);

	right_stick = panel_is_applet_right_stick (applet_info->widget);
	key = panel_gconf_full_key (
			key_type, profile, id, "panel_right_stick");
	gconf_client_set_bool (client, key, right_stick, NULL);

	position = panel_applet_get_position (applet_info);
	if (right_stick && !PANEL_WIDGET (applet_info->widget->parent)->packed)
		position = PANEL_WIDGET (applet_info->widget->parent)->size - position;

	key = panel_gconf_full_key (key_type, profile, id, "position");
	gconf_client_set_int (client, key, position, NULL);

	g_object_unref (client);
}

static void
panel_applet_save_to_gconf (AppletInfo *applet_info)
{
	PanelGConfKeyType  key_type;
	GConfClient       *client;
	const char        *profile;
	const char        *key;
	GSList            *id_list, *l;

	client  = gconf_client_get_default ();
	profile = panel_profile_get_name ();

	key = panel_gconf_general_key (profile,
				       applet_info->type == APPLET_BONOBO ? "applet_id_list" :
									    "object_id_list");
	id_list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	key_type = applet_info->type == APPLET_BONOBO ? PANEL_GCONF_APPLETS :
							PANEL_GCONF_OBJECTS;

	if (!applet_info->id)
		applet_info->id = panel_profile_find_new_id (key_type, id_list);

	for (l = id_list; l; l = l->next)
		if (strcmp (applet_info->id, (char *) l->data) == 0)
			break;

	if (!l) {
		id_list = g_slist_prepend (id_list, g_strdup (applet_info->id));

		gconf_client_set_list (client, key, GCONF_VALUE_STRING, id_list, NULL);
	}

	for (l = id_list; l; l = l->next)
		g_free (l->data);
	g_slist_free (id_list);

	key = panel_gconf_full_key (key_type, profile, applet_info->id, "object_type");
	gconf_client_set_string (
		client, key,
		gconf_enum_to_string (object_type_enum_map, applet_info->type),
		NULL);

	panel_applet_save_position (applet_info, applet_info->id, TRUE);

	switch (applet_info->type) {
	case APPLET_BONOBO:
		panel_applet_frame_save_to_gconf (PANEL_APPLET_FRAME (applet_info->widget),
						  applet_info->id);
		break;
	case APPLET_DRAWER:
		drawer_save_to_gconf ((Drawer *) applet_info->data,
				      applet_info->id);
		break;
	case APPLET_MENU:
		panel_menu_button_save_to_gconf (PANEL_MENU_BUTTON (applet_info->widget),
						 applet_info->id);
		break;
	case APPLET_LAUNCHER:
		launcher_save_to_gconf ((Launcher *) applet_info->data,
					applet_info->id);
		break;
	case APPLET_ACTION:
	case APPLET_LOGOUT:
	case APPLET_LOCK:
		panel_action_button_save_to_gconf (
			PANEL_ACTION_BUTTON (applet_info->widget),
			applet_info->id);
		break;
	case APPLET_MENU_BAR:
		panel_menu_bar_save_to_gconf (
			PANEL_MENU_BAR (applet_info->widget),
			applet_info->id);
		break;
	default:
		break;
	}

	g_object_unref (client);
}

AppletInfo *
panel_applet_register (GtkWidget      *applet,
		       gpointer        data,
		       GDestroyNotify  data_destroy,
		       PanelWidget    *panel,
		       gint            pos,
		       gboolean        exactpos,
		       AppletType      type,
		       const char     *id)
{
	AppletInfo *info;
	int newpos;
	gboolean insert_at_pos;
	
	g_return_val_if_fail (applet != NULL && panel != NULL, NULL);

	if ( ! GTK_WIDGET_NO_WINDOW (applet))
		gtk_widget_set_events (applet, (gtk_widget_get_events (applet) |
						APPLET_EVENT_MASK) &
				       ~( GDK_POINTER_MOTION_MASK |
					  GDK_POINTER_MOTION_HINT_MASK));

	info = g_new0 (AppletInfo, 1);
	info->applet_id = applet_count;
	info->type = type;
	info->widget = applet;
	info->menu = NULL;
	info->data = data;
	info->data_destroy = data_destroy;
	info->user_menu = NULL;

	g_object_set_data (G_OBJECT (applet), "applet_info", info);

	if (type == APPLET_DRAWER) {
		Drawer *drawer = data;
		PanelWidget *assoc_panel;

		assoc_panel = panel_toplevel_get_panel_widget (drawer->toplevel);

		g_object_set_data (G_OBJECT (applet),
				   PANEL_APPLET_ASSOC_PANEL_KEY, assoc_panel);
		assoc_panel->master_widget = applet;
		g_object_add_weak_pointer (
			G_OBJECT (applet), (gpointer *) &assoc_panel->master_widget);
	}

	g_object_set_data (G_OBJECT (applet),
			   PANEL_APPLET_FORBIDDEN_PANELS, NULL);

	applets = g_slist_append (applets, info);

	applet_count++;

	/*add at the beginning if pos == -1*/
	if (pos >= 0) {
		newpos = pos;
		insert_at_pos = FALSE;
	} else {
		newpos = 0;
		insert_at_pos = FALSE;
	}
	/* if exact pos is on then insert at that precise location */
	if (exactpos)
		insert_at_pos = TRUE;

	if (panel_widget_add (panel, applet, newpos, insert_at_pos) == -1) {
		GSList *l;

		for (l = panels; l; l = l->next)
			if (panel_widget_add (panel, applet, 0, TRUE) != -1)
				break;

		if (!l) {
			panel_applet_clean (info, TRUE);
			g_warning (_("Can't find an empty spot"));
			return NULL;
		}

		panel = PANEL_WIDGET (l->data);
	}

	if (BUTTON_IS_WIDGET (applet) ||
	    !GTK_WIDGET_NO_WINDOW (applet)) {
		g_signal_connect (applet, "button_press_event",
				  G_CALLBACK (applet_button_press),
				  info);

		g_signal_connect (applet, "popup_menu",
				  G_CALLBACK (applet_popup_menu),
				  info);
	}

	g_signal_connect (applet, "destroy",
			  G_CALLBACK (panel_applet_destroy),
			  info);

	gtk_widget_show_all (applet);

	orientation_change (info, panel);
	size_change (info, panel);
	back_change (info, panel);

	if (id) info->id = g_strdup (id);

	panel_applet_save_to_gconf (info);

	if (type != APPLET_BONOBO)
		gtk_widget_grab_focus (applet);
	else
		gtk_widget_child_focus (applet, GTK_DIR_TAB_FORWARD);

	return info;
}

int
panel_applet_get_position (AppletInfo *applet)
{
	AppletData *applet_data;

	g_return_val_if_fail (applet != NULL, 0);
	g_return_val_if_fail (G_IS_OBJECT (applet->widget), 0);

	applet_data = g_object_get_data (G_OBJECT (applet->widget), PANEL_APPLET_DATA);

	return applet_data->pos;
}
