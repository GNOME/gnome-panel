#include <config.h>
#include <string.h>
#include <glib.h>

#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-client.h>

#include "panel-gconf.h"

static gchar * 
panel_gconf_global_config_get_full_key (const gchar *key) {
	return g_strdup_printf ("/apps/panel/global/%s", key);
}

GConfClient * 
panel_gconf_get_client (void) {
	static GConfClient *client = NULL;

        if (!client)
                client = gconf_client_get_default ();

        return client;
}

gchar * 
panel_gconf_get_session_key (void) {
	static gchar *session_key;
	if (session_key == NULL)  {
		const gchar *panel_client_id;

		panel_client_id = gnome_client_get_id (gnome_master_client ());	
		if (!panel_client_id) {
			g_warning (G_STRLOC " : gnome_client_get_id returned NULL");
			return NULL;
		}

		session_key =  g_strdup_printf ("/apps/panel/sessions/%s", panel_client_id);
	}
	return session_key;
}

GSList *
panel_gconf_all_global_entries (void)
{
	GSList *list;

	list = gconf_client_all_entries (panel_gconf_get_client (),
					 "/apps/panel/global",
					 NULL);

	return list;
}

gint 
panel_gconf_global_config_get_int (const gchar *key) {
	gint value;
	gchar *full_key;

	full_key = panel_gconf_global_config_get_full_key (key);
	value =  gconf_client_get_int (panel_gconf_get_client (),
				       full_key,
				       NULL);
	g_free (full_key);
	return value;
}

gboolean
panel_gconf_global_config_get_bool (const gchar *key) {
	gboolean value;
	gchar *full_key;
	
	full_key = panel_gconf_global_config_get_full_key (key);
	value = gconf_client_get_bool (panel_gconf_get_client (),
				       full_key,
				       NULL);
	g_free (full_key);
	return value;
}

gchar * 
panel_gconf_global_config_get_string (const gchar *key) {
	gchar *value;
	gchar *full_key;

	full_key = panel_gconf_global_config_get_full_key (key);
	value = gconf_client_get_string (panel_gconf_get_client (),
					 full_key,
					 NULL);
	g_free (full_key);
	return value;
}

void 
panel_gconf_global_config_set_int (const gchar *key, gint value) {
	gchar *full_key;

	full_key = panel_gconf_global_config_get_full_key (key);
	gconf_client_set_int (panel_gconf_get_client (),
			      full_key,
			      value,
			      NULL);
			      

	g_free (full_key);
	return;
}

void 
panel_gconf_global_config_set_bool (const gchar *key, gboolean value) {
	gchar *full_key;

	full_key = panel_gconf_global_config_get_full_key (key);
	gconf_client_set_bool (panel_gconf_get_client (),
			       full_key,
			       value,
			       NULL);

	g_free (full_key);
	return;
}

void 
panel_gconf_global_config_set_string (const gchar *key, const gchar *value) {
	gchar *full_key;

	full_key = panel_gconf_global_config_get_full_key (key);
	gconf_client_set_string (panel_gconf_get_client (),
				 full_key,
				 value,
				 NULL);

	g_free (full_key);
	return;
}

void 
panel_gconf_notify_add (const gchar *key, GConfClientNotifyFunc notify_func, gpointer user_data) {
	gconf_client_notify_add (panel_gconf_get_client (),
				 key,
				 notify_func,
				 user_data,
				 NULL,
				 NULL);
	return;
}

void
panel_gconf_add_dir (const gchar *key) {
	gconf_client_add_dir (panel_gconf_get_client (),
			      key,
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);
	return;
}
