#include <config.h>
#include <string.h>

#include <glib.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-client.h>

#include "panel-gconf.h"

#undef PANEL_GCONF_DEBUG

static GConfClient *panel_gconf_client = NULL;
static char        *panel_gconf_profile = NULL;

G_CONST_RETURN char *
panel_gconf_get_profile (void)
{
	return panel_gconf_profile;
}

/*
 * panel_gconf_sprintf:
 * @format: the format string. See sprintf() documentation.
 * @...: the arguments to be inserted.
 *
 * This is a version of sprintf using a static buffer which is
 * intended for use in generating the full gconf key for all panel
 * config keys.
 * Note, you should not free the return value from this function and
 * you should realize that the return value will get overwritten or
 * freed by a subsequent call to this function.
 *
 * Return Value: a pointer to the static string buffer.
 */
G_CONST_RETURN char *
panel_gconf_sprintf (const char *format,
		     ...)
{
	static char *buffer = NULL;
	static int   buflen = 128;
	va_list      args;
	int          len;

	if (!buffer)
		buffer = g_new (char, buflen);

	va_start (args, format);
	len = g_vsnprintf (buffer, buflen, format, args);

	if (len >= buflen) {
		int i;

		/* Round up length to the nearest power of 2 */
		for (i = 0; len != 1; i++, len >>= 1);

		buflen = len << (i + 1);
		g_assert (buflen > 0);

		g_free (buffer);
		buffer = g_new (char, buflen);

		va_start (args, format);
		len = g_vsnprintf (buffer, buflen, format, args);

		g_assert (len < buflen);
	}

	va_end (args);

	return buffer;
}

G_CONST_RETURN char * 
panel_gconf_global_key (const char *key)
{
	return panel_gconf_sprintf ("/apps/panel/global/%s", key);
}

G_CONST_RETURN char *
panel_gconf_general_key (const char *profile,
			 const char *key)
{
	return panel_gconf_sprintf (
			"/apps/panel/profiles/%s/general/%s", profile, key);
}

G_CONST_RETURN char *
panel_gconf_full_key (PanelGConfKeyType  type,
		      const char        *profile,
		      const char        *id,
		      const char        *key)
{
	char *subdir = NULL;

	switch (type) {
	case PANEL_GCONF_PANELS:
		subdir = "panels";
		break;
	case PANEL_GCONF_OBJECTS:
		subdir = "objects";
		break;
	case PANEL_GCONF_APPLETS:
		subdir = "applets";
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return panel_gconf_sprintf (
			"/apps/panel/profiles/%s/%s/%s/%s",
			profile, subdir, id, key);
}

GConfClient * 
panel_gconf_get_client (void)
{
        if (!panel_gconf_client)
                panel_gconf_client = gconf_client_get_default ();

        return panel_gconf_client;
}

GSList *
panel_gconf_all_global_entries (void)
{
	GSList *list;

	list = gconf_client_all_entries (
			panel_gconf_get_client (), "/apps/panel/global", NULL);

	return list;
}

int 
panel_gconf_get_int (const char *key,
		     int         default_val)
{
	GConfValue *value;
	GError     *error = NULL;

	value = gconf_client_get (panel_gconf_get_client (), key, &error);

	if (value) {
		int retval; 
	
		if (value->type == GCONF_VALUE_INT) 
			retval = gconf_value_get_int (value);
		else
			retval = default_val;

		gconf_value_free (value);

		return retval;
	}

	return default_val;
}

gboolean
panel_gconf_get_bool (const char *key,
		      gboolean    default_val)
{
	GConfValue *value;
	GError     *error = NULL;

	value = gconf_client_get (panel_gconf_get_client (), key, &error);
	
	if (value) {
		gboolean retval;

		if (value->type == GCONF_VALUE_BOOL)
			retval = gconf_value_get_bool (value);
		else
			retval = default_val;

		gconf_value_free (value);

		return retval;
	}

	return default_val;
}

char * 
panel_gconf_get_string (const char *key,
			const char *default_val)
{
	GConfValue *value;
	GError     *error = NULL;

	value = gconf_client_get (panel_gconf_get_client (), key, &error);

	if (value) {
		char *retval;
	
		if (value->type == GCONF_VALUE_STRING)
			retval = g_strdup (gconf_value_get_string (value));
		else
			retval = g_strdup (default_val);

		gconf_value_free (value);

		return retval;
	}

	return g_strdup (default_val);
}

void 
panel_gconf_set_int (const char *key,
		     int         value)
{
	gconf_client_set_int (panel_gconf_get_client (), key, value, NULL);
}

void 
panel_gconf_set_bool (const char *key,
		      gboolean    value)
{
	gconf_client_set_bool (panel_gconf_get_client (), key, value, NULL);
}

void 
panel_gconf_set_string (const char *key,
			const char *value)
{
	gconf_client_set_string (panel_gconf_get_client (), key, value, NULL);
}

guint 
panel_gconf_notify_add (const char            *key,
		        GConfClientNotifyFunc  notify_func,
			gpointer               user_data)
{
	return gconf_client_notify_add (
			panel_gconf_get_client (), key, notify_func,
			user_data, NULL, NULL);
}

static void
panel_notify_object_dead (guint notify_id)
{
	gconf_client_notify_remove (panel_gconf_get_client (), notify_id);
}

guint
panel_gconf_notify_add_while_alive (const char            *key, 
				    GConfClientNotifyFunc  notify_func, 
				    GObject               *alive_object)
{
	guint notify_id;

	g_return_val_if_fail (G_IS_OBJECT (alive_object), 0);

	notify_id = panel_gconf_notify_add (key, notify_func, alive_object);

	if (notify_id > 0) {
		/* Add a weak reference to the object so that we can
		 * remove the notification when the object's gone.
		 */
		g_object_weak_ref (alive_object,
				   (GWeakNotify) panel_notify_object_dead,
				   GUINT_TO_POINTER (notify_id));
	}

	return notify_id;
}

void
panel_gconf_add_dir (const char *key)
{
	gconf_client_add_dir (
		panel_gconf_get_client (), key, GCONF_CLIENT_PRELOAD_NONE, NULL);
}

void
panel_gconf_clean_dir (GConfClient *client,
		       const char  *dir)
{
	GSList *subdirs;
	GSList *entries;
	GSList *l;

	subdirs = gconf_client_all_dirs (client, dir, NULL);

	for (l = subdirs; l; l = l->next) {
		panel_gconf_clean_dir (client, (char *) l->data);
		g_free (l->data);
	}

	g_slist_free (subdirs);
 
  	entries = gconf_client_all_entries (client, dir, NULL);

	for (l = entries; l; l = l->next) {
		GConfEntry *entry = l->data;

		gconf_client_unset (client, gconf_entry_get_key (entry), NULL);
		gconf_entry_free (entry);
	}
    		
	g_slist_free (entries);

	gconf_client_unset (client, dir, NULL);
}

static void
panel_gconf_associate_schemas_in_dir (GConfClient *client,
				      const char  *profile_dir,
				      const char  *schema_dir,
				      GError      **error)
{
	GSList *list, *l;

#ifdef PANEL_GCONF_DEBUG
	g_print ("associating schemas in %s to %s\n", schema_dir, profile_dir);
#endif

	list = gconf_client_all_entries (client, schema_dir, error);

	g_return_if_fail (*error == NULL);

	for (l = list; l; l = l->next) {
		GConfEntry *entry = l->data;
		const char *key;
		char       *tmp;

		tmp = g_path_get_basename (gconf_entry_get_key (entry));

		key = panel_gconf_sprintf ("%s/%s", profile_dir, tmp);

		g_free (tmp);

		gconf_engine_associate_schema (
			client->engine, key, gconf_entry_get_key (entry), error);

		gconf_entry_free (entry);

		if (*error) {
			g_slist_free (list);
			return;
		}
	}

	g_slist_free (list);

	list = gconf_client_all_dirs (client, schema_dir, error);

	for (l = list; l; l = l->next) {
		char *subdir = l->data;
		char *prefs_subdir;
		char *schema_subdir;
		char *tmp;

		tmp = g_path_get_basename (subdir);

		prefs_subdir  = g_strdup_printf ("%s/%s", profile_dir, tmp);
		schema_subdir = g_strdup_printf ("%s/%s", schema_dir, tmp);

		panel_gconf_associate_schemas_in_dir (
			client, prefs_subdir, schema_subdir, error);

		g_free (prefs_subdir);
		g_free (schema_subdir);
		g_free (subdir);
		g_free (tmp);

		if (*error) {
			g_slist_free (list);
			return;
		}
	}

	g_slist_free (list);
}

void
panel_gconf_setup_profile (const char *profile)
{
	GError *error = NULL;
	char   *profile_dir;
	char   *schema_dir;

	if (profile)
		panel_gconf_profile = g_strdup (profile);
	else
		panel_gconf_profile = g_strdup ("default");

	profile_dir = g_strconcat ("/apps/panel/profiles/", panel_gconf_profile, NULL);

	if (gconf_client_dir_exists (panel_gconf_get_client (), profile_dir, NULL)) {
		g_free (profile_dir);
		return;
	}

#ifdef PANEL_GCONF_DEBUG
	g_print ("%s does not exist\n", profile_dir);
#endif

	/*
	 * FIXME: work out what set of defaults we want to use
	 */
	schema_dir = g_strconcat ("/schemas/apps/panel/default_profiles/", "medium", NULL);

	panel_gconf_associate_schemas_in_dir (
		panel_gconf_get_client (), profile_dir, schema_dir, &error);

	if (error != NULL) {
		g_warning ("gconf error: %s", error->message);
		g_clear_error (&error);
	}

	g_free (profile_dir);
	g_free (schema_dir);

	/*
	 * FIXME: setup notifies
	 */
}

char *
panel_gconf_load_default_config_for_screen (PanelGConfKeyType   type,
					    const char         *profile,
					    const char         *id,
					    int                 screen,
					    GError            **error)
{
	const char *subdir = NULL;
	const char *schemas_dir;
	char       *new_dir;
	char       *new_id;

	switch (type) {
	case PANEL_GCONF_PANELS:
		subdir = "panels";
		break;
	case PANEL_GCONF_OBJECTS:
		subdir = "objects";
		break;
	case PANEL_GCONF_APPLETS:
		subdir = "applets";
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	schemas_dir = panel_gconf_sprintf (
				"/schemas/apps/panel/default_profiles/medium/%s/%s",
				subdir, id);

	new_id = g_strdup_printf ("%s_screen%d", id, screen);

	new_dir = g_strdup_printf ("/apps/panel/profiles/%s/%s/%s", profile, subdir, new_id);

	panel_gconf_associate_schemas_in_dir (
		panel_gconf_get_client (), new_dir, schemas_dir, error);

	g_free (new_dir);

	if (error && *error) {
		g_free (new_id);
		new_id = NULL;
	}

	return new_id;
}
