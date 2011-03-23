/*
 * panel-config-global.c: panel global configuration module
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

#include <config.h>

#include "panel-config-global.h"

#include <string.h>
#include <gconf/gconf.h>

#include "panel-globals.h"
#include "panel-gconf.h"

typedef struct {
	guint               tooltips_enabled : 1;
} GlobalConfig;

static GlobalConfig global_config = { 0, };
static gboolean global_config_initialised = FALSE;

gboolean
panel_global_config_get_tooltips_enabled (void)
{
	g_assert (global_config_initialised == TRUE);

	return global_config.tooltips_enabled;
}

static void
panel_global_config_set_entry (GConfEntry *entry)
{
	GConfValue *value;
	const char *key;

	g_return_if_fail (entry != NULL);

	value = gconf_entry_get_value (entry);
	key   = panel_gconf_basename (gconf_entry_get_key (entry));

	if (!value || !key)
		return;

	if (strcmp (key, "tooltips_enabled") == 0)
		global_config.tooltips_enabled =
				gconf_value_get_bool (value);
}

static void
panel_global_config_notify (GConfClient *client,
			    guint        cnxn_id,
			    GConfEntry  *entry,
			    gpointer     user_data)
{
        panel_global_config_set_entry (entry);
}

void
panel_global_config_load (void)
{
	GConfClient *client;
	GSList      *l, *entries;
	const char  *key = "/apps/panel/global";

	client = panel_gconf_get_client ();

	gconf_client_add_dir (client, key, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	entries = gconf_client_all_entries (client, key, NULL);

	for (l = entries; l; l = l->next) {
		panel_global_config_set_entry (l->data);
		gconf_entry_unref (l->data);
	}
	g_slist_free (entries);

	gconf_client_notify_add (client, key, panel_global_config_notify, NULL, NULL, NULL);

	global_config_initialised = TRUE;
}
