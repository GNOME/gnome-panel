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
 */

#ifndef __SYSTEM_TIMEZONE_H__
#define __SYSTEM_TIMEZONE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define SYSTEM_TIMEZONE_TYPE         (system_timezone_get_type ())
#define SYSTEM_TIMEZONE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SYSTEM_TIMEZONE_TYPE, SystemTimezone))
#define SYSTEM_TIMEZONE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), SYSTEM_TIMEZONE_TYPE, SystemTimezoneClass))
#define IS_SYSTEM_TIMEZONE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SYSTEM_TIMEZONE_TYPE))
#define IS_SYSTEM_TIMEZONE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), SYSTEM_TIMEZONE_TYPE))
#define SYSTEM_TIMEZONE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SYSTEM_TIMEZONE_TYPE, SystemTimezoneClass))

typedef struct _SystemTimezonePrivate SystemTimezonePrivate;

typedef struct
{
        GObject g_object;

        SystemTimezonePrivate *priv;
} SystemTimezone;

typedef struct
{
        GObjectClass g_object_class;
} SystemTimezoneClass;

GType system_timezone_get_type (void);

SystemTimezone *system_timezone_new (void);

const char *system_timezone_get (SystemTimezone *systz);

G_END_DECLS
#endif /* __SYSTEM_TIMEZONE_H__ */
