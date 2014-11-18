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
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <libgweather/gweather-enum-types.h>

#include "clock-common.h"
#include "clock-location-edit.h"
#include "clock-preferences.h"
#include "clock-utils.h"
#include "gdesktop-enum-types.h"

struct _ClockPreferencesPrivate
{
	GSettings       *applet_settings;
	GSettings       *interface_settings;
	GSettings       *gweather_settings;

	GtkWidget       *help_button;
	GtkWidget       *time_settings_button;

	GDesktopAppInfo *app_info;

	GtkListStore    *cities_store;
	GtkTreeView     *tree_view;
	GList           *locations;

	GtkWidget       *notebook;

	/* General */
	GtkWidget       *clock_options;
	GtkWidget       *clock_format_combo;
	GtkWidget       *show_date;
	GtkWidget       *show_seconds;
	GtkWidget       *show_weather;
	GtkWidget       *show_temperature;

	/* Locations */
	GtkWidget       *cities_list;
	GtkWidget       *add_button;
	GtkWidget       *edit_button;
	GtkWidget       *remove_button;

	/* Weather */
	GtkWidget       *temperature_combo;
	GtkWidget       *wind_speed_combo;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClockPreferences,
                            clock_preferences,
                            GTK_TYPE_WINDOW)

enum
{
	PROP_0,
	PROP_APPLET_SETTINGS,
	N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

static void
free_locations (ClockPreferences *preferences)
{
	ClockPreferencesPrivate *priv;
	GList                   *l;

	priv = preferences->priv;

	for (l = priv->locations; l; l = l->next)
		g_object_unref (l->data);

	g_list_free (priv->locations);
	priv->locations = NULL;
}

static void
cities_changed (GSettings   *settings,
                const gchar *key,
                gpointer     user_data)
{
	ClockPreferences        *preferences;
	ClockPreferencesPrivate *priv;
	GVariantIter            *variant_iter;
	const gchar             *name;
	const gchar             *code;
	gboolean                 has_custom_coords;
	gdouble                  latitude;
	gdouble                  longitude;
	GtkTreeIter              tree_iter;

	preferences = CLOCK_PREFERENCES (user_data);
	priv = preferences->priv;

	gtk_list_store_clear (priv->cities_store);
	free_locations (preferences);

	g_settings_get (priv->applet_settings,
	                KEY_CITIES,
	                "a(ssm(dd))",
	                &variant_iter);

	while (g_variant_iter_loop (variant_iter,
	                            "(&s&sm(dd))",
	                            &name,
	                            &code,
	                            &has_custom_coords,
	                            &latitude,
	                            &longitude)) {
		ClockLocation *location;

		location = clock_location_new (name,
		                               code,
		                               has_custom_coords,
		                               latitude,
		                               longitude);

		gtk_list_store_append (priv->cities_store,
		                       &tree_iter);
		gtk_list_store_set (priv->cities_store,
		                    &tree_iter,
		                    COLUMN_NAME,
		                    clock_location_get_name (location),
		                    COLUMN_TIMEZONE,
		                    clock_location_get_timezone (location),
		                    COLUMN_LOCATION,
		                    location,
		                    -1);

		priv->locations = g_list_append (priv->locations, location);
	}
}

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
edit_tree_row (GtkTreeModel *model,
               GtkTreePath  *path,
               GtkTreeIter  *iter,
               gpointer      user_data)
{
	ClockPreferences *preferences;
	ClockLocation    *edit_location;
	GtkWidget        *edit;

	preferences = CLOCK_PREFERENCES (user_data);

	gtk_tree_model_get (model,
	                    iter,
	                    COLUMN_LOCATION,
	                    &edit_location,
	                    -1);

	edit = clock_location_edit_new (preferences->priv->applet_settings,
	                                GTK_WINDOW (preferences),
	                                edit_location);

	gtk_window_present (GTK_WINDOW (edit));
}

static void
remove_tree_row (GtkTreeModel *model,
                 GtkTreePath  *path,
                 GtkTreeIter  *iter,
                 gpointer      user_data)
{
	ClockPreferences        *preferences;
	ClockPreferencesPrivate *priv;
	ClockLocation           *remove_location;

	preferences = CLOCK_PREFERENCES (user_data);
	priv = preferences->priv;

	gtk_tree_model_get (model,
	                    iter,
	                    COLUMN_LOCATION,
	                    &remove_location,
	                    -1);

	clock_preferences_update_locations (priv->applet_settings,
	                                    remove_location,
	                                    NULL);
}

static void
locations_selection_changed (GtkTreeSelection *selection,
                             gpointer          user_data)
{
	ClockPreferences *preferences;
	gint              n;

	preferences = CLOCK_PREFERENCES (user_data);
	n = gtk_tree_selection_count_selected_rows (selection);

	gtk_widget_set_sensitive (preferences->priv->edit_button, n > 0);
	gtk_widget_set_sensitive (preferences->priv->remove_button, n > 0);
}

static void
add_clicked (ClockPreferences *preferences)
{
	GtkWidget *edit;

	edit = clock_location_edit_new (preferences->priv->applet_settings,
	                                GTK_WINDOW (preferences),
	                                NULL);

	gtk_window_present (GTK_WINDOW (edit));
}

static void
edit_clicked (ClockPreferences *preferences)
{
	ClockPreferencesPrivate *priv;
	GtkTreeSelection        *selection;

	priv = preferences->priv;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));

	gtk_tree_selection_selected_foreach (selection,
	                                     edit_tree_row,
	                                     preferences);
}

static void
remove_clicked (ClockPreferences *preferences)
{
	ClockPreferencesPrivate *priv;
	GtkTreeSelection        *selection;

	priv = preferences->priv;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));

	gtk_tree_selection_selected_foreach (selection,
	                                     remove_tree_row,
	                                     preferences);
}

static void
help_clicked (ClockPreferences *preferences)
{
	GdkScreen   *screen;
	GError      *error;
	GtkWidget   *dialog;
	const gchar *primary;

	screen = gtk_widget_get_screen (GTK_WIDGET (preferences));

	error = NULL;
	gtk_show_uri (screen,
	              "help:clock/clock-settings",
	              GDK_CURRENT_TIME,
	              &error);

	if (error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		return;
	}

	if (!error)
		return;

	primary = _("Could not display help document 'clock'");
	dialog = gtk_message_dialog_new (GTK_WINDOW (preferences),
	                                 GTK_DIALOG_DESTROY_WITH_PARENT,
	                                 GTK_MESSAGE_ERROR,
	                                 GTK_BUTTONS_CLOSE,
	                                 "%s",
	                                 primary);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
	                                          "%s",
	                                          error->message);

	g_error_free (error);

	g_signal_connect (dialog, "response",
	                  G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_window_set_icon_name (GTK_WINDOW (dialog),
	                          CLOCK_ICON);
	gtk_window_present (GTK_WINDOW (dialog));
}

static void
time_settings_clicked (ClockPreferences *preferences)
{
	ClockPreferencesPrivate *priv;
	GdkDisplay              *display;
	GdkAppLaunchContext     *context;
	GError                  *error;
	GtkWidget               *dialog;

	priv = preferences->priv;

	if (!priv->app_info)
		return;

	display = gdk_display_get_default ();
	context = gdk_display_get_app_launch_context (display);
	error = NULL;

	g_app_info_launch (G_APP_INFO (priv->app_info),
	                   NULL,
	                   (GAppLaunchContext *) context,
	                   &error);
	g_object_unref (context);

	if (!error)
		return;

	dialog = gtk_message_dialog_new (GTK_WINDOW (preferences),
	                                 GTK_DIALOG_DESTROY_WITH_PARENT,
	                                 GTK_MESSAGE_ERROR,
	                                 GTK_BUTTONS_CLOSE,
	                                 "%s",
	                                 _("Failed to open the time settings"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
	                                          "%s",
	                                          error->message);

	g_signal_connect (dialog, "response",
	                  G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_window_present (GTK_WINDOW (dialog));

	g_error_free (error);
}

static void
clock_preferences_connect_signals (ClockPreferences *preferences)
{
	ClockPreferencesPrivate *priv;

	priv = preferences->priv;

	g_signal_connect_swapped (priv->add_button,
	                          "clicked",
	                          G_CALLBACK (add_clicked),
	                          preferences);
	g_signal_connect_swapped (priv->edit_button,
	                          "clicked",
	                          G_CALLBACK (edit_clicked),
	                          preferences);
	g_signal_connect_swapped (priv->remove_button,
	                          "clicked",
	                          G_CALLBACK (remove_clicked),
	                          preferences);

	g_signal_connect_swapped (priv->help_button,
	                          "clicked",
	                          G_CALLBACK (help_clicked),
	                          preferences);
	g_signal_connect_swapped (priv->time_settings_button,
	                          "clicked",
	                          G_CALLBACK (time_settings_clicked),
	                          preferences);
}

static gboolean
get_mapping1 (GValue *value,
              GVariant *variant,
              gpointer user_data)
{
	GType (* get_type) (void) = user_data;
	GEnumClass *klass;
	GEnumValue *eval = NULL;
	const char *s;
	guint i;

	g_variant_get (variant, "&s", &s);

	klass = g_type_class_ref (get_type ());
	for (i = 0; i < klass->n_values; ++i) {
		if (g_strcmp0 (klass->values[i].value_nick, s) != 0)
			continue;

		eval = &klass->values[i];
		break;
	}

	if (eval)
		g_value_set_int (value, eval->value);

	g_type_class_unref (klass);

	return eval != NULL;
}

static GVariant *
set_mapping1 (const GValue *value,
              const GVariantType *expected_type,
              gpointer user_data)
{
	GType (* get_type) (void) = user_data;
	GEnumClass *klass;
	GEnumValue *eval = NULL;
	int val;
	guint i;
	GVariant *variant = NULL;

	val = g_value_get_int (value);

	klass = g_type_class_ref (get_type ());
	for (i = 0; i < klass->n_values; ++i) {
		if (klass->values[i].value != val)
			continue;

		eval = &klass->values[i];
		break;
	}

	if (eval)
		variant = g_variant_new_string (eval->value_nick);

	g_type_class_unref (klass);

	return variant;
}


static gboolean
get_mapping2 (GValue *value,
              GVariant *variant,
              gpointer user_data)
{
	GType (* get_type) (void) = user_data;
	GEnumClass *klass;
	GEnumValue *eval = NULL;
	const char *s;
	guint i;

	g_variant_get (variant, "&s", &s);

	klass = g_type_class_ref (get_type ());
	for (i = 0; i < klass->n_values; ++i) {
		if (g_strcmp0 (klass->values[i].value_nick, s) != 0)
			continue;

		eval = &klass->values[i];
		break;
	}

	if (eval)
		g_value_set_int (value, eval->value - 1);

	g_type_class_unref (klass);

	return eval != NULL;
}

static GVariant *
set_mapping2 (const GValue *value,
              const GVariantType *expected_type,
              gpointer user_data)
{
	GType (* get_type) (void) = user_data;
	GEnumClass *klass;
	GEnumValue *eval = NULL;
	int val;
	guint i;
	GVariant *variant = NULL;

	val = g_value_get_int (value) + 1;

	klass = g_type_class_ref (get_type ());
	for (i = 0; i < klass->n_values; ++i) {
		if (klass->values[i].value != val)
			continue;

		eval = &klass->values[i];
		break;
	}

	if (eval)
		variant = g_variant_new_string (eval->value_nick);

	g_type_class_unref (klass);

	return variant;
}

static void
clock_preferences_setup_general (ClockPreferences *preferences)
{
	ClockPreferencesPrivate *priv;

	priv = preferences->priv;

	g_settings_bind_with_mapping (priv->interface_settings,
	                              KEY_CLOCK_FORMAT,
	                              priv->clock_format_combo,
	                              "active",
	                              G_SETTINGS_BIND_DEFAULT,
	                              get_mapping1,
	                              set_mapping1,
	                              g_desktop_clock_format_get_type,
	                              NULL);

	g_settings_bind (priv->interface_settings,
	                 KEY_CLOCK_SHOW_DATE,
	                 priv->show_date,
	                 "active",
	                 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (priv->interface_settings,
	                 KEY_CLOCK_SHOW_SECONDS,
	                 priv->show_seconds,
	                 "active",
	                 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (priv->applet_settings,
	                 KEY_SHOW_WEATHER,
	                 priv->show_weather,
	                 "active",
	                 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (priv->applet_settings,
	                 KEY_SHOW_TEMPERATURE,
	                 priv->show_temperature,
	                 "active",
	                 G_SETTINGS_BIND_DEFAULT);

	if (!clock_locale_supports_am_pm ())
		gtk_widget_hide (priv->clock_options);
}

static void
clock_preferences_setup_locations (ClockPreferences *preferences)
{
	ClockPreferencesPrivate *priv;
	GtkTreeSelection        *selection;
	GtkCellRenderer         *renderer;
	GtkTreeViewColumn       *col;

	priv = preferences->priv;

	priv->tree_view = GTK_TREE_VIEW (priv->cities_list);
	selection = gtk_tree_view_get_selection (priv->tree_view);

	g_signal_connect (selection, "changed",
	                  G_CALLBACK (locations_selection_changed), preferences);

	renderer = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("City Name"),
	                                                renderer,
	                                                "text",
	                                                COLUMN_NAME,
	                                                NULL);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (priv->cities_list),
	                             col,
	                             -1);

	renderer = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("City Time Zone"),
	                                                renderer,
	                                                "text",
	                                                COLUMN_TIMEZONE,
	                                                NULL);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (priv->cities_list),
	                             col,
	                             -1);

	priv->cities_store = gtk_list_store_new (COLUMN_LAST,
	                                         G_TYPE_STRING,
	                                         G_TYPE_STRING,
	                                         CLOCK_TYPE_LOCATION);

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->cities_list),
	                         GTK_TREE_MODEL (priv->cities_store));

	g_signal_connect (priv->applet_settings,
	                  "changed::" KEY_CITIES,
	                  G_CALLBACK (cities_changed),
	                  preferences);
	cities_changed (priv->applet_settings,
	                KEY_CITIES,
	                preferences);
}

static void
clock_preferences_setup_weather (ClockPreferences *preferences)
{
	g_settings_bind_with_mapping (preferences->priv->gweather_settings,
	                              KEY_TEMPERATURE_UNIT,
	                              preferences->priv->temperature_combo,
	                              "active",
	                              G_SETTINGS_BIND_DEFAULT,
	                              get_mapping2,
	                              set_mapping2,
	                              gweather_temperature_unit_get_type,
	                              NULL);
	g_settings_bind_with_mapping (preferences->priv->gweather_settings,
	                              KEY_SPEED_UNIT,
	                              preferences->priv->wind_speed_combo,
	                              "active",
	                              G_SETTINGS_BIND_DEFAULT,
	                              get_mapping2,
	                              set_mapping2,
	                              gweather_speed_unit_get_type,
	                              NULL);
}

static GObject *
clock_preferences_constructor (GType                  type,
                               guint                  n_properties,
                               GObjectConstructParam *properties)
{
	GObject                 *object;
	ClockPreferences        *preferences;
	ClockPreferencesPrivate *priv;

	object = G_OBJECT_CLASS (clock_preferences_parent_class)->constructor (type,
	                                                                       n_properties,
	                                                                       properties);
	preferences = CLOCK_PREFERENCES (object);
	priv = preferences->priv;

	clock_preferences_setup_general (preferences);
	clock_preferences_setup_locations (preferences);
	clock_preferences_setup_weather (preferences);

	gtk_widget_set_sensitive (priv->time_settings_button,
	                          priv->app_info != NULL);

	clock_preferences_connect_signals (preferences);

	return object;
}

static void
clock_preferences_finalize (GObject *object)
{
	ClockPreferences *preferences;

	preferences = CLOCK_PREFERENCES (object);

	g_clear_object (&preferences->priv->cities_store);
	g_clear_object (&preferences->priv->app_info);

	g_clear_object (&preferences->priv->applet_settings);
	g_clear_object (&preferences->priv->interface_settings);
	g_clear_object (&preferences->priv->gweather_settings);

	free_locations (preferences);

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
		case PROP_APPLET_SETTINGS:
			settings = g_value_get_object (value);
			preferences->priv->applet_settings = g_object_ref (settings);
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
		case PROP_APPLET_SETTINGS:
			g_value_set_object (value, preferences->priv->applet_settings);
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

	object_class->constructor = clock_preferences_constructor;
	object_class->finalize = clock_preferences_finalize;
	object_class->set_property = clock_preferences_set_property;
	object_class->get_property = clock_preferences_get_property;

	object_properties[PROP_APPLET_SETTINGS] =
		g_param_spec_object ("applet-settings",
		                     "applet-settings",
		                     "applet-settings",
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
	                                              notebook);

	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              clock_options);
	gtk_widget_class_bind_template_child_private (widget_class,
	                                              ClockPreferences,
	                                              clock_format_combo);
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
}

static void
clock_preferences_init (ClockPreferences *preferences)
{
	ClockPreferencesPrivate *priv;

	preferences->priv = clock_preferences_get_instance_private (preferences);
	priv = preferences->priv;

	priv->interface_settings = g_settings_new (DESKTOP_INTERFACE_SCHEMA);
	priv->gweather_settings = g_settings_new (GWEATHER_SCHEMA);

	priv->app_info = g_desktop_app_info_new ("gnome-datetime-panel.desktop");

	gtk_widget_init_template (GTK_WIDGET (preferences));
}

GtkWidget *
clock_preferences_new (GSettings *applet_settings,
                       GtkWindow *parent,
                       gint       page_number)
{
	GObject          *object;
	ClockPreferences *preferences;
	GtkWindow        *window;

	object = g_object_new (CLOCK_TYPE_PREFERENCES,
                               "applet-settings", applet_settings,
                               NULL);
	preferences = CLOCK_PREFERENCES (object);
	window = GTK_WINDOW (object);

	gtk_window_set_icon_name (window, CLOCK_ICON);

	if (page_number > -1)
		gtk_notebook_set_current_page (GTK_NOTEBOOK (preferences->priv->notebook),
		                               page_number);

	return GTK_WIDGET (object);
}

void
clock_preferences_update_locations (GSettings     *settings,
                                    ClockLocation *edit_or_remove_location,
                                    ClockLocation *new_or_edited_location)
{
	GVariantIter             *iter;
	GList                    *locations;
	const gchar              *name;
	const gchar              *code;
	gboolean                  latlon_override;
	gdouble                   latitude;
	gdouble                   longitude;
	GList                    *l;
	GVariantBuilder           builder;

	g_settings_get (settings, KEY_CITIES, "a(ssm(dd))", &iter);
	locations = NULL;

	while (g_variant_iter_loop (iter, "(&s&sm(dd))",
	                            &name, &code,
	                            &latlon_override,
	                            &latitude, &longitude)) {
		ClockLocation *location;

		location = clock_location_new (name, code,
		                               latlon_override,
		                               latitude, longitude);

		if (edit_or_remove_location)
			if (clock_location_equal (edit_or_remove_location, location))
				continue;

		locations = g_list_append (locations, location);
	}

	if (new_or_edited_location)
		locations = g_list_append (locations, new_or_edited_location);
	locations = g_list_sort (locations, sort_locations_by_name);

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssm(dd))"));

	for (l = locations; l; l = l->next) {
		ClockLocation *tmp;

		tmp = CLOCK_LOCATION (l->data);
		g_variant_builder_add_value (&builder,
		                             clock_location_serialize (tmp));
		g_object_unref (tmp);
	}

	g_settings_set_value (settings, KEY_CITIES,
	                      g_variant_builder_end (&builder));

	g_list_free (locations);
}
