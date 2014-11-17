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

#include <gdesktop-enums.h>

#include "gdesktop-enum-types.h"

static const GEnumValue values[] = {
	{ G_DESKTOP_CLOCK_FORMAT_24H, "G_DESKTOP_CLOCK_FORMAT_24H", "24h" },
	{ G_DESKTOP_CLOCK_FORMAT_12H, "G_DESKTOP_CLOCK_FORMAT_12H", "12h" },
	{ 0, NULL, NULL }
};

GType
g_desktop_clock_format_get_type (void)
{
	static GType type = 0;

	if (!type)
		type = g_enum_register_static ("GDesktopClockFormat", values);

	return type;
}
