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

#ifndef GNOME_CLOCK_APPLET_MECHANISM_H
#define GNOME_CLOCK_APPLET_MECHANISM_H

#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define GNOME_CLOCK_APPLET_TYPE_MECHANISM         (gnome_clock_applet_mechanism_get_type ())
#define GNOME_CLOCK_APPLET_MECHANISM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_CLOCK_APPLET_TYPE_MECHANISM, GnomeClockAppletMechanism))
#define GNOME_CLOCK_APPLET_MECHANISM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GNOME_CLOCK_APPLET_TYPE_MECHANISM, GnomeClockAppletMechanismClass))
#define GNOME_CLOCK_APPLET_IS_MECHANISM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_CLOCK_APPLET_TYPE_MECHANISM))
#define GNOME_CLOCK_APPLET_IS_MECHANISM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_CLOCK_APPLET_TYPE_MECHANISM))
#define GNOME_CLOCK_APPLET_MECHANISM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_CLOCK_APPLET_TYPE_MECHANISM, GnomeClockAppletMechanismClass))

typedef struct GnomeClockAppletMechanismPrivate GnomeClockAppletMechanismPrivate;

typedef struct
{
        GObject        parent;
        GnomeClockAppletMechanismPrivate *priv;
} GnomeClockAppletMechanism;

typedef struct
{
        GObjectClass   parent_class;
} GnomeClockAppletMechanismClass;

typedef enum
{
        GNOME_CLOCK_APPLET_MECHANISM_ERROR_GENERAL,
        GNOME_CLOCK_APPLET_MECHANISM_ERROR_NOT_PRIVILEGED,
        GNOME_CLOCK_APPLET_MECHANISM_ERROR_INVALID_TIMEZONE_FILE,
        GNOME_CLOCK_APPLET_MECHANISM_NUM_ERRORS
} GnomeClockAppletMechanismError;

#define GNOME_CLOCK_APPLET_MECHANISM_ERROR gnome_clock_applet_mechanism_error_quark ()

GType gnome_clock_applet_mechanism_error_get_type (void);
#define GNOME_CLOCK_APPLET_MECHANISM_TYPE_ERROR (gnome_clock_applet_mechanism_error_get_type ())


GQuark                     gnome_clock_applet_mechanism_error_quark         (void);
GType                      gnome_clock_applet_mechanism_get_type            (void);
GnomeClockAppletMechanism *gnome_clock_applet_mechanism_new                 (void);

/* exported methods */
gboolean            gnome_clock_applet_mechanism_set_timezone (GnomeClockAppletMechanism    *mechanism,
                                                               const char                   *zone_file,
                                                               DBusGMethodInvocation        *context);

gboolean            gnome_clock_applet_mechanism_can_set_timezone (GnomeClockAppletMechanism    *mechanism,
                                                                   DBusGMethodInvocation        *context);

gboolean            gnome_clock_applet_mechanism_set_time     (GnomeClockAppletMechanism    *mechanism,
                                                               gint64                        seconds_since_epoch,
                                                               DBusGMethodInvocation        *context);

gboolean            gnome_clock_applet_mechanism_can_set_time (GnomeClockAppletMechanism    *mechanism,
                                                               DBusGMethodInvocation        *context);

gboolean            gnome_clock_applet_mechanism_adjust_time  (GnomeClockAppletMechanism    *mechanism,
                                                               gint64                        seconds_to_add,
                                                               DBusGMethodInvocation        *context);

gboolean            gnome_clock_applet_mechanism_get_hardware_clock_using_utc  (GnomeClockAppletMechanism    *mechanism,
                                                                                DBusGMethodInvocation        *context);

gboolean            gnome_clock_applet_mechanism_set_hardware_clock_using_utc  (GnomeClockAppletMechanism    *mechanism,
                                                                                gboolean                      using_utc,
                                                                                DBusGMethodInvocation        *context);

G_END_DECLS

#endif /* GNOME_CLOCK_APPLET_MECHANISM_H */
