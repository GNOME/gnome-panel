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

/**
 * SECTION:gconf
 * @short_description: utility API to use GConf with applets.
 * @stability: Unstable
 *
 * The <function>panel_applet_gconf_*()</function> set of API provides
 * convenience functions to access GConf keys that are specific to an
 * applet instance.
 *
 * Keep in mind that it might be worth considering using <link
 * linkend="getting-started.settings">global settings</link> for your applet,
 * instead of settings specific to an instance.
 */

static GConfClient *
panel_applet_gconf_get_client (void)
{
	static GConfClient *client = NULL;

	if (!client)
		client = gconf_client_get_default ();

	return client;
}

/**
 * panel_applet_gconf_get_full_key:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 *
 * Returns the full GConf path of @key, in the per-instance GConf directory of
 * @applet. The string should be freed by the caller.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
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

/**
 * panel_applet_gconf_set_bool:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 * @the_bool: new value for @key.
 * @error: a #GError, or %NULL.
 *
 * Convenience wrapper around gconf_client_set_bool() to update @key in the
 * per-instance GConf directory of @applet.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
void
panel_applet_gconf_set_bool (PanelApplet  *applet,
			     const gchar  *key,
			     gboolean      the_bool,
			     GError      **error)
{
	GConfClient  *client;
	gchar        *full_key;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	gconf_client_set_bool (client, full_key, the_bool, error);

	g_free (full_key);
}

/**
 * panel_applet_gconf_set_int:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 * @the_int: new value for @key.
 * @error: a #GError, or %NULL.
 *
 * Convenience wrapper around gconf_client_set_int() to update @key in the
 * per-instance GConf directory of @applet.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
void
panel_applet_gconf_set_int (PanelApplet  *applet,
			    const gchar  *key,
			    gint          the_int,
			    GError      **error)
{
	GConfClient  *client;
	gchar        *full_key;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	gconf_client_set_int (client, full_key, the_int, error);

	g_free (full_key);
}

/**
 * panel_applet_gconf_set_string:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 * @the_string: new value for @key.
 * @error: a #GError, or %NULL.
 *
 * Convenience wrapper around gconf_client_set_string() to update @key in the
 * per-instance GConf directory of @applet.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
void
panel_applet_gconf_set_string (PanelApplet  *applet,
			       const gchar  *key,
			       const gchar  *the_string,
			       GError      **error)
{
	GConfClient  *client;
	gchar        *full_key;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	gconf_client_set_string (client, full_key, the_string, error);

	g_free (full_key);
}

/**
 * panel_applet_gconf_set_float:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 * @the_float: new value for @key.
 * @error: a #GError, or %NULL.
 *
 * Convenience wrapper around gconf_client_set_float() to update @key in the
 * per-instance GConf directory of @applet.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
void
panel_applet_gconf_set_float (PanelApplet  *applet,
			      const gchar  *key,
			      gdouble       the_float,
			      GError      **error)
{
	GConfClient  *client;
	gchar        *full_key;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	gconf_client_set_float (client, full_key, the_float, error);

	g_free (full_key);
}

/**
 * panel_applet_gconf_set_list:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 * @list_type: type of items in @list.
 * @list: new value for @key.
 * @error: a #GError, or %NULL.
 *
 * Convenience wrapper around gconf_client_set_list() to update @key in the
 * per-instance GConf directory of @applet.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
void
panel_applet_gconf_set_list (PanelApplet     *applet,
			     const gchar     *key,
			     GConfValueType   list_type,
			     GSList          *list,
			     GError         **error)
{
	GConfClient  *client;
	gchar        *full_key;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	gconf_client_set_list (client, full_key, list_type, list, error);

	g_free (full_key);
}

/**
 * panel_applet_gconf_set_value:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 * @value: new value for @key.
 * @error: a #GError, or %NULL.
 *
 * Convenience wrapper around gconf_client_set_value() to update @key in the
 * per-instance GConf directory of @applet.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
void
panel_applet_gconf_set_value (PanelApplet  *applet,
			      const gchar  *key,
			      GConfValue   *value,
			      GError      **error)
{
	GConfClient  *client;
	gchar        *full_key;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	gconf_client_set (client, full_key, value, error);

	g_free (full_key);
}

/**
 * panel_applet_gconf_get_bool:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 * @error: a #GError, or %NULL.
 *
 * Convenience wrapper around gconf_client_get_bool() to get the value of @key
 * in the per-instance GConf directory of @applet.
 *
 * Returns: the value of @key.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
gboolean
panel_applet_gconf_get_bool (PanelApplet  *applet,
			     const gchar  *key,
			     GError      **error)
{
	GConfClient  *client;
	gchar        *full_key;
	gboolean      retval;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), FALSE);

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	retval = gconf_client_get_bool (client, full_key, error);

	g_free (full_key);

	return retval;
}

/**
 * panel_applet_gconf_get_int:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 * @error: a #GError, or %NULL.
 *
 * Convenience wrapper around gconf_client_get_int() to get the value of @key
 * in the per-instance GConf directory of @applet.
 *
 * Returns: the value of @key.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
gint
panel_applet_gconf_get_int (PanelApplet  *applet,
			    const gchar  *key,
			    GError      **error)
{
	GConfClient  *client;
	gchar        *full_key;
	gint          retval;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), -1);

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	retval = gconf_client_get_int (client, full_key, error);

	g_free (full_key);

	return retval;
}

/**
 * panel_applet_gconf_get_string:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 * @error: a #GError, or %NULL.
 *
 * Convenience wrapper around gconf_client_get_string() to get the value of @key
 * in the per-instance GConf directory of @applet.
 *
 * Returns: the value of @key. The string should be freed by the caller.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
gchar *
panel_applet_gconf_get_string (PanelApplet  *applet,
			       const gchar  *key,
			       GError      **error)
{
	GConfClient  *client;
	gchar        *full_key;
	gchar        *retval;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	retval = gconf_client_get_string (client, full_key, error);

	g_free (full_key);

	return retval;
}

/**
 * panel_applet_gconf_get_float:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 * @error: a #GError, or %NULL.
 *
 * Convenience wrapper around gconf_client_get_float() to get the value of @key
 * in the per-instance GConf directory of @applet.
 *
 * Returns: the value of @key.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
gdouble
panel_applet_gconf_get_float (PanelApplet  *applet,
			      const gchar  *key,
			      GError      **error)
{
	GConfClient  *client;
	gchar        *full_key;
	gdouble       retval;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), 0.0);

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	retval = gconf_client_get_float (client, full_key, error);

	g_free (full_key);

	return retval;
}

/**
 * panel_applet_gconf_get_value:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 * @error: a #GError, or %NULL.
 *
 * Convenience wrapper around gconf_client_get_value() to get the value of @key
 * in the per-instance GConf directory of @applet.
 *
 * Returns: the value of @key.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
GConfValue *
panel_applet_gconf_get_value (PanelApplet  *applet,
			      const gchar  *key,
			      GError      **error)
{
	GConfClient  *client;
	gchar        *full_key;
	GConfValue   *retval;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	retval = gconf_client_get (client, full_key, error);

	g_free (full_key);

	return retval;
}

/**
 * panel_applet_gconf_get_list:
 * @applet: a #PanelApplet.
 * @key: a GConf key name.
 * @error: a #GError, or %NULL.
 *
 * Convenience wrapper around gconf_client_get_list() to get the value of @key
 * in the per-instance GConf directory of @applet.
 *
 * Returns: the value of @key. The list and its content should be freed by the
 * caller.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
GSList *
panel_applet_gconf_get_list (PanelApplet     *applet,
			     const gchar     *key,
			     GConfValueType   list_type,
			     GError         **error)
{
	GConfClient  *client;
	gchar        *full_key;
	GSList       *retval;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	full_key = panel_applet_gconf_get_full_key (applet, key);

	client = panel_applet_gconf_get_client ();

	retval = gconf_client_get_list (client, full_key, list_type, error);

	g_free (full_key);

	return retval;
}
