#include "config.h"
#include <stdio.h>
#include <stdlib.h>

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>

#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-directory.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "menu-ditem.h"
#include "panel-util.h"

#include "nothing.cP"

/* Just so we can link with panel-util.c for the convert keys stuff*/
GSList *applets;

static int dialogs = 0;
static gboolean create_new = FALSE;

struct poptOption options [] = {
	{ "create-new", '\0', POPT_ARG_NONE,
	  &create_new, 0, N_("Create new file in the given directory"), NULL },
        POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0}
};


static void
dialog_destroyed (GtkWidget *dialog, gpointer data)
{
	dialogs --;

	if (dialogs <= 0)
		gtk_main_quit ();
}

int
main(int argc, char * argv[])
{
	poptContext ctx;
	GnomeProgram *program;
	char **desktops;
	int i;
	GnomeVFSFileInfo *info;

	program = gnome_program_init ("gnome-desktop-item-edit", VERSION,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_POPT_TABLE, options,
				      NULL);
	g_object_get (G_OBJECT (program),
		      GNOME_PARAM_POPT_CONTEXT, &ctx,
		      NULL);

	desktops = (char **)poptGetArgs (ctx);

	if (desktops == NULL ||
	    desktops[0] == NULL) {
		fprintf (stderr, "gnome-desktop-item-edit: no filename to edit\n");
		return 0;
	}

	info = gnome_vfs_file_info_new ();

	for (i = 0; desktops[i] != NULL; i++) {
		char *uri = g_strdup (desktops[i]);
		GtkWidget *dlg;

		gnome_vfs_file_info_clear (info);

		if (gnome_vfs_get_file_info
		    (uri, info, GNOME_VFS_FILE_INFO_DEFAULT) != GNOME_VFS_OK) {
			g_free (uri);

			if (g_path_is_absolute (desktops[i])) {
				uri = gnome_vfs_get_uri_from_local_path
					(desktops[i]);
			} else {
				char *cur = g_get_current_dir ();
				char *full = g_build_filename
					(cur, desktops[i], NULL);
				g_free (cur);
				uri = gnome_vfs_get_uri_from_local_path (full);
				g_free (full);
			}

			gnome_vfs_file_info_clear (info);
			if (gnome_vfs_get_file_info
			    (uri, info, GNOME_VFS_FILE_INFO_DEFAULT) != GNOME_VFS_OK) {
				g_free (uri);
				/* FIXME: we now assume these are uris, this is all
				 * somewhat broken */
				/* ok, this doesn't exist, really */
				if (is_ext (desktops[i], ".directory")) {
					char *dirname = g_path_get_dirname (desktops[i]);
					char *basename = g_path_get_basename (dirname);
					dlg = panel_edit_direntry (dirname, basename);
					g_free (basename);
					g_free (dirname);
				} else {
					char *dirname = g_path_get_dirname (desktops[i]);
					dlg = panel_edit_dentry (desktops[i], dirname);
					g_free (dirname);
				}
			}
		}

		if (dlg == NULL &&
		    info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			if (create_new) {
				dlg = panel_new_launcher (uri);
			} else {
				char *basename = g_path_get_basename (desktops[i]);
				dlg = panel_edit_direntry (uri, basename);
				g_free (basename);
			}
		} else if (dlg == NULL) {
			char *dirname = g_path_get_dirname (uri);
			dlg = panel_edit_dentry (uri, dirname);
			g_free (dirname);
		}

		if (dlg != NULL) {
			dialogs ++;
			g_signal_connect (G_OBJECT (dlg), "destroy",
					  G_CALLBACK (dialog_destroyed),
					  NULL);
		}

		g_free (uri);
	}

	gnome_vfs_file_info_unref (info);

	if (dialogs > 0)
		gtk_main ();

        return 0;
}
