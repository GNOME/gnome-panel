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

G_CONST_RETURN char *panel_gconf_sprintf     (const char *format,
					      ...) G_GNUC_PRINTF (1, 2);
G_CONST_RETURN char *panel_gconf_global_key  (const char *key);
G_CONST_RETURN char *panel_gconf_general_key (const char *profile,
					      const char *key);
G_CONST_RETURN char *panel_gconf_full_key    (PanelGConfKeyType  type,
					      const char        *profile,
					      const char        *panel_id,
					      const char        *key);


GSList         *panel_gconf_all_global_entries (void);

int		panel_gconf_get_int     (const char *key,
					 int         default_val);
gboolean	panel_gconf_get_bool    (const char *key,
					 gboolean    default_val);
char*		panel_gconf_get_string  (const char *key,
					 const char *default_val);

void 		panel_gconf_set_int     (const char *key,
					 int         value);
void 		panel_gconf_set_bool    (const char *key,
					 gboolean    value);
void 		panel_gconf_set_string  (const char *key,
					 const char *value);

guint		panel_gconf_notify_add             (const char            *key,
						    GConfClientNotifyFunc  notify_func,
						    gpointer               user_data);
guint		panel_gconf_notify_add_while_alive (const char            *key,
						    GConfClientNotifyFunc  notify_func,
						    GObject               *alive_object);

void 		panel_gconf_add_dir       (const char  *key);

void		panel_gconf_clean_dir     (GConfClient *client,
					   const char  *dir);

void            panel_gconf_setup_profile (const char  *profile);

char           *panel_gconf_load_default_config_for_screen (PanelGConfKeyType   type,
							    const char         *profile,
							    const char         *id,
							    int                 screen,
							    GError            **error);

G_END_DECLS

#endif /* PANEL_GCONF_H */
