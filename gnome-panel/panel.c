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
#include "drawer.h"
#include "button-widget.h"
#include "distribution.h"
#include "launcher.h"
#include "menu-fentry.h"
#include "menu-util.h"
#include "menu.h"
#include "panel-util.h"
#include "panel-config-global.h"
#include "panel-gconf.h"
#include "panel-profile.h"
#include "panel-applet-frame.h"
#include "panel-action-button.h"
#include "panel-menu-bar.h"
#include "panel-compatibility.h"
#include "panel-multiscreen.h"
#include "panel-toplevel.h"
#include "panel-menu-button.h"
#include "panel-globals.h"
#include "panel-lockdown.h"

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
	case PANEL_OBJECT_BONOBO:
		panel_applet_frame_change_orientation (
				PANEL_APPLET_FRAME (info->widget), orientation);
		break;
	case PANEL_OBJECT_MENU:
	case PANEL_OBJECT_LAUNCHER:
	case PANEL_OBJECT_ACTION:
		button_widget_set_orientation (BUTTON_WIDGET (info->widget), orientation);
		break;
	case PANEL_OBJECT_DRAWER: {
		Drawer      *drawer = info->data;
		PanelWidget *panel_widget;

		panel_widget = panel_toplevel_get_panel_widget (drawer->toplevel);

		button_widget_set_orientation (BUTTON_WIDGET (info->widget), orientation);

		gtk_widget_queue_resize (GTK_WIDGET (drawer->toplevel));
		gtk_container_foreach (GTK_CONTAINER (panel_widget),
				       orient_change_foreach,
				       panel_widget);
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
}

/*we call this recursively*/
static void size_change_foreach(GtkWidget *w, gpointer data);

void
size_change (AppletInfo  *info,
	     PanelWidget *panel)
{
	if (info->type == PANEL_OBJECT_BONOBO)
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
}

void
back_change (AppletInfo  *info,
	     PanelWidget *panel)
{
	if (info->type == PANEL_OBJECT_BONOBO)
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

#ifdef FIXME_FOR_NEW_CONFIG
	/*update the configuration box if it is displayed*/
	update_config_back(PANEL_WIDGET(widget));
#endif /* FIXME_FOR_NEW_CONFIG */
}

static void
panel_applet_added(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	PanelToplevel *toplevel;
	AppletInfo    *info;

	toplevel = PANEL_WIDGET (widget)->toplevel;
	info = g_object_get_data (G_OBJECT (applet), "applet_info");

	orientation_change(info,PANEL_WIDGET(widget));
	size_change(info,PANEL_WIDGET(widget));
	back_change(info,PANEL_WIDGET(widget));
}

static void
panel_applet_removed(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	PanelToplevel *toplevel;
	AppletInfo    *info;

	toplevel = PANEL_WIDGET (widget)->toplevel;
	info = g_object_get_data (G_OBJECT (applet), "applet_info");

	if (info->type == PANEL_OBJECT_DRAWER) {
		Drawer *drawer = info->data;

		if (drawer->toplevel)
			panel_toplevel_queue_auto_hide (toplevel);
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
	if (pd->deactivate_idle == 0)
		pd->deactivate_idle = g_idle_add (deactivate_idle, pd);

	panel_toplevel_unblock_auto_hide (PANEL_TOPLEVEL (pd->panel));
}

static void
panel_recreate_context_menu (PanelData *pd)
{
	if (pd->menu)
		g_object_unref (pd->menu);
	pd->menu = NULL;
}

static void
panel_destroy (PanelToplevel *toplevel,
	       PanelData     *pd)
{
	PanelWidget *panel_widget;

	panel_widget = panel_toplevel_get_panel_widget (toplevel);

	panel_lockdown_notify_remove (G_CALLBACK (panel_recreate_context_menu),
				      pd);

	if (pd->menu)
		g_object_unref (pd->menu);
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

static GtkWidget *
panel_menu_get (PanelWidget *panel, PanelData *pd)
{
	if (!pd->menu) {
		pd->menu = g_object_ref (create_panel_context_menu (panel));
		gtk_object_sink (GTK_OBJECT (pd->menu));
		g_signal_connect (pd->menu, "deactivate",
				  G_CALLBACK (menu_deactivate), pd);
	}

	return pd->menu;
}

GtkWidget *
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
	if (current_event->type == GDK_BUTTON_PRESS) {
		int x = -1, y = 1;
	
		gtk_widget_get_pointer (GTK_WIDGET (panel_widget), &x, &y);

		panel_data->insertion_pos = panel_widget->orient == GTK_ORIENTATION_HORIZONTAL ? x : y;
	} else
		panel_data->insertion_pos = -1;
	
	menu = make_popup_panel_menu (panel_widget);

	panel_toplevel_block_auto_hide (toplevel);

	gtk_menu_set_screen (GTK_MENU (menu),
			     gtk_window_get_screen (GTK_WINDOW (toplevel)));

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			panel_menu_position, toplevel, button, activate_time);

	return TRUE;
}

static gboolean
panel_popup_menu_signal (PanelToplevel *toplevel)
{
	return panel_popup_menu (toplevel, 3, GDK_CURRENT_TIME);
}

static gboolean
panel_button_press_event (PanelToplevel  *toplevel,
			  GdkEventButton *event)
{
	if (event->button != 3)
		return FALSE;

	return panel_popup_menu (toplevel, event->button, event->time);
}

static gboolean
panel_key_press_event (GtkWidget   *widget,
		       GdkEventKey *event)
{
	/*
  	 * If the focus widget is a GtkSocket, i.e. the
	 * focus is in an applet in another process, then key 
	 * bindings do not work. We get around this by
	 * activating the key bindings here.
	 */ 
	if (GTK_IS_SOCKET (GTK_WINDOW (widget)->focus_widget) &&
	    event->keyval == GDK_F10 &&
	    (event->state & gtk_accelerator_get_default_mod_mask ()) == GDK_CONTROL_MASK)
		return gtk_bindings_activate (GTK_OBJECT (widget),
					      event->keyval,
					      event->state);

	return FALSE;
}

static gboolean
set_background_image_from_uri (PanelToplevel *toplevel,
			       const char    *uri)
{
	char *image;

	if ( ! panel_profile_is_writable_background_type (toplevel) ||
	     ! panel_profile_is_writable_background_image (toplevel))
		return FALSE;

	if (!(image = g_filename_from_uri (uri, NULL, NULL)))
		return FALSE;

	panel_profile_set_background_image (toplevel, image);
	panel_profile_set_background_type (toplevel, PANEL_BACK_IMAGE);

	g_free (image);

	return FALSE;
}

static gboolean
set_background_color (PanelToplevel *toplevel,
		      guint16       *dropped)
{
	PanelColor color;

	if (!dropped)
		return FALSE;

	if ( ! panel_profile_is_writable_background_type (toplevel) ||
	     ! panel_profile_is_writable_background_color (toplevel))
		return FALSE;

	color.gdk.red   = dropped [0];
	color.gdk.green = dropped [1];
	color.gdk.blue  = dropped [2];
	color.alpha     = 65535;

	panel_profile_set_background_color (toplevel, &color);
	panel_profile_set_background_type (toplevel, PANEL_BACK_COLOR);

	return TRUE;
}

static gboolean
drop_url (PanelWidget *panel,
	  int          position,
	  const char  *url)
{
	char *comment;

	g_return_val_if_fail (url != NULL, FALSE);

	if ( ! panel_profile_list_is_writable (PANEL_GCONF_OBJECTS))
		return FALSE;

	comment = g_strdup_printf (_("Open URL: %s"), url);

	panel_launcher_create_from_info (
		panel->toplevel, position, FALSE, url, url, comment, "gnome-globe.png");

	g_free (comment);

	return TRUE;
}

static gboolean
drop_menu (PanelWidget *panel,
	   int          position,
	   const char  *menu_path)
{
	if ( ! panel_profile_list_is_writable (PANEL_GCONF_OBJECTS))
		return FALSE;

	panel_menu_button_create (panel->toplevel, position, menu_path,
				  menu_path != NULL, menu_path);

	return TRUE;
}

static gboolean
drop_nautilus_uri (PanelWidget *panel,
		   int          position,
		   const char  *uri,
		   const char  *icon)
{
	char *quoted;
	char *exec;
	char *base;

	if ( ! panel_profile_list_is_writable (PANEL_GCONF_OBJECTS))
		return FALSE;

	/* Add -- to avoid the possibility of filenames which would
	 * be interpreted as command line arguments
	 */
	quoted = g_shell_quote (uri);
	exec = g_strdup_printf ("nautilus -- %s", quoted);
	g_free (quoted);

	base = g_path_get_basename (uri);

	panel_launcher_create_from_info (
		panel->toplevel, position, TRUE, exec, base, uri, icon);

	g_free (exec);
	g_free (base);

	return TRUE;
}

static gboolean
drop_directory (PanelWidget *panel, int pos, const char *dir)
{
	char *tmp;

	/* not filename, but path, these are uris, not local
	 * files */
	tmp = g_build_path ("/", dir, ".directory", NULL);
	if (panel_uri_exists (tmp)) {
		g_free (tmp);
		return drop_menu (panel, pos, dir);
	}
	g_free (tmp);

	tmp = g_build_path ("/", dir, ".order", NULL);
	if (panel_uri_exists (tmp)) {
		g_free (tmp);
		return drop_menu (panel, pos, dir);
	}
	g_free (tmp);

	if (panel_is_program_in_path ("nautilus")) {
		/* nautilus */
		return drop_nautilus_uri (panel, pos, dir, "gnome-fs-directory.png");
	} else {
		return drop_menu (panel, pos, dir);
	}
}

static gboolean
drop_urilist (PanelWidget *panel, int pos, char *urilist)
{
	gboolean success = TRUE;
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
			if ( ! drop_url (panel, pos, uri))
				success = FALSE;
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

		if (mimetype &&
		    !strncmp (mimetype, "image", sizeof ("image") - 1)) {
			if ( ! set_background_image_from_uri (panel->toplevel, uri))
				success = FALSE;
		} else if (basename != NULL &&
			   strcmp (basename, ".directory") == 0 &&
			   dirname != NULL) {
			/* This is definately a menu */
			char *menu_uri = g_strconcat (vfs_uri->method_string, ":",
						      dirname, NULL);
			if ( ! drop_menu (panel, pos, menu_uri))
				success = FALSE;
			g_free (menu_uri);
		} else if (mimetype != NULL &&
			   (strcmp(mimetype, "application/x-gnome-app-info") == 0 ||
			    strcmp(mimetype, "application/x-desktop") == 0 ||
			    strcmp(mimetype, "application/x-kde-app-info") == 0)) {
			if (panel_profile_list_is_writable (PANEL_GCONF_OBJECTS))
				panel_launcher_create (panel->toplevel, pos, uri);
			else
				success = FALSE;
		} else if (info != NULL &&
			   info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			if ( ! drop_directory (panel, pos, uri))
				success = FALSE;
		} else if (info != NULL &&
			   info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS &&
			   info->permissions &
			     (GNOME_VFS_PERM_USER_EXEC |
			      GNOME_VFS_PERM_GROUP_EXEC |
			      GNOME_VFS_PERM_OTHER_EXEC) &&
			   (filename = g_filename_from_uri (uri, NULL, NULL)) != NULL) {
			if (panel_profile_list_is_writable (PANEL_GCONF_OBJECTS))
				/* executable and local, so add a launcher with
				 * it */
				ask_about_launcher (filename, panel, pos, TRUE);
			else
				success = FALSE;
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
			if ( ! drop_nautilus_uri (panel, pos, uri, icon))
				success = FALSE;
		}
		if (info != NULL)
			gnome_vfs_file_info_unref (info);
		g_free (basename);
		g_free (dirname);
		g_free (uri);
	}

	gnome_vfs_uri_list_free (files);

	return success;
}

static gboolean
drop_internal_icon (PanelWidget *panel,
		    int          pos,
		    const char  *icon_name,
		    int          action)
{
	Launcher *old_launcher = NULL;

	if (!icon_name)
		return FALSE;

	if ( ! panel_profile_list_is_writable (PANEL_GCONF_OBJECTS))
		return FALSE;

	if (action == GDK_ACTION_MOVE)
		old_launcher = find_launcher (icon_name);
	
	panel_launcher_create_copy (panel->toplevel, pos, icon_name);

	if (old_launcher && old_launcher->button) {
		if (old_launcher->prop_dialog) {
			g_signal_handler_disconnect (old_launcher->button,
						     old_launcher->destroy_handler);
			launcher_properties_destroy (old_launcher);
		}
		panel_profile_delete_object (old_launcher->info);
	}

	return TRUE;
}

static gboolean
move_applet (PanelWidget *panel, int pos, int applet_index)
{
	GSList     *applet_list;
	AppletInfo *info;

	applet_list = panel_applet_list_applets ();

	info = g_slist_nth_data (applet_list, applet_index);

	if ( ! panel_applet_can_freely_move (info))
		return FALSE;

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

	return TRUE;
}

static gboolean
drop_internal_applet (PanelWidget *panel, int pos, const char *applet_type,
		      int action)
{
	int applet_index = -1;
	gboolean remove_applet = FALSE;
	gboolean success = FALSE;

	if (applet_type == NULL)
		return FALSE;

	if (sscanf (applet_type, "MENU:%d", &applet_index) == 1 ||
	    sscanf (applet_type, "DRAWER:%d", &applet_index) == 1) {
		if (action != GDK_ACTION_MOVE)
			g_warning ("Only MOVE supported for menus/drawers");
		success = move_applet (panel, pos, applet_index);

	} else if (strncmp (applet_type, "MENU:", strlen("MENU:")) == 0) {
		const char *menu = &applet_type[strlen ("MENU:")];
		if (strcmp (menu, "MAIN") == 0)
			success = drop_menu (panel, pos, NULL);
		else
			success = drop_menu (panel, pos, menu);

	} else if (!strcmp (applet_type, "DRAWER:NEW")) {
		if (panel_profile_list_is_writable (PANEL_GCONF_OBJECTS) &&
		    panel_profile_list_is_writable (PANEL_GCONF_TOPLEVELS)) {
			panel_drawer_create (panel->toplevel, pos, NULL, FALSE, NULL);
			success = TRUE;
		} else {
			success = FALSE;
		}

	} else if (!strncmp (applet_type, "ACTION:", strlen ("ACTION:"))) {
		if (panel_profile_list_is_writable (PANEL_GCONF_OBJECTS)) {
			remove_applet = panel_action_button_load_from_drag (
							panel->toplevel,
							pos,
							applet_type,
							&applet_index);
			success = TRUE;
		} else {
			success = FALSE;
		}

	} else if (!strcmp (applet_type, "MENUBAR:NEW")) {
		if (panel_profile_list_is_writable (PANEL_GCONF_OBJECTS)) {
			panel_menu_bar_create (panel->toplevel, pos);
			success = TRUE;
		} else {
			success = FALSE;
		}

	} else if (!strcmp(applet_type,"LAUNCHER:ASK")) {
		if (panel_profile_list_is_writable (PANEL_GCONF_OBJECTS)) {
			ask_about_launcher (NULL, panel, pos, TRUE);
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
			panel_profile_delete_object (info);
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

gboolean
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

	if (!(context->actions & (GDK_ACTION_COPY|GDK_ACTION_MOVE)))
		return FALSE;

	for (l = context->targets; l; l = l->next) {
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

gboolean
panel_check_drop_forbidden (PanelWidget    *panel,
			    GdkDragContext *context,
			    guint           info,
			    guint           time_)
{
	if (!panel)
		return FALSE;

	if (panel_lockdown_get_locked_down ())
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
	panel_toplevel_queue_auto_hide (toplevel);

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
	do_highlight (widget, FALSE);
}

void
panel_receive_dnd_data (PanelWidget      *panel,
			guint             info,
			int               pos,
			GtkSelectionData *selection_data,
			GdkDragContext   *context,
			guint             time_)
{
	gboolean success = FALSE;

	if (panel_lockdown_get_locked_down ()) {
		gtk_drag_finish (context, FALSE, FALSE, time_);
		return;
	}

	switch (info) {
	case TARGET_URL:
		success = drop_urilist (panel, pos, (char *)selection_data->data);
		break;
	case TARGET_NETSCAPE_URL:
		success = drop_url (panel, pos, (char *)selection_data->data);
		break;
	case TARGET_COLOR:
		success = set_background_color (panel->toplevel, (guint16 *) selection_data->data);
		break;
	case TARGET_BGIMAGE:
		success = set_background_image_from_uri (panel->toplevel, (char *) selection_data->data);
		break;
	case TARGET_BACKGROUND_RESET:
		if (panel_profile_is_writable_background_type (panel->toplevel)) {
			panel_profile_set_background_type (panel->toplevel, PANEL_BACK_NONE);
			success = TRUE;
		} else {
			success = FALSE;
		}
		break;
	case TARGET_DIRECTORY:
		success = drop_directory (panel, pos, (char *)selection_data->data);
		break;
	case TARGET_APPLET:
		if (!selection_data->data) {
			gtk_drag_finish (context, FALSE, FALSE, time_);
			return;
		}
		if (panel_profile_list_is_writable (PANEL_GCONF_APPLETS)) {
			panel_applet_frame_create (panel->toplevel, pos, (char *) selection_data->data);
			success = TRUE;
		} else {
			success = FALSE;
		}
		break;
	case TARGET_APPLET_INTERNAL:
		success = drop_internal_applet (panel, pos, (char *)selection_data->data,
						context->action);
		break;
	case TARGET_ICON_INTERNAL:
		success = drop_internal_icon (panel, pos, (char *)selection_data->data,
					      context->action);
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
	PanelWidget *panel_widget;
	int          pos;

	g_return_if_fail (PANEL_IS_TOPLEVEL (widget));

	/* we use this only to really find out the info, we already
	   know this is an ok drop site and the info that got passed
	   to us is bogus (it's always 0 in fact) */
	if (!panel_check_dnd_target_data (widget, context, &info, NULL)) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	panel_widget = panel_toplevel_get_panel_widget (PANEL_TOPLEVEL (widget));

	pos = panel_widget_get_cursorloc (panel_widget);
	
	/* 
	 * -1 passed to panel_applet_register will turn on 
	 * the insert_at_pos flag for panel_widget_add_full,
	 * which will not place it after the first applet.
	 */
	if(pos < 0)
		pos = -1;
	else if(pos > panel_widget->size)
		pos = panel_widget->size;

	panel_receive_dnd_data (
		panel_widget, info, pos, selection_data, context, time);
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
	pd->insertion_pos = -1;
	pd->deactivate_idle = 0;

	panel_list = g_slist_append (panel_list, pd);
	
	g_object_set_data (G_OBJECT (toplevel), "PanelData", pd);

	panel_lockdown_notify_add (G_CALLBACK (panel_recreate_context_menu),
				   pd);

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
	g_signal_connect (toplevel, "popup-menu",
			  G_CALLBACK (panel_popup_menu_signal), NULL);

	g_signal_connect_swapped (toplevel, "notify::orientation",
				  G_CALLBACK (panel_orient_change), panel_widget);
 
	g_signal_connect (toplevel, "destroy", G_CALLBACK (panel_destroy), pd);

	return pd;
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
panel_screen_from_panel_widget (PanelWidget *panel)
{
	g_return_val_if_fail (PANEL_IS_WIDGET (panel), NULL);
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (panel->toplevel), NULL);

	return gtk_window_get_screen (GTK_WINDOW (panel->toplevel));
}

gboolean
panel_is_applet_right_stick (GtkWidget *applet)
{
	PanelWidget *panel_widget;

	g_return_val_if_fail (GTK_IS_WIDGET (applet), FALSE);
	g_return_val_if_fail (PANEL_IS_WIDGET (applet->parent), FALSE);

	panel_widget = PANEL_WIDGET (applet->parent);

	if (!panel_toplevel_get_expand (panel_widget->toplevel))
		return FALSE;

	return panel_widget_is_applet_stuck (panel_widget, applet);
}

static void
panel_delete_without_query (PanelToplevel *toplevel)
{
	PanelWidget *panel_widget;

	panel_widget = panel_toplevel_get_panel_widget (toplevel);

	if (panel_toplevel_get_is_attached (toplevel) &&
	    panel_widget->master_widget) {
		AppletInfo *info;
		Drawer *drawer;

		info = g_object_get_data (G_OBJECT (panel_widget->master_widget),
					  "applet_info");

		drawer = (Drawer *) info->data;

		panel_profile_delete_object (info);
	} else
		panel_profile_delete_toplevel (toplevel);
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
	panel_toplevel_unblock_auto_hide (toplevel);
	g_object_set_data (G_OBJECT (toplevel), "panel-delete-dialog", NULL);
}

GtkWidget *
panel_deletion_dialog (PanelToplevel *toplevel)
{

	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
			GTK_WINDOW (toplevel),
			GTK_DIALOG_MODAL,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			_("When a panel is deleted, the panel "
			"and its\n settings are lost. "
			"Delete this panel?"));
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_DELETE, GTK_RESPONSE_OK,
				NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Delete Panel"));

	gtk_window_set_wmclass (GTK_WINDOW (dialog),
				"panel_remove_query", "Panel");

	gtk_window_set_screen (GTK_WINDOW (dialog),
				gtk_window_get_screen (GTK_WINDOW (toplevel)));

	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

	 g_signal_connect (dialog, "destroy",
                           G_CALLBACK (panel_deletion_destroy_dialog),
                           toplevel);

	g_object_set_data (G_OBJECT (toplevel), "panel-delete-dialog", dialog);

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
	PanelWidget *panel_widget;

	panel_widget = panel_toplevel_get_panel_widget (toplevel);

	if (!panel_global_config_get_confirm_panel_remove () ||
	    !g_list_length (panel_widget->applet_list)) {
		panel_delete_without_query (toplevel);
		return;
	}

	panel_toplevel_block_auto_hide (toplevel);

	panel_query_deletion (toplevel);
}
