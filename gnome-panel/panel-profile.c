/*
 * panel-profile.c:
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-profile.h"

#include <string.h>
#include <libgnome/gnome-i18n.h>

#include "panel-gconf.h"
#include "panel-widget.h"
#include "panel.h"

#define CONFIG_DIR   "/apps/new_panel/profiles"
#define SCHEMAS_DIR  "/schemas/apps/new_panel"
#define DEFAULTS_DIR "/apps/new_panel/default_setup"

typedef struct {
	GdkScreen       *screen;
	int              monitor;
	int              size;
	int              x;
	gboolean         x_centered;
	int              y;
	gboolean         y_centered;
	PanelOrientation orientation;

	guint screen_changed : 1;
	guint monitor_changed : 1;
	guint size_changed : 1;
	guint x_changed : 1;
	guint x_centered_changed : 1;
	guint y_changed : 1;
	guint y_centered_changed : 1;
	guint orientation_changed : 1;
} ToplevelLocationChange;

static char *current_profile = NULL;

static GConfEnumStringPair panel_orientation_map [] = {
	{ PANEL_ORIENTATION_TOP,    "top"    },
	{ PANEL_ORIENTATION_BOTTOM, "bottom" },
	{ PANEL_ORIENTATION_LEFT,   "left"   },
	{ PANEL_ORIENTATION_RIGHT,  "right"  }
};

static GConfEnumStringPair panel_animation_speed_map [] = {
	{ PANEL_ANIMATION_SLOW,   "slow"   },
	{ PANEL_ANIMATION_MEDIUM, "medium" },
	{ PANEL_ANIMATION_FAST,   "fast"   }
};

static GConfEnumStringPair panel_background_type_map [] = {
	{ PANEL_BACK_NONE,  "gtk"   },
	{ PANEL_BACK_COLOR, "color" },
	{ PANEL_BACK_IMAGE, "image" }
};

static GQuark toplevel_id_quark = 0;
static GQuark queued_changes_quark = 0;
static GQuark commit_timeout_quark = 0;

#undef PANEL_PROFILE_DEBUG
#ifdef PANEL_PROFILE_DEBUG
#ifdef G_HAVE_ISO_VARARGS
#define d_print(...) g_print (__VA_ARGS__)
#elif defined(G_HAVE_GNUC_VARARGS)
#define d_print(args...) g_print (args)
#else
#error "Your compiler's weird - its okay, though, this is only debugging code. It'll be gone soon"
#endif
#else
#ifdef G_HAVE_ISO_VARARGS
#define d_print(...)
#elif defined(G_HAVE_GNUC_VARARGS)
#define d_print(args...)
#else
#error "Your compiler's weird - its okay, though, this is only debugging code. It'll be gone soon"
#endif
#endif

const char *
panel_profile_get_name (void)
{
	return current_profile;
}

static void
panel_profile_set_toplevel_id (PanelToplevel *toplevel,
			       char          *id)
{
	if (!toplevel_id_quark)
		toplevel_id_quark = g_quark_from_static_string ("panel-toplevel-id");

	g_object_set_qdata_full (G_OBJECT (toplevel), toplevel_id_quark, id, g_free);
}

static const char *
panel_profile_get_toplevel_id (PanelToplevel *toplevel)
{
	if (!toplevel_id_quark)
		return NULL;

	return g_object_get_qdata (G_OBJECT (toplevel), toplevel_id_quark);
}

static void
panel_profile_set_queued_changes (GObject        *object,
				  GConfChangeSet *changes)
{
	if (!queued_changes_quark)
		queued_changes_quark = g_quark_from_static_string ("panel-queued-changes");

	g_object_set_qdata_full (object,
				 queued_changes_quark,
				 changes,
				 (GDestroyNotify) gconf_change_set_unref);
}

static GConfChangeSet *
panel_profile_get_queued_changes (GObject *object)
{
	if (!queued_changes_quark)
		return NULL;

	return g_object_get_qdata (object, queued_changes_quark);
}

static void
panel_profile_set_commit_timeout (GObject *object,
				  guint    timeout)
{
	if (!commit_timeout_quark)
		commit_timeout_quark = g_quark_from_static_string ("panel-queued-timeout");

	g_object_set_qdata (object, commit_timeout_quark, GUINT_TO_POINTER (timeout));
}

static const guint
panel_profile_get_commit_timeout (GObject *object)
{
	if (!commit_timeout_quark)
		return 0;

	return GPOINTER_TO_UINT (g_object_get_qdata (object, commit_timeout_quark));
}

static const char *
panel_profile_get_toplevel_key (PanelToplevel *toplevel,
				const char    *key)
{
	const char *id;

	id = panel_profile_get_toplevel_id (toplevel);

	return panel_gconf_sprintf (CONFIG_DIR "/%s/toplevels/%s/%s",
				    current_profile, id, key);
}

#define TOPLEVEL_SET_FUNC(k, t, s, a)                               \
	void                                                        \
	panel_profile_set_toplevel_##s (PanelToplevel *toplevel, a) \
	{                                                           \
		GConfClient *client;                                \
		const char  *key;                                   \
		client = gconf_client_get_default ();               \
		d_print ("Setting '%s'\n", k);                      \
		key = panel_profile_get_toplevel_key (toplevel, k); \
		gconf_client_set_##t (client, key, s, NULL);        \
		g_object_unref (client);                            \
	}

TOPLEVEL_SET_FUNC ("name",              string, name,              const char *name)
TOPLEVEL_SET_FUNC ("expand",            bool,   expand,            gboolean expand)
TOPLEVEL_SET_FUNC ("auto_hide",         bool,   auto_hide,         gboolean auto_hide)
TOPLEVEL_SET_FUNC ("enable_buttons",    bool,   enable_buttons,    gboolean enable_buttons)
TOPLEVEL_SET_FUNC ("enable_arrows",     bool,   enable_arrows,     gboolean enable_arrows)
TOPLEVEL_SET_FUNC ("enable_animations", bool,   enable_animations, gboolean enable_animations)

static PanelBackgroundType
get_background_type (GConfClient *client,
		     const char  *toplevel_dir)
{
	GError     *error = NULL;
	const char *key;
	char       *type_str;
	int         background_type;

	key = panel_gconf_sprintf ("%s/background/type", toplevel_dir);
	type_str = gconf_client_get_string (client, key, &error);
	if (error) {
		g_warning (_("Error reading GConf string value '%s': %s"),
			   key, error->message);
		g_error_free (error);
		return PANEL_BACK_NONE;
	}

	if (!type_str || !gconf_string_to_enum (panel_background_type_map,
						type_str,
						&background_type))
		background_type = PANEL_BACK_NONE;

	g_free (type_str);
	
	return background_type;
}

static void
get_background_color (PanelBackground *background,
		      GConfClient     *client,
		      const char      *toplevel_dir)
{
	PanelColor  color;
	PangoColor  pango_color;
	GError     *error = NULL;
	const char *key;
	char       *color_str;

	key = panel_gconf_sprintf ("%s/background/color", toplevel_dir);
	color_str = gconf_client_get_string (client, key, &error);
	if (error) {
		g_warning (_("Error reading GConf string value '%s': %s"),
			   key, error->message);
		g_error_free (error);
		panel_background_set_none (background);
		return;
	}

	if (!color_str || !pango_color_parse (&pango_color, color_str)) {
		panel_background_set_none (background);
		g_free (color_str);
		return;
	}

	color.gdk.red   = pango_color.red;
	color.gdk.green = pango_color.green;
	color.gdk.blue  = pango_color.blue;

	g_free (color_str);

	key = panel_gconf_sprintf ("%s/background/opacity", toplevel_dir);
	color.alpha = gconf_client_get_int (client, key, &error);
	if (error) {
		g_warning (_("Error reading GConf integer value '%s': %s"),
			   key, error->message);
		g_error_free (error);
		color.alpha = 65535; /* fallback to fully opaque */
	}

	panel_background_set_color (background, &color);
}

static void
get_background_image (PanelBackground *background,
		      GConfClient     *client,
		      const char      *toplevel_dir)
{
	const char *key;
	GError     *error = NULL;
	char       *image;
	gboolean    fit;
	gboolean    stretch;
	gboolean    rotate;

	key = panel_gconf_sprintf ("%s/background/image", toplevel_dir);
	image = gconf_client_get_string (client, key, &error);
	if (error) {
		g_warning (_("Error reading GConf string value '%s': %s"),
			   key, error->message);
		g_error_free (error);
		panel_background_set_none (background);
		return;
	}

	if (!image) {
		panel_background_set_none (background);
		return;
	}

	key = panel_gconf_sprintf ("%s/background/fit", toplevel_dir);
	fit = gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_sprintf ("%s/background/stretch", toplevel_dir);
	stretch = gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_sprintf ("%s/background/rotate", toplevel_dir);
	rotate = gconf_client_get_bool (client, key, NULL);

	panel_background_set_image (background, image, fit, stretch, rotate);

	g_free (image);
}

static void
panel_profile_load_background (PanelToplevel *toplevel,
			       GConfClient   *client,
			       const char    *toplevel_dir)
{
	PanelWidget         *panel_widget;
	PanelBackground     *background;
	PanelBackgroundType  background_type;

	panel_widget = panel_toplevel_get_panel_widget (toplevel);
	background = &panel_widget->background;

	background_type = get_background_type (client, toplevel_dir);
	switch (background_type) {
	case PANEL_BACK_NONE:
		panel_background_set_none (background);
		break;
	case PANEL_BACK_COLOR:
		get_background_color (background, client, toplevel_dir);
		break;
	case PANEL_BACK_IMAGE:
		get_background_image (background, client, toplevel_dir);
		break;
	}
}

static GdkScreen *
get_toplevel_screen (GConfClient   *client,
		     const char    *toplevel_dir)
{
	GError     *error = NULL;
	GdkDisplay *display;
	const char *key;
	int         screen_n;

	key = panel_gconf_sprintf ("%s/screen", toplevel_dir);
	screen_n = gconf_client_get_int (client, key, &error);
	if (error) {
		g_warning (_("Error reading GConf integer value '%s': %s"),
			   key, error->message);
		g_error_free (error);
		return gdk_screen_get_default ();
	}

	display = gdk_display_get_default ();

	if (screen_n < 0 || screen_n >= gdk_display_get_n_screens (display)) {
		g_warning (_("Panel '%s' is set to be displayed on screen %d which "
			     "is not currently available. Not loading this panel."),
			   toplevel_dir, screen_n);
		return NULL;
	}

	return gdk_display_get_screen (display, screen_n);
}

static gboolean
panel_profile_commit_toplevel_changes (PanelToplevel *toplevel)
{
	GConfChangeSet *queued_changes;

	queued_changes = panel_profile_get_queued_changes (G_OBJECT (toplevel));
	if (queued_changes) {
		GConfClient *client;

		client = gconf_client_get_default ();
		gconf_client_commit_change_set (client, queued_changes, FALSE, NULL);
		g_object_unref (client);
	}

	panel_profile_set_queued_changes (G_OBJECT (toplevel), NULL);
	panel_profile_set_commit_timeout (G_OBJECT (toplevel), 0);

	return FALSE;
}

static void
panel_profile_queue_toplevel_location_change (PanelToplevel          *toplevel,
					      ToplevelLocationChange *change)
{
	GConfChangeSet *queued_changes;
	guint           commit_timeout;

	queued_changes = panel_profile_get_queued_changes (G_OBJECT (toplevel));
	if (!queued_changes) {
		queued_changes = gconf_change_set_new ();
		panel_profile_set_queued_changes (G_OBJECT (toplevel), queued_changes);
	}

#define DEBUG_CHANGE(k,n) \
	if (change->n##_changed) \
		d_print ("queued change '%s'\n", k);

	DEBUG_CHANGE ("screen", screen);
	DEBUG_CHANGE ("monitor", monitor);
	DEBUG_CHANGE ("size", size);
	DEBUG_CHANGE ("x_centered", x_centered);
	DEBUG_CHANGE ("x", x);
	DEBUG_CHANGE ("y", y);
	DEBUG_CHANGE ("y_centered", y_centered);
	DEBUG_CHANGE ("orientation", orientation);

	if (change->screen_changed)
		gconf_change_set_set_int (
			queued_changes,
			panel_profile_get_toplevel_key (toplevel, "screen"),
			gdk_screen_get_number (change->screen));

	if (change->monitor_changed)
		gconf_change_set_set_int (
			queued_changes,
			panel_profile_get_toplevel_key (toplevel, "monitor"),
			change->monitor);

	if (change->size_changed)
		gconf_change_set_set_int (
			queued_changes,
			panel_profile_get_toplevel_key (toplevel, "size"),
			change->size);

	if (change->x_changed)
		gconf_change_set_set_int (
			queued_changes,
			panel_profile_get_toplevel_key (toplevel, "x"),
			change->x);

	if (change->x_centered_changed)
		gconf_change_set_set_bool (
			queued_changes,
			panel_profile_get_toplevel_key (toplevel, "x_centered"),
			change->x_centered);

	if (change->y_changed)
		gconf_change_set_set_int (
			queued_changes,
			panel_profile_get_toplevel_key (toplevel, "y"),
			change->y);

	if (change->y_centered_changed)
		gconf_change_set_set_bool (
			queued_changes,
			panel_profile_get_toplevel_key (toplevel, "y_centered"),
			change->y_centered);

	if (change->orientation)
		gconf_change_set_set_string (
			queued_changes,
			 panel_profile_get_toplevel_key (toplevel, "orientation"),
			gconf_enum_to_string (panel_orientation_map, change->orientation));

	commit_timeout = panel_profile_get_commit_timeout (G_OBJECT (toplevel));
	if (!commit_timeout) {
		commit_timeout =
			g_timeout_add (500,
				       (GSourceFunc) panel_profile_commit_toplevel_changes,
				       toplevel);
		panel_profile_set_commit_timeout (G_OBJECT (toplevel), commit_timeout);
	}
}

#define TOPLEVEL_LOCATION_CHANGED_HANDLER(c)                                      \
	static void                                                               \
	panel_profile_toplevel_##c##_changed (PanelToplevel *toplevel)            \
	{                                                                         \
		ToplevelLocationChange change = { 0, };                           \
		change.c##_changed = TRUE;                                        \
		change.c = panel_toplevel_get_##c (toplevel);                     \
		panel_profile_queue_toplevel_location_change (toplevel, &change); \
	}

TOPLEVEL_LOCATION_CHANGED_HANDLER(monitor)
TOPLEVEL_LOCATION_CHANGED_HANDLER(size)
TOPLEVEL_LOCATION_CHANGED_HANDLER(orientation)
TOPLEVEL_LOCATION_CHANGED_HANDLER(x_centered)
TOPLEVEL_LOCATION_CHANGED_HANDLER(y_centered)

#define TOPLEVEL_POSITION_CHANGED_HANDLER(c)                                      \
	static void                                                               \
	panel_profile_toplevel_##c##_changed (PanelToplevel *toplevel)            \
	{                                                                         \
		ToplevelLocationChange change = { 0, };                           \
		int                    x, y;                                      \
		change.c##_changed = TRUE;                                        \
		panel_toplevel_get_position (toplevel, &x, &y);                   \
		change.c = c;                                                     \
		panel_profile_queue_toplevel_location_change (toplevel, &change); \
	}

TOPLEVEL_POSITION_CHANGED_HANDLER(x)
TOPLEVEL_POSITION_CHANGED_HANDLER(y)

static void
panel_profile_toplevel_screen_changed (PanelToplevel *toplevel)
{
	ToplevelLocationChange change = { 0, };

	change.screen_changed = TRUE;
	change.screen = gtk_window_get_screen (GTK_WINDOW (toplevel));

	panel_profile_queue_toplevel_location_change (toplevel, &change);
}

static void
panel_profile_connect_to_toplevel (PanelToplevel *toplevel)
{
	g_signal_connect (toplevel, "notify::screen",
			  G_CALLBACK (panel_profile_toplevel_screen_changed), NULL);
	g_signal_connect (toplevel, "notify::monitor",
			  G_CALLBACK (panel_profile_toplevel_monitor_changed), NULL);
	g_signal_connect (toplevel, "notify::size",
			  G_CALLBACK (panel_profile_toplevel_size_changed), NULL);
	g_signal_connect (toplevel, "notify::x",
			  G_CALLBACK (panel_profile_toplevel_x_changed), NULL);
	g_signal_connect (toplevel, "notify::x-centered",
			  G_CALLBACK (panel_profile_toplevel_x_centered_changed), NULL);
	g_signal_connect (toplevel, "notify::y",
			  G_CALLBACK (panel_profile_toplevel_y_changed), NULL);
	g_signal_connect (toplevel, "notify::y-centered",
			  G_CALLBACK (panel_profile_toplevel_y_centered_changed), NULL);
	g_signal_connect (toplevel, "notify::orientation",
			  G_CALLBACK (panel_profile_toplevel_orientation_changed), NULL);
}

static void
set_name_from_string (PanelToplevel *toplevel,
		      const char    *str)
{
	if (!str)
		return;

	panel_toplevel_set_name (toplevel, str);
}

static void
set_orientation_from_string (PanelToplevel *toplevel,
			     const char    *str)
{
	int orientation;

	if (!str || !gconf_string_to_enum (panel_orientation_map,
					   str,
					   &orientation))
		return;

	panel_toplevel_set_orientation (toplevel, orientation);
}

static void
set_animation_speed_from_string (PanelToplevel *toplevel,
				 const char    *str)
{
	int animation_speed;

	if (!str || !gconf_string_to_enum (panel_animation_speed_map,
					   str,
					   &animation_speed))
		return;

	panel_toplevel_set_animation_speed (toplevel, animation_speed);
}

static void
panel_profile_toplevel_change_notify (GConfClient   *client,
				      guint          cnxn_id,
				      GConfEntry    *entry,
				      PanelToplevel *toplevel)
{
	GConfValue *value;
	const char *key;

	key = panel_gconf_basename (gconf_entry_get_key (entry));

	if (!(value = gconf_entry_get_value (entry)))
		return;

#define UPDATE_STRING(k, n)                                                             \
		if (!strcmp (key, k)) {                                                 \
			d_print ("Notify on '%s'\n", k);                                \
			if (value->type == GCONF_VALUE_STRING)                          \
				set_##n##_from_string (toplevel,                        \
						       gconf_value_get_string (value)); \
		}

#define UPDATE_INT(k, n)                                                                \
		if (!strcmp (key, k)) {                                                 \
			d_print ("Notify on '%s'\n", k);                                \
			if (value->type == GCONF_VALUE_INT)                             \
				panel_toplevel_set_##n (toplevel,                       \
							gconf_value_get_int (value));   \
		}

#define UPDATE_BOOL(k, n)                                                               \
		if (!strcmp (key, k)) {                                                 \
			d_print ("Notify on '%s'\n", k);                                \
			if (value->type == GCONF_VALUE_BOOL)                            \
				panel_toplevel_set_##n (toplevel,                       \
							gconf_value_get_bool (value));  \
		}

#define UPDATE_POS(k, n)                                                                \
		if (!strcmp (key, k)) {                                                 \
			d_print ("Notify on '%s'\n", k);                                \
			if (value->type == GCONF_VALUE_INT)                             \
				panel_toplevel_set_##n (                                \
					toplevel,                                       \
					gconf_value_get_int (value),                    \
					panel_toplevel_get_##n##_centered (toplevel));  \
		}

#define UPDATE_CENTERED(k, n)                                                           \
		if (!strcmp (key, k)) {                                                 \
			d_print ("Notify on '%s'\n", k);                                \
			if (value->type == GCONF_VALUE_BOOL) {                          \
				int x, y;                                               \
				panel_toplevel_get_position (toplevel, &x, &y);         \
				panel_toplevel_set_##n (                                \
					toplevel, n, gconf_value_get_bool (value));     \
			}                                                               \
		}

	if (!strcmp (key, "screen")) {
		d_print ("Notify on 'screen'\n");
		if (value->type == GCONF_VALUE_INT) {
			GdkScreen *screen;

			screen = gdk_display_get_screen (
					gdk_display_get_default (), 
					gconf_value_get_int (value));
			if (screen)
				gtk_window_set_screen (GTK_WINDOW (toplevel), screen);
			else
				/* Make sure to set the key back to an actual
				 * available screen so it will get loaded on
				 * next startup.
				 */
				panel_profile_toplevel_screen_changed (toplevel);
		}
			
	}
	else UPDATE_INT ("monitor", monitor)
	else UPDATE_STRING ("name", name)
	else UPDATE_BOOL ("expand", expand)
	else UPDATE_STRING ("orientation", orientation)
	else UPDATE_INT ("size", size)
	else UPDATE_POS ("x", x)
	else UPDATE_POS ("y", y)
	else UPDATE_CENTERED ("x_centered", x)
	else UPDATE_CENTERED ("y_centered", y)
	else UPDATE_BOOL ("auto_hide", auto_hide)
	else UPDATE_BOOL ("enable_animations", animate)
	else UPDATE_BOOL ("enable_buttons", enable_buttons)
	else UPDATE_BOOL ("enable_arrows", enable_arrows)
	else UPDATE_INT ("hide_delay", hide_delay)
	else UPDATE_INT ("unhide_delay", unhide_delay)
	else UPDATE_INT ("auto_hide_size", auto_hide_size)
	else UPDATE_STRING ("animation_speed", animation_speed)

	/* FIXME: handle background changes */
}

static void
panel_profile_disconnect_toplevel (PanelToplevel *toplevel,
				   gpointer       data)
{
	GConfClient *client;
	guint        notify_id = GPOINTER_TO_UINT (data);

	client = gconf_client_get_default ();

	gconf_client_notify_remove (client, notify_id);

	g_object_unref (client);
}

static void
panel_profile_load_toplevel (GConfClient *client,
			     const char  *profile_dir,
			     char        *toplevel_id)
{
	PanelToplevel *toplevel;
	GdkScreen     *screen;
	GError        *error;
	const char    *key;
	char          *toplevel_dir;
	guint          notify_id;

	toplevel_dir = g_strdup_printf ("%s/toplevels/%s", profile_dir, toplevel_id);

	if (!gconf_client_dir_exists (client, toplevel_dir, NULL))
		panel_gconf_associate_schemas_in_dir (
			client, toplevel_dir, SCHEMAS_DIR "/toplevels");

	if (!(screen = get_toplevel_screen (client, toplevel_dir))) {
		g_free (toplevel_dir);
		return;
	}

	gconf_client_add_dir (client,
			      toplevel_dir,
			      GCONF_CLIENT_PRELOAD_RECURSIVE,
			      NULL);

	toplevel = g_object_new (PANEL_TYPE_TOPLEVEL,
				 "screen", screen,
				 NULL);

#define GET_INT(k, fn)                                                              \
	{                                                                           \
		int val;                                                            \
		error = NULL;                                                       \
		key = panel_gconf_sprintf ("%s/" k, toplevel_dir);                  \
		val = gconf_client_get_int (client, key, &error);                   \
		if (!error)                                                         \
			panel_toplevel_set_##fn (toplevel, val);                    \
		else {                                                              \
			g_warning (_("Error reading GConf integer value '%s': %s"), \
				   key, error->message);                            \
			g_error_free (error);                                       \
		}                                                                   \
	}

#define GET_BOOL(k, fn)                                                             \
	{                                                                           \
		gboolean val;                                                       \
		error = NULL;                                                       \
		key = panel_gconf_sprintf ("%s/" k, toplevel_dir);                  \
		val = gconf_client_get_bool (client, key, &error);                  \
		if (!error)                                                         \
			panel_toplevel_set_##fn (toplevel, val);                    \
		else {                                                              \
			g_warning (_("Error reading GConf boolean value '%s': %s"), \
				   key, error->message);                            \
			g_error_free (error);                                       \
		}                                                                   \
	}

#define GET_STRING(k, fn)                                                           \
	{                                                                           \
		char *val;                                                          \
		error = NULL;                                                       \
		key = panel_gconf_sprintf ("%s/" k, toplevel_dir);                  \
		val = gconf_client_get_string (client, key, &error);                \
		if (!error && val) {                                                \
			set_##fn##_from_string (toplevel, val);                     \
			g_free (val);                                               \
		} else if (!error) {                                                \
			g_warning (_("Error reading GConf string value '%s': %s"),  \
				   key, error->message);                            \
			g_error_free (error);                                       \
		}                                                                   \
	}

	GET_STRING ("name", name);
	GET_INT ("monitor", monitor);
	GET_BOOL ("expand", expand);
	GET_STRING ("orientation", orientation);
	GET_INT ("size", size);
	GET_BOOL ("auto_hide", auto_hide);
	GET_BOOL ("enable_animations", animate);
	GET_BOOL ("enable_buttons", enable_buttons);
	GET_BOOL ("enable_arrows", enable_arrows);
	GET_INT ("hide_delay", hide_delay);
	GET_INT ("unhide_delay", unhide_delay);
	GET_INT ("auto_hide_size", auto_hide_size);
	GET_STRING ("animation_speed", animation_speed);

#define GET_POSITION(a, c, fn)                                                      \
	{                                                                           \
		gboolean centered;                                                  \
		int      position;                                                  \
		key = panel_gconf_sprintf ("%s/" c, toplevel_dir);                  \
		centered = gconf_client_get_bool (client, key, &error);             \
		error = NULL;                                                       \
		key = panel_gconf_sprintf ("%s/" a, toplevel_dir);                  \
		position = gconf_client_get_int (client, key, &error);              \
		if (!error)                                                         \
			panel_toplevel_set_##fn (toplevel, position, centered);     \
		else {                                                              \
			g_warning (_("Error reading GConf integer value '%s': %s"), \
				   key, error->message);                            \
			g_error_free (error);                                       \
		}                                                                   \
	}

	GET_POSITION ("x", "x_centered", x);
	GET_POSITION ("y", "y_centered", y);

	panel_profile_load_background (toplevel, client, toplevel_dir);

	panel_profile_set_toplevel_id (toplevel, toplevel_id);

	panel_profile_connect_to_toplevel (toplevel);

	notify_id = gconf_client_notify_add (
			client, toplevel_dir,
			(GConfClientNotifyFunc) panel_profile_toplevel_change_notify,
			toplevel, NULL, NULL);

	g_signal_connect (toplevel, "destroy",
			  G_CALLBACK (panel_profile_disconnect_toplevel),
			  GUINT_TO_POINTER (notify_id));

	g_free (toplevel_dir);

	panel_setup (toplevel);
	gtk_widget_show (GTK_WIDGET (toplevel));
}

static void
panel_profile_delete_toplevel_dir (GConfClient *client,
				   const char  *toplevel_id)
{
	const char *key;

	key = panel_gconf_sprintf (CONFIG_DIR "/%s/toplevels/%s",
				   current_profile,
				   toplevel_id);
	panel_gconf_clean_dir (client, key);
}

static gboolean
panel_profile_id_exists (GSList     *toplevel_list,
			 const char *toplevel_id)
{
	GSList *l;

	if (!toplevel_list || !toplevel_id)
		return FALSE;

	for (l = toplevel_list; l; l = l->next) {
		PanelToplevel *toplevel = l->data;
		const char    *id;

		id = panel_profile_get_toplevel_id (toplevel);
		g_assert (id != NULL);

		if (!strcmp (toplevel_id, id))
			return TRUE;
	}

	return FALSE;
}

static gboolean
panel_profile_create_added_toplevels (GConfClient *client,
				      GSList      *toplevel_ids)
{
	GSList *existing_toplevels;
	GSList *ids_to_create = NULL;
	GSList *l;
	char   *profile_dir = NULL;

	existing_toplevels = panel_toplevel_list_toplevels ();

	for (l = toplevel_ids; l; l = l->next) {
		const char *toplevel_id = gconf_value_get_string (l->data);

		if (!panel_profile_id_exists (existing_toplevels, toplevel_id))
			ids_to_create = g_slist_prepend (ids_to_create,
							 g_strdup (toplevel_id));
	}

	for (l = ids_to_create; l; l = l->next) {
		char *toplevel_id = l->data;

		if (!profile_dir)
			profile_dir = gconf_concat_dir_and_key (CONFIG_DIR, current_profile);

		/* takes ownership of toplevel_id */
		panel_profile_load_toplevel (client, profile_dir, toplevel_id);
	}
	g_slist_free (ids_to_create);

	g_free (profile_dir);
}

static gboolean
panel_profile_toplevel_exists (GSList        *toplevel_ids_list,
			       PanelToplevel *toplevel)
{
	GSList     *l;
	const char *toplevel_id;

	if (!toplevel_ids_list || !toplevel)
		return FALSE;

	toplevel_id = panel_profile_get_toplevel_id (toplevel);

	for (l = toplevel_ids_list; l; l = l->next) {
		const char *id = gconf_value_get_string (l->data);

		if (!strcmp (toplevel_id, id))
			return TRUE;
	}

	return FALSE;
}

static gboolean
panel_profile_delete_removed_toplevels (GConfClient *client,
					GSList      *toplevel_ids)
{
	GSList *toplevels_to_delete = NULL;
	GSList *existing_toplevels;
	GSList *l;

	existing_toplevels = panel_toplevel_list_toplevels ();

	for (l = existing_toplevels; l; l = l->next) {
		PanelToplevel *toplevel = l->data;

		if (!panel_profile_toplevel_exists (toplevel_ids, toplevel))
			toplevels_to_delete =
				g_slist_prepend (toplevels_to_delete, toplevel);
	}

	for (l = toplevels_to_delete; l; l = l->next) {
		PanelToplevel *toplevel = l->data;
		const char    *toplevel_id;

		toplevel_id = panel_profile_get_toplevel_id (toplevel);

		panel_profile_delete_toplevel_dir (client, toplevel_id);

		gtk_widget_destroy (GTK_WIDGET (toplevel));
	}
	g_slist_free (toplevels_to_delete);
}

static void
panel_profile_toplevel_id_list_notify (GConfClient *client,
				       guint        cnxn_id,
				       GConfEntry  *entry)
{
	GConfValue *value;
	GSList     *toplevel_ids;

	if (!(value = gconf_entry_get_value (entry)))
		return;

	if (value->type != GCONF_VALUE_LIST ||
	    gconf_value_get_list_type (value) != GCONF_VALUE_STRING) {
		gconf_value_free (value);
		return;
	}

	toplevel_ids = gconf_value_get_list (value);

	panel_profile_create_added_toplevels (client, toplevel_ids);
	panel_profile_delete_removed_toplevels (client, toplevel_ids);
}

static void
panel_profile_load_toplevel_list (GConfClient *client,
				  const char  *profile_dir)
{
	const char *key;
	GSList     *list;
	GSList     *l;
	
	key = panel_gconf_sprintf ("%s/general/toplevel_id_list", profile_dir);

	gconf_client_notify_add (
		client, key, 
		(GConfClientNotifyFunc) panel_profile_toplevel_id_list_notify,
		NULL, NULL, NULL);

	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);
	for (l = list; l; l = l->next) {
		char *toplevel_id = l->data;

		/* takes ownership of toplevel_id */
		panel_profile_load_toplevel (client, profile_dir, toplevel_id);
	}
	g_slist_free (list);

}

static char *
panel_profile_find_new_id (GSList     *existing_ids,
			   const char *prefix)
{
	GSList *l;
	char   *retval = NULL;
	int     i;

	for (i = 0; !retval; i++) {
		retval = g_strdup_printf ("%s_%d", prefix, i);

		for (l = existing_ids; l; l = l->next)
			if (!strcmp (l->data, retval)) {
				g_free (retval);
				retval = NULL;
				break;
			}
	}

	return retval;
}

void
panel_profile_create_toplevel (void)
{
	GConfClient *client;
	GSList      *list, *l;
	const char  *key;

	client = gconf_client_get_default ();

	key = panel_gconf_sprintf (CONFIG_DIR "/%s/general/toplevel_id_list",
				   current_profile);
	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	list = g_slist_append (list, panel_profile_find_new_id (list, "panel"));

	gconf_client_set_list (client, key, GCONF_VALUE_STRING, list, NULL);

	for (l = list; l; l = l->next)
		g_free (l->data);
	g_slist_free (list);

	g_object_unref (client);
}

void
panel_profile_delete_toplevel (PanelToplevel *toplevel)
{
	GConfClient *client;
	GSList      *list, *l;
	const char  *key;
	const char  *id;

	client = gconf_client_get_default ();

	key = panel_gconf_sprintf (CONFIG_DIR "/%s/general/toplevel_id_list",
				   current_profile);
	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	id = panel_profile_get_toplevel_id (toplevel);

	for (l = list; l; l = l->next)
		if (!strcmp (id, l->data))
			break;
	if (l) {
		g_free (l->data);
		list = g_slist_delete_link (list, l);
	}

	gconf_client_set_list (client, key, GCONF_VALUE_STRING, list, NULL);

	for (l = list; l; l = l->next)
		g_free (l->data);
	g_slist_free (list);

	g_object_unref (client);
}

static void
panel_profile_apply_schemas (GConfClient *client,
			     const char  *profile_dir,
			     const char  *schemas_dir,
			     const char  *id_list_key,
			     const char  *type)
{

	const char *key;
	GSList     *list, *l;
	GError     *error = NULL;

	key = panel_gconf_sprintf ("%s/general/%s", profile_dir, id_list_key);
	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, &error);
	if (error) {
		g_warning (_("Error reading GConf list value '%s': %s"),
			   key, error->message);
		g_error_free (error);
		return;
	}

	for (l = list; l; l = l->next) {
		char *dir;
		char *id = l->data;

		dir = g_strdup_printf ("%s/%s/%s", profile_dir, type, id);
		panel_gconf_associate_schemas_in_dir (client, dir, schemas_dir);
		g_free (dir);
		g_free (id);
	}

	g_slist_free (list);
}

static void
panel_configuration_copy_defaults (GConfClient *client,
				   const char  *profile_dir)
{
	char *dir;

	panel_gconf_copy_dir (client, DEFAULTS_DIR, profile_dir);

	dir = gconf_concat_dir_and_key (profile_dir, "general");
	panel_gconf_associate_schemas_in_dir (client, dir, SCHEMAS_DIR "/general");
	g_free (dir);

	panel_profile_apply_schemas (client,
				     profile_dir,
				     SCHEMAS_DIR "/toplevels",
				     "toplevel_id_list",
				     "toplevels");
	panel_profile_apply_schemas (client,
				     profile_dir,
				     SCHEMAS_DIR "/applets",
				     "applet_id_list",
				     "applets");
	panel_profile_apply_schemas (client,
				     profile_dir,
				     SCHEMAS_DIR "/objects",
				     "object_id_list",
				     "objects");
}

void
panel_profile_load (char *profile_name)
{
	GConfClient *client;
	char        *dir;
	const char  *key;

	if (current_profile) {
		g_warning ("Changing profiles not yet implemented\n");
		return;
	}

	current_profile = profile_name ? profile_name : "default";

	client  = gconf_client_get_default ();

	dir = gconf_concat_dir_and_key (CONFIG_DIR, current_profile);
	if (!gconf_client_dir_exists (client, dir, NULL))
		panel_configuration_copy_defaults (client, dir);

	key = panel_gconf_sprintf ("%s/general", dir);
	gconf_client_add_dir (client, key, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	panel_profile_load_toplevel_list (client, dir);

	g_free (dir);

	g_object_unref (client);
}

static gboolean
get_program_listing_setting (const char *setting)
{
	GConfClient *client;
	const char  *key;
	gboolean     retval;

	client = gconf_client_get_default ();

	key = panel_gconf_sprintf ("%s/general/%s", current_profile, setting);
	retval = gconf_client_get_bool (client, key, NULL);

	g_object_unref (client);

	return retval;
}

gboolean
panel_profile_get_show_program_list (void)
{
	return get_program_listing_setting ("show_program_list");
}

gboolean
panel_profile_get_enable_program_list (void)
{
	return get_program_listing_setting ("enable_program_list");
}

void
panel_profile_set_show_program_list (gboolean show_program_list)
{
	GConfClient *client;
	const char  *key;

	client = gconf_client_get_default ();

	key = panel_gconf_sprintf ("%s/general/show_program_list", current_profile);
	gconf_client_set_bool (client, key, show_program_list, NULL);

	g_object_unref (client);
}
