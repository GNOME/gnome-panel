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
#include "panel-widget.h"
#include "panel-util.h"
#include "panel-multiscreen.h"
#include "panel-toplevel.h"
#include "panel-lockdown.h"

typedef const char *(*PanelProfileGetIdFunc)   (gpointer           object);
typedef gboolean    (*PanelProfileOnLoadQueue) (const char        *id);
typedef void        (*PanelProfileLoadFunc)    (GConfClient       *client,
						const char        *profile_dir, 
						PanelGConfKeyType  type,
						const char        *id);
typedef void        (*PanelProfileDestroyFunc) (const char        *id);

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

static void panel_profile_object_id_list_update (GConfClient       *client,
						 GConfValue        *value,
						 PanelGConfKeyType  type);

const char *
panel_profile_map_orientation (PanelOrientation orientation)
{
	return gconf_enum_to_string (panel_orientation_map, orientation);
}

gboolean
panel_profile_map_object_type_string (const char       *str,
				      PanelObjectType  *object_type)
{
	int mapped;

	g_return_val_if_fail (object_type != NULL, FALSE);

	if (!str)
		return FALSE;

	if (!gconf_string_to_enum (panel_object_type_map, str, &mapped))
		return FALSE;

	*object_type = mapped;

	return TRUE;
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

static inline void
panel_profile_save_other_id_lists (PanelGConfKeyType type)
{
	if (type != PANEL_GCONF_TOPLEVELS)
		panel_profile_save_id_list (PANEL_GCONF_TOPLEVELS, NULL, TRUE);

	if (type != PANEL_GCONF_OBJECTS)
		panel_profile_save_id_list (PANEL_GCONF_OBJECTS, NULL, TRUE);

	if (type != PANEL_GCONF_APPLETS)
		panel_profile_save_id_list (PANEL_GCONF_APPLETS, NULL, TRUE);
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
	panel_profile_save_other_id_lists (type);
}

void
panel_profile_remove_from_list (PanelGConfKeyType  type,
				const char        *id)
{
	GConfClient *client;
	GSList      *list, *l;
	const char  *key;
	const char  *id_list;

	client = panel_gconf_get_client ();

	id_list = panel_gconf_key_type_to_id_list (type);

	key = panel_gconf_general_key (id_list);
	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	/* Remove all occurrence of id and not just the first. We're more solid
	 * this way (see bug #137308 for example). */
	l = list;
	while (l) {
		GSList *next;

		next = l->next;

		if (!strcmp (id, l->data)) {
			g_free (l->data);
			list = g_slist_delete_link (list, l);
		}

		l = next;
	}

	panel_profile_save_id_list (type, list, FALSE);
	panel_profile_save_other_id_lists (type);
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

static void
panel_profile_delete_toplevel_objects (const char        *toplevel_id,
				       PanelGConfKeyType  key_type)
{
	GConfClient *client;
	const char  *key;
	GSList      *new_list = NULL,*list, *l;

	client = panel_gconf_get_client ();

	key = panel_gconf_general_key (panel_gconf_key_type_to_id_list (key_type));
	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	for (l = list; l; l = l->next) {
		char *id = l->data;
		char *parent_toplevel_id;

		key = panel_gconf_full_key (key_type, id, "toplevel_id");
		parent_toplevel_id = gconf_client_get_string (client, key, NULL);

		if (parent_toplevel_id && !strcmp (toplevel_id, parent_toplevel_id)) {
			g_free (id);
			g_free (parent_toplevel_id);
			continue;
		}

		new_list = g_slist_prepend (new_list, id);

		g_free (parent_toplevel_id);
	}
	g_slist_free (list);

	key = panel_gconf_general_key (panel_gconf_key_type_to_id_list (key_type));
	gconf_client_set_list (client, key, GCONF_VALUE_STRING, new_list, NULL);

	for (l = new_list; l; l = l->next)
		g_free (l->data);
	g_slist_free (new_list);
}

void
panel_profile_delete_toplevel (PanelToplevel *toplevel)
{
	const char *toplevel_id;

	toplevel_id = panel_toplevel_get_toplevel_id (toplevel);

	panel_profile_delete_toplevel_objects (toplevel_id, PANEL_GCONF_OBJECTS);
	panel_profile_delete_toplevel_objects (toplevel_id, PANEL_GCONF_APPLETS);

	panel_profile_remove_from_list (PANEL_GCONF_TOPLEVELS, toplevel_id);
}

static void
panel_profile_load_and_show_toplevel (GConfClient       *client,
				      const char        *profile_dir,
				      PanelGConfKeyType  type,
				      const char        *toplevel_id)
{
	PanelToplevel *toplevel;
	const char    *id_list;
	const char    *key;
	GConfValue    *value;
	gboolean       loading_queued_applets;

	toplevel = NULL;
	if (!toplevel)
		return;

	loading_queued_applets = FALSE;

	/* reload list of objects to get those that might be on the new
	 * toplevel */
	id_list = panel_gconf_key_type_to_id_list (PANEL_GCONF_OBJECTS);
	key = panel_gconf_sprintf ("%s/general/%s", profile_dir, id_list);
	value = gconf_client_get (client, key, NULL);
	if (value) {
		panel_profile_object_id_list_update (client, value,
						     PANEL_GCONF_OBJECTS);
		loading_queued_applets = TRUE;
		gconf_value_free (value);
	}

	id_list = panel_gconf_key_type_to_id_list (PANEL_GCONF_APPLETS);
	key = panel_gconf_sprintf ("%s/general/%s", profile_dir, id_list);
	value = gconf_client_get (client, key, NULL);
	if (value) {
		panel_profile_object_id_list_update (client, value,
						     PANEL_GCONF_APPLETS);
		loading_queued_applets = TRUE;
		gconf_value_free (value);
	}

	if (!loading_queued_applets)
		panel_applet_load_queued_applets (FALSE);
}

static void
panel_profile_load_and_show_toplevel_startup (GConfClient       *client,
					      const char        *profile_dir,
					      PanelGConfKeyType  type,
					      const char        *toplevel_id)
{
}

static void
panel_profile_destroy_toplevel (const char *id)
{
	PanelToplevel *toplevel;

	if (!(toplevel = panel_toplevel_get_by_id (id)))
		return;

	gtk_widget_destroy (GTK_WIDGET (toplevel));
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
panel_profile_delete_object (AppletInfo *applet_info)
{
	PanelGConfKeyType  type;
	const char        *id;

	type = (applet_info->type) == PANEL_OBJECT_APPLET ? PANEL_GCONF_APPLETS :
							    PANEL_GCONF_OBJECTS;
	id = panel_applet_get_id (applet_info);

	panel_profile_remove_from_list (type, id);
}

static void
panel_profile_load_object (GConfClient       *client,
			   const char        *profile_dir,
			   PanelGConfKeyType  type,
			   const char        *id)
{
	PanelObjectType  object_type;
	char            *object_dir;
	const char      *key;
	char            *type_string;
	char            *toplevel_id;
	int              position;
	gboolean         right_stick;

	object_dir = g_strdup_printf ("%s/%s/%s",
				      profile_dir,
				      type == PANEL_GCONF_OBJECTS ? "objects" : "applets",
				      id);

	gconf_client_add_dir (client, object_dir, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	key = panel_gconf_sprintf ("%s/object_type", object_dir);
	type_string = gconf_client_get_string (client, key, NULL);
        
	if (!panel_profile_map_object_type_string (type_string, &object_type)) {
		g_free (type_string);
		gconf_client_remove_dir (client, object_dir, NULL);
		g_free (object_dir);
		return;
	}
	
	key = panel_gconf_sprintf ("%s/position", object_dir);
	position = gconf_client_get_int (client, key, NULL);
	
	key = panel_gconf_sprintf ("%s/toplevel_id", object_dir);
	toplevel_id = gconf_client_get_string (client, key, NULL);

	key = panel_gconf_sprintf ("%s/panel_right_stick", object_dir);
	right_stick = gconf_client_get_bool (client, key, NULL);

	panel_applet_queue_applet_to_load (id,
					   object_type,
					   toplevel_id,
					   position,
					   right_stick);

	g_free (toplevel_id);
	g_free (type_string);
	g_free (object_dir);
}

static void
panel_profile_destroy_object (const char *id)
{
	AppletInfo *info;

	info = panel_applet_get_by_id (id);

	panel_applet_clean (info);
}

static void
panel_profile_delete_dir (GConfClient       *client,
			  PanelGConfKeyType  type,
			  const char        *id)
{
	const char *key;
	char       *type_str;

	switch (type) {
	case PANEL_GCONF_TOPLEVELS:
		type_str = "toplevels";
		break;
	case PANEL_GCONF_OBJECTS:
		type_str = "objects";
		break;
	case PANEL_GCONF_APPLETS:
		type_str = "applets";
		break;
	default:
		type_str = NULL;
		g_assert_not_reached ();
		break;
	}

	if (type == PANEL_GCONF_TOPLEVELS) {
		key = panel_gconf_sprintf (PANEL_CONFIG_DIR "/%s/%s/background",
					   type_str, id);
		gconf_client_remove_dir (client, key, NULL);
	}

	key = panel_gconf_sprintf (PANEL_CONFIG_DIR "/%s/%s",
				   type_str, id);
	gconf_client_remove_dir (client, key, NULL);

	gconf_client_recursive_unset (client,
				      key,
				      GCONF_UNSET_INCLUDING_SCHEMA_NAMES,
				      NULL);
}

static gboolean
panel_profile_object_exists (GSList                *list,
			     const char            *id,
			     PanelProfileGetIdFunc  get_id_func)
{
	GSList *l;

	if (!list || !id)
		return FALSE;

	for (l = list; l; l = l->next) {
		const char *check_id;

		check_id = get_id_func (l->data);
		g_assert (check_id != NULL);

		if (!strcmp (check_id, id))
			return TRUE;
	}

	return FALSE;
}

static gboolean
panel_profile_id_exists (GSList     *id_list,
			 const char *id)
{
	GSList *l;

	if (!id_list || !id)
		return FALSE;

	for (l = id_list; l; l = l->next) {
		const char *check_id = gconf_value_get_string (l->data);

		if (!strcmp (id, check_id))
			return TRUE;
	}

	return FALSE;
}

static void
panel_profile_load_added_ids (GConfClient            *client,
			      PanelGConfKeyType       type,
			      GSList                 *list,
			      GSList                 *id_list,
			      PanelProfileGetIdFunc   get_id_func,
			      PanelProfileLoadFunc    load_handler,
			      PanelProfileOnLoadQueue on_load_queue)
{
	GSList *added_ids = NULL;
	GSList *l;

	for (l = id_list; l; l = l->next) {
		const char *id = gconf_value_get_string (l->data);

		if (!panel_profile_object_exists (list, id, get_id_func) &&
		    (on_load_queue == NULL || !on_load_queue (id)))
			added_ids = g_slist_prepend (added_ids, g_strdup (id));
	}

	for (l = added_ids; l; l = l->next) {
		char *id;
		id = (char *) l->data;

		if (id && id[0])
			load_handler (client, PANEL_CONFIG_DIR, type, id);

		g_free (l->data);
		l->data = NULL;
	}

	g_slist_free (added_ids);
}

static void
panel_profile_delete_removed_ids (GConfClient             *client,
				  PanelGConfKeyType        type,
				  GSList                  *list,
				  GSList                  *id_list,
				  PanelProfileGetIdFunc    get_id_func,
				  PanelProfileDestroyFunc  destroy_handler)
{
	GSList *removed_ids = NULL;
	GSList *l;

	for (l = list; l; l = l->next) {
		const char *id;

		id = get_id_func (l->data);

		if (!panel_profile_id_exists (id_list, id))
			removed_ids = g_slist_prepend (removed_ids, g_strdup (id));
	}

	for (l = removed_ids; l; l = l->next) {
		const char *id = l->data;

		panel_profile_delete_dir (client, type, id);
		destroy_handler (id);

		g_free (l->data);
		l->data = NULL;
	}
	g_slist_free (removed_ids);
}

static void
panel_profile_toplevel_id_list_notify (GConfClient *client,
				       guint        cnxn_id,
				       GConfEntry  *entry)
{
	GConfValue *value;
	GSList     *l, *existing_toplevels;
	GSList     *toplevel_ids;

	if (!(value = gconf_entry_get_value (entry)))
		return;

	if (value->type != GCONF_VALUE_LIST ||
	    gconf_value_get_list_type (value) != GCONF_VALUE_STRING) {
		gconf_value_free (value);
		return;
	}

	toplevel_ids = g_slist_copy (gconf_value_get_list (value));
	toplevel_ids = panel_g_slist_make_unique (toplevel_ids,
						  panel_gconf_value_strcmp,
						  FALSE);

	existing_toplevels = NULL;
	for (l = panel_toplevel_list_toplevels (); l; l = l->next) {
		PanelToplevel *toplevel = l->data;
		existing_toplevels = g_slist_prepend (existing_toplevels, toplevel);
	}

	panel_profile_load_added_ids (client,
				      PANEL_GCONF_TOPLEVELS,
				      existing_toplevels,
				      toplevel_ids,
				      (PanelProfileGetIdFunc) panel_toplevel_get_toplevel_id,
				      (PanelProfileLoadFunc) panel_profile_load_and_show_toplevel,
				      (PanelProfileOnLoadQueue) NULL);

	panel_profile_delete_removed_ids (client,
					  PANEL_GCONF_TOPLEVELS,
					  existing_toplevels,
					  toplevel_ids,
					  (PanelProfileGetIdFunc) panel_toplevel_get_toplevel_id,
					  (PanelProfileDestroyFunc) panel_profile_destroy_toplevel);

	g_slist_free (existing_toplevels);
	g_slist_free (toplevel_ids);
}

static void
panel_profile_object_id_list_update (GConfClient       *client,
				     GConfValue        *value,
				     PanelGConfKeyType  type)
{
	GSList *existing_applets;
	GSList *sublist = NULL, *l;
	GSList *object_ids;

	if (value->type != GCONF_VALUE_LIST ||
	    gconf_value_get_list_type (value) != GCONF_VALUE_STRING) {
		gconf_value_free (value);
		return;
	}

	object_ids = g_slist_copy (gconf_value_get_list (value));
	object_ids = panel_g_slist_make_unique (object_ids,
						panel_gconf_value_strcmp,
						FALSE);

	existing_applets = panel_applet_list_applets ();

	for (l = existing_applets; l; l = l->next) {
		AppletInfo *info = l->data;

		if ((type == PANEL_GCONF_APPLETS && info->type == PANEL_OBJECT_APPLET) ||
		    (type == PANEL_GCONF_OBJECTS && info->type != PANEL_OBJECT_APPLET))
			sublist = g_slist_prepend (sublist, info);
	}

	panel_profile_load_added_ids (client,
				      type,
				      sublist,
				      object_ids,
				      (PanelProfileGetIdFunc) panel_applet_get_id,
				      (PanelProfileLoadFunc) panel_profile_load_object,
				      (PanelProfileOnLoadQueue) panel_applet_on_load_queue);

	panel_profile_delete_removed_ids (client,
					  type,
					  sublist,
					  object_ids,
					  (PanelProfileGetIdFunc) panel_applet_get_id,
					  (PanelProfileDestroyFunc) panel_profile_destroy_object);

	g_slist_free (sublist);
	g_slist_free (object_ids);

	panel_applet_load_queued_applets (FALSE);
}

static void
panel_profile_object_id_list_notify (GConfClient *client,
				     guint        cnxn_id,
				     GConfEntry  *entry,
				     gpointer     data)
{
	GConfValue        *value;
	PanelGConfKeyType  type = GPOINTER_TO_INT (data);

	if (!(value = gconf_entry_get_value (entry)))
		return;

	panel_profile_object_id_list_update (client, value, type);
}

static void
panel_profile_load_list (GConfClient           *client,
			 const char            *profile_dir,
			 PanelGConfKeyType      type,
			 PanelProfileLoadFunc   load_handler,
			 GConfClientNotifyFunc  notify_handler)
{

	const char *key;
	GSList     *list;
	GSList     *l;
	const char *id_list;

	id_list = panel_gconf_key_type_to_id_list (type);

	key = panel_gconf_sprintf ("%s/general/%s", profile_dir, id_list);

	gconf_client_notify_add (client, key, notify_handler,
				 GINT_TO_POINTER (type),
				 NULL, NULL);

	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);
	list = panel_g_slist_make_unique (list,
					  (GCompareFunc) strcmp,
					  TRUE);

	for (l = list; l; l = l->next) {
		char *id;
		id = (char *) l->data;

		if (id && id[0])
			load_handler (client, profile_dir, type, id);

		g_free (l->data);
		l->data = NULL;
	}
	g_slist_free (list);
}

void
panel_profile_load (void)
{
	GConfClient *client;

	client  = panel_gconf_get_client ();

	gconf_client_add_dir (client, PANEL_CONFIG_DIR "/general", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	panel_profile_load_list (client,
				 PANEL_CONFIG_DIR,
				 PANEL_GCONF_TOPLEVELS,
				 panel_profile_load_and_show_toplevel_startup,
				 (GConfClientNotifyFunc) panel_profile_toplevel_id_list_notify);
	panel_profile_load_list (client,
				 PANEL_CONFIG_DIR,
				 PANEL_GCONF_OBJECTS,
				 panel_profile_load_object,
				 (GConfClientNotifyFunc) panel_profile_object_id_list_notify);
	panel_profile_load_list (client,
				 PANEL_CONFIG_DIR,
				 PANEL_GCONF_APPLETS,
				 panel_profile_load_object,
				 (GConfClientNotifyFunc) panel_profile_object_id_list_notify);

	panel_applet_load_queued_applets (TRUE);
}
