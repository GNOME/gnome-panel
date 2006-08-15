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
#include "fixedtip.h"
#include "obox.h"

#define NOTIFICATION_AREA_ICON "gnome-panel-notification-area"

typedef struct
{
  NaTrayManager *tray_manager;
  GSList        *all_trays;
  GHashTable    *icon_table;
  GHashTable    *tip_table;
} TraysScreen;

typedef struct
{
  PanelApplet *applet;

  TraysScreen *trays_screen;

  GtkWidget *box;
  GtkWidget *about_dialog;
  GtkWidget *frame;

  GtkOrientation orientation;
} SystemTray;

typedef struct
{
  char  *text;
  glong  id;
  glong  timeout;
} IconTipBuffer;

typedef struct
{
  SystemTray *tray;      /* tray containing the tray icon */
  GtkWidget  *icon;      /* tray icon sending the message */
  GtkWidget  *fixedtip;
  guint       source_id;
  glong       id;        /* id of the current message */
  GSList     *buffer;    /* buffered messages */
} IconTip;


static gboolean     initialized   = FALSE;
static int          screens       = 0;
static TraysScreen *trays_screens = NULL;

static void icon_tip_show_next (IconTip *icontip);

static void
help_cb (BonoboUIComponent *uic,
	 SystemTray        *tray,	  
	 const gchar       *verbname)   
{
  GError *err;

  err = NULL;  
  gnome_help_display_desktop_on_screen (NULL, "user-guide",
                                        "user-guide.xml", "gospanel-567",
					gtk_widget_get_screen (GTK_WIDGET (tray->applet)),
					&err);

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
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);
      
      gtk_window_set_icon_name (GTK_WINDOW (dialog), NOTIFICATION_AREA_ICON);
      gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
      gtk_window_set_screen (GTK_WINDOW (dialog),
                             gtk_widget_get_screen (GTK_WIDGET (tray->applet)));
      
      gtk_widget_show (dialog);

      g_error_free (err);
    }
}


static void
about_cb (BonoboUIComponent *uic,
          SystemTray        *tray,
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

  screen = gtk_widget_get_screen (GTK_WIDGET (tray->applet));

  if (tray->about_dialog)
    {
      gtk_window_set_screen (GTK_WINDOW (tray->about_dialog), screen);
      gtk_window_present (GTK_WINDOW (tray->about_dialog));
      return;
    }

  tray->about_dialog = gtk_about_dialog_new ();
  g_object_set (tray->about_dialog,
                "name", _("Notification Area"),
                "version", VERSION,
                "copyright", "Copyright \xc2\xa9 2002 Red Hat, Inc.",
                "authors", (const char **) authors,
                "documenters", (const char **) documenters,
                "translator-credits", _("translator-credits"),
                "logo-icon-name", NOTIFICATION_AREA_ICON,
                NULL);
  
  gtk_window_set_icon_name (GTK_WINDOW (tray->about_dialog),
                            NOTIFICATION_AREA_ICON);
  gtk_window_set_screen (GTK_WINDOW (tray->about_dialog), screen);

  g_object_add_weak_pointer (G_OBJECT (tray->about_dialog),
                             (gpointer) &tray->about_dialog);

  g_signal_connect (tray->about_dialog, "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);

  gtk_widget_show (tray->about_dialog);
}

static const BonoboUIVerb menu_verbs [] = {
  BONOBO_UI_UNSAFE_VERB ("SystemTrayHelp",       help_cb),
  BONOBO_UI_UNSAFE_VERB ("SystemTrayAbout",      about_cb),
  BONOBO_UI_VERB_END
};

static SystemTray *
get_tray (TraysScreen *trays_screen)
{
  if (trays_screen->all_trays == NULL)
    return NULL;
  
  return trays_screen->all_trays->data;
}

static void
force_redraw (SystemTray *tray)
{
  /* Force the icons to redraw their backgrounds.
   * gtk_widget_queue_draw() doesn't work across process boundaries,
   * so we do this instead.
   */
  gtk_widget_hide (tray->box);
  gtk_widget_show (tray->box);
}

static void
tray_added (NaTrayManager *manager,
            GtkWidget     *icon,
            TraysScreen   *trays_screen)
{
  SystemTray *tray;

  tray = get_tray (trays_screen);
  if (tray == NULL)
    return;

  g_hash_table_insert (tray->trays_screen->icon_table, icon, tray);

  gtk_box_pack_end (GTK_BOX (tray->box), icon, FALSE, FALSE, 0);
  
  gtk_widget_show (icon);
  force_redraw (tray);
}

static void
tray_removed (NaTrayManager *manager,
              GtkWidget     *icon,
              TraysScreen   *trays_screen)
{
  SystemTray *tray;

  tray = g_hash_table_lookup (trays_screen->icon_table, icon);
  if (tray == NULL)
    return;

  force_redraw (tray);

  g_hash_table_remove (trays_screen->icon_table, icon);
  /* this will also destroy the tip associated to this icon */
  g_hash_table_remove (trays_screen->tip_table, icon);
}

static void
icon_tip_buffer_free (gpointer data,
                      gpointer userdata)
{
  IconTipBuffer *buffer;

  buffer = data;

  g_free (buffer->text);
  buffer->text = NULL;

  g_free (buffer);
}

static void
icon_tip_free (gpointer data)
{
  IconTip *icontip;

  if (data == NULL)
    return;

  icontip = data;

  if (icontip->fixedtip != NULL)
    gtk_widget_destroy (GTK_WIDGET (icontip->fixedtip));
  icontip->fixedtip = NULL;

  if (icontip->source_id != 0)
    g_source_remove (icontip->source_id);
  icontip->source_id = 0;

  if (icontip->buffer != NULL)
    {
      g_slist_foreach (icontip->buffer, icon_tip_buffer_free, NULL);
      g_slist_free (icontip->buffer);
    }
  icontip->buffer = NULL;

  g_free (icontip);
}

static int
icon_tip_buffer_compare (gconstpointer a,
                         gconstpointer b)
{
  IconTipBuffer *buffer_a;
  IconTipBuffer *buffer_b;

  if (buffer_a == NULL || buffer_b == NULL)
    return !(buffer_a == buffer_b);

  return buffer_a->id - buffer_b->id;
}

static void
icon_tip_show_next_clicked (GtkWidget *widget,
                            gpointer   data)
{
  icon_tip_show_next ((IconTip *) data);
}

static gboolean
icon_tip_show_next_timeout (gpointer data)
{
  icon_tip_show_next ((IconTip *) data);

  return FALSE;
}

static void
icon_tip_show_next (IconTip *icontip)
{
  IconTipBuffer *buffer;

  if (icontip->buffer == NULL)
    {
      /* this will also destroy the tip window */
      g_hash_table_remove (icontip->tray->trays_screen->tip_table,
                           icontip->icon);
      return;
    }

  if (icontip->source_id != 0)
    g_source_remove (icontip->source_id);
  icontip->source_id = 0;

  buffer = icontip->buffer->data;
  icontip->buffer = g_slist_remove (icontip->buffer, buffer);

  if (icontip->fixedtip == NULL)
    {
      icontip->fixedtip = na_fixed_tip_new (icontip->icon,
                                            icontip->tray->orientation);

      g_signal_connect (icontip->fixedtip, "clicked",
                        G_CALLBACK (icon_tip_show_next_clicked), icontip);
    }

  na_fixed_tip_set_markup (icontip->fixedtip, buffer->text);

  if (!GTK_WIDGET_MAPPED (icontip->fixedtip))
    gtk_widget_show (icontip->fixedtip);

  icontip->id = buffer->id;

  if (buffer->timeout > 0)
    icontip->source_id = g_timeout_add (buffer->timeout * 1000,
                                        icon_tip_show_next_timeout, icontip);

  icon_tip_buffer_free (buffer, NULL);
}

static void
message_sent (NaTrayManager *manager,
              GtkWidget     *icon,
              const char    *text,
              glong          id,
              glong          timeout,
              TraysScreen   *trays_screen)
{
  IconTip       *icontip;
  IconTipBuffer  find_buffer;
  IconTipBuffer *buffer;
  gboolean       show_now;

  icontip = g_hash_table_lookup (trays_screen->tip_table, icon);

  find_buffer.id = id;
  if (icontip && 
      (icontip->id == id ||
       g_slist_find_custom (icontip->buffer, &find_buffer,
                            icon_tip_buffer_compare) != NULL))
    /* we already have this message, so ignore it */
    /* FIXME: in an ideal world, we'd remember all the past ids and ignore them
     * too */
    return;

  show_now = FALSE;

  if (icontip == NULL)
    {
      SystemTray *tray;

      tray = g_hash_table_lookup (trays_screen->icon_table, icon);
      if (tray == NULL)
        {
          /* We don't know about the icon sending the message, so ignore it.
           * But this should never happen since NaTrayManager shouldn't send
           * us the message if there's no socket for it. */
          g_critical ("Ignoring a message sent by a tray icon "
                      "we don't know: \"%s\".\n", text);
          return;
        }

      icontip = g_new0 (IconTip, 1);
      icontip->tray = tray;
      icontip->icon = icon;

      g_hash_table_insert (trays_screen->tip_table, icon, icontip);

      show_now = TRUE;
    }

  buffer = g_new0 (IconTipBuffer, 1);

  buffer->text    = g_strdup (text);
  buffer->id      = id;
  buffer->timeout = timeout;

  icontip->buffer = g_slist_append (icontip->buffer, buffer);

  if (show_now)
    icon_tip_show_next (icontip);
}

static void
message_cancelled (NaTrayManager *manager,
                   GtkWidget     *icon,
                   glong          id,
                   TraysScreen   *trays_screen)
{
  IconTip       *icontip;
  IconTipBuffer  find_buffer;
  GSList        *cancel_buffer_l;
  IconTipBuffer *cancel_buffer;

  icontip = g_hash_table_lookup (trays_screen->tip_table, icon);
  if (icontip == NULL)
    return;

  if (icontip->id == id)
    {
      icon_tip_show_next (icontip);
      return;
    }

  find_buffer.id = id;
  cancel_buffer_l = g_slist_find_custom (icontip->buffer, &find_buffer,
                                         icon_tip_buffer_compare);
  if (cancel_buffer_l == NULL)
    return;

  cancel_buffer = cancel_buffer_l->data;
  icon_tip_buffer_free (cancel_buffer, NULL);

  icontip->buffer = g_slist_remove_link (icontip->buffer, cancel_buffer_l);
  g_slist_free_1 (cancel_buffer_l);
}

static void
update_orientation_for_messages (gpointer key,
                                 gpointer value,
                                 gpointer data)
{
  SystemTray *tray;
  IconTip    *icontip;

  if (value == NULL)
    return;

  icontip = value;
  tray    = data;
  if (icontip->tray != tray)
    return;

  if (icontip->fixedtip)
    na_fixed_tip_set_orientation (icontip->fixedtip, tray->orientation);
}

static void
update_size_and_orientation (SystemTray *tray)
{
  na_obox_set_orientation (NA_OBOX (tray->box), tray->orientation);
  g_hash_table_foreach (tray->trays_screen->tip_table,
                        update_orientation_for_messages, tray);

  if (get_tray (tray->trays_screen) == tray)
    na_tray_manager_set_orientation (tray->trays_screen->tray_manager,
                                     tray->orientation);

  /* note, you want this larger if the frame has non-NONE relief by default. */
#define MIN_BOX_SIZE 3
  switch (tray->orientation)
    {
    case GTK_ORIENTATION_VERTICAL:
      /* Give box a min size so the frame doesn't look dumb */
      gtk_widget_set_size_request (tray->box, MIN_BOX_SIZE, -1);
      break;
    case GTK_ORIENTATION_HORIZONTAL:
      gtk_widget_set_size_request (tray->box, -1, MIN_BOX_SIZE);
      break;
    }

  force_redraw (tray);
}

static void
applet_change_background (PanelApplet               *applet,
                          PanelAppletBackgroundType  type,
                          GdkColor                  *color,
                          GdkPixmap                 *pixmap,
                          SystemTray                *tray)
{
  force_redraw (tray);
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
applet_destroy (PanelApplet *applet,
		SystemTray  *tray)
{
  if (tray->about_dialog)
    gtk_widget_destroy (tray->about_dialog);
}

static void
free_tray (SystemTray *tray)
{
  tray->trays_screen->all_trays = g_slist_remove (tray->trays_screen->all_trays,
                                                  tray);

  if (tray->trays_screen->all_trays == NULL)
    {
      /* Make sure we drop the manager selection */
      g_object_unref (G_OBJECT (tray->trays_screen->tray_manager));
      tray->trays_screen->tray_manager = NULL;

      g_hash_table_destroy (tray->trays_screen->icon_table);
      tray->trays_screen->icon_table = NULL;

      g_hash_table_destroy (tray->trays_screen->tip_table);
      tray->trays_screen->tip_table = NULL;
    }
  else
    {
      SystemTray *new_tray;

      new_tray = get_tray (tray->trays_screen);
      if (new_tray != NULL)
        na_tray_manager_set_orientation (tray->trays_screen->tray_manager,
                                         new_tray->orientation);
    }

  g_free (tray);
}

static gboolean
applet_factory (PanelApplet *applet,
                const gchar *iid,
                gpointer     data)
{
  GdkScreen  *screen;
  SystemTray *tray;
  AtkObject  *atko;
  int         screen_number;
  
  if (!(strcmp (iid, "OAFIID:GNOME_NotificationAreaApplet") == 0 ||
        strcmp (iid, "OAFIID:GNOME_SystemTrayApplet") == 0))
    return FALSE;

  if (!initialized)
    {
      GdkDisplay *display;

      display = gdk_display_get_default ();
      screens = gdk_display_get_n_screens (display);
      trays_screens = g_new0 (TraysScreen, screens);
      initialized = TRUE;
    }

  screen = gtk_widget_get_screen (GTK_WIDGET (applet));
  screen_number = gdk_screen_get_number (screen);

  if (trays_screens [screen_number].tray_manager == NULL)
    {
      NaTrayManager *tray_manager;

      tray_manager = na_tray_manager_new ();

      if (na_tray_manager_manage_screen (tray_manager, screen))
        {
          trays_screens [screen_number].tray_manager = tray_manager;

          g_signal_connect (tray_manager, "tray_icon_added",
                            G_CALLBACK (tray_added),
                            &trays_screens [screen_number]);
          g_signal_connect (tray_manager, "tray_icon_removed",
                            G_CALLBACK (tray_removed),
                            &trays_screens [screen_number]);
          g_signal_connect (tray_manager, "message_sent",
                            G_CALLBACK (message_sent),
                            &trays_screens [screen_number]);
          g_signal_connect (tray_manager, "message_cancelled",
                            G_CALLBACK (message_cancelled),
                            &trays_screens [screen_number]);

          trays_screens [screen_number].icon_table = g_hash_table_new (NULL,
                                                                       NULL);
          trays_screens [screen_number].tip_table = g_hash_table_new_full (
                                                                NULL,
                                                                NULL,
                                                                NULL,
                                                                icon_tip_free);
        }
      else
        {
          g_printerr ("System tray didn't get the system tray manager selection\n");
          g_object_unref (tray_manager);
        }
    }
      
  tray = g_new0 (SystemTray, 1);

  tray->trays_screen = &trays_screens [screen_number];

  tray->applet = applet;
  g_object_set_data_full (G_OBJECT (tray->applet),
                          "system-tray",
                          tray,
                          (GDestroyNotify) free_tray);

  atko = gtk_widget_get_accessible (GTK_WIDGET (tray->applet));
  atk_object_set_name (atko, _("Panel Notification Area"));

  panel_applet_set_flags (PANEL_APPLET (tray->applet),
                          PANEL_APPLET_HAS_HANDLE|PANEL_APPLET_EXPAND_MINOR);
  
  tray->frame = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
  tray->box = na_obox_new ();
  gtk_box_set_spacing (GTK_BOX (tray->box), 1);

  gtk_container_add (GTK_CONTAINER (tray->frame), tray->box);

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
  
  trays_screens [screen_number].all_trays = g_slist_append (trays_screens [screen_number].all_trays,
                                                            tray);
  
  g_signal_connect (G_OBJECT (tray->applet),
                    "change_orient",
                    G_CALLBACK (applet_change_orientation),
                    tray);

  g_signal_connect (G_OBJECT (tray->applet),
                    "change_background",
                    G_CALLBACK (applet_change_background),
                    tray);

  g_signal_connect (tray->applet,
                    "destroy",
                    G_CALLBACK (applet_destroy),
                    tray);

  panel_applet_set_background_widget (tray->applet, GTK_WIDGET (tray->applet));

  update_size_and_orientation (tray);
  
  gtk_container_add (GTK_CONTAINER (tray->applet), tray->frame);
  
#ifndef NOTIFICATION_AREA_INPROCESS
  gtk_window_set_default_icon_name (NOTIFICATION_AREA_ICON);
#endif
  gtk_widget_show_all (GTK_WIDGET (tray->applet));
  
  panel_applet_setup_menu_from_file (PANEL_APPLET (applet), 
  			             NULL,
                                     "GNOME_NotificationAreaApplet.xml",
                                     NULL,
                                     menu_verbs,
                                     tray);
  
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
