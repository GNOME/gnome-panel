#ifndef PANEL_GCONF_H
#define PANEL_CONF_H

#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PANEL_GCONF_DEFAULT_PROFILE	"medium"

/* FIXME : I guess we really need to do some sort of error checking with all of this */

GConfClient*	panel_gconf_get_client (void);

/* Global Configuration */
gchar *		panel_gconf_global_config_get_full_key (const gchar *key);

/* User defined Profiles */
gchar * 	panel_gconf_general_profile_get_full_key (const gchar * profile, const gchar *key);
gchar *		panel_gconf_panel_profile_get_full_key (const gchar *profile, const gchar *panel_id, const gchar *key);
gchar *		panel_gconf_applets_profile_get_full_key (const gchar *profile, const gchar *applet_id, const gchar *key);
gchar *		panel_gconf_objects_profile_get_full_key (const gchar *profile, const gchar *object_id, const gchar *key);

gchar *         panel_gconf_objects_get_full_key (const gchar *profile, const gchar *object_id, const gchar *key, gboolean use_default);

/* Default Profiles */
gchar *		panel_gconf_general_default_profile_get_full_key (const gchar *profile, const gchar *key);
gchar *		panel_gconf_panel_default_profile_get_full_key (const gchar *profile, const gchar *panel_id, const gchar *key);
gchar *		panel_gconf_applets_default_profile_get_full_key (const gchar *profile, const gchar *applet_id, const gchar *key);
gchar *		panel_gconf_objects_default_profile_get_full_key (const gchar *profile, const gchar *object_id, const gchar *key);

GSList         *panel_gconf_all_global_entries (void);

gint		panel_gconf_get_int (const gchar *key, gint default_val);
gboolean	panel_gconf_get_bool (const gchar *key, gboolean default_val);
gchar*		panel_gconf_get_string (const gchar *key, const gchar *default_val);

void 		panel_gconf_set_int (const gchar *key, gint value);
void 		panel_gconf_set_bool (const gchar *key, gboolean value);
void 		panel_gconf_set_string (const gchar *key, const gchar *value);

guint		panel_gconf_notify_add (const gchar *key, GConfClientNotifyFunc notify_func, gpointer user_data);
guint		panel_gconf_notify_add_while_alive (const gchar *key, GConfClientNotifyFunc notify_func, GObject *alive_object);

void 		panel_gconf_add_dir (const gchar *key);

gboolean	panel_gconf_dir_exists (const gchar *key);

void		panel_gconf_directory_recursive_clean (GConfClient *client, const gchar *dir);
G_END_DECLS

#endif /* PANEL_GCONF_H */
