/* Gnome panel: dynamic linking and handling of applet library files
 * (C) 1997 the Free Software Foundation
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include "gnome.h"
#include "libgnome/gnome-dl.h"
#include "applet_files.h"


#define APPLET_CMD_FUNC_NAME "applet_cmd_func"


GHashTable *applet_files_ht;


static AppletFile *
applet_file_new(GnomeLibHandle *dl_handle, char *filename, AppletCmdFunc cmd_func)
{
	AppletFile *af;

	af = g_new(AppletFile, 1);

	af->dl_handle = dl_handle;
	af->filename  = filename;
	af->cmd_func  = cmd_func;

	return af;
}


static void
applet_file_destroy(gpointer key, gpointer value, gpointer user_data)
{
	AppletFile *af;

	af = value;

	gnome_dl_unload(af->dl_handle);
	g_free(af->filename);
	g_free(af);
}


static GnomeFuncHandle *
get_dl_func(GnomeLibHandle *handle, char *name)
{
	GnomeFuncHandle *func;
	const char *error;

	func = gnome_dl_findsym(name, handle);

	if ((error = gnome_dl_error()) != NULL) {
		fprintf(stderr, "get_dl_func: %s (func is %s)\n", error,
			name);
		g_assert(error == NULL);
	}

	return func;
}


static void
init_applet_file(char *filename)
{
	GnomeLibHandle *handle;
	const char     *error;
	char           *id;
	AppletCmdFunc   cmd_func;
	AppletCommand   cmd;
	AppletFile     *af;

	handle = gnome_dl_load(filename, GNOME_DL_BIND_LAZY);
	if (!handle) {
		error = gnome_dl_error();
		fprintf(stderr, "init_applet_file: %s\n", error);
		return;
	}

	cmd_func = (AppletCmdFunc)get_dl_func(handle, APPLET_CMD_FUNC_NAME);

	cmd.cmd = APPLET_CMD_QUERY;
	id = (*cmd_func) (&cmd);

	if (!id) {
		fprintf(stderr, "init_applet_file: APPLET_CMD_QUERY on %s returned a NULL identifier\n", filename);
		return;
	}

	af = applet_file_new(handle, g_strdup(filename), cmd_func);

	g_hash_table_insert(applet_files_ht, id, af);
}


void
applet_files_init(void)
{
	char          *dirname;
	DIR           *dir;
	struct dirent *dent;
	char          *filename;
	struct stat    filestat;
	int            len;
	
	applet_files_ht = g_hash_table_new(g_string_hash, g_string_equal);

	dirname = gnome_unconditional_libdir_file("."); /* Get libdir name */

	dir = opendir(dirname);
	if (!dir)
		return;

	while ((dent = readdir(dir)) != NULL) {
		/* We only want stuff of the form "libpanel_*.so" */

		if (strncmp(dent->d_name, "libpanel_", 9) != 0)
			continue;

		len = strlen(dent->d_name); /* Cannot be less than 9, because of above */

		if (strcmp(dent->d_name + (len - 3), ".so") != 0)
			continue;

		filename = g_concat_dir_and_file(dirname, dent->d_name);

		if ((stat(filename, &filestat) == 0) &&
		    S_ISREG(filestat.st_mode))
			init_applet_file(filename);

		g_free(filename);
	}

	closedir(dir);

	g_free(dirname);
}


void
applet_files_destroy(void)
{
	g_hash_table_foreach(applet_files_ht, applet_file_destroy, NULL);
	g_hash_table_destroy(applet_files_ht);
}


AppletCmdFunc
get_applet_cmd_func(char *id)
{
	AppletFile *af;

	af = g_hash_table_lookup(applet_files_ht, id);

	if (!af)
		return NULL;
	else
		return af->cmd_func;
}
