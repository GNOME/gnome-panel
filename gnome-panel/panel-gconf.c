#include <config.h>
#include <string.h>
#include <glib.h>

#include <gconf/gconf-client.h>

#include "panel-gconf.h"

GConfClient * panel_gconf_get_client (void) {
	static GConfClient *client = NULL;

        if (!client)
                client = gconf_client_get_default ();

        return client;
}

gint panel_gconf_get_int (const gchar *key) {
	return 0;
}

gboolean panel_gconf_get_bool (const gchar *key) {
	return TRUE;
}
