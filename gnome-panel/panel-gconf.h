#ifndef PANEL_GCONF_H
#define PANEL_CONF_H

#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GConfClient*	panel_gconf_get_client (void);

gchar *		panel_gconf_get_session_key (void);

GSList         *panel_gconf_all_global_entries (void);

gint		panel_gconf_global_config_get_int (const gchar *key);
gboolean	panel_gconf_global_config_get_bool (const gchar *key);
gchar*		panel_gconf_global_config_get_string (const gchar *key);

void 		panel_gconf_global_config_set_int (const gchar *key, 
						   gint value);
void 		panel_gconf_global_config_set_bool (const gchar *key, 
						    gboolean value);
void 		panel_gconf_global_config_set_string (const gchar *key, 
						      const gchar *value);

void		panel_gconf_notify_add (const gchar *key, 
					GConfClientNotifyFunc notify_func, 
					gpointer user_data);

void 		panel_gconf_add_dir (const gchar *key);
G_END_DECLS

#endif /* PANEL_GCONF_H */
