/*  gconf-extensions.h
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

#ifndef _GCONF_EXTENSIONS_H_
#define _GCONF_EXTENSIONS_H_

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf-value.h>

G_BEGIN_DECLS

void gconf_extensions_client_setup(void);
void gconf_extensions_client_free(void);
GConfClient *gconf_extensions_client_get(void);
void gconf_extensions_set_boolean(gchar *path, gchar *key, gboolean boolean_value);
gboolean gconf_extensions_get_boolean(gchar *path, const char *key, gboolean default_value);
void gconf_extensions_set_integer(gchar *path, gchar *key, int int_value);
gint gconf_extensions_get_integer(gchar *path, gchar *key, gint default_value);
void gconf_extensions_set_float(gchar *path, gchar *key, gfloat float_value);
gfloat gconf_extensions_get_float(gchar *path, gchar *key, gfloat default_value);
void gconf_extensions_set_string(gchar *path, gchar *key, gchar *string_value);
gchar *gconf_extensions_get_string(gchar *path, gchar *key, gchar *default_value);
void gconf_extensions_suggest_sync(void);
gboolean gconf_extensions_handle_error(GError **error);

G_END_DECLS

#endif
