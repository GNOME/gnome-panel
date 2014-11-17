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

#ifndef CLOCK_PREFERENCES_H
#define CLOCK_PREFERENCES_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CLOCK_TYPE_PREFERENCES         (clock_preferences_get_type ())
#define CLOCK_PREFERENCES(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), \
                                        CLOCK_TYPE_PREFERENCES,          \
                                        ClockPreferences))
#define CLOCK_PREFERENCES_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    \
                                        CLOCK_TYPE_PREFERENCES,          \
                                        ClockPreferencesClass))
#define CLOCK_IS_PREFERENCES(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
                                        CLOCK_TYPE_PREFERENCES))
#define CLOCK_IS_PREFERENCES_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    \
                                        CLOCK_TYPE_PREFERENCES))
#define CLOCK_PREFERENCES_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o),   \
                                        CLOCK_TYPE_PREFERENCES,          \
                                        ClockPreferencesClass))

typedef struct _ClockPreferences        ClockPreferences;
typedef struct _ClockPreferencesClass   ClockPreferencesClass;
typedef struct _ClockPreferencesPrivate ClockPreferencesPrivate;

struct _ClockPreferences
{
	GtkDialog                parent;
	ClockPreferencesPrivate *priv;
};

struct _ClockPreferencesClass
{
	GtkDialogClass parent_class;
};

GType      clock_preferences_get_type         (void);

GtkWidget *clock_preferences_new              (GSettings     *applet_settings,
                                               GtkWindow     *parent,
                                               gint           page_number);

void       clock_preferences_update_locations (GSettings     *settings,
                                               ClockLocation *edit_or_remove_location,
                                               ClockLocation *new_or_edited_location);

G_END_DECLS

#endif
