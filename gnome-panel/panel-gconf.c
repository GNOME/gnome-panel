#include <config.h>
#include <string.h>
#include <glib.h>

#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-client.h>

#include "panel-gconf.h"

#define PANEL_GCONF_DEBUG 1

gchar * 
panel_gconf_global_config_get_full_key (const gchar *key) {
	return g_strdup_printf ("/apps/panel/global/%s", key);
}

gchar *
panel_gconf_general_profile_get_full_key (const gchar *profile, const gchar *key) {
	return g_strdup_printf ("/apps/panel/profiles/%s/general/%s", profile, key);
}

gchar *
panel_gconf_panel_profile_get_full_key (const gchar *profile, const gchar *panel_id, const gchar *key) {
	return g_strdup_printf ("/apps/panel/profiles/%s/panels/%s/%s", profile, panel_id, key);
}

gchar *
panel_gconf_applets_profile_get_full_key (const gchar *profile, const gchar *applet_id, const gchar *key) {
	return g_strdup_printf ("/apps/panel/profiles/%s/applets/%s/%s", profile, applet_id, key);
}

gchar *
panel_gconf_general_default_profile_get_full_key (const gchar *profile, const gchar *key) {
	return g_strdup_printf ("/apps/panel/default-profiles/%s/general/%s", profile, key);
}

gchar *
panel_gconf_panel_default_profile_get_full_key (const gchar *profile, const gchar *panel_id, const gchar *key) {
	return g_strdup_printf ("/apps/panel/default-profiles/%s/panels/%s/%s", profile, panel_id, key);
}

gchar *
panel_gconf_applets_default_profile_get_full_key (const gchar *profile, const gchar *applet_id, const gchar *key) {
	return g_strdup_printf ("/apps/panel/default-profiles/%s/applets/%s/%s", profile, applet_id, key);
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
panel_gconf_get_int (const gchar *key, gint default_val) {
	GConfValue *value;
	
	value =  gconf_client_get (panel_gconf_get_client (), key, NULL);

	if (value != NULL) {
		gint retval;

		retval = gconf_value_get_int (value);
		gconf_value_free (value);
		return retval;
	} else {
		return default_val;
	}
}

gboolean
panel_gconf_get_bool (const gchar *key, gboolean default_val) {
	GConfValue *value;

	value = gconf_client_get (panel_gconf_get_client (), key, NULL);
	
	if (value != NULL) {
		gboolean retval;

		retval = gconf_value_get_bool (value);
		gconf_value_free (value);
		return retval;
	} else {
		return default_val;
	}
}

gchar * 
panel_gconf_get_string (const gchar *key, const gchar *default_val) {
	GConfValue *value;

	value = gconf_client_get (panel_gconf_get_client (), key, NULL);

	if (value != NULL) {
		gchar *retval;
		
		retval = g_strdup (gconf_value_get_string (value));
		gconf_value_free (value);
		return retval;
	} else {
		return default_val;
	}
}

void 
panel_gconf_set_int (const gchar *key, gint value) {

	gconf_client_set_int (panel_gconf_get_client (), key, value, NULL);
			      
	return;
}

void 
panel_gconf_set_bool (const gchar *key, gboolean value) {

	gconf_client_set_bool (panel_gconf_get_client (), key, value, NULL);

	return;
}

void 
panel_gconf_set_string (const gchar *key, const gchar *value) {

	gconf_client_set_string (panel_gconf_get_client (), key, value, NULL);

	return;
}

guint 
panel_gconf_notify_add (const gchar *key, GConfClientNotifyFunc notify_func, gpointer user_data) {
	guint notify_id;
	notify_id = gconf_client_notify_add (panel_gconf_get_client (), 
					     key, 
					     notify_func, 
					     user_data, 
					     NULL, NULL);
	return notify_id;
}

static void
panel_notify_object_dead (gpointer data)
{
	guint notify_id = GPOINTER_TO_UINT (data);
	gconf_client_notify_remove (panel_gconf_get_client (),
				    notify_id);
}

guint
panel_gconf_notify_add_while_alive (const gchar *key, 
				    GConfClientNotifyFunc notify_func, 
				    GObject *alive_object)
{
	guint notify_id;

	g_return_val_if_fail (G_IS_OBJECT (alive_object), 0);

	notify_id = panel_gconf_notify_add (key, notify_func, alive_object);
	if (notify_id > 0) {
		static int cookie = 0;
		char *str;
		/* eek a hack, we want a unique key each time,
		 * how else do we hook into the destruction of an object?? */
		str = g_strdup_printf ("PanelGConfNotify-%d", cookie++);
		g_object_set_data_full (alive_object,
					str,
					GUINT_TO_POINTER (notify_id),
					panel_notify_object_dead);
		g_free (str);
	}
	return notify_id;
}

void
panel_gconf_add_dir (const gchar *key) {
	gconf_client_add_dir (panel_gconf_get_client (),
			      key,
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);
	return;
}

gboolean
panel_gconf_dir_exists (const gchar *key) {
	return gconf_client_dir_exists (panel_gconf_get_client (),
				 	key,
				 	NULL);
}

void
panel_gconf_panel_profile_set_int (const gchar *profile, const gchar *panel_id, const gchar *key, gint value) {
	gchar *panel_profile_key;

	panel_profile_key = panel_gconf_panel_profile_get_full_key (profile, panel_id, key);

	panel_gconf_set_int (panel_profile_key, value);	
	return;
}

void
panel_gconf_panel_profile_set_bool (const gchar *profile, const gchar *panel_id, const gchar *key, gboolean value) {
	gchar *panel_profile_key;

	panel_profile_key = panel_gconf_panel_profile_get_full_key (profile, panel_id, key);

	panel_gconf_set_bool (panel_profile_key, value);	

}

void
panel_gconf_panel_profile_set_string (const gchar *profile, const gchar *panel_id, const gchar *key, const gchar *value) {
	gchar *panel_profile_key;

	panel_profile_key = panel_gconf_panel_profile_get_full_key (profile, panel_id, key);

	panel_gconf_set_string (panel_profile_key, value);	
	return;
}

gchar *
panel_gconf_panel_profile_get_conditional_string (const gchar *profile, const gchar *panel_id, const gchar *key, gboolean use_default, const gchar *default_val) {
	gchar *panel_profile_key;
	gchar *return_val;

	/* FIXME: Make this check screen sizes and stuff */
                if (use_default) {
                        panel_profile_key = panel_gconf_panel_default_profile_get_full_key ("medium", panel_id, key);
		}
                else {
                        panel_profile_key = panel_gconf_panel_profile_get_full_key (profile, panel_id, key);
		}

	return_val = panel_gconf_get_string (panel_profile_key, default_val);
	g_free (panel_profile_key);
	return return_val;
}

gint
panel_gconf_panel_profile_get_conditional_int (const gchar *profile, const gchar *panel_id, const gchar *key, gboolean use_default, gint default_val) {
	gchar *panel_profile_key;
	gint return_val;

	/* FIXME: Make this check screen sizes and stuff */
                if (use_default) {
                        panel_profile_key = panel_gconf_panel_default_profile_get_full_key ("medium", panel_id, key);
		}
                else  {
                        panel_profile_key = panel_gconf_panel_profile_get_full_key (profile, panel_id, key);
		}
	
	return_val = panel_gconf_get_int (panel_profile_key, default_val);
	g_free (panel_profile_key);
	return return_val;
}

gboolean
panel_gconf_panel_profile_get_conditional_bool (const gchar *profile, const gchar *panel_id, const gchar *key, gboolean use_default, gboolean default_val) {
	gchar *panel_profile_key;
	gboolean return_val;

	/* FIXME: Make this check screen sizes and stuff */
                if (use_default) {
                        panel_profile_key = panel_gconf_panel_default_profile_get_full_key ("medium", panel_id, key);
		}
                else {
                        panel_profile_key = panel_gconf_panel_profile_get_full_key (profile, panel_id, key);
		}

	return_val = panel_gconf_get_bool (panel_profile_key, default_val);
	g_free (panel_profile_key);
	return return_val;
}
