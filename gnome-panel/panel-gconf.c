#include <config.h>
#include <string.h>
#include <glib.h>

#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-client.h>

#include "panel-gconf.h"

static gchar * panel_gconf_global_config_get_full_key (const gchar *key) {
	return g_strdup_printf ("/apps/panel/%s", key);
}

GConfClient * panel_gconf_get_client (void) {
	static GConfClient *client = NULL;

        if (!client)
                client = gconf_client_get_default ();

        return client;
}

gchar * panel_gconf_get_session_key (void) {
	static gchar *session_key;
	if (session_key == NULL)  {
		const gchar *panel_client_id;
		panel_client_id = gnome_client_get_id (gnome_master_client ());	
		session_key =  g_strdup_printf ("/apps/panel/sessions/%s/", panel_client_id);
	}
	return session_key;
}

gint panel_gconf_global_config_get_int (const gchar *key) {
	gint value;
	gchar *full_key;

	full_key = panel_gconf_global_config_get_full_key (key);
	value =  gconf_client_get_int (panel_gconf_get_client (),
				       full_key,
				       NULL);
	g_free (full_key);
	return value;
}

gboolean panel_gconf_global_config_get_bool (const gchar *key) {
	gboolean value;
	gchar *full_key;
	
	full_key = panel_gconf_global_config_get_full_key (key);
	value = gconf_client_get_bool (panel_gconf_get_client (),
				       full_key,
				       NULL);
	g_free (full_key);
	return value;
}

gchar * panel_gconf_global_config_get_string (const gchar *key) {
	gchar *value;
	gchar *full_key;

	full_key = panel_gconf_global_config_get_full_key (key);
	value = gconf_client_get_string (panel_gconf_get_client (),
					 full_key,
					 NULL);
	g_free (full_key);
	return value;
}

void panel_gconf_global_config_set_int (const gchar *key, gint value) {
	gchar *full_key;

	full_key = panel_gconf_global_config_get_full_key (key);
	gconf_client_set_int (panel_gconf_get_client (),
			      full_key,
			      value,
			      NULL);
			      

	g_free (full_key);
	return;
}

void panel_gconf_global_config_set_bool (const gchar *key, gboolean value) {
	gchar *full_key;

	full_key = panel_gconf_global_config_get_full_key (key);
	gconf_client_set_bool (panel_gconf_get_client (),
			       full_key,
			       value,
			       NULL);

	g_free (full_key);
	return;
}

void panel_gconf_global_config_set_string (const gchar *key, const gchar *value) {
	gchar *full_key;

	full_key = panel_gconf_global_config_get_full_key (key);
	gconf_client_set_string (panel_gconf_get_client (),
				 full_key,
				 value,
				 NULL);

	g_free (full_key);
	return;
}
