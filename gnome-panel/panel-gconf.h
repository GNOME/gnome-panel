#ifndef PANEL_GCONF_H
#define PANEL_CONF_H

#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GConfClient*	panel_gconf_get_client (void);

/* Global Configuration */
gchar *		panel_gconf_global_config_get_full_key (const gchar *key);

/* User defined Profiles */
gchar * 	panel_gconf_general_profile_get_full_key (const gchar * profile, const gchar *key);
gchar *		panel_gconf_panel_profile_get_full_key (const gchar *profile, const gchar *panel_id, const gchar *key);
gchar *		panel_gconf_applets_profile_get_full_key (const gchar *profile, const gchar *applet_id, const gchar *key);

/* Default Profiles */
gchar *		panel_gconf_general_default_profile_get_full_key (const gchar *profile, const gchar *key);
gchar *		panel_gconf_panel_default_profile_get_full_key (const gchar *profile, const gchar *panel_id, const gchar *key); 
gchar *		panel_gconf_applets_default_profile_get_full_key (const gchar *profile, const gchar *applet_id, const gchar *key);

gboolean	panel_gconf_panel_profile_get_conditional_bool (const gchar *profile, const gchar *panel_id, const gchar *key, gboolean use_default);
gchar *		panel_gconf_panel_profile_get_conditional_string (const gchar *profile, const gchar *panel_id, const gchar *key, gboolean use_default);
gint		panel_gconf_panel_profile_get_conditional_int (const gchar *profile, const gchar *panel_id, const gchar *key, gboolean use_default);

/* FIXME - Probably not need anymore */
gchar *		panel_gconf_get_session_key (void);

GSList         *panel_gconf_all_global_entries (void);

gint		panel_gconf_get_int (const gchar *key);
gboolean	panel_gconf_get_bool (const gchar *key);
gchar*		panel_gconf_get_string (const gchar *key);

void 		panel_gconf_set_int (const gchar *key, gint value);
void 		panel_gconf_set_bool (const gchar *key, gboolean value);
void 		panel_gconf_set_string (const gchar *key, const gchar *value);

guint		panel_gconf_notify_add (const gchar *key, GConfClientNotifyFunc notify_func, gpointer user_data);
guint		panel_gconf_notify_add_while_alive (const gchar *key, GConfClientNotifyFunc notify_func, GObject *alive_object);

void 		panel_gconf_add_dir (const gchar *key);

gboolean	panel_gconf_dir_exists (const gchar *key);

G_END_DECLS

#endif /* PANEL_GCONF_H */
