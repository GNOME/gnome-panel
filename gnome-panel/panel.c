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
#include "gnome-run.h"
#include "launcher.h"
#include "menu-fentry.h"
#include "menu-util.h"
#include "menu.h"
#include "panel-util.h"
#include "panel-config.h"
#include "panel-config-global.h"
#include "panel-gconf.h"
#include "panel-applet-frame.h"
#include "global-keys.h"
#include "panel-action-button.h"
#include "panel-menu-bar.h"
#include "panel-compatibility.h"
#include "panel-multiscreen.h"
#include "panel-toplevel.h"
#include "panel-menu-button.h"
#include "panel-globals.h"

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

static GConfEnumStringPair panel_speed_type_enum_map [] = {
	{ PANEL_SPEED_MEDIUM, "panel-speed-medium" },
	{ PANEL_SPEED_SLOW,   "panel-speed-slow" },
	{ PANEL_SPEED_FAST,   "panel-speed-fast" },
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
	case APPLET_BONOBO:
		panel_applet_frame_change_orientation (
				PANEL_APPLET_FRAME (info->widget), orientation);
		break;
	case APPLET_MENU:
		panel_menu_button_change_orientation (PANEL_MENU_BUTTON (info->widget),
						      orientation);
		break;
	case APPLET_DRAWER: {
		Drawer      *drawer = info->data;
		PanelWidget *panel_widget;

		panel_widget = panel_toplevel_get_panel_widget (drawer->toplevel);

		set_drawer_applet_orientation (drawer, orientation);
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

#ifdef FIXME_FOR_NEW_CONFIG
	/*update the configuration box if it is displayed*/
	update_config_size (PANEL_WIDGET (widget)->toplevel);
#endif /* FIXME_FOR_NEW_CONFIG */
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

	panel_toplevel_unhide (toplevel);
	panel_toplevel_queue_auto_hide (toplevel);

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

	if (info->type == APPLET_DRAWER) {
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
panel_destroy (PanelToplevel *toplevel,
	       PanelData     *pd)
{
	PanelWidget *panel_widget;

	panel_widget = panel_toplevel_get_panel_widget (toplevel);

	panel_remove_applets (panel_widget);

#ifdef FIXME_FOR_NEW_CONFIG		
	kill_config_dialog (toplevel);
#endif /* FIXME_FOR_NEW_CONFIG */

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
	if (pd->menu != NULL)
		return pd->menu;
	
	pd->menu = g_object_ref (create_panel_context_menu (panel));
	gtk_object_sink (GTK_OBJECT (pd->menu));
	g_signal_connect (G_OBJECT (pd->menu), "deactivate",
			  G_CALLBACK (menu_deactivate), pd);
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
	int          x = -1, y = 1;

	panel_widget = panel_toplevel_get_panel_widget (toplevel);
	panel_data   = g_object_get_data (G_OBJECT (toplevel), "PanelData");

	gtk_widget_get_pointer (GTK_WIDGET (panel_widget), &x, &y);

	if (panel_widget->orient == GTK_ORIENTATION_VERTICAL)
		panel_data->insertion_pos = x;
	else
		panel_data->insertion_pos = y;
	
	menu = make_popup_panel_menu (panel_widget);

	panel_toplevel_block_auto_hide (toplevel);

	gtk_menu_set_screen (GTK_MENU (menu),
			     gtk_window_get_screen (GTK_WINDOW (toplevel)));

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			panel_menu_position, panel_widget, button, activate_time);

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
	 * activating the key binding we require here.
	 */ 
	if (GTK_IS_SOCKET (GTK_WINDOW (widget)->focus_widget) &&
	    event->keyval == GDK_F10 && event->state == GDK_CONTROL_MASK)
		return gtk_bindings_activate (GTK_OBJECT (widget),
					      event->keyval,
					      event->state);

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
drop_menu (PanelWidget *panel,
	   int          pos,
	   const char  *menu_path)
{
	panel_menu_button_load (menu_path,
				menu_path != NULL,
				NULL,
				FALSE,
				panel,
				pos,
				TRUE,
				NULL);
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
drop_urilist (PanelWidget *panel, int pos, char *urilist)
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

		if (mimetype != NULL &&
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
		load_drawer_applet (NULL, NULL, NULL, panel->toplevel, pos, TRUE, NULL);

	else if (!strncmp (applet_type, "ACTION:", strlen ("ACTION:")))
		remove_applet = panel_action_button_load_from_drag (
					applet_type, panel, pos,
					TRUE, NULL, &applet_num);

	else if (!strcmp (applet_type, "MENUBAR:NEW"))
		remove_applet = panel_menu_bar_load (panel, pos, TRUE, NULL) != NULL;

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
	GdkAtom ret_atom = 0;

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
	switch (info) {
	case TARGET_URL:
		drop_urilist (panel, pos, (char *)selection_data->data);
		break;
	case TARGET_NETSCAPE_URL:
		drop_url (panel, pos, (char *)selection_data->data);
		break;
	case TARGET_COLOR:
		drop_color (panel, pos, (guint16 *)selection_data->data);
		break;
	case TARGET_BGIMAGE:
		drop_bgimage (panel, (char *)selection_data->data);
		break;
	case TARGET_BACKGROUND_RESET:
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

PanelData *
panel_data_by_id (const char *id)
{
	GSList *l;

	if (!id)
		return NULL;

	for (l = panel_list; l; l = l->next) {
		PanelData   *pd = l->data;
		PanelWidget *panel_widget;

		panel_widget = panel_toplevel_get_panel_widget (PANEL_TOPLEVEL (pd->panel));

		if (panel_widget->unique_id && !strcmp (id, panel_widget->unique_id))
			return pd;
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
panel_screen_from_panel_widget (PanelWidget *panel)
{
	g_return_val_if_fail (PANEL_IS_WIDGET (panel), NULL);
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (panel->toplevel), NULL);

	return gtk_window_get_screen (GTK_WINDOW (panel->toplevel));
}

int
panel_monitor_from_panel_widget (PanelWidget *panel)
{
	g_return_val_if_fail (PANEL_IS_WIDGET (panel), 0);
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (panel->toplevel), 0);

	return panel_toplevel_get_monitor (panel->toplevel);
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
	if (global_config.tooltips_enabled)
		gtk_tooltips_enable (panel_tooltips);
	else
		gtk_tooltips_disable (panel_tooltips);

	panel_global_keys_setup ();
}
