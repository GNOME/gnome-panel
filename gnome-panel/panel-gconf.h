/*
 * panel-gconf.h: panel gconf utility methods
 *
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 *      Glynn Foster <glynn.foster@sun.com>
 */

#ifndef __PANEL_GCONF_H__
#define __PANEL_GCONF_H__

#include <gconf/gconf-client.h>

#include "panel-enums.h"

#define PANEL_CONFIG_DIR     "/apps/panel"
#define PANEL_SCHEMAS_DIR    "/schemas/apps/panel"
#define PANEL_DEFAULTS_DIR   "/apps/panel/default_setup"
#define PANEL_OLD_CONFIG_DIR "/apps/panel/profiles/default"

G_BEGIN_DECLS

GConfClient *panel_gconf_get_client          (void);

const char  *panel_gconf_sprintf             (const char        *format, ...) G_GNUC_PRINTF (1, 2);
const char  *panel_gconf_basename            (const char        *key);
char        *panel_gconf_dirname             (const char        *key);
const char  *panel_gconf_global_key          (const char        *key);
const char  *panel_gconf_general_key         (const char        *key);
const char  *panel_gconf_full_key            (PanelGConfKeyType  type,
					      const char        *id,
					      const char        *key);
const char  *panel_gconf_key_type_to_id_list (PanelGConfKeyType  type);

guint		panel_gconf_notify_add             (const char            *key,
						    GConfClientNotifyFunc  notify_func,
						    gpointer               user_data);
guint		panel_gconf_notify_add_while_alive (const char            *key,
						    GConfClientNotifyFunc  notify_func,
						    GObject               *alive_object);

void            panel_gconf_copy_dir      (GConfClient  *client,
					   const char   *src_dir,
					   const char   *dest_dir);

void            panel_gconf_associate_schemas_in_dir       (GConfClient *client,
							    const char  *profile_dir,
							    const char  *schema_dir);

gint            panel_gconf_value_strcmp (gconstpointer a,
					  gconstpointer b);

G_END_DECLS

#endif /* __PANEL_GCONF_H__ */
