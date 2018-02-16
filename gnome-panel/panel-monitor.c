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

#include "panel-monitor.h"

guint
panel_monitor_get_index (GdkMonitor *monitor)
{
  GdkDisplay *display;

  g_assert (monitor);

  display = gdk_display_get_default ();

  for (guint i = 0; i < gdk_display_get_n_monitors (display); i++)
    {
      if (monitor == gdk_display_get_monitor (display, i))
        return i;
    }
}
