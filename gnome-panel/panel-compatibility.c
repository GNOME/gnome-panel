/*
 * panel-compatibility.c: panel backwards compatibility support
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
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-compatibility.h"

#include <libgnome/gnome-i18n.h>

#include "panel-profile.h"
#include "panel-menu-bar.h"
#include "panel-applet-frame.h"
#include "panel-globals.h"

/* Incompatibilities:
 *
 *   Toplevels:
 *     + toplevel_id_list instead of panel_id_list.
 *     + the schemas for toplevels and panels are completely different.
 *
 *   All applets
 *     + s/panel_id/toplevel_id/
 *
 *   Drawers:
 *     + we ignore the old "parameters" setting.
 *     + s/unique-drawer-panel-id/attached_toplevel_id/
 *     + s/pixmap/custom_icon/
 *     + we should use the "usr_custom_icon" setting.
 *
 *   Menu buttons:
 *     + we ignore "main-menu", "global-main" and "main-menu-flags".
 *     + s/custom-icon/use_custom_icon/
 *     + s/custom-icon-file/custom_icon/
 *     + s/path/menu_path/
 *     + we now have use_menu_path. Need to figure out how this
 *       relates to the old main_menu and global_main flags.
 *
 *   Global config
 *     + need to figure out what to do about the old global config
 *       settings that now apply to individual panels
 */

typedef enum {
	PANEL_ORIENT_UP    = GNOME_Vertigo_PANEL_ORIENT_UP,
	PANEL_ORIENT_DOWN  = GNOME_Vertigo_PANEL_ORIENT_DOWN,
	PANEL_ORIENT_LEFT  = GNOME_Vertigo_PANEL_ORIENT_LEFT,
	PANEL_ORIENT_RIGHT = GNOME_Vertigo_PANEL_ORIENT_RIGHT,
} PanelOrient;

static GConfEnumStringPair panel_orient_map [] = {
	{ PANEL_ORIENT_UP,    "panel-orient-up" },
	{ PANEL_ORIENT_DOWN,  "panel-orient-down" },
	{ PANEL_ORIENT_LEFT,  "panel-orient-left" },
	{ PANEL_ORIENT_RIGHT, "panel-orient-right" },
};

static gboolean
panel_compatibility_map_orient_string (const char  *str,
				       PanelOrient *orient)
{
	int mapped;

	g_return_val_if_fail (str != NULL, FALSE);
	g_return_val_if_fail (orient != NULL, FALSE);

	if (!gconf_string_to_enum (panel_orient_map, str, &mapped))
		return FALSE;

	*orient = mapped;

	return TRUE;
}

static GConfEnumStringPair panel_orientation_map [] = {
	{ GTK_ORIENTATION_HORIZONTAL, "panel-orientation-horizontal" },
	{ GTK_ORIENTATION_VERTICAL,   "panel-orientation-vertical" },
};


static gboolean
panel_compatibility_map_orientation_string (const char     *str,
					    GtkOrientation *orientation)
{
	int mapped;

	g_return_val_if_fail (str != NULL, FALSE);
	g_return_val_if_fail (orientation != NULL, FALSE);

	if (!gconf_string_to_enum (panel_orientation_map, str, &mapped))
		return FALSE;

	*orientation = mapped;

	return TRUE;
}

typedef enum {
	BORDER_TOP,
	BORDER_RIGHT,
	BORDER_BOTTOM,
	BORDER_LEFT
} BorderEdge;

static GConfEnumStringPair panel_edge_map [] = {
	{ BORDER_TOP,    "panel-edge-top" },
	{ BORDER_RIGHT,  "panel-edge-right" },
	{ BORDER_BOTTOM, "panel-edge-bottom" },
	{ BORDER_LEFT,   "panel-edge-left" },
};

static gboolean
panel_compatibility_map_edge_string (const char *str,
				     BorderEdge *edge)
{
	int mapped;

	g_return_val_if_fail (str != NULL, FALSE);
	g_return_val_if_fail (edge != NULL, FALSE);

	if (!gconf_string_to_enum (panel_edge_map, str, &mapped))
		return FALSE;

	*edge = mapped;

	return TRUE;
}

typedef enum {
	EDGE_PANEL,
	DRAWER_PANEL,
	ALIGNED_PANEL,
	SLIDING_PANEL,
	FLOATING_PANEL,
	MENU_PANEL
} PanelType;

static GConfEnumStringPair panel_type_map [] = {
	{ EDGE_PANEL,      "edge-panel" },
	{ DRAWER_PANEL,    "drawer-panel" },
	{ ALIGNED_PANEL,   "aligned-panel" },
	{ SLIDING_PANEL,   "sliding-panel" },
	{ FLOATING_PANEL,  "floating-panel" },
	{ MENU_PANEL,      "menu-panel" },
};

static gboolean
panel_compatibility_map_panel_type_string (const char *str,
					   PanelType  *type)
{
	int mapped;

	g_return_val_if_fail (str != NULL, FALSE);
	g_return_val_if_fail (type != NULL, FALSE);

	if (!gconf_string_to_enum (panel_type_map, str, &mapped))
		return FALSE;

	*type = mapped;

	return TRUE;
}

static GConfEnumStringPair panel_size_map [] = {
	{ PANEL_SIZE_XX_SMALL, "panel-size-xx-small" },
	{ PANEL_SIZE_X_SMALL,  "panel-size-x-small" },
	{ PANEL_SIZE_SMALL,    "panel-size-small" },
	{ PANEL_SIZE_MEDIUM,   "panel-size-medium" },
	{ PANEL_SIZE_LARGE,    "panel-size-large" },
	{ PANEL_SIZE_X_LARGE,  "panel-size-x-large" },
	{ PANEL_SIZE_XX_LARGE, "panel-size-xx-large" },
};

static gboolean
panel_compatibility_map_panel_size_string (const char *str,
					   int        *size)
{
	int mapped;

	g_return_val_if_fail (str != NULL, FALSE);
	g_return_val_if_fail (size != NULL, FALSE);

	if (!gconf_string_to_enum (panel_size_map, str, &mapped))
		return FALSE;

	*size = mapped;

	return TRUE;
}

static void
panel_compatibility_migrate_background_settings (GConfClient *client,
						 const char  *toplevel_dir,
						 const char  *panel_dir)
{
	g_warning ("FIXME: implement migrating the panel background settings");
}

static void
panel_compatibility_migrate_edge_panel_settings (GConfClient *client,
						 const char  *toplevel_dir,
						 const char  *panel_dir)
{
	BorderEdge  edge;
	const char *key;
	char       *edge_str;

	key = panel_gconf_sprintf ("%s/screen_edge", panel_dir);
	edge_str = gconf_client_get_string (client, key, NULL);

	if (panel_compatibility_map_edge_string (edge_str, &edge)) {
		PanelOrientation orientation;

		switch (edge) {
		case BORDER_TOP:
			orientation = PANEL_ORIENTATION_TOP;
			break;
		case BORDER_BOTTOM:
			orientation = PANEL_ORIENTATION_BOTTOM;
			break;
		case BORDER_LEFT:
			orientation = PANEL_ORIENTATION_LEFT;
			break;
		case BORDER_RIGHT:
			orientation = PANEL_ORIENTATION_RIGHT;
			break;
		default:
			orientation = 0;
			g_assert_not_reached ();
			break;
		}

		key = panel_gconf_sprintf ("%s/orientation", toplevel_dir);
		gconf_client_set_string (client,
					 key,
					 panel_profile_map_orientation (orientation),
					 NULL);
	}

	g_free (edge_str);
}

static void
panel_compatibility_migrate_drawer_panel_settings (GConfClient *client,
						   const char  *toplevel_dir,
						   const char  *panel_dir)
{
	PanelOrient  orient;
	const char  *key;
	char        *orient_str;

	key = panel_gconf_sprintf ("%s/panel_orient", panel_dir);
	orient_str = gconf_client_get_string (client, key, NULL);

	if (panel_compatibility_map_orient_string (orient_str, &orient)) {
		PanelOrientation orientation;

		switch (orient) {
		case PANEL_ORIENT_DOWN:
			orientation = PANEL_ORIENTATION_TOP;
			break;
		case PANEL_ORIENT_UP:
			orientation = PANEL_ORIENTATION_BOTTOM;
			break;
		case PANEL_ORIENT_RIGHT:
			orientation = PANEL_ORIENTATION_LEFT;
			break;
		case PANEL_ORIENT_LEFT:
			orientation = PANEL_ORIENTATION_RIGHT;
			break;
		default:
			orientation = 0;
			g_assert_not_reached ();
			break;
		}

		key = panel_gconf_sprintf ("%s/orientation", toplevel_dir);
		gconf_client_set_string (client,
					 key,
					 panel_profile_map_orientation (orientation),
					 NULL);
	}

	g_free (orient_str);
}

static void
panel_compatibility_migrate_corner_panel_settings (GConfClient *client,
						   const char  *toplevel_dir,
						   const char  *panel_dir)

{
	/* screen edge */
	panel_compatibility_migrate_edge_panel_settings (client, toplevel_dir, panel_dir);

	g_warning ("FIXME: implement migrating the 'panel_align' setting");
}

static void
panel_compatibility_migrate_sliding_panel_settings (GConfClient *client,
						    const char  *toplevel_dir,
						    const char  *panel_dir)
{
	/* screen edge */
	panel_compatibility_migrate_edge_panel_settings (client, toplevel_dir, panel_dir);

	g_warning ("FIXME: implement migrating the 'panel_anchor' and 'panel_offset' settings");
}

static void
panel_compatibility_migrate_floating_panel_settings (GConfClient *client,
						     const char  *toplevel_dir,
						     const char  *panel_dir)
{
	GtkOrientation  orientation;
	const char     *key;
	char           *orientation_str;
	int             x, y;

	key = panel_gconf_sprintf ("%s/panel_orient", panel_dir);
	orientation_str = gconf_client_get_string (client, key, NULL);

	if (panel_compatibility_map_orientation_string (orientation_str, &orientation)) {
		PanelOrientation panel_orientation;

		switch (orientation) {
		case GTK_ORIENTATION_HORIZONTAL:
			panel_orientation = PANEL_ORIENTATION_TOP;
			break;
		case GTK_ORIENTATION_VERTICAL:
			panel_orientation = PANEL_ORIENTATION_LEFT;
			break;
		default:
			panel_orientation = 0;
			g_assert_not_reached ();
			break;
		}

		key = panel_gconf_sprintf ("%s/orientation", toplevel_dir);
		gconf_client_set_string (client,
					 key,
					 panel_profile_map_orientation (panel_orientation),
					 NULL);
	}

	g_free (orientation_str);

	/* x */
	key = panel_gconf_sprintf ("%s/panel_x_position", panel_dir);
	x = gconf_client_get_int (client, key, NULL);

	key = panel_gconf_sprintf ("%s/x", toplevel_dir);
	gconf_client_set_int (client, key, x, NULL);

	/* y */
	key = panel_gconf_sprintf ("%s/panel_y_position", panel_dir);
	y = gconf_client_get_int (client, key, NULL);

	key = panel_gconf_sprintf ("%s/y", toplevel_dir);
	gconf_client_set_int (client, key, y, NULL);
}

static void
panel_compatibility_migrate_menu_panel_settings (GConfClient *client,
						      const char  *toplevel_dir,
						      const char  *panel_dir)
{
	g_warning ("FIXME: implement migrating menu panel settings");
}

static void
panel_compatibility_migrate_panel_types (GConfClient *client,
					 const char  *toplevel_dir,
					 const char  *panel_dir)
{
	PanelType   type;
	const char *key;
	char       *type_str;

	key = panel_gconf_sprintf ("%s/panel_type", panel_dir);
	type_str = gconf_client_get_string (client, key, NULL);

	if (!panel_compatibility_map_panel_type_string (type_str, &type)) {
		g_free (type_str);
		return;
	}

	g_free (type_str);

	switch (type) {
	case EDGE_PANEL:
		panel_compatibility_migrate_edge_panel_settings (client, toplevel_dir, panel_dir);
		break;
	case DRAWER_PANEL:
		panel_compatibility_migrate_drawer_panel_settings (client, toplevel_dir, panel_dir);
		break;
	case ALIGNED_PANEL:
		panel_compatibility_migrate_corner_panel_settings (client, toplevel_dir, panel_dir);
		break;
	case SLIDING_PANEL:
		panel_compatibility_migrate_sliding_panel_settings (client, toplevel_dir, panel_dir);
		break;
	case FLOATING_PANEL:
		panel_compatibility_migrate_floating_panel_settings (client, toplevel_dir, panel_dir);
		break;
	case MENU_PANEL:
		panel_compatibility_migrate_menu_panel_settings (client, toplevel_dir, panel_dir);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static char *
panel_compatibility_migrate_panel_settings (GConfClient *client,
					    GSList      *toplevel_id_list,
					    const char  *panel_id)
{
	const char *profile;
	const char *key;
	char       *toplevel_id;
	char       *toplevel_dir;
	char       *panel_dir;
	char       *size_str;
	int         screen;
	int         monitor;
	int         size;
	gboolean    enable_buttons;
	gboolean    enable_arrows;

	profile = panel_profile_get_name ();

	toplevel_id = panel_profile_find_new_id (PANEL_GCONF_TOPLEVELS, toplevel_id_list);

	toplevel_dir = g_strdup_printf (PANEL_CONFIG_DIR "/%s/toplevels/%s", profile, toplevel_id);
	panel_dir    = g_strdup_printf (PANEL_CONFIG_DIR "/%s/panels/%s", profile, panel_id);

	panel_gconf_associate_schemas_in_dir (
			client, toplevel_dir, PANEL_SCHEMAS_DIR "/toplevels");

	/* screen */
	key = panel_gconf_sprintf ("%s/screen", panel_dir);
	screen = gconf_client_get_int (client, key, NULL);

	key = panel_gconf_sprintf ("%s/screen", toplevel_dir);
	gconf_client_set_int (client, key, screen, NULL);

	/* monitor */
	key = panel_gconf_sprintf ("%s/monitor", panel_dir);
	monitor = gconf_client_get_int (client, key, NULL);

	key = panel_gconf_sprintf ("%s/monitor", toplevel_dir);
	gconf_client_set_int (client, key, monitor, NULL);

	/* size */
	key = panel_gconf_sprintf ("%s/panel_size", panel_dir);
	size_str = gconf_client_get_string (client, key, NULL);

	if (panel_compatibility_map_panel_size_string (size_str, &size)) {
		key = panel_gconf_sprintf ("%s/size", toplevel_dir);
		gconf_client_set_int (client, key, size, NULL);
	}

	g_free (size_str);

	/* enable_buttons */
	key = panel_gconf_sprintf ("%s/hide_buttons_enabled", panel_dir);
	enable_buttons = gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_sprintf ("%s/enable_buttons", toplevel_dir);
	gconf_client_set_bool (client, key, enable_buttons, NULL);

	/* enable_arrows */
	key = panel_gconf_sprintf ("%s/hide_button_pixmaps_enabled", panel_dir);
	enable_arrows = gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_sprintf ("%s/enable_arrows", toplevel_dir);
	gconf_client_set_bool (client, key, enable_arrows, NULL);

	/* migrate different panel types to toplevels */
	panel_compatibility_migrate_panel_types (client, toplevel_dir, panel_dir);

	/* background settings */
	panel_compatibility_migrate_background_settings (client, toplevel_dir, panel_dir);

	g_free (toplevel_dir);	
	g_free (panel_dir);

	return toplevel_id;
}


/* If toplevel_id_list is unset, migrate all the panels in
 * panel_id_list to toplevels
 */
void
panel_compatibility_migrate_panel_id_list (GConfClient *client)
{
	GConfValue *value;
	GError     *error = NULL;
	const char *profile;
	const char *key;
	GSList     *panel_id_list;
	GSList     *toplevel_id_list = NULL;
	GSList     *l;

	profile = panel_profile_get_name ();

	key = panel_gconf_general_key (profile, "toplevel_id_list");
	value = gconf_client_get (client, key, &error);
	if (error) {
		g_warning ("Error reading GConf value from '%s': %s", key, error->message);
		g_error_free (error);
		return;
	}

	if (value) {
		gconf_value_free (value);
		return;
	}

	key = panel_gconf_general_key (profile, "panel_id_list");
	panel_id_list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	for (l = panel_id_list; l; l = l->next) {
		char *new_id;

		new_id = panel_compatibility_migrate_panel_settings (client,
								     toplevel_id_list,
								     l->data);
		toplevel_id_list = g_slist_prepend (toplevel_id_list, new_id);
		g_free (l->data);
	}
	g_slist_free (panel_id_list);

	key = panel_gconf_general_key (profile, "toplevel_id_list");
	gconf_client_set_list (client, key, GCONF_VALUE_STRING, toplevel_id_list, NULL);

	for (l = toplevel_id_list; l; l = l->next)
		g_free (l->data);
	g_slist_free (toplevel_id_list);
}


#ifdef FIXME_FOR_NEW_TOPLEVEL
static void
panel_compatibility_warn (const char *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
			NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			message);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_widget_show (dialog);
}

GtkWidget *
panel_compatibility_load_menu_panel (const char *panel_id,
				     int         screen,
				     int         monitor)
{
	PanelColor  color = { { 0, 0, 0, 0 }, 0xffff };
	GtkWidget  *retval;

	panel_compatibility_warn (_("This is your lucky day. I've just broken compatibility with GNOME 2.0 and 2.2 by converting your menu panel into an edge panel with two applets. If you log back into GNOME 2.0/2.2 you will find that instead of having a menu panel you will have an edge panel on the top of your screen without an Applications/Actions menu or a Window Menu. This will will be fixed before GNOME 2.4 gets anywhere near release."));

	/* A menu panel was like a x-small edge panel at the
	 * top of the screen.
	 */
	retval = edge_widget_new (panel_id,
				  screen,
				  monitor,
				  BORDER_TOP,
				  BASEP_EXPLICIT_HIDE,
				  BASEP_SHOWN,
				  PANEL_SIZE_X_SMALL,
				  FALSE,
				  FALSE,
				  PANEL_BACK_NONE,
				  NULL,
				  FALSE,
				  FALSE,
				  FALSE,
				  &color);

	g_object_set_data (G_OBJECT (BASEP_WIDGET (retval)->panel),
			   "load-compatibility-applets", GINT_TO_POINTER (1));

	return retval;
}
#endif /* FIXME_FOR_NEW_TOPLEVEL */

void
panel_compatibility_load_applets (void)
{
	GSList *l;

	for (l = panels; l; l = l->next) {
		PanelWidget *panel = l->data;

		if (!g_object_get_data (G_OBJECT (panel), "load-compatibility-applets"))
			continue;

		g_object_set_data (G_OBJECT (panel), "load-compatibility-applets", NULL);

		/* A menu panel contained a menu bar on the far left
	         * and a window menu on the far right.
		 */
		/* FIXME_FOR_NEW_CONFIG : panel_menu_bar_load (panel, 0, TRUE, NULL); */

		panel_applet_frame_create (panel->toplevel,
					   panel->size - 10,
					   "OAFIID:GNOME_WindowMenuApplet");
	}
}
