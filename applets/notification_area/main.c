/* System tray main() */

/* 
 * Copyright (C) 2002 Red Hat, Inc.
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
#include <libintl.h>

#include <panel-applet.h>
#include <panel-applet-gconf.h>

#include <libgnome/gnome-help.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkhbox.h>
#include <libgnomeui/gnome-about.h>

#include <bonobo/bonobo-shlib-factory.h>

#include "eggtraymanager.h"
#include "fixedtip.h"
#include "obox.h"

#ifndef _
#define _(x) gettext(x)
#endif

#ifndef N_
#define N_(x) x
#endif

static EggTrayManager *tray_manager = NULL;
static GSList *all_trays = NULL;

typedef struct
{
  PanelApplet *applet;

  GtkWidget *box;
  GtkWidget *frame;

  GtkOrientation orientation;
  int size;
  
} SystemTray;

static void
help_cb (PanelApplet *applet,
         void        *data)
{
  GError *err;

  err = NULL;  
  gnome_help_display ("notification-area-applet", NULL, &err);

  if (err != NULL)
    {
      GtkWidget *dialog;
      
      dialog = gtk_message_dialog_new (NULL,
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("There was an error displaying help: %s"),
                                       err->message);
      
      g_signal_connect (G_OBJECT (dialog), "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);
      
      gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
      
      gtk_widget_show (dialog);

      g_error_free (err);
    }
}


static void
about_cb (PanelApplet *applet,
          void        *data)
{
  static GtkWidget *about = NULL;
  GdkPixbuf *pixbuf = NULL;
  gchar *file;

  const char *authors[] = {
    "Havoc Pennington <hp@redhat.com>",
    "Anders Carlsson <andersca@gnu.org>",
    NULL
  };
  const char *documenters [] = {
    NULL
  };
  const char *translator_credits = _("translator_credits");

  if (about)
    {
      gtk_window_present (GTK_WINDOW (about));
      return;
    }

  file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, "notification-area-applet.png",
                                    TRUE, NULL);
  pixbuf = gdk_pixbuf_new_from_file (file, NULL);
  g_free (file);

  about = gnome_about_new (_("Panel Notification Area"), VERSION,
                           "Copyright \xc2\xa9 2002 Red Hat, Inc.",
                           NULL,
                           (const char **)authors,
                           (const char **)documenters,
                           strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
                           pixbuf);
  
  g_object_add_weak_pointer (G_OBJECT (about),
                             (void**) &about);

  gtk_widget_show (about);
}

static const BonoboUIVerb menu_verbs [] = {
  BONOBO_UI_UNSAFE_VERB ("SystemTrayHelp",       help_cb),
  BONOBO_UI_UNSAFE_VERB ("SystemTrayAbout",      about_cb),
  BONOBO_UI_VERB_END
};

static void
tray_added (EggTrayManager *manager, GtkWidget *icon, void *data)
{
  SystemTray *tray;

  if (all_trays == NULL)
    return;
  
  tray = all_trays->data;

  gtk_box_pack_end (GTK_BOX (tray->box), icon, FALSE, FALSE, 0);
  
  gtk_widget_show (icon);
}

static void
tray_removed (EggTrayManager *manager, GtkWidget *icon, void *data)
{

}

static void
message_sent (EggTrayManager *manager, GtkWidget *icon, const char *text, glong id, glong timeout,
              void *data)
{
  /* FIXME multihead */
  int x, y;

  gdk_window_get_origin (icon->window, &x, &y);
  
  fixed_tip_show (0, x, y, FALSE, gdk_screen_height () - 50, text);
}

static void
message_cancelled (EggTrayManager *manager, GtkWidget *icon, glong id,
                   void *data)
{
  
}

static void
update_size_and_orientation (SystemTray *tray)
{
  gtk_obox_set_orientation (GTK_OBOX (tray->box),
                            tray->orientation);

  /* note, you want this larger if the frame has non-NONE relief by default. */
#define MIN_BOX_SIZE 3
  switch (tray->orientation)
    {
    case GTK_ORIENTATION_VERTICAL:
      gtk_widget_set_size_request (tray->frame,
                                   tray->size, -1);

      /* Give box a min size so the frame doesn't look dumb */
      gtk_widget_set_size_request (tray->box, MIN_BOX_SIZE, -1);
      break;
    case GTK_ORIENTATION_HORIZONTAL:
      gtk_widget_set_size_request (tray->frame,
                                   -1, tray->size);
      gtk_widget_set_size_request (tray->box, -1, MIN_BOX_SIZE);
      break;
    }
}

static void
applet_change_orientation (PanelApplet       *applet,
                           PanelAppletOrient  orient,
                           SystemTray        *tray)
{
  GtkOrientation new_orient;
  
  switch (orient)
    {
    case PANEL_APPLET_ORIENT_LEFT:
    case PANEL_APPLET_ORIENT_RIGHT:
      new_orient = GTK_ORIENTATION_VERTICAL;
      break;
    case PANEL_APPLET_ORIENT_UP:
    case PANEL_APPLET_ORIENT_DOWN:
    default:
      new_orient = GTK_ORIENTATION_HORIZONTAL;
      break;
    }
  
  if (new_orient == tray->orientation)
    return;
  
  tray->orientation = new_orient;

  update_size_and_orientation (tray);
}

static void
applet_change_background (PanelApplet               *applet,
			  PanelAppletBackgroundType  type,
			  GdkColor                  *color,
			  const gchar               *pixmap,
			  SystemTray                *tray)
{
  if (type == PANEL_NO_BACKGROUND)
    {
      GtkRcStyle *rc_style = gtk_rc_style_new ();
      
      gtk_widget_modify_style (GTK_WIDGET (tray->applet), rc_style);

      g_object_unref (G_OBJECT (rc_style));
    }
  else if (type == PANEL_COLOR_BACKGROUND)
    {
      gtk_widget_modify_bg (GTK_WIDGET (tray->applet),
                            GTK_STATE_NORMAL,
                            color);
    }
  else
    { /* pixmap */
      /* FIXME: Handle this when the panel support works again */
    }
}

static void
applet_change_pixel_size (PanelApplet  *applet,
			  gint          size,
			  SystemTray   *tray)
{
  if (tray->size == size)
    return;
  
  tray->size = size;
  update_size_and_orientation (tray);
}

static void
free_tray (SystemTray *tray)
{
  all_trays = g_slist_remove (all_trays, tray);

  if (all_trays == NULL)
    {
      /* Make sure we drop the manager selection */
      g_object_unref (G_OBJECT (tray_manager));
      tray_manager = NULL;
      fixed_tip_hide ();
    }
  
  g_free (tray);
}

static gboolean
applet_factory (PanelApplet *applet,
                const gchar *iid,
                gpointer     data)
{
  SystemTray *tray;
  
  if (!(strcmp (iid, "OAFIID:GNOME_NotificationAreaApplet") == 0 ||
        strcmp (iid, "OAFIID:GNOME_SystemTrayApplet") == 0))
    return FALSE;

  if (tray_manager == NULL)
    {
      tray_manager = egg_tray_manager_new ();

      if (!egg_tray_manager_manage (tray_manager))
        g_printerr ("System tray didn't get the system tray manager selection\n");

      g_signal_connect (tray_manager, "tray_icon_added",
                        G_CALLBACK (tray_added), NULL);
      g_signal_connect (tray_manager, "tray_icon_removed",
                        G_CALLBACK (tray_removed), NULL);
      g_signal_connect (tray_manager, "message_sent",
                        G_CALLBACK (message_sent), NULL);
      g_signal_connect (tray_manager, "message_cancelled",
                        G_CALLBACK (message_cancelled), NULL);
    }
      
  tray = g_new0 (SystemTray, 1);

  tray->applet = applet;
  g_object_set_data_full (G_OBJECT (tray->applet),
                          "system-tray",
                          tray,
                          (GDestroyNotify) free_tray);

  tray->frame = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
  tray->box = gtk_obox_new ();
  gtk_box_set_spacing (GTK_BOX (tray->box), 1);

  gtk_container_add (GTK_CONTAINER (tray->frame), tray->box);

  tray->size = panel_applet_get_size (applet);

  switch (panel_applet_get_orient (applet))
    {
    case PANEL_APPLET_ORIENT_LEFT:
    case PANEL_APPLET_ORIENT_RIGHT:
      tray->orientation = GTK_ORIENTATION_VERTICAL;
      break;
    case PANEL_APPLET_ORIENT_UP:
    case PANEL_APPLET_ORIENT_DOWN:
    default:
      tray->orientation = GTK_ORIENTATION_HORIZONTAL;
      break;
    }
  
  all_trays = g_slist_append (all_trays, tray);
  
  panel_applet_set_flags (PANEL_APPLET (tray->applet),
                          PANEL_APPLET_HAS_HANDLE);

  g_signal_connect (G_OBJECT (tray->applet),
                    "change_size",
                    G_CALLBACK (applet_change_pixel_size),
                    tray);

  g_signal_connect (G_OBJECT (tray->applet),
                    "change_orient",
                    G_CALLBACK (applet_change_orientation),
                    tray);

  g_signal_connect (G_OBJECT (tray->applet),
                    "change_background",
                    G_CALLBACK (applet_change_background),
                    tray);

  update_size_and_orientation (tray);
  
  gtk_container_add (GTK_CONTAINER (tray->applet), tray->frame);
  
  gtk_widget_show_all (GTK_WIDGET (tray->applet));
  
  panel_applet_setup_menu_from_file (PANEL_APPLET (applet), 
  			             NULL,
                                     "GNOME_NotificationAreaApplet.xml",
                                     NULL,
                                     menu_verbs,
                                     tray);
  
  return TRUE;
}

#if 1
PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_NotificationAreaApplet_Factory",
			     PANEL_TYPE_APPLET,
                             "NotificationArea",
                             "0",
                             applet_factory,
                             NULL)

#else

PANEL_APPLET_BONOBO_SHLIB_FACTORY ("OAFIID:GNOME_NotificationAreaApplet_Factory",
				   PANEL_TYPE_APPLET,
				   "NotificationArea",
				    applet_factory, NULL);
#endif
