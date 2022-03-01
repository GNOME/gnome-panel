/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * clock.c: the GNOME clock applet
 *
 * Copyright (C) 1997-2003 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Miguel de Icaza
 *      Frederico Mena
 *      Stuart Parmenter
 *      Alexander Larsson
 *      George Lebl
 *      Gediminas Paulauskas
 *      Mark McLoughlin
 */

#include "config.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <locale.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>

#include <libgnome-desktop/gnome-wall-clock.h>
#include <libgweather/gweather.h>

#include "clock-applet.h"
#include "clock-button.h"

#include "calendar-window.h"
#include "clock-location.h"
#include "clock-location-entry.h"
#include "clock-location-tile.h"
#include "clock-map.h"
#include "clock-utils.h"

enum {
	COL_CITY_NAME = 0,
	COL_CITY_TZ,
        COL_CITY_LOC,
	COL_CITY_LAST
};

struct _ClockApplet
{
	GpApplet parent;

	/* widgets */
        GtkWidget *panel_button;	/* main toggle button for the whole clock */

	GtkWidget *calendar_popup;

        GtkWidget *clock_vbox;
	GtkSizeGroup *clock_group;

	GtkBuilder *builder;

        /* Preferences dialog */
        GtkWidget *prefs_window;
        GtkTreeView *prefs_locations;

	GtkWidget *prefs_location_add_button;
	GtkWidget *prefs_location_edit_button;
	GtkWidget *prefs_location_remove_button;

	ClockLocationEntry *location_entry;

	GtkWidget *time_settings_button;
	GAppInfo *datetime_appinfo;

	GtkListStore *cities_store;
        GtkWidget *cities_section;
        GtkWidget *map_widget;

	/* preferences */
        GSettings   *applet_settings;
        GSettings   *weather_settings;
        GSettings   *clock_settings;

        /* Locations */
        GWeatherLocation *world;
        GList *locations;
        GList *location_tiles;

	/* runtime data */
        GnomeWallClock    *wall_clock;
};

G_DEFINE_TYPE (ClockApplet, clock_applet, GP_TYPE_APPLET)

static void display_properties_dialog (ClockApplet       *applet,
                                       gboolean           start_in_locations_page);

static inline GtkWidget *
_clock_get_widget (ClockApplet *cd,
                   const gchar *name)
{
	return GTK_WIDGET (gtk_builder_get_object (cd->builder, name));
}

/* sets accessible name and description for the widget */
static void
set_atk_name_description (GtkWidget  *widget,
			  const char *name,
			  const char *desc)
{
	AtkObject *obj;
	obj = gtk_widget_get_accessible (widget);

	/* return if gail is not loaded */
	if (!GTK_IS_ACCESSIBLE (obj))
		return;

	if (desc != NULL)
		atk_object_set_description (obj, desc);
	if (name != NULL)
		atk_object_set_name (obj, name);
}

static void
update_location_tiles (ClockApplet *cd)
{
        GList *l;

        for (l = cd->location_tiles; l; l = l->next) {
                ClockLocationTile *tile;

                tile = CLOCK_LOCATION_TILE (l->data);
                clock_location_tile_refresh (tile, FALSE);
        }
}

static void
update_tooltip (ClockApplet *cd)
{
        gboolean show_date;

        show_date = g_settings_get_boolean (cd->clock_settings, "clock-show-date");
        if (!show_date) {
                GDateTime *dt;
                char *tip;

                dt = g_date_time_new_now_local ();

		/* Translators: This is a strftime format string.
		 * It is used to display a date.
		 */
                tip = g_date_time_format (dt, _("%A %B %d (%Z)"));
                g_date_time_unref (dt);

                gtk_widget_set_tooltip_text (cd->panel_button, tip);
                g_free (tip);
        } else {
#ifdef HAVE_EDS
		if (cd->calendar_popup)
			gtk_widget_set_tooltip_text (cd->panel_button,
						     _("Click to hide your appointments and tasks"));
		else
			gtk_widget_set_tooltip_text (cd->panel_button,
						     _("Click to view your appointments and tasks"));
#else
		if (cd->calendar_popup)
			gtk_widget_set_tooltip_text (cd->panel_button,
						     _("Click to hide month calendar"));
		else
			gtk_widget_set_tooltip_text (cd->panel_button,
						     _("Click to view month calendar"));
#endif
        }
}

static void
update_clock (GnomeWallClock *wall_clock, GParamSpec *pspec, ClockApplet *cd)
{
        const char *clock;

        clock = gnome_wall_clock_get_clock (cd->wall_clock);
        clock_button_set_clock (CLOCK_BUTTON (cd->panel_button), clock);

        update_tooltip (cd);
        update_location_tiles (cd);

        if (cd->map_widget && cd->calendar_popup && gtk_widget_get_visible (cd->calendar_popup))
                clock_map_update_time (CLOCK_MAP (cd->map_widget));
}

static void
free_locations (ClockApplet *cd)
{
        GList *l;

        for (l = cd->locations; l; l = l->next)
                g_object_unref (l->data);

        g_list_free (cd->locations);
        cd->locations = NULL;
}

static gboolean
close_on_escape (GtkWidget       *widget,
		 GdkEventKey     *event,
		 GtkToggleButton *toggle_button)
{
	if (event->keyval == GDK_KEY_Escape) {
		gtk_toggle_button_set_active (toggle_button, FALSE);
		return TRUE;
	}

	return FALSE;
}

static gboolean
delete_event (GtkWidget       *widget,
	      GdkEvent        *event,
	      GtkToggleButton *toggle_button)
{
	gtk_toggle_button_set_active (toggle_button, FALSE);
	return TRUE;
}

static void
edit_locations_cb (CalendarWindow *calwin, gpointer data)
{
        ClockApplet *cd;

        cd = data;

        display_properties_dialog (cd, TRUE);
}

static GtkWidget *
create_calendar (ClockApplet *cd)
{
	gboolean invert;
	GtkWidget *window;

	invert = gp_applet_get_position (GP_APPLET (cd)) == GTK_POS_BOTTOM;
	window = calendar_window_new (cd->applet_settings, invert);

	g_object_bind_property (cd, "locked-down",
				window, "locked-down",
				G_BINDING_DEFAULT|G_BINDING_SYNC_CREATE);

	calendar_window_set_show_weeks (CALENDAR_WINDOW (window),
				        g_settings_get_boolean (cd->applet_settings, "show-weeks"));
	calendar_window_set_time_format (CALENDAR_WINDOW (window),
					 g_settings_get_enum (cd->clock_settings, "clock-format"));

        gtk_window_set_screen (GTK_WINDOW (window),
			       gtk_widget_get_screen (GTK_WIDGET (cd)));

        g_signal_connect (window, "edit-locations",
                          G_CALLBACK (edit_locations_cb), cd);

	g_signal_connect (window, "delete_event",
			  G_CALLBACK (delete_event), cd->panel_button);
	g_signal_connect (window, "key_press_event",
			  G_CALLBACK (close_on_escape), cd->panel_button);

	return window;
}

static void
get_monitor_geometry (ClockApplet  *applet,
                      GdkRectangle *geometry)
{
	GdkDisplay *display;
	GdkWindow *window;
	GdkMonitor *monitor;

	display = gdk_display_get_default ();
	window = gtk_widget_get_window (applet->panel_button);
	monitor = gdk_display_get_monitor_at_window (display, window);

	gdk_monitor_get_geometry (monitor, geometry);
}

static void
position_calendar_popup (ClockApplet *cd)
{
	GtkRequisition  req;
	GtkAllocation   allocation;
	GdkRectangle    monitor;
	GdkGravity      gravity = GDK_GRAVITY_NORTH_WEST;
	int             button_w, button_h;
	int             x, y;
	int             w, h;

	/* Get root origin of the toggle button, and position above that. */
	gdk_window_get_origin (gtk_widget_get_window (cd->panel_button),
			       &x, &y);

	gtk_window_get_size (GTK_WINDOW (cd->calendar_popup), &w, &h);
	gtk_widget_get_preferred_size (cd->calendar_popup, &req, NULL);
	w = req.width;
	h = req.height;

	gtk_widget_get_allocation (cd->panel_button, &allocation);
	button_w = allocation.width;
	button_h = allocation.height;

	get_monitor_geometry (cd, &monitor);

	/* Based on panel orientation, position the popup.
	 * Ignore window gravity since the window is undecorated.
	 * The orientations are all named backward from what
	 * I expected.
	 */
	switch (gp_applet_get_position (GP_APPLET (cd))) {
	case GTK_POS_LEFT:
		x += button_w;
		if ((y + h) > monitor.y + monitor.height)
			y -= (y + h) - (monitor.y + monitor.height);

		if ((y + h) > (monitor.height / 2))
			gravity = GDK_GRAVITY_SOUTH_WEST;
		else
			gravity = GDK_GRAVITY_NORTH_WEST;

		break;
	case GTK_POS_RIGHT:
		x -= w;
		if ((y + h) > monitor.y + monitor.height)
			y -= (y + h) - (monitor.y + monitor.height);

		if ((y + h) > (monitor.height / 2))
			gravity = GDK_GRAVITY_SOUTH_EAST;
		else
			gravity = GDK_GRAVITY_NORTH_EAST;

		break;
	case GTK_POS_TOP:
		y += button_h;
		if ((x + w) > monitor.x + monitor.width)
			x -= (x + w) - (monitor.x + monitor.width);

		gravity = GDK_GRAVITY_NORTH_WEST;

		break;
	case GTK_POS_BOTTOM:
		y -= h;
		if ((x + w) > monitor.x + monitor.width)
			x -= (x + w) - (monitor.x + monitor.width);

		gravity = GDK_GRAVITY_SOUTH_WEST;

		break;
	default:
		g_assert_not_reached ();
		break;
	}

	gtk_window_move (GTK_WINDOW (cd->calendar_popup), x, y);
	gtk_window_set_gravity (GTK_WINDOW (cd->calendar_popup), gravity);
}

static void
add_to_group (GtkWidget *child, gpointer data)
{
	GtkSizeGroup *group = data;

	gtk_size_group_add_widget (group, child);
}

static void
create_clock_window (ClockApplet *cd)
{
	GtkWidget *locations_box;

        locations_box = calendar_window_get_locations_box (CALENDAR_WINDOW (cd->calendar_popup));
        gtk_widget_show (locations_box);

	cd->clock_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add (GTK_CONTAINER (locations_box), cd->clock_vbox);

	cd->clock_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_container_foreach (GTK_CONTAINER (locations_box),
			       (GtkCallback) add_to_group,
			       cd->clock_group);
}

static gint
sort_locations_by_name (gconstpointer a, gconstpointer b)
{
        ClockLocation *loc_a = (ClockLocation *) a;
        ClockLocation *loc_b = (ClockLocation *) b;

        const char *name_a = clock_location_get_name (loc_a);
        const char *name_b = clock_location_get_name (loc_b);

        return strcmp (name_a, name_b);
}

static void
create_cities_store (ClockApplet *cd)
{
	GtkTreeIter iter;
        GList *cities = cd->locations;
        GList *list = NULL;

        if (cd->cities_store) {
                g_object_unref (G_OBJECT (cd->cities_store));
                cd->cities_store = NULL;
        }

	/* City name, Timezone name, Coordinates in lat/long */
	cd->cities_store = g_object_ref (gtk_list_store_new (COL_CITY_LAST,
                                                             G_TYPE_STRING,		/* COL_CITY_NAME */
                                                             G_TYPE_STRING,		/* COL_CITY_TZ */
                                                             CLOCK_LOCATION_TYPE));	/* COL_CITY_LOC */

        list = g_list_copy (cities);
        list = g_list_sort (list, sort_locations_by_name);

	while (list) {
		ClockLocation *loc = CLOCK_LOCATION (list->data);

		gtk_list_store_append (cd->cities_store, &iter);
		gtk_list_store_set (cd->cities_store, &iter,
				    COL_CITY_NAME, clock_location_get_name (loc),
				    /* FIXME: translate the timezone */
				    COL_CITY_TZ, clock_location_get_timezone_identifier (loc),
                                    COL_CITY_LOC, loc,
				    -1);

                list = list->next;
	}

	 
	if (cd->prefs_window) { 	 
		GtkWidget *widget = _clock_get_widget (cd, "cities_list"); 	 
		gtk_tree_view_set_model (GTK_TREE_VIEW (widget), 	 
		GTK_TREE_MODEL (cd->cities_store)); 	 
	}
}

static gint
sort_locations_by_time (gconstpointer a, gconstpointer b)
{
        ClockLocation *loc_a = (ClockLocation *) a;
        ClockLocation *loc_b = (ClockLocation *) b;

        GDateTime *dt1;
        GDateTime *dt2;
        gint ret;

        dt1 = clock_location_localtime (loc_a);
        dt2 = clock_location_localtime (loc_b);

        ret = g_date_time_compare (dt1, dt2);

        g_date_time_unref (dt1);
        g_date_time_unref (dt2);

        return ret;
}

static void
location_tile_pressed_cb (ClockLocationTile *tile, gpointer data)
{
        ClockApplet *cd = data;
        ClockLocation *loc;

        loc = clock_location_tile_get_location (tile);

        clock_map_blink_location (CLOCK_MAP (cd->map_widget), loc);

        g_object_unref (loc);
}

static int
location_tile_need_clock_format_cb(ClockLocationTile *tile, gpointer data)
{
        ClockApplet *cd = data;

        return g_settings_get_enum (cd->clock_settings, "clock-format");
}

static void
create_cities_section (ClockApplet *cd)
{
        GList *node;
        ClockLocationTile *city;
        GList *cities;

        if (cd->cities_section) {
                gtk_widget_destroy (cd->cities_section);
                cd->cities_section = NULL;
        }

        g_list_free (cd->location_tiles);
        cd->location_tiles = NULL;

        cd->cities_section = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width (GTK_CONTAINER (cd->cities_section), 0);

	cities = cd->locations;
        if (g_list_length (cities) == 0) {
                /* if the list is empty, don't bother showing the
                   cities section */
                gtk_widget_hide (cd->cities_section);
                return;
        }

        /* Copy the existing list, so we can sort it nondestructively */
        node = g_list_copy (cities);
        node = g_list_sort (node, sort_locations_by_time);
        node = g_list_reverse (node);

        while (node) {
                ClockLocation *loc = node->data;

                city = clock_location_tile_new (loc, CLOCK_FACE_SMALL);
                g_signal_connect (city, "tile-pressed",
                                  G_CALLBACK (location_tile_pressed_cb), cd);
                g_signal_connect (city, "need-clock-format",
                                  G_CALLBACK (location_tile_need_clock_format_cb), cd);

                gtk_box_pack_start (GTK_BOX (cd->cities_section),
                                    GTK_WIDGET (city),
                                    FALSE, FALSE, 0);

                cd->location_tiles = g_list_prepend (cd->location_tiles, city);

                clock_location_tile_refresh (city, TRUE);

                node = g_list_next (node);
        }

        g_list_free (node);

        gtk_box_pack_end (GTK_BOX (cd->clock_vbox),
                          cd->cities_section, FALSE, FALSE, 0);

        gtk_widget_show_all (cd->cities_section);
}

static GList *
map_need_locations_cb (ClockMap *map, gpointer data)
{
        ClockApplet *cd = data;

        return cd->locations;
}

static void
create_map_section (ClockApplet *cd)
{
        ClockMap *map;

        if (cd->map_widget) {
                gtk_widget_destroy (cd->map_widget);
                cd->map_widget = NULL;
        }

        map = clock_map_new ();
        g_signal_connect (map, "need-locations",
                          G_CALLBACK (map_need_locations_cb), cd);

        cd->map_widget = GTK_WIDGET (map);

        gtk_widget_set_margin_top (cd->map_widget, 1);
        gtk_widget_set_margin_bottom (cd->map_widget, 1);
        gtk_widget_set_margin_start (cd->map_widget, 1);
        gtk_widget_set_margin_end (cd->map_widget, 1);

        gtk_box_pack_start (GTK_BOX (cd->clock_vbox), cd->map_widget, TRUE, TRUE, 0);
        gtk_widget_show (cd->map_widget);
}

static void
update_calendar_popup (ClockApplet *cd)
{
        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (cd->panel_button))) {
                if (cd->calendar_popup) {
                        gtk_widget_destroy (cd->calendar_popup);
                        cd->calendar_popup = NULL;
                        cd->cities_section = NULL;
                        cd->map_widget = NULL;
			cd->clock_vbox = NULL;
			
        		g_list_free (cd->location_tiles);
        		cd->location_tiles = NULL;
                }
		update_tooltip (cd);
                return;
        }

        if (!cd->calendar_popup) {
                cd->calendar_popup = create_calendar (cd);
                g_object_add_weak_pointer (G_OBJECT (cd->calendar_popup),
                                           (gpointer *) &cd->calendar_popup);
		update_tooltip (cd);

                create_clock_window (cd);
                create_cities_store (cd);
                create_cities_section (cd);
                create_map_section (cd);
        }

        if (cd->calendar_popup && gtk_widget_get_realized (cd->panel_button)) {
		calendar_window_refresh (CALENDAR_WINDOW (cd->calendar_popup));
		position_calendar_popup (cd);
		gtk_window_present (GTK_WINDOW (cd->calendar_popup));
        }
}

static void
toggle_calendar (GtkWidget *button,
                 ClockApplet *cd)
{
	/* if time is wrong, the user might try to fix it by clicking on the
	 * clock */
	update_clock (NULL, NULL, cd);
	update_calendar_popup (cd);
}

static gboolean
weather_tooltip (GtkWidget   *widget,
                 gint         x,
                 gint         y,
                 gboolean     keyboard_mode,
                 GtkTooltip  *tooltip,
                 ClockApplet *cd)
{
        GList *locations, *l;
        GWeatherInfo *info;

        locations = cd->locations;

        for (l = locations; l; l = l->next) {
		ClockLocation *location = l->data;
                if (clock_location_is_current (location)) {
                        info = clock_location_get_weather_info (location);
                        if (!info || !gweather_info_is_valid (info))
                                continue;

                        weather_info_setup_tooltip (info, location, tooltip,
                                                    g_settings_get_enum (cd->clock_settings, "clock-format"));

                        return TRUE;
                }
        }

        return FALSE;
}

static void
panel_icon_size_cb (GpApplet    *applet,
                    GParamSpec  *pspec,
                    ClockApplet *self)
{
        clock_button_set_icon_size (CLOCK_BUTTON (self->panel_button),
                                    gp_applet_get_panel_icon_size (applet));
}

static void
create_clock_widget (ClockApplet *cd)
{
        GtkWidget *weather_box;

        g_signal_connect (cd->wall_clock, "notify::clock",
                          G_CALLBACK (update_clock), cd);

        /* Main toggle button */
        cd->panel_button = clock_button_new ();

        clock_button_set_orientation (CLOCK_BUTTON (cd->panel_button),
                                      gp_applet_get_orientation (GP_APPLET (cd)));

        clock_button_set_position (CLOCK_BUTTON (cd->panel_button),
                                   gp_applet_get_position (GP_APPLET (cd)));

        clock_button_set_icon_size (CLOCK_BUTTON (cd->panel_button),
                                    gp_applet_get_panel_icon_size (GP_APPLET (cd)));

        g_signal_connect (GP_APPLET (cd),
                          "notify::panel-icon-size",
                          G_CALLBACK (panel_icon_size_cb),
                          cd);

        g_signal_connect (cd->panel_button,
                          "toggled",
                          G_CALLBACK (toggle_calendar),
                          cd);

        /* Weather orientable box */
        weather_box = clock_button_get_weather_box (CLOCK_BUTTON (cd->panel_button));
        gtk_widget_set_has_tooltip (weather_box, TRUE);

        g_signal_connect (weather_box,
                          "query-tooltip",
                          G_CALLBACK (weather_tooltip),
                          cd);

        /* Done! */

	set_atk_name_description (GTK_WIDGET (cd), NULL, _("Computer Clock"));

	gtk_container_add (GTK_CONTAINER (cd), cd->panel_button);
	gtk_container_set_border_width (GTK_CONTAINER (cd), 0);
	gtk_widget_show (cd->panel_button);

	/* Refresh the clock so that it paints its first state */
        update_clock (NULL, NULL, cd);
}

static void
verb_display_properties_dialog (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
        display_properties_dialog (CLOCK_APPLET (user_data), FALSE);
}

static void
copy_time (GSimpleAction *action,
           GVariant      *parameter,
	   gpointer       user_data)
{
	ClockApplet *cd = (ClockApplet *) user_data;
	const char *time;

        time = gnome_wall_clock_get_clock (cd->wall_clock);

	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
				time, -1);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				time, -1);
}

static void
ensure_datetime_appinfo (ClockApplet *cd)
{
	if (!cd->datetime_appinfo)
		cd->datetime_appinfo = (GAppInfo *) g_desktop_app_info_new ("gnome-datetime-panel.desktop");
}

static void
update_set_time_button (ClockApplet *cd)
{
	if (!cd->time_settings_button)
		return;

	ensure_datetime_appinfo (cd);

	gtk_widget_set_sensitive (cd->time_settings_button,
				  cd->datetime_appinfo != NULL);
}

static void
run_time_settings (GtkWidget   *unused,
                   ClockApplet *cd)
{
	GdkScreen           *screen;
	GdkDisplay          *display;
	GdkAppLaunchContext *context;
	GError              *error;

	update_set_time_button (cd);
	ensure_datetime_appinfo (cd);

	if (!cd->datetime_appinfo)
		return;

        screen = gtk_widget_get_screen (GTK_WIDGET (cd));
        display = gdk_screen_get_display (screen);
        context = gdk_display_get_app_launch_context (display);
        gdk_app_launch_context_set_screen (context, screen);

	error = NULL;
	g_app_info_launch (cd->datetime_appinfo, NULL,
			   (GAppLaunchContext *) context, &error);

	g_object_unref (context);

	if (error) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL,
						 0,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
                                                 _("Failed to open the time settings"));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_window_present (GTK_WINDOW (dialog));

		g_error_free (error);
	}
}

static void
config_date (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
	run_time_settings (NULL, CLOCK_APPLET (user_data));
}

static const GActionEntry clock_menu_actions [] = {
        { "preferences", verb_display_properties_dialog, NULL, NULL, NULL },
        { "copy-time",   copy_time,                      NULL, NULL, NULL },
        { "config",      config_date,                    NULL, NULL, NULL },
        { NULL }
};

static void
format_changed (GSettings   *settings,
                const gchar *key,
                ClockApplet *clock)
{
	if (clock->calendar_popup != NULL) {
		calendar_window_set_time_format (CALENDAR_WINDOW (clock->calendar_popup),
                                                 g_settings_get_enum (settings, "clock-format"));
                position_calendar_popup (clock);
	}

}

static void
location_weather_updated_cb (ClockLocation  *location,
                             GWeatherInfo   *info,
                             gpointer        data)
{
	ClockApplet *cd = data;
	const gchar *icon_name;
	const gchar *temp;

	if (!info || !gweather_info_is_valid (info))
		return;

	if (!clock_location_is_current (location))
		return;

	icon_name = NULL;
	if (g_settings_get_boolean (cd->applet_settings, "show-weather")) {
		if (gp_applet_get_prefer_symbolic_icons (GP_APPLET (cd)))
			icon_name = gweather_info_get_symbolic_icon_name (info);
		else
			icon_name = gweather_info_get_icon_name (info);
	}

	temp = NULL;
	if (g_settings_get_boolean (cd->applet_settings, "show-temperature")) {
		temp = gweather_info_get_temp_summary (info);
	}

	clock_button_set_weather (CLOCK_BUTTON (cd->panel_button),
	                          icon_name,
	                          temp);
}

static void
location_set_current_cb (ClockLocation *loc, 
			 gpointer       data)
{
	ClockApplet *cd = data;
	GWeatherInfo *info;

	info = clock_location_get_weather_info (loc);
	location_weather_updated_cb (loc, info, cd);

	if (cd->map_widget)
		clock_map_refresh (CLOCK_MAP (cd->map_widget));
        update_location_tiles (cd);
}

static void
locations_changed (GSettings   *settings,
                   const gchar *key,
                   ClockApplet *cd)
{
	GList *l;
	ClockLocation *loc;
	glong id;

	if (!cd->locations) {
		if (cd->panel_button) {
			clock_button_set_weather (CLOCK_BUTTON (cd->panel_button),
			                          NULL,
			                          NULL);
		}
	}

	for (l = cd->locations; l; l = l->next) {
		loc = l->data;

		id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (loc), "weather-updated"));
		if (id == 0) {
			id = g_signal_connect (loc, "weather-updated",
						G_CALLBACK (location_weather_updated_cb), cd);
			g_object_set_data (G_OBJECT (loc), "weather-updated", GINT_TO_POINTER (id));
			g_signal_connect (loc, "set-current", 
					  G_CALLBACK (location_set_current_cb), cd);
		}
	}

	if (cd->map_widget)
		clock_map_refresh (CLOCK_MAP (cd->map_widget));

	if (cd->clock_vbox)
		create_cities_section (cd);
}

static void
show_week_changed (GSettings   *settings,
                   const gchar *key,
                   ClockApplet *clock)
{
	if (clock->calendar_popup != NULL) {
		calendar_window_set_show_weeks (CALENDAR_WINDOW (clock->calendar_popup),
                                                g_settings_get_boolean (settings, "show-weeks"));
                position_calendar_popup (clock);
	}
}

static void
load_cities (ClockApplet *cd)
{
        GVariantIter *iter;
        const char *name;
        const char *code;
        gboolean latlon_override;
        gdouble latitude, longitude;

        g_settings_get (cd->applet_settings, "cities", "a(ssm(dd))", &iter);

        while (g_variant_iter_loop (iter, "(&s&sm(dd))", &name, &code, &latlon_override,
                                    &latitude, &longitude)) {
                ClockLocation *loc;

                loc = clock_location_new (cd->wall_clock,
                                          cd->world,
                                          name, code,
                                          latlon_override, latitude, longitude);

                cd->locations = g_list_prepend (cd->locations, loc);
        }

        cd->locations = g_list_reverse (cd->locations);
}

static gboolean
fill_clock_applet (ClockApplet *cd)
{
        GpApplet *applet;
        GAction *action;

        applet = GP_APPLET (cd);

        cd->applet_settings = gp_applet_settings_new (applet, "org.gnome.gnome-panel.applet.clock");
        cd->clock_settings = g_settings_new ("org.gnome.desktop.interface");
        cd->weather_settings = g_settings_new ("org.gnome.GWeather4");

        g_signal_connect (cd->clock_settings, "changed::clock-format",
                          G_CALLBACK (format_changed), cd);
        g_signal_connect (cd->clock_settings, "changed::clock-show-weeks",
                          G_CALLBACK (show_week_changed), cd);
        g_signal_connect (cd->applet_settings, "changed::cities",
                          G_CALLBACK (locations_changed), cd);

        cd->wall_clock = g_object_new (GNOME_TYPE_WALL_CLOCK, NULL);

        cd->world = gweather_location_get_world ();
        load_cities (cd);
        locations_changed (NULL, NULL, cd);

	cd->builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (cd->builder, GETTEXT_PACKAGE);
	gtk_builder_add_from_resource (cd->builder, CLOCK_RESOURCE_PATH "clock.ui", NULL);

	create_clock_widget (cd);

	gp_applet_setup_menu_from_resource (applet,
	                                    CLOCK_RESOURCE_PATH "clock-menu.ui",
	                                    clock_menu_actions);

	action = gp_applet_menu_lookup_action (applet, "preferences");
	g_object_bind_property (cd, "locked-down",
				action, "enabled",
				G_BINDING_DEFAULT|G_BINDING_INVERT_BOOLEAN|G_BINDING_SYNC_CREATE);

	action = gp_applet_menu_lookup_action (applet, "config");
	g_object_bind_property (cd, "locked-down",
				action, "enabled",
				G_BINDING_DEFAULT|G_BINDING_INVERT_BOOLEAN|G_BINDING_SYNC_CREATE);

	gtk_widget_show (GTK_WIDGET (cd));

	return TRUE;
}

static void
prefs_locations_changed (GtkTreeSelection *selection,
                         ClockApplet      *cd)
{
        gint n;

        n = gtk_tree_selection_count_selected_rows (selection);
        gtk_widget_set_sensitive (cd->prefs_location_edit_button, n > 0);
        gtk_widget_set_sensitive (cd->prefs_location_remove_button, n > 0);
}

static GVariant *
location_serialize (ClockLocation *loc)
{
        gdouble lat, lon;

        clock_location_get_coords (loc, &lat, &lon);
        return g_variant_new ("(ssm(dd))",
                              clock_location_get_name (loc),
                              clock_location_get_weather_code (loc),
                              TRUE, lat, lon);
}

static void
save_cities_store (ClockApplet *cd)
{
        ClockLocation *loc;
        GVariantBuilder builder;
        GList *list;

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssm(dd))"));

        list = cd->locations;
        while (list) {
                loc = CLOCK_LOCATION (list->data);
                g_variant_builder_add_value (&builder,
                                             location_serialize (loc));

                list = list->next;
        }

        g_settings_set_value (cd->applet_settings, "cities",
                              g_variant_builder_end (&builder));

        create_cities_store (cd);
}

static void
edit_clear (ClockApplet *cd)
{
        GtkWidget *lat_entry = _clock_get_widget (cd, "edit-location-latitude-entry");
        GtkWidget *lon_entry = _clock_get_widget (cd, "edit-location-longitude-entry");
        GtkWidget *lat_combo = _clock_get_widget (cd, "edit-location-latitude-combo");
        GtkWidget *lon_combo = _clock_get_widget (cd, "edit-location-longitude-combo");

        /* clear out the old data */
        clock_location_entry_set_location (cd->location_entry, NULL);

        gtk_entry_set_text (GTK_ENTRY (lat_entry), "");
        gtk_entry_set_text (GTK_ENTRY (lon_entry), "");

        gtk_combo_box_set_active (GTK_COMBO_BOX (lat_combo), -1);
        gtk_combo_box_set_active (GTK_COMBO_BOX (lon_combo), -1);
}

static void
edit_hide (GtkWidget   *unused,
           ClockApplet *applet)
{
        GtkWidget *edit_window;

        edit_window = _clock_get_widget (applet, "edit-location-window");
        gtk_widget_hide (edit_window);

        edit_clear (applet);
}

static GWeatherLocation *
get_weather_station_location (GWeatherLocation *location)
{
        GWeatherLocation *station_loc;

        /* According to the documentation, the parent of a detached location
         * is the nearest weather station.
         */
        if (gweather_location_get_level (location) == GWEATHER_LOCATION_DETACHED) {
                station_loc = gweather_location_get_parent (location);
                g_assert (station_loc != NULL);
        } else {
                station_loc = g_object_ref (location);
        }

        while (gweather_location_get_level (station_loc) < GWEATHER_LOCATION_WEATHER_STATION) {
                GWeatherLocation *tmp;

                tmp = station_loc;

                station_loc = gweather_location_next_child (station_loc, NULL);
                g_assert (station_loc != NULL);

                g_object_unref (tmp);
        }

        return station_loc;
}

static void
run_prefs_edit_save (GtkButton   *button,
                     ClockApplet *cd)
{
        GtkWidget *edit_window = _clock_get_widget (cd, "edit-location-window");

        ClockLocation *loc = g_object_get_data (G_OBJECT (edit_window), "clock-location");

        GtkWidget *lat_entry = _clock_get_widget (cd, "edit-location-latitude-entry");
        GtkWidget *lon_entry = _clock_get_widget (cd, "edit-location-longitude-entry");
        GtkWidget *lat_combo = _clock_get_widget (cd, "edit-location-latitude-combo");
        GtkWidget *lon_combo = _clock_get_widget (cd, "edit-location-longitude-combo");

        const char *weather_code;
        gchar *city, *name;

        GWeatherLocation *gloc, *station_loc;
        gfloat lat = 0;
        gfloat lon = 0;

        if (loc) {
                cd->locations = g_list_remove (cd->locations, loc);
                g_object_unref (loc);
        }

        city = NULL;
        weather_code = NULL;
        name = NULL;

        gloc = clock_location_entry_get_location (cd->location_entry);
        if (!gloc) {
                edit_hide (NULL, cd);
                return;
        }

        station_loc = get_weather_station_location (gloc);
        g_object_unref (gloc);

        weather_code = gweather_location_get_code (station_loc);
        g_object_unref (station_loc);

        if (clock_location_entry_has_custom_text (cd->location_entry)) {
                name = gtk_editable_get_chars (GTK_EDITABLE (cd->location_entry), 0, -1);
        }

        sscanf (gtk_entry_get_text (GTK_ENTRY (lat_entry)), "%f", &lat);
        sscanf (gtk_entry_get_text (GTK_ENTRY (lon_entry)), "%f", &lon);

        if (gtk_combo_box_get_active (GTK_COMBO_BOX (lat_combo)) != 0) {
                lat = -lat;
        }

        if (gtk_combo_box_get_active (GTK_COMBO_BOX (lon_combo)) != 0) {
                lon = -lon;
        }

        loc = clock_location_new (cd->wall_clock,
                                  cd->world,
                                  name,
                                  weather_code,
                                  TRUE,
                                  lat,
                                  lon);
        /* has the side-effect of setting the current location if
         * there's none and this one can be considered as a current one
         */
        clock_location_is_current (loc);

        cd->locations = g_list_append (cd->locations, loc);

        g_free (city);

	/* This will update everything related to locations to take into
	 * account the new location (via the gconf notification) */
        save_cities_store (cd);

        edit_hide (edit_window, cd);
}

static void
update_coords_helper (gdouble    value,
                      GtkWidget *entry,
                      GtkWidget *combo)
{
        gchar *tmp;

        tmp = g_strdup_printf ("%f", fabs (value));
        gtk_entry_set_text (GTK_ENTRY (entry), tmp);
        g_free (tmp);

        if (value > 0) {
                gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
        } else {
                gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 1);
        }
}

static void
update_coords (ClockApplet *cd,
               gboolean     valid,
               gdouble      lat,
               gdouble      lon)
{
        GtkWidget *lat_entry = _clock_get_widget (cd, "edit-location-latitude-entry");
        GtkWidget *lon_entry = _clock_get_widget (cd, "edit-location-longitude-entry");
        GtkWidget *lat_combo = _clock_get_widget (cd, "edit-location-latitude-combo");
        GtkWidget *lon_combo = _clock_get_widget (cd, "edit-location-longitude-combo");

	if (!valid) {
        	gtk_entry_set_text (GTK_ENTRY (lat_entry), "");
        	gtk_entry_set_text (GTK_ENTRY (lon_entry), "");
                gtk_combo_box_set_active (GTK_COMBO_BOX (lat_combo), -1);
                gtk_combo_box_set_active (GTK_COMBO_BOX (lon_combo), -1);

		return;
	}

	update_coords_helper (lat, lat_entry, lat_combo);
	update_coords_helper (lon, lon_entry, lon_combo);
}

static void
location_update_ok_sensitivity (ClockApplet *cd)
{
	GtkWidget *ok_button;
        gchar *name;

        ok_button = _clock_get_widget (cd, "edit-location-ok-button");

        name = gtk_editable_get_chars (GTK_EDITABLE (cd->location_entry), 0, -1);

        if (name && name[0] != '\0') {
                gtk_widget_set_sensitive (ok_button, TRUE);
        } else {
                gtk_widget_set_sensitive (ok_button, FALSE);
        }

        g_free (name);
}

static void
location_changed (GObject     *object,
                  GParamSpec  *param,
                  ClockApplet *cd)
{
        ClockLocationEntry *entry = CLOCK_LOCATION_ENTRY (object);
        GWeatherLocation *gloc;
        gboolean latlon_valid;
        double latitude = 0.0, longitude = 0.0;

        gloc = clock_location_entry_get_location (entry);

	latlon_valid = gloc && gweather_location_has_coords (gloc);
        if (latlon_valid)
                gweather_location_get_coords (gloc, &latitude, &longitude);
        update_coords (cd, latlon_valid, latitude, longitude);

        if (gloc)
                g_object_unref (gloc);
}

static void
location_name_changed (GObject     *object,
                       ClockApplet *cd)
{
    location_update_ok_sensitivity (cd);
}

static gboolean
edit_delete (GtkWidget   *unused,
             GdkEvent    *event,
             ClockApplet *cd)
{
	edit_hide (unused, cd);

	return TRUE;
}

static gboolean
edit_hide_event (GtkWidget *widget, GdkEvent *event, ClockApplet *cd)
{
        edit_hide (widget, cd);

        return TRUE;
}

static void
prefs_hide (GtkWidget   *widget,
            ClockApplet *cd)
{
        GtkWidget *tree;

	edit_hide (widget, cd);

	gtk_widget_hide (cd->prefs_window);

	tree = _clock_get_widget (cd, "cities_list");

        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree)));
}

static gboolean
prefs_hide_event (GtkWidget   *widget,
                  GdkEvent    *event,
                  ClockApplet *cd)
{
        prefs_hide (widget, cd);

        return TRUE;
}

static void
prefs_help (GtkWidget   *widget,
            ClockApplet *cd)
{
        gp_applet_show_help (GP_APPLET (cd), "clock-settings");
}

static void
remove_tree_row (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
        ClockApplet *cd = data;
        ClockLocation *loc = NULL;

        gtk_tree_model_get (model, iter, COL_CITY_LOC, &loc, -1);
	cd->locations = g_list_remove (cd->locations, loc);
	g_object_unref (loc);

	/* This will update everything related to locations to take into
	 * account the removed location (via the gconf notification) */
        save_cities_store (cd);
}

static void
run_prefs_locations_remove (GtkButton   *button,
                            ClockApplet *cd)
{
        GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (cd->prefs_locations));

        gtk_tree_selection_selected_foreach (sel, remove_tree_row, cd);
}

static void
run_prefs_locations_add (GtkButton   *button,
                         ClockApplet *cd)
{
        GtkWidget *edit_window = _clock_get_widget (cd, "edit-location-window");

        g_object_set_data (G_OBJECT (edit_window), "clock-location", NULL);
        gtk_window_set_title (GTK_WINDOW (edit_window), _("Choose Location"));
        gtk_window_set_transient_for (GTK_WINDOW (edit_window), GTK_WINDOW (cd->prefs_window));

	if (g_object_get_data (G_OBJECT (edit_window), "delete-handler") == NULL) {
		g_object_set_data (G_OBJECT (edit_window), "delete-handler",
				   GINT_TO_POINTER (g_signal_connect (edit_window, "delete_event", G_CALLBACK (edit_delete), cd)));
	}

        location_update_ok_sensitivity (cd);

	gtk_widget_grab_focus (GTK_WIDGET (cd->location_entry));
	gtk_editable_set_position (GTK_EDITABLE (cd->location_entry), -1);

        gtk_window_present_with_time (GTK_WINDOW (edit_window), gtk_get_current_event_time ());
}

static void
edit_tree_row (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
        ClockApplet *cd = data;
        ClockLocation *loc;
        const char *name;
        gchar *tmp;
        gdouble lat, lon;

        /* fill the dialog with this location's data, show it */
        GtkWidget *edit_window = _clock_get_widget (cd, "edit-location-window");

        GtkWidget *lat_entry = _clock_get_widget (cd, "edit-location-latitude-entry");

        GtkWidget *lon_entry = _clock_get_widget (cd, "edit-location-longitude-entry");

        GtkWidget *lat_combo = _clock_get_widget (cd, "edit-location-latitude-combo");

        GtkWidget *lon_combo = _clock_get_widget (cd, "edit-location-longitude-combo");

        edit_clear (cd);

        gtk_tree_model_get (model, iter, COL_CITY_LOC, &loc, -1);

        clock_location_entry_set_city (cd->location_entry,
                                       clock_location_get_city (loc),
                                       clock_location_get_weather_code (loc));
	name = clock_location_get_name (loc);
        if (name && name[0]) {
                gtk_entry_set_text (GTK_ENTRY (cd->location_entry), name);
	}

        clock_location_get_coords (loc, &lat, &lon);

        tmp = g_strdup_printf ("%f", fabs (lat));
        gtk_entry_set_text (GTK_ENTRY (lat_entry), tmp);
        g_free (tmp);

        if (lat > 0) {
                gtk_combo_box_set_active (GTK_COMBO_BOX (lat_combo), 0);
        } else {
                gtk_combo_box_set_active (GTK_COMBO_BOX (lat_combo), 1);
        }

        tmp = g_strdup_printf ("%f", fabs (lon));
        gtk_entry_set_text (GTK_ENTRY (lon_entry), tmp);
        g_free (tmp);

        if (lon > 0) {
                gtk_combo_box_set_active (GTK_COMBO_BOX (lon_combo), 0);
        } else {
                gtk_combo_box_set_active (GTK_COMBO_BOX (lon_combo), 1);
        }

        location_update_ok_sensitivity (cd);

        g_object_set_data (G_OBJECT (edit_window), "clock-location", loc);

	gtk_widget_grab_focus (GTK_WIDGET (cd->location_entry));
	gtk_editable_set_position (GTK_EDITABLE (cd->location_entry), -1);

        gtk_window_set_title (GTK_WINDOW (edit_window), _("Edit Location"));
        gtk_window_present (GTK_WINDOW (edit_window));
}

static void
run_prefs_locations_edit (GtkButton   *unused,
                          ClockApplet *cd)
{
        GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (cd->prefs_locations));

        gtk_tree_selection_selected_foreach (sel, edit_tree_row, cd);
}

static void
set_12hr_format_radio_cb (GtkWidget   *widget,
                          ClockApplet *cd)
{
        GDesktopClockFormat format;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
                format = G_DESKTOP_CLOCK_FORMAT_12H;
        else
                format = G_DESKTOP_CLOCK_FORMAT_24H;

        g_settings_set_enum (cd->clock_settings, "clock-format", format);
}

static void
fill_prefs_window (ClockApplet *cd)
{
        struct int_char_pair {
                int v;
                const char *c;
        };

        static const struct int_char_pair temperatures[] = {
                { GWEATHER_TEMP_UNIT_DEFAULT, N_("Default") },
                { GWEATHER_TEMP_UNIT_KELVIN, N_("Kelvin") },
                { GWEATHER_TEMP_UNIT_CENTIGRADE, N_("Celsius") },
                { GWEATHER_TEMP_UNIT_FAHRENHEIT, N_("Fahrenheit") },
                { -1 }
        };

        static const struct int_char_pair speeds[] = {
                { GWEATHER_SPEED_UNIT_DEFAULT, N_("Default") },
                { GWEATHER_SPEED_UNIT_MS, N_("Meters per second (m/s)") },
                { GWEATHER_SPEED_UNIT_KPH, N_("Kilometers per hour (kph)") },
                { GWEATHER_SPEED_UNIT_MPH, N_("Miles per hour (mph)") },
                { GWEATHER_SPEED_UNIT_KNOTS, N_("Knots") },
                { GWEATHER_SPEED_UNIT_BFT, N_("Beaufort scale") },
                { -1 }
        };

        GtkWidget *radio_12hr;
        GtkWidget *radio_24hr;
	GtkWidget *widget;
	GtkCellRenderer *renderer;
        GtkTreeViewColumn *col;
	GtkListStore *store;
        GtkTreeIter iter;
        GEnumClass *enum_class;
        int i;

	/* Set the 12 hour / 24 hour widget */
        radio_12hr = _clock_get_widget (cd, "12hr_radio");
        radio_24hr = _clock_get_widget (cd, "24hr_radio");

        if (g_settings_get_enum (cd->clock_settings, "clock-format") ==
            G_DESKTOP_CLOCK_FORMAT_12H)
                widget = radio_12hr;
        else
                widget = radio_24hr;

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	g_signal_connect (radio_12hr, "toggled",
			  G_CALLBACK (set_12hr_format_radio_cb), cd);

	/* Set the "Show Date" checkbox */
	widget = _clock_get_widget (cd, "date_check");
        g_settings_bind (cd->clock_settings, "clock-show-date", widget, "active",
                         G_SETTINGS_BIND_DEFAULT);

	/* Set the "Show Seconds" checkbox */
	widget = _clock_get_widget (cd, "seconds_check");
        g_settings_bind (cd->clock_settings, "clock-show-seconds", widget, "active",
                         G_SETTINGS_BIND_DEFAULT);

	/* Set the "Show weather" checkbox */
	widget = _clock_get_widget (cd, "weather_check");
        g_settings_bind (cd->applet_settings, "show-weather", widget, "active",
                         G_SETTINGS_BIND_DEFAULT);

	/* Set the "Show temperature" checkbox */
	widget = _clock_get_widget (cd, "temperature_check");
        g_settings_bind (cd->applet_settings, "show-temperature", widget, "active",
                         G_SETTINGS_BIND_DEFAULT);

	/* Fill the Cities list */
	widget = _clock_get_widget (cd, "cities_list");

	renderer = gtk_cell_renderer_text_new ();
        col = gtk_tree_view_column_new_with_attributes (_("City Name"), renderer, "text", COL_CITY_NAME, NULL);
        gtk_tree_view_insert_column (GTK_TREE_VIEW (widget), col, -1);

	renderer = gtk_cell_renderer_text_new ();
        col = gtk_tree_view_column_new_with_attributes (_("City Time Zone"), renderer, "text", COL_CITY_TZ, NULL);
        gtk_tree_view_insert_column (GTK_TREE_VIEW (widget), col, -1);
	
	if (cd->cities_store == NULL)
		create_cities_store (cd);

        gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
                                 GTK_TREE_MODEL (cd->cities_store));

        /* Temperature combo */
	widget = _clock_get_widget (cd, "temperature_combo");
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));
        gtk_combo_box_set_id_column (GTK_COMBO_BOX (widget), 0);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), renderer, "text", 1, NULL);

        enum_class = g_type_class_ref (GWEATHER_TYPE_TEMPERATURE_UNIT);
        for (i = 0; temperatures[i].v != -1; i++)
                gtk_list_store_insert_with_values (store, &iter, -1,
                                                   0, g_enum_get_value (enum_class, temperatures[i].v)->value_nick,
                                                   1, gettext (temperatures[i].c),
                                                   -1);
        g_type_class_unref (enum_class);

        g_settings_bind (cd->weather_settings, "temperature-unit", widget, "active-id",
                         G_SETTINGS_BIND_DEFAULT);

        /* Wind speed combo */
	widget = _clock_get_widget (cd, "wind_speed_combo");
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));
        gtk_combo_box_set_id_column (GTK_COMBO_BOX (widget), 0);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), renderer, "text", 1, NULL);

        enum_class = g_type_class_ref (GWEATHER_TYPE_SPEED_UNIT);
        for (i = 0; speeds[i].v != -1; i++)
                gtk_list_store_insert_with_values (store, &iter, -1,
                                                   0, g_enum_get_value (enum_class, speeds[i].v)->value_nick,
                                                   1, gettext (speeds[i].c),
                                                   -1);
        g_type_class_unref (enum_class);

        g_settings_bind (cd->weather_settings, "speed-unit", widget, "active-id",
                         G_SETTINGS_BIND_DEFAULT);
}

static void
ensure_prefs_window_is_created (ClockApplet *cd)
{
        GtkWidget *edit_window;
	GtkWidget *prefs_close_button;
	GtkWidget *prefs_help_button;
	GtkWidget *clock_options;
        GtkWidget *edit_cancel_button;
        GtkWidget *edit_ok_button;
        GtkWidget *location_box;
        GtkWidget *location_name_label;
        GtkTreeSelection *selection;

        if (cd->prefs_window)
                return;

        cd->prefs_window = _clock_get_widget (cd, "prefs-window");

	gtk_window_set_icon_name (GTK_WINDOW (cd->prefs_window), CLOCK_ICON);

        prefs_close_button = _clock_get_widget (cd, "prefs-close-button");
        prefs_help_button = _clock_get_widget (cd, "prefs-help-button");
        clock_options = _clock_get_widget (cd, "clock-options");
        cd->prefs_locations = GTK_TREE_VIEW (_clock_get_widget (cd, "cities_list"));
        location_name_label = _clock_get_widget (cd, "location-name-label");

	if (!clock_locale_supports_am_pm ())
		gtk_widget_hide (clock_options);

        selection = gtk_tree_view_get_selection (cd->prefs_locations);
        g_signal_connect (G_OBJECT (selection), "changed",
                          G_CALLBACK (prefs_locations_changed), cd);

        g_signal_connect (G_OBJECT (cd->prefs_window), "delete_event",
                          G_CALLBACK (prefs_hide_event), cd);

        g_signal_connect (G_OBJECT (prefs_close_button), "clicked",
                          G_CALLBACK (prefs_hide), cd);

        g_signal_connect (G_OBJECT (prefs_help_button), "clicked",
                          G_CALLBACK (prefs_help), cd);

        cd->prefs_location_remove_button = _clock_get_widget (cd, "prefs-locations-remove-button");

        g_signal_connect (G_OBJECT (cd->prefs_location_remove_button), "clicked",
                          G_CALLBACK (run_prefs_locations_remove), cd);

        cd->prefs_location_add_button = _clock_get_widget (cd, "prefs-locations-add-button");

        g_signal_connect (G_OBJECT (cd->prefs_location_add_button), "clicked",
                          G_CALLBACK (run_prefs_locations_add), cd);

        cd->prefs_location_edit_button = _clock_get_widget (cd, "prefs-locations-edit-button");

        g_signal_connect (G_OBJECT (cd->prefs_location_edit_button), "clicked",
                          G_CALLBACK (run_prefs_locations_edit), cd);

        edit_window = _clock_get_widget (cd, "edit-location-window");

        gtk_window_set_transient_for (GTK_WINDOW (edit_window),
                                      GTK_WINDOW (cd->prefs_window));

        g_signal_connect (G_OBJECT (edit_window), "delete_event",
                          G_CALLBACK (edit_hide_event), cd);

        edit_cancel_button = _clock_get_widget (cd, "edit-location-cancel-button");

        edit_ok_button = _clock_get_widget (cd, "edit-location-ok-button");

        location_box = _clock_get_widget (cd, "edit-location-name-box");
        cd->location_entry = CLOCK_LOCATION_ENTRY (clock_location_entry_new (cd->world));
        gtk_widget_show (GTK_WIDGET (cd->location_entry));
        gtk_container_add (GTK_CONTAINER (location_box), GTK_WIDGET (cd->location_entry));
        gtk_label_set_mnemonic_widget (GTK_LABEL (location_name_label),
                                       GTK_WIDGET (cd->location_entry));

        g_signal_connect (G_OBJECT (cd->location_entry), "notify::location",
                          G_CALLBACK (location_changed), cd);
        g_signal_connect (G_OBJECT (cd->location_entry), "changed",
                          G_CALLBACK (location_name_changed), cd);

        g_signal_connect (G_OBJECT (edit_cancel_button), "clicked",
                          G_CALLBACK (edit_hide), cd);

        g_signal_connect (G_OBJECT (edit_ok_button), "clicked",
                          G_CALLBACK (run_prefs_edit_save), cd);

        /* Set up the time setting section */

        cd->time_settings_button = _clock_get_widget (cd, "time-settings-button");
        g_signal_connect (cd->time_settings_button, "clicked",
                          G_CALLBACK (run_time_settings), cd);

        /* fill it with the current preferences */
        fill_prefs_window (cd);
}

static void
display_properties_dialog (ClockApplet *cd,
                           gboolean     start_in_locations_page)
{
        ensure_prefs_window_is_created (cd);

        if (start_in_locations_page) {
                GtkWidget *notebook = _clock_get_widget (cd, "notebook");
                gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 1);
        }

	update_set_time_button (cd);

        gtk_window_set_screen (GTK_WINDOW (cd->prefs_window),
                               gtk_widget_get_screen (GTK_WIDGET (cd)));
	gtk_window_present (GTK_WINDOW (cd->prefs_window));
}

static void
clock_applet_constructed (GObject *object)
{
        G_OBJECT_CLASS (clock_applet_parent_class)->constructed (object);

        fill_clock_applet (CLOCK_APPLET (object));
}

static void
clock_applet_dispose (GObject *object)
{
        ClockApplet *applet;

        applet = CLOCK_APPLET (object);

        g_clear_object (&applet->applet_settings);
        g_clear_object (&applet->clock_settings);
        g_clear_object (&applet->weather_settings);

        g_clear_object (&applet->wall_clock);

        g_clear_pointer (&applet->calendar_popup, gtk_widget_destroy);

        g_clear_object (&applet->datetime_appinfo);

        g_clear_object (&applet->world);

        free_locations (applet);

        if (applet->location_tiles != NULL) {
                g_list_free (applet->location_tiles);
                applet->location_tiles = NULL;
        }

        g_clear_object (&applet->cities_store);
        g_clear_object (&applet->builder);

        G_OBJECT_CLASS (clock_applet_parent_class)->dispose (object);
}

static void
clock_applet_placement_changed (GpApplet        *applet,
                                GtkOrientation   orientation,
                                GtkPositionType  position)
{
        ClockApplet *self;

        self = CLOCK_APPLET (applet);

        clock_button_set_orientation (CLOCK_BUTTON (self->panel_button), orientation);
        clock_button_set_position (CLOCK_BUTTON (self->panel_button), position);

        update_calendar_popup (self);
}

static void
clock_applet_class_init (ClockAppletClass *clock_class)
{
	GObjectClass *object_class;
	GpAppletClass *applet_class;

	object_class = G_OBJECT_CLASS (clock_class);
	applet_class = GP_APPLET_CLASS (clock_class);

	object_class->constructed = clock_applet_constructed;
	object_class->dispose = clock_applet_dispose;

	applet_class->placement_changed = clock_applet_placement_changed;
}

static void
clock_applet_init (ClockApplet *applet)
{
        gp_applet_set_flags (GP_APPLET (applet), GP_APPLET_FLAGS_EXPAND_MINOR);
}
