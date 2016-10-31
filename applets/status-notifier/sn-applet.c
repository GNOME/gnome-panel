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

struct _SnApplet
{
  GpApplet   parent;

  GtkWidget *box;
};

G_DEFINE_TYPE (SnApplet, sn_applet, GP_TYPE_APPLET)

static void
sn_applet_constructed (GObject *object)
{
  G_OBJECT_CLASS (sn_applet_parent_class)->constructed (object);
  gtk_widget_show (GTK_WIDGET (object));
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
