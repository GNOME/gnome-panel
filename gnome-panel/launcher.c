/*
 * GNOME panel launcher module.
 * (C) 1997,1998,1999,2000 The Free Software Foundation
 * (C) 2000 Eazel, Inc.
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 * CORBAized by George Lebl
 * de-CORBAized by George Lebl
 *
 */

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdkx.h>

#include <libpanel-util/panel-error.h>
#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-keyfile.h>
#include <libpanel-util/panel-launch.h>
#include <libpanel-util/panel-show.h>

#include "launcher.h"

#include "button-widget.h"
#include "panel-util.h"
#include "xstuff.h"
#include "panel-toplevel.h"
#include "panel-a11y.h"
#include "panel-globals.h"
#include "panel-multiscreen.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-ditem-editor.h"
#include "panel-icon-names.h"
#include "panel-schemas.h"

static GdkScreen *
launcher_get_screen (Launcher *launcher)
{
	PanelWidget *panel_widget;

	g_return_val_if_fail (launcher != NULL, NULL);
	g_return_val_if_fail (launcher->info != NULL, NULL);
	g_return_val_if_fail (launcher->info->widget != NULL, NULL);

	panel_widget = PANEL_WIDGET (gtk_widget_get_parent (launcher->info->widget));

	return gtk_window_get_screen (GTK_WINDOW (panel_widget->toplevel));
}

static void
launcher_widget_open_dialog_destroyed (GtkWidget *dialog,
				       Launcher *launcher)
{
	g_return_if_fail (launcher->error_dialogs != NULL);

	launcher->error_dialogs = g_slist_remove (launcher->error_dialogs, dialog);
}

static void
launcher_widget_destroy_open_dialogs (Launcher *launcher)
{
	GSList *l, *list;

	list = launcher->error_dialogs;
	launcher->error_dialogs = NULL;

	for (l = list; l; l = l->next) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (l->data),
						      G_CALLBACK (launcher_widget_open_dialog_destroyed),
						      launcher);
		gtk_widget_destroy (l->data);
	}
	g_slist_free (list);
}

static void
launcher_register_error_dialog (Launcher *launcher, 
				GtkWidget *dialog)
{
	launcher->error_dialogs = g_slist_append (launcher->error_dialogs,
						  dialog);
	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (launcher_widget_open_dialog_destroyed),
			  launcher);
}

static void
launcher_do_zoom_animation (GtkWidget *widget)
{
	GtkSettings *settings;
	gboolean     enable_animations;

	settings = gtk_widget_get_settings (widget);

	enable_animations = TRUE;
	g_object_get (settings,
		      "gtk-enable-animations", &enable_animations,
		      NULL);

	if (enable_animations)
		xstuff_zoom_animate (widget,
				     button_widget_get_pixbuf (BUTTON_WIDGET (widget)),
				     button_widget_get_orientation (BUTTON_WIDGET (widget)),
				     NULL);
}

static void
launch_url (Launcher *launcher)
{
	char *url;
	GdkScreen *screen;

	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->key_file != NULL);

	/* FIXME panel_ditem_launch() should be enough for this! */
	url = panel_key_file_get_string (launcher->key_file, "URL");

	screen = launcher_get_screen (launcher);

	if (!url || *url == 0) {
		GtkWidget *error_dialog;

		error_dialog = panel_error_dialog (NULL, screen,
						   "no_url_dialog", TRUE,
						   _("Could not show this URL"),
						   _("No URL was specified."));
		launcher_register_error_dialog (launcher, error_dialog);
		g_free (url);
		return;
	}

	panel_show_uri (screen, url, gtk_get_current_event_time (), NULL);

	g_free (url);
}

void
launcher_launch (Launcher  *launcher,
		 GtkWidget *widget)
{
	char *type;

	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->key_file != NULL);

	launcher_do_zoom_animation (widget);
	
	type = panel_key_file_get_string (launcher->key_file, "Type");
	if (type && !strcmp (type, "Link"))
		launch_url (launcher);
	else {
		GError *error = NULL;

		panel_launch_key_file (launcher->key_file, NULL,
				       launcher_get_screen (launcher), &error);
		if (error) {
			GtkWidget *error_dialog;

			error_dialog = panel_error_dialog (
						NULL,
						launcher_get_screen (launcher),
						"cannot_launch_application",
						TRUE,
						_("Could not launch application"),
						error->message);
			launcher_register_error_dialog (launcher, error_dialog);
			g_clear_error (&error);
		}
	}
	g_free (type);
}

static void
drag_data_received_cb (GtkWidget        *widget,
		       GdkDragContext   *context,
		       gint              x,
		       gint              y,
		       GtkSelectionData *selection_data,
		       guint             info,
		       guint             time,
		       Launcher         *launcher)
{
	GError  *error = NULL;
	char   **uris;
	int      i;
	GList   *file_list;

	launcher_do_zoom_animation (widget);
	
	file_list = NULL;
	uris = g_uri_list_extract_uris ((const char *) gtk_selection_data_get_data (selection_data));
	for (i = 0; uris[i]; i++)
		file_list = g_list_prepend (file_list, uris[i]);
	file_list = g_list_reverse (file_list);

	panel_launch_key_file (launcher->key_file, file_list,
			       launcher_get_screen (launcher), &error);

	g_list_free (file_list);
	g_strfreev (uris);

	if (error) {
		GtkWidget *error_dialog;
		error_dialog = panel_error_dialog (NULL,
						   launcher_get_screen (launcher),
						   "cannot_use_dropped_item",
						   TRUE,
						   _("Could not use dropped item"),
						   error->message);
		launcher_register_error_dialog (launcher, error_dialog);
		g_clear_error (&error);
	}

	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
destroy_launcher (GtkWidget *widget,
		  Launcher  *launcher)
{
	launcher_properties_destroy (launcher);
	launcher_widget_destroy_open_dialogs (launcher);
}

void
launcher_properties_destroy (Launcher *launcher)
{
	GtkWidget *dialog;

	dialog = launcher->prop_dialog;
	launcher->prop_dialog = NULL;

	if (dialog)
		gtk_widget_destroy (dialog);
}

static void
free_launcher (gpointer data)
{
	Launcher *launcher = data;

	if (launcher->key_file)
		g_key_file_free (launcher->key_file);
	launcher->key_file = NULL;

	if (launcher->location != NULL)
		g_free (launcher->location);
	launcher->location = NULL;

	g_free (launcher);
}

void
panel_launcher_delete (Launcher *launcher)
{
	if (!launcher->location)
		return;

	/* do not remove the file if it's not in the user's launchers path */
	if (panel_launcher_is_in_personal_path (launcher->location)) {
		GError *error;
		GFile  *file;

		file = panel_launcher_get_gfile (launcher->location);

		error = NULL;
		if (!g_file_delete (file, NULL, &error)) {
			char *path;

			path = g_file_get_path (file);
			g_warning ("Error deleting '%s': %s\n",
				   path, error->message);
			g_free (path);
			g_error_free (error);
		}

		g_object_unref (file);
	}
}

static gboolean
is_this_drop_ok (GtkWidget      *widget,
		 GdkDragContext *context)
{
	static GdkAtom  text_uri_list = GDK_NONE;
	GList           *l;
	GtkWidget       *source;

	source = gtk_drag_get_source_widget (context);

	if (source == widget)
		return FALSE;

	if (!(gdk_drag_context_get_actions (context) & GDK_ACTION_COPY))
		return FALSE;

	if (!text_uri_list)
		text_uri_list = gdk_atom_intern_static_string ("text/uri-list");

	for (l = gdk_drag_context_list_targets (context); l; l = l->next) {
		if (GDK_POINTER_TO_ATOM (l->data) == text_uri_list)
			break;
	}

	return l ? TRUE : FALSE;
}

static void  
drag_leave_cb(GtkWidget	       *widget,
	      GdkDragContext   *context,
	      guint             time,
	      Launcher *launcher)
{
	button_widget_set_dnd_highlight(BUTTON_WIDGET(widget), FALSE);
}


static gboolean
drag_motion_cb(GtkWidget *widget,
	       GdkDragContext *context,
	       gint x,
	       gint y,
	       guint time,
	       Launcher *launcher)
{
	if ( ! is_this_drop_ok (widget, context))
		return FALSE;

	gdk_drag_status (context, GDK_ACTION_COPY, time);

	button_widget_set_dnd_highlight(BUTTON_WIDGET(widget), TRUE);

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
	static GdkAtom text_uri_list = NULL;

	if ( ! is_this_drop_ok (widget, context))
		return FALSE;

	if (text_uri_list == NULL)
		text_uri_list = gdk_atom_intern_static_string ("text/uri-list");

	gtk_drag_get_data (widget, context, text_uri_list, time);

	return TRUE;
}

enum {
	TARGET_ICON_INTERNAL,
	TARGET_URI_LIST
};


static void  
drag_data_get_cb (GtkWidget        *widget,
		  GdkDragContext   *context,
		  GtkSelectionData *selection_data,
		  guint             info,
		  guint             time,
		  Launcher         *launcher)
{
	char *location;
	
	g_return_if_fail (launcher != NULL);

	location = launcher->location;

	if (info == TARGET_URI_LIST) {
		char *uri[2];

		uri[0] = panel_launcher_get_uri (location);
		uri[1] = NULL;

		gtk_selection_data_set_uris (selection_data, uri);

		g_free (uri[0]);
	} else if (info == TARGET_ICON_INTERNAL)
		gtk_selection_data_set (selection_data,
					gtk_selection_data_get_target (selection_data), 8,
					(unsigned char *) location,
					strlen (location));

}

static Launcher *
create_launcher (const char *location)
{
	GKeyFile *key_file;
	char     *scheme;
	gboolean  is_uri;
	gboolean  loaded;
	Launcher *launcher;
	GError   *error = NULL;
	char     *new_location;

	if (!location) {
		g_printerr (_("No URI provided for panel launcher desktop file\n"));
		return NULL;
	}

	new_location = NULL;
	key_file = g_key_file_new ();

	scheme = g_uri_parse_scheme (location);
	is_uri = scheme != NULL;
	g_free (scheme);

	if (!is_uri && !g_path_is_absolute (location)) {
		/* try to first load a file in our config directory, and if it
		 * doesn't exist there, try to find it in the xdg data dirs */
		char *path;

		path = panel_make_full_path (NULL, location);

		if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
			g_free (path);
			path = panel_g_lookup_in_applications_dirs (location);
			/* it's important to keep the full path if the desktop
			 * file comes from a data dir: when the user will edit
			 * it, we'll want to save it in PANEL_LAUNCHERS_PATH
			 * with a random name (and not evolution.desktop, eg)
			 * and having only a basename as location will make
			 * this impossible */
			if (path)
				new_location = g_strdup (path);
		}

		if (path) {
			loaded = g_key_file_load_from_file (key_file, path,
							    G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS,
							    &error);
			g_free (path);
		} else {
			loaded = FALSE;
		}
	} else
		loaded = panel_key_file_load_from_uri (key_file, location,
						       G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS,
						       &error);

	if (!loaded) {
		g_printerr (_("Unable to open desktop file %s for panel launcher%s%s\n"),
			    location,
			    error ? ": " : "",
			    error ? error->message : "");
		if (error)
			g_error_free (error);

		g_key_file_free (key_file);

		return NULL; /*button is null*/
	}

	if (!new_location)
		new_location = g_strdup (location);

	launcher = g_new0 (Launcher, 1);

	launcher->info = NULL;
	launcher->button = NULL;
	launcher->location = new_location;
	launcher->key_file = key_file;
	launcher->prop_dialog = NULL;
	launcher->destroy_handler = 0;

	/* Icon will be setup later */
	launcher->button = button_widget_new (NULL /* icon */,
					      FALSE,
					      PANEL_ORIENTATION_TOP);

	gtk_widget_show (launcher->button);

	/*gtk_drag_dest_set (GTK_WIDGET (launcher->button),
			   GTK_DEST_DEFAULT_ALL,
			   dnd_targets, 2,
			   GDK_ACTION_COPY);*/
	gtk_drag_dest_set (GTK_WIDGET (launcher->button),
			   0, NULL, 0, 0);

	g_signal_connect (launcher->button, "drag_data_get",
			   G_CALLBACK (drag_data_get_cb), launcher);
	g_signal_connect (launcher->button, "drag_data_received",
			   G_CALLBACK (drag_data_received_cb), launcher);
	g_signal_connect (launcher->button, "drag_motion",
			   G_CALLBACK (drag_motion_cb), launcher);
	g_signal_connect (launcher->button, "drag_drop",
			   G_CALLBACK (drag_drop_cb), launcher);
	g_signal_connect (launcher->button, "drag_leave",
			   G_CALLBACK (drag_leave_cb), launcher);
	g_signal_connect_swapped (launcher->button, "clicked",
				  G_CALLBACK (launcher_launch), launcher);

	launcher->destroy_handler =
			g_signal_connect (launcher->button, "destroy",
					  G_CALLBACK (destroy_launcher),
					  launcher);

	return launcher;
}

static void
setup_button (Launcher *launcher)
{
	char *comment;
	char *name;
	char *str;
	char *icon;
	char *unescaped_str;
	
	g_return_if_fail (launcher != NULL);

	name = panel_key_file_get_locale_string (launcher->key_file, "Name");
	comment = panel_key_file_get_locale_string (launcher->key_file,
						    "Comment");

	/* Setup tooltip */
	if (!PANEL_GLIB_STR_EMPTY (name) && !PANEL_GLIB_STR_EMPTY (comment))
		str = g_strdup_printf ("%s\n%s", name, comment);
	else if (!PANEL_GLIB_STR_EMPTY (name))
		str = g_strdup (name);
	else
		str = g_strdup (comment);

	g_free (name);
	g_free (comment);

	/* If we can unescape the string, then we probably have an escaped
	 * string (a location e.g.). If we can't, then it most probably means
	 * we have a % that is not here to encode a character, and we don't
	 * want to unescape in this case. See bug #170516 for details. */
	unescaped_str = g_uri_unescape_string (str, NULL);
	if (unescaped_str) {
		g_free (str);
		str = unescaped_str;
	}

	panel_util_set_tooltip_text (launcher->button, str);

	/* Setup accessible name */
	panel_a11y_set_atk_name_desc (launcher->button, str, NULL);

	g_free (str);

	/* Setup icon */
	icon = panel_key_file_get_locale_string (launcher->key_file, "Icon");
	if (icon && icon[0] == '\0') {
		g_free (icon);
		icon = NULL;
	}

	if (!icon)
		icon = guess_icon_from_exec (button_widget_get_icon_theme (BUTTON_WIDGET (launcher->button)),
					     launcher->key_file);
	if (!icon)
		icon = g_strdup (PANEL_ICON_LAUNCHER);

	button_widget_set_icon_name (BUTTON_WIDGET (launcher->button), icon);
	g_free (icon);
}

static char *
panel_launcher_find_writable_uri (const char *launcher_location,
				  const char *source)
{
	char *path;
	char *uri;

	if (!launcher_location)
		return panel_make_unique_desktop_uri (NULL, source);

	if (!strchr (launcher_location, G_DIR_SEPARATOR)) {
		path = panel_make_full_path (NULL, launcher_location);
		uri = g_filename_to_uri (path, NULL, NULL);
		g_free (path);
		return uri;
	}

	if (panel_launcher_get_filename (launcher_location) != NULL) {
		/* we have a file in the user directory. We either have a path
		 * or an URI */
		if (g_path_is_absolute (launcher_location))
			return g_filename_to_uri (launcher_location,
						  NULL, NULL);
		else
			return g_strdup (launcher_location);
	}

	return panel_make_unique_desktop_uri (NULL, source);
}

static void
launcher_changed (PanelDItemEditor *dialog,
		  Launcher         *launcher)
{
	/* Setup the button look */
	setup_button (launcher);
}

static void
launcher_command_changed (PanelDItemEditor *dialog,
			  const char       *command,
			  Launcher         *launcher)
{
	char     *exec;
	char     *old_exec;
	GKeyFile *revert_key_file;

	revert_key_file = panel_ditem_editor_get_revert_key_file (dialog);

	if (revert_key_file) {
		exec = panel_key_file_get_string (launcher->key_file, "Exec");
		old_exec = panel_key_file_get_string (revert_key_file, "Exec");

		if (!old_exec || !exec || strcmp (old_exec, exec))
			panel_key_file_remove_key (launcher->key_file,
						   "StartupNotify");

		g_free (exec);
		g_free (old_exec);
	}
}

static char *
launcher_save_uri (PanelDItemEditor *dialog,
		   gpointer          data)
{
	GKeyFile   *key_file;
	char       *type;
	char       *exec_or_uri;
	Launcher   *launcher;
	char       *new_uri;
	const char *uri;

	key_file = panel_ditem_editor_get_key_file (dialog);
	type = panel_key_file_get_string (key_file, "Type");
	if (type && !strcmp (type, "Application"))
		exec_or_uri = panel_key_file_get_string (key_file, "Exec");
	else if (type && !strcmp (type, "Link"))
		exec_or_uri = panel_key_file_get_string (key_file, "URL");
	else
		exec_or_uri = panel_key_file_get_string (key_file, "Name");
	g_free (type);

	launcher = (Launcher *) data;

	if (launcher)
		new_uri = panel_launcher_find_writable_uri (launcher->location,
							    exec_or_uri);
	else
		new_uri = panel_launcher_find_writable_uri (NULL, exec_or_uri);

	g_free (exec_or_uri);

	uri = panel_ditem_editor_get_uri (dialog);

	if (!uri || (new_uri && strcmp (new_uri, uri)))
		return new_uri;

	g_free (new_uri);

	return NULL;
}

static void
launcher_saved (GtkWidget *dialog,
		Launcher  *launcher)
{
	const char *uri;

	uri = panel_ditem_editor_get_uri (PANEL_DITEM_EDITOR (dialog));
	if (panel_launcher_get_filename (uri) != NULL)
		uri = panel_launcher_get_filename (uri);

	if (uri && launcher->location && strcmp (uri, launcher->location)) {
		GSettings *settings;

		settings = panel_layout_get_instance_settings (launcher->info->settings,
							       PANEL_LAUNCHER_SCHEMA);
		g_settings_set_string (settings, PANEL_LOCATION_KEY, uri);
		g_object_unref (settings);

		if (launcher->location)
			g_free (launcher->location);
		launcher->location = g_strdup (uri);
	}
}

static void
launcher_error_reported (GtkWidget  *dialog,
			 const char *primary,
			 const char *secondary,
			 gpointer    data)
{
	panel_error_dialog (GTK_WINDOW (dialog), NULL,
			    "error_editing_launcher", TRUE,
			    primary, secondary);
}

void
launcher_properties (Launcher  *launcher)
{
	if (launcher->prop_dialog != NULL) {
		gtk_window_set_screen (GTK_WINDOW (launcher->prop_dialog),
				       gtk_widget_get_screen (launcher->button));
		gtk_window_present (GTK_WINDOW (launcher->prop_dialog));
		return;
	}

	launcher->prop_dialog = panel_ditem_editor_new (NULL,
							launcher->key_file,
							launcher->location,
							_("Launcher Properties"));

	panel_widget_register_open_dialog (PANEL_WIDGET 
					   (gtk_widget_get_parent (launcher->info->widget)),
					   launcher->prop_dialog);

	panel_ditem_register_save_uri_func (PANEL_DITEM_EDITOR (launcher->prop_dialog),
					    launcher_save_uri,
					    launcher);

	g_signal_connect (launcher->prop_dialog, "changed",
			  G_CALLBACK (launcher_changed), launcher);

	g_signal_connect (launcher->prop_dialog, "command_changed",
			  G_CALLBACK (launcher_command_changed), launcher);

	g_signal_connect (launcher->prop_dialog, "saved",
			  G_CALLBACK (launcher_saved), launcher);

	g_signal_connect (launcher->prop_dialog, "error_reported",
			  G_CALLBACK (launcher_error_reported), NULL);

	g_signal_connect (launcher->prop_dialog, "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &launcher->prop_dialog);

	gtk_widget_show (launcher->prop_dialog);
}

static gboolean
lancher_properties_enabled (void)
{
	if (panel_lockdown_get_panels_locked_down_s () ||
	    panel_lockdown_get_disable_command_line_s ())
		return FALSE;

	return TRUE;
}

static Launcher *
load_launcher_applet (const char  *location,
		      PanelWidget *panel,
		      const char  *id,
		      GSettings   *settings)
{
	Launcher *launcher;

	launcher = create_launcher (location);

	if (!launcher)
		return NULL;

	launcher->info = panel_applet_register (launcher->button, panel,
						PANEL_OBJECT_LAUNCHER, id,
						settings,
						launcher, free_launcher);
	if (!launcher->info) {
		free_launcher (launcher);
		return NULL;
	}

	panel_applet_add_callback (launcher->info,
				   "launch",
				   GTK_STOCK_EXECUTE,
				   _("_Launch"),
				   NULL);

	panel_applet_add_callback (launcher->info,
				   "properties",
				   GTK_STOCK_PROPERTIES,
				   _("_Properties"),
				   lancher_properties_enabled);

	panel_widget_set_applet_expandable (panel, GTK_WIDGET (launcher->button), FALSE, TRUE);
	panel_widget_set_applet_size_constrained (panel, GTK_WIDGET (launcher->button), TRUE);

	/* setup button according to ditem */
	setup_button (launcher);

	return launcher;
}

void
launcher_load (PanelWidget *panel_widget,
	       const char  *id,
	       GSettings   *settings)
{
	GSettings *settings_instance;
	Launcher  *launcher;
	char      *launcher_location;

	g_return_if_fail (panel_widget != NULL);
	g_return_if_fail (id != NULL);

	settings_instance = panel_layout_get_instance_settings (settings,
								PANEL_LAUNCHER_SCHEMA);

	launcher_location = g_settings_get_string (settings_instance,
						   PANEL_LOCATION_KEY);

	if (PANEL_GLIB_STR_EMPTY (launcher_location)) {
		g_printerr (_("Launcher location is not set, cannot load launcher\n"));
		g_free (launcher_location);
		g_object_unref (settings_instance);
		return;
	}

	launcher = load_launcher_applet (launcher_location,
					 panel_widget,
					 id, settings);

	if (launcher) {
		if (!g_settings_is_writable (settings_instance,
					     PANEL_LOCATION_KEY)) {
			AppletUserMenu *menu;

			menu = panel_applet_get_callback (launcher->info->user_menu,
							  "properties");
			if (menu != NULL)
				menu->sensitive = FALSE;
		}
	}

	g_free (launcher_location);
	g_object_unref (settings_instance);
}

static void
launcher_new_saved (GtkWidget *dialog,
		    gpointer   data)
{
	PanelWidget         *panel;
	PanelObjectPackType  pack_type;
	int                  pack_index;
	const char          *uri;

	pack_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog),
				     "pack-type"));
	panel = g_object_get_data (G_OBJECT (dialog), "panel");

	pack_index = panel_widget_get_new_pack_index (panel, pack_type);

	uri = panel_ditem_editor_get_uri (PANEL_DITEM_EDITOR (dialog));
	if (panel_launcher_get_filename (uri) != NULL)
		uri = panel_launcher_get_filename (uri);
	panel_launcher_create (panel->toplevel, pack_type, pack_index, uri);
}

void
ask_about_launcher (const char          *file,
		    PanelWidget         *panel,
		    PanelObjectPackType  pack_type)
{
	GtkWidget *dialog;
	GKeyFile  *key_file;

	if (panel_lockdown_get_disable_command_line_s ())
		return;

	dialog = panel_ditem_editor_new (NULL, NULL, NULL,
					 _("Create Launcher"));
	panel_widget_register_open_dialog (panel, dialog);

	key_file = panel_ditem_editor_get_key_file (PANEL_DITEM_EDITOR (dialog));
	if (file != NULL)
		panel_key_file_set_string (key_file, "Exec", file);
	panel_key_file_set_string (key_file, "Type", "Application");
	panel_ditem_editor_sync_display (PANEL_DITEM_EDITOR (dialog));

	panel_ditem_register_save_uri_func (PANEL_DITEM_EDITOR (dialog),
					    launcher_save_uri,
					    NULL);

	g_signal_connect (G_OBJECT (dialog), "saved",
			  G_CALLBACK (launcher_new_saved), NULL);

	g_signal_connect (G_OBJECT (dialog), "error_reported",
			  G_CALLBACK (launcher_error_reported), NULL);

	gtk_window_set_screen (GTK_WINDOW (dialog),
			       gtk_widget_get_screen (GTK_WIDGET (panel)));

	g_object_set_data (G_OBJECT (dialog), "pack-type",
			   GINT_TO_POINTER (pack_type));
	g_object_set_data (G_OBJECT (dialog), "panel", panel);

	gtk_widget_show (dialog);
}

void
panel_launcher_create_from_info (PanelToplevel       *toplevel,
				 PanelObjectPackType  pack_type,
				 int                  pack_index,
				 gboolean             exec_info,
				 const char          *exec_or_uri,
				 const char          *name,
				 const char          *comment,
				 const char          *icon)
{
	GKeyFile *key_file;
	char     *location;
	GError   *error;

	key_file = panel_key_file_new_desktop ();

	/* set current language and the "C" locale to this name,
	 * this is kind of evil... */
	panel_key_file_set_string (key_file, "Name", name);
	panel_key_file_set_string (key_file, "Comment", comment);
	panel_key_file_set_string (key_file, "Icon", icon);
	panel_key_file_set_locale_string (key_file, "Name", name);
	panel_key_file_set_locale_string (key_file, "Comment", comment);
	panel_key_file_set_locale_string (key_file, "Icon", icon);

	if (exec_info) {
		panel_key_file_set_string (key_file, "Exec", exec_or_uri);
		panel_key_file_set_string (key_file, "Type", "Application");
	} else {
		panel_key_file_set_string (key_file, "URL", exec_or_uri);
		panel_key_file_set_string (key_file, "Type", "Link");
	}

	location = panel_make_unique_desktop_uri (NULL, exec_or_uri);

	error = NULL;
	if (panel_key_file_to_file (key_file, location, &error)) {
		panel_launcher_create (toplevel, pack_type, pack_index, location);
	} else {
		panel_error_dialog (GTK_WINDOW (toplevel),
				    gtk_window_get_screen (GTK_WINDOW (toplevel)),
				    "cannot_save_launcher", TRUE,
				    _("Could not save launcher"),
				    error->message);
		g_error_free (error);
	}

	g_key_file_free (key_file);
}

static void
panel_launcher_create_with_id (const char          *toplevel_id,
			       PanelObjectPackType  pack_type,
			       int                  pack_index,
			       const char          *location)
{
	char       *id;
	GSettings  *settings;
	GSettings  *settings_instance;
	char       *no_uri;
	const char *new_location;

	g_return_if_fail (location != NULL);

	id = panel_layout_object_create_start (PANEL_OBJECT_LAUNCHER,
					       NULL,
					       toplevel_id, pack_type, pack_index,
					       &settings);

	no_uri = NULL;
	/* if we have an URI, it might contain escaped characters (? : etc)
	 * that might get unescaped on disk */
	if (!g_ascii_strncasecmp (location, "file:", strlen ("file:")))
		no_uri = g_filename_from_uri (location, NULL, NULL);
	if (!no_uri)
		no_uri = g_strdup (location);

	new_location = panel_launcher_get_filename (no_uri);
	if (new_location == NULL)
		new_location = no_uri;

	settings_instance = panel_layout_get_instance_settings (settings,
								PANEL_LAUNCHER_SCHEMA);

	g_settings_set_string (settings_instance, PANEL_LOCATION_KEY,
			       new_location);

	panel_layout_object_create_finish (id);

	g_object_unref (settings_instance);
	g_object_unref (settings);
	g_free (no_uri);
	g_free (id);
}

void
panel_launcher_create (PanelToplevel       *toplevel,
		       PanelObjectPackType  pack_type,
		       int                  pack_index,
		       const char          *location)
{
	panel_launcher_create_with_id (panel_toplevel_get_id (toplevel),
				       pack_type, pack_index,
				       location);
}

gboolean
panel_launcher_create_copy (PanelToplevel       *toplevel,
			    PanelObjectPackType  pack_type,
			    int                  pack_index,
			    const char          *location)
{
	char       *new_location;
	GFile      *source;
	GFile      *dest;
	gboolean    copied;
	const char *filename;

	new_location = panel_make_unique_desktop_uri (NULL, location);

	source = panel_launcher_get_gfile (location);
	dest = g_file_new_for_uri (new_location);

	copied = g_file_copy (source, dest, G_FILE_COPY_OVERWRITE,
			      NULL, NULL, NULL, NULL);
	
	if (!copied) {
		g_free (new_location);
		return FALSE;
	}

	filename = panel_launcher_get_filename (new_location);
	panel_launcher_create (toplevel, pack_type, pack_index, filename);
	g_free (new_location);

	return TRUE;
}

Launcher *
find_launcher (const char *path)
{
	GSList *l;

	g_return_val_if_fail (path != NULL, NULL);

	for (l = panel_applet_list_applets (); l; l = l->next) {
		AppletInfo *info = l->data;
		Launcher *launcher;

		if (info->type != PANEL_OBJECT_LAUNCHER)
			continue;

		launcher = info->data;

		if (launcher->key_file == NULL)
			continue;

		if (launcher->location != NULL &&
		    strcmp (launcher->location, path) == 0)
			return launcher;
	}

	return NULL;
}

void
panel_launcher_set_dnd_enabled (Launcher *launcher,
				gboolean  dnd_enabled)
{
	GdkPixbuf *pixbuf;

	if (dnd_enabled) {
		static GtkTargetEntry dnd_targets[] = {
			{ "application/x-panel-icon-internal", 0, TARGET_ICON_INTERNAL },
			{ "text/uri-list", 0, TARGET_URI_LIST }
		};

		gtk_widget_set_has_window (launcher->button, TRUE);
		gtk_drag_source_set (launcher->button,
				     GDK_BUTTON1_MASK,
				     dnd_targets, 2,
				     GDK_ACTION_COPY | GDK_ACTION_MOVE);
		//FIXME: this doesn't work since the pixbuf isn't loaded yet
		pixbuf = button_widget_get_pixbuf (BUTTON_WIDGET (launcher->button));
		if (pixbuf) {
			gtk_drag_source_set_icon_pixbuf (launcher->button,
							 pixbuf);
			g_object_unref (pixbuf);
		}
		gtk_widget_set_has_window (launcher->button, FALSE);
	

	} else
		gtk_drag_source_unset (launcher->button);
}
