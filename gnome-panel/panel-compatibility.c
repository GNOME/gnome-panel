/*
 * panel-compatibility.c: panel backwards compatibility support
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "string.h"

#include <libpanel-util/panel-glib.h>

#include "panel-compatibility.h"

#include "panel-profile.h"
#include "panel-applet-frame.h"
#include "panel-applets-manager.h"
#include "panel-util.h"

void
panel_compatibility_migrate_applications_scheme (GConfClient *client,
						 const char  *key)
{
	char *location;

	location = gconf_client_get_string (client, key, NULL);

	if (!location)
		return;

	if (!strncmp (location, "applications:", strlen ("applications:")) ||
	    !strncmp (location, "applications-all-users:", strlen ("applications-all-users:")) ||
	    !strncmp (location, "all-applications:", strlen ("all-applications:")) ||
	    !strncmp (location, "preferences:", strlen ("preferences:")) ||
	    !strncmp (location, "preferences-all-users:", strlen ("preferences-all-users:")) ||
	    !strncmp (location, "all-preferences:", strlen ("all-preferences:")) ||
	    !strncmp (location, "system-settings:", strlen ("system-settings:")) ||
	    !strncmp (location, "server-settings:", strlen ("server-settings:"))) {
		char *basename;
		char *new_location;

		basename = g_path_get_basename (location);
		new_location = panel_g_lookup_in_applications_dirs (basename);
		g_free (basename);

		if (new_location != NULL) {
			gconf_client_set_string (client, key,
						 new_location, NULL);
			g_free (new_location);
		}
	}

	g_free (location);
}

void
panel_compatibility_migrate_screenshot_action (GConfClient *client,
					      const char  *id)
{
	const char *key;

	panel_profile_remove_from_list (PANEL_GCONF_OBJECTS, id);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, id,
				    "launcher_location");
	gconf_client_set_string (client, key, "gnome-screenshot.desktop", NULL);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, id,
				    "object_type");
	//FIXME: ideally, we would use panel_object_type_map, but it's private
	//in panel-profile.c
	gconf_client_set_string (client, key, "launcher-object", NULL);

	panel_profile_add_to_list (PANEL_GCONF_OBJECTS, id);
}

void
panel_compatiblity_migrate_settings_menu_button (GConfClient *client,
						 const char  *id)
{
	const char *key;

	panel_profile_remove_from_list (PANEL_GCONF_OBJECTS, id);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, id,
				    "launcher_location");
	gconf_client_set_string (client, key,
				 "gnome-control-center.desktop", NULL);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, id,
				    "object_type");
	gconf_client_set_string (client, key, "launcher-object", NULL);

	panel_profile_add_to_list (PANEL_GCONF_OBJECTS, id);
}

gchar *
panel_compatibility_get_applet_iid (const gchar *id)
{
	GConfClient *client = panel_gconf_get_client ();
	PanelAppletInfo *info;
	const char *key;
	gchar *applet_iid;
	gboolean needs_migration;
	const char *iid;

	/*
	 * There are two compatibility steps here:
	 *
	 * 1) we need to migrate from bonobo_iid to applet_iid if there's no
	 *    value in the applet_iid key. Always.
	 *
	 * 2) we need to try to migrate the iid to a new iid. We can't assume
	 *    that the fact that the applet_iid key was used mean anything
	 *    since the value there could well be a bonobo iid.
	 *    The reason we really have to try to migrate first is this case:
	 *    if an applet was added with the bonobo iid but gets ported later
	 *    to dbus, then the reference to the bonobo iid will only be valid
	 *    as an old reference.
	 *    And if migration fails, we just use the iid as it is.
	 */

	needs_migration = FALSE;

	key = panel_gconf_full_key (PANEL_GCONF_APPLETS, id, "applet_iid");
	applet_iid = gconf_client_get_string (client, key, NULL);

	if (!applet_iid || !applet_iid[0]) {
		needs_migration = TRUE;

		key = panel_gconf_full_key (PANEL_GCONF_APPLETS, id, "bonobo_iid");
		applet_iid = gconf_client_get_string (client, key, NULL);

		if (!applet_iid || !applet_iid[0])
			return NULL;
	}

	info = panel_applets_manager_get_applet_info_from_old_id (applet_iid);
	if (!info)
		info = panel_applets_manager_get_applet_info (applet_iid);

	if (!info)
		return NULL;

	iid = panel_applet_info_get_iid (info);

	/* migrate if the iid in the configuration is different than the real
	 * iid that will get used */
	if (!g_str_equal (iid, applet_iid))
		needs_migration = TRUE;

	g_free (applet_iid);

	if (needs_migration) {
		key = panel_gconf_full_key (PANEL_GCONF_APPLETS, id, "applet_iid");
		gconf_client_set_string (client, key, iid, NULL);
	}

	return g_strdup (iid);
}
