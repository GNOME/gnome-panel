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

static gboolean
is_an_uri (const char *uri)
{
	GnomeVFSURI *suri = gnome_vfs_uri_new (uri);
	if (suri == NULL)
		return FALSE;
	gnome_vfs_uri_unref (suri);
	return TRUE;
}

static char *
get_uri (const char *arg)
{
	char *uri;

	if (is_an_uri (arg))
		return gnome_vfs_make_uri_canonical (arg);

	if (g_path_is_absolute (arg)) {
		uri = gnome_vfs_get_uri_from_local_path (arg);
	} else {
		char *cur = g_get_current_dir ();
		char *full = g_build_filename (cur, arg, NULL);
		g_free (cur);
		uri = gnome_vfs_get_uri_from_local_path (full);
		g_free (full);
	}

	return uri;
}

int
main (int argc, char * argv[])
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
		fprintf (stderr, "gnome-desktop-item-edit: no file to edit\n");
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
				dlg = panel_new_launcher (
						uri, gdk_screen_get_default ());

			} else if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
				/* rerun this iteration with the .directory file */
				/* Note: No need to free, for one we can't free an
				 * individual member of desktops and secondly we
				 * will soon exit */
			        desktops[i] =
					g_build_path ("/", uri,
						      ".directory", NULL);
				g_free (uri);
				i--;
				continue;

			} else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR
				   && is_ext (desktops[i], ".directory")
				   && !create_new) {
				char *dirname = g_path_get_dirname (uri);
				dlg = panel_edit_direntry (dirname, NULL,
							   gdk_screen_get_default ());
				g_free (dirname);
			
			} else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR
				   && is_ext (desktops[i], ".desktop")
				   && !create_new) {
				char *dirname = g_path_get_dirname (uri);
				dlg = panel_edit_dentry (uri, dirname,
							 gdk_screen_get_default ());
				g_free (dirname);
				
			} else {
				fprintf (stderr, "gnome-desktop-item-edit: %s "
					 "doesn't seem like a desktop item\n",
					 uri);
			}

		} else if (is_ext (desktops[i], ".directory")
			   && !create_new) {
			/* a non-existant file.  Well we can still edit that sort
			 * of.  We will just create it new */
			char *dirname = g_path_get_dirname (uri);
			dlg = panel_edit_direntry (dirname, NULL,
						   gdk_screen_get_default ());
			g_free (dirname);
		
		} else if (is_ext (desktops[i], ".desktop")
			   && !create_new) {
			/* a non-existant file.  Well we can still edit that sort
			 * of.  We will just create it new */
			/* FIXME: deal with issues of item existing in
			 * another vfolder! */
			char *dirname = g_path_get_dirname (uri);
			dlg = panel_edit_dentry (uri, dirname,
						 gdk_screen_get_default ());
			g_free (dirname);

		} else {
			fprintf (stderr, "gnome-desktop-item-edit: %s does "
				 "not exist\n", uri);
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
