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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include <string.h>

#include <panel-applet.h>
#include <panel-applet-gconf.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "na-tray-manager.h"
#include "na-tray.h"
#include "fixedtip.h"

#define NOTIFICATION_AREA_ICON "gnome-panel-notification-area"

typedef struct
{
  PanelApplet *applet;
  NaTray      *tray;
  GtkWidget   *about_dialog;
} AppletData;

static GtkOrientation
get_orientation_from_applet (PanelApplet *applet)
{
  GtkOrientation orientation;

  switch (panel_applet_get_orient (applet))
    {
    case PANEL_APPLET_ORIENT_LEFT:
    case PANEL_APPLET_ORIENT_RIGHT:
      orientation = GTK_ORIENTATION_VERTICAL;
      break;
    case PANEL_APPLET_ORIENT_UP:
    case PANEL_APPLET_ORIENT_DOWN:
    default:
      orientation = GTK_ORIENTATION_HORIZONTAL;
      break;
    }

  return orientation;
}

static const GtkActionEntry menu_actions [] = {
};

static void
applet_change_background (PanelApplet     *applet,
                          cairo_pattern_t *pattern,
                          AppletData      *data)
{
  na_tray_force_redraw (data->tray);
}


static void
applet_change_orientation (PanelApplet       *applet,
                           PanelAppletOrient  orient,
                           AppletData         *data)
{
  na_tray_set_orientation (data->tray, get_orientation_from_applet (applet));
}

static void
applet_destroy (PanelApplet *applet,
		AppletData  *data)
{
  if (data->about_dialog)
    gtk_widget_destroy (data->about_dialog);
  g_assert (data->about_dialog == NULL);
}

static void
free_applet_data (AppletData *data)
{
  g_slice_free (AppletData, data);
}

static void
on_applet_realized (GtkWidget *widget,
                    gpointer   user_data)
{
  PanelApplet    *applet;
  AppletData     *data;
  NaTray         *tray;
  GtkActionGroup *action_group;
  gchar          *ui_path;

  applet = PANEL_APPLET (widget);
  data = g_object_get_data (G_OBJECT (widget), "system-tray-data");

  if (data != NULL)
    return;

  tray = na_tray_new_for_screen (gtk_widget_get_screen (GTK_WIDGET (applet)),
                                 get_orientation_from_applet (applet));

  data = g_slice_new (AppletData);
  data->applet = applet;
  data->tray = tray;
  data->about_dialog = NULL;

  g_object_set_data_full (G_OBJECT (applet),
                          "system-tray-data",
                          data,
                          (GDestroyNotify) free_applet_data);

  g_signal_connect (applet, "change_orient",
                    G_CALLBACK (applet_change_orientation), data);
  g_signal_connect (applet, "change_background",
                    G_CALLBACK (applet_change_background), data);
  g_signal_connect (applet, "destroy",
                    G_CALLBACK (applet_destroy), data);

  gtk_container_add (GTK_CONTAINER (applet), GTK_WIDGET (tray));
  gtk_widget_show (GTK_WIDGET (tray));

  action_group = gtk_action_group_new ("ClockApplet Menu Actions");
  gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (action_group,
                                menu_actions,
                                G_N_ELEMENTS (menu_actions),
                                data);
  ui_path = g_build_filename (NOTIFICATION_AREA_MENU_UI_DIR, "notification-area-menu.xml", NULL);
  panel_applet_setup_menu_from_file (applet,
                                     ui_path, action_group);
  g_free (ui_path);
  g_object_unref (action_group);

}

static inline void
force_no_focus_padding (GtkWidget *widget)
{
  GtkCssProvider *provider;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider,
                                   "#na-tray {\n"
                                   " -GtkWidget-focus-line-width: 0px;\n"
                                   " -GtkWidget-focus-padding: 0px;\n"
				   "}",
                                   -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  gtk_widget_set_name (widget, "na-tray");
}

static gboolean
applet_factory (PanelApplet *applet,
                const gchar *iid,
                gpointer     user_data)
{
  AtkObject *atko;

  if (!(strcmp (iid, "NotificationArea") == 0 ||
        strcmp (iid, "SystemTrayApplet") == 0))
    return FALSE;

  /* Defer loading until applet is added to panel so
   * gtk_widget_get_screen returns correct information */
  g_signal_connect (GTK_WIDGET (applet), "realize",
                    G_CALLBACK (on_applet_realized),
                    NULL);

  atko = gtk_widget_get_accessible (GTK_WIDGET (applet));
  atk_object_set_name (atko, _("Panel Notification Area"));

  panel_applet_set_flags (applet,
                          PANEL_APPLET_HAS_HANDLE|PANEL_APPLET_EXPAND_MINOR);
  
  panel_applet_set_background_widget (applet, GTK_WIDGET (applet));

  force_no_focus_padding (GTK_WIDGET (applet));

#ifndef NOTIFICATION_AREA_INPROCESS
  gtk_window_set_default_icon_name (NOTIFICATION_AREA_ICON);
#endif
  gtk_widget_show_all (GTK_WIDGET (applet));
  return TRUE;
}

#ifdef NOTIFICATION_AREA_INPROCESS
PANEL_APPLET_IN_PROCESS_FACTORY ("NotificationAreaAppletFactory",
				 PANEL_TYPE_APPLET,
				 "NotificationArea",
				 applet_factory,
				 NULL)
#else
PANEL_APPLET_OUT_PROCESS_FACTORY ("NotificationAreaAppletFactory",
				  PANEL_TYPE_APPLET,
				  "NotificationArea",
				  applet_factory,
				  NULL)
#endif
