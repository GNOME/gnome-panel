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

#include <libpanel-util/panel-glib.h>

#include "panel-applets-manager.h"
#include "panel-schemas.h"

#include "panel-compatibility.h"

gchar *
panel_compatibility_get_applet_iid (GSettings   *settings,
				    const gchar *id)
{
	PanelAppletInfo *info;
	gchar *object_iid;
	gboolean needs_migration;
	const char *iid;

	needs_migration = FALSE;

	object_iid = g_settings_get_string (settings, PANEL_OBJECT_IID_KEY);
	if (PANEL_GLIB_STR_EMPTY (object_iid)) {
		g_free (object_iid);
		return NULL;
	}

	info = panel_applets_manager_get_applet_info_from_old_id (object_iid);
	if (!info)
		info = panel_applets_manager_get_applet_info (object_iid);

	if (!info) {
		g_free (object_iid);
		return NULL;
	}

	iid = panel_applet_info_get_iid (info);

	/* migrate if the iid in the configuration is different than the real
	 * iid that will get used */
	if (!g_str_equal (iid, object_iid))
		needs_migration = TRUE;

	g_free (object_iid);

	if (needs_migration)
		g_settings_set_string (settings, PANEL_OBJECT_IID_KEY, iid);

	return g_strdup (iid);
}
