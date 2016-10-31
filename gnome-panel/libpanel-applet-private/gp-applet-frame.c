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

#include <glib/gi18n.h>
#include <string.h>

#include "gp-applet-frame.h"
#include "panel-layout.h"

struct _GpAppletFrame
{
  PanelAppletFrame  parent;
  GpApplet         *applet;
};

G_DEFINE_TYPE (GpAppletFrame, gp_applet_frame, PANEL_TYPE_APPLET_FRAME)

static void
popup_menu (GtkMenu  *menu,
            GpApplet *applet)
{
  GtkOrientation orientation;
  GtkPositionType position;
  GdkGravity widget_anchor;
  GdkGravity menu_anchor;

  orientation = gp_applet_get_orientation (applet);
  position = gp_applet_get_position (applet);

  widget_anchor = GDK_GRAVITY_SOUTH_WEST;
  menu_anchor = GDK_GRAVITY_NORTH_WEST;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (position == GTK_POS_TOP)
        {
          widget_anchor = GDK_GRAVITY_SOUTH_WEST;
          menu_anchor = GDK_GRAVITY_NORTH_WEST;
        }
      else if (position == GTK_POS_BOTTOM)
        {
          widget_anchor = GDK_GRAVITY_NORTH_WEST;
          menu_anchor = GDK_GRAVITY_SOUTH_WEST;
        }
      else
        g_assert_not_reached ();
    }
  else
    {
      if (position == GTK_POS_LEFT)
        {
          widget_anchor = GDK_GRAVITY_NORTH_EAST;
          menu_anchor = GDK_GRAVITY_NORTH_WEST;
        }
      else if (position == GTK_POS_RIGHT)
        {
          widget_anchor = GDK_GRAVITY_NORTH_WEST;
          menu_anchor = GDK_GRAVITY_NORTH_EAST;
        }
      else
        g_assert_not_reached ();
    }

  gtk_menu_attach_to_widget (menu, GTK_WIDGET (applet), NULL);
  gtk_menu_popup_at_widget (menu, GTK_WIDGET (applet),
                            widget_anchor, menu_anchor, NULL);
}

static void
move_cb (GtkMenuItem      *menuitem,
         PanelAppletFrame *applet_frame)
{
  _panel_applet_frame_applet_move (applet_frame);
}

static void
remove_cb (GtkMenuItem      *menuitem,
           PanelAppletFrame *applet_frame)
{
  _panel_applet_frame_applet_remove (applet_frame);
}

static void
update_flags (PanelAppletFrame *applet_frame)
{
  GpAppletFrame *frame;
  GpAppletFlags flags;
  gboolean major;
  gboolean minor;
  gboolean has_handle;

  frame = GP_APPLET_FRAME (applet_frame);
  flags = gp_applet_get_flags (frame->applet);

  major = (flags & GP_APPLET_FLAGS_EXPAND_MAJOR) != 0;
  minor = (flags & GP_APPLET_FLAGS_EXPAND_MINOR) != 0;
  has_handle = (flags & GP_APPLET_FLAGS_HAS_HANDLE) != 0;

  _panel_applet_frame_update_flags (applet_frame, major, minor, has_handle);
}

static void
update_size_hints (PanelAppletFrame *applet_frame)
{
  GpAppletFrame *frame;
  GArray *array;
  gint *size_hints;
  guint i;

  frame = GP_APPLET_FRAME (applet_frame);
  array = gp_applet_get_size_hints (frame->applet);

  if (!array || array->len == 0)
    {
      _panel_applet_frame_update_size_hints (applet_frame, NULL, 0);
      return;
    }

  size_hints = g_new (gint, array->len);

  for (i = 0; i < array->len; i++)
    {
      size_hints[i] = g_array_index (array, gint, i);
    }

  _panel_applet_frame_update_size_hints (applet_frame, size_hints, array->len);
}

static void
flags_changed_cb (GObject       *object,
                  GParamSpec    *pspec,
                  GpAppletFrame *frame)
{
  update_flags (PANEL_APPLET_FRAME (frame));
}

static void
size_hints_changed_cb (GObject       *object,
                       GParamSpec    *pspec,
                       GpAppletFrame *frame)
{
  update_size_hints (PANEL_APPLET_FRAME (frame));
}

static void
gp_applet_frame_init_properties (PanelAppletFrame *applet_frame)
{
  update_flags (applet_frame);
  update_size_hints (applet_frame);
}

static void
gp_applet_frame_sync_menu_state (PanelAppletFrame *applet_frame,
                                 gboolean          movable,
                                 gboolean          removable,
                                 gboolean          locked_down)
{
  GpAppletFrame *frame;

  frame = GP_APPLET_FRAME (applet_frame);

  gp_applet_set_locked_down (frame->applet, locked_down);
}

static void
gp_applet_frame_popup_menu (PanelAppletFrame *applet_frame,
                            guint             button,
                            guint32           timestamp)
{
  GpAppletFrame *frame;
  GtkWidget *menu;

  frame = GP_APPLET_FRAME (applet_frame);
  menu = gp_applet_get_menu (frame->applet);

  if (menu == NULL)
    return;

  popup_menu (GTK_MENU (menu), frame->applet);
}

static void
gp_applet_frame_popup_edit_menu (PanelAppletFrame *applet_frame,
                                 guint             button,
                                 guint32           timestamp)
{
  GpAppletFrame *frame;
  GtkWidget *menu;
  GtkWidget *menuitem;
  gboolean movable;
  gboolean removable;

  frame = GP_APPLET_FRAME (applet_frame);
  menu = gtk_menu_new ();

  movable = _panel_applet_frame_get_can_move (applet_frame);
  removable = panel_layout_is_writable ();

  menuitem = gtk_menu_item_new_with_mnemonic (_("_Move"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  g_signal_connect (menuitem, "activate", G_CALLBACK (move_cb), applet_frame);
  gtk_widget_set_sensitive (menuitem, movable);

  menuitem = gtk_menu_item_new_with_mnemonic (_("_Remove From Panel"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  g_signal_connect (menuitem, "activate", G_CALLBACK (remove_cb), applet_frame);
  gtk_widget_set_sensitive (menuitem, removable);

  popup_menu (GTK_MENU (menu), frame->applet);
}

static void
gp_applet_frame_change_orientation (PanelAppletFrame *applet_frame,
                                    PanelOrientation  panel_orientation)
{
  GpAppletFrame *frame;
  GtkOrientation orientation;
  GtkPositionType position;

  switch (panel_orientation)
    {
      case PANEL_ORIENTATION_BOTTOM:
        orientation = GTK_ORIENTATION_HORIZONTAL;
        position = GTK_POS_BOTTOM;
        break;
      case PANEL_ORIENTATION_LEFT:
        orientation = GTK_ORIENTATION_VERTICAL;
        position = GTK_POS_LEFT;
        break;
      case PANEL_ORIENTATION_RIGHT:
        orientation = GTK_ORIENTATION_VERTICAL;
        position = GTK_POS_RIGHT;
        break;
      case PANEL_ORIENTATION_TOP:
        orientation = GTK_ORIENTATION_HORIZONTAL;
        position = GTK_POS_TOP;
      default:
        break;
    }

  frame = GP_APPLET_FRAME (applet_frame);

  gp_applet_set_orientation (frame->applet, orientation);
  gp_applet_set_position (frame->applet, position);
  gtk_widget_queue_resize (GTK_WIDGET (frame));
}

static void
gp_applet_frame_class_init (GpAppletFrameClass *frame_class)
{
  PanelAppletFrameClass *applet_frame_class;

  applet_frame_class = PANEL_APPLET_FRAME_CLASS (frame_class);

  applet_frame_class->init_properties = gp_applet_frame_init_properties;
  applet_frame_class->sync_menu_state = gp_applet_frame_sync_menu_state;
  applet_frame_class->popup_menu = gp_applet_frame_popup_menu;
  applet_frame_class->popup_edit_menu = gp_applet_frame_popup_edit_menu;
  applet_frame_class->change_orientation = gp_applet_frame_change_orientation;
}

static void
gp_applet_frame_init (GpAppletFrame *frame)
{
}

void
gp_applet_frame_set_applet (GpAppletFrame *frame,
                            GpApplet      *applet)
{
  frame->applet = applet;

  g_signal_connect (applet, "notify::flags",
                    G_CALLBACK (flags_changed_cb), frame);

  g_signal_connect (applet, "notify::size-hints",
                    G_CALLBACK (size_hints_changed_cb), frame);
}
