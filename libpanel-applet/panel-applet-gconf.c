/*
 * panel-applet-gconf.c: panel applet preferences handling.
 *
 * Copyright (C) 2001-2003 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *     Mark McLoughlin <mark@skynet.ie>
 */

#include <gconf/gconf-client.h>

#include "panel-applet-gconf.h"

static GConfClient *
panel_applet_gconf_get_client (void)
{
	static GConfClient *client = NULL;

	if (!client)
		client = gconf_client_get_default ();

	return client;
}

gchar *
panel_applet_gconf_get_full_key (PanelApplet *applet,
				 const gchar *key)
{
	gchar *prefs_key;
	gchar *full_key;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	if (!key)
		return NULL;

	prefs_key = panel_applet_get_preferences_key (applet);

	full_key = g_strdup_printf ("%s/%s", prefs_key, key);

	g_free (prefs_key);

	return full_key;
}

void
panel_applet_gconf_set_bool (PanelApplet  *applet,
			     const gchar  *key,
			     gboolean      the_bool,
			     GError      **opt_error)
{
	GConfClient  *client;
	gchar        *full_key;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	gconf_client_set_bool (client, full_key, the_bool, error);

	g_free (full_key);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": gconf error : '%s'", our_error->message);
		g_error_free (our_error);
	}
}

void
panel_applet_gconf_set_int (PanelApplet  *applet,
			    const gchar  *key,
			    gint          the_int,
			    GError      **opt_error)
{
	GConfClient  *client;
	gchar        *full_key;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	gconf_client_set_int (client, full_key, the_int, error);

	g_free (full_key);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": gconf error : '%s'", our_error->message);
		g_error_free (our_error);
	}
}

void
panel_applet_gconf_set_string (PanelApplet  *applet,
			       const gchar  *key,
			       const gchar  *the_string,
			       GError      **opt_error)
{
	GConfClient  *client;
	gchar        *full_key;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	gconf_client_set_string (client, full_key, the_string, error);

	g_free (full_key);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": gconf error : '%s'", our_error->message);
		g_error_free (our_error);
	}
}

void
panel_applet_gconf_set_float (PanelApplet  *applet,
			      const gchar  *key,
			      gdouble       the_float,
			      GError      **opt_error)
{
	GConfClient  *client;
	gchar        *full_key;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	gconf_client_set_float (client, full_key, the_float, error);

	g_free (full_key);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": gconf error : '%s'", our_error->message);
		g_error_free (our_error);
	}
}

void
panel_applet_gconf_set_list (PanelApplet     *applet,
			     const gchar     *key,
			     GConfValueType   list_type,
			     GSList          *list,
			     GError         **opt_error)
{
	GConfClient  *client;
	gchar        *full_key;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	gconf_client_set_list (client, full_key, list_type, list, error);

	g_free (full_key);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": gconf error : '%s'", our_error->message);
		g_error_free (our_error);
	}
}

void
panel_applet_gconf_set_value (PanelApplet  *applet,
			      const gchar  *key,
			      GConfValue   *value,
			      GError      **opt_error)
{
	GConfClient  *client;
	gchar        *full_key;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	gconf_client_set (client, full_key, value, error);

	g_free (full_key);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": gconf error : '%s'", our_error->message);
		g_error_free (our_error);
	}
}

gboolean
panel_applet_gconf_get_bool (PanelApplet  *applet,
			     const gchar  *key,
			     GError      **opt_error)
{
	GConfClient  *client;
	gchar        *full_key;
	gboolean      retval;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), FALSE);

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	retval = gconf_client_get_bool (client, full_key, error);

	g_free (full_key);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": gconf error : '%s'", our_error->message);
		g_error_free (our_error);
	}

	return retval;
}

gint
panel_applet_gconf_get_int (PanelApplet  *applet,
			    const gchar  *key,
			    GError      **opt_error)
{
	GConfClient  *client;
	gchar        *full_key;
	gint          retval;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), -1);

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	retval = gconf_client_get_int (client, full_key, error);

	g_free (full_key);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": gconf error : '%s'", our_error->message);
		g_error_free (our_error);
	}

	return retval;
}

gchar *
panel_applet_gconf_get_string (PanelApplet  *applet,
			       const gchar  *key,
			       GError      **opt_error)
{
	GConfClient  *client;
	gchar        *full_key;
	gchar        *retval;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	retval = gconf_client_get_string (client, full_key, error);

	g_free (full_key);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": gconf error : '%s'", our_error->message);
		g_error_free (our_error);
	}

	return retval;
}

gdouble
panel_applet_gconf_get_float (PanelApplet  *applet,
			      const gchar  *key,
			      GError      **opt_error)
{
	GConfClient  *client;
	gchar        *full_key;
	gdouble       retval;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), 0.0);

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	retval = gconf_client_get_float (client, full_key, error);

	g_free (full_key);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": gconf error : '%s'", our_error->message);
		g_error_free (our_error);
	}

	return retval;
}

GConfValue *
panel_applet_gconf_get_value (PanelApplet  *applet,
			      const gchar  *key,
			      GError      **opt_error)
{
	GConfClient  *client;
	gchar        *full_key;
	GConfValue   *retval;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	retval = gconf_client_get (client, full_key, error);

	g_free (full_key);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": gconf error : '%s'", our_error->message);
		g_error_free (our_error);
	}

	return retval;
}

GSList *
panel_applet_gconf_get_list (PanelApplet     *applet,
			     const gchar     *key,
			     GConfValueType   list_type,
			     GError         **opt_error)
{
	GConfClient  *client;
	gchar        *full_key;
	GSList       *retval;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	retval = gconf_client_get_list (client, full_key, list_type, error);

	g_free (full_key);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": gconf error : '%s'", our_error->message);
		g_error_free (our_error);
	}

	return retval;
}
