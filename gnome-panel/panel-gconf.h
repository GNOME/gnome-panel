#ifndef PANEL_GCONF_H
#define PANEL_GCONF_H

#include <gconf/gconf-client.h>

G_BEGIN_DECLS

typedef enum {
	PANEL_GCONF_PANELS,
	PANEL_GCONF_OBJECTS,
	PANEL_GCONF_APPLETS
} PanelGConfKeyType;

G_CONST_RETURN char *panel_gconf_get_profile (void);

GConfClient         *panel_gconf_get_client  (void);

G_CONST_RETURN char *panel_gconf_sprintf     (const gchar *format,
					      ...);
G_CONST_RETURN char *panel_gconf_global_key  (const gchar *key);
G_CONST_RETURN char *panel_gconf_general_key (const gchar *profile,
					      const gchar *key);
G_CONST_RETURN char *panel_gconf_full_key    (PanelGConfKeyType  type,
					      const gchar       *profile,
					      const gchar       *panel_id,
					      const gchar       *key);

GSList         *panel_gconf_all_global_entries (void);

int		panel_gconf_get_int     (const char *key,
					 int         default_val);
gboolean	panel_gconf_get_bool    (const char *key,
					 gboolean    default_val);
char*		panel_gconf_get_string  (const char *key,
					 const char *default_val);

void 		panel_gconf_set_int     (const gchar *key,
					 int         value);
void 		panel_gconf_set_bool    (const char *key,
					 gboolean    value);
void 		panel_gconf_set_string  (const char *key,
					 const char *value);

guint		panel_gconf_notify_add             (const gchar           *key,
						    GConfClientNotifyFunc  notify_func,
						    gpointer               user_data);
guint		panel_gconf_notify_add_while_alive (const gchar           *key,
						    GConfClientNotifyFunc  notify_func,
						    GObject               *alive_object);

void 		panel_gconf_add_dir       (const gchar *key);

void		panel_gconf_clean_dir     (GConfClient *client,
					   const gchar *dir);

void            panel_gconf_setup_profile (const char  *profile);

G_END_DECLS

#endif /* PANEL_GCONF_H */
