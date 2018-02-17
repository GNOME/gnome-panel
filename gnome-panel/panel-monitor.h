/*
 * Copyright (C) 2018 Sebastian Geiger
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

#ifndef PANEL_MONITOR_H
#define PANEL_MONITOR_H

#include <gtk/gtk.h>

void panel_monitor_get_geometry (int monitor_index,
                                 int *x,
                                 int *y,
                                 int *height,
                                 int *width);

guint panel_monitor_get_index (GdkMonitor *monitor);

#endif //PANEL_MONITOR_H
