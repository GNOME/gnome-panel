/*
 * Copyright (C) 2007 Vincent Untz
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
 *    Vincent Untz <vuntz@gnome.org>
 */

#ifndef CLOCK_COMMON_H
#define CLOCK_COMMON_H

G_BEGIN_DECLS

#define CLOCK_ICON "gnome-panel-clock"

#define CLOCK_RESOURCE_PATH "/org/gnome/panel/applet/clock/"

#define CLOCK_SCHEMA             "org.gnome.gnome-panel.applet.clock"
#define DESKTOP_INTERFACE_SCHEMA "org.gnome.desktop.interface"
#define GWEATHER_SCHEMA          "org.gnome.GWeather"

#define KEY_CITIES              "cities"
#define KEY_EXPAND_APPOINTMENTS "expand-appointments"
#define KEY_EXPAND_BIRTHDAYS    "expand-birthdays"
#define KEY_EXPAND_LOCATIONS    "expand-locations"
#define KEY_EXPAND_TASKS        "expand-tasks"
#define KEY_EXPAND_WEATHER      "expand-weather"
#define KEY_SHOW_TEMPERATURE    "show-temperature"
#define KEY_SHOW_TOOLTIP        "show-tooltip"
#define KEY_SHOW_WEATHER        "show-weather"
#define KEY_SHOW_WEEKS          "show-weeks"
#define KEY_CLOCK_FORMAT        "clock-format"
#define KEY_CLOCK_SHOW_WEEKS    "clock-show-weeks"
#define KEY_CLOCK_SHOW_DATE     "clock-show-date"
#define KEY_CLOCK_SHOW_SECONDS  "clock-show-seconds"
#define KEY_TEMPERATURE_UNIT    "temperature-unit"
#define KEY_SPEED_UNIT          "speed-unit"

G_END_DECLS

#endif
