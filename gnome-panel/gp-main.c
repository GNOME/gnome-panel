/*
 * Copyright (C) 2019 Alberts Muktupāvels
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

#include <glib-unix.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "gp-application.h"
#include "gp-session.h"
#include "panel-icon-names.h"

typedef struct
{
  GMainLoop     *loop;
  GpApplication *application;
  int            exit_status;
} GpMainData;

static gboolean replace = FALSE;
static gboolean version = FALSE;

static GOptionEntry entries[] =
{
  {
    "replace", 'r', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, &replace,
    N_("Replace a currently running panel"),
    NULL
  },
  {
    "version", 'v', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, &version,
    N_("Print version"),
    NULL
  },
  {
    NULL
  }
};

static gboolean
parse_arguments (int    *argc,
                 char ***argv)
{
  GOptionContext *context;
  GOptionGroup *gtk_group;
  GError *error;

  context = g_option_context_new (NULL);
  gtk_group = gtk_get_option_group (FALSE);

  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gtk_group);

  error = NULL;
  if (g_option_context_parse (context, argc, argv, &error) == FALSE)
    {
      g_warning ("Failed to parse command line arguments: %s", error->message);
      g_error_free (error);

      g_option_context_free (context);

      return FALSE;
    }

  g_option_context_free (context);

  return TRUE;
}

static void
main_loop_quit (GpMainData *main_data)
{
  g_clear_object (&main_data->application);
  g_main_loop_quit (main_data->loop);
}

static gboolean
on_term_signal (gpointer user_data)
{
  main_loop_quit (user_data);

  return G_SOURCE_REMOVE;
}

static gboolean
on_int_signal (gpointer user_data)
{
  main_loop_quit (user_data);

  return G_SOURCE_REMOVE;
}

static void
name_lost_cb (GpSession  *session,
              gboolean    was_acquired,
              GpMainData *main_data)
{
  main_loop_quit (main_data);

  if (was_acquired)
    return;

  main_data->exit_status = EXIT_FAILURE;

  g_warning ("Failed to acquire bus name!");
}

static void
session_ready_cb (GpSession  *session,
                  GpMainData *main_data)
{
  GError *error;

  g_unix_signal_add (SIGTERM, on_term_signal, main_data);
  g_unix_signal_add (SIGINT, on_int_signal, main_data);

  g_set_application_name (_("Panel"));
  gtk_window_set_default_icon_name (PANEL_ICON_PANEL);

  error = NULL;
  main_data->application = gp_application_new (&error);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      main_loop_quit (main_data);

      main_data->exit_status = EXIT_FAILURE;
      return;
    }

  gp_session_register (session);
}

static void
end_session_cb (GpSession  *session,
                GpMainData *main_data)
{
  main_loop_quit (main_data);
}

int
main (int argc, char *argv[])
{
  const char *autostart_id;
  GpMainData main_data;
  GpSession *session;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gtk_init (&argc, &argv);

  if (!parse_arguments (&argc, &argv))
    return EXIT_FAILURE;

  if (version)
    {
      g_print (PACKAGE_STRING "\n");

      return EXIT_SUCCESS;
    }

  main_data.exit_status = EXIT_SUCCESS;
  main_data.loop = g_main_loop_new (NULL, FALSE);
  main_data.application = NULL;

  autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
  session = gp_session_new (replace, autostart_id != NULL ? autostart_id : "");
  g_unsetenv ("DESKTOP_AUTOSTART_ID");

  g_unsetenv ("GNOME_TERMINAL_SCREEN");
  g_unsetenv ("GNOME_TERMINAL_SERVICE");

  g_signal_connect (session,
                    "name-lost",
                    G_CALLBACK (name_lost_cb),
                    &main_data);

  g_signal_connect (session,
                    "session-ready",
                    G_CALLBACK (session_ready_cb),
                    &main_data);

  g_signal_connect (session,
                    "end-session",
                    G_CALLBACK (end_session_cb),
                    &main_data);

  g_main_loop_run (main_data.loop);

  g_main_loop_unref (main_data.loop);
  g_object_unref (session);

  return main_data.exit_status;
}
