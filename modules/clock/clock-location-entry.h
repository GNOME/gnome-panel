/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* location-entry.h - Location-selecting text entry
 *
 * Copyright 2008, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#ifndef CLOCK_LOCATION_ENTRY_H
#define CLOCK_LOCATION_ENTRY_H

#include <gtk/gtk.h>
#include <libgweather/gweather.h>

typedef struct _ClockLocationEntry ClockLocationEntry;
typedef struct _ClockLocationEntryClass ClockLocationEntryClass;
typedef struct _ClockLocationEntryPrivate ClockLocationEntryPrivate;

#define CLOCK_TYPE_LOCATION_ENTRY            (clock_location_entry_get_type ())
#define CLOCK_LOCATION_ENTRY(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), CLOCK_TYPE_LOCATION_ENTRY, ClockLocationEntry))
#define CLOCK_LOCATION_ENTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLOCK_TYPE_LOCATION_ENTRY, ClockLocationEntryClass))
#define CLOCK_IS_LOCATION_ENTRY(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), CLOCK_TYPE_LOCATION_ENTRY))
#define CLOCK_IS_LOCATION_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLOCK_TYPE_LOCATION_ENTRY))
#define CLOCK_LOCATION_ENTRY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLOCK_TYPE_LOCATION_ENTRY, ClockLocationEntryClass))

struct _ClockLocationEntry {
    GtkSearchEntry parent;

    /*< private >*/
    ClockLocationEntryPrivate *priv;
};

struct _ClockLocationEntryClass {
    GtkSearchEntryClass parent_class;
};

GType             clock_location_entry_get_type        (void);
GtkWidget        *clock_location_entry_new             (GWeatherLocation   *top);
void              clock_location_entry_set_location    (ClockLocationEntry *entry,
							GWeatherLocation   *loc);
GWeatherLocation *clock_location_entry_get_location    (ClockLocationEntry *entry);
gboolean          clock_location_entry_has_custom_text (ClockLocationEntry *entry);
gboolean          clock_location_entry_set_city        (ClockLocationEntry *entry,
							const char         *city_name,
							const char         *code);

#endif
