/*
 * Copyright (C) 1997-2003 Free Software Foundation, Inc.
 * Copyright (C) 2014      Alberts Muktupāvels
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
 *    Alexander Larsson
 *    Frederico Mena
 *    Gediminas Paulauskas
 *    George Lebl
 *    Mark McLoughlin
 *    Miguel de Icaza
 *    Stuart Parmenter
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgweather/location-entry.h>
#include <libgweather/timezone-menu.h>
#include <math.h>

#include "clock-common.h"
#include "clock-location-edit.h"

struct _ClockLocationEditPrivate
{
	GSettings             *settings;
	ClockLocation         *clock_location;

	GtkWidget             *ok_button;
	GtkWidget             *cancel_button;

	GtkWidget             *location_label;
	GtkWidget             *location_box;
	GWeatherLocationEntry *location_entry;
	GtkWidget             *timezone_label;
	GtkWidget             *timezone_box;
	GWeatherTimezoneMenu  *timezone_combo;
	GtkWidget             *latitude_entry;
	GtkWidget             *latitude_combo;
	GtkWidget             *longitude_entry;
	GtkWidget             *longitude_combo;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClockLocationEdit,
                            clock_location_edit,
                            GTK_TYPE_DIALOG)

enum
{
	PROP_0,
	PROP_SETTINGS,
	PROP_CLOCK_LOCATION,
	N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

static gint
sort_locations_by_name (gconstpointer a,
                        gconstpointer b)
{
	ClockLocation *location1;
	ClockLocation *location2;
	const gchar   *name1;
	const gchar   *name2;

	location1 = CLOCK_LOCATION (a);
	location2 = CLOCK_LOCATION (a);

	name1 = clock_location_get_name (location1);
	name2 = clock_location_get_name (location2);

	return g_strcmp0 (name1, name2);
}

static void
update_cities (ClockLocationEdit *edit,
               ClockLocation     *clock_location)
{
	ClockLocationEditPrivate *priv;
	GVariantIter             *iter;
	GList                    *locations;
	const gchar              *name;
	const gchar              *code;
	gboolean                  latlon_override;
	gdouble                   latitude;
	gdouble                   longitude;
	GList                    *l;
	GVariantBuilder           builder;

	priv = edit->priv;

	g_settings_get (priv->settings, KEY_CITIES, "a(ssm(dd))", &iter);
	locations = NULL;

	while (g_variant_iter_loop (iter, "(&s&sm(dd))",
	                            &name, &code,
	                            &latlon_override,
	                            &latitude, &longitude)) {
		ClockLocation *location;

		location = clock_location_new (name, code,
		                               latlon_override,
		                               latitude, longitude);

		if (priv->clock_location) {
			if (clock_location_equal (priv->clock_location, location)) {
				g_clear_object (&priv->clock_location);
				continue;
			}
		}

		locations = g_list_append (locations, location);
	}

	locations = g_list_append (locations, clock_location);
	locations = g_list_sort (locations, sort_locations_by_name);

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssm(dd))"));

	for (l = locations; l; l = l->next) {
		ClockLocation *tmp;

		tmp = CLOCK_LOCATION (l->data);
		g_variant_builder_add_value (&builder,
		                             clock_location_serialize (tmp));
		g_object_unref (tmp);
	}

	g_settings_set_value (priv->settings, KEY_CITIES,
	                      g_variant_builder_end (&builder));

	g_object_unref (clock_location);
	g_list_free (locations);

	gtk_widget_destroy (GTK_WIDGET (edit));
}

static void
ok_button_clicked (GtkButton *button,
                   gpointer   user_data)
{
	ClockLocationEdit        *edit;
	ClockLocationEditPrivate *priv;
	GWeatherLocation         *location;
	const gchar              *timezone;
	GWeatherLocation         *station;
	const gchar              *weather_code;
	gchar                    *name;
	gdouble                   latitude;
	gdouble                   longitude;
	ClockLocation            *clock_location;

	edit = CLOCK_LOCATION_EDIT (user_data);
	priv = edit->priv;

	location = gweather_location_entry_get_location (priv->location_entry);
	if (!location) {
		/* FIXME: show error */
		g_warning ("ClockLocationEdit: location == NULL");
		return;
	}

	timezone = gweather_timezone_menu_get_tzid (priv->timezone_combo);
	if (!timezone) {
		/* FIXME: show error */
		g_warning ("ClockLocationEdit: timezone == NULL");
		return;
	}

	station = location;
	while (gweather_location_get_level (station) < GWEATHER_LOCATION_WEATHER_STATION) {
		station = gweather_location_get_children (station)[0];
		g_assert (station != NULL);
	}

	weather_code = gweather_location_get_code (station);

	if (gweather_location_entry_has_custom_text (priv->location_entry))
		name = gtk_editable_get_chars (GTK_EDITABLE (priv->location_entry), 0, -1);
	else
		name = NULL;

	sscanf (gtk_entry_get_text (GTK_ENTRY (priv->latitude_entry)),
	        "%lf", &latitude);
    sscanf (gtk_entry_get_text (GTK_ENTRY (priv->longitude_entry)),
            "%lf", &longitude);

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (priv->latitude_combo)) != 0)
		latitude = -latitude;

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (priv->longitude_combo)) != 0)
		longitude = -longitude;

	clock_location = clock_location_new (name,
	                                     weather_code,
	                                     TRUE,
	                                     latitude,
	                                     longitude);
	g_free (name);

	update_cities (edit, clock_location);
}

static void
update_coords_helper (gdouble    value,
                      GtkWidget *entry,
                      GtkWidget *combo)
{
	gchar *tmp;

	tmp = g_strdup_printf ("%lf", fabs (value));
	gtk_entry_set_text (GTK_ENTRY (entry), tmp);
	g_free (tmp);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), value > 0 ? 0 : 1);
}

static void
update_coords (ClockLocationEdit *edit,
               gboolean           has_coords,
               gdouble            latitude,
               gdouble            longitude)
{
	ClockLocationEditPrivate *priv;

	priv = edit->priv;

	if (!has_coords) {
		gtk_entry_set_text (GTK_ENTRY (priv->latitude_entry), "");
		gtk_combo_box_set_active (GTK_COMBO_BOX (priv->latitude_combo), -1);

		gtk_entry_set_text (GTK_ENTRY (priv->longitude_entry), "");
		gtk_combo_box_set_active (GTK_COMBO_BOX (priv->longitude_combo), -1);

		return;
	}

	update_coords_helper (latitude,
	                      priv->latitude_entry,
	                      priv->latitude_combo);
	update_coords_helper (longitude,
	                      priv->longitude_entry,
	                      priv->longitude_combo);
}

static void
location_changed (GObject    *object,
                  GParamSpec *param,
                  gpointer    user_data)
{
	ClockLocationEdit        *edit;
	ClockLocationEditPrivate *priv;
	GWeatherLocationEntry    *entry;
	GWeatherLocation         *location;
	gboolean                  has_coords;
	GWeatherTimezone         *timezone;
	gdouble                   latitude;
	gdouble                   longitude;

	edit = CLOCK_LOCATION_EDIT (user_data);
	priv = edit->priv;
	entry = GWEATHER_LOCATION_ENTRY (object);

	location = gweather_location_entry_get_location (entry);
	has_coords = location && gweather_location_has_coords (location);
	timezone = location ? gweather_location_get_timezone (location) : NULL;
	latitude = 0.0;
	longitude = 0.0;

	if (has_coords)
		gweather_location_get_coords (location, &latitude, &longitude);

	update_coords (edit, has_coords, latitude, longitude);

	if (timezone) {
		gweather_timezone_menu_set_tzid (priv->timezone_combo,
		                                 gweather_timezone_get_tzid (timezone));
	} else {
		gweather_timezone_menu_set_tzid (priv->timezone_combo,
		                                 NULL);
	}

	if (location)
		gweather_location_unref (location);
}

static void
update_ok_button_sensitivity (ClockLocationEdit *edit)
{
	ClockLocationEditPrivate *priv;
	const gchar              *timezone;
	gchar                    *name;
	gboolean                  sensitive;

	priv = edit->priv;
	timezone = gweather_timezone_menu_get_tzid (priv->timezone_combo);
	name = gtk_editable_get_chars (GTK_EDITABLE (priv->location_entry), 0, -1);
	sensitive = FALSE;

	if (timezone && name && name[0] != '\0')
		sensitive = TRUE;
	g_free (name);

	gtk_widget_set_sensitive (priv->ok_button, sensitive);
}

static void
clock_location_edit_connect_signals (ClockLocationEdit *edit)
{
	ClockLocationEditPrivate *priv;

	priv = edit->priv;

	g_signal_connect_swapped (priv->cancel_button, "clicked",
	                          G_CALLBACK (gtk_widget_destroy), GTK_WIDGET (edit));

	g_signal_connect (priv->ok_button, "clicked",
	                  G_CALLBACK (ok_button_clicked), edit);

	g_signal_connect (priv->location_entry, "notify::location",
	                  G_CALLBACK (location_changed), edit);

	g_signal_connect_swapped (priv->location_entry, "changed",
	                          G_CALLBACK (update_ok_button_sensitivity), edit);
	g_signal_connect_swapped (priv->timezone_combo, "notify::tzid",
	                          G_CALLBACK (update_ok_button_sensitivity), edit);
}

static void
clock_location_edit_setup_widgets (ClockLocationEdit *edit)
{
	GWeatherLocation *world;
	GtkWidget        *location_entry;
	GtkWidget        *timezone_combo;

	world = gweather_location_get_world ();
	location_entry = gweather_location_entry_new (world);
	timezone_combo = gweather_timezone_menu_new (world);

	gtk_widget_show (location_entry);
	gtk_widget_show (timezone_combo);

	gtk_container_add (GTK_CONTAINER (edit->priv->location_box),
	                   location_entry);
	gtk_container_add (GTK_CONTAINER (edit->priv->timezone_box),
	                   timezone_combo);

	gtk_label_set_mnemonic_widget (GTK_LABEL (edit->priv->location_label),
	                               location_entry);
	gtk_label_set_mnemonic_widget (GTK_LABEL (edit->priv->timezone_label),
	                               timezone_combo);

	edit->priv->location_entry = GWEATHER_LOCATION_ENTRY (location_entry);
	edit->priv->timezone_combo = GWEATHER_TIMEZONE_MENU (timezone_combo);
}

static void
clock_location_edit_set_clock_location (ClockLocationEdit *edit,
                                        ClockLocation     *clock_location)
{
	ClockLocationEditPrivate *priv;
	const gchar              *name;
	gdouble                   latitude;
	gdouble                   longitude;
	gchar                    *tmp;

	priv = edit->priv;

	if (!clock_location) {
		gtk_window_set_title (GTK_WINDOW (edit), _("Add Location"));
		return;
	}

	gtk_window_set_title (GTK_WINDOW (edit), _("Edit Location"));

	priv->clock_location = g_object_ref (clock_location);

	gweather_location_entry_set_city (priv->location_entry,
	                                  clock_location_get_city (clock_location),
                                      clock_location_get_weather_code (clock_location));

	name = clock_location_get_name (clock_location);
	if (name && name[0] != '\0')
		gtk_entry_set_text (GTK_ENTRY (priv->location_entry), name);

	if (clock_location) {
		gweather_timezone_menu_set_tzid (priv->timezone_combo,
		                                 clock_location_get_tzid (clock_location));
	} else {
		gweather_timezone_menu_set_tzid (priv->timezone_combo,
		                                 NULL);
	}

	clock_location_get_coords (clock_location, &latitude, &longitude);

	tmp = g_strdup_printf ("%lf", fabs (latitude));
	gtk_entry_set_text (GTK_ENTRY (priv->latitude_entry), tmp);
	g_free (tmp);

	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->latitude_combo), latitude > 0 ? 0 : 1);

	tmp = g_strdup_printf ("%lf", fabs (longitude));
	gtk_entry_set_text (GTK_ENTRY (priv->longitude_entry), tmp);
	g_free (tmp);

	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->longitude_combo), longitude > 0 ? 0 : 1);

	update_ok_button_sensitivity (edit);
}

static GObject *
clock_location_edit_constructor (GType                  type,
                                 guint                  n_properties,
                                 GObjectConstructParam *properties)
{
	GObject           *object;
	ClockLocationEdit *edit;

	object = G_OBJECT_CLASS (clock_location_edit_parent_class)->constructor (type,
	                                                                         n_properties,
	                                                                         properties);
	edit = CLOCK_LOCATION_EDIT (object);

	clock_location_edit_connect_signals (edit);

	return object;
}

static void
clock_location_edit_finalize (GObject *object)
{
	ClockLocationEdit        *edit;
	ClockLocationEditPrivate *priv;

	edit = CLOCK_LOCATION_EDIT (object);
	priv = edit->priv;

	g_clear_object (&priv->clock_location);
	g_clear_object (&priv->settings);

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
	ClockLocation     *clock_location;

	edit = CLOCK_LOCATION_EDIT (object);

	switch (property_id)
	{
		case PROP_SETTINGS:
			settings = g_value_get_object (value);
			edit->priv->settings = g_object_ref (settings);
			break;
		case PROP_CLOCK_LOCATION:
			clock_location = g_value_get_object (value);
			clock_location_edit_set_clock_location (edit,
			                                        clock_location);
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
		case PROP_CLOCK_LOCATION:
			g_value_set_object (value, edit->priv->clock_location);
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

	object_class->constructor = clock_location_edit_constructor;
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

	object_properties[PROP_CLOCK_LOCATION] =
		g_param_spec_object ("clock-location",
		                     "clock-location",
		                     "clock-location",
		                     CLOCK_TYPE_LOCATION,
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_WRITABLE);

	g_object_class_install_properties (object_class,
	                                   N_PROPERTIES,
	                                   object_properties);

	gtk_widget_class_set_template_from_resource (widget_class,
	                                             CLOCK_RESOURCE_PATH "clock-location-edit.ui");

	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockLocationEdit,
	                                              ok_button);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockLocationEdit,
	                                              cancel_button);

	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockLocationEdit,
	                                              location_label);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockLocationEdit,
	                                              location_box);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockLocationEdit,
	                                              timezone_label);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockLocationEdit,
	                                              timezone_box);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockLocationEdit,
	                                              latitude_entry);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockLocationEdit,
	                                              latitude_combo);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockLocationEdit,
	                                              longitude_entry);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockLocationEdit,
	                                              longitude_combo);
}

static void
clock_location_edit_init (ClockLocationEdit *edit)
{
	edit->priv = clock_location_edit_get_instance_private (edit);

	gtk_widget_init_template (GTK_WIDGET (edit));

	clock_location_edit_setup_widgets (edit);
}

GtkWidget *
clock_location_edit_new (GSettings     *settings,
                         GtkWindow     *parent,
                         ClockLocation *clock_location)
{
        GObject *object;

        object = g_object_new (CLOCK_TYPE_LOCATION_EDIT,
                               "settings", settings,
                               "clock-location", clock_location,
                               NULL);

        gtk_window_set_transient_for (GTK_WINDOW (object),
                                      parent);

        return GTK_WIDGET (object);
}
