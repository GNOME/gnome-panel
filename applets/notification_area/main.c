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

static void
help_cb (GtkAction  *action,
	 AppletData *data)
{
  GError *error = NULL;
  char   *uri;
#define NA_HELP_DOC "user-guide"

  uri = g_strdup_printf ("ghelp:%s?%s",
                         NA_HELP_DOC, "panels-notification-area");

  gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (data->applet)), uri,
                gtk_get_current_event_time (), &error);

  g_free (uri);

  if (error &&
      g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
    }
  else if (error)
    {
      GtkWidget *dialog;
      char      *primary;

      primary = g_markup_printf_escaped (
                                    _("Could not display help document '%s'"),
                                    NA_HELP_DOC);
      dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                       "%s", primary);

      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s", error->message);

      g_error_free (error);
      g_free (primary);

      g_signal_connect (dialog, "response",
                        G_CALLBACK (gtk_widget_destroy), NULL);

      gtk_window_set_icon_name (GTK_WINDOW (dialog), NOTIFICATION_AREA_ICON);
      gtk_window_set_screen (GTK_WINDOW (dialog),
                             gtk_widget_get_screen (GTK_WIDGET (data->applet)));
      /* we have no parent window */
      gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
      gtk_window_set_title (GTK_WINDOW (dialog),
                            _("Error displaying help document"));

      gtk_widget_show (dialog);
    }
}

static void
about_cb (GtkAction  *action,
          AppletData *data)
{
  GdkScreen    *screen;

  const char *authors[] = {
    "Havoc Pennington <hp@redhat.com>",
    "Anders Carlsson <andersca@gnu.org>",
    "Vincent Untz <vuntz@gnome.org>",
    NULL
  };
  const char *documenters [] = {
    "Sun GNOME Documentation Team <gdocteam@sun.com>",
    NULL
  };

  screen = gtk_widget_get_screen (GTK_WIDGET (data->applet));

  if (data->about_dialog)
    {
      gtk_window_set_screen (GTK_WINDOW (data->about_dialog), screen);
      gtk_window_present (GTK_WINDOW (data->about_dialog));
      return;
    }

  data->about_dialog = gtk_about_dialog_new ();
  g_object_set (data->about_dialog,
                "program-name", _("Notification Area"),
                "version", VERSION,
                "copyright", "Copyright \xc2\xa9 2002 Red Hat, Inc.",
                "authors", (const char **) authors,
                "documenters", (const char **) documenters,
                "translator-credits", _("translator-credits"),
                "logo-icon-name", NOTIFICATION_AREA_ICON,
                NULL);
  
  gtk_window_set_icon_name (GTK_WINDOW (data->about_dialog),
                            NOTIFICATION_AREA_ICON);
  gtk_window_set_screen (GTK_WINDOW (data->about_dialog), screen);

  g_object_add_weak_pointer (G_OBJECT (data->about_dialog),
                             (gpointer) &data->about_dialog);

  g_signal_connect (data->about_dialog, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);

  gtk_window_present (GTK_WINDOW (data->about_dialog));
}

static const GtkActionEntry menu_actions [] = {
	{ "SystemTrayHelp", GTK_STOCK_HELP, N_("_Help"),
	  NULL, NULL,
	  G_CALLBACK (help_cb) },
	{ "SystemTrayAbout", GTK_STOCK_ABOUT, N_("_About"),
	  NULL, NULL,
	  G_CALLBACK (about_cb) }
};

static void
applet_change_background (PanelApplet               *applet,
                          PanelAppletBackgroundType  type,
                          GdkColor                  *color,
                          GdkPixmap                 *pixmap,
                          AppletData                *data)
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
  static gboolean first_time = TRUE;

  if (first_time)
    {
      gtk_rc_parse_string ("\n"
                           "   style \"na-tray-style\"\n"
                           "   {\n"
                           "      GtkWidget::focus-line-width=0\n"
                           "      GtkWidget::focus-padding=0\n"
                           "   }\n"
                           "\n"
                           "    widget \"*.na-tray\" style \"na-tray-style\"\n"
                           "\n");
      first_time = FALSE;
    }

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
