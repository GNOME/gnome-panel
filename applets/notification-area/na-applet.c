/*
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2003-2006 Vincent Untz
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
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "na-applet.h"
#include "na-tray-manager.h"
#include "na-tray.h"
#include "fixedtip.h"

struct _NaApplet
{
  GpApplet  parent;
  NaTray   *tray;
};

G_DEFINE_TYPE (NaApplet, na_applet, GP_TYPE_APPLET)

static void
update_style (GtkWidget *widget)
{
  NaApplet *na;
  GtkStyleContext *context;
  GdkRGBA fg;
  GdkRGBA error;
  GdkRGBA warning;
  GdkRGBA success;
  gint padding;
  gint icon_size;

  na = NA_APPLET (widget);

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);

  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &fg);

  if (!gtk_style_context_lookup_color (context, "error_color", &error))
    error = fg;
  if (!gtk_style_context_lookup_color (context, "warning_color", &warning))
    warning = fg;
  if (!gtk_style_context_lookup_color (context, "success_color", &success))
    success = fg;

  gtk_style_context_restore (context);

  na_tray_set_colors (na->tray, &fg, &error, &warning, &success);

  gtk_widget_style_get (widget, "icon-padding", &padding, NULL);
  na_tray_set_padding (na->tray, padding);

  gtk_widget_style_get (widget, "icon-size", &icon_size, NULL);
  na_tray_set_icon_size (na->tray, icon_size);
}

static void
na_applet_style_updated (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (na_applet_parent_class)->style_updated (widget);
  update_style (widget);
}

static void
na_applet_constructed (GObject *object)
{
  G_OBJECT_CLASS (na_applet_parent_class)->constructed (object);
  gtk_widget_show (GTK_WIDGET (object));
}

static void
na_applet_placement_changed (GpApplet        *applet,
                             GtkOrientation   orientation,
                             GtkPositionType  position)
{
  NaApplet *na;

  na = NA_APPLET (applet);

  na_tray_set_orientation (na->tray, orientation);
}

static void
install_style_properties (GtkWidgetClass *widget_class)
{
  GParamSpec *spec;

  spec = g_param_spec_int ("icon-padding",
                           "Padding around icons",
                           "Padding that should be put around icons, in pixels",
                           0, G_MAXINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  gtk_widget_class_install_style_property (widget_class, spec);

  spec = g_param_spec_int ("icon-size",
                           "Icon size",
                           "If non-zero, hardcodes the size of the icons in pixels",
                           0, G_MAXINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  gtk_widget_class_install_style_property (widget_class, spec);
}

static void
na_applet_class_init (NaAppletClass *na_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GpAppletClass *applet_class;

  object_class = G_OBJECT_CLASS (na_class);
  widget_class = GTK_WIDGET_CLASS (na_class);
  applet_class = GP_APPLET_CLASS (na_class);

  object_class->constructed = na_applet_constructed;

  widget_class->style_updated = na_applet_style_updated;

  applet_class->placement_changed = na_applet_placement_changed;

  gtk_widget_class_set_css_name (widget_class, "na-tray-applet");
  install_style_properties (widget_class);
}

static void
na_applet_init (NaApplet *na)
{
  GpAppletFlags flags;
  AtkObject *atk_object;
  GdkScreen *screen;
  GtkOrientation orientation;

  flags = GP_APPLET_FLAGS_HAS_HANDLE | GP_APPLET_FLAGS_EXPAND_MINOR;
  gp_applet_set_flags (GP_APPLET (na), flags);

  atk_object = gtk_widget_get_accessible (GTK_WIDGET (na));
  atk_object_set_name (atk_object, _("Panel Notification Area"));

  screen = gdk_screen_get_default ();
  orientation = gp_applet_get_orientation (GP_APPLET (na));
  na->tray = na_tray_new_for_screen (screen, orientation);

  gtk_container_add (GTK_CONTAINER (na), GTK_WIDGET (na->tray));
  gtk_widget_show (GTK_WIDGET (na->tray));

  update_style (GTK_WIDGET (na));
}
