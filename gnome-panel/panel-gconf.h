#ifndef PANEL_GCONF_H
#define PANEL_CONF_H

#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GConfClient*	panel_gconf_get_client (void);
gint		panel_gconf_global_config_get_int (const gchar *key);
gboolean	panel_gconf_global_config_get_bool (const gchar *bool);
	
G_END_DECLS

#endif /* PANEL_GCONF_H */
