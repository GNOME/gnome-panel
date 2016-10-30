/*
 * clock.h
 *
 * Copyright (C) 2007 Vincent Untz <vuntz@gnome.org>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Vincent Untz <vuntz@gnome.org>
 *
 * Most of the original code comes from clock.c
 */

#ifndef CLOCK_APPLET_H
#define CLOCK_APPLET_H

#include <libgnome-panel/gp-applet.h>

G_BEGIN_DECLS

#define CLOCK_ICON "gnome-panel-clock"
#define CLOCK_RESOURCE_PATH "/org/gnome/panel/applet/clock/"

#define CLOCK_TYPE_APPLET clock_applet_get_type ()
G_DECLARE_FINAL_TYPE (ClockApplet, clock_applet, CLOCK, APPLET, GpApplet)

G_END_DECLS

#endif /* CLOCK_H */
