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

#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkhbox.h>
#include <libgnomeui/gnome-help.h>

#include "na-tray-manager.h"
#include "na-tray.h"
#include "fixedtip.h"
#include "obox.h"

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
help_cb (BonoboUIComponent *uic,
	 AppletData        *data,	  
	 const gchar       *verbname)   
{
  GdkScreen *screen;
  GError *err = NULL;

  screen = gtk_widget_get_screen (GTK_WIDGET (data->applet));

  gnome_help_display_desktop_on_screen (NULL, "user-guide",
                                        "user-guide.xml", "gospanel-567",
					screen, &err);

  if (err != NULL)
    {
      GtkWidget *dialog;
      
      dialog = gtk_message_dialog_new (NULL,
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_OK,
                                       _("There was an error displaying help: %s"),
                                       err->message);
      
      g_signal_connect (G_OBJECT (dialog), "response",
                        G_CALLBACK (gtk_widget_destroy), NULL);
      
      gtk_window_set_icon_name (GTK_WINDOW (dialog), NOTIFICATION_AREA_ICON);
      gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
      gtk_window_set_screen (GTK_WINDOW (dialog), screen);
      
      gtk_widget_show (dialog);

      g_error_free (err);
    }
}

static void
about_cb (BonoboUIComponent *uic,
          AppletData        *data,
          const gchar       *verbname)
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

static const BonoboUIVerb menu_verbs [] = {
  BONOBO_UI_UNSAFE_VERB ("SystemTrayHelp",       help_cb),
  BONOBO_UI_UNSAFE_VERB ("SystemTrayAbout",      about_cb),
  BONOBO_UI_VERB_END
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

static gboolean
applet_factory (PanelApplet *applet,
                const gchar *iid,
                gpointer     user_data)
{
  NaTray     *tray;
  AppletData *data;
  AtkObject  *atko;

  if (!(strcmp (iid, "OAFIID:GNOME_NotificationAreaApplet") == 0 ||
        strcmp (iid, "OAFIID:GNOME_SystemTrayApplet") == 0))
    return FALSE;

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

  atko = gtk_widget_get_accessible (GTK_WIDGET (applet));
  atk_object_set_name (atko, _("Panel Notification Area"));

  panel_applet_set_flags (applet,
                          PANEL_APPLET_HAS_HANDLE|PANEL_APPLET_EXPAND_MINOR);
  
  g_signal_connect (applet, "change_orient",
                    G_CALLBACK (applet_change_orientation), data);
  g_signal_connect (applet, "change_background",
                    G_CALLBACK (applet_change_background), data);
  g_signal_connect (applet, "destroy",
		    G_CALLBACK (applet_destroy), data);

  panel_applet_set_background_widget (applet, GTK_WIDGET (applet));

  gtk_container_add (GTK_CONTAINER (applet), GTK_WIDGET (tray));
  
#ifndef NOTIFICATION_AREA_INPROCESS
  gtk_window_set_default_icon_name (NOTIFICATION_AREA_ICON);
#endif
  gtk_widget_show_all (GTK_WIDGET (applet));

  panel_applet_setup_menu_from_file (applet,
  			             NULL,
                                     "GNOME_NotificationAreaApplet.xml",
                                     NULL,
                                     menu_verbs,
                                     data);
  
  return TRUE;
}

#if NOTIFICATION_AREA_INPROCESS
PANEL_APPLET_BONOBO_SHLIB_FACTORY ("OAFIID:GNOME_NotificationAreaApplet_Factory",
				   PANEL_TYPE_APPLET,
				   "NotificationArea",
				   applet_factory,
				   NULL);
#else
PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_NotificationAreaApplet_Factory",
			     PANEL_TYPE_APPLET,
                             "NotificationArea",
                             "0",
                             applet_factory,
                             NULL)
#endif
