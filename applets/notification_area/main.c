/* System tray main() */

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

#include <config.h>
#include <string.h>

#include <panel-applet.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "main.h"
#include "na-tray-manager.h"
#include "na-tray.h"
#include "fixedtip.h"

#define NOTIFICATION_AREA_ICON "gnome-panel-notification-area"

static void na_tray_applet_style_updated (GtkWidget *widget);

struct _NaTrayAppletPrivate
{
  NaTray *tray;
};

G_DEFINE_TYPE (NaTrayApplet, na_tray_applet, PANEL_TYPE_APPLET)

static GtkOrientation
get_gtk_orientation_from_applet_orient (PanelAppletOrient orient)
{
  switch (orient)
    {
    case PANEL_APPLET_ORIENT_LEFT:
    case PANEL_APPLET_ORIENT_RIGHT:
      return GTK_ORIENTATION_VERTICAL;
    case PANEL_APPLET_ORIENT_UP:
    case PANEL_APPLET_ORIENT_DOWN:
    default:
      return GTK_ORIENTATION_HORIZONTAL;
    }

  g_assert_not_reached ();

  return GTK_ORIENTATION_HORIZONTAL;
}

static void
na_tray_applet_realize (GtkWidget *widget)
{
  NaTrayApplet      *applet = NA_TRAY_APPLET (widget);
  PanelAppletOrient  orient;

  GTK_WIDGET_CLASS (na_tray_applet_parent_class)->realize (widget);

  g_assert (applet->priv->tray == NULL);

  orient = panel_applet_get_orient (PANEL_APPLET (widget));

  applet->priv->tray = na_tray_new_for_screen (gtk_widget_get_screen (widget),
                                               get_gtk_orientation_from_applet_orient (orient));

  gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (applet->priv->tray));
  na_tray_applet_style_updated (widget);
  gtk_widget_show (GTK_WIDGET (applet->priv->tray));
}

static void
na_tray_applet_unrealize (GtkWidget *widget)
{
  NaTrayApplet *applet = NA_TRAY_APPLET (widget);

  g_assert (applet->priv->tray != NULL);

  gtk_widget_destroy (GTK_WIDGET (applet->priv->tray));
  applet->priv->tray = NULL;

  GTK_WIDGET_CLASS (na_tray_applet_parent_class)->unrealize (widget);
}

static void
na_tray_applet_style_updated (GtkWidget *widget)
{
  NaTrayApplet    *applet = NA_TRAY_APPLET (widget);
  GtkStyleContext *context;
  GdkRGBA          fg;
  GdkRGBA          error;
  GdkRGBA          warning;
  GdkRGBA          success;
  gint             padding;
  gint             icon_size;

  GTK_WIDGET_CLASS (na_tray_applet_parent_class)->style_updated (widget);

  if (!applet->priv->tray)
    return;

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

  na_tray_set_colors (applet->priv->tray, &fg, &error, &warning, &success);

  gtk_widget_style_get (widget, "icon-padding", &padding, NULL);
  na_tray_set_padding (applet->priv->tray, padding);

  gtk_widget_style_get (widget, "icon-size", &icon_size, NULL);
  na_tray_set_icon_size (applet->priv->tray, icon_size);
}

static void
na_tray_applet_change_background (PanelApplet     *panel_applet,
                                  cairo_pattern_t *pattern)
{
  NaTrayApplet *applet = NA_TRAY_APPLET (panel_applet);

  if (!applet->priv->tray)
    return;

  na_tray_force_redraw (applet->priv->tray);
}

static void
na_tray_applet_change_orient (PanelApplet       *panel_applet,
                              PanelAppletOrient  orient)
{
  NaTrayApplet *applet = NA_TRAY_APPLET (panel_applet);

  if (!applet->priv->tray)
    return;

  na_tray_set_orientation (applet->priv->tray,
                           get_gtk_orientation_from_applet_orient (orient));
}

static void
na_tray_applet_class_init (NaTrayAppletClass *class)
{
  GtkWidgetClass   *widget_class = GTK_WIDGET_CLASS (class);
  PanelAppletClass *applet_class = PANEL_APPLET_CLASS (class);

  widget_class->realize = na_tray_applet_realize;
  widget_class->unrealize = na_tray_applet_unrealize;
  widget_class->style_updated = na_tray_applet_style_updated;
  applet_class->change_background = na_tray_applet_change_background;
  applet_class->change_orient = na_tray_applet_change_orient;

  gtk_widget_class_install_style_property (
          widget_class,
          g_param_spec_int ("icon-padding",
                            "Padding around icons",
                            "Padding that should be put around icons, in pixels",
                            0, G_MAXINT, 0,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_install_style_property (
          widget_class,
          g_param_spec_int ("icon-size",
                            "Icon size",
                            "If non-zero, hardcodes the size of the icons in pixels",
                            0, G_MAXINT, 0,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (class, sizeof (NaTrayAppletPrivate));
}

static void
na_tray_applet_init (NaTrayApplet *applet)
{
  AtkObject *atko;

  applet->priv = G_TYPE_INSTANCE_GET_PRIVATE (applet, NA_TYPE_TRAY_APPLET,
                                              NaTrayAppletPrivate);

  /* Defer creating NaTray until applet is added to panel so
   * gtk_widget_get_screen returns correct information */
  applet->priv->tray = NULL;

  atko = gtk_widget_get_accessible (GTK_WIDGET (applet));
  atk_object_set_name (atko, _("Panel Notification Area"));

  panel_applet_set_flags (PANEL_APPLET (applet),
                          PANEL_APPLET_HAS_HANDLE|PANEL_APPLET_EXPAND_MINOR);
}

static gboolean
applet_factory (PanelApplet *applet,
                const gchar *iid,
                gpointer     user_data)
{
  if (!(strcmp (iid, "NotificationArea") == 0 ||
        strcmp (iid, "SystemTrayApplet") == 0))
    return FALSE;

  gtk_widget_show_all (GTK_WIDGET (applet));

  return TRUE;
}

PANEL_APPLET_IN_PROCESS_FACTORY ("NotificationAreaAppletFactory",
				 NA_TYPE_TRAY_APPLET,
				 applet_factory,
				 NULL)
