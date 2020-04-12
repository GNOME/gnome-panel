/*
 * GNOME panel launcher module.
 * (C) 1997,1998,1999,2000 The Free Software Foundation
 * (C) 2000, 2001 Eazel, Inc.
 * (C) 2002 Sun Microsystems Inc
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 *          George Lebl <jirka@5z.com>
 *          Mark McLoughlin <mark@skynet.ie>
 * CORBAized by George Lebl
 * de-CORBAized by George Lebl
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
#include "panel-toplevel.h"
#include "panel-a11y.h"
#include "panel-multiscreen.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-ditem-editor.h"
#include "panel-icon-names.h"
#include "panel-schemas.h"

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
launcher_error_reported (GtkWidget  *dialog,
			 const char *primary,
			 const char *secondary,
			 gpointer    data)
{
	panel_error_dialog (GTK_WINDOW (dialog), NULL,
			    "error_editing_launcher", TRUE,
			    primary, secondary);
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
					       NULL, &settings);

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
