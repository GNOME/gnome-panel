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
#include <libgnome/libgnome.h>
#include <libbonobo.h>
#include <gdk/gdkx.h>

#include "basep-widget.h"
#include "button-widget.h"
#include "drawer.h"
#include "launcher.h"
#include "menu-util.h"
#include "menu.h"
#include "panel-config.h"
#include "panel-gconf.h"
#include "panel-config-global.h"
#include "session.h"
#include "panel-applet-frame.h"
#include "egg-screen-exec.h"
#include "panel-action-button.h"

#define SMALL_ICON_SIZE 20

extern GSList *panels;

GSList *applets = NULL;
int applet_count = 0;

/*config sync stuff*/
extern int applets_to_sync;
extern int need_complete_save;

extern gboolean commie_mode;
extern GlobalConfig global_config;

static GConfEnumStringPair object_type_enum_map [] = {
	{ APPLET_DRAWER,   "drawer-object" },
	{ APPLET_MENU,     "menu-object" },
	{ APPLET_LAUNCHER, "launcher-object" }, 
	{ APPLET_BONOBO,   "bonobo-applet" },
	{ APPLET_ACTION,   "action-applet" },
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
			  const char *gconf_key,
			  gboolean    clean_gconf)
{
	GConfClient *client;
	GSList      *id_list, *l;
	const char  *temp_key = NULL;
	const char  *profile;

	g_return_if_fail (gconf_key != NULL);

	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	if (type == APPLET_BONOBO)
		temp_key = panel_gconf_general_key (profile, "applet_id_list");
	else
		temp_key = panel_gconf_general_key (profile, "object_id_list");

        id_list = gconf_client_get_list (
			panel_gconf_get_client (), temp_key,
			GCONF_VALUE_STRING, NULL);

        for (l = id_list; l; l = l->next)
                if (strcmp (gconf_key, (char *) l->data) == 0)
                        break;

        if (l) {
		g_free (l->data);
		l->data = NULL;
                id_list = g_slist_delete_link (id_list, l);

                gconf_client_set_list (client, temp_key, GCONF_VALUE_STRING, id_list, NULL);

		if (type == APPLET_BONOBO)
			temp_key = panel_gconf_sprintf (
					"/apps/panel/profiles/%s/applets/%s",
					profile, gconf_key);
		else
			temp_key = panel_gconf_sprintf (
					"/apps/panel/profiles/%s/objects/%s",
					profile, gconf_key);

		if (clean_gconf)
			panel_gconf_clean_dir (client, temp_key);
        }

	panel_g_slist_deep_free (id_list);
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
		Launcher    *launcher = info->data;
		const gchar *location;

		location = gnome_desktop_item_get_location (launcher->ditem);

		/* Launcher may not yet have been hoarded */
		if (location)
			session_add_dead_launcher (location);
	}

	panel_applet_clean_gconf (info->type, info->gconf_key, clean_gconf);

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
	return gtk_window_get_screen (
			GTK_WINDOW (get_panel_parent (menu->info->widget)));
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
			panel_config(drawer->drawer);
		} else if (strcmp (menu->name, "help") == 0) {
			panel_show_help (screen, "wgospanel.xml", "gospanel-18");
		}
		break;
	case APPLET_MENU:
		if (!strcmp (menu->name, "help"))
			panel_show_help (screen, "wgospanel.xml", "gospanel-37");
		break;
	case APPLET_ACTION:
	case APPLET_LOGOUT:
	case APPLET_LOCK:
		panel_action_button_invoke_menu (
			PANEL_ACTION_BUTTON (menu->info->widget), menu->name);
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
applet_menu_deactivate(GtkWidget *w, AppletInfo *info)
{
	GtkWidget *panel = get_panel_parent(info->widget);
	info->menu_age = 0;
	
	if(BASEP_IS_WIDGET(panel))
		BASEP_WIDGET(panel)->autohide_inhibit = FALSE;
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
		info->menu_age = 0;
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
		info->menu_age = 0;
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
	GtkWidget *panel;

	g_return_if_fail (info != NULL);

	panel = get_panel_parent (info->widget);

	if (!info->menu)
		info->menu = panel_applet_create_menu (info);

	g_assert (info->menu);

	if (BASEP_IS_WIDGET (panel)) {
		BASEP_WIDGET (panel)->autohide_inhibit = TRUE;
		basep_widget_queue_autohide (BASEP_WIDGET (panel));
	}

	info->menu_age = 0;

	panel_applet_menu_set_recurse (GTK_MENU (info->menu),
				       "menu_panel",
				       info->widget->parent);

	gtk_menu_set_screen (GTK_MENU (info->menu),
			     panel_screen_from_toplevel (panel));

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
	if (panel_applet_in_drag)
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

		if (drawer->drawer) {
			PANEL_WIDGET (BASEP_WIDGET (drawer->drawer)->panel)->master_widget = NULL;

			gtk_widget_destroy (drawer->drawer);
			drawer->drawer = NULL;
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

	g_free (info->gconf_key);
	info->gconf_key = NULL;

	g_free (info);
}

typedef struct {
	AppletType   type;
	PanelWidget *panel_widget;
	int          position;
	char        *unique_id;
	gulong       destroy_handler;
} PanelAppletToLoad;

static GSList *panel_applets_to_load = NULL;

static void
whack_applet_to_load (PanelAppletToLoad *applet)
{
	if (applet->destroy_handler > 0)
		g_signal_handler_disconnect (applet->panel_widget,
					     applet->destroy_handler);
	applet->destroy_handler = 0;

	g_free (applet->unique_id);
	applet->unique_id = NULL;

	g_free (applet);
}

static gboolean
panel_applet_load_idle_handler (gpointer dummy)
{
	PanelAppletToLoad *applet;

	if (panel_applets_to_load == NULL)
		return FALSE;

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
					applet->unique_id);
		break;
	case APPLET_DRAWER:
		drawer_load_from_gconf (applet->panel_widget,
					applet->position,
					applet->unique_id);
		break;
	case APPLET_MENU:
		menu_load_from_gconf (applet->panel_widget,
				      applet->position,
				      applet->unique_id);
		break;
	case APPLET_LAUNCHER:
		launcher_load_from_gconf (applet->panel_widget,
					  applet->position,
					  applet->unique_id);
		break;
	case APPLET_LOGOUT:  /* FIXME: This is backward compatibility only. */
	case APPLET_LOCK:    /*        Remove at some time in the future    */
		panel_action_button_load (
				applet->type == APPLET_LOGOUT ? PANEL_ACTION_LOGOUT :
								PANEL_ACTION_LOCK,
				applet->panel_widget,
				applet->position,
				TRUE,
				applet->unique_id,
				TRUE);
		break;
	case APPLET_ACTION:
		panel_action_button_load_from_gconf (
				applet->panel_widget,
				applet->position,
				TRUE,
				applet->unique_id);
	default:
		break;
	}

	whack_applet_to_load (applet);

	return TRUE;
}

static void
panel_destroyed_while_loading (GtkWidget *panel, gpointer data)
{
	PanelAppletToLoad *applet = data;

	applet->destroy_handler = 0;

	panel_applets_to_load =
		g_slist_remove (panel_applets_to_load, applet);

	whack_applet_to_load (applet);
}

static void
panel_applet_load_from_unique_id (PanelGConfKeyType  type,
				  GConfClient       *gconf_client,
				  const char        *profile,
				  const char        *unique_id)
{
	PanelAppletToLoad *applet;
	PanelWidget       *panel_widget;
	AppletType         applet_type = 0;
	const char        *temp_key;
	char              *type_string;
	char              *panel_id;
	int                position;
	gboolean           right_stick;

	temp_key = panel_gconf_full_key (type, profile, unique_id, "object_type");
	type_string = gconf_client_get_string (gconf_client, temp_key, NULL);
	if (!type_string) {
		g_printerr (_("No object_type set for panel object with ID %s\n"), unique_id);
		return;
	}
        
	if (!gconf_string_to_enum (object_type_enum_map,
				   type_string,
				   (int *) &applet_type)) {
		g_free (type_string);
		g_warning ("Unkown applet type %s from %s", type_string, temp_key);
		return;
	}
	
	g_free (type_string);

	temp_key = panel_gconf_full_key (type, profile, unique_id, "position");
	position = gconf_client_get_int (gconf_client, temp_key, NULL);
	
	temp_key = panel_gconf_full_key (type, profile, unique_id, "panel_id");
	panel_id = gconf_client_get_string (gconf_client, temp_key, NULL);
	if (!panel_id) {
		g_printerr (_("No panel_id set for panel object with ID %s\n"), unique_id);
		return;
	}

	temp_key = panel_gconf_full_key (type, profile, unique_id, "panel_right_stick");
	right_stick = gconf_client_get_bool (gconf_client, temp_key, NULL);

	panel_widget = panel_widget_get_by_id (panel_id);
	g_free (panel_id);

	if (!panel_widget)
		return;

	if (right_stick && !panel_widget->packed)
		position = panel_widget->size - position;

	applet = g_new0 (PanelAppletToLoad, 1);

	applet->type            = applet_type;
	applet->panel_widget    = panel_widget;
	applet->position        = position;
	applet->unique_id       = g_strdup (unique_id);
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
	const char  *temp_key = NULL;
	
	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	if (type == PANEL_GCONF_APPLETS)
		temp_key = panel_gconf_general_key (profile, "applet_id_list");
	else
        	temp_key = panel_gconf_general_key (profile, "object_id_list");

        id_list = gconf_client_get_list (
			panel_gconf_get_client (), temp_key,
			GCONF_VALUE_STRING, NULL);

        for (l = id_list; l; l = l->next)
		panel_applet_load_from_unique_id (type, client, profile, (char *) l->data);

	panel_g_slist_deep_free (id_list);
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

static void
panel_applet_load_default_applet_for_screen (PanelGConfKeyType  type,
					     const char        *profile,
					     const char        *applet_id,
					     int                screen)
{
	GConfClient *client;
	GError      *error = NULL;
	GSList      *id_list, *l;
	const char  *key;
	char        *new_applet_id;
	char        *panel_id, *new_panel_id;

	new_applet_id = panel_gconf_load_default_config_for_screen (
				type, profile, applet_id, screen, &error);
	if (error) {
		g_warning ("Could not load default config for applet '%s': '%s'\n",
			   applet_id, error->message);
		g_error_free (error);
		return;
	}

	client = panel_gconf_get_client ();

	key = panel_gconf_full_key (type, profile, new_applet_id, "panel_id");
	panel_id = gconf_client_get_string (client, key, NULL);
	if (!panel_id) {
		g_printerr (_("No panel_id set for panel object with ID %s\n"), new_applet_id);
		g_free (new_applet_id);
		return;
	}

	new_panel_id = g_strdup_printf ("%s_screen%d", panel_id, screen);
	gconf_client_set_string (client, key, new_panel_id, NULL);
	g_free (new_panel_id);
	g_free (panel_id);

	if (type == PANEL_GCONF_APPLETS)
		key = panel_gconf_general_key (profile, "applet_id_list");
	else
		key = panel_gconf_general_key (profile, "object_id_list");

	id_list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	for (l = id_list; l; l = l->next)
		if (!strcmp (new_applet_id, (char *) l->data))
			break;

	if (!l) {
		id_list = g_slist_prepend (id_list, new_applet_id);
		new_applet_id = NULL;

		gconf_client_set_list (client, key, GCONF_VALUE_STRING, id_list, NULL);
	}

	panel_g_slist_deep_free (id_list);
	g_free (new_applet_id);
}

void
panel_applet_load_defaults_for_screen (PanelGConfKeyType  type,
				       const char        *profile,
				       int                screen)
{
	GConfClient *client;
	GSList      *applets, *l;
	GError      *error = NULL;
	const char  *subdir = NULL;
	const char  *schemas_dir = NULL;

	switch (type) {
	case PANEL_GCONF_APPLETS:
		subdir = "applets";
		break;
	case PANEL_GCONF_OBJECTS:
		subdir = "objects";
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	client = panel_gconf_get_client ();

	/* FIXME: "medium" shouldn't be hardcoded.
	 */
	schemas_dir = panel_gconf_sprintf (
			"/schemas/apps/panel/default_profiles/medium/%s", subdir);

	applets = gconf_client_all_dirs (client, schemas_dir, &error);
	if (error) {
		g_warning ("Cannot list default '%s': '%s'\n", subdir, error->message);
		g_error_free (error);
		return;
	}

	for (l = applets; l; l = l->next) {
		char *applet_id;

		applet_id = g_path_get_basename (l->data);

		panel_applet_load_default_applet_for_screen (	
				type, profile, applet_id, screen);

		g_free (applet_id);
		g_free (l->data);
	}

        g_slist_free (applets);
}

static G_CONST_RETURN char *
panel_applet_get_panel_id (AppletInfo *applet)
{
	PanelWidget *panel;

	g_return_val_if_fail (applet != NULL, NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (applet->widget), NULL);

	panel = PANEL_WIDGET (applet->widget->parent);
	if (!panel)
		return NULL;

	return panel->unique_id;
}

static gboolean
panel_applet_position_save_timeout (gpointer dummy)
{
	GSList *l;

	queued_position_source = 0;

	for (l = queued_position_saves; l; l = l->next) {
		AppletInfo *info = l->data;

		panel_applet_save_position (info, info->gconf_key, TRUE);
	}

	g_slist_free (queued_position_saves);
	queued_position_saves = NULL;

	return FALSE;
}

void
panel_applet_save_position (AppletInfo *applet_info,
			    const char *gconf_key,
			    gboolean    immediate)
{
	PanelGConfKeyType  key_type;
	GConfClient       *client;
	const char        *profile;
	const char        *temp_key;
	const char        *panel_id;
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

	if (!(panel_id = panel_applet_get_panel_id (applet_info)))
		return;

	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	if (applet_info->type == APPLET_BONOBO)
		key_type = PANEL_GCONF_APPLETS;
	else
		key_type = PANEL_GCONF_OBJECTS;

	temp_key = panel_gconf_full_key (key_type, profile, gconf_key, "panel_id");
	gconf_client_set_string (client, temp_key, panel_id, NULL);

	right_stick = panel_is_applet_right_stick (applet_info->widget);
	temp_key = panel_gconf_full_key (
			key_type, profile, gconf_key, "panel_right_stick");
	gconf_client_set_bool (client, temp_key, right_stick, NULL);

	position = panel_applet_get_position (applet_info);
	if (right_stick && !PANEL_WIDGET (applet_info->widget->parent)->packed)
		position = PANEL_WIDGET (applet_info->widget->parent)->size - position;

	temp_key = panel_gconf_full_key (
			key_type, profile, gconf_key, "position");
	gconf_client_set_int (client, temp_key, position, NULL);
}

void
panel_applet_save_to_gconf (AppletInfo *applet_info)
{
	PanelGConfKeyType  key_type;
	GConfClient       *client;
	const char        *profile;
	const char        *temp_key;
	GSList            *id_list, *l;

	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	if (applet_info->type == APPLET_BONOBO)
		temp_key = panel_gconf_general_key (profile, "applet_id_list");
	else
		temp_key = panel_gconf_general_key (profile, "object_id_list");

	id_list = gconf_client_get_list (client, temp_key, GCONF_VALUE_STRING, NULL);

	if (!applet_info->gconf_key)
		applet_info->gconf_key = gconf_unique_key ();

	for (l = id_list; l; l = l->next)
		if (strcmp (applet_info->gconf_key, (char *) l->data) == 0)
			break;

	if (!l) {
		id_list = g_slist_prepend (id_list, g_strdup (applet_info->gconf_key));

		gconf_client_set_list (client, temp_key, GCONF_VALUE_STRING, id_list, NULL);
	}

	panel_g_slist_deep_free (id_list);

	if (applet_info->type == APPLET_BONOBO)
		key_type = PANEL_GCONF_APPLETS;
	else
		key_type = PANEL_GCONF_OBJECTS;

	temp_key = panel_gconf_full_key (
			key_type, profile, applet_info->gconf_key, "object_type");
	gconf_client_set_string (
		client, temp_key,
		gconf_enum_to_string (object_type_enum_map, applet_info->type),
		NULL);

	panel_applet_save_position (applet_info, applet_info->gconf_key, TRUE);

	switch (applet_info->type) {
	case APPLET_BONOBO:
		panel_applet_frame_save_to_gconf (PANEL_APPLET_FRAME (applet_info->widget),
						  applet_info->gconf_key);
		break;
	case APPLET_DRAWER:
		drawer_save_to_gconf ((Drawer *) applet_info->data,
				      applet_info->gconf_key);
		break;
	case APPLET_MENU:
		menu_save_to_gconf ((Menu *) applet_info->data,
				    applet_info->gconf_key);
		break;
	case APPLET_LAUNCHER:
		launcher_save_to_gconf ((Launcher *) applet_info->data,
					applet_info->gconf_key);
		break;
	case APPLET_ACTION:
	case APPLET_LOGOUT:
	case APPLET_LOCK:
		panel_action_button_save_to_gconf (
			PANEL_ACTION_BUTTON (applet_info->widget),
			applet_info->gconf_key);
		break;
	default:
		break;
	}
}

AppletInfo *
panel_applet_register (GtkWidget      *applet,
		       gpointer        data,
		       GDestroyNotify  data_destroy,
		       PanelWidget    *panel,
		       gint            pos,
		       gboolean        exactpos,
		       AppletType      type,
		       const char     *gconf_key)
{
	AppletInfo *info;
	int newpos;
	gboolean insert_at_pos;
	gboolean expand_major = FALSE, expand_minor = FALSE;
	
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
	info->menu_age = 0;
	info->data = data;
	info->data_destroy = data_destroy;
	info->user_menu = NULL;

	g_object_set_data (G_OBJECT (applet), "applet_info", info);

	if (type == APPLET_DRAWER) {
		Drawer *drawer = data;
		PanelWidget *assoc_panel =
			PANEL_WIDGET (BASEP_WIDGET (drawer->drawer)->panel);

		g_object_set_data (G_OBJECT (applet),
				   PANEL_APPLET_ASSOC_PANEL_KEY, assoc_panel);
		assoc_panel->master_widget = applet;
		g_object_add_weak_pointer (
			G_OBJECT (applet), (gpointer *) &assoc_panel->master_widget);
	}

	g_object_set_data (G_OBJECT (applet),
			   PANEL_APPLET_FORBIDDEN_PANELS, NULL);

	if (type == APPLET_BONOBO)
		panel_applet_frame_get_expand_flags (PANEL_APPLET_FRAME (applet),
						     &expand_major,
						     &expand_minor);
	
	applets = g_slist_append (applets, info);

	applet_count++;

	/*we will need to save this applet's config now*/
	applets_to_sync = TRUE;

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

	if (panel_widget_add (panel, applet, newpos,
			      insert_at_pos, expand_major, expand_minor) == -1) {
		GSList *l;

		for (l = panels; l; l = l->next)
			if (panel_widget_add (panel, applet, 0, TRUE,
					      expand_major, expand_minor) != -1)
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

	if (gconf_key)
		info->gconf_key = g_strdup (gconf_key);

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
