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

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkx.h>

#include <libpanel-util/panel-glib.h>

#include "panel.h"

#include "applet.h"
#include "button-widget.h"
#include "launcher.h"
#include "panel-bindings.h"
#include "panel-context-menu.h"
#include "panel-util.h"
#include "panel-applet-frame.h"
#include "panel-action-button.h"
#include "panel-menu-bar.h"
#include "panel-separator.h"
#include "panel-multiscreen.h"
#include "panel-toplevel.h"
#include "panel-menu-button.h"
#include "panel-globals.h"
#include "panel-lockdown.h"
#include "panel-icon-names.h"
#include "panel-layout.h"
#include "panel-schemas.h"
#include "panel-user-menu.h"

enum {
	TARGET_URL,
	TARGET_NETSCAPE_URL,
	TARGET_DIRECTORY,
	TARGET_COLOR,
	TARGET_APPLET,
	TARGET_APPLET_INTERNAL,
	TARGET_ICON_INTERNAL,
	TARGET_BGIMAGE,
	TARGET_BACKGROUND_RESET
};

/*we call this recursively*/
static void orient_change_foreach(GtkWidget *w, gpointer data);

void
orientation_change (AppletInfo  *info,
		    PanelWidget *panel)
{
	PanelOrientation orientation;

	orientation = panel_widget_get_applet_orientation (panel);

	switch (info->type) {
	case PANEL_OBJECT_APPLET:
		panel_applet_frame_change_orientation (
				PANEL_APPLET_FRAME (info->widget), orientation);
		break;
	case PANEL_OBJECT_MENU:
	case PANEL_OBJECT_LAUNCHER:
	case PANEL_OBJECT_ACTION:
		button_widget_set_orientation (BUTTON_WIDGET (info->widget), orientation);
		break;
	case PANEL_OBJECT_MENU_BAR:
	case PANEL_OBJECT_USER_MENU:
		panel_menu_bar_object_set_orientation (PANEL_MENU_BAR_OBJECT (info->widget), orientation);
		break;
	case PANEL_OBJECT_SEPARATOR:
		panel_separator_set_orientation (PANEL_SEPARATOR (info->widget),
						 orientation);
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
}

void
back_change (AppletInfo  *info,
	     PanelWidget *panel)
{
	switch (info->type) {
	case PANEL_OBJECT_APPLET:
		panel_applet_frame_change_background (
			PANEL_APPLET_FRAME (info->widget), panel->background.type);
		break;
	case PANEL_OBJECT_MENU_BAR:
	case PANEL_OBJECT_USER_MENU:
		panel_menu_bar_object_change_background (PANEL_MENU_BAR_OBJECT (info->widget));
		break;
	case PANEL_OBJECT_SEPARATOR:
		panel_separator_change_background (PANEL_SEPARATOR (info->widget));
		break;
	default:
		break;
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
panel_back_change (GtkWidget *widget, gpointer data)
{
	gtk_container_foreach (GTK_CONTAINER (widget),
			       (GtkCallback) back_change_foreach,
			       widget);

#ifdef FIXME_FOR_NEW_CONFIG
	/*update the configuration box if it is displayed*/
	update_config_back(PANEL_WIDGET(widget));
#endif /* FIXME_FOR_NEW_CONFIG */
}

static void
panel_applet_added(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	AppletInfo    *info;

	info = g_object_get_data (G_OBJECT (applet), "applet_info");

	orientation_change(info,PANEL_WIDGET(widget));
	back_change(info,PANEL_WIDGET(widget));
}

static gboolean
deactivate_idle (gpointer data)
{
	PanelData *pd = data;
	pd->deactivate_idle = 0;

	pd->insert_pack_type = PANEL_OBJECT_PACK_START;

	return FALSE;
}

static void
context_menu_deactivate (GtkWidget *w,
			 PanelData *pd)
{
	if (pd->deactivate_idle == 0)
		pd->deactivate_idle = g_idle_add (deactivate_idle, pd);

	panel_toplevel_pop_autohide_disabler (PANEL_TOPLEVEL (pd->panel));
}

static void
context_menu_show (GtkWidget *w,
		   PanelData *pd)
{
	panel_toplevel_push_autohide_disabler (PANEL_TOPLEVEL (pd->panel));
}

static void
panel_destroy (PanelToplevel *toplevel,
	       PanelData     *pd)
{
	if (pd->menu) {
		g_signal_handlers_disconnect_by_func (pd->menu,
						      context_menu_deactivate,
						      pd);
		g_object_unref (pd->menu);
	}
	pd->menu = NULL;

	pd->panel = NULL;

	if (pd->deactivate_idle != 0)
		g_source_remove (pd->deactivate_idle);
	pd->deactivate_idle = 0;

	g_object_set_data (G_OBJECT (toplevel), "PanelData", NULL);

	panel_list = g_slist_remove (panel_list, pd);
	g_free (pd);
}

static void
panel_applet_move(PanelWidget *panel, GtkWidget *widget, gpointer data)
{
	AppletInfo *info;

	info = g_object_get_data (G_OBJECT (widget), "applet_info");

	g_return_if_fail (info);

	panel_applet_save_position (info, info->id, FALSE);
}

static void
panel_menu_lockdown_changed (PanelLockdown *lockdown,
			     gpointer       user_data)
{
	PanelData *pd = user_data;

	if (pd->menu) {
		if (gtk_widget_get_visible (pd->menu))
			gtk_menu_shell_deactivate (GTK_MENU_SHELL (pd->menu));

		g_signal_handlers_disconnect_by_func (pd->menu,
						      context_menu_deactivate,
						      pd);

		g_object_unref (pd->menu);
		pd->menu = NULL;
	}
}

static GtkWidget *
panel_menu_get (PanelWidget *panel, PanelData *pd)
{
	if (!pd->menu) {
		pd->menu = panel_context_menu_create (panel);
		if (pd->menu != NULL) {
			g_object_ref_sink (pd->menu);
			g_signal_connect (pd->menu, "deactivate",
					  G_CALLBACK (context_menu_deactivate),
					  pd);
			g_signal_connect (pd->menu, "show",
					  G_CALLBACK (context_menu_show), pd);

			panel_lockdown_on_notify (panel_lockdown_get (),
						  NULL,
						  G_OBJECT (pd->menu),
						  panel_menu_lockdown_changed,
						  pd);
		}
	}

	return pd->menu;
}

static GtkWidget *
make_popup_panel_menu (PanelWidget *panel_widget)
{
	PanelData *pd;
	GtkWidget *menu;

	if (!panel_widget) {
		PanelToplevel *toplevel;

		toplevel = PANEL_TOPLEVEL (((PanelData *) panel_list->data)->panel);

		panel_widget = panel_toplevel_get_panel_widget (toplevel);
	}

	pd = g_object_get_data (G_OBJECT (panel_widget->toplevel), "PanelData");
	menu = panel_menu_get (panel_widget, pd);
	if (menu)
		g_object_set_data (G_OBJECT (menu), "menu_panel", panel_widget);

	return menu;
}

static gboolean
panel_popup_menu (PanelToplevel *toplevel,
		  guint          button,
		  guint32        activate_time)
{
	PanelWidget *panel_widget;
	GtkWidget   *menu;
	PanelData   *panel_data;
	GdkEvent    *current_event;

	panel_widget = panel_toplevel_get_panel_widget (toplevel);
	panel_data   = g_object_get_data (G_OBJECT (toplevel), "PanelData");

	current_event = gtk_get_current_event ();
	if (current_event->type == GDK_BUTTON_PRESS)
		panel_data->insert_pack_type = panel_widget_get_insert_pack_type_at_cursor (panel_widget);
	else
		panel_data->insert_pack_type = PANEL_OBJECT_PACK_START;
	
	menu = make_popup_panel_menu (panel_widget);
	if (!menu)
		return FALSE;

	gtk_menu_set_screen (GTK_MENU (menu),
			     gtk_window_get_screen (GTK_WINDOW (toplevel)));

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, activate_time);

	return TRUE;
}

static gboolean
panel_button_press_event (PanelToplevel  *toplevel,
			  GdkEventButton *event)
{
	guint modifiers;

	if (event->button != 3)
		return FALSE;

	modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

	if (modifiers == panel_bindings_get_mouse_button_modifier_keymask ())
		return panel_popup_menu (toplevel, event->button, event->time);

	return FALSE;
}

static gboolean
panel_key_press_event (PanelToplevel *toplevel,
		       GdkEventKey   *event)
{
	gboolean is_popup = FALSE;

	/* We're not connecting to the popup-menu signal since we want to be
	 * able to handle keybinding of popup-menu + modifier from metacity */

	panel_util_key_event_is_popup (event, NULL, &is_popup);
	if (!is_popup)
		panel_util_key_event_is_popup_panel (event, &is_popup, NULL);

	if (is_popup)
		return panel_popup_menu (toplevel, 3, event->time);

	return FALSE;
}

static GSettings *
get_settings_background_for_toplevel (PanelToplevel *toplevel)
{
	char      *toplevel_settings_path;
	GSettings *settings;
	GSettings *settings_background;

	g_object_get (toplevel, "settings-path", &toplevel_settings_path, NULL);
	settings = g_settings_new_with_path (PANEL_TOPLEVEL_SCHEMA,
					     toplevel_settings_path);
	settings_background = g_settings_get_child (settings,
						    PANEL_BACKGROUND_SCHEMA_CHILD);

	g_object_unref (settings);
	g_free (toplevel_settings_path);

	return settings_background;
}

static gboolean
set_background_image_from_uri (PanelToplevel *toplevel,
			       const char    *uri)
{
	GFile     *file;
	GSettings *settings;

	file = g_file_new_for_uri (uri);
	if (!g_file_is_native (file)) {
		g_object_unref (file);
		return FALSE;
	}
	g_object_unref (file);

	settings = get_settings_background_for_toplevel (toplevel);

	if (!g_settings_is_writable (settings,
				     PANEL_BACKGROUND_TYPE_KEY) ||
	    !g_settings_is_writable (settings,
				     PANEL_BACKGROUND_IMAGE_URI_KEY)) {
		g_object_unref (settings);
		return FALSE;
	}

	g_settings_set_string (settings, PANEL_BACKGROUND_IMAGE_URI_KEY, uri);
	g_settings_set_enum (settings, PANEL_BACKGROUND_TYPE_KEY, PANEL_BACK_IMAGE);

	g_object_unref (settings);

	return FALSE;
}

static gboolean
set_background_color (PanelToplevel *toplevel,
		      guint16       *dropped)
{
	GSettings *settings;
	GdkRGBA    color;
	char      *color_str;

	if (!dropped)
		return FALSE;

	settings = get_settings_background_for_toplevel (toplevel);

	if (!g_settings_is_writable (settings,
				     PANEL_BACKGROUND_TYPE_KEY) ||
	    !g_settings_is_writable (settings,
				     PANEL_BACKGROUND_COLOR_KEY)) {
		g_object_unref (settings);
		return FALSE;
	}

	color.red   = dropped [0] / 65535.;
	color.green = dropped [1] / 65535.;
	color.blue  = dropped [2] / 65535.;
	color.alpha = 1.;

	color_str = gdk_rgba_to_string (&color);

	g_settings_set_string (settings, PANEL_BACKGROUND_COLOR_KEY, color_str);
	g_settings_set_enum (settings, PANEL_BACKGROUND_TYPE_KEY, PANEL_BACK_COLOR);

	g_free (color_str);
	g_object_unref (settings);

	return TRUE;
}

static gboolean
reset_background (PanelToplevel *toplevel)
{
	GSettings *settings;

	settings = get_settings_background_for_toplevel (toplevel);

	if (!g_settings_is_writable (settings,
				     PANEL_BACKGROUND_TYPE_KEY)) {
		g_object_unref (settings);
		return FALSE;
	}

	g_settings_set_enum (settings, PANEL_BACKGROUND_TYPE_KEY, PANEL_BACK_NONE);

	g_object_unref (settings);

	return TRUE;
}

static gboolean
drop_url (PanelWidget         *panel,
	  PanelObjectPackType  pack_type,
	  int                  pack_index,
	  const char          *url)
{
	enum {
		NETSCAPE_URL_URL,
		NETSCAPE_URL_NAME
	};
	char **netscape_url;
	char  *name;
	char  *comment;

	g_return_val_if_fail (url != NULL, FALSE);

	if (!panel_layout_is_writable ())
		return FALSE;

	netscape_url = g_strsplit (url, "\n", 2);
	if (!netscape_url ||
	    PANEL_GLIB_STR_EMPTY (netscape_url[NETSCAPE_URL_URL])) {
		g_strfreev (netscape_url);
		return FALSE;
	}
	
	comment = g_strdup_printf (_("Open URL: %s"),
				   netscape_url[NETSCAPE_URL_URL]);

	if (PANEL_GLIB_STR_EMPTY (netscape_url[NETSCAPE_URL_NAME]))
		name = netscape_url[NETSCAPE_URL_URL];
	else
		name = netscape_url[NETSCAPE_URL_NAME];

	panel_launcher_create_from_info (panel->toplevel, pack_type, pack_index,
					 FALSE,
					 netscape_url[NETSCAPE_URL_URL],
					 name, comment, PANEL_ICON_REMOTE);

	g_free (comment);
	g_strfreev (netscape_url);

	return TRUE;
}

static gboolean
drop_menu (PanelWidget         *panel,
	   PanelObjectPackType  pack_type,
	   int                  pack_index,
	   const char          *menu_filename,
	   const char          *menu_path)
{
	if (!panel_layout_is_writable ())
		return FALSE;

	return panel_menu_button_create (panel->toplevel,
					 pack_type, pack_index,
					 menu_filename,
					 menu_path,
					 NULL);

}

static gboolean
drop_uri (PanelWidget         *panel,
	  PanelObjectPackType  pack_type,
	  int                  pack_index,
	  const char          *uri,
	  const char          *fallback_icon)
{
	char  *name;
	char  *comment;
	char  *buf;
	char  *icon;
	GFile *file;

	if (!panel_layout_is_writable ())
		return FALSE;

	name = panel_util_get_label_for_uri (uri);
	icon = panel_util_get_icon_for_uri (uri);
	if (!icon)
		icon = g_strdup (fallback_icon);

	/* FIXME: we might get icons like "folder-music" that might not exist in
	 * the icon theme. This would usually be okay if we could use fallback
	 * icons (and get "folder" this way). However, this is not possible for
	 * launchers: this could be an application that uses an icon named
	 * folder-magic-app, for which we don't want fallbacks. We just want to
	 * go to hicolor. */

	file = g_file_new_for_uri (uri);
	buf = g_file_get_parse_name (file);
	g_object_unref (file);
	/* Translators: %s is a URI */
	comment = g_strdup_printf (_("Open '%s'"), buf);
	g_free (buf);

	panel_launcher_create_from_info (panel->toplevel, pack_type, pack_index,
					 FALSE, uri, name, comment, icon);

	g_free (name);
	g_free (comment);
	g_free (icon);

	return TRUE;
}

static gboolean
drop_nautilus_desktop_uri (PanelWidget         *panel,
			   PanelObjectPackType  pack_type,
			   int                  pack_index,
			   const char          *uri)
{
	gboolean    success;
	const char *basename;

	if (g_ascii_strncasecmp (uri, "x-nautilus-desktop:///",
				 strlen ("x-nautilus-desktop:///")) != 0)
			return FALSE;

	success = TRUE;
	basename = uri + strlen ("x-nautilus-desktop:///");

	if (strncmp (basename, "trash", strlen ("trash")) == 0)
		panel_applet_frame_create (panel->toplevel, pack_type, pack_index,
					   "OAFIID:GNOME_Panel_TrashApplet");
	else if (strncmp (basename, "home", strlen ("home")) == 0) {
		char  *name;
		char  *uri;
		GFile *file;

		file = g_file_new_for_path (g_get_home_dir ());
		uri = g_file_get_uri (file);
		name = panel_util_get_label_for_uri (uri);
		g_free (uri);
		g_object_unref (file);

		panel_launcher_create_from_info (panel->toplevel,
						 pack_type, pack_index,
						 TRUE, /* is_exec? */
						 "nautilus --no-desktop", /* exec */
						 name, /* name */
						 _("Open your personal folder"), /* comment */
						 PANEL_ICON_HOME); /* icon name */

		g_free (name);
	} else if (strncmp (basename, "computer", strlen ("computer")) == 0)
		panel_launcher_create_from_info (panel->toplevel,
						 pack_type, pack_index,
						 TRUE, /* is_exec? */
						 "nautilus --no-desktop computer://", /* exec */
						 _("Computer"), /* name */
						 _("Browse all local and remote disks and folders accessible from this computer"), /* comment */
						 PANEL_ICON_COMPUTER); /* icon name */
	else if (strncmp (basename, "network", strlen ("network")) == 0)
		panel_launcher_create_from_info (panel->toplevel,
						 pack_type, pack_index,
						 TRUE, /* is_exec? */
						 "nautilus --no-desktop network://", /* exec */
						 _("Network"), /* name */
						 _("Browse bookmarked and local network locations"), /* comment */
						 PANEL_ICON_NETWORK); /* icon name */
	else
		success = FALSE;

	return success;
}

static gboolean
drop_urilist (PanelWidget         *panel,
	      PanelObjectPackType  pack_type,
	      int                  pack_index,
	      char                *urilist)
{
	char     **uris;
	GFile     *home;
	GFile     *trash;
	GFile     *computer;
	GFile     *network;
	gboolean   success;
	int        i;

	uris = g_uri_list_extract_uris (urilist);
	home = g_file_new_for_path (g_get_home_dir ());
	trash = g_file_new_for_uri ("trash://");
	computer = g_file_new_for_uri ("computer://");
	network = g_file_new_for_uri ("network://");

	success = TRUE;
	for (i = 0; uris[i]; i++) {
		GFile      *file;
		GFileInfo  *info;
		const char *uri;

		uri = uris[i];

		if (g_ascii_strncasecmp (uri, "http:", strlen ("http:")) == 0 ||
		    g_ascii_strncasecmp (uri, "https:", strlen ("https:")) == 0 ||
		    g_ascii_strncasecmp (uri, "ftp:", strlen ("ftp:")) == 0 ||
		    g_ascii_strncasecmp (uri, "gopher:", strlen ("gopher:")) == 0 ||
		    g_ascii_strncasecmp (uri, "ghelp:", strlen ("ghelp:")) == 0 ||
		    g_ascii_strncasecmp (uri, "help:", strlen ("help:")) == 0 ||
		    g_ascii_strncasecmp (uri, "man:", strlen ("man:")) == 0 ||
		    g_ascii_strncasecmp (uri, "info:", strlen ("info:")) == 0) {
			/* FIXME: probably do this only on link,
			 * in fact, on link always set up a link,
			 * on copy do all the other stuff.  Or something. */
			if ( ! drop_url (panel, pack_type, pack_index, uri))
				success = FALSE;
			continue;
		}

		if (g_ascii_strncasecmp (uri, "x-nautilus-desktop:",
					 strlen ("x-nautilus-desktop:")) == 0) {
			success = drop_nautilus_desktop_uri (panel,
							     pack_type, pack_index,
							     uri);
			continue;
		}

		file = g_file_new_for_uri (uri);

		if (g_file_equal (home, file)) {
			success = drop_nautilus_desktop_uri (panel,
							     pack_type, pack_index,
							     "x-nautilus-desktop:///home");
			g_object_unref (file);
			continue;
		} else if (g_file_equal (trash, file)) {
			success = drop_nautilus_desktop_uri (panel,
							     pack_type, pack_index,
							     "x-nautilus-desktop:///trash");
			g_object_unref (file);
			continue;
		} else if (g_file_equal (computer, file)) {
			success = drop_nautilus_desktop_uri (panel,
							     pack_type, pack_index,
							     "x-nautilus-desktop:///computer");
			g_object_unref (file);
			continue;
		} else if (g_file_equal (network, file)) {
			success = drop_nautilus_desktop_uri (panel,
							     pack_type, pack_index,
							     "x-nautilus-desktop:///network");
			g_object_unref (file);
			continue;
		}

		info = g_file_query_info (file,
					  "standard::type,"
					  "standard::content-type,"
					  "access::can-execute",
					  G_FILE_QUERY_INFO_NONE,
					  NULL, NULL);

		if (info) {
			const char *mime;
			GFileType   type;
			gboolean    can_exec;

			mime = g_file_info_get_content_type (info);
			type = g_file_info_get_file_type (info);
			can_exec = g_file_info_get_attribute_boolean (info,
								      G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE);

			if (mime &&
			    g_str_has_prefix (mime, "image")) {
				if (!set_background_image_from_uri (panel->toplevel, uri))
					success = FALSE;
			} else if (mime &&
				   (!strcmp (mime, "application/x-gnome-app-info") ||
				    !strcmp (mime, "application/x-desktop") ||
				    !strcmp (mime, "application/x-kde-app-info"))) {
				if (panel_layout_is_writable ())
					panel_launcher_create (panel->toplevel,
							       pack_type, pack_index,
							       uri);
				else
					success = FALSE;
			} else if (type != G_FILE_TYPE_DIRECTORY && can_exec) {
				char *filename;

				filename = g_file_get_path (file);

				if (panel_layout_is_writable ())
					/* executable and local, so add a
					 * launcher with it */
					ask_about_launcher (filename, panel,
							    pack_type);
				else
					success = FALSE;
				g_free (filename);
			} else {
				if (!drop_uri (panel, pack_type, pack_index, uri,
					       PANEL_ICON_UNKNOWN))
					success = FALSE;
			}
		} else {
			if (!drop_uri (panel, pack_type, pack_index,
				       uri, PANEL_ICON_UNKNOWN))
				success = FALSE;
		}

		g_object_unref (info);
		g_object_unref (file);
	}

	g_object_unref (home);
	g_object_unref (trash);
	g_object_unref (computer);
	g_object_unref (network);
	g_strfreev (uris);

	return success;
}

static gboolean
drop_internal_icon (PanelWidget         *panel,
		    PanelObjectPackType  pack_type,
		    int                  pack_index,
		    const char          *icon_name,
		    int                  action)
{
	Launcher *old_launcher = NULL;

	if (!icon_name)
		return FALSE;

	if (!panel_layout_is_writable ())
		return FALSE;

	if (action == GDK_ACTION_MOVE)
		old_launcher = find_launcher (icon_name);
	
	if (!panel_launcher_create_copy (panel->toplevel, pack_type, pack_index,
					 icon_name))
		return FALSE;

	if (old_launcher && old_launcher->button) {
		const char *object_id;

		if (old_launcher->prop_dialog) {
			g_signal_handler_disconnect (old_launcher->button,
						     old_launcher->destroy_handler);
			launcher_properties_destroy (old_launcher);
		}

		object_id = panel_applet_get_id (old_launcher->info);
		panel_layout_delete_object (object_id);
	}

	return TRUE;
}

static gboolean
move_applet (PanelWidget         *panel,
	     PanelObjectPackType  pack_type,
	     int                  pack_index,
	     int                  applet_index)
{
	GSList     *applet_list;
	AppletInfo *info;
	GtkWidget  *parent;

	applet_list = panel_applet_list_applets ();

	info = g_slist_nth_data (applet_list, applet_index);

	if (info == NULL || info->widget == NULL ||
	    !panel_applet_can_freely_move (info))
		return FALSE;

	parent = gtk_widget_get_parent (info->widget);

	if (parent != NULL &&
	    PANEL_IS_WIDGET (parent)) {
		panel_widget_reparent (PANEL_WIDGET (parent),
				       panel,
				       info->widget,
				       pack_type, pack_index);
	}

	return TRUE;
}

static gboolean
drop_internal_applet (PanelWidget         *panel,
		      PanelObjectPackType  pack_type,
		      int                  pack_index,
		      const char          *applet_type,
		      int                  action)
{
	int applet_index = -1;
	gboolean remove_applet = FALSE;
	gboolean success = FALSE;

	if (applet_type == NULL)
		return FALSE;

	if (sscanf (applet_type, "MENU:%d", &applet_index) == 1) {
		if (action != GDK_ACTION_MOVE)
			g_warning ("Only MOVE supported for menus");
		success = move_applet (panel, pack_type, pack_index, applet_index);

	} else if (strncmp (applet_type, "MENU:", strlen ("MENU:")) == 0) {
		const char *menu;
		const char *menu_path;

		menu = &applet_type[strlen ("MENU:")];
		menu_path = strchr (menu, '/');

		if (!menu_path) {
			if (strncmp (menu, "MAIN", strlen ("MAIN")) == 0)
				success = drop_menu (panel, pack_type, pack_index,
						     NULL, NULL);
			else
				success = drop_menu (panel, pack_type, pack_index,
						     menu, NULL);
		} else {
			char *menu_filename;

			menu_filename = g_strndup (menu, menu_path - menu);
			menu_path++;
			success = drop_menu (panel, pack_type, pack_index,
					     menu_filename, menu_path);
			g_free (menu_filename);
		}

	} else if (!strncmp (applet_type, "ACTION:", strlen ("ACTION:"))) {
		if (panel_layout_is_writable ()) {
			remove_applet = panel_action_button_load_from_drag (
							panel->toplevel,
							pack_type, pack_index,
							applet_type,
							&applet_index);
			success = TRUE;
		} else {
			success = FALSE;
		}

	} else if (!strcmp (applet_type, "MENUBAR:NEW")) {
		if (panel_layout_is_writable ()) {
			panel_menu_bar_create (panel->toplevel,
					       pack_type, pack_index);
			success = TRUE;
		} else {
			success = FALSE;
		}

	} else if (!strcmp(applet_type,"SEPARATOR:NEW")) {
		if (panel_layout_is_writable ()) {
			panel_separator_create (panel->toplevel,
						pack_type, pack_index);
			success = TRUE;
		} else {
			success = FALSE;
		}

	} else if (!strcmp (applet_type, "USERMENU:NEW")) {
		if (panel_layout_is_writable ()) {
			panel_user_menu_create (panel->toplevel,
						pack_type, pack_index);
			success = TRUE;
		} else {
			success = FALSE;
		}

	} else if (!strcmp(applet_type,"LAUNCHER:ASK")) {
		if (panel_layout_is_writable ()) {
			ask_about_launcher (NULL, panel, pack_type);
			success = TRUE;
		} else {
			success = FALSE;
		}
	}

	if (remove_applet &&
	    action == GDK_ACTION_MOVE) {
		AppletInfo *info;
		GSList     *applet_list;

		applet_list = panel_applet_list_applets ();

		info = g_slist_nth_data (applet_list, applet_index);

		if (info)
			panel_layout_delete_object (panel_applet_get_id (info));
	}

	return success;
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
panel_check_dnd_target_data (GtkWidget      *widget,
			     GdkDragContext *context,
			     guint          *ret_info,
			     GdkAtom        *ret_atom)
{
	GList *l;

	g_return_val_if_fail (widget, FALSE);

	if (!PANEL_IS_TOPLEVEL  (widget) &&
	    !BUTTON_IS_WIDGET (widget))
		return FALSE;

	if (!(gdk_drag_context_get_actions (context) & (GDK_ACTION_COPY|GDK_ACTION_MOVE)))
		return FALSE;

	for (l = gdk_drag_context_list_targets (context); l; l = l->next) {
		GdkAtom atom;
		guint   info;

		atom = GDK_POINTER_TO_ATOM (l->data);

		if (gtk_target_list_find (get_target_list (), atom, &info)) {
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

	/* FIXME: what's going on here ? How are we highlighting
	 *        the toplevel widget ? I don't think we are ...
	 */

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
panel_check_drop_forbidden (PanelWidget    *panel,
			    GdkDragContext *context,
			    guint           info,
			    guint           time_)
{
	if (!panel)
		return FALSE;

	if (panel_lockdown_get_panels_locked_down_s ())
		return FALSE;

	if (info == TARGET_ICON_INTERNAL ||
	    info == TARGET_APPLET_INTERNAL) {
		if (gdk_drag_context_get_actions (context) & GDK_ACTION_MOVE)
			gdk_drag_status (context, GDK_ACTION_MOVE, time_);
		else
			gdk_drag_status (context,
					 gdk_drag_context_get_suggested_action (context),
					 time_);

	} else if (gdk_drag_context_get_actions (context) & GDK_ACTION_COPY)
		gdk_drag_status (context, GDK_ACTION_COPY, time_);
	else
		gdk_drag_status (context,
				 gdk_drag_context_get_suggested_action (context),
				 time_);

	return TRUE;

}

static gboolean
drag_motion_cb (GtkWidget	   *widget,
		GdkDragContext     *context,
		gint                x,
		gint                y,
		guint               time)
{
	PanelToplevel *toplevel;
	PanelWidget   *panel_widget;
	guint          info;

	g_return_val_if_fail (PANEL_IS_TOPLEVEL (widget), FALSE);

	if (!panel_check_dnd_target_data (widget, context, &info, NULL))
		return FALSE;

	toplevel = PANEL_TOPLEVEL (widget);
	panel_widget = panel_toplevel_get_panel_widget (toplevel);

	if (!panel_check_drop_forbidden (panel_widget, context, info, time))
		return FALSE;

	do_highlight (widget, TRUE);

	panel_toplevel_unhide (toplevel);

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
	GdkAtom ret_atom = NULL;

	if (!panel_check_dnd_target_data (widget, context, NULL, &ret_atom))
		return FALSE;

	gtk_drag_get_data (widget, context, ret_atom, time);

	return TRUE;
}

static void  
drag_leave_cb (GtkWidget	*widget,
	       GdkDragContext   *context,
	       guint             time,
	       Launcher         *launcher)
{
	PanelToplevel *toplevel;

	do_highlight (widget, FALSE);

	toplevel = PANEL_TOPLEVEL (widget);
	panel_toplevel_queue_auto_hide (toplevel);
}

static void
panel_receive_dnd_data (PanelWidget         *panel,
			guint                info,
			PanelObjectPackType  pack_type,
			int                  pack_index,
			GtkSelectionData    *selection_data,
			GdkDragContext      *context,
			guint                time_)
{
	const guchar *data;
	gboolean      success = FALSE;

	if (panel_lockdown_get_panels_locked_down_s ()) {
		gtk_drag_finish (context, FALSE, FALSE, time_);
		return;
	}

	data = gtk_selection_data_get_data (selection_data);

	switch (info) {
	case TARGET_URL:
		success = drop_urilist (panel, pack_type, pack_index, (char *)data);
		break;
	case TARGET_NETSCAPE_URL:
		success = drop_url (panel, pack_type, pack_index, (char *)data);
		break;
	case TARGET_COLOR:
		success = set_background_color (panel->toplevel, (guint16 *) data);
		break;
	case TARGET_BGIMAGE:
		success = set_background_image_from_uri (panel->toplevel, (char *) data);
		break;
	case TARGET_BACKGROUND_RESET:
		success = reset_background (panel->toplevel);
		break;
	case TARGET_DIRECTORY:
		success = drop_uri (panel, pack_type, pack_index, (char *)data,
				    PANEL_ICON_FOLDER);
		break;
	case TARGET_APPLET:
		if (!gtk_selection_data_get_data (selection_data)) {
			gtk_drag_finish (context, FALSE, FALSE, time_);
			return;
		}
		if (panel_layout_is_writable ()) {
			panel_applet_frame_create (panel->toplevel,
						   pack_type, pack_index,
						   (char *) data);
			success = TRUE;
		} else {
			success = FALSE;
		}
		break;
	case TARGET_APPLET_INTERNAL:
		success = drop_internal_applet (panel, pack_type, pack_index,
						(char *)data,
						gdk_drag_context_get_selected_action (context));
		break;
	case TARGET_ICON_INTERNAL:
		success = drop_internal_icon (panel, pack_type, pack_index,
					      (char *)data,
					      gdk_drag_context_get_selected_action (context));
		break;
	default:
		gtk_drag_finish (context, FALSE, FALSE, time_);
		return;
	}

	gtk_drag_finish (context, success, FALSE, time_);
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
	PanelWidget         *panel_widget;
	PanelObjectPackType  pack_type = PANEL_OBJECT_PACK_START;
	int                  pack_index = 0;

	g_return_if_fail (PANEL_IS_TOPLEVEL (widget));

	/* we use this only to really find out the info, we already
	   know this is an ok drop site and the info that got passed
	   to us is bogus (it's always 0 in fact) */
	if (!panel_check_dnd_target_data (widget, context, &info, NULL)) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	panel_widget = panel_toplevel_get_panel_widget (PANEL_TOPLEVEL (widget));

	panel_widget_get_insert_at_cursor (panel_widget, &pack_type, &pack_index);
	
	panel_receive_dnd_data (
		panel_widget, info, pack_type, pack_index,
		selection_data, context, time);
}

static void
panel_widget_setup(PanelWidget *panel)
{
	g_signal_connect (G_OBJECT(panel),
			  "applet_added",
			  G_CALLBACK(panel_applet_added),
			  NULL);
	g_signal_connect (G_OBJECT(panel),
			  "applet_move",
			  G_CALLBACK(panel_applet_move),
			  NULL);
	g_signal_connect (G_OBJECT (panel),
			  "back_change",
			  G_CALLBACK (panel_back_change),
			  NULL);
}

PanelData *
panel_setup (PanelToplevel *toplevel)
{
	PanelWidget *panel_widget;
	PanelData   *pd;

	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), NULL);

	panel_widget = panel_toplevel_get_panel_widget (toplevel);

	pd = g_new0 (PanelData,1);
	pd->menu = NULL;
	pd->panel = GTK_WIDGET (toplevel);
	pd->insert_pack_type = PANEL_OBJECT_PACK_START;
	pd->deactivate_idle = 0;

	panel_list = g_slist_append (panel_list, pd);
	
	g_object_set_data (G_OBJECT (toplevel), "PanelData", pd);

	panel_widget_setup (panel_widget);

	g_signal_connect (toplevel, "drag_data_received",
			  G_CALLBACK (drag_data_recieved_cb), NULL);
	g_signal_connect (toplevel, "drag_motion",
			  G_CALLBACK (drag_motion_cb), NULL);
	g_signal_connect (toplevel, "drag_leave",
			  G_CALLBACK (drag_leave_cb), NULL);
	g_signal_connect (toplevel, "drag_drop",
			  G_CALLBACK (drag_drop_cb), NULL);

	gtk_drag_dest_set (GTK_WIDGET (toplevel), 0, NULL, 0, 0);

	g_signal_connect (toplevel, "key-press-event",
			  G_CALLBACK (panel_key_press_event), NULL);
	g_signal_connect (toplevel, "button-press-event",
			  G_CALLBACK (panel_button_press_event), NULL);

	g_signal_connect_swapped (toplevel, "notify::orientation",
				  G_CALLBACK (panel_orient_change), panel_widget);
 
	g_signal_connect (toplevel, "destroy", G_CALLBACK (panel_destroy), pd);

	return pd;
}

GdkScreen *
panel_screen_from_panel_widget (PanelWidget *panel)
{
	g_return_val_if_fail (PANEL_IS_WIDGET (panel), NULL);
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (panel->toplevel), NULL);

	return gtk_window_get_screen (GTK_WINDOW (panel->toplevel));
}

static void
panel_delete_without_query (PanelToplevel *toplevel)
{
	panel_layout_delete_toplevel (panel_toplevel_get_id (toplevel));
} 

static void
panel_deletion_response (GtkWidget     *dialog,
			 int            response,
			 PanelToplevel *toplevel)
{
	if (response == GTK_RESPONSE_OK) {
		panel_push_window_busy (dialog);
		panel_delete_without_query (toplevel);
		panel_pop_window_busy (dialog);
	}

	gtk_widget_destroy (dialog);
}

static void
panel_deletion_destroy_dialog (GtkWidget *widget,
			       PanelToplevel *toplevel)
{
	panel_toplevel_pop_autohide_disabler (toplevel);
	g_object_set_data (G_OBJECT (toplevel), "panel-delete-dialog", NULL);
}

GtkWidget *
panel_deletion_dialog (PanelToplevel *toplevel)
{

	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
			GTK_WINDOW (toplevel),
			GTK_DIALOG_MODAL,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_NONE,
			"%s", _("Delete this panel?"));
	
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
	                                          "%s",
						  _("When a panel is deleted, the panel and its\n"
						  "settings are lost."));

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_Delete"), GTK_RESPONSE_OK,
				NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_window_set_screen (GTK_WINDOW (dialog),
				gtk_window_get_screen (GTK_WINDOW (toplevel)));

	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

	 g_signal_connect (dialog, "destroy",
                           G_CALLBACK (panel_deletion_destroy_dialog),
                           toplevel);

	g_object_set_data (G_OBJECT (toplevel), "panel-delete-dialog", dialog);
	panel_toplevel_push_autohide_disabler (toplevel);

	return dialog;
}

static void
panel_query_deletion (PanelToplevel *toplevel)
{
	GtkWidget *dialog;

	dialog = g_object_get_data (G_OBJECT (toplevel), "panel-delete-dialog");

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog));
		return;
	}

	dialog = panel_deletion_dialog (toplevel);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (panel_deletion_response),
			  toplevel);

	g_signal_connect_object (toplevel, "destroy",
				 G_CALLBACK (gtk_widget_destroy),
				 dialog,
				 G_CONNECT_SWAPPED);

	gtk_widget_show_all (dialog);
}

void
panel_delete (PanelToplevel *toplevel)
{
	GSettings   *settings;
	gboolean     confirm;
	PanelWidget *panel_widget;

	settings = g_settings_new (PANEL_GENERAL_SCHEMA);
	confirm = g_settings_get_boolean (settings,
					  PANEL_GENERAL_CONFIRM_PANEL_REMOVAL_KEY);
	g_object_unref (settings);

	panel_widget = panel_toplevel_get_panel_widget (toplevel);

	if (!confirm ||
	    !g_list_length (panel_widget->applet_list)) {
		panel_delete_without_query (toplevel);
		return;
	}

	panel_query_deletion (toplevel);
}
