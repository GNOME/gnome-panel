#include <config.h>
#include <string.h>
#include <glib.h>

#include <gconf/gconf-client.h>

#include "panel-gconf.h"

static gchar * panel_gconf_global_config_get_full_key (const gchar *key) {
	gchar *full_key;

	full_key = g_strdup_printf ("/apps/panel/%s", key);
	return full_key;
}

GConfClient * panel_gconf_get_client (void) {
	static GConfClient *client = NULL;

        if (!client)
                client = gconf_client_get_default ();

        return client;
}

gint panel_gconf_global_config_get_int (const gchar *key) {
	return gconf_client_get_int (panel_gconf_get_client (),
				     panel_gconf_global_config_get_full_key (key),
				     NULL);
}

gboolean panel_gconf_global_config_get_bool (const gchar *key) {
	return gconf_client_get_bool (panel_gconf_get_client (),
				      panel_gconf_global_config_get_full_key (key),
				      NULL);
}

gchar * panel_gconf_global_config_get_string (const gchar *key) {
	return gconf_client_get_string (panel_gconf_get_client (),
					panel_gconf_global_config_get_full_key (key),
					NULL);
}
