/* System timezone handling
 *
 * Copyright (C) 2008 Novell, Inc.
 *
 * Authors: Vincent Untz <vuntz@gnome.org>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 * 
 * Some code is based on previous code in clock-location.c and on code from
 * tz.c (shipped with version <= 2.22.0). Those files were under the same
 * license, with those authors and copyrights:
 * 
 * clock-location.c:
 * ================
 * No header, but most of the work was done (AFAIK) by
 * Federico Mena Quintero <federico@novell.com>
 * Matthias Clasen <mclasen@redhat.com>
 *
 * tz.c:
 * ====
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2004 Sun Microsystems, Inc.
 *
 * Authors: Hans Petter Jansson <hpj@ximian.com>
 *	    additional functions by Erwann Chenede <erwann.chenede@sun.com>
 *	    reworked by Vincent Untz <vuntz@gnome.org>
 * 
 * Largely based on Michael Fulbright's work on Anaconda.
 */

/* FIXME: it'd be nice to filter out the timezones that we might get when
 * parsing config files that are not in zone.tab. Note that it's also wrong
 * in some cases: eg, in tzdata2008b, Asia/Calcutta got renamed to
 * Asia/Kolkata and the old name is not in zone.tab. */

#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "system-timezone.h"

/* Files that we look at and that should be monitored */
#define CHECK_NB 5
#define ETC_TIMEZONE        "/etc/timezone"
#define ETC_TIMEZONE_MAJ    "/etc/TIMEZONE"
#define ETC_RC_CONF         "/etc/rc.conf"
#define ETC_SYSCONFIG_CLOCK "/etc/sysconfig/clock"
#define ETC_CONF_D_CLOCK    "/etc/conf.d/clock"
#define ETC_LOCALTIME       "/etc/localtime"

static const gchar *files_to_check[CHECK_NB] = {
        ETC_TIMEZONE,
        ETC_TIMEZONE_MAJ,
        ETC_SYSCONFIG_CLOCK,
        ETC_CONF_D_CLOCK,
        ETC_LOCALTIME
};

static GObject *systz_singleton = NULL;

G_DEFINE_TYPE (SystemTimezone, system_timezone, G_TYPE_OBJECT)

typedef struct {
        char *tz;
        GFileMonitor *monitors[CHECK_NB];
} SystemTimezonePrivate;

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint system_timezone_signals[LAST_SIGNAL] = { 0 };

static GObject *system_timezone_constructor (GType                  type,
                                             guint                  n_construct_properties,
                                             GObjectConstructParam *construct_properties);
static void system_timezone_finalize (GObject *obj);

static void system_timezone_monitor_changed (GFileMonitor *handle,
                                             GFile *file,
                                             GFile *other_file,
                                             GFileMonitorEvent event,
                                             gpointer user_data);
static char *system_timezone_find (void);

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SYSTEM_TIMEZONE_TYPE, SystemTimezonePrivate))

SystemTimezone *
system_timezone_new (void)
{
        return g_object_new (SYSTEM_TIMEZONE_TYPE, NULL);
}

const char *
system_timezone_get (SystemTimezone *systz)
{
        SystemTimezonePrivate *priv;

        g_return_val_if_fail (IS_SYSTEM_TIMEZONE (systz), NULL);

        priv = PRIVATE (systz);
        return priv->tz;
}

static void
system_timezone_class_init (SystemTimezoneClass *class)
{
        GObjectClass *g_obj_class = G_OBJECT_CLASS (class);

        g_obj_class->constructor = system_timezone_constructor;
        g_obj_class->finalize = system_timezone_finalize;

        system_timezone_signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (g_obj_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (SystemTimezoneClass, changed),
			      NULL, NULL,
			      NULL,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

        g_type_class_add_private (class, sizeof (SystemTimezonePrivate));
}

static void
system_timezone_init (SystemTimezone *systz)
{
        int i;
        SystemTimezonePrivate *priv = PRIVATE (systz);

        priv->tz = NULL;
        for (i = 0; i < CHECK_NB; i++) 
                priv->monitors[i] = NULL;
}

static GObject *
system_timezone_constructor (GType                  type,
                             guint                  n_construct_properties,
                             GObjectConstructParam *construct_properties)
{
        GObject *obj;
        SystemTimezonePrivate *priv;
        int i;

        /* This is a singleton, we don't need to have it per-applet */
        if (systz_singleton)
                return g_object_ref (systz_singleton);

        obj = G_OBJECT_CLASS (system_timezone_parent_class)->constructor (
                                                type,
                                                n_construct_properties,
                                                construct_properties);

        priv = PRIVATE (obj);

        priv->tz = system_timezone_find ();

        for (i = 0; i < CHECK_NB; i++) {
                GFile     *file;
                GFile     *parent;
                GFileType  parent_type;

                file = g_file_new_for_path (files_to_check[i]);

                parent = g_file_get_parent (file);
                parent_type = g_file_query_file_type (parent, G_FILE_QUERY_INFO_NONE, NULL);
                g_object_unref (parent);

                /* We don't try to monitor the file if the parent directory
                 * doesn't exist: this means we're on a system where this file
                 * is not useful to determine the system timezone.
                 * Since gio does not monitor file in non-existing directories
                 * in a clever way (as of gio 2.22, it just polls every other
                 * seconds to see if the directory now exists), this avoids
                 * unnecessary wakeups. */
                if (parent_type == G_FILE_TYPE_DIRECTORY)
                        priv->monitors[i] = g_file_monitor_file (file,
                                                                 G_FILE_MONITOR_NONE,
                                                                 NULL, NULL);
                g_object_unref (file);

                if (priv->monitors[i])
                        g_signal_connect (G_OBJECT (priv->monitors[i]),
                                          "changed", 
                                          G_CALLBACK (system_timezone_monitor_changed),
                                          obj);
        }

        systz_singleton = obj;

        return systz_singleton;
}

static void
system_timezone_finalize (GObject *obj)
{
        int i;
        SystemTimezonePrivate *priv = PRIVATE (obj);

        if (priv->tz) {
                g_free (priv->tz);
                priv->tz = NULL;
        }

        for (i = 0; i < CHECK_NB; i++) {
                if (priv->monitors[i])
                        g_object_unref (priv->monitors[i]);
                priv->monitors[i] = NULL;
        }

        G_OBJECT_CLASS (system_timezone_parent_class)->finalize (obj);

        g_assert (obj == systz_singleton);

        systz_singleton = NULL;
}

static void
system_timezone_monitor_changed (GFileMonitor *handle,
                                 GFile *file,
                                 GFile *other_file,
                                 GFileMonitorEvent event,
                                 gpointer user_data)
{
        SystemTimezonePrivate *priv = PRIVATE (user_data);
        char *new_tz;

        if (event != G_FILE_MONITOR_EVENT_CHANGED &&
            event != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT &&
            event != G_FILE_MONITOR_EVENT_DELETED &&
            event != G_FILE_MONITOR_EVENT_CREATED)
                return;

        new_tz = system_timezone_find ();

        g_assert (priv->tz != NULL && new_tz != NULL);

        if (strcmp (priv->tz, new_tz) != 0) {
                g_free (priv->tz);
                priv->tz = new_tz;

                g_signal_emit (G_OBJECT (user_data),
                               system_timezone_signals[CHANGED],
                               0, priv->tz);
        } else
                g_free (new_tz);
}


/*
 * Code to deal with the system timezone on all distros.
 * There's no dependency on the SystemTimezone GObject here.
 *
 * Here's what we know:
 *
 *  + /etc/localtime contains the binary data of the timezone.
 *    It can be a symlink to the actual data file, a hard link to the data
 *    file, or just a copy. So we can determine the timezone with this
 *    (reading the symlink, comparing inodes, or comparing content).
 *
 *  + However, most distributions also have the timezone setting
 *    configured somewhere else. This might be better to read it from there.
 *
 *    Debian/Ubuntu/Gentoo (new): content of /etc/timezone
 *    Fedora/Mandriva: the ZONE key in /etc/sysconfig/clock
 *    openSUSE: the TIMEZONE key in /etc/sysconfig/clock
 *    Solaris/OpenSolaris: the TZ key in /etc/TIMEZONE
 *    Arch Linux: the TIMEZONE key in /etc/rc.conf
 *    Gentoo (old): the ZONE key in /etc/conf.d/clock
 *
 *    FIXME: reading the system-tools-backends, it seems there's this too:
 *           Solaris: the TZ key in /etc/default/init
 *                    /etc/TIMEZONE seems to be a link to /etc/default/init
 *
 * First, some functions to handle those system config files.
 *
 */

/* This works for Debian and derivatives (including Ubuntu), and new Gentoo */
static char *
system_timezone_read_etc_timezone (void)
{
        FILE    *etc_timezone;
        GString *reading;
        int      c;

        etc_timezone = g_fopen (ETC_TIMEZONE, "r");
        if (!etc_timezone)
                return NULL;

        reading = g_string_new ("");

        c = fgetc (etc_timezone);
        /* only get the first line, we'll validate the value later */
        while (c != EOF && !g_ascii_isspace (c)) {
                reading = g_string_append_c (reading, c);
                c = fgetc (etc_timezone);
        }

        fclose (etc_timezone);

        if (reading->str && reading->str[0] != '\0')
                return g_string_free (reading, FALSE);
        else
                g_string_free (reading, TRUE);

        return NULL;
}

/* Read a file that looks like a key-file (but there's no need for groups)
 * and get the last value for a specific key */
static char *
system_timezone_read_key_file (const char *filename,
                               const char *key)
{
        GIOChannel *channel;
        char       *key_eq;
        char       *line;
        char       *retval;

        if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
                return NULL;

        channel = g_io_channel_new_file (filename, "r", NULL);
        if (!channel)
                return NULL;

        key_eq = g_strdup_printf ("%s=", key);
        retval = NULL;

        while (g_io_channel_read_line (channel, &line, NULL,
                                       NULL, NULL) == G_IO_STATUS_NORMAL) {
                if (g_str_has_prefix (line, key_eq)) {
                        char *value;
                        int   len;

                        value = line + strlen (key_eq);
                        g_strstrip (value);

                        len = strlen (value);

                        if (value[0] == '\"') {
                                if (value[len - 1] == '\"') {
                                        if (retval)
                                                g_free (retval);

                                        retval = g_strndup (value + 1,
                                                            len - 2);
                                }
                        } else {
                                if (retval)
                                        g_free (retval);

                                retval = g_strdup (line + strlen (key_eq));
                        }

                        g_strstrip (retval);
                }

                g_free (line);
        }

        g_free (key_eq);
        g_io_channel_unref (channel);

        return retval;
}

/* This works for Solaris/OpenSolaris */
static char *
system_timezone_read_etc_TIMEZONE (void)
{
        return system_timezone_read_key_file (ETC_TIMEZONE_MAJ,
                                              "TZ");
}

/* This works for Fedora and Mandriva */
static char *
system_timezone_read_etc_sysconfig_clock (void)
{
        return system_timezone_read_key_file (ETC_SYSCONFIG_CLOCK,
                                              "ZONE");
}

/* This works for openSUSE */
static char *
system_timezone_read_etc_sysconfig_clock_alt (void)
{
        return system_timezone_read_key_file (ETC_SYSCONFIG_CLOCK,
                                              "TIMEZONE");
}

/* This works for old Gentoo */
static char *
system_timezone_read_etc_conf_d_clock (void)
{
        return system_timezone_read_key_file (ETC_CONF_D_CLOCK,
                                              "TIMEZONE");
}

/* This works for Arch Linux */
static char *
system_timezone_read_etc_rc_conf (void)
{
        return system_timezone_read_key_file (ETC_RC_CONF,
                                              "TIMEZONE");
}

/*
 *
 * First, getting the timezone.
 *
 */

static char *
system_timezone_strip_path_if_valid (const char *filename)
{
        int skip;

        if (!filename || !g_str_has_prefix (filename, SYSTEM_ZONEINFODIR"/"))
                return NULL;

        /* Timezone data files also live under posix/ and right/ for some
         * reason.
         * FIXME: make sure accepting those files is valid. I think "posix" is
         * okay, not sure about "right" */
        if (g_str_has_prefix (filename, SYSTEM_ZONEINFODIR"/posix/"))
                skip = strlen (SYSTEM_ZONEINFODIR"/posix/");
        else if (g_str_has_prefix (filename, SYSTEM_ZONEINFODIR"/right/"))
                skip = strlen (SYSTEM_ZONEINFODIR"/right/");
        else
                skip = strlen (SYSTEM_ZONEINFODIR"/");

        return g_strdup (filename + skip);
}

/* Read the soft symlink from /etc/localtime */
static char *
system_timezone_read_etc_localtime_softlink (void)
{
        char *file;
        char *tz;

        if (!g_file_test (ETC_LOCALTIME, G_FILE_TEST_IS_SYMLINK))
                return NULL;

        file = g_file_read_link (ETC_LOCALTIME, NULL);
        tz = system_timezone_strip_path_if_valid (file);
        g_free (file);

        return tz;
}

typedef gboolean (*CompareFiles) (struct stat *a_stat,
                                  struct stat *b_stat,
                                  const char  *a_content,
                                  gsize        a_content_len,
                                  const char  *b_filename);

static char *
recursive_compare (struct stat  *localtime_stat,
                   const char   *localtime_content,
                   gsize         localtime_content_len,
                   const gchar  *file,
                   CompareFiles  compare_func)
{
        struct stat file_stat;

        if (g_stat (file, &file_stat) != 0)
                return NULL;

        if (S_ISREG (file_stat.st_mode)) {
                if (compare_func (localtime_stat,
                                  &file_stat,
                                  localtime_content,
                                  localtime_content_len,
                                  file))
                        return system_timezone_strip_path_if_valid (file);
                else
                        return NULL;
        } else if (S_ISDIR (file_stat.st_mode)) {
                GDir       *dir = NULL;
                char       *ret = NULL;
                const char *subfile = NULL;
                char       *subpath = NULL;

                dir = g_dir_open (file, 0, NULL);
                if (dir == NULL)
                        return NULL;

                while ((subfile = g_dir_read_name (dir)) != NULL) {
                        subpath = g_build_filename (file, subfile, NULL);

                        ret = recursive_compare (localtime_stat,
                                                 localtime_content,
                                                 localtime_content_len,
                                                 subpath,
                                                 compare_func);

                        g_free (subpath);

                        if (ret != NULL)
                                break;
                }

                g_dir_close (dir);

                return ret;
        }

        return NULL;
}


static gboolean
files_are_identical_inode (struct stat *a_stat,
                           struct stat *b_stat,
                           const char  *a_content,
                           gsize        a_content_len,
                           const char  *b_filename)
{
        return (a_stat->st_ino == b_stat->st_ino);
}


/* Determine if /etc/localtime is a hard link to some file, by looking at
 * the inodes */
static char *
system_timezone_read_etc_localtime_hardlink (void)
{
        struct stat stat_localtime;

        if (g_stat (ETC_LOCALTIME, &stat_localtime) != 0)
                return NULL;

        if (!S_ISREG (stat_localtime.st_mode))
                return NULL;

        return recursive_compare (&stat_localtime,
                                  NULL,
                                  0,
                                  SYSTEM_ZONEINFODIR,
                                  files_are_identical_inode);
}

static gboolean
files_are_identical_content (struct stat *a_stat,
                             struct stat *b_stat,
                             const char  *a_content,
                             gsize        a_content_len,
                             const char  *b_filename)
{
        char  *b_content = NULL;
        gsize  b_content_len = -1;
        int    cmp;

        if (a_stat->st_size != b_stat->st_size)
                return FALSE;

        if (!g_file_get_contents (b_filename,
                                  &b_content, &b_content_len, NULL))
                return FALSE;

        if (a_content_len != b_content_len) {
                g_free (b_content);
                return FALSE;
        }

        cmp = memcmp (a_content, b_content, a_content_len);
        g_free (b_content);

        return (cmp == 0);
}

/* Determine if /etc/localtime is a copy of a timezone file */
static char *
system_timezone_read_etc_localtime_content (void)
{
        struct stat  stat_localtime;
        char        *localtime_content = NULL;
        gsize        localtime_content_len = -1;
        char        *retval;

        if (g_stat (ETC_LOCALTIME, &stat_localtime) != 0)
                return NULL;

        if (!S_ISREG (stat_localtime.st_mode))
                return NULL;

        if (!g_file_get_contents (ETC_LOCALTIME,
                                  &localtime_content,
                                  &localtime_content_len,
                                  NULL))
                return NULL;

        retval = recursive_compare (&stat_localtime,
                                   localtime_content,
                                   localtime_content_len,
                                   SYSTEM_ZONEINFODIR,
                                   files_are_identical_content);

        g_free (localtime_content);

        return retval;
}

typedef char * (*GetSystemTimezone) (void);
/* The order of the functions here define the priority of the methods used
 * to find the timezone. First method has higher priority. */
static GetSystemTimezone get_system_timezone_methods[] = {
        /* cheap and "more correct" than data from a config file */
        system_timezone_read_etc_localtime_softlink,
        /* reading various config files */
        system_timezone_read_etc_timezone,
        system_timezone_read_etc_sysconfig_clock,
        system_timezone_read_etc_sysconfig_clock_alt,
        system_timezone_read_etc_TIMEZONE,
        system_timezone_read_etc_rc_conf,
        /* reading deprecated config files */
        system_timezone_read_etc_conf_d_clock,
        /* reading /etc/timezone directly. Expensive since we have to stat
         * many files */
        system_timezone_read_etc_localtime_hardlink,
        system_timezone_read_etc_localtime_content,
        NULL
};

static gboolean
system_timezone_is_valid (const char *tz)
{
        const char *c;

        if (!tz)
                return FALSE;

        for (c = tz; *c != '\0'; c++) {
                if (!(g_ascii_isalnum (*c) ||
                      *c == '/' || *c == '-' || *c == '_'))
                        return FALSE;
        }

        return TRUE;
}

static char *
system_timezone_find (void)
{
        char *tz;
        int   i;

        for (i = 0; get_system_timezone_methods[i] != NULL; i++) {
                tz = get_system_timezone_methods[i] ();

                if (system_timezone_is_valid (tz))
                        return tz;

                g_free (tz);
        }

        return g_strdup ("UTC");
}
