/*
 * Copyright (C) 2007 Vincent Untz
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
 *	Vincent Untz <vuntz@gnome.org>
 */

#include <config.h>

#include <string.h>

#include <gtk/gtkwidget.h>
#include <gconf/gconf-client.h>

#include "tz-list.h"

#include "clock.h"
#include "tz.h"
#include "tz-sel-dialog.h"

G_DEFINE_TYPE (TzList, tz_list, G_TYPE_OBJECT);

#define TZ_LIST_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TZ_TYPE_LIST, TzListPrivate))

#define MAX_TIMEZONES 255

#define KEYDIR_TIMEZONES       "timezones"
#define KEY_TZ_LIST            "tz_id_list"
#define KEYSUFFIX_TZ_ITEM_NAME "_name"
#define KEYSUFFIX_TZ_ITEM_ZONE "_zone"

struct _TzListItem {
	TzLocation *tz_loc;
	char       *name;
	char       *zone;
	char       *id;
	int         sort_order;
};

struct _TzListPrivate {
	char  *prefs_dir;
	GList *items;

	GtkWidget   *tz_sel_dialog;

	guint        listener;
	GConfClient *gconfclient;
};

enum {
	ADDED,
	CHANGED,
	REMOVED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_PREFSDIR
};

static guint tz_list_signals[LAST_SIGNAL] = { 0 };

static void        tz_list_remove_item (TzList     *list,
					TzListItem *item);
static const char *tz_list_get_prefs_dir (TzList *list);
static void        tz_list_set_prefs_dir (TzList     *list,
					  const char *prefs_dir);

static void
tz_list_item_free (TzListItem *item,
		   TzList     *list)
{
	if (list)
		g_signal_emit (list, tz_list_signals[REMOVED], 0, item);

	if (item->name)
		g_free (item->name);
	item->name = NULL;

	if (item->zone)
		g_free (item->zone);
	item->zone = NULL;

	if (item->id)
		g_free (item->id);
	item->id = NULL;

	g_slice_free (TzListItem, item);
}

static TzListItem *
tz_list_item_new (TzLocation *tz_loc,
		  const char *name,
		  const char *zone,
		  const char *id)
{
	TzListItem *item;

	item = g_slice_new (TzListItem);

	item->tz_loc = tz_loc;

	if (name)
		item->name = g_strdup (name);
	else
		item->name = NULL;

	item->zone   = g_strdup (zone);
	item->id     = g_strdup (id);
	item->sort_order = -1;

	return item;
}

TzLocation *
tz_list_item_get_tz_location (TzListItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return item->tz_loc;
}

const char *
tz_list_item_get_zone (TzListItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return tz_location_get_zone (item->tz_loc);
}

const char *
tz_list_item_get_name (TzListItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	if (item->name)
		return item->name;
	else
		return item->zone;
}

int
tz_list_item_get_sort_order (TzListItem *item)
{
	g_return_val_if_fail (item != NULL, -1);

	return item->sort_order;
}

static void
tz_list_add_item (TzList     *list,
		  const char *id,
		  const char *zone,
		  const char *name)
{
	TzListItem *item;
	TzLocation *tz_loc;

	g_return_if_fail (TZ_IS_LIST (list));
	g_return_if_fail (id   != NULL);
	g_return_if_fail (zone != NULL);

	tz_loc = tz_get_location_by_name (zone);
	if (!tz_loc)
		return;

	item = tz_list_item_new (tz_loc, name, zone, id);
	if (!item)
		return;

	item->sort_order = g_list_length (list->priv->items);
	list->priv->items = g_list_append (list->priv->items, item);

	g_signal_emit (list, tz_list_signals[ADDED], 0, item);
}

static void
tz_list_modify_item_zone (TzList     *list,
			  TzListItem *item,
			  const char *zone)
{
	TzLocation *tz_loc;

	g_return_if_fail (TZ_IS_LIST (list));
	g_return_if_fail (item != NULL);

	if (!zone) {
		tz_list_remove_item (list, item);
		return;
	}

	if (strcmp (item->zone, zone) == 0)
		return;

	tz_loc = tz_get_location_by_name (zone);

	if (!tz_loc) {
		tz_list_remove_item (list, item);
		return;
	}

	g_free (item->zone);
	item->tz_loc = tz_loc;
	item->zone = g_strdup (zone);
	g_signal_emit (list, tz_list_signals[CHANGED], 0, item);
}

static void
tz_list_modify_item_name (TzList     *list,
			  TzListItem *item,
			  const char *name)
{
	g_return_if_fail (TZ_IS_LIST (list));
	g_return_if_fail (item != NULL);

	if (!name && !item->name)
		return;

	if (name && item->name && strcmp (item->name, name) == 0)
		return;

	g_free (item->name);
	if (name)
		item->name = g_strdup (name);
	else
		item->name = NULL;

	g_signal_emit (list, tz_list_signals[CHANGED], 0, item);
}

static void
tz_list_modify_item_sort_order (TzList     *list,
				TzListItem *item,
				int         sort_order)
{
	g_return_if_fail (TZ_IS_LIST (list));
	g_return_if_fail (item != NULL);

	if (item->sort_order == sort_order)
		return;

	item->sort_order = sort_order;

	g_signal_emit (list, tz_list_signals[CHANGED], 0, item);
}

static void
tz_list_remove_item (TzList     *list,
		     TzListItem *item)
{
	g_return_if_fail (TZ_IS_LIST (list));
	g_return_if_fail (item != NULL);

	list->priv->items = g_list_remove (list->priv->items, item);
	tz_list_item_free (item, list);
}

static void
tz_list_remove_all_items (TzList *list)
{
	if (list->priv->items) {
		g_list_foreach (list->priv->items,
				(GFunc) tz_list_item_free, list);
		g_list_free (list->priv->items);
	}
	list->priv->items = NULL;
}

static TzListItem *
tz_list_find_item_by_id_in_list (GList      *item_list,
				 const char *id)
{
	GList      *l;
	TzListItem *item;

	g_return_val_if_fail (id != NULL, NULL);

	for (l = item_list; l != NULL; l = l->next) {
		item = l->data;
		if (strcmp (item->id, id) == 0)
			return item;
	}

	return NULL;
}

static TzListItem *
tz_list_find_item_by_id (TzList     *list,
			 const char *id)
{
	g_return_val_if_fail (TZ_IS_LIST (list), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	return tz_list_find_item_by_id_in_list (list->priv->items, id);
}

static void
tz_list_init (TzList *list)
{
	list->priv = TZ_LIST_GET_PRIVATE (list);

	list->priv->prefs_dir = NULL;
	list->priv->items     = NULL;

	list->priv->tz_sel_dialog = NULL;

	list->priv->listener    = 0;
	list->priv->gconfclient = gconf_client_get_default ();

	tz_ref_db ();
}

static void
tz_list_set_property (GObject       *object,
		      guint          prop_id,
		      const GValue  *value,
		      GParamSpec    *pspec)
{
	TzList *list;

	g_return_if_fail (TZ_IS_LIST (object));

	list = TZ_LIST (object);

	switch (prop_id) {
	case PROP_PREFSDIR:
		tz_list_set_prefs_dir (list, g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tz_list_get_property (GObject    *object,
		      guint       prop_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	TzList *list;

	g_return_if_fail (TZ_IS_LIST (object));

	list = TZ_LIST (object);

	switch (prop_id) {
	case PROP_PREFSDIR:
		g_value_set_string (value, tz_list_get_prefs_dir (list));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tz_list_finalize (GObject *object)
{
	TzList *list;

	list = TZ_LIST (object);

	g_assert (list->priv->gconfclient != NULL);

	if (list->priv->prefs_dir) {
		gconf_client_remove_dir (list->priv->gconfclient,
					 list->priv->prefs_dir, NULL);
		g_free (list->priv->prefs_dir);
	}
	list->priv->prefs_dir = NULL;

	tz_list_remove_all_items (list);

	if (list->priv->tz_sel_dialog)
		gtk_widget_destroy (list->priv->tz_sel_dialog);
	list->priv->tz_sel_dialog = NULL;

	if (list->priv->listener)
		gconf_client_notify_remove (list->priv->gconfclient,
					    list->priv->listener);
	list->priv->listener = 0;

	g_object_unref (list->priv->gconfclient);
	list->priv->gconfclient = NULL;

	tz_unref_db ();

	G_OBJECT_CLASS (tz_list_parent_class)->finalize (object);
}

static void
tz_list_class_init (TzListClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass *) klass;

	gobject_class->set_property = tz_list_set_property;
	gobject_class->get_property = tz_list_get_property;
	gobject_class->finalize = tz_list_finalize;

	g_type_class_add_private (klass, sizeof (TzListPrivate));

	tz_list_signals[ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TzListClass,
					       added),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	tz_list_signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TzListClass,
					       changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	tz_list_signals[REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TzListClass,
					       removed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	g_object_class_install_property (
		gobject_class,
		PROP_PREFSDIR,
		g_param_spec_string ("prefs-dir",
				     "Preferences Directory",
				     "Preferences directory in GConf",
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
tz_list_add_item_with_id (TzList     *list,
			  const char *id)
{
	char *zone;
	char *name;
	char *key;

	key = g_strdup_printf ("%s/%s%s", list->priv->prefs_dir, id,
			       KEYSUFFIX_TZ_ITEM_ZONE);
	zone = gconf_client_get_string (list->priv->gconfclient, key, NULL);
	g_free (key);

	if (!zone)
		return;

	key = g_strdup_printf ("%s/%s%s", list->priv->prefs_dir, id,
			       KEYSUFFIX_TZ_ITEM_NAME);
	name = gconf_client_get_string (list->priv->gconfclient, key, NULL);
	g_free (key);

	if (name && name[0] == '\0') {
		g_free (name);
		name = NULL;
	}

	tz_list_add_item (list, id, zone, name);

	g_free (zone);
	g_free (name);
}

static void
tz_list_update_from_prefs (TzList     *list,
			   GConfValue *value)
{
	GList  *old_items;
	GSList *id_list;
	GSList *l;

	if (!value) {
		tz_list_remove_all_items (list);
		return;
	}

	if (value->type != GCONF_VALUE_LIST)
		return;

	if (gconf_value_get_list_type (value) != GCONF_VALUE_STRING)
		return;

	old_items = list->priv->items;
	list->priv->items = NULL;

	id_list = gconf_value_get_list (value);

	for (l = id_list; l != NULL; l = l->next) {
		GConfValue *l_value;
		const char *id;
		TzListItem *item;

		l_value = l->data;
		id = gconf_value_get_string (l_value);

		if (!id || id[0] == '\0')
			continue;

		item = tz_list_find_item_by_id_in_list (old_items, id);
		if (item) {
			tz_list_modify_item_sort_order (list, item,
							g_list_length (list->priv->items));

			old_items = g_list_remove (old_items, item);
			list->priv->items = g_list_append (list->priv->items,
							   item);
		} else
			tz_list_add_item_with_id (list, id);
	}

	if (old_items) {
		g_list_foreach (old_items, (GFunc) tz_list_item_free, list);
		g_list_free (old_items);
	}
}

static void
tz_list_prefs_changed (GConfClient *client,
		       guint        cnxn_id,
		       GConfEntry  *entry,
		       gpointer     user_data)
{
	TzList     *list;
	TzListItem *item;
	GConfValue *value;
	const char *key;
	const char *short_key;
	char       *id;
	const char *value_str;
	gboolean    zone_changed;
	gboolean    name_changed;

	list = TZ_LIST (user_data);

	g_assert (list->priv->prefs_dir != NULL);

	key = gconf_entry_get_key (entry);

	if (!g_str_has_prefix (key, list->priv->prefs_dir))
		return;

	short_key = key + strlen (list->priv->prefs_dir) + 1;

	if (strcmp (short_key, KEY_TZ_LIST) == 0) {
		tz_list_update_from_prefs (list,
					   gconf_entry_get_value (entry));

		return;
	}

	name_changed = FALSE;
	zone_changed = FALSE;

	if (g_str_has_suffix (short_key, KEYSUFFIX_TZ_ITEM_NAME)) {
		name_changed = TRUE;
		id = g_strndup (short_key, strlen (short_key)
					   - strlen (KEYSUFFIX_TZ_ITEM_NAME));
	} else if (g_str_has_suffix (short_key, KEYSUFFIX_TZ_ITEM_ZONE)) {
		zone_changed = TRUE;
		id = g_strndup (short_key, strlen (short_key)
					    - strlen (KEYSUFFIX_TZ_ITEM_ZONE));
	} else
		return;

	item = tz_list_find_item_by_id (list, id);
	if (item == NULL)
		return;

	value = gconf_entry_get_value (entry);

	if (value == NULL) {
		if (zone_changed)
			tz_list_remove_item (list, item);
		else if (name_changed)
			tz_list_modify_item_name (list, item, NULL);

		return;
	}

	if (value->type != GCONF_VALUE_STRING)
		return;

	value_str = gconf_value_get_string (value);

	if (zone_changed && value_str == NULL) {
		tz_list_remove_item (list, item);
		return;
	}

	if (zone_changed)
		tz_list_modify_item_zone (list, item, value_str);
	else if (name_changed)
		tz_list_modify_item_name (list, item, value_str);
}

static void
tz_list_fill_from_pref (TzList *list)
{
	char       *key;
	GConfValue *value;

	g_return_if_fail (TZ_IS_LIST (list));

	g_assert (list->priv->gconfclient != NULL);

	if (list->priv->listener)
		gconf_client_notify_remove (list->priv->gconfclient,
					    list->priv->listener);
	list->priv->listener = 0;

	tz_list_remove_all_items (list);

	if (!list->priv->prefs_dir)
		return;

	gconf_client_add_dir (list->priv->gconfclient, list->priv->prefs_dir,
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	list->priv->listener = gconf_client_notify_add (list->priv->gconfclient,
							list->priv->prefs_dir,
							tz_list_prefs_changed,
							list, NULL, NULL);

	key = g_strdup_printf ("%s/%s", list->priv->prefs_dir, KEY_TZ_LIST);
	value = gconf_client_get (list->priv->gconfclient, key, NULL);
	g_free (key);

	tz_list_update_from_prefs (list, value);

	if (value)
		gconf_value_free (value);
}

static const char *
tz_list_get_prefs_dir (TzList *list)
{
	g_return_val_if_fail (TZ_IS_LIST (list), NULL);

	return list->priv->prefs_dir;
}

static void
tz_list_set_prefs_dir (TzList     *list,
		       const char *prefs_dir)
{
	char *complete_dir;

	g_return_if_fail (TZ_IS_LIST (list));

	g_assert (list->priv->gconfclient != NULL);

	if (!list->priv->prefs_dir && (!prefs_dir || !prefs_dir [0]))
		return;

	if (prefs_dir && prefs_dir [0])
		complete_dir = g_strdup_printf ("%s/%s",
						prefs_dir, KEYDIR_TIMEZONES);
	else
		complete_dir = NULL;

	if (list->priv->prefs_dir && complete_dir &&
	    !strcmp (list->priv->prefs_dir, complete_dir)) {
		g_free (complete_dir);
		return;
	}

	if (list->priv->prefs_dir) {
		gconf_client_remove_dir (list->priv->gconfclient,
					 list->priv->prefs_dir, NULL);
		g_free (list->priv->prefs_dir);
	}

	list->priv->prefs_dir = complete_dir;

	tz_list_fill_from_pref (list);

	g_object_notify (G_OBJECT (list), "prefs-dir");
}

TzList *
tz_list_new (const char *prefs_dir)
{
	g_return_val_if_fail (prefs_dir != NULL, NULL);

	return g_object_new (TZ_TYPE_LIST,
			     "prefs-dir", prefs_dir,
			     NULL);
}

GList *
tz_list_get_items (TzList *list)
{
	g_return_val_if_fail (TZ_IS_LIST (list), NULL);

	return list->priv->items;
}

gboolean
tz_list_contains (TzList     *list,
		  TzLocation *tz_loc)
{
	GList      *l;
	TzListItem *item;

	g_return_val_if_fail (TZ_IS_LIST (list), FALSE);

	for (l = list->priv->items; l != NULL; l = l->next) {
		item = l->data;

		if (item->tz_loc == tz_loc)
			return TRUE;
	}

	return FALSE;
}

void
tz_list_edit (TzList    *list,
	      GdkScreen *screen)
{
	g_return_if_fail (TZ_IS_LIST (list));

	if (!list->priv->tz_sel_dialog) {
		list->priv->tz_sel_dialog = tz_sel_dialog_new (list);
                g_object_add_weak_pointer (G_OBJECT (list->priv->tz_sel_dialog),
                                           (gpointer *) &list->priv->tz_sel_dialog);
	}

	gtk_window_set_screen (GTK_WINDOW (list->priv->tz_sel_dialog), screen);
	tz_sel_dialog_present (GTK_WINDOW (list->priv->tz_sel_dialog));
}

static char *
tz_list_find_id (TzList *list)
{
	char *id;
	int   i;

	id = NULL;

	for (i = 0; i < MAX_TIMEZONES; i++) {
		GConfValue *value;
		char       *key;

		id = g_strdup_printf ("timezone_%d", i);

		key = g_strdup_printf ("%s/%s%s", list->priv->prefs_dir, id,
				       KEYSUFFIX_TZ_ITEM_ZONE);
		value = gconf_client_get (list->priv->gconfclient, key, NULL);
		g_free (key);

		if (value) {
			gconf_value_free (value);
			g_free (id);
			id = NULL;
			continue;
		}

		key = g_strdup_printf ("%s/%s%s", list->priv->prefs_dir, id,
				       KEYSUFFIX_TZ_ITEM_NAME);
		value = gconf_client_get (list->priv->gconfclient, key, NULL);
		g_free (key);

		if (value) {
			gconf_value_free (value);
			g_free (id);
			id = NULL;
			continue;
		}

		break;
	}

	return id;
}

void
tz_list_add (TzList     *list,
	     TzLocation *tz_loc,
	     const char *name)
{
	const char     *zone;
	GConfChangeSet *changeset;
	GConfChangeSet *revert_changeset;
	GConfSchema    *schema;
	GSList         *id_list;
	GList          *l;
	char           *key;
	char           *id;

	g_return_if_fail (TZ_IS_LIST (list));
	g_return_if_fail (tz_loc != NULL);

	if (tz_list_contains (list, tz_loc))
		return;

	zone = tz_location_get_zone (tz_loc);

	id = tz_list_find_id (list);
	if (!id)
		return;

	if (!name)
		name = "";

	changeset = gconf_change_set_new ();

	id_list = NULL;
	for (l = list->priv->items; l != NULL; l = l->next) {
		TzListItem *list_item;
		list_item = l->data;
		id_list = g_slist_prepend (id_list, list_item->id);
	}
	id_list = g_slist_prepend (id_list, id);
	id_list = g_slist_reverse (id_list);

	key = g_strdup_printf ("%s/%s", list->priv->prefs_dir, KEY_TZ_LIST);
	gconf_change_set_set_list (changeset, key, GCONF_VALUE_STRING, id_list);
	g_free (key);
	g_slist_free (id_list);

	//TODO: associate schema for new keys: why doesn't it work?
	//TODO: don't have timezone_name/timezone_zone
	key = g_strdup_printf ("%s/%s%s", list->priv->prefs_dir, id,
			       KEYSUFFIX_TZ_ITEM_ZONE);
	schema = gconf_client_get_schema (list->priv->gconfclient,
					  CLOCK_SCHEMA_DIR"/"KEYDIR_TIMEZONES"/timezone"KEYSUFFIX_TZ_ITEM_ZONE,
					  NULL);
	if (schema) {
		gconf_change_set_set_schema (changeset, key, schema);
		gconf_schema_free (schema);
	}
	gconf_change_set_set_string (changeset, key, zone);
	g_free (key);

	key = g_strdup_printf ("%s/%s%s", list->priv->prefs_dir, id,
			       KEYSUFFIX_TZ_ITEM_NAME);
	schema = gconf_client_get_schema (list->priv->gconfclient,
					  CLOCK_SCHEMA_DIR"/"KEYDIR_TIMEZONES"/timezone"KEYSUFFIX_TZ_ITEM_NAME,
					  NULL);
	if (schema) {
		gconf_change_set_set_schema (changeset, key, schema);
		gconf_schema_free (schema);
	}
	gconf_change_set_set_string (changeset, key, name);
	g_free (key);

	revert_changeset = gconf_client_reverse_change_set (
					list->priv->gconfclient,
					changeset, NULL);
	if (!gconf_client_commit_change_set (list->priv->gconfclient,
					     changeset, TRUE, NULL))
		gconf_client_commit_change_set (list->priv->gconfclient,
						revert_changeset, TRUE, NULL);

	gconf_change_set_unref (changeset);
	gconf_change_set_unref (revert_changeset);

	g_free (id);
}

void
tz_list_rename (TzList     *list,
		TzListItem *item,
		const char *new_name)
{
	GList *l;
	char  *key;

	g_return_if_fail (TZ_IS_LIST (list));
	g_return_if_fail (item != NULL);

	if (!item->name && !new_name)
		return;

	if (new_name && item->name && strcmp (item->name, new_name) == 0)
		return;

	for (l = list->priv->items; l != NULL; l = l->next) {
		if (l->data == item)
			break;
	}

	if (l == NULL)
		return;

	if (!new_name)
		new_name = "";

	key = g_strdup_printf ("%s/%s%s", list->priv->prefs_dir, item->id,
			       KEYSUFFIX_TZ_ITEM_NAME);
	gconf_client_set_string (list->priv->gconfclient, key, new_name, NULL);
	g_free (key);
}

void
tz_list_move_up (TzList     *list,
		 TzListItem *item)
{
	TzListItem *list_item;
	GSList     *id_list;
	GList      *l;
	char       *key;

	g_return_if_fail (TZ_IS_LIST (list));
	g_return_if_fail (item != NULL);

	if (item->sort_order == 0)
		return;

	id_list = NULL;

	for (l = list->priv->items; l != NULL; l = l->next) {
		list_item = l->data;

		if (list_item == item)
			id_list = g_slist_insert (id_list, item->id, 1);
		else
			id_list = g_slist_prepend (id_list, list_item->id);
	}

	id_list = g_slist_reverse (id_list);
	key = g_strdup_printf ("%s/%s", list->priv->prefs_dir, KEY_TZ_LIST);

	gconf_client_set_list (list->priv->gconfclient, key,
			       GCONF_VALUE_STRING, id_list, NULL);

	g_slist_free (id_list);
	g_free (key);
}

void
tz_list_move_down (TzList     *list,
		   TzListItem *item)
{
	TzListItem *list_item;
	TzListItem *found;
	GSList     *id_list;
	GList      *l;
	char       *key;

	g_return_if_fail (TZ_IS_LIST (list));
	g_return_if_fail (item != NULL);

	if (item->sort_order == g_list_length (list->priv->items) - 1)
		return;

	id_list = NULL;
	found = NULL;

	for (l = list->priv->items; l != NULL; l = l->next) {
		list_item = l->data;

		if (list_item == item) {
			found = item;
			continue;
		}

		id_list = g_slist_prepend (id_list, list_item->id);
		if (found) {
			id_list = g_slist_prepend (id_list, found->id);
			found = NULL;
		}
	}

	id_list = g_slist_reverse (id_list);
	key = g_strdup_printf ("%s/%s", list->priv->prefs_dir, KEY_TZ_LIST);

	gconf_client_set_list (list->priv->gconfclient, key,
			       GCONF_VALUE_STRING, id_list, NULL);

	g_slist_free (id_list);
	g_free (key);
}

void
tz_list_remove (TzList     *list,
		TzListItem *item)
{
	TzListItem *list_item;
	GSList     *id_list;
	GList      *l;
	gboolean    found;
	char       *key;

	g_return_if_fail (TZ_IS_LIST (list));
	g_return_if_fail (item != NULL);

	id_list = NULL;
	found = FALSE;

	for (l = list->priv->items; l != NULL; l = l->next) {
		list_item = l->data;

		if (list_item == item) {
			found = TRUE;
			continue;
		}

		id_list = g_slist_prepend (id_list, list_item->id);
	}

	if (!found) {
		g_slist_free (id_list);
		return;
	}

	id_list = g_slist_reverse (id_list);
	key = g_strdup_printf ("%s/%s", list->priv->prefs_dir, KEY_TZ_LIST);

	if (!gconf_client_set_list (list->priv->gconfclient, key,
				    GCONF_VALUE_STRING, id_list, NULL)) {
		g_slist_free (id_list);
		g_free (key);
		return;
	}

	g_slist_free (id_list);
	g_free (key);

	key = g_strdup_printf ("%s/%s%s", list->priv->prefs_dir, item->id,
			       KEYSUFFIX_TZ_ITEM_ZONE);
	gconf_client_unset (list->priv->gconfclient, key, NULL);
	g_free (key);

	key = g_strdup_printf ("%s/%s%s", list->priv->prefs_dir, item->id,
			       KEYSUFFIX_TZ_ITEM_NAME);
	gconf_client_unset (list->priv->gconfclient, key, NULL);
	g_free (key);
}
