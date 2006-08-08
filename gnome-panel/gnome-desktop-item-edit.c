#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "panel-ditem-editor.h"
#include "panel-util.h"

#include "nothing.cP"

/* FIXME Symbol needed by panel-util.c - sucky */
#include "applet.h"
GSList *panel_applet_list_applets (void) { return NULL; }

static int dialogs = 0;
static gboolean create_new = FALSE;
static char **desktops = NULL;

static const GOptionEntry options[] = {
	{ "create-new", 0, 0, G_OPTION_ARG_NONE, &create_new, N_("Create new file in the given directory"), NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &desktops, NULL, N_("[FILE...]") },
	{ NULL }
};

static void
dialog_destroyed (GtkWidget *dialog, gpointer data)
{
	dialogs --;

	if (dialogs <= 0)
		gtk_main_quit ();
}

static char *
get_uri (const char *arg)
{
	char *current_dir;
	char *resolved;
	char *uri;

	if (!g_path_is_absolute (arg)) {
		current_dir = g_get_current_dir ();
		uri = g_strconcat (current_dir, "/", NULL);
		g_free (current_dir);
		current_dir = uri;

		resolved = gnome_vfs_uri_make_full_from_relative (current_dir, arg);
		g_free (current_dir);

		uri = gnome_vfs_make_uri_canonical (resolved);
		g_free (resolved);
	} else
		uri = gnome_vfs_get_uri_from_local_path (arg);

	return uri;
}

static void
validate_for_filename (char *file)
{
	char *ptr;

	g_return_if_fail (file != NULL);
	
	ptr = file;
	while (*ptr != '\0') {
		if (*ptr == '/')
			*ptr = '_';
		ptr++;
	}
}

static char *
get_unique_name (const char *dir,
		 const char *name)
{
	int i;
	char *full;
	char *nameext = g_strdup_printf ("%s.desktop", name);

	full = g_build_path ("/", dir, nameext, NULL);
	if (!panel_uri_exists (full)) {
		g_free (nameext);
		return full;
	}
	g_free (full);

	i = 2;
	for (;;) {
		g_free (nameext);
		nameext = g_strdup_printf ("%s%d.desktop", name, i++);

		/* randomize further same name desktops */
		if (i > 5)
			i = g_random_int ();

		full = g_build_path ("/", dir, nameext, NULL);
		if (!panel_uri_exists (full)) {
			g_free (nameext);
			return full;
		}
		g_free (full);
	}
}

static char *
find_uri_on_save_directory (PanelDItemEditor *dialog,
			    gpointer          data)
{
	char *filename;

	filename = g_object_get_data (G_OBJECT (dialog), "filename");

	return g_strdup (filename);
}

static char *
find_uri_on_save (PanelDItemEditor *dialog,
		  gpointer          data)
{
	GKeyFile *keyfile;
	char     *name;
	char     *filename;
	char     *uri;
	char     *dir;

	keyfile = panel_ditem_editor_get_key_file (dialog);
	name = panel_util_key_file_get_string (keyfile, "Name");

	validate_for_filename (name);
	filename = g_filename_from_utf8 (name, -1, NULL, NULL, NULL);
	g_free (name);

	if (filename == NULL)
		filename = g_strdup ("foo");

	dir = g_object_get_data (G_OBJECT (dialog), "dir");
	uri = get_unique_name (dir, filename);

	g_free (filename);

	return uri;
}

static void
error_reported (GtkWidget  *dialog,
		const char *primary,
		const char *secondary,
		gpointer    data)
{
	panel_error_dialog (GTK_WINDOW (dialog), NULL,
			    "error_editing_launcher", TRUE,
			    primary, secondary);
}

int
main (int argc, char * argv[])
{
	GOptionContext *context;
	GnomeProgram *program;
	int i;
	GnomeVFSFileInfo *info;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (_("- Edit .desktop files"));

	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	program = gnome_program_init ("gnome-desktop-item-edit", VERSION,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_GOPTION_CONTEXT, context,
				      NULL);

	gtk_window_set_default_icon_name ("launcher-program");

	if (desktops == NULL ||
	    desktops[0] == NULL) {
		g_printerr ("gnome-desktop-item-edit: no file to edit\n");
		return 0;
	}

	info = gnome_vfs_file_info_new ();

	for (i = 0; desktops[i] != NULL; i++) {
		char *uri = get_uri (desktops[i]);
		GtkWidget *dlg = NULL;

		gnome_vfs_file_info_clear (info);

		if (gnome_vfs_get_file_info
		    (uri, info, GNOME_VFS_FILE_INFO_DEFAULT) == GNOME_VFS_OK) {

			if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY && create_new) {
				dlg = panel_ditem_editor_new (NULL, NULL, NULL,
							     _("Create Launcher"));
				g_object_set_data_full (G_OBJECT (dlg), "dir",
							g_strdup (uri),
							(GDestroyNotify)g_free);

				panel_ditem_register_save_uri_func (PANEL_DITEM_EDITOR (dlg),
								    find_uri_on_save,
								    NULL);

			} else if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
				/* Rerun this iteration with the .directory
				 * file
				 * Note: No need to free, for one we can't free
				 * an individual member of desktops and
				 * secondly we will soon exit */
			        desktops[i] = g_build_path ("/", uri,
							    ".directory", NULL);
				g_free (uri);
				i--;
				continue;
			} else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR
				   && g_str_has_suffix (desktops [i], ".directory")
				   && !create_new) {
				dlg = panel_ditem_editor_new (NULL, NULL, uri,
							      _("Directory Properties"));
			} else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR
				   && g_str_has_suffix (desktops [i], ".desktop")
				   && !create_new) {
				dlg = panel_ditem_editor_new (NULL, NULL, uri,
							      _("Launcher Properties"));
			} else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR
				   && create_new) {
				g_printerr ("gnome-desktop-item-edit: %s "
					    "already exists\n", uri);
			} else {
				g_printerr ("gnome-desktop-item-edit: %s "
					    "doesn't seem like a desktop "
					    "item\n", uri);
			}

		} else if (g_str_has_suffix (desktops [i], ".directory")
			   && !create_new) {
			/* a non-existant file.  Well we can still edit that
			 * sort of.  We will just create it new */
			GKeyFile *key_file;
			char     *dirname;
			char     *filename;

			dirname = g_path_get_dirname (uri);
			filename = g_strconcat (dirname, "/", ".directory",
						NULL);
			g_free (dirname);

			key_file = panel_util_key_file_new_desktop ();
			panel_util_key_file_set_string (key_file,
							"Type", "Directory");

			dlg = panel_ditem_editor_new (NULL, key_file, NULL,
						      _("Directory Properties"));
			g_object_set_data_full (G_OBJECT (dlg), "filename",
						filename,
						(GDestroyNotify)g_free);

			panel_ditem_register_save_uri_func (PANEL_DITEM_EDITOR (dlg),
							    find_uri_on_save_directory,
							    NULL);

		} else if (g_str_has_suffix (desktops [i], ".desktop")
			   && !create_new) {
			/* a non-existant file.  Well we can still edit that
			 * sort of.  We will just create it new */
			dlg = panel_ditem_editor_new (NULL, NULL, uri,
						      _("Create Launcher"));

		} else {
			g_printerr ("gnome-desktop-item-edit: %s does "
				    "not exist\n", uri);
		}

		if (dlg != NULL) {
			dialogs ++;
			g_signal_connect (G_OBJECT (dlg), "destroy",
					  G_CALLBACK (dialog_destroyed), NULL);
			g_signal_connect (G_OBJECT (dlg), "error_reported",
					  G_CALLBACK (error_reported), NULL);
			gtk_widget_show (dlg);
		}

		g_free (uri);
	}

	gnome_vfs_file_info_unref (info);

	if (dialogs > 0)
		gtk_main ();

	g_object_unref (program);

        return 0;
}
