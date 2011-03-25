/*
 * panel-profile.c:
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
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-profile.h"

#include <string.h>
#include <glib/gi18n.h>

#include <libpanel-util/panel-list.h>

#include "applet.h"
#include "panel-gconf.h"
#include "panel.h"
#include "panel-object-loader.h"
#include "panel-widget.h"
#include "panel-util.h"
#include "panel-multiscreen.h"
#include "panel-toplevel.h"
#include "panel-lockdown.h"

static GConfEnumStringPair panel_orientation_map [] = {
	{ PANEL_ORIENTATION_TOP,    "top"    },
	{ PANEL_ORIENTATION_BOTTOM, "bottom" },
	{ PANEL_ORIENTATION_LEFT,   "left"   },
	{ PANEL_ORIENTATION_RIGHT,  "right"  },
	{ 0,                        NULL     }
};

static GConfEnumStringPair panel_object_type_map [] = {
	{ PANEL_OBJECT_MENU,      "menu-object" },
	{ PANEL_OBJECT_LAUNCHER,  "launcher-object" },
	{ PANEL_OBJECT_APPLET,    "external-applet" },
	{ PANEL_OBJECT_ACTION,    "action-applet" },
	{ PANEL_OBJECT_MENU_BAR,  "menu-bar" },
	{ PANEL_OBJECT_SEPARATOR, "separator" },
	/* The following is for backwards compatibility with 2.30.x and earlier */
	{ PANEL_OBJECT_APPLET,    "bonobo-applet" },
	{ 0,                      NULL }
};

const char *
panel_profile_map_orientation (PanelOrientation orientation)
{
	return gconf_enum_to_string (panel_orientation_map, orientation);
}

char *
panel_profile_find_new_id (PanelGConfKeyType type)
{
	GConfClient *client;
	GSList      *l, *existing_ids;
	const char  *key;
	char        *retval = NULL;
	char        *prefix;
	char        *dir;
	int          i;

	client  = panel_gconf_get_client ();

	switch (type) {
	case PANEL_GCONF_TOPLEVELS:
		prefix = "panel";
		dir = "toplevels";
		break;
	case PANEL_GCONF_OBJECTS:
		prefix = "object";
		dir = "objects";
		break;
	case PANEL_GCONF_APPLETS:
		prefix = "applet";
		dir = "applets";
		break;
	default:
		prefix = dir = NULL;
		g_assert_not_reached ();
		break;
	}

	key = panel_gconf_sprintf (PANEL_CONFIG_DIR "/%s", dir);
	existing_ids = gconf_client_all_dirs (client, key, NULL);

	for (i = 0; !retval; i++) {
		retval = g_strdup_printf ("%s_%d", prefix, i);

		for (l = existing_ids; l; l = l->next)
			if (!strcmp (panel_gconf_basename (l->data), retval)) {
				g_free (retval);
				retval = NULL;
				break;
			}
	}

	g_assert (retval != NULL);

	for (l = existing_ids; l; l = l->next)
		g_free (l->data);
	g_slist_free (existing_ids);

	return retval;
}

static void
panel_profile_save_id_list (PanelGConfKeyType  type,
			    GSList            *list,
			    gboolean           resave)
{
	GConfClient *client;
	const char  *key;
	const char  *id_list;

	g_assert (!(resave && list != NULL));

	client = panel_gconf_get_client ();

	id_list = panel_gconf_key_type_to_id_list (type);

	key = panel_gconf_general_key (id_list);
	if (resave)
		list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);
	else {
		/* Make sure the elements in the list appear only once. We only
		 * do it when we save a list with new elements. */
		list = panel_g_slist_make_unique (list,
						  (GCompareFunc) strcmp,
						  TRUE);
	}

	gconf_client_set_list (client, key, GCONF_VALUE_STRING, list, NULL);

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);
}

void
panel_profile_add_to_list (PanelGConfKeyType  type,
			   const char        *id)
{
	GConfClient *client;
	GSList      *list;
	const char  *key;
	const char  *id_list;
	char        *new_id;

	client = panel_gconf_get_client ();

	id_list = panel_gconf_key_type_to_id_list (type);

	key = panel_gconf_general_key (id_list);
	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	new_id = id ? g_strdup (id) : panel_profile_find_new_id (type);

	list = g_slist_append (list, new_id);

	panel_profile_save_id_list (type, list, FALSE);
}

static gboolean
panel_profile_id_list_is_writable (PanelGConfKeyType type)
{
	GConfClient *client;
	const char  *key;
	const char  *id_list;

	client = panel_gconf_get_client ();

	id_list = panel_gconf_key_type_to_id_list (type);

	key = panel_gconf_general_key (id_list);
	return gconf_client_key_is_writable (client, key, NULL);
}

gboolean
panel_profile_id_lists_are_writable (void)
{
  return
    panel_profile_id_list_is_writable (PANEL_GCONF_TOPLEVELS) &&
    panel_profile_id_list_is_writable (PANEL_GCONF_APPLETS)   &&
    panel_profile_id_list_is_writable (PANEL_GCONF_OBJECTS);
}

static gboolean
panel_profile_find_empty_spot (GdkScreen *screen,
			       PanelOrientation *orientation,
			       int *monitor)
{
	GSList *li;
	int i;
	int *filled_spots;
	gboolean found_a_spot = FALSE;

	*monitor = 0;
	*orientation = PANEL_ORIENTATION_TOP;

	filled_spots = g_new0 (int, panel_multiscreen_monitors (screen));

	for (li = panel_toplevel_list_toplevels (); li != NULL; li = li->next) {
		PanelToplevel *toplevel = li->data;
		GdkScreen *toplevel_screen = gtk_window_get_screen (GTK_WINDOW (toplevel));
		int toplevel_monitor = panel_toplevel_get_monitor (toplevel);

		if (toplevel_screen != screen ||
		    toplevel_monitor < 0)
			continue;

		filled_spots[toplevel_monitor] |= panel_toplevel_get_orientation (toplevel);
	}

	for (i = 0; i < panel_multiscreen_monitors (screen); i++) {
		/* These are ordered based on "priority" of the
		   orientation when picking it */
		if ( ! (filled_spots[i] & PANEL_ORIENTATION_TOP)) {
			*orientation = PANEL_ORIENTATION_TOP;
			*monitor = i;
			found_a_spot = TRUE;
			break;
		} else if ( ! (filled_spots[i] & PANEL_ORIENTATION_BOTTOM)) {
			*orientation = PANEL_ORIENTATION_BOTTOM;
			*monitor = i;
			found_a_spot = TRUE;
			break;
		} else if ( ! (filled_spots[i] & PANEL_ORIENTATION_RIGHT)) {
			*orientation = PANEL_ORIENTATION_RIGHT;
			*monitor = i;
			found_a_spot = TRUE;
			break;
		} else if ( ! (filled_spots[i] & PANEL_ORIENTATION_LEFT)) {
			*orientation = PANEL_ORIENTATION_LEFT;
			*monitor = i;
			found_a_spot = TRUE;
			break;
		}
	}

	g_free (filled_spots);

	return found_a_spot;
}

void
panel_profile_create_toplevel (GdkScreen *screen)
{
	GConfClient     *client;
	const char      *key;
	char            *id;
	char            *dir;
	PanelOrientation orientation;
	int              monitor;
       
	g_return_if_fail (screen != NULL);

	client = panel_gconf_get_client ();

	id = panel_profile_find_new_id (PANEL_GCONF_TOPLEVELS);

	dir = g_strdup_printf (PANEL_CONFIG_DIR "/toplevels/%s", id);
	panel_gconf_associate_schemas_in_dir (client, dir, PANEL_SCHEMAS_DIR "/toplevels");
	g_free (dir);

	key = panel_gconf_full_key (PANEL_GCONF_TOPLEVELS, id, "screen");
	gconf_client_set_int (client, key, gdk_screen_get_number (screen), NULL);

	if (panel_profile_find_empty_spot (screen, &orientation, &monitor)) {
		key = panel_gconf_full_key (PANEL_GCONF_TOPLEVELS, id, "monitor");
		gconf_client_set_int (client, key, monitor, NULL);

		key = panel_gconf_full_key (PANEL_GCONF_TOPLEVELS, id, "orientation");
		gconf_client_set_string (client, key, panel_profile_map_orientation (orientation), NULL);
	}
	
	panel_profile_add_to_list (PANEL_GCONF_TOPLEVELS, id);

	g_free (id);
}


char *
panel_profile_prepare_object_with_id (PanelObjectType  object_type,
				      const char      *toplevel_id,
				      int              position,
				      gboolean         right_stick)
{
	PanelGConfKeyType  key_type;
	GConfClient       *client;
	const char        *key;
	char              *id;
	char              *dir;

	key_type = (object_type == PANEL_OBJECT_APPLET) ? PANEL_GCONF_APPLETS : PANEL_GCONF_OBJECTS;

	client  = panel_gconf_get_client ();

	id = panel_profile_find_new_id (key_type);

	dir = g_strdup_printf (PANEL_CONFIG_DIR "/%s/%s",
			       (key_type == PANEL_GCONF_APPLETS) ? "applets" : "objects",
			       id);
	panel_gconf_associate_schemas_in_dir (client, dir, PANEL_SCHEMAS_DIR "/objects");

	key = panel_gconf_full_key (key_type, id, "object_type");
	gconf_client_set_string (client,
				 key,
				 gconf_enum_to_string (panel_object_type_map, object_type),
				 NULL);

	key = panel_gconf_full_key (key_type, id, "toplevel_id");
	gconf_client_set_string (client, key, toplevel_id, NULL);

	key = panel_gconf_full_key (key_type, id, "position");
	gconf_client_set_int (client, key, position, NULL);

	key = panel_gconf_full_key (key_type, id, "panel_right_stick");
	gconf_client_set_bool (client, key, right_stick, NULL);

	g_free (dir);

	return id;
}

char *
panel_profile_prepare_object (PanelObjectType  object_type,
			      PanelToplevel   *toplevel,
			      int              position,
			      gboolean         right_stick)
{
	return panel_profile_prepare_object_with_id (object_type,
						     panel_toplevel_get_toplevel_id (toplevel),
						     position,
						     right_stick);
}

void
panel_profile_load (void)
{
}
