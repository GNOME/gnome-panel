/*  gconf-extensions.c
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf-value.h>

#include "gconf-extensions.h"

static GConfClient *gconf_client = NULL;

void
gconf_extensions_client_setup()
{
    if(!gconf_is_initialized())
    {
        char *argv[] = { "panel-menu-preferences", NULL };
        if(!gconf_init (1, argv, NULL))
            exit(1);
    }
    if(gconf_client == NULL)
    {
        gconf_client = gconf_client_get_default();
        g_atexit(gconf_extensions_client_free);
    }
}

void
gconf_extensions_client_free()
{
    if(gconf_client != NULL)
    {
        g_object_unref(G_OBJECT(gconf_client));
        gconf_client = NULL;
    }
}

GConfClient *
gconf_extensions_client_get()
{
    return(gconf_client);
}

void
gconf_extensions_set_boolean(gchar *path, gchar *key, gboolean boolean_value)
{
    GConfClient *client;
    GError *error = NULL;
    gchar *full;
    
    g_return_if_fail(path != NULL);
    g_return_if_fail(key != NULL);

    client = gconf_extensions_client_get();
    g_return_if_fail(client != NULL);

    full = g_strconcat(path, "/", key, NULL);
    gconf_client_set_bool(client, full, boolean_value, &error);
    g_free(full);
    gconf_extensions_handle_error(&error);
}

gboolean
gconf_extensions_get_boolean(gchar *path, const char *key, gboolean default_value)
{
    gboolean result;
    GConfClient *client;
    GError *error = NULL;
    gchar *full;

    g_return_val_if_fail(path != NULL, FALSE);
    g_return_val_if_fail(key != NULL, FALSE);

    client = gconf_extensions_client_get();
    g_return_val_if_fail(client != NULL, FALSE);

    full = g_strconcat(path, "/", key, NULL);
    if(gconf_client_dir_exists(client, full, NULL))
        result = gconf_client_get_bool(client, path, &error);
    else
        result = default_value;
    g_free(full);

    if(gconf_extensions_handle_error(&error))
    {
        result = default_value;
    }
    return(result);
}

void
gconf_extensions_set_integer(gchar *path, gchar *key, int int_value)
{
    GConfClient *client;
    GError *error = NULL;
    gchar *full;

    g_return_if_fail(path != NULL);
    g_return_if_fail(key != NULL);

    client = gconf_extensions_client_get();
    g_return_if_fail(client != NULL);

    full = g_strconcat(path, "/", key, NULL);
    gconf_client_set_int(client, full, int_value, &error);
    g_free(full);
    gconf_extensions_handle_error (&error);
}

gint
gconf_extensions_get_integer(gchar *path, gchar *key, gint default_value)
{
    int result;
    GConfClient *client;
    GError *error = NULL;
    gchar *full;

    g_return_val_if_fail(path != NULL, 0);
    g_return_val_if_fail(key != NULL, 0);

    client = gconf_extensions_client_get();
    g_return_val_if_fail(client != NULL, 0);

    full = g_strconcat(path, "/", key, NULL);
    if(gconf_client_dir_exists(client, path, NULL))
        result = gconf_client_get_int(client, full, &error);
    else
        result = default_value;
    g_free(full);

    if(gconf_extensions_handle_error(&error))
    {
        result = default_value;
    }
    return(result);
}

void
gconf_extensions_set_float(gchar *path, gchar *key, gfloat float_value)
{
    GConfClient *client;
    GError *error = NULL;
    gchar *full;

    g_return_if_fail(path != NULL);
    g_return_if_fail(key != NULL);

    client = gconf_extensions_client_get();
    g_return_if_fail(client != NULL);

    full = g_strconcat(path, "/", key, NULL);
    gconf_client_set_float(client, full, float_value, &error);
    g_free(full);
    gconf_extensions_handle_error(&error);
}

gfloat
gconf_extensions_get_float(gchar *path, gchar *key, gfloat default_value)
{
    gfloat result;
    GConfClient *client;
    GError *error = NULL;
    gchar *full;

    g_return_val_if_fail(path != NULL, 0);
    g_return_val_if_fail(key != NULL, 0);

    client = gconf_extensions_client_get();
    g_return_val_if_fail(client != NULL, 0);

    full = g_strconcat(path, "/", key, NULL);
    if(gconf_client_dir_exists(client, path, NULL))
        result = gconf_client_get_float(client, key, &error);
    else
        result = default_value;
    g_free(full);

    if(gconf_extensions_handle_error (&error))
    {
        result = default_value;
    }
    return(result);
}

void
gconf_extensions_set_string(gchar *path, gchar *key, gchar *string_value)
{
    GConfClient *client;
    GError *error = NULL;
    gchar *full;

    g_return_if_fail(path != NULL);
    g_return_if_fail(key != NULL);

    client = gconf_extensions_client_get();
    g_return_if_fail(client != NULL);
    
    full = g_strconcat(path, "/", key, NULL);
    gconf_client_set_string(client, full, string_value ? string_value : "", &error);
    g_free(full);
    gconf_extensions_handle_error(&error);
}

gchar *
gconf_extensions_get_string(gchar *path, gchar *key, gchar *default_value)
{
    GConfClient *client;
    GError *error = NULL;
    gchar *result = NULL;
    gchar *full;

    g_return_val_if_fail (key != NULL, NULL);

    client = gconf_extensions_client_get();
    g_return_val_if_fail (client != NULL, NULL);
    
    full = g_strconcat(path, "/", key, NULL);
    if(gconf_client_dir_exists(client, path, NULL))
    {
        result = gconf_client_get_string(client, full, &error);
        gconf_extensions_handle_error(&error);
    }
    g_free(full);
    if(!result) result = g_strdup(default_value ? default_value : "");
    return(result);
}

void
gconf_extensions_suggest_sync()
{
	GConfClient *client;
	GError *error = NULL;

	client = gconf_extensions_client_get();
	g_return_if_fail(client != NULL);

	gconf_client_suggest_sync(client, &error);
	gconf_extensions_handle_error(&error);
}

gboolean
gconf_extensions_handle_error(GError **error)
{
    g_return_val_if_fail (error != NULL, FALSE);

    if(*error != NULL)
    {
        g_warning("GConf error:\n  %s", (*error)->message);
        g_error_free(*error);
        *error = NULL;
        return(TRUE);
    }
    return(FALSE);
}
