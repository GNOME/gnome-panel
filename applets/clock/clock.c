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

#include <panel-applet.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-wall-clock.h>

#include <libgweather/location-entry.h>
#include <libgweather/timezone-menu.h>
#include <libgweather/gweather-enum-types.h>

#include "clock-common.h"
#include "calendar-window.h"
#include "clock-location.h"
#include "clock-location-edit.h"
#include "clock-preferences.h"
#include "clock-location-tile.h"
#include "clock-map.h"
#include "clock-utils.h"
#include "timedate1.h"
#include "system-timezone.h"

typedef struct _ClockData ClockData;

struct _ClockData {
	/* widgets */
	GtkWidget *applet;

        GtkWidget *panel_button;	/* main toggle button for the whole clock */

	GtkWidget *main_obox;		/* orientable box inside panel_button */
        GtkWidget *weather_obox;        /* orientable box for the weather widgets */

	GtkWidget *clockw;		/* main label for the date/time display */

        GtkWidget *panel_weather_icon;
        GtkWidget *panel_temperature_label;

	GtkWidget *props;
	GtkWidget *calendar_popup;

        GtkWidget *clock_vbox;
	GtkSizeGroup *clock_group;

	GtkBuilder *builder;

        /* Preferences dialog */
	GWeatherLocationEntry *location_entry;
        GWeatherTimezoneMenu *zone_combo;

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
        time_t             current_time;
        GnomeWallClock    *wall_clock;
	PanelAppletOrient  orient;
	GtkAllocation      old_allocation;

	SystemTimezone *systz;

        GtkWidget *showseconds_check;
        GtkWidget *showdate_check;
        GtkWidget *custom_hbox;
        GtkWidget *custom_label;
        GtkWidget *custom_entry;
        gboolean   custom_format_shown;

	gboolean   can_handle_format_12;

        Timedate1 *timedate1;
};

static void  update_clock (GnomeWallClock *, GParamSpec *, ClockData * cd);
static void  update_tooltip (ClockData * cd);
static void  update_panel_weather (ClockData *cd);

static void set_atk_name_description (GtkWidget *widget,
                                      const char *name,
                                      const char *desc);
static void verb_display_properties_dialog (GSimpleAction *action,
                                            GVariant      *parameter,
                                            gpointer       user_data);

static void display_properties_dialog (ClockData  *cd,
                                       gboolean    start_in_locations_page);
static void position_calendar_popup   (ClockData  *cd);
static void update_orient (ClockData *cd);
static void applet_change_orient (PanelApplet       *applet,
				  PanelAppletOrient  orient,
				  ClockData         *cd);

/* ClockBox, an instantiable GtkBox */

typedef GtkBox      ClockBox;
typedef GtkBoxClass ClockBoxClass;

static GType clock_box_get_type (void);

G_DEFINE_TYPE (ClockBox, clock_box, GTK_TYPE_BOX)

static void
clock_box_init (ClockBox *box)
{
}

static void
clock_box_class_init (ClockBoxClass *klass)
{
}

/* Clock */

static inline GtkWidget *
_clock_get_widget (ClockData  *cd,
		   const char *name)
{
	return GTK_WIDGET (gtk_builder_get_object (cd->builder, name));
}

static int
calculate_minimum_width (GtkWidget   *widget,
			 const gchar *text)
{
	PangoContext *pango_context;
	PangoLayout  *layout;
	int	      width, height;
	int	      focus_width = 0;
	int	      focus_pad = 0;
	GtkStyleContext *style_context;
	GtkStateFlags    state;
	GtkBorder        padding;

	pango_context = gtk_widget_get_pango_context (widget);

	layout = pango_layout_new (pango_context);
	pango_layout_set_alignment (layout, PANGO_ALIGN_LEFT);
	pango_layout_set_text (layout, text, -1);
	pango_layout_get_pixel_size (layout, &width, &height);
	g_object_unref (G_OBJECT (layout));
	layout = NULL;

	state = gtk_widget_get_state_flags (widget);
	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_get_padding (style_context, state, &padding);
	gtk_style_context_get_style (style_context,
			             "focus-line-width", &focus_width,
			             "focus-padding", &focus_pad,
			             NULL);

	width += 2 * (focus_width + focus_pad) + padding.left + padding.right;

	return width;
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
update_clock (GnomeWallClock *wall_clock, GParamSpec *pspec, ClockData * cd)
{
        const char *clock;

        clock = gnome_wall_clock_get_clock (cd->wall_clock);
        gtk_label_set_text (GTK_LABEL (cd->clockw), clock);

	update_orient (cd);
	gtk_widget_queue_resize (cd->panel_button);

	update_tooltip (cd);

        if (cd->map_widget && cd->calendar_popup && gtk_widget_get_visible (cd->calendar_popup))
                clock_map_update_time (CLOCK_MAP (cd->map_widget));
}

static void
update_tooltip (ClockData * cd)
{
        gboolean show_date;

        show_date = g_settings_get_boolean (cd->clock_settings, KEY_CLOCK_SHOW_DATE);
        if (!show_date) {
                GDateTime *dt;
                char *tip, *format;

                dt = g_date_time_new_now_local ();
		/* Translators: This is a strftime format string.
		 * It is used to display a date. Please leave "%%s" as it is:
		 * it will be used to insert the timezone name later. */
                format = g_date_time_format (dt, _("%A %B %d (%%s)"));
                tip = g_strdup_printf (format, g_date_time_get_timezone_abbreviation (dt));

                gtk_widget_set_tooltip_text (cd->panel_button, tip);

                g_date_time_unref (dt);
                g_free (format);
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
free_locations (ClockData *cd)
{
        GList *l;

        for (l = cd->locations; l; l = l->next)
                g_object_unref (l->data);

        g_list_free (cd->locations);
        cd->locations = NULL;
}

static void
destroy_clock (GtkWidget * widget, ClockData *cd)
{
        g_clear_object (&cd->timedate1);

        g_clear_object (&cd->applet_settings);
        g_clear_object (&cd->clock_settings);
        g_clear_object (&cd->weather_settings);

        g_clear_object (&cd->wall_clock);

	if (cd->props)
		gtk_widget_destroy (cd->props);
        cd->props = NULL;

	if (cd->calendar_popup)
		gtk_widget_destroy (cd->calendar_popup);
	cd->calendar_popup = NULL;

        free_locations (cd);

        g_list_free (cd->location_tiles);
        cd->location_tiles = NULL;

	if (cd->builder) {
		g_object_unref (cd->builder);
		cd->builder = NULL;
	}

	g_free (cd);
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
        ClockData *cd;

        cd = data;

        display_properties_dialog (cd, TRUE);
}

static GtkWidget *
create_calendar (ClockData *cd)
{
	GtkWidget *window;

	window = calendar_window_new (cd->applet_settings,
				      cd->orient == PANEL_APPLET_ORIENT_UP);

	g_object_bind_property (cd->applet, "locked-down",
				window, "locked-down",
				G_BINDING_DEFAULT|G_BINDING_SYNC_CREATE);

	calendar_window_set_show_weeks (CALENDAR_WINDOW (window),
				        g_settings_get_boolean (cd->applet_settings, KEY_SHOW_WEEKS));
	calendar_window_set_time_format (CALENDAR_WINDOW (window),
					 g_settings_get_enum (cd->clock_settings, KEY_CLOCK_FORMAT));

        gtk_window_set_screen (GTK_WINDOW (window),
			       gtk_widget_get_screen (cd->applet));

        g_signal_connect (window, "edit-locations",
                          G_CALLBACK (edit_locations_cb), cd);

	g_signal_connect (window, "delete_event",
			  G_CALLBACK (delete_event), cd->panel_button);
	g_signal_connect (window, "key_press_event",
			  G_CALLBACK (close_on_escape), cd->panel_button);

	return window;
}

static void
position_calendar_popup (ClockData *cd)
{
	GtkRequisition  req;
	GtkAllocation   allocation;
	GdkScreen      *screen;
	GdkRectangle    monitor;
	GdkGravity      gravity = GDK_GRAVITY_NORTH_WEST;
	int             button_w, button_h;
	int             x, y;
	int             w, h;
	int             i, n;
	gboolean        found_monitor = FALSE;

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

	screen = gtk_window_get_screen (GTK_WINDOW (cd->calendar_popup));

	n = gdk_screen_get_n_monitors (screen);
	for (i = 0; i < n; i++) {
		gdk_screen_get_monitor_geometry (screen, i, &monitor);
		if (x >= monitor.x && x <= monitor.x + monitor.width &&
		    y >= monitor.y && y <= monitor.y + monitor.height) {
			found_monitor = TRUE;
			break;
		}
	}

	if (!found_monitor) {
		/* eek, we should be on one of those xinerama
		   monitors */
		monitor.x = 0;
		monitor.y = 0;
		monitor.width = gdk_screen_get_width (screen);
		monitor.height = gdk_screen_get_height (screen);
	}

	/* Based on panel orientation, position the popup.
	 * Ignore window gravity since the window is undecorated.
	 * The orientations are all named backward from what
	 * I expected.
	 */
	switch (cd->orient) {
	case PANEL_APPLET_ORIENT_RIGHT:
		x += button_w;
		if ((y + h) > monitor.y + monitor.height)
			y -= (y + h) - (monitor.y + monitor.height);

		if ((y + h) > (monitor.height / 2))
			gravity = GDK_GRAVITY_SOUTH_WEST;
		else
			gravity = GDK_GRAVITY_NORTH_WEST;

		break;
	case PANEL_APPLET_ORIENT_LEFT:
		x -= w;
		if ((y + h) > monitor.y + monitor.height)
			y -= (y + h) - (monitor.y + monitor.height);

		if ((y + h) > (monitor.height / 2))
			gravity = GDK_GRAVITY_SOUTH_EAST;
		else
			gravity = GDK_GRAVITY_NORTH_EAST;

		break;
	case PANEL_APPLET_ORIENT_DOWN:
		y += button_h;
		if ((x + w) > monitor.x + monitor.width)
			x -= (x + w) - (monitor.x + monitor.width);

		gravity = GDK_GRAVITY_NORTH_WEST;

		break;
	case PANEL_APPLET_ORIENT_UP:
		y -= h;
		if ((x + w) > monitor.x + monitor.width)
			x -= (x + w) - (monitor.x + monitor.width);

		gravity = GDK_GRAVITY_SOUTH_WEST;

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
create_clock_window (ClockData *cd)
{
	GtkWidget *locations_box;

        locations_box = calendar_window_get_locations_box (CALENDAR_WINDOW (cd->calendar_popup));
        gtk_widget_show (locations_box);

	cd->clock_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add (GTK_CONTAINER (locations_box), cd->clock_vbox);

	cd->clock_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_set_ignore_hidden (cd->clock_group, FALSE);

	gtk_container_foreach (GTK_CONTAINER (locations_box),
			       (GtkCallback) add_to_group,
			       cd->clock_group);
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
location_tile_pressed_cb (ClockLocationTile *tile,
                          ClockLocation     *loc,
                          gpointer           data)
{
        ClockData *cd = data;

        clock_map_blink_location (CLOCK_MAP (cd->map_widget), loc);
}

static void
create_cities_section (ClockData *cd)
{
        GList *node;
        GtkWidget *city;
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

                city = clock_location_tile_new (loc);
                g_signal_connect (city, "tile-pressed",
                                  G_CALLBACK (location_tile_pressed_cb), cd);

                gtk_box_pack_start (GTK_BOX (cd->cities_section),
                                    GTK_WIDGET (city),
                                    FALSE, FALSE, 0);

                cd->location_tiles = g_list_prepend (cd->location_tiles, city);

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
        ClockData *cd = data;

        return cd->locations;
}

static void
create_map_section (ClockData *cd)
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
update_calendar_popup (ClockData *cd)
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
                 ClockData *cd)
{
	/* if time is wrong, the user might try to fix it by clicking on the
	 * clock */
	update_clock (NULL, NULL, cd);
	update_calendar_popup (cd);
}

static gboolean
do_not_eat_button_press (GtkWidget      *widget,
                         GdkEventButton *event)
{
	if (event->button != 1)
		g_signal_stop_emission_by_name (widget, "button_press_event");

	return FALSE;
}

static void
clock_update_text_gravity (GtkWidget *label)
{
	PangoLayout  *layout;
	PangoContext *context;

	layout = gtk_label_get_layout (GTK_LABEL (label));
	context = pango_layout_get_context (layout);
	pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);
}

static inline void
force_no_focus_padding (GtkWidget *widget)
{
        static gboolean first_time = TRUE;
        GtkCssProvider  *provider;

        if (first_time) {
                provider = gtk_css_provider_new ();
                gtk_css_provider_load_from_data (provider,
                                         "#clock-applet-button {\n"
                                         " -GtkWidget-focus-line-width: 0px;\n"
                                         " -GtkWidget-focus-padding: 0px;\n"
					 "}",
                                         -1, NULL);
                gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
                                        GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
                g_object_unref (provider);

                first_time = FALSE;
        }

        gtk_widget_set_name (widget, "clock-applet-button");
}

static GtkWidget *
create_main_clock_button (void)
{
        GtkWidget *button;

        button = gtk_toggle_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

        force_no_focus_padding (button);

        return button;
}

static GtkWidget *
create_main_clock_label (ClockData *cd)
{
        GtkWidget *label;

        label = gtk_label_new (NULL);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
	clock_update_text_gravity (label);
	g_signal_connect (label, "screen-changed",
			  G_CALLBACK (clock_update_text_gravity),
			  NULL);

        return label;
}

static gboolean
weather_tooltip (GtkWidget   *widget,
                 gint         x,
                 gint         y,
                 gboolean     keyboard_mode,
                 GtkTooltip  *tooltip,
                 ClockData   *cd)
{
        GList *locations, *l;

        locations = cd->locations;

        for (l = locations; l; l = l->next) {
		ClockLocation *location = l->data;
                if (clock_location_is_current (location)) {
                        gboolean ok;

                        ok = clock_location_setup_weather_tooltip (location,
                                                                   tooltip);
                        if (ok == FALSE)
                                continue;

                        return TRUE;
                }
        }

        return FALSE;
}

static void
create_clock_widget (ClockData *cd)
{
        cd->wall_clock = g_object_new (GNOME_TYPE_WALL_CLOCK, NULL);
        g_signal_connect (cd->wall_clock, "notify::clock",
                          G_CALLBACK (update_clock), cd);

        /* Main toggle button */
        cd->panel_button = create_main_clock_button ();
	g_signal_connect (cd->panel_button, "button_press_event",
			  G_CALLBACK (do_not_eat_button_press), NULL);
	g_signal_connect (cd->panel_button, "toggled",
			  G_CALLBACK (toggle_calendar), cd);
	g_signal_connect (G_OBJECT (cd->panel_button), "destroy",
			  G_CALLBACK (destroy_clock),
			  cd);
        gtk_widget_show (cd->panel_button);

        /* Main orientable box */
        cd->main_obox = g_object_new (clock_box_get_type (), NULL);
        gtk_box_set_spacing (GTK_BOX (cd->main_obox), 12); /* spacing between weather and time */
        gtk_container_add (GTK_CONTAINER (cd->panel_button), cd->main_obox);
        gtk_widget_show (cd->main_obox);

        /* Weather orientable box */
        cd->weather_obox = g_object_new (clock_box_get_type (), NULL);
        gtk_box_set_spacing (GTK_BOX (cd->weather_obox), 2); /* spacing between weather icon and temperature */
        gtk_box_pack_start (GTK_BOX (cd->main_obox), cd->weather_obox, FALSE, FALSE, 0);
        gtk_widget_set_has_tooltip (cd->weather_obox, TRUE);
        g_signal_connect (cd->weather_obox, "query-tooltip",
                          G_CALLBACK (weather_tooltip), cd);

        /* Weather widgets */
        cd->panel_weather_icon = gtk_image_new ();
        gtk_box_pack_start (GTK_BOX (cd->weather_obox), cd->panel_weather_icon, FALSE, FALSE, 0);
        g_settings_bind (cd->applet_settings, KEY_SHOW_WEATHER, cd->panel_weather_icon, "visible",
                         G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

        cd->panel_temperature_label = gtk_label_new (NULL);
        gtk_box_pack_start (GTK_BOX (cd->weather_obox), cd->panel_temperature_label, FALSE, FALSE, 0);
        g_settings_bind (cd->applet_settings, KEY_SHOW_TEMPERATURE, cd->panel_temperature_label, "visible",
                         G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

        /* Main label for time display */
	cd->clockw = create_main_clock_label (cd);
        gtk_box_pack_start (GTK_BOX (cd->main_obox), cd->clockw, FALSE, FALSE, 0);
	gtk_widget_show (cd->clockw);

        /* Done! */

	set_atk_name_description (GTK_WIDGET (cd->applet), NULL,
	                          _("Computer Clock"));

	gtk_container_add (GTK_CONTAINER (cd->applet), cd->panel_button);
	gtk_container_set_border_width (GTK_CONTAINER (cd->applet), 0);

	cd->props = NULL;
	cd->orient = -1;

	update_panel_weather (cd);

	/* Refresh the clock so that it paints its first state */
        update_clock (NULL, NULL, cd);
	applet_change_orient (PANEL_APPLET (cd->applet),
			      panel_applet_get_orient (PANEL_APPLET (cd->applet)),
			      cd);
}

static void
update_orient (ClockData *cd)
{
	const gchar   *text;
	int            min_width;
	GtkAllocation  allocation;
	gdouble        new_angle;
	gdouble        angle;

	text = gtk_label_get_text (GTK_LABEL (cd->clockw));
	min_width = calculate_minimum_width (cd->panel_button, text);
	gtk_widget_get_allocation (cd->panel_button, &allocation);

	if (cd->orient == PANEL_APPLET_ORIENT_LEFT &&
	    min_width > allocation.width)
		new_angle = 270;
	else if (cd->orient == PANEL_APPLET_ORIENT_RIGHT &&
		 min_width > allocation.width)
		new_angle = 90;
	else
		new_angle = 0;

	angle = gtk_label_get_angle (GTK_LABEL (cd->clockw));
	if (angle != new_angle) {
		gtk_label_set_angle (GTK_LABEL (cd->clockw), new_angle);
                gtk_label_set_angle (GTK_LABEL (cd->panel_temperature_label), new_angle);
	}
}

/* this is when the panel orientation changes */
static void
applet_change_orient (PanelApplet       *applet,
		      PanelAppletOrient  orient,
		      ClockData         *cd)
{
        GtkOrientation o;

	if (orient == cd->orient)
		return;

        cd->orient = orient;

	switch (cd->orient) {
        case PANEL_APPLET_ORIENT_RIGHT:
                o = GTK_ORIENTATION_VERTICAL;
		break;
        case PANEL_APPLET_ORIENT_LEFT:
                o = GTK_ORIENTATION_VERTICAL;
		break;
        case PANEL_APPLET_ORIENT_DOWN:
                o = GTK_ORIENTATION_HORIZONTAL;
		break;
        case PANEL_APPLET_ORIENT_UP:
                o = GTK_ORIENTATION_HORIZONTAL;
		break;
        default:
                g_assert_not_reached ();
                return;
	}

        gtk_orientable_set_orientation (GTK_ORIENTABLE (cd->main_obox), o);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (cd->weather_obox), o);

        update_clock (NULL, NULL, cd);
        update_calendar_popup (cd);
}

/* this is when the panel size changes */
static void
panel_button_change_pixel_size (GtkWidget     *widget,
                                GtkAllocation *allocation,
                                ClockData	*cd)
{
	if (cd->old_allocation.width  == allocation->width &&
	    cd->old_allocation.height == allocation->height)
		return;

	cd->old_allocation = *allocation;

	update_clock (NULL, NULL, cd);
}

static void
copy_time (GSimpleAction *action,
           GVariant      *parameter,
	   gpointer       user_data)
{
        ClockData *cd = (ClockData *) user_data;
	const char *time;

        time = gnome_wall_clock_get_clock (cd->wall_clock);

	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
				time, -1);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				time, -1);
}

static void
config_date (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
	GDesktopAppInfo     *app_info;
	GdkDisplay          *display;
	GdkAppLaunchContext *context;
	GError              *error;
	ClockData           *cd;
	GtkWidget           *dialog;

	app_info = g_desktop_app_info_new ("gnome-datetime-panel.desktop");
	if (!app_info)
		return;

	display = gdk_display_get_default ();
	context = gdk_display_get_app_launch_context (display);
	error = NULL;

	g_app_info_launch (G_APP_INFO (app_info),
	                   NULL,
	                   (GAppLaunchContext *) context,
	                   &error);

	g_object_unref (app_info);
	g_object_unref (context);

	if (!error)
		return;

        cd = user_data;

	dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (cd->applet)),
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

static const GActionEntry clock_menu_actions [] = {
        { "preferences", verb_display_properties_dialog, NULL, NULL, NULL },
        { "copy-time",   copy_time,                      NULL, NULL, NULL },
        { "config",      config_date,                    NULL, NULL, NULL }
};

static void
format_changed (GSettings    *settings,
                const char   *key,
                ClockData    *clock)
{
	if (clock->calendar_popup != NULL) {
		calendar_window_set_time_format (CALENDAR_WINDOW (clock->calendar_popup),
                                                 g_settings_get_enum (settings, KEY_CLOCK_FORMAT));
                position_calendar_popup (clock);
	}

}

static void
update_panel_weather (ClockData *cd)
{
        gboolean show_weather, show_temperature;

        show_weather = g_settings_get_boolean (cd->applet_settings, KEY_SHOW_WEATHER);
        show_temperature = g_settings_get_boolean (cd->applet_settings, KEY_SHOW_TEMPERATURE);

	if ((show_weather || show_temperature) &&
	    g_list_length (cd->locations) > 0)
                gtk_widget_show (cd->weather_obox);
        else
                gtk_widget_hide (cd->weather_obox);

	gtk_widget_queue_resize (cd->applet);
}

static void
location_weather_updated_cb (ClockLocation  *location,
                             GWeatherInfo   *info,
                             gpointer        data)
{
	ClockData *cd = data;
	const gchar *icon_name;
	const gchar *temp;
	GtkIconTheme *theme;
	GdkPixbuf *pixbuf;

	if (!info || !gweather_info_is_valid (info))
		return;

	if (!clock_location_is_current (location))
		return;

	icon_name = gweather_info_get_icon_name (info);
	/* FIXME: mmh, screen please? Also, don't hardcode to 16 */
	theme = gtk_icon_theme_get_default ();
	pixbuf = gtk_icon_theme_load_icon (theme, icon_name, 16,
					   GTK_ICON_LOOKUP_GENERIC_FALLBACK, NULL);

	temp = gweather_info_get_temp_summary (info);

	gtk_image_set_from_pixbuf (GTK_IMAGE (cd->panel_weather_icon), pixbuf);
	gtk_label_set_text (GTK_LABEL (cd->panel_temperature_label), temp);
}

static void
location_set_current_cb (ClockLocation *loc, 
			 gpointer       data)
{
	ClockData *cd = data;
	GWeatherInfo *info;

	info = clock_location_get_weather_info (loc);
	location_weather_updated_cb (loc, info, cd);

	if (cd->map_widget)
		clock_map_refresh (CLOCK_MAP (cd->map_widget));
}

static void
locations_changed (GSettings  *settings,
                   const char *key,
                   ClockData  *cd)
{
	GList *l;
	ClockLocation *loc;
	glong id;

	if (!cd->locations) {
		if (cd->weather_obox)
			gtk_widget_hide (cd->weather_obox);
		if (cd->panel_weather_icon)
			gtk_image_set_from_pixbuf (GTK_IMAGE (cd->panel_weather_icon),
						   NULL);
		if (cd->panel_temperature_label)
			gtk_label_set_text (GTK_LABEL (cd->panel_temperature_label),
					    "");
	} else {
		if (cd->weather_obox)
			gtk_widget_show (cd->weather_obox);
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
show_week_changed (GSettings    *settings,
                   const char   *key,
		   ClockData    *clock)
{
	if (clock->calendar_popup != NULL) {
		calendar_window_set_show_weeks (CALENDAR_WINDOW (clock->calendar_popup),
                                                g_settings_get_boolean (settings, KEY_SHOW_WEEKS));
                position_calendar_popup (clock);
	}
}

static void
load_cities (ClockData *cd)
{
        GVariantIter *iter;
        const char *name;
        const char *code;
        gboolean latlon_override;
        gdouble latitude, longitude;

        g_settings_get (cd->applet_settings, KEY_CITIES, "a(ssm(dd))", &iter);

        while (g_variant_iter_loop (iter, "(&s&sm(dd))", &name, &code, &latlon_override,
                                    &latitude, &longitude)) {
                ClockLocation *loc;

                loc = clock_location_new (name, code,
                                          latlon_override, latitude, longitude);

                g_settings_bind (cd->clock_settings, KEY_CLOCK_FORMAT,
                                 loc, "clock-format",
                                 G_SETTINGS_BIND_GET);

                cd->locations = g_list_prepend (cd->locations, loc);
        }

        cd->locations = g_list_reverse (cd->locations);
}

static void
timezone_changed (ClockData *cd)
{
        const gchar *timezone;
        GList       *locations;
        GList       *l;

        timezone = timedate1_get_timezone (cd->timedate1);

        if (timezone == NULL)
                return;

        locations = cd->locations;
        for (l = locations; l; l = l->next) {
                ClockLocation *location = l->data;
                const gchar   *tzname = clock_location_get_tzname (location);

                if (g_strcmp0 (timezone, tzname) == 0) {
                        /* FIXME: make this location as current */
                        break;
                }
        }
}

static gboolean
fill_clock_applet (PanelApplet *applet)
{
	ClockData          *cd;
        GSimpleActionGroup *action_group;
        GAction            *action;
        GError             *error;

	panel_applet_set_flags (applet, PANEL_APPLET_EXPAND_MINOR);

	cd = g_new0 (ClockData, 1);

	cd->applet_settings = panel_applet_settings_new (applet, CLOCK_SCHEMA);
        cd->clock_settings = g_settings_new (DESKTOP_INTERFACE_SCHEMA);
        cd->weather_settings = g_settings_new (GWEATHER_SCHEMA);

        g_signal_connect (cd->clock_settings, "changed::" KEY_CLOCK_FORMAT,
                          G_CALLBACK (format_changed), cd);
        g_signal_connect (cd->clock_settings, "changed::" KEY_CLOCK_SHOW_WEEKS,
                          G_CALLBACK (show_week_changed), cd);
        g_signal_connect (cd->applet_settings, "changed::" KEY_CITIES,
                          G_CALLBACK (locations_changed), cd);

	cd->applet = GTK_WIDGET (applet);

        cd->world = gweather_location_get_world ();
        load_cities (cd);
        locations_changed (NULL, NULL, cd);

        error = NULL;
        cd->timedate1 = timedate1_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                          G_DBUS_PROXY_FLAGS_NONE,
                                                          "org.freedesktop.timedate1",
                                                          "/org/freedesktop/timedate1",
                                                          NULL,
                                                          &error);
        if (error) {
                g_warning ("%s", error->message);
                g_error_free (error);
        } else {
                g_signal_connect_swapped (cd->timedate1, "notify::timezone",
                                          G_CALLBACK (timezone_changed), cd);
        }

	cd->builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (cd->builder, GETTEXT_PACKAGE);
	gtk_builder_add_from_resource (cd->builder, CLOCK_RESOURCE_PATH "clock.ui", NULL);

	create_clock_widget (cd);

	gtk_widget_show (cd->applet);

	/* FIXME: Update this comment. */
	/* we have to bind change_orient before we do applet_widget_add
	   since we need to get an initial change_orient signal to set our
	   initial oriantation, and we get that during the _add call */
	g_signal_connect (G_OBJECT (cd->applet),
			  "change_orient",
			  G_CALLBACK (applet_change_orient),
			  cd);

	g_signal_connect (G_OBJECT (cd->panel_button),
			  "size_allocate",
			  G_CALLBACK (panel_button_change_pixel_size),
			  cd);

	panel_applet_set_background_widget (PANEL_APPLET (cd->applet),
					    GTK_WIDGET (cd->applet));

        action_group = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP (action_group),
	                                 clock_menu_actions,
	                                 G_N_ELEMENTS (clock_menu_actions),
	                                 cd);
	panel_applet_setup_menu_from_resource (PANEL_APPLET (cd->applet),
					       CLOCK_RESOURCE_PATH "clock-menu.xml",
					       action_group,
					       GETTEXT_PACKAGE);

        gtk_widget_insert_action_group (GTK_WIDGET (applet), "clock",
	                                G_ACTION_GROUP (action_group));

	action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "preferences");
	g_object_bind_property (cd->applet, "locked-down",
				action, "enabled",
				G_BINDING_DEFAULT|G_BINDING_INVERT_BOOLEAN|G_BINDING_SYNC_CREATE);

	action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "config");
	g_object_bind_property (cd->applet, "locked-down",
				action, "enabled",
				G_BINDING_DEFAULT|G_BINDING_INVERT_BOOLEAN|G_BINDING_SYNC_CREATE);
        g_object_unref (action_group);

	return TRUE;
}

static void
display_properties_dialog (ClockData *cd, gboolean start_in_locations_page)
{
        gint       page_number;
        GtkWidget *prefs_window;

        page_number = 0;
        if (start_in_locations_page)
                page_number = 1;

        prefs_window = clock_preferences_new (cd->applet_settings,
                                              page_number);

        gtk_window_present (GTK_WINDOW (prefs_window));
}

static void
verb_display_properties_dialog (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
        ClockData *cd = (ClockData *) user_data;
        display_properties_dialog (cd, FALSE);
}

static gboolean
clock_factory (PanelApplet *applet,
	       const char  *iid,
	       gpointer     data)
{
	gboolean retval = FALSE;

	if (!strcmp (iid, "ClockApplet"))
		retval = fill_clock_applet (applet);

	return retval;
}

PANEL_APPLET_IN_PROCESS_FACTORY ("ClockAppletFactory",
                                 PANEL_TYPE_APPLET,
                                 clock_factory,
                                 NULL)
