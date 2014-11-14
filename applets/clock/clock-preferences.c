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

#include "clock-preferences.h"

struct _ClockPreferencesPrivate
{
	GSettings *settings;

	GtkWidget *help_button;
	GtkWidget *time_settings_button;
	GtkWidget *close_button;

	/* General */
	GtkWidget *12hr_radio;
	GtkWidget *24hr_radio;
	GtkWidget *show_date;
	GtkWidget *show_seconds;
	GtkWidget *show_weather;
	GtkWidget *show_temperature;

	/* Locations */
	GtkWidget *cities_list;
	GtkWidget *add_button;
	GtkWidget *edit_button;
	GtkWidget *remove_button;

	/* Weather */
	GtkWidget *temperature_combo;
	GtkWidget *wind_speed_combo;
	GtkWidget *pressure_combo;
	GtkWidget *visibility_combo;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClockPreferences,
                            clock_preferences,
                            GTK_TYPE_DIALOG)

enum
{
	PROP_0,
	PROP_SETTINGS,
	N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

static void
clock_preferences_finalize (GObject *object)
{
	ClockPreferences *preferences;

	preferences = CLOCK_PREFERENCES (object);

	g_clear_object (&preferences->priv->settings);

	G_OBJECT_CLASS (clock_preferences_parent_class)->finalize (object);
}

static void
clock_preferences_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	ClockPreferences *preferences;
	GSettings        *settings;

	preferences = CLOCK_PREFERENCES (object);

	switch (property_id)
	{
		case PROP_SETTINGS:
		        settings = g_value_get_object (value);
			preferences->priv->settings = g_object_ref (settings);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
			                                   property_id,
			                                   pspec);
			break;
	}
}

static void
clock_preferences_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	ClockPreferences *preferences;

	preferences = CLOCK_PREFERENCES (object);

	switch (property_id)
	{
		case PROP_SETTINGS:
			g_value_set_object (value, preferences->priv->settings);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
			                                   property_id,
			                                   pspec);
			break;
	}
}

static void
clock_preferences_class_init (ClockPreferencesClass *class)
{
	GObjectClass   *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	object_class->finalize = clock_preferences_finalize;
	object_class->set_property = clock_preferences_set_property;
	object_class->get_property = clock_preferences_get_property;

	object_properties[PROP_SETTINGS] =
		g_param_spec_object ("settings",
		                     "settings",
		                     "settings",
		                     G_TYPE_SETTINGS,
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_WRITABLE);

	g_object_class_install_properties (object_class,
	                                   N_PROPERTIES,
	                                   object_properties);

	gtk_widget_class_set_template_from_resource (widget_class,
	                                             CLOCK_RESOURCE_PATH "clock-preferences.ui");

	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              help_button);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              time_settings_button);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              close_button);

	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              12hr_radio);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              24hr_radio);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              show_date);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              show_seconds);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              show_weather);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              show_temperature);

	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              cities_list);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              add_button);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              edit_button);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              remove_button);

	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              temperature_combo);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              wind_speed_combo);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              pressure_combo);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              visibility_combo);
}

static void
clock_preferences_init (ClockPreferences *preferences)
{
	preferences->priv = clock_preferences_get_instance_private (preferences);

	gtk_widget_init_template (GTK_WIDGET (preferences));
}

GtkWidget *
clock_preferences_new (GSettings *settings,
                       GtkWindow *parent)
{
        GObject *object;

        object = g_object_new (CLOCK_TYPE_PREFERENCES,
                               "settings", settings,
                               NULL);

        gtk_window_set_transient_for (GTK_WINDOW (object),
                                      parent);

        return GTK_WIDGET (object);
}
