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
#include "menu-properties.h"
#include "menu-util.h"
#include "panel-config.h"
#include "panel-gconf.h"
#include "panel-config-global.h"
#include "session.h"
#include "status.h"
#include "swallow.h"
#include "panel-applet-frame.h"

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
	{ APPLET_LOGOUT,   "logout-object" },
	{ APPLET_SWALLOW,  "swallow-object" },
	{ APPLET_LAUNCHER, "launcher-object" }, 
	{ APPLET_EMPTY,    "empty-object" },
	{ APPLET_LOCK,     "lock-object" },
	{ APPLET_STATUS,   "status-object" },
	{ APPLET_RUN,      "run-object" },
	{ APPLET_BONOBO,   "bonobo-applet" },
};

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

static void
panel_applet_clean_gconf (AppletInfo *info)
{
	GConfClient *client;
	GSList      *id_list, *l;
	char        *temp_key = NULL;
	char        *profile;

	g_return_if_fail (info && info->gconf_key);

        client  = panel_gconf_get_client ();
	profile = session_get_current_profile ();

	if (info->type == APPLET_BONOBO)
		temp_key = panel_gconf_general_profile_get_full_key (profile, "applet-id-list");
	else
		temp_key = panel_gconf_general_profile_get_full_key (profile, "object-id-list");

        id_list = gconf_client_get_list (panel_gconf_get_client (), temp_key, GCONF_VALUE_STRING, NULL);

        for (l = id_list; l; l = l->next)
                if (!strcmp (info->gconf_key, (char *) l->data))
                        break;

        if (l) {
		char *tmp;

                id_list = g_slist_remove_link (id_list, l);

		g_free (l->data);
		g_slist_free (l);

                gconf_client_set_list (client, temp_key, GCONF_VALUE_STRING, id_list, NULL);

		if (info->type == APPLET_BONOBO)
			tmp = g_strdup_printf ("/apps/panel/profiles/%s/applets/%s",
					       profile, info->gconf_key);
		else
			tmp = g_strdup_printf ("/apps/panel/profiles/%s/objects/%s",
					       profile, info->gconf_key);

		panel_gconf_directory_recursive_clean (client, tmp);

		g_free (tmp);
        }

        g_free (temp_key);
        g_slist_foreach (id_list, (GFunc) g_free, NULL);
        g_slist_free (id_list);
}

/*destroy widgets and call the above cleanup function*/
void
panel_applet_clean (AppletInfo *info)
{
	g_return_if_fail (info != NULL);

	panel_applet_clean_gconf (info);

	applets = g_slist_remove (applets, info);

	if (info->widget != NULL) {
		if(info->type == APPLET_STATUS) {
			status_applet_put_offscreen (info->data);
		}
		/* destroy will remove it from the panel */
		gtk_widget_destroy (info->widget);
		info->widget = NULL;
	}

	info->data = NULL;

	/*
	 * FIXME: where are we supposed to free the structure?
	 */
}

static gboolean
applet_idle_remove (gpointer data)
{
	AppletInfo *info = data;

	panel_applet_save_position (info, info->gconf_key);

	if (info->type == APPLET_SWALLOW) {
		Swallow *swallow = info->data;

		swallow->clean_remove = TRUE;
	}

	panel_applet_clean (info);

	return FALSE;
}

static void
applet_remove_callback (GtkWidget  *widget,
			AppletInfo *info)
{
	g_idle_add (applet_idle_remove, info);
}

static void
applet_callback_callback(GtkWidget *widget, gpointer data)
{
	AppletUserMenu *menu = data;

	g_return_if_fail(menu->info != NULL);

	switch(menu->info->type) {
	case APPLET_LAUNCHER:
		if (strcmp (menu->name, "properties") == 0) {
			launcher_properties (menu->info->data);
		} else if (strcmp (menu->name, "help") == 0) {
			panel_show_help ("launchers", NULL);
		} else if (strcmp (menu->name, "help_on_app") == 0) {
			Launcher * launcher = menu->info->data;
			if (launcher->ditem != NULL) {
				const char *docpath =
					gnome_desktop_item_get_string
					    (launcher->ditem, "DocPath");
				char *path =
					panel_gnome_kde_help_path (docpath);
				if (path != NULL) {
					gnome_url_show (path, NULL);
					/* FIXME: handle errors */
					g_free (path);
				}
			}
		}
		break;
	case APPLET_DRAWER: 
		if (strcmp (menu->name, "properties")==0) {
			Drawer *drawer = menu->info->data;
			g_assert(drawer);
			panel_config(drawer->drawer);
		} else if (strcmp (menu->name, "help") == 0) {
			panel_show_help ("drawers", NULL);
		}
		break;
	case APPLET_SWALLOW:
 		if (strcmp (menu->name, "help") == 0)
			panel_show_help ("specialobjects", "SWALLOWEDAPP");
#if 0
		if(strcmp(menu->name,"properties")==0) {
			Swallow *swallow = info->data;
			g_assert(swallow);
			swallow_properties(swallow); /* doesn't exist yet*/
		}
#endif
		break; 
	case APPLET_MENU:
		if(strcmp(menu->name,"properties")==0)
			menu_properties(menu->info->data);
		else if(strcmp(menu->name,"edit_menus")==0) {
			char *tmp[3] = { NULL, NULL, NULL };
			if((tmp[0] = g_find_program_in_path ("nautilus")) != NULL)  {
				tmp[1] = "applications:/";
				gnome_execute_async (g_get_home_dir (), 2, tmp);
				g_free (tmp[0]);
			}
		} else if (strcmp (menu->name, "help") == 0) {
			Menu *menu2 = menu->info->data;
			char *page;
			page = menu2->main_menu ? "mainmenu" : "menus";
			panel_show_help (page, NULL);
		}
		break;
	case APPLET_LOCK: {
                /*
		  <jwz> Blank Screen Now
		  <jwz> Lock Screen Now
		  <jwz> Kill Daemon
		  <jwz> Restart Daemon
		  <jwz> Preferences
		  <jwz> (or "configuration" instead?  whatever word you use)
		  <jwz> those should do xscreensaver-command -activate, -lock, -exit...
		  <jwz> and "xscreensaver-command -exit ; xscreensaver &"
		  <jwz> and "xscreensaver-demo"
		*/
		char *command = NULL;
		gboolean freeit = FALSE;
		if (strcmp (menu->name, "help") == 0)
			panel_show_help ("specialobjects", "LOCKBUTTON");
		else if (!strcmp (menu->name, "restart")) {
			command = "xscreensaver-command -exit ; xscreensaver &";
		} else if (!strcmp (menu->name, "prefs")) {
			command = "xscreensaver-demo";
		} else {
			command = g_strdup_printf ("xscreensaver-command -%s",
						   menu->name);
			freeit = TRUE;
		}
		if (command)
			gnome_execute_shell (g_get_home_dir (), command);
		if (freeit)
			g_free (command);
		break;
	}
	case APPLET_LOGOUT:
		if (strcmp (menu->name, "help") == 0)
			panel_show_help ("specialobjects", "LOGOUTBUTTON");
		break;
	case APPLET_STATUS:
		if (strcmp (menu->name, "help") == 0)
			panel_show_help ("specialobjects", "STATUSDOC");
		break;
	case APPLET_RUN:
		if (strcmp (menu->name, "help") == 0)
			panel_show_help ("specialobjects", "RUNBUTTON");
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

		if (!strcmp (menu->name, name))
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

void
panel_applet_callback_set_sensitive (AppletInfo  *info,
				     const gchar *callback_name,
				     gint         sensitive)
{
	AppletUserMenu *menu;

	g_return_if_fail(info != NULL);
	
	menu = panel_applet_get_callback(info->user_menu, callback_name);
	if (menu != NULL)
		menu->sensitive = sensitive;
	else
		return; /*it just isn't there*/

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

static void
setup_an_item(AppletUserMenu *menu,
	      GtkWidget *submenu,
	      int is_submenu)
{
	menu->menuitem = gtk_image_menu_item_new ();

	g_signal_connect (G_OBJECT (menu->menuitem), "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &menu->menuitem);
	if(menu->stock_item && *(menu->stock_item))
		setup_menuitem (menu->menuitem,
				gtk_image_new_from_stock (menu->stock_item,
							  GTK_ICON_SIZE_MENU),
				menu->text);
	else
		setup_menuitem (menu->menuitem,
				NULL,
				menu->text);

	if(submenu)
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menu->menuitem);

	/*if an item not a submenu*/
	if(!is_submenu) {
		g_signal_connect(G_OBJECT(menu->menuitem), "activate",
				 G_CALLBACK (applet_callback_callback),
				   menu);
		g_signal_connect(G_OBJECT (submenu), "destroy",
				 G_CALLBACK (gtk_widget_destroyed),
				 &menu->submenu);
	/* if the item is a submenu and doesn't have it's menu
	   created yet*/
	} else if(!menu->submenu) {
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
applet_setup_panel_menu (gboolean is_basep)
{
	GtkWidget *menuitem;
	GtkWidget *panel_menu;
	gchar     *pixmap_path;

	menuitem = gtk_image_menu_item_new ();

	pixmap_path = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
						 "gnome-panel.png", TRUE, NULL);
	if (pixmap_path) {
		GdkPixbuf *pixbuf, *scaled_pixbuf = NULL;
		GtkWidget *image = NULL;
		
		pixbuf = gdk_pixbuf_new_from_file (pixmap_path, NULL);

		if (pixbuf != NULL) {
			scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
								 SMALL_ICON_SIZE,
								 SMALL_ICON_SIZE,
								 GDK_INTERP_BILINEAR);
			g_object_unref (pixbuf);
		}

		if (scaled_pixbuf != NULL) {
			image = gtk_image_new_from_pixbuf (scaled_pixbuf);
			g_object_unref (scaled_pixbuf);
		}
		
		setup_menuitem (menuitem, 
				image,
				_("Panel"));

		g_free (pixmap_path);
	} else {
		g_message (_("Cannot find pixmap file %s"), "gnome-panel.png");

		setup_menuitem (menuitem, NULL, _("Panel"));
	}

	panel_menu = gtk_menu_new ();

	make_panel_submenu (panel_menu, TRUE, is_basep);

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), panel_menu);

	return menuitem;
}

void
panel_applet_create_menu (AppletInfo *info,
			  gboolean    is_basep)
{
	GtkWidget *menuitem;
	GList     *l;

	info->menu = g_object_ref (gtk_menu_new ());
	gtk_object_sink (GTK_OBJECT (info->menu));

	if (!commie_mode) {
		GtkWidget *image;

		menuitem = gtk_image_menu_item_new ();

		image = gtk_image_new_from_stock (GTK_STOCK_REMOVE,
						  GTK_ICON_SIZE_MENU);

		setup_menuitem (menuitem, image , _("Remove from panel"));

		g_signal_connect (G_OBJECT (menuitem),
				  "activate",
				  G_CALLBACK (applet_remove_callback),
				  info);

		gtk_menu_shell_append (GTK_MENU_SHELL (info->menu), menuitem);

		menuitem = gtk_image_menu_item_new();

		/*
		 * FIXME: should have a "Move" pixmap.
		 */
		setup_menuitem (menuitem, NULL, _("Move"));

		g_signal_connect (G_OBJECT (menuitem),
				  "activate",
				  G_CALLBACK (move_applet_callback),
				  info);

		gtk_menu_shell_append (GTK_MENU_SHELL (info->menu), menuitem);
	}

	menuitem = applet_setup_panel_menu (is_basep);

	gtk_menu_shell_append (GTK_MENU_SHELL (info->menu), menuitem);

	if (info->user_menu) {
		menuitem = gtk_image_menu_item_new ();

		gtk_menu_shell_append (GTK_MENU_SHELL (info->menu), menuitem);

		gtk_widget_set_sensitive (menuitem, FALSE);

		gtk_widget_show (menuitem);
	}

	for (l = info->user_menu; l; l = l->next) {
		AppletUserMenu *menu = (AppletUserMenu *)l->data;

		add_to_submenus (info, "", menu->name, menu, 
				 info->menu, info->user_menu);
	}

	/*
	 * connect the deactivate signal, so that we can "re-allow" 
	 * autohide when the menu is deactivated.
	 */
	g_signal_connect (G_OBJECT (info->menu),
			    "deactivate",
			     G_CALLBACK (applet_menu_deactivate),
			     info);
}

static void
applet_menu_set_data_recursively (GtkMenu     *menu,
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
			applet_menu_set_data_recursively (GTK_MENU (submenu), key, data);
	}

	g_list_free (children);
}

static void
applet_show_menu (AppletInfo     *info,
		  GdkEventButton *event)
{
	GtkWidget *panel;

	g_return_if_fail (info);

	panel = get_panel_parent (info->widget);

	if (!info->menu)
		panel_applet_create_menu (info, BASEP_IS_WIDGET (panel));

	g_assert (info->menu);

	if (BASEP_IS_WIDGET (panel)) {
		BASEP_WIDGET (panel)->autohide_inhibit = TRUE;
		basep_widget_queue_autohide (BASEP_WIDGET (panel));
	}

	info->menu_age = 0;

	applet_menu_set_data_recursively (GTK_MENU (info->menu),
					  "menu_panel",
					  info->widget->parent);

	if (!GTK_WIDGET_REALIZED (info->menu))
		gtk_widget_show (info->menu);

	/* FIXME - off panel popups, should be automatic with new gtk-menu? */
	gtk_menu_popup (GTK_MENU (info->menu),
			NULL,
			NULL,
			applet_menu_position,
			info,
			event->button,
			event->time);
}

static gboolean
applet_button_press (GtkWidget      *widget,
		     GdkEventButton *event,
		     AppletInfo     *info)
{
	if (event->button == 3) {
		if (panel_applet_in_drag)
			return FALSE;

		switch (info->type) {
		case APPLET_BONOBO:
			break;
		case APPLET_SWALLOW: {
			GtkHandleBox *handle_box;

			handle_box = GTK_HANDLE_BOX (((Swallow *)info->data)->handle_box);

			if (handle_box->child_detached)
				applet_show_menu (info, event);
			}
			break;
		default:
			applet_show_menu (info, event);
			break;
		}

		return TRUE;
	}

	if (BUTTON_IS_WIDGET (widget))
		return FALSE;
	else
		return TRUE;
}

static void
applet_destroy (GtkWidget *w, AppletInfo *info)
{
	GList *li;
	
	g_return_if_fail (info != NULL);

	info->widget = NULL;

	if (info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		g_assert(drawer);
		if(drawer->drawer) {
			GtkWidget *dw = drawer->drawer;
			drawer->drawer = NULL;
			PANEL_WIDGET(BASEP_WIDGET(dw)->panel)->master_widget = NULL;
			gtk_widget_destroy(dw);
		}
	} else if (info->type == APPLET_LAUNCHER) {
		Launcher    *launcher = info->data;
		const gchar *location;

		location = gnome_desktop_item_get_location (launcher->ditem);

		session_add_dead_launcher (location);
	}


	if (info->menu != NULL) {
		gtk_widget_unref(info->menu);
		info->menu = NULL;
		info->menu_age = 0;
	}

	info->type = APPLET_EMPTY;

	if (info->data_destroy)
		info->data_destroy (info->data);
	info->data_destroy = NULL;
	info->data = NULL;

	g_free (info->gconf_key);
	info->gconf_key = NULL;

	/*free the user menu*/
	for(li = info->user_menu; li != NULL; li = g_list_next(li)) {
		AppletUserMenu *umenu = li->data;

		li->data = NULL;

		g_free(umenu->name);
		umenu->name = NULL;
		g_free(umenu->stock_item);
		umenu->stock_item = NULL;
		g_free(umenu->text);
		umenu->text = NULL;

		g_free(umenu);
	}
	g_list_free (info->user_menu);
	info->user_menu = NULL;
}

static char *
panel_applet_get_full_gconf_key (AppletType   type,
				 const gchar *profile,
				 const gchar *object_id,
				 const gchar *key)
{
	char *retval = NULL;

	if (type == APPLET_BONOBO)
		retval = panel_gconf_applets_default_profile_get_full_key (profile, object_id, key);
	else
		retval = panel_gconf_objects_default_profile_get_full_key (profile, object_id, key);

	return retval;
}

static void
panel_applet_load_from_unique_id (GConfClient *gconf_client,
				  const char  *profile,
				  const char  *unique_id)
{
	PanelWidget *panel_widget;
	AppletType   applet_type;
	char        *type_string;
	char        *temp_key;
	char        *panel_id;
	int          position;
	gboolean     right_stick;

	temp_key = panel_gconf_applets_default_profile_get_full_key (profile, unique_id, "object-type");
	type_string = gconf_client_get_string (gconf_client, temp_key, NULL);

	if (!gconf_string_to_enum (object_type_enum_map, type_string, (int *) &applet_type)) {
		g_warning ("Unkown applet type %s from %s", type_string, temp_key);
		return;
	}
	
	g_free (temp_key);
	g_free (type_string);

	temp_key = panel_gconf_applets_default_profile_get_full_key (profile, unique_id, "position");
	position = gconf_client_get_int (gconf_client, temp_key, NULL);
	g_free (temp_key);

	temp_key = panel_gconf_applets_default_profile_get_full_key (profile, unique_id, "panel-id");
	panel_id = gconf_client_get_string (gconf_client, temp_key, NULL);
	g_free (temp_key);

	temp_key = panel_gconf_applets_default_profile_get_full_key (profile, unique_id, "right-stick");
	right_stick = gconf_client_get_bool (gconf_client, temp_key, NULL);
	g_free (temp_key);

	panel_widget = panel_widget_get_by_id (panel_id);
	g_free (panel_id);

	/*
	 * A hack from the old panel to make sure the applet is on the right
	 */
	position += right_stick ? G_MAXINT/2 : 0;

	switch (applet_type) {
	case APPLET_BONOBO: {
		char *applet_iid;

		temp_key = panel_gconf_applets_default_profile_get_full_key (profile, unique_id, "bonobo-iid");
		applet_iid = gconf_client_get_string (gconf_client, temp_key, NULL);
		g_free (temp_key);

		panel_applet_frame_load (applet_iid, panel_widget, position, unique_id);

		g_free (applet_iid);
		}
		break;
	default:
		break;
	}
}

static void
panel_applet_load_list (GConfClient *client,
			const char  *profile,
			const char *list_key)
{
	GSList *id_list, *l;
	char   *temp_key;

        temp_key = panel_gconf_general_profile_get_full_key (profile, list_key);

        id_list = gconf_client_get_list (panel_gconf_get_client (), temp_key, GCONF_VALUE_STRING, NULL);

        for (l = id_list; l; l = l->next)
		panel_applet_load_from_unique_id (client, profile, (char *) l->data);

        g_free (temp_key);
        g_slist_foreach (id_list, (GFunc) g_free, NULL);
        g_slist_free (id_list);
}

void
panel_applet_load_applets_from_gconf (void)
{
	GConfClient *client;
	char        *profile;

        client  = panel_gconf_get_client ();
	profile = session_get_current_profile ();

	panel_applet_load_list (client, profile, "applet-id-list");
	panel_applet_load_list (client, profile, "object-id-list");
}

void
panel_applet_save_position (AppletInfo *applet_info,
			    const char *gconf_key)
{
	GConfClient *client;
	char        *profile;
	char        *temp_key;

	client  = panel_gconf_get_client ();
	profile = session_get_current_profile ();

	temp_key = panel_applet_get_full_gconf_key (applet_info->type, profile, gconf_key, "position");
	gconf_client_set_int (client, temp_key, panel_applet_get_position (applet_info), NULL);
	g_free (temp_key);

	temp_key = panel_applet_get_full_gconf_key (applet_info->type, profile, gconf_key, "panel-id");
	gconf_client_set_string (client, temp_key, panel_applet_get_panel_id (applet_info), NULL);
	g_free (temp_key);

	temp_key = panel_applet_get_full_gconf_key (applet_info->type, profile, gconf_key, "right-stick");
	gconf_client_set_bool (client, temp_key, panel_applet_get_right_stick (applet_info), NULL);
	g_free (temp_key);
}

void
panel_applet_save_to_gconf (AppletInfo *applet_info)
{
	GConfClient *client;
	char        *profile;
	char        *temp_key;
	GSList      *id_list, *l;

	client  = panel_gconf_get_client ();
	profile = session_get_current_profile ();

	if (applet_info->type == APPLET_BONOBO)
		temp_key = panel_gconf_general_profile_get_full_key (profile, "applet-id-list");
	else
		temp_key = panel_gconf_general_profile_get_full_key (profile, "object-id-list");

	id_list = gconf_client_get_list (client, temp_key, GCONF_VALUE_STRING, NULL);

	if (!applet_info->gconf_key)
		applet_info->gconf_key = gconf_unique_key ();

	for (l = id_list; l; l = l->next)
		if (!strcmp (applet_info->gconf_key, (char *) l->data))
			break;

	if (!l) {
		id_list = g_slist_prepend (id_list, g_strdup (applet_info->gconf_key));

		gconf_client_set_list (client, temp_key, GCONF_VALUE_STRING, id_list, NULL);
	}

	g_free (temp_key);
	g_slist_foreach (id_list, (GFunc) g_free, NULL);
	g_slist_free (id_list);

	temp_key = panel_applet_get_full_gconf_key (applet_info->type, profile,
						    applet_info->gconf_key, "object-type");
	gconf_client_set_string (client, temp_key,
				 gconf_enum_to_string (object_type_enum_map, applet_info->type),
				 NULL);
	g_free (temp_key);

	panel_applet_save_position (applet_info, applet_info->gconf_key);

	switch (applet_info->type) {
	case APPLET_BONOBO:
		panel_applet_frame_save_to_gconf (PANEL_APPLET_FRAME (applet_info->data),
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
	
	g_return_val_if_fail (applet && panel, NULL);

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
	}

	g_object_set_data (G_OBJECT (applet),
			   PANEL_APPLET_FORBIDDEN_PANELS, NULL);

	if (type == APPLET_BONOBO) {
		panel_applet_frame_get_expand_flags (PANEL_APPLET_FRAME (applet),
						     &expand_major,
						     &expand_minor);
	}
	
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


	

	if (panel_widget_add_full (panel, applet, newpos, TRUE,
				   insert_at_pos, expand_major, expand_minor)==-1) {
		GSList *list;
		for(list = panels; list != NULL; list = g_slist_next(list))
			if (panel_widget_add_full (panel, applet, 0,
						   TRUE, TRUE, expand_major, expand_minor)!=-1)
				break;
		if(!list) {
			/*can't put it anywhere, clean up*/
			gtk_widget_destroy(applet);
			info->widget = NULL;
			panel_applet_clean(info);
			g_warning(_("Can't find an empty spot"));
			g_free (info);
			return NULL;
		}
		panel = PANEL_WIDGET(list->data);
	}

	if(BUTTON_IS_WIDGET (applet) || !GTK_WIDGET_NO_WINDOW(applet))
		g_signal_connect(G_OBJECT(applet),
				   "button_press_event",
				   G_CALLBACK(applet_button_press),
				   info);

	g_signal_connect(G_OBJECT(applet), "destroy",
			   G_CALLBACK(applet_destroy),
			   info);

	gtk_widget_show_all(applet);

	/*g_signal_connect (G_OBJECT (applet), 
			    "drag_request_event",
			    G_CALLBACK(panel_dnd_drag_request),
			    info);

	gtk_widget_dnd_drag_set (GTK_WIDGET(applet), TRUE,
				 applet_drag_types, 1);*/

	orientation_change (info, panel);
	size_change (info, panel);
	back_change (info, panel);

	if (gconf_key)
		info->gconf_key = g_strdup (gconf_key);

	panel_applet_save_to_gconf (info);

	return info;
}

int
panel_applet_get_position (AppletInfo *applet)
{
	AppletData *applet_data;

	g_return_val_if_fail (applet, 0);
	g_return_val_if_fail (G_IS_OBJECT (applet->widget), 0);

	applet_data = g_object_get_data (G_OBJECT (applet->widget), PANEL_APPLET_DATA);

	return applet_data->pos;
}

gchar *
panel_applet_get_panel_id (AppletInfo *applet)
{
	PanelWidget *panel;

	g_return_val_if_fail (applet, NULL);
	g_return_val_if_fail (G_IS_OBJECT (applet->widget), NULL);

	panel = PANEL_WIDGET (applet->widget->parent);

	return panel->unique_id;
}

/*
 * WTF ?
 */
gboolean
panel_applet_get_right_stick (AppletInfo *applet)
{
	PanelWidget *panel;

	g_return_val_if_fail (applet, FALSE);
	g_return_val_if_fail (G_IS_OBJECT (applet->widget), FALSE);

	panel = PANEL_WIDGET (applet->widget->parent);

	return panel_widget_is_applet_stuck (panel, applet->widget);
}
