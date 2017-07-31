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

#include "separator-applet.h"

struct _SeparatorApplet
{
  GpApplet   parent;

  GtkWidget *separator;
};

G_DEFINE_TYPE (SeparatorApplet, separator_applet, GP_TYPE_APPLET)

static void
separator_applet_placement_changed (GpApplet        *applet,
                                    GtkOrientation   orientation,
                                    GtkPositionType  position)
{
  SeparatorApplet *separator;
  GtkOrientable *orientable;

  separator = SEPARATOR_APPLET (applet);
  orientable = GTK_ORIENTABLE (separator->separator);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_orientable_set_orientation (orientable, GTK_ORIENTATION_VERTICAL);
  else
    gtk_orientable_set_orientation (orientable, GTK_ORIENTATION_HORIZONTAL);
}

static void
separator_applet_class_init (SeparatorAppletClass *separator_class)
{
  GpAppletClass *applet_class;

  applet_class = GP_APPLET_CLASS (separator_class);

  applet_class->placement_changed = separator_applet_placement_changed;
}

static void
separator_applet_init (SeparatorApplet *separator)
{
  GpApplet *applet;
  GpAppletFlags flags;
  GtkOrientation orientation;

  applet = GP_APPLET (separator);

  flags = GP_APPLET_FLAGS_EXPAND_MINOR;
  orientation = gp_applet_get_orientation (applet);

  gp_applet_set_flags (applet, flags);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    separator->separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  else
    separator->separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);

  gtk_container_add (GTK_CONTAINER (separator), separator->separator);
  gtk_widget_show (separator->separator);
}
