/*
 * panel-object-loader.c: object loader
 * vim: set et:
 *
 * Copyright (C) 2011 Novell, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Most of this code is originally from applet.c.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#include <config.h>

#include <string.h>

#include <gio/gio.h>

#include <libpanel-util/panel-glib.h>

#include "panel-enums-gsettings.h"
#include "panel-schemas.h"
#include "panel-toplevel.h"

/* Includes for objects we can load */
#include "panel-applet-frame.h"

#include "panel-object-loader.h"

typedef struct {
        char                *id;
        char                *settings_path;
        GSettings           *settings;
        char                *toplevel_id;
        PanelObjectPackType  pack_type;
        int                  pack_index;
} PanelObjectToLoad;

/* Each time those lists get both empty,
 * panel_object_loader_queue_initial_unhide_toplevels() should be called */
static GSList  *panel_objects_to_load = NULL;
static GSList  *panel_objects_loading = NULL;

/* We have a timeout to always unhide toplevels after a delay, in case of some
 * blocking object */
#define         UNHIDE_TOPLEVELS_TIMEOUT_SECONDS 5
static guint    panel_object_loader_unhide_toplevels_timeout = 0;

static gboolean panel_object_loader_have_idle = FALSE;

static void
free_object_to_load (PanelObjectToLoad *object)
{
        g_free (object->id);
        object->id = NULL;

        g_free (object->settings_path);
        object->settings_path = NULL;

        g_object_unref (object->settings);
        object->settings = NULL;

        g_free (object->toplevel_id);
        object->toplevel_id = NULL;

        g_free (object);
}

/* This doesn't do anything if the initial unhide already happened */
static gboolean
panel_object_loader_queue_initial_unhide_toplevels (gpointer user_data)
{
        GSList *l;

        if (panel_object_loader_unhide_toplevels_timeout != 0) {
                g_source_remove (panel_object_loader_unhide_toplevels_timeout);
                panel_object_loader_unhide_toplevels_timeout = 0;
        }

        for (l = panel_toplevel_list_toplevels (); l != NULL; l = l->next)
                panel_toplevel_queue_initial_unhide ((PanelToplevel *) l->data);

        return FALSE;
}

void
panel_object_loader_stop_loading (const char *id)
{
        PanelObjectToLoad *object;
        GSList *l;
        char *tmp_id;

        /* because the 'id' can come from object->id, and
           free_object_to_load() frees it, which makes 'id' invalid */
        tmp_id = g_strdup (id);

        for (l = panel_objects_loading; l; l = l->next) {
                object = l->data;
                if (g_strcmp0 (object->id, tmp_id) == 0)
                        break;
        }
        if (l != NULL) {
                panel_objects_loading = g_slist_delete_link (panel_objects_loading, l);
                free_object_to_load (object);
        }

        for (l = panel_objects_to_load; l; l = l->next) {
                object = l->data;
                if (g_strcmp0 (object->id, tmp_id) == 0)
                        break;
        }
        if (l != NULL) {
                panel_objects_to_load = g_slist_delete_link (panel_objects_to_load, l);
                free_object_to_load (object);
        }

        g_free (tmp_id);

        if (panel_objects_loading == NULL && panel_objects_to_load == NULL)
                panel_object_loader_queue_initial_unhide_toplevels (NULL);
}

static gboolean
is_valid_iid (const char *iid)
{
        const char *instance_id;

        instance_id = g_strrstr (iid, "::");
        if (!instance_id)
                return FALSE;

        return TRUE;
}

static gboolean
panel_object_loader_idle_handler (gpointer dummy)
{
        PanelObjectToLoad *object = NULL;
        PanelToplevel     *toplevel = NULL;
        PanelWidget       *panel_widget;
        GSList            *l;
        char              *iid = NULL;

        if (!panel_objects_to_load) {
                panel_object_loader_have_idle = FALSE;
                return FALSE;
        }

        for (l = panel_objects_to_load; l; l = l->next) {
                object = l->data;

                toplevel = panel_toplevel_get_by_id (object->toplevel_id);
                if (toplevel)
                        break;
        }

        if (!l) {
                /* All the remaining objects don't have a panel */
                for (l = panel_objects_to_load; l; l = l->next)
                        free_object_to_load (l->data);
                g_slist_free (panel_objects_to_load);
                panel_objects_to_load = NULL;
                panel_object_loader_have_idle = FALSE;

                if (panel_objects_loading == NULL) {
                        /* unhide any potential initially hidden toplevel */
                        panel_object_loader_queue_initial_unhide_toplevels (NULL);
                }

                return FALSE;
        }

        panel_objects_to_load = g_slist_delete_link (panel_objects_to_load, l);
        panel_objects_loading = g_slist_append (panel_objects_loading, object);

        panel_widget = panel_toplevel_get_panel_widget (toplevel);

        iid = g_settings_get_string (object->settings, PANEL_OBJECT_IID_KEY);

        if (!is_valid_iid (iid)) {
                g_printerr ("Object '%s' has an invalid iid ('%s')\n",
                            object->id, iid);
                panel_object_loader_stop_loading (object->id);
                g_free (iid);
                return TRUE;
        }

        g_free (iid);

        panel_applet_frame_load (panel_widget, object->id, object->settings);

        return TRUE;
}

void
panel_object_loader_queue (const char *id,
                           const char *settings_path)
{
        PanelObjectToLoad *object;
        GSettings         *settings;
        char              *toplevel_id;

        if (panel_object_loader_is_queued (id))
                return;

        settings = g_settings_new_with_path (PANEL_OBJECT_SCHEMA,
                                             settings_path);
        toplevel_id = g_settings_get_string (settings,
                                             PANEL_OBJECT_TOPLEVEL_ID_KEY);

        if (PANEL_GLIB_STR_EMPTY (toplevel_id)) {
                g_warning ("No toplevel on which to load object '%s'\n", id);
                g_free (toplevel_id);
                g_object_unref (settings);
                return;
        }

        object = g_new0 (PanelObjectToLoad, 1);

        object->id            = g_strdup (id);
        object->settings_path = g_strdup (settings_path);
        object->settings      = g_object_ref (settings);
        object->toplevel_id   = toplevel_id;
        object->pack_type     = g_settings_get_enum (settings,
                                                     PANEL_OBJECT_PACK_TYPE_KEY);
        object->pack_index    = g_settings_get_int (settings,
                                                    PANEL_OBJECT_PACK_INDEX_KEY);

        panel_objects_to_load = g_slist_prepend (panel_objects_to_load, object);

        g_object_unref (settings);
}

static int
panel_object_compare (const PanelObjectToLoad *a,
                      const PanelObjectToLoad *b)
{
        int c;

        if ((c = g_strcmp0 (a->toplevel_id, b->toplevel_id)))
                return c;
        else if (a->pack_type != b->pack_type)
                return a->pack_type - b->pack_type; /* start < center < end */
        else
                /* note: for packed-end, we explicitly want to start loading
                 * from the right/bottom instead of left/top to avoid moving
                 * applets that are on the inside; so the maths are good even
                 * in this case */
                return a->pack_index - b->pack_index;
}

void
panel_object_loader_do_load (gboolean initial_load)
{
        if (!panel_objects_to_load) {
                panel_object_loader_queue_initial_unhide_toplevels (NULL);
                return;
        }

        if (panel_objects_to_load && panel_object_loader_unhide_toplevels_timeout == 0) {
                /* Install a timeout to make sure we don't block the
                 * unhiding because of an object that doesn't load */
                panel_object_loader_unhide_toplevels_timeout =
                        g_timeout_add_seconds (UNHIDE_TOPLEVELS_TIMEOUT_SECONDS,
                                               panel_object_loader_queue_initial_unhide_toplevels,
                                               NULL);
        }

        panel_objects_to_load = g_slist_sort (panel_objects_to_load,
                                              (GCompareFunc) panel_object_compare);

        if (!panel_object_loader_have_idle) {
                /* on panel startup, we don't care about redraws of the
                 * toplevels since they are hidden, so we give a higher
                 * priority to loading of objects */
                if (initial_load)
                        g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                                         panel_object_loader_idle_handler,
                                         NULL, NULL);
                else
                        g_idle_add (panel_object_loader_idle_handler, NULL);

                panel_object_loader_have_idle = TRUE;
        }
}

gboolean
panel_object_loader_is_queued (const char *id)
{
        GSList *li;
        for (li = panel_objects_to_load; li != NULL; li = li->next) {
                PanelObjectToLoad *object = li->data;
                if (g_strcmp0 (object->id, id) == 0)
                        return TRUE;
        }
        for (li = panel_objects_loading; li != NULL; li = li->next) {
                PanelObjectToLoad *object = li->data;
                if (g_strcmp0 (object->id, id) == 0)
                        return TRUE;
        }
        return FALSE;
}
