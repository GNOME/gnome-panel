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
#include <errno.h>
#include <sys/time.h>

#include <glib.h>
#include <glib-object.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <polkit-dbus/polkit-dbus.h>

#include "gnome-clock-applet-mechanism.h"
#include "gnome-clock-applet-mechanism-glue.h"

static gboolean
do_exit (gpointer user_data)
{
        g_debug ("Exiting due to inactivity");
        exit (1);
        return FALSE;
}

static void
reset_killtimer (void)
{
        static guint timer_id = 0;

        if (timer_id > 0) {
                g_source_remove (timer_id);
        }
        g_debug ("Setting killtimer to 30 seconds...");
        timer_id = g_timeout_add (30 * 1000, do_exit, NULL);
}

struct GnomeClockAppletMechanismPrivate
{
        DBusGConnection *system_bus_connection;
        DBusGProxy      *system_bus_proxy;
        PolKitContext   *pol_ctx;
};

static void     gnome_clock_applet_mechanism_class_init  (GnomeClockAppletMechanismClass *klass);
static void     gnome_clock_applet_mechanism_init        (GnomeClockAppletMechanism      *seat);
static void     gnome_clock_applet_mechanism_finalize    (GObject     *object);

G_DEFINE_TYPE (GnomeClockAppletMechanism, gnome_clock_applet_mechanism, G_TYPE_OBJECT)

#define GNOME_CLOCK_APPLET_MECHANISM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_CLOCK_APPLET_TYPE_MECHANISM, GnomeClockAppletMechanismPrivate))

GQuark
gnome_clock_applet_mechanism_error_quark (void)
{
        static GQuark ret = 0;

        if (ret == 0) {
                ret = g_quark_from_static_string ("gnome_clock_applet_mechanism_error");
        }

        return ret;
}


#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

GType
gnome_clock_applet_mechanism_error_get_type (void)
{
        static GType etype = 0;
        
        if (etype == 0)
        {
                static const GEnumValue values[] =
                        {
                                ENUM_ENTRY (GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL, "GeneralError"),
                                ENUM_ENTRY (GNOME_CLOCK_APPLET_MECHANISM_ERROR_NOT_PRIVILEGED, "NotPrivileged"),
                                ENUM_ENTRY (GNOME_CLOCK_APPLET_MECHANISM_ERROR_INVALID_TIMEZONE_FILE, "InvalidTimezoneFile"),
                                { 0, 0, 0 }
                        };
                
                g_assert (GNOME_CLOCK_APPLET_MECHANISM_NUM_ERRORS == G_N_ELEMENTS (values) - 1);
                
                etype = g_enum_register_static ("GnomeClockAppletMechanismError", values);
        }
        
        return etype;
}


static GObject *
gnome_clock_applet_mechanism_constructor (GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
        GnomeClockAppletMechanism      *mechanism;
        GnomeClockAppletMechanismClass *klass;

        klass = GNOME_CLOCK_APPLET_MECHANISM_CLASS (g_type_class_peek (GNOME_CLOCK_APPLET_TYPE_MECHANISM));

        mechanism = GNOME_CLOCK_APPLET_MECHANISM (G_OBJECT_CLASS (gnome_clock_applet_mechanism_parent_class)->constructor (
                                                type,
                                                n_construct_properties,
                                                construct_properties));

        return G_OBJECT (mechanism);
}

static void
gnome_clock_applet_mechanism_class_init (GnomeClockAppletMechanismClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = gnome_clock_applet_mechanism_constructor;
        object_class->finalize = gnome_clock_applet_mechanism_finalize;

        g_type_class_add_private (klass, sizeof (GnomeClockAppletMechanismPrivate));

        dbus_g_object_type_install_info (GNOME_CLOCK_APPLET_TYPE_MECHANISM, &dbus_glib_gnome_clock_applet_mechanism_object_info);

        dbus_g_error_domain_register (GNOME_CLOCK_APPLET_MECHANISM_ERROR, NULL, GNOME_CLOCK_APPLET_MECHANISM_TYPE_ERROR);

}

static void
gnome_clock_applet_mechanism_init (GnomeClockAppletMechanism *mechanism)
{
        mechanism->priv = GNOME_CLOCK_APPLET_MECHANISM_GET_PRIVATE (mechanism);

}

static void
gnome_clock_applet_mechanism_finalize (GObject *object)
{
        GnomeClockAppletMechanism *mechanism;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GNOME_CLOCK_APPLET_IS_MECHANISM (object));

        mechanism = GNOME_CLOCK_APPLET_MECHANISM (object);

        g_return_if_fail (mechanism->priv != NULL);

        g_object_unref (mechanism->priv->system_bus_proxy);

        G_OBJECT_CLASS (gnome_clock_applet_mechanism_parent_class)->finalize (object);
}

static gboolean
pk_io_watch_have_data (GIOChannel *channel, GIOCondition condition, gpointer user_data)
{
        int fd;
        PolKitContext *pk_context = user_data;
        fd = g_io_channel_unix_get_fd (channel);
        polkit_context_io_func (pk_context, fd);
        return TRUE;
}

static int 
pk_io_add_watch (PolKitContext *pk_context, int fd)
{
        guint id = 0;
        GIOChannel *channel;
        channel = g_io_channel_unix_new (fd);
        if (channel == NULL)
                goto out;
        id = g_io_add_watch (channel, G_IO_IN, pk_io_watch_have_data, pk_context);
        if (id == 0) {
                g_io_channel_unref (channel);
                goto out;
        }
        g_io_channel_unref (channel);
out:
        return id;
}

static void 
pk_io_remove_watch (PolKitContext *pk_context, int watch_id)
{
        g_source_remove (watch_id);
}

static gboolean
register_mechanism (GnomeClockAppletMechanism *mechanism)
{
        GError *error = NULL;

        mechanism->priv->pol_ctx = polkit_context_new ();
        polkit_context_set_io_watch_functions (mechanism->priv->pol_ctx, pk_io_add_watch, pk_io_remove_watch);
        if (!polkit_context_init (mechanism->priv->pol_ctx, NULL)) {
                g_critical ("cannot initialize libpolkit");
                goto error;
        }

        error = NULL;
        mechanism->priv->system_bus_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
        if (mechanism->priv->system_bus_connection == NULL) {
                if (error != NULL) {
                        g_critical ("error getting system bus: %s", error->message);
                        g_error_free (error);
                }
                goto error;
        }

        dbus_g_connection_register_g_object (mechanism->priv->system_bus_connection, "/", 
                                             G_OBJECT (mechanism));

        mechanism->priv->system_bus_proxy = dbus_g_proxy_new_for_name (mechanism->priv->system_bus_connection,
                                                                      DBUS_SERVICE_DBUS,
                                                                      DBUS_PATH_DBUS,
                                                                      DBUS_INTERFACE_DBUS);

        reset_killtimer ();

        return TRUE;

error:
        return FALSE;
}


GnomeClockAppletMechanism *
gnome_clock_applet_mechanism_new (void)
{
        GObject *object;
        gboolean res;

        object = g_object_new (GNOME_CLOCK_APPLET_TYPE_MECHANISM, NULL);

        res = register_mechanism (GNOME_CLOCK_APPLET_MECHANISM (object));
        if (! res) {
                g_object_unref (object);
                return NULL;
        }

        return GNOME_CLOCK_APPLET_MECHANISM (object);
}

static gboolean
_check_polkit_for_action (GnomeClockAppletMechanism *mechanism, DBusGMethodInvocation *context, const char *action)
{
        const char *sender;
        GError *error;
        DBusError dbus_error;
        PolKitCaller *pk_caller;
        PolKitAction *pk_action;
        PolKitResult pk_result;

        error = NULL;

        /* Check that caller is privileged */
        sender = dbus_g_method_get_sender (context);
        dbus_error_init (&dbus_error);
        pk_caller = polkit_caller_new_from_dbus_name (
                dbus_g_connection_get_connection (mechanism->priv->system_bus_connection),
                sender, 
                &dbus_error);
        if (pk_caller == NULL) {
                error = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                     GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                     "Error getting information about caller: %s: %s",
                                     dbus_error.name, dbus_error.message);
                dbus_error_free (&dbus_error);
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                return FALSE;
        }

        pk_action = polkit_action_new ();
        polkit_action_set_action_id (pk_action, action);
        pk_result = polkit_context_can_caller_do_action (mechanism->priv->pol_ctx, pk_action, pk_caller);
        polkit_caller_unref (pk_caller);
        polkit_action_unref (pk_action);

        if (pk_result != POLKIT_RESULT_YES) {
                error = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                     GNOME_CLOCK_APPLET_MECHANISM_ERROR_NOT_PRIVILEGED,
                                     "%s %s <-- (action, result)",
                                     action,
                                     polkit_result_to_string_representation (pk_result));
                dbus_error_free (&dbus_error);
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                return FALSE;
        }

        return TRUE;
}


static gboolean
_set_time (GnomeClockAppletMechanism    *mechanism,
           const struct timeval         *tv,
           DBusGMethodInvocation        *context)
{
        GError *error;
        const char *sender;
        DBusError dbus_error;
        PolKitCaller *pk_caller;
        PolKitAction *pk_action;
        PolKitResult pk_result;

        if (!_check_polkit_for_action (mechanism, context, "org.gnome.clockapplet.mechanism.settime"))
                return FALSE;

        if (settimeofday (tv, NULL) != 0) {
                error = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                     GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                     "Error calling settimeofday({%lld,%lld}): %s", 
                                     (gint64) tv->tv_sec, (gint64) tv->tv_usec,
                                     strerror (errno));
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                return FALSE;
        }

        if (g_file_test ("/sbin/hwclock", 
                         G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_EXECUTABLE)) {
                int exit_status;
                if (!g_spawn_command_line_sync ("/sbin/hwclock --systohc", NULL, NULL, &exit_status, &error)) {
                        GError *error2;
                        error2 = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                              GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                              "Error spawning /sbin/hwclock: %s", error->message);
                        g_error_free (error);
                        dbus_g_method_return_error (context, error2);
                        g_error_free (error2);
                        return FALSE;
                }
                if (WEXITSTATUS (exit_status) != 0) {
                        error = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                             GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                             "/sbin/hwclock returned %d", exit_status);
                        dbus_g_method_return_error (context, error);
                        g_error_free (error);
                        return FALSE;
                }
        }

        dbus_g_method_return (context);
        return TRUE;
}

static gboolean
_rh_update_etc_sysconfig_clock (DBusGMethodInvocation *context, const char *key, const char *value)
{
        /* On Red Hat / Fedora, the /etc/sysconfig/clock file needs to be kept in sync */
        if (g_file_test ("/etc/sysconfig/clock", G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
                char *p;
                char **lines;
                int n;
                gboolean replaced;
                char *data;
                gsize len;
                GError *error;
                
                error = NULL;

                if (!g_file_get_contents ("/etc/sysconfig/clock", &data, &len, &error)) {
                        GError *error2;
                        error2 = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                              GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                              "Error reading /etc/sysconfig/clock file: %s", error->message);
                        g_error_free (error);
                        dbus_g_method_return_error (context, error2);
                        g_error_free (error2);
                        return FALSE;
                }
                replaced = FALSE;
                lines = g_strsplit (data, "\n", 0);
                g_free (data);

                for (n = 0; lines[n] != NULL; n++) {
                        if (g_str_has_prefix (lines[n], key)) {
                                g_free (lines[n]);
                                lines[n] = g_strdup_printf ("%s%s", key, value);
                                replaced = TRUE;
                        }
                }
                if (replaced) {
                        GString *str;

                        str = g_string_new (NULL);
                        for (n = 0; lines[n] != NULL; n++) {
                                g_string_append (str, lines[n]);
                                if (lines[n + 1] != NULL)
                                        g_string_append_c (str, '\n');
                        }
                        data = g_string_free (str, FALSE);
                        len = strlen (data);
                        if (!g_file_set_contents ("/etc/sysconfig/clock", data, len, &error)) {
                                GError *error2;
                                error2 = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                                      GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                                      "Error updating /etc/sysconfig/clock: %s", error->message);
                                g_error_free (error);
                                dbus_g_method_return_error (context, error2);
                                g_error_free (error2);
                                g_free (data);
                                return FALSE;
                        }
                        g_free (data);
                }
                g_strfreev (lines);
        }

        return TRUE;
}

/* exported methods */

gboolean
gnome_clock_applet_mechanism_set_time (GnomeClockAppletMechanism    *mechanism,
                                       gint64                        seconds_since_epoch,
                                       DBusGMethodInvocation        *context)
{
        struct timeval tv;

        reset_killtimer ();
        g_debug ("SetTime(%lld) called", seconds_since_epoch);

        tv.tv_sec = (time_t) seconds_since_epoch;
        tv.tv_usec = 0;
        return _set_time (mechanism, &tv, context);
}

gboolean
gnome_clock_applet_mechanism_adjust_time (GnomeClockAppletMechanism    *mechanism,
                                          gint64                        seconds_to_add,
                                          DBusGMethodInvocation        *context)
{
        struct timeval tv;

        reset_killtimer ();
        g_debug ("AdjustTime(%lld) called", seconds_to_add);

        if (gettimeofday (&tv, NULL) != 0) {
                GError *error;
                error = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                     GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                     "Error calling gettimeofday(): %s", strerror (errno));
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                return FALSE;
        }

        tv.tv_sec += (time_t) seconds_to_add;
        return _set_time (mechanism, &tv, context);        
}


gboolean
gnome_clock_applet_mechanism_set_timezone (GnomeClockAppletMechanism    *mechanism,
                                           const char                   *zone_file,
                                           DBusGMethodInvocation        *context)
{
        char *data;
        gsize len;
        GError *error;

        reset_killtimer ();
        g_debug ("SetTimezone('%s') called", zone_file);

        if (!_check_polkit_for_action (mechanism, context, "org.gnome.clockapplet.mechanism.settimezone"))
                return FALSE;

        /* First, check the zone_file is properly rooted (TODO: May be OS specific) */
        if (!g_str_has_prefix (zone_file, "/usr/share/zoneinfo/")) {
                error = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                     GNOME_CLOCK_APPLET_MECHANISM_ERROR_INVALID_TIMEZONE_FILE,
                                     "Timezone file needs to be somewhere under /usr/share/zoneinfo");
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                return FALSE;
        }

        /* Second, check it's a regular file that exists */
        if (!g_file_test (zone_file, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
                error = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                     GNOME_CLOCK_APPLET_MECHANISM_ERROR_INVALID_TIMEZONE_FILE,
                                     "No such timezone file %s", zone_file);
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                return FALSE;
        }

        /* Now copy the file to /etc/localtime. This is what we do on
         * Red Hat / Fedora; to handle updates of tzdata
         * /etc/sysconfig/clock has a ZONE="<timezone>" entry that is
         * used to regenerate /etc/localtime.
         *
         * TODO: Check if /etc/localtime is a symlink and write out a symlink
         *       into /usr/share/zoneinfo instead.
         */

        error = NULL;
        if (!g_file_get_contents (zone_file, &data, &len, &error)) {
                GError *error2;
                error2 = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                      GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                      "Error reading timezone file %s: %s", zone_file, error->message);
                g_error_free (error);
                dbus_g_method_return_error (context, error2);
                g_error_free (error2);
                return FALSE;
        }

        /* Verify it's a tzfile (see tzfile(5))
         *
         * TODO: is there glibc API for this? 
         */
        if (data[0] != 'T' ||
            data[1] != 'Z' ||
            data[2] != 'i' ||
            data[3] != 'f') {
                error = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                     GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                     "File %s is not a timezone file", zone_file);
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                g_free (data);
                return FALSE;
        }

        if (!g_file_set_contents ("/etc/localtime", data, len, &error)) {
                GError *error2;
                error2 = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                      GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                      "Error writing timezone data to /etc/localtime: %s", error->message);
                g_error_free (error);
                dbus_g_method_return_error (context, error2);
                g_error_free (error2);
                g_free (data);
                return FALSE;
        }
        g_free (data);

        /* OS specific bits here: */

	/* FMQ: openSUSE uses "TIMEZONE=", not "ZONE=" */
        data = g_strdup_printf ("\"%s\"", zone_file + sizeof ("/usr/share/zoneinfo"));
        if (!_rh_update_etc_sysconfig_clock (context, "ZONE=", data)) {
                g_free (data);
                return FALSE;
        }
        g_free (data);
        
        dbus_g_method_return (context);
        return TRUE;
}



gboolean
gnome_clock_applet_mechanism_get_hardware_clock_using_utc  (GnomeClockAppletMechanism    *mechanism,
                                                            DBusGMethodInvocation        *context)
{
        char **lines;
        char *data;
        gsize len;
        GError *error;
        gboolean is_utc;

        error = NULL;

        if (!g_file_get_contents ("/etc/adjtime", &data, &len, &error)) {
                GError *error2;
                error2 = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                      GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                      "Error reading /etc/adjtime file: %s", error->message);
                g_error_free (error);
                dbus_g_method_return_error (context, error2);
                g_error_free (error2);
                return FALSE;
        }

        lines = g_strsplit (data, "\n", 0);
        g_free (data);

        if (g_strv_length (lines) < 3) {
                error = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                     GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                     "Cannot parse /etc/adjtime");
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                g_strfreev (lines);
                return FALSE;
        }

        if (strcmp (lines[2], "UTC") == 0) {
                is_utc = TRUE;
        } else if (strcmp (lines[2], "LOCAL") == 0) {
                is_utc = FALSE;
        } else {
                error = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                     GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                     "Expected UTC or LOCAL at line 3 of /etc/adjtime; found '%s'", lines[2]);
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                g_strfreev (lines);
                return FALSE;
        }
        g_strfreev (lines);
        dbus_g_method_return (context, is_utc);
        return TRUE;
}

gboolean
gnome_clock_applet_mechanism_set_hardware_clock_using_utc  (GnomeClockAppletMechanism    *mechanism,
                                                            gboolean                      using_utc,
                                                            DBusGMethodInvocation        *context)
{
        GError *error;

        error = NULL;

        if (!_check_polkit_for_action (mechanism, context, "org.gnome.clockapplet.mechanism.configurehwclock"))
                return FALSE;

        if (g_file_test ("/sbin/hwclock", 
                         G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_EXECUTABLE)) {
                int exit_status;
                char *cmd;
                cmd = g_strdup_printf ("/sbin/hwclock %s --systohc", using_utc ? "--utc" : "--localtime");
                if (!g_spawn_command_line_sync (cmd, NULL, NULL, &exit_status, &error)) {
                        GError *error2;
                        error2 = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                              GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                              "Error spawning /sbin/hwclock: %s", error->message);
                        g_error_free (error);
                        dbus_g_method_return_error (context, error2);
                        g_error_free (error2);
                        g_free (cmd);
                        return FALSE;
                }
                g_free (cmd);
                if (WEXITSTATUS (exit_status) != 0) {
                        error = g_error_new (GNOME_CLOCK_APPLET_MECHANISM_ERROR,
                                             GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
                                             "/sbin/hwclock returned %d", exit_status);
                        dbus_g_method_return_error (context, error);
                        g_error_free (error);
                        return FALSE;
                }

                if (!_rh_update_etc_sysconfig_clock (context, "UTC=", using_utc ? "true" : "false"))
                        return FALSE;

        }
        dbus_g_method_return (context);
        return TRUE;

}
