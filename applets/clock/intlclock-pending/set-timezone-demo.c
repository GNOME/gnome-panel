/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include <gtk/gtk.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

static DBusGConnection *
get_session_bus (void)
{
        GError          *error;
        DBusGConnection *bus;

        error = NULL;
        bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
        if (bus == NULL) {
                g_warning ("Couldn't connect to session bus: %s", error->message);
                g_error_free (error);
                goto out;
        }
out:
        return bus;
}

static DBusGConnection *
get_system_bus (void)
{
        GError          *error;
        DBusGConnection *bus;

        error = NULL;
        bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
        if (bus == NULL) {
                g_warning ("Couldn't connect to system bus: %s", error->message);
                g_error_free (error);
                goto out;
        }
out:
        return bus;
}

int 
main (int argc, char *argv[])
{
        DBusGConnection *session_bus;
        DBusGConnection *system_bus;
        int              ret;
        DBusError        dbus_error;
        GtkWidget       *dialog;
        int              response;
        char            *filename;
        DBusGProxy      *mechanism_proxy;
        DBusGProxy      *polkit_gnome_proxy;

        ret = 1;
        filename = NULL;

        if (!g_thread_supported ()) {
                g_thread_init (NULL);
        }
        dbus_g_thread_init ();
        g_type_init ();
        gtk_init (&argc, &argv);

        session_bus = get_session_bus ();
        if (session_bus == NULL) {
                goto out;
        }

        system_bus = get_system_bus ();
        if (system_bus == NULL) {
                goto out;
        }

	mechanism_proxy = dbus_g_proxy_new_for_name (system_bus,
                                                     "org.gnome.ClockApplet.Mechanism",
                                                     "/",
                                                     "org.gnome.ClockApplet.Mechanism");

	polkit_gnome_proxy = dbus_g_proxy_new_for_name (session_bus,
                                                        "org.gnome.PolicyKit",
                                                        "/org/gnome/PolicyKit/Manager",
                                                        "org.gnome.PolicyKit.Manager");


        dialog = gtk_file_chooser_dialog_new ("Select zonefile",
                                              NULL,
                                              GTK_FILE_CHOOSER_ACTION_OPEN,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                              GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                              NULL);
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), "/usr/share/zoneinfo");
        response = gtk_dialog_run (GTK_DIALOG (dialog));
        if (response == GTK_RESPONSE_ACCEPT) {
                filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        }
        gtk_widget_destroy (dialog);

        if (filename != NULL) {
                GError *error;

                g_debug ("User chose '%s'", filename);

        try_again:
                error = NULL;
                /* first, try to call into the mechanism */
                if (!dbus_g_proxy_call_with_timeout (mechanism_proxy,
                                                     "SetTimezone",
                                                     INT_MAX,
                                                     &error,
                                                     /* parameters: */
                                                     G_TYPE_STRING, filename,
                                                     G_TYPE_INVALID,
                                                     /* return values: */
                                                     G_TYPE_INVALID)) {
                        if (dbus_g_error_has_name (error, "org.gnome.ClockApplet.Mechanism.NotPrivileged")) {
                                char **tokens;
                                char *polkit_result_textual;
                                char *polkit_action;
                                gboolean gained_privilege;

                                tokens = g_strsplit (error->message, " ", 2);
                                g_error_free (error);                                
                                if (g_strv_length (tokens) != 2) {
                                        g_warning ("helper return string malformed");
                                        g_strfreev (tokens);
                                        goto out;
                                }
                                polkit_action = tokens[0];
                                polkit_result_textual = tokens[1];

                                g_debug ("helper refused; returned polkit_result='%s' and polkit_action='%s'", 
                                         polkit_result_textual, polkit_action);

                                /* Now ask the user for auth... */
                                if (!dbus_g_proxy_call_with_timeout (polkit_gnome_proxy,
                                                                     "ShowDialog",
                                                                     INT_MAX,
                                                                     &error,
                                                                     /* parameters: */
                                                                     G_TYPE_STRING, polkit_action,
                                                                     G_TYPE_UINT, 0, /* X11 window ID; none */
                                                                     G_TYPE_INVALID,
                                                                     /* return values: */
                                                                     G_TYPE_BOOLEAN, &gained_privilege,
                                                                     G_TYPE_INVALID)) {
                                        g_warning ("Caught exception '%s'", error->message);
                                        g_error_free (error);
                                        g_strfreev (tokens);
                                        goto out;
                                }
                                g_strfreev (tokens);

                                if (gained_privilege) {
                                        g_debug ("Gained privilege; trying to set timezone again");
                                        goto try_again;
                                }

                        } else {
                                g_warning ("Caught exception %s '%s'", dbus_g_error_get_name (error), error->message);
                                g_error_free (error);
                        }
                        goto out;
                }

                g_debug ("Succesfully set time zone to '%s'", filename);
                g_free (filename);
        }

        ret = 0;
out:
        return ret;
}
