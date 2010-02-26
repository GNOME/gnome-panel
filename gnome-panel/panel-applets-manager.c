/*
 * panel-applets-manmanger.c
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <gio/gio.h>
#include <gmodule.h>
#include <string.h>

#include "panel-applets-manager.h"

static GHashTable *applet_factories;
static GList      *monitors = NULL;

typedef gint (* ActivateAppletFunc) (void);

typedef struct _PanelAppletFactoryInfo {
	gchar             *id;
	gchar             *location;
	gboolean           in_process;
	GModule           *module;
	ActivateAppletFunc activate_applet;
	guint              n_applets;

	gchar             *srcdir;

	GList             *applet_list;
	gboolean           has_old_ids;
} PanelAppletFactoryInfo;

struct _PanelAppletInfo {
	gchar  *iid;

	gchar  *name;
	gchar  *comment;
	gchar  *icon;

	gchar **old_ids;
};

#define PANEL_APPLET_FACTORY_GROUP "Applet Factory"
#define PANEL_APPLETS_EXTENSION    ".panel-applet"

static void
panel_applet_info_free (PanelAppletInfo *info)
{
	if (!info)
		return;

	g_free (info->iid);
	g_free (info->name);
	g_free (info->comment);
	g_free (info->icon);
	g_strfreev (info->old_ids);

	g_slice_free (PanelAppletInfo, info);
}

static void
panel_applet_factory_info_free (PanelAppletFactoryInfo *info)
{
	if (!info)
		return;

	g_free (info->id);
	g_free (info->location);
	g_list_foreach (info->applet_list,
			(GFunc)panel_applet_info_free,
			NULL);
	g_list_free (info->applet_list);
	info->applet_list = NULL;
	g_free (info->srcdir);

	g_slice_free (PanelAppletFactoryInfo, info);
}

static PanelAppletInfo *
_panel_applets_manager_get_applet_info (GKeyFile    *applet_file,
					const gchar *group,
					const gchar *factory_id)
{
	PanelAppletInfo *info;

	info = g_slice_new0 (PanelAppletInfo);

	info->iid = g_strdup_printf ("%s::%s", factory_id, group);
	info->name = g_key_file_get_locale_string (applet_file, group,
						   "Name", NULL, NULL);
	info->comment = g_key_file_get_locale_string (applet_file, group,
						      "Description", NULL, NULL);
	info->icon = g_key_file_get_string (applet_file, group, "Icon", NULL);

	/* Bonobo compatibility */
	info->old_ids = g_key_file_get_string_list (applet_file, group,
						    "BonoboId", NULL, NULL);

	return info;
}

static PanelAppletFactoryInfo *
panel_applets_manager_get_applet_factory_info_from_file (const gchar *filename)
{
	PanelAppletFactoryInfo *info;
	GKeyFile               *applet_file;
	gchar                 **groups;
	gsize                   n_groups;
	gint                    i;
	GError                 *error = NULL;

	applet_file = g_key_file_new ();
	if (!g_key_file_load_from_file (applet_file, filename, G_KEY_FILE_NONE, &error)) {
		g_warning ("Error opening panel applet file %s: %s",
			   filename, error->message);
		g_error_free (error);
		g_key_file_free (applet_file);

		return NULL;
	}

	info = g_slice_new0 (PanelAppletFactoryInfo);
	info->id = g_key_file_get_string (applet_file, PANEL_APPLET_FACTORY_GROUP, "Id", NULL);
	if (!info->id) {
		g_warning ("Bad panel applet file %s: Could not find 'Id' in group '%s'",
			   filename, PANEL_APPLET_FACTORY_GROUP);
		panel_applet_factory_info_free (info);
		g_key_file_free (applet_file);

		return NULL;
	}

	info->in_process = g_key_file_get_boolean (applet_file, PANEL_APPLET_FACTORY_GROUP,
						   "InProcess", NULL);
	if (info->in_process) {
		info->location = g_key_file_get_string (applet_file, PANEL_APPLET_FACTORY_GROUP,
							"Location", NULL);
		if (!info->location) {
			g_warning ("Bad panel applet file %s: In-process applet without 'Location'",
				   filename);
			panel_applet_factory_info_free (info);
			g_key_file_free (applet_file);

			return NULL;
		}
	}

	info->has_old_ids = FALSE;

	groups = g_key_file_get_groups (applet_file, &n_groups);
	for (i = 0; i < n_groups; i++) {
		PanelAppletInfo *ainfo;

		if (strcmp (groups[i], PANEL_APPLET_FACTORY_GROUP) == 0)
			continue;

		ainfo = _panel_applets_manager_get_applet_info (applet_file,
								groups[i], info->id);
		if (ainfo->old_ids)
			info->has_old_ids = TRUE;

		info->applet_list = g_list_prepend (info->applet_list, ainfo);
	}
	g_strfreev (groups);

	g_key_file_free (applet_file);

	if (!info->applet_list) {
		panel_applet_factory_info_free (info);
		return NULL;
	}

	info->srcdir = g_path_get_dirname (filename);

	return info;
}

static GSList *
panel_applets_manager_get_applets_dirs (void)
{
	const gchar *dir = NULL;
	gchar      **paths;
	guint        i;
	GSList      *retval = NULL;

	dir = g_getenv ("PANEL_APPLETS_DIR");
	if (!dir || strcmp (dir, "") == 0) {
		return g_slist_prepend (NULL, g_strdup (PANEL_APPLETS_DIR));
	}

	paths = g_strsplit (dir, ":", 0);
	for (i = 0; paths[i]; i++) {
		if (g_slist_find_custom (retval, paths[i], (GCompareFunc)strcmp))
			continue;
		retval = g_slist_prepend (retval, g_strdup (paths[i]));
	}
	g_strfreev (paths);

	return g_slist_reverse (retval);
}

static void
applets_directory_changed (GFileMonitor     *monitor,
			   GFile            *file,
			   GFile            *other_file,
			   GFileMonitorEvent event_type,
			   gpointer          user_data)
{
	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CHANGED:
	case G_FILE_MONITOR_EVENT_CREATED: {
		PanelAppletFactoryInfo *info;
		PanelAppletFactoryInfo *old_info;
		gchar                  *filename;
		GSList                 *dirs, *d;

		filename = g_file_get_path (file);
		if (!g_str_has_suffix (filename, PANEL_APPLETS_EXTENSION)) {
			g_free (filename);
			return;
		}

		info = panel_applets_manager_get_applet_factory_info_from_file (filename);
		g_free (filename);

		if (!info)
			return;

		old_info = g_hash_table_lookup (applet_factories, info->id);
		if (!old_info) {
			/* New applet, just insert it */
			g_hash_table_insert (applet_factories, g_strdup (info->id), info);
			return;
		}

		/* Make sure we don't update an applet
		 * that has changed in another source dir
		 * unless it takes precedence over the
		 * current one
		 */
		if (strcmp (info->srcdir, old_info->srcdir) == 0) {
			g_hash_table_insert (applet_factories, g_strdup (info->id), info);
			return;
		}

		dirs = panel_applets_manager_get_applets_dirs ();

		for (d = dirs; d; d = g_slist_next (d)) {
			gchar *path = (gchar *)d->data;

			if (strcmp (path, old_info->srcdir) == 0) {
				panel_applet_factory_info_free (info);
				break;
			} else if (strcmp (path, info->srcdir) == 0) {
				g_hash_table_insert (applet_factories, g_strdup (info->id), info);
				break;
			}
		}

		g_slist_foreach (dirs, (GFunc)g_free, NULL);
		g_slist_free (dirs);
	}
		break;
	default:
		/* Ignore any other change */
		break;
	}
}

static gboolean
_panel_applets_manager_init (void)
{
	GSList      *dirs, *d;
	GDir        *dir;
	const gchar *dirent;
	GError      *error = NULL;
	gboolean     retval = FALSE;

	dirs = panel_applets_manager_get_applets_dirs ();
	for (d = dirs; d; d = g_slist_next (d)) {
		GFileMonitor *monitor;
		GFile        *dir_file;
		gchar        *path = (gchar *)d->data;

		dir = g_dir_open (path, 0, &error);
		if (!dir) {
			g_warning ("%s", error->message);
			g_error_free (error);
			g_free (path);

			continue;
		}

		/* Monitor dir */
		dir_file = g_file_new_for_path (path);
		monitor = g_file_monitor_directory (dir_file,
						    G_FILE_MONITOR_NONE,
						    NULL, NULL);
		if (monitor) {
			g_signal_connect (monitor, "changed",
					  G_CALLBACK (applets_directory_changed),
					  NULL);
			monitors = g_list_prepend (monitors, monitor);
		}
		g_object_unref (dir_file);

		while ((dirent = g_dir_read_name (dir))) {
			PanelAppletFactoryInfo *info;
			gchar                  *file;

			if (!g_str_has_suffix (dirent, PANEL_APPLETS_EXTENSION))
				continue;

			file = g_build_filename (path, dirent, NULL);
			info = panel_applets_manager_get_applet_factory_info_from_file (file);
			g_free (file);

			if (!info)
				continue;

			if (g_hash_table_lookup (applet_factories, info->id)) {
				panel_applet_factory_info_free (info);
				continue;
			}

			g_hash_table_insert (applet_factories, g_strdup (info->id), info);
			retval = TRUE;
		}

		g_dir_close (dir);
		g_free (path);
	}

	g_slist_free (dirs);

	return retval;
}

gboolean
panel_applets_manager_init (void)
{
	if (applet_factories)
		return TRUE;

	applet_factories = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  (GDestroyNotify)g_free,
						  (GDestroyNotify)panel_applet_factory_info_free);

	return _panel_applets_manager_init ();
}

void
panel_applets_manager_shutdown (void)
{
	if (monitors) {
		g_list_foreach (monitors, (GFunc)g_object_unref, NULL);
		g_list_free (monitors);
		monitors = NULL;
	}

	if (applet_factories) {
		g_hash_table_destroy (applet_factories);
		applet_factories = NULL;
	}
}

GList *
panel_applets_manager_get_applets (void)
{
	GHashTableIter iter;
	gpointer       key, value;
	GList         *retval = NULL;

	g_hash_table_iter_init (&iter, applet_factories);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		PanelAppletFactoryInfo *info;

		info = (PanelAppletFactoryInfo *)value;
		retval = g_list_concat (retval, g_list_copy (info->applet_list));
	}

	return retval;
}

static PanelAppletFactoryInfo *
get_applet_factory_info (const gchar *iid)
{
	PanelAppletFactoryInfo *info;
	const gchar            *sp;
	gchar                  *factory_id;

	if (!applet_factories)
		return NULL;

	sp = g_strrstr (iid, "::");
	if (!sp)
		return NULL;

	factory_id = g_strndup (iid, strlen (iid) - strlen (sp));
	info = g_hash_table_lookup (applet_factories, factory_id);
	g_free (factory_id);

	return info;
}

gboolean
panel_applets_manager_factory_activate (const gchar *iid)
{
	PanelAppletFactoryInfo *info;
	ActivateAppletFunc      activate_applet;

	info = get_applet_factory_info (iid);
	if (!info)
		return FALSE;

	/* Out process applets are activated
	 * by the session bus
	 */
	if (!info->in_process)
		return TRUE;

	if (info->module) {
		if (info->n_applets == 0) {
			if (info->activate_applet () != 0) {
				g_warning ("Failed to reactivate factory %s\n", iid);
				return FALSE;
			}
		}
		info->n_applets++;

		return TRUE;
	}

	info->module = g_module_open (info->location, G_MODULE_BIND_LAZY);
	if (!info->module) {
		/* FIXME: use a GError? */
		g_warning ("Failed to load applet %s: %s\n",
			   iid, g_module_error ());
		return FALSE;
	}

	if (!g_module_symbol (info->module, "_panel_applet_shlib_factory", (gpointer *)&activate_applet)) {
		/* FIXME: use a GError? */
		g_warning ("Failed to load applet %s: %s\n",
			   iid, g_module_error ());
		g_module_close (info->module);
		info->module = NULL;

		return FALSE;
	}

	/* Activate the applet */
	if (activate_applet () != 0) {
		/* FIXME: use a GError? */
		g_warning ("Failed to load applet %s\n", iid);
		g_module_close (info->module);
		info->module = NULL;

		return FALSE;
	}
	info->activate_applet = activate_applet;

	info->n_applets = 1;

	return TRUE;
}

void
panel_applets_manager_factory_deactivate (const gchar *iid)
{
	PanelAppletFactoryInfo *info;

	info = get_applet_factory_info (iid);
	if (!info)
		return;

	/* Out process applets are deactivated
	 * by the session bus
	 */
	if (!info->in_process)
		return;

	if (!info->module)
		return;

	if (--info->n_applets == 0) {
		/* FIXME: we should close the module here,
		 * however applet types are registered static
		 */
#if 0
		g_module_close (info->module);
		info->module = NULL;
#endif
	}
}

gboolean
panel_applets_manager_is_factory_in_process (const gchar *iid)
{
	PanelAppletFactoryInfo *info;

	info = get_applet_factory_info (iid);
	if (!info)
		return FALSE;

	return info->in_process;
}

PanelAppletInfo *
panel_applets_manager_get_applet_info (const gchar *iid)
{
	PanelAppletFactoryInfo *info;
	GList                  *l;

	info = get_applet_factory_info (iid);
	if (!info)
		return NULL;

	for (l = info->applet_list; l; l = g_list_next (l)) {
		PanelAppletInfo *ainfo = (PanelAppletInfo *)l->data;

		if (strcmp (ainfo->iid, iid) == 0)
			return ainfo;
	}

	return NULL;
}

PanelAppletInfo *
panel_applets_manager_get_applet_info_from_old_id (const gchar *iid)
{
	GHashTableIter iter;
	gpointer       key, value;

	g_hash_table_iter_init (&iter, applet_factories);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		PanelAppletFactoryInfo *info;
		GList                  *l;

		info = (PanelAppletFactoryInfo *)value;
		if (!info->has_old_ids)
			continue;

		for (l = info->applet_list; l; l = g_list_next (l)) {
			PanelAppletInfo *ainfo;
			gint             i = 0;

			ainfo = (PanelAppletInfo *)l->data;

			if (!ainfo->old_ids)
				continue;

			while (ainfo->old_ids[i]) {
				if (strcmp (ainfo->old_ids[i], iid) == 0)
					return ainfo;
				i++;
			}
		}
	}

	return NULL;
}

const gchar *
panel_applet_info_get_iid (PanelAppletInfo *info)
{
	return info->iid;
}

const gchar *
panel_applet_info_get_name (PanelAppletInfo *info)
{
	return info->name;
}

const gchar *
panel_applet_info_get_description (PanelAppletInfo *info)
{
	return info->comment;
}

const gchar *
panel_applet_info_get_icon (PanelAppletInfo *info)
{
	return info->icon;
}


