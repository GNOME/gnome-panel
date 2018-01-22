/*
 * Copyright (C) 2018 Alberts Muktupāvels
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

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "gp-menu-utils.h"

static void
pid_cb (GDesktopAppInfo *info,
        GPid             pid,
        gpointer         user_data)
{
  g_child_watch_add (pid, (GChildWatchFunc) g_spawn_close_pid, NULL);
}

void
gp_menu_launch_app_info (GDesktopAppInfo *app_info)
{
  GSpawnFlags flags;
  GError *error;
  gboolean ret;

  flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;
  error = NULL;

  ret = g_desktop_app_info_launch_uris_as_manager (app_info, NULL, NULL,
                                                   flags, NULL, NULL,
                                                   pid_cb, NULL,
                                                   &error);

  if (ret == FALSE)
    {
      const gchar *display_name;
      GtkWidget *dialog;

      display_name = g_app_info_get_display_name (G_APP_INFO (app_info));
      dialog = gtk_message_dialog_new (NULL, 0,
                                       GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                       _("Could not launch '%s'"),
                                       display_name);

      if (error != NULL)
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  "%s", error->message);

      g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_window_present (GTK_WINDOW (dialog));
    }

  g_clear_error (&error);
}
