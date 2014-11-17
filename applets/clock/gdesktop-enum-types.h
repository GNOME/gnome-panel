/*
 * Copyright (C) 2014 Alberts Muktupāvels
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *    Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 */

#ifndef G_DESKTOP_ENUM_TYPES_H
#define G_DESKTOP_ENUM_TYPES_H

#include <glib-object.h>

G_BEGIN_DECLS

#define G_DESKTOP_TYPE_CLOCK_FORMAT	(g_desktop_clock_format_get_type ())
GType g_desktop_clock_format_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
