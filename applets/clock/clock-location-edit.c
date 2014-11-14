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

#include "clock-location-edit.h"

struct _ClockLocationEditPrivate
{
        GSettings *settings;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClockLocationEdit,
                            clock_location_edit,
                            GTK_TYPE_DIALOG)

enum
{
	PROP_0,
	PROP_SETTINGS,
	N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

static void
clock_location_edit_finalize (GObject *object)
{
	ClockLocationEdit *edit;

	edit = CLOCK_LOCATION_EDIT (object)

	g_clear_object (&edit->priv->settings);

	G_OBJECT_CLASS (clock_location_edit_parent_class)->finalize (object);
}

static void
clock_location_edit_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
	ClockLocationEdit *edit;
	GSettings         *settings;

	edit = CLOCK_LOCATION_EDIT (object);

	switch (property_id)
	{
		case PROP_SETTINGS:
		        settings = g_value_get_object (value);
			edit->priv->settings = g_object_ref (settings);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
			                                   property_id,
			                                   pspec);
			break;
	}
}

static void
clock_location_edit_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
	ClockLocationEdit *edit;

	edit = CLOCK_LOCATION_EDIT (object);

	switch (property_id)
	{
		case PROP_SETTINGS:
			g_value_set_object (value, edit->priv->settings);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
			                                   property_id,
			                                   pspec);
			break;
	}
}

static void
clock_location_edit_class_init (ClockLocationEditClass *class)
{
	GObjectClass   *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	object_class->finalize = clock_location_edit_finalize;
	object_class->set_property = clock_location_edit_set_property;
	object_class->get_property = clock_location_edit_get_property;

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
                                                     CLOCK_RESOURCE_PATH "clock-location-edit.ui"
}

static void
clock_location_edit_init (ClockLocationEdit *edit)
{
	edit->priv = clock_location_edit_get_instance_private (edit);

        gtk_widget_init_template (GTK_WIDGET (edit));
}

GtkWidget *
clock_location_edit_new (GSettings *settings,
                         GtkWindow *parent)
{
        GObject *object;

        object = g_object_new (CLOCK_TYPE_LOCATION_EDIT,
                               "settings", settings,
                               NULL);

        gtk_window_set_transient_for (GTK_WINDOW (object),
                                      parent);

        return GTK_WIDGET (object);
}
