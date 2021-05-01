/*
 * Copyright (C) 2021 Alberts Muktupāvels
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

#ifndef CLOCK_BUTTON_H
#define CLOCK_BUTTON_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CLOCK_TYPE_BUTTON (clock_button_get_type ())
G_DECLARE_FINAL_TYPE (ClockButton, clock_button, CLOCK, BUTTON, GtkToggleButton)

GtkWidget *clock_button_new             (void);

GtkWidget *clock_button_get_weather_box (ClockButton     *self);

void       clock_button_set_orientation (ClockButton     *self,
                                         GtkOrientation   orientation);

void       clock_button_set_position    (ClockButton     *self,
                                         GtkPositionType  position);

void       clock_button_set_icon_size   (ClockButton     *self,
                                         guint            icon_size);

void       clock_button_set_clock       (ClockButton     *self,
                                         const char      *clock);

void       clock_button_set_weather     (ClockButton     *self,
                                         const char      *icon_name,
                                         const char      *temperature);

G_END_DECLS

#endif
