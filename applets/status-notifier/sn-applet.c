/*
 * Copyright (C) 2015 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "sn-applet.h"
#include "sn-button.h"

struct _SnApplet
{
  PanelApplet  parent;

  SnHost      *host;
  GSList      *items;

  GtkWidget   *box;
};

G_DEFINE_TYPE (SnApplet, sn_applet, PANEL_TYPE_APPLET)

static gint
compare_items (gconstpointer a,
               gconstpointer b)
{
  SnItem *item1;
  SnItemCategory c1;
  const gchar *id1;
  SnItem *item2;
  SnItemCategory c2;
  const gchar *id2;

  item1 = SN_ITEM (a);
  c1 = sn_item_get_category (item1);

  item2 = SN_ITEM (b);
  c2 = sn_item_get_category (item2);

  if (c1 < c2)
    return -1;

  if (c2 < c1)
    return 1;

  id1 = sn_item_get_id (item1);
  id2 = sn_item_get_id (item2);

  return g_strcmp0 (id1, id2);
}

static void
reorder_items (GtkWidget *widget,
               gpointer   data)
{
  SnApplet *applet;
  SnButton *button;
  gint position;

  applet = SN_APPLET (data);
  button = SN_BUTTON (widget);

  position = g_slist_index (applet->items, sn_button_get_item (button));

  gtk_box_reorder_child (GTK_BOX (applet->box), widget, position);
}

static void
item_added_cb (SnHost   *host,
               SnItem   *item,
               SnApplet *applet)
{
  GtkWidget *button;

  applet->items = g_slist_prepend (applet->items, item);
  applet->items = g_slist_sort (applet->items, compare_items);

  button = sn_button_new (item);
  gtk_box_pack_start (GTK_BOX (applet->box), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_object_bind_property (applet->box, "orientation", item, "orientation",
                          G_BINDING_DEFAULT);

  gtk_container_foreach (GTK_CONTAINER (applet->box), reorder_items, applet);
}

static void
item_remove (GtkWidget *widget,
             gpointer   data)
{
  SnButton *button;
  SnItem *item;

  button = SN_BUTTON (widget);
  item = SN_ITEM (data);

  if (sn_button_get_item (button) == item)
    gtk_widget_destroy (widget);
}

static void
item_removed_cb (SnHost   *host,
                 SnItem   *item,
                 SnApplet *applet)
{
  GSList *l;

  for (l = applet->items; l != NULL; l = g_slist_next (l))
    {
      SnItem *tmp;

      tmp = SN_ITEM (l->data);

      if (tmp != item)
        continue;

      applet->items = g_slist_remove (applet->items, l->data);
      gtk_container_foreach (GTK_CONTAINER (applet->box),
                             item_remove, item);
    }
}

static gboolean
sn_applet_fill (SnApplet *applet)
{
  applet->host = sn_host_new (SN_HOST_FLAGS_NONE);

  g_signal_connect (applet->host, "item-added",
                    G_CALLBACK (item_added_cb), applet);
  g_signal_connect (applet->host, "item-removed",
                    G_CALLBACK (item_removed_cb), applet);

  gtk_widget_show (GTK_WIDGET (applet));

  return TRUE;
}

static gboolean
sn_applet_factory (PanelApplet *applet,
                   const gchar *iid,
                   gpointer     user_data)
{
  if (g_strcmp0 (iid, "SnApplet") == 0)
    return sn_applet_fill (SN_APPLET (applet));

  return FALSE;
}

static void
sn_applet_dispose (GObject *object)
{
  SnApplet *applet;

  applet = SN_APPLET (object);

  g_clear_object (&applet->host);
  g_clear_pointer (&applet->items, g_slist_free);

  G_OBJECT_CLASS (sn_applet_parent_class)->dispose (object);
}

static void
sn_applet_change_orient (PanelApplet       *applet,
                         PanelAppletOrient  orient)
{
  SnApplet *sn_applet;
  GtkOrientation orientation;

  sn_applet = SN_APPLET (applet);
  orientation = panel_applet_get_gtk_orientation (applet);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (sn_applet->box),
                                  orientation);
}

static void
sn_applet_class_init (SnAppletClass *applet_class)
{
  GObjectClass *object_class;
  PanelAppletClass *panel_applet_class;

  object_class = G_OBJECT_CLASS (applet_class);
  panel_applet_class = PANEL_APPLET_CLASS (applet_class);

  object_class->dispose = sn_applet_dispose;

  panel_applet_class->change_orient = sn_applet_change_orient;
}

static void
sn_applet_init (SnApplet *applet)
{
  PanelApplet *panel_applet;
  GtkOrientation orientation;

  panel_applet = PANEL_APPLET (applet);
  orientation = panel_applet_get_gtk_orientation (panel_applet);

  panel_applet_set_flags (panel_applet, PANEL_APPLET_HAS_HANDLE |
                          PANEL_APPLET_EXPAND_MINOR);

  applet->box = gtk_box_new (orientation, 0);
  gtk_container_add (GTK_CONTAINER (applet), applet->box);
  gtk_widget_show (applet->box);
}

PANEL_APPLET_IN_PROCESS_FACTORY ("SnAppletFactory", SN_TYPE_APPLET,
                                 sn_applet_factory, NULL);
