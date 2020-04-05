/*
 * Copyright (C) 2016 Alberts Muktupāvels
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
#include "sn-host-v0.h"
#include "sn-item.h"

struct _SnApplet
{
  GpApplet   parent;

  GtkWidget *box;

  GSList    *hosts;
  GSList    *items;
};

G_DEFINE_TYPE (SnApplet, sn_applet, GP_TYPE_APPLET)

static gint
compare_items (gconstpointer a,
               gconstpointer b)
{
  SnItem *item1;
  SnItem *item2;
  SnItemCategory c1;
  SnItemCategory c2;
  const gchar *id1;
  const gchar *id2;

  item1 = (SnItem *) a;
  item2 = (SnItem *) b;

  c1 = sn_item_get_category (item1);
  c2 = sn_item_get_category (item2);

  if (c1 < c2)
    return -1;
  else if (c1 > c2)
    return 1;

  id1 = sn_item_get_id (item1);
  id2 = sn_item_get_id (item2);

  return g_strcmp0 (id1, id2);
}

static void
reorder_items (GtkWidget *widget,
               gpointer   user_data)
{
  SnApplet *sn;
  gint position;

  sn = SN_APPLET (user_data);

  position = g_slist_index (sn->items, widget);
  gtk_box_reorder_child (GTK_BOX (sn->box), widget, position);
}

static void
get_popup_position (SnApplet *sn,
                    SnItem   *item,
                    gint     *x,
                    gint     *y)
{
  GtkWidget *widget;
  GtkWidget *toplevel;
  GdkWindow *window;
  GpApplet *applet;
  GtkPositionType position;

  widget = GTK_WIDGET (item);
  toplevel = gtk_widget_get_toplevel (widget);
  window = gtk_widget_get_window (toplevel);

  gtk_widget_translate_coordinates (widget, toplevel, 0, 0, x, y);
  gdk_window_get_root_coords (window, *x, *y, x, y);

  applet = GP_APPLET (sn);
  position = gp_applet_get_position (applet);

  if (position == GTK_POS_TOP || position == GTK_POS_LEFT)
    {
      gint width, height;

      gdk_window_get_geometry (window, NULL, NULL, &width, &height);

      if (gp_applet_get_orientation (applet) == GTK_ORIENTATION_HORIZONTAL)
        *y += height;
      else
        *x += width;
    }
}

static gboolean
button_press_event_cb (GtkWidget      *widget,
                       GdkEventButton *event,
                       SnApplet       *sn)
{
  SnItem *item;
  gint x, y;

  item = SN_ITEM (widget);

  if (event->button == 2)
    {
      get_popup_position (sn, item, &x, &y);
      SN_ITEM_GET_CLASS (item)->secondary_activate (item, x, y);
    }
  else if (event->button == 3)
    {
      GtkMenu *menu;

      menu = sn_item_get_menu (item);

      if (menu != NULL)
        {
          gp_applet_popup_menu_at_widget (GP_APPLET (sn),
                                          menu,
                                          GTK_WIDGET (item),
                                          (GdkEvent *) event);

          return GDK_EVENT_STOP;
        }
      else
        {
          get_popup_position (sn, item, &x, &y);
          SN_ITEM_GET_CLASS (item)->context_menu (item, x, y);
        }
    }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
popup_menu_cb (GtkWidget *widget,
               SnApplet  *sn)
{
  SnItem *item;
  GtkMenu *menu;

  item = SN_ITEM (widget);
  menu = sn_item_get_menu (item);

  if (menu != NULL)
    {
      gp_applet_popup_menu_at_widget (GP_APPLET (sn),
                                      menu,
                                      GTK_WIDGET (item),
                                      NULL);
    }
  else
    {
      gint x, y;

      get_popup_position (sn, item, &x, &y);
      SN_ITEM_GET_CLASS (item)->context_menu (item, x, y);
    }

  return TRUE;
}

static void
clicked_cb (GtkButton *button,
            SnApplet  *sn)
{
  SnItem *item;
  gint x, y;

  item = SN_ITEM (button);

  get_popup_position (sn, item, &x, &y);
  SN_ITEM_GET_CLASS (item)->activate (item, x, y);
}

static void
item_added_cb (SnHost   *host,
               SnItem   *item,
               SnApplet *sn)
{
  sn->items = g_slist_prepend (sn->items, item);
  gtk_box_pack_start (GTK_BOX (sn->box), GTK_WIDGET (item), FALSE, FALSE, 0);

  sn->items = g_slist_sort (sn->items, compare_items);
  gtk_container_foreach (GTK_CONTAINER (sn->box), reorder_items, sn);

  g_signal_connect (item, "button-press-event",
                    G_CALLBACK (button_press_event_cb), sn);

  g_signal_connect (item, "popup-menu",
                    G_CALLBACK (popup_menu_cb), sn);

  g_signal_connect (item, "clicked",
                    G_CALLBACK (clicked_cb), sn);

  g_object_bind_property (sn->box, "orientation",
                          item, "orientation",
                          G_BINDING_DEFAULT);
}

static void
item_removed_cb (SnHost   *host,
                 SnItem   *item,
                 SnApplet *sn)
{
  gtk_container_remove (GTK_CONTAINER (sn->box), GTK_WIDGET (item));
  sn->items = g_slist_remove (sn->items, item);
}

static void
sn_applet_constructed (GObject *object)
{
  SnApplet *sn;
  SnHost *host;

  G_OBJECT_CLASS (sn_applet_parent_class)->constructed (object);
  sn = SN_APPLET (object);

  host = sn_host_v0_new (sn);
  sn->hosts = g_slist_prepend (sn->hosts, host);

  g_signal_connect (host, "item-added", G_CALLBACK (item_added_cb), sn);
  g_signal_connect (host, "item-removed", G_CALLBACK (item_removed_cb), sn);

  gtk_widget_show (GTK_WIDGET (object));
}

static void
sn_applet_dispose (GObject *object)
{
  SnApplet *sn;

  sn = SN_APPLET (object);

  if (sn->hosts != NULL)
    {
      g_slist_free_full (sn->hosts, g_object_unref);
      sn->hosts = NULL;
    }

  g_clear_pointer (&sn->items, g_slist_free);

  G_OBJECT_CLASS (sn_applet_parent_class)->dispose (object);
}

static void
sn_applet_placement_changed (GpApplet        *applet,
                             GtkOrientation   orientation,
                             GtkPositionType  position)
{
  SnApplet *sn;
  GtkOrientable *orientable;

  sn = SN_APPLET (applet);
  orientable = GTK_ORIENTABLE (sn->box);

  gtk_orientable_set_orientation (orientable, orientation);
}

static void
sn_applet_class_init (SnAppletClass *sn_class)
{
  GObjectClass *object_class;
  GpAppletClass *applet_class;

  object_class = G_OBJECT_CLASS (sn_class);
  applet_class = GP_APPLET_CLASS (sn_class);

  object_class->constructed = sn_applet_constructed;
  object_class->dispose = sn_applet_dispose;

  applet_class->placement_changed = sn_applet_placement_changed;
}

static void
sn_applet_init (SnApplet *sn)
{
  GpApplet *applet;
  GpAppletFlags flags;
  GtkOrientation orientation;

  applet = GP_APPLET (sn);

  flags = GP_APPLET_FLAGS_EXPAND_MINOR | GP_APPLET_FLAGS_HAS_HANDLE;
  orientation = gp_applet_get_orientation (applet);

  gp_applet_set_flags (applet, flags);

  sn->box = gtk_box_new (orientation, 0);
  gtk_container_add (GTK_CONTAINER (sn), sn->box);
  gtk_widget_show (sn->box);
}
