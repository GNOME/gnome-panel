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

#include "panel-profile.h"
#include "panel-menu-bar.h"
#include "panel-applet-frame.h"
#include "panel-globals.h"

typedef enum {
	PANEL_ORIENT_UP    = GNOME_Vertigo_PANEL_ORIENT_UP,
	PANEL_ORIENT_DOWN  = GNOME_Vertigo_PANEL_ORIENT_DOWN,
	PANEL_ORIENT_LEFT  = GNOME_Vertigo_PANEL_ORIENT_LEFT,
	PANEL_ORIENT_RIGHT = GNOME_Vertigo_PANEL_ORIENT_RIGHT
} PanelOrient;

static GConfEnumStringPair panel_orient_map [] = {
	{ PANEL_ORIENT_UP,    "panel-orient-up" },
	{ PANEL_ORIENT_DOWN,  "panel-orient-down" },
	{ PANEL_ORIENT_LEFT,  "panel-orient-left" },
	{ PANEL_ORIENT_RIGHT, "panel-orient-right" }
};

static gboolean
panel_compatibility_map_orient_string (const char  *str,
				       PanelOrient *orient)
{
	int mapped;

	g_return_val_if_fail (orient != NULL, FALSE);

	if (!str)
		return FALSE;

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

	g_return_val_if_fail (orientation != NULL, FALSE);

	if (!str)
		return FALSE;

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

	g_return_val_if_fail (edge != NULL, FALSE);

	if (!str)
		return FALSE;

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

	g_return_val_if_fail (type != NULL, FALSE);

	if (!str)
		return FALSE;

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

	g_return_val_if_fail (size != NULL, FALSE);

	if (!str)
		return FALSE;

	if (!gconf_string_to_enum (panel_size_map, str, &mapped))
		return FALSE;

	*size = mapped;

	return TRUE;
}

static GConfEnumStringPair panel_background_type_map [] = {
	{ PANEL_BACK_NONE,   "no-background" },
	{ PANEL_BACK_COLOR,  "color-background" },
	{ PANEL_BACK_IMAGE,  "pixmap-background" },
};

static gboolean
panel_compatibility_map_background_type_string (const char          *str,
						PanelBackgroundType *type)
{
	int mapped;

	g_return_val_if_fail (type != NULL, FALSE);

	if (!str)
		return FALSE;

	if (!gconf_string_to_enum (panel_background_type_map, str, &mapped))
		return FALSE;

	*type = mapped;

	return TRUE;
}

static void
panel_compatibility_migrate_background_settings (GConfClient *client,
						 const char  *toplevel_dir,
						 const char  *panel_dir)
{
	PanelBackgroundType  type;
	const char          *key;
	char                *background_dir;
	char                *type_str;
	char                *color_str;
	char                *image_str;
	gboolean             fit;
	gboolean             stretch;
	gboolean             rotate;
	int                  opacity;

	background_dir = gconf_concat_dir_and_key (toplevel_dir, "background");

	/* panel_background_type -> background/type */
	key = panel_gconf_sprintf ("%s/panel_background_type", panel_dir);
	type_str = gconf_client_get_string (client, key, NULL);

	if (panel_compatibility_map_background_type_string (type_str, &type)) {
		key = panel_gconf_sprintf ("%s/type", background_dir);
		gconf_client_set_string (client,
					 key,
					 panel_profile_map_background_type (type),
					 NULL);
	}

	g_free (type_str);

	/* panel_background_color -> background/color */
	key = panel_gconf_sprintf ("%s/panel_background_color", panel_dir);
	color_str = gconf_client_get_string (client, key, NULL);

	if (color_str) {
		key = panel_gconf_sprintf ("%s/color", background_dir);
		gconf_client_set_string (client, key, color_str, NULL);
	}

	g_free (color_str);

	/* panel_background_color_alpha -> background/opacity */
	key = panel_gconf_sprintf ("%s/panel_background_color_alpha", panel_dir);
	opacity = gconf_client_get_int (client, key, NULL);

	key = panel_gconf_sprintf ("%s/opacity", background_dir);
	gconf_client_set_int (client, key, opacity, NULL);

	/* panel_background_pixmap -> background/image */
	key = panel_gconf_sprintf ("%s/panel_background_pixmap", panel_dir);
	image_str = gconf_client_get_string (client, key, NULL);

	if (image_str) {
		key = panel_gconf_sprintf ("%s/image", background_dir);
		gconf_client_set_string (client, key, image_str, NULL);
	}

	g_free (image_str);

	/* panel_background_pixmap_fit -> background/fit */
	key = panel_gconf_sprintf ("%s/panel_background_pixmap_fit", panel_dir);
	fit = gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_sprintf ("%s/fit", background_dir);
	gconf_client_set_bool (client, key, fit, NULL);

	/* panel_background_pixmap_stretch -> background/stretch */
	key = panel_gconf_sprintf ("%s/panel_background_pixmap_stretch", panel_dir);
	stretch = gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_sprintf ("%s/stretch", background_dir);
	gconf_client_set_bool (client, key, stretch, NULL);

	/* panel_background_pixmap_rotate -> background/rotate */
	key = panel_gconf_sprintf ("%s/panel_background_pixmap_rotate", panel_dir);
	rotate = gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_sprintf ("%s/rotate", background_dir);
	gconf_client_set_bool (client, key, rotate, NULL);

	g_free (background_dir);
}

static void
panel_compatibility_migrate_edge_setting (GConfClient *client,
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
panel_compatibility_migrate_edge_panel_settings (GConfClient *client,
						 const char  *toplevel_dir,
						 const char  *panel_dir)
{
	const char *key;

	key = panel_gconf_sprintf ("%s/expand", toplevel_dir);
	gconf_client_set_bool (client, key, TRUE, NULL);

	panel_compatibility_migrate_edge_setting (client, toplevel_dir, panel_dir);
}

static void
panel_compatibility_migrate_drawer_panel_settings (GConfClient *client,
						   const char  *toplevel_dir,
						   const char  *panel_dir)
{
	PanelOrient  orient;
	const char  *key;
	char        *orient_str;

	key = panel_gconf_sprintf ("%s/expand", toplevel_dir);
	gconf_client_set_bool (client, key, FALSE, NULL);

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
	const char *key;

	key = panel_gconf_sprintf ("%s/expand", toplevel_dir);
	gconf_client_set_bool (client, key, FALSE, NULL);

	/* screen edge */
	panel_compatibility_migrate_edge_setting (client, toplevel_dir, panel_dir);

	g_warning ("FIXME: implement migrating the 'panel_align' setting");
}

static void
panel_compatibility_migrate_sliding_panel_settings (GConfClient *client,
						    const char  *toplevel_dir,
						    const char  *panel_dir)
{
	const char *key;

	key = panel_gconf_sprintf ("%s/expand", toplevel_dir);
	gconf_client_set_bool (client, key, FALSE, NULL);

	/* screen edge */
	panel_compatibility_migrate_edge_setting (client, toplevel_dir, panel_dir);

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

	key = panel_gconf_sprintf ("%s/expand", toplevel_dir);
	gconf_client_set_bool (client, key, FALSE, NULL);

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
	const char *key;
	const char *profile;
	const char *toplevel_id;
	char       *id;

	profile = panel_profile_get_name ();

	key = panel_gconf_sprintf ("%s/expand", toplevel_dir);
	gconf_client_set_bool (client, key, TRUE, NULL);

	key = panel_gconf_sprintf ("%s/orientation", toplevel_dir);
	gconf_client_set_string (client, key,
				 panel_profile_map_orientation (PANEL_ORIENTATION_TOP),
				 NULL);

	toplevel_id = panel_gconf_basename (toplevel_dir);

	/* menu bar on far right corner */
        id = panel_profile_prepare_object_with_id (PANEL_OBJECT_MENU_BAR, toplevel_id, 0, FALSE);
        panel_profile_add_to_list (PANEL_GCONF_OBJECTS, id);
	g_free (id);

	/* window menu on far left corner */
        id = panel_profile_prepare_object_with_id (PANEL_OBJECT_BONOBO, toplevel_id, 0, TRUE);

	key = panel_gconf_full_key (PANEL_GCONF_APPLETS, profile, id, "bonobo_iid");
        gconf_client_set_string (client, key, "OAFIID:GNOME_WindowMenuApplet", NULL);

        panel_profile_add_to_list (PANEL_GCONF_APPLETS, id);
	g_free (id);
}

static void
panel_compatibility_migrate_panel_type (GConfClient *client,
					const char  *toplevel_dir,
					const char  *panel_dir,
					gboolean    *is_drawer)
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
		*is_drawer = TRUE;
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
					    const char  *panel_id,
					    gboolean    *is_drawer)
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
	gboolean    auto_hide;

	profile = panel_profile_get_name ();

	toplevel_id = panel_profile_find_new_id (PANEL_GCONF_TOPLEVELS);

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

	/* auto hide */
	key = panel_gconf_sprintf ("%s/panel_hide_mode", panel_dir);
	auto_hide = gconf_client_get_int (client, key, NULL);

	key = panel_gconf_sprintf ("%s/auto_hide", toplevel_dir);
	gconf_client_set_bool (client, key, auto_hide, NULL);

	/* migrate different panel types to toplevels */
	panel_compatibility_migrate_panel_type (client, toplevel_dir, panel_dir, is_drawer);

	/* background settings */
	panel_compatibility_migrate_background_settings (client, toplevel_dir, panel_dir);

	g_free (toplevel_dir);	
	g_free (panel_dir);

	return toplevel_id;
}

static gboolean
panel_compatibility_migrate_panel_id (GConfClient       *client,
				      PanelGConfKeyType  key_type,
				      const char        *object_id,
				      GHashTable        *panel_id_hash)
{
	const char *profile;
	const char *key;
	char       *panel_id;
	char       *toplevel_id;
	gboolean    retval = FALSE;

	profile = panel_profile_get_name ();

	/* panel_id -> toplevel_id */
	key = panel_gconf_full_key (key_type, profile, object_id, "panel_id");
	panel_id = gconf_client_get_string (client, key, NULL);

	if (panel_id && (toplevel_id = g_hash_table_lookup (panel_id_hash, panel_id))) {
		key = panel_gconf_full_key (key_type, profile, object_id, "toplevel_id");
		gconf_client_set_string (client, key, toplevel_id, NULL);

		retval = TRUE;
	}

	g_free (panel_id);

	return retval;
}

static void
panel_compatibility_migrate_drawer_settings (GConfClient       *client,
					     PanelGConfKeyType  key_type,
					     const char        *object_id,
					     GHashTable        *panel_id_hash)
{
	const char *profile;
	const char *key;
	char       *toplevel_id;
	char       *panel_id;
	char       *custom_icon;
	char       *pixmap;

	profile = panel_profile_get_name ();

	/* unique-drawer-panel-id -> attached_toplevel_id */
	key = panel_gconf_full_key (key_type, profile, object_id, "attached_toplevel_id");
	toplevel_id = gconf_client_get_string (client, key, NULL);

	key = panel_gconf_full_key (key_type, profile, object_id, "unique-drawer-panel-id");
	panel_id = gconf_client_get_string (client, key, NULL);

	if (!toplevel_id && panel_id &&
	    (toplevel_id = g_hash_table_lookup (panel_id_hash, panel_id))) {
		key = panel_gconf_full_key (key_type, profile, object_id, "attached_toplevel_id");
		gconf_client_set_string (client, key, toplevel_id, NULL);

		toplevel_id = NULL;
	}

	/* pixmap -> custom_icon */	
	key = panel_gconf_full_key (key_type, profile, object_id, "custom_icon");
	custom_icon = gconf_client_get_string (client, key, NULL);

	key = panel_gconf_full_key (key_type, profile, object_id, "pixmap");
	pixmap = gconf_client_get_string (client, key, NULL);

	if (!custom_icon && pixmap) {
		key = panel_gconf_full_key (key_type, profile, object_id, "custom_icon");
		gconf_client_set_string (client, key, pixmap, NULL);

		key = panel_gconf_full_key (key_type, profile, object_id, "use_custom_icon");
		gconf_client_set_bool (client, key, TRUE, NULL);
	}

	g_free (toplevel_id);
	g_free (panel_id);
	g_free (custom_icon);
	g_free (pixmap);
}

static void
panel_compatibility_migrate_menu_button_settings (GConfClient       *client,
						  PanelGConfKeyType  key_type,
						  const char        *object_id)
{
	const char *profile;
	const char *key;
	gboolean    use_custom_icon;
	gboolean    use_menu_path;
	char       *custom_icon;
	char       *menu_path;

	profile = panel_profile_get_name ();

	/* custom-icon -> use_custom_icon */
	key = panel_gconf_full_key (key_type, profile, object_id, "custom-icon");
	use_custom_icon = gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_full_key (key_type, profile, object_id, "use_custom_icon");
	gconf_client_set_bool (client, key, use_custom_icon, NULL);

	/* custom-icon-file -> custom_icon */
	key = panel_gconf_full_key (key_type, profile, object_id, "custom-icon-file");
	custom_icon = gconf_client_get_string (client, key, NULL);

	if (custom_icon) {
		key = panel_gconf_full_key (key_type, profile, object_id, "custom_icon");
		gconf_client_set_string (client, key, custom_icon, NULL);
	}

	/* main_menu -> ! use_menu_path */
	key = panel_gconf_full_key (key_type, profile, object_id, "main-menu");
	use_menu_path = ! gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_full_key (key_type, profile, object_id, "use_menu_path");
	gconf_client_set_bool (client, key, use_menu_path, NULL);

	/* path -> menu_path */
	key = panel_gconf_full_key (key_type, profile, object_id, "path");
	menu_path = gconf_client_get_string (client, key, NULL);

	if (menu_path) {
		key = panel_gconf_full_key (key_type, profile, object_id, "menu_path");
		gconf_client_set_string (client, key, menu_path, NULL);
	}

	g_free (custom_icon);
	g_free (menu_path);
}

static void
panel_compatibility_migrate_objects (GConfClient       *client,
				     PanelGConfKeyType  key_type,
				     GHashTable        *panel_id_hash)
{
	const char *key;
	const char *profile;
	GSList     *l, *objects;

	profile = panel_profile_get_name ();

	key = panel_gconf_general_key (profile, panel_gconf_key_type_to_id_list (key_type));
	objects = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	for (l = objects; l; l = l->next) {
		const char      *id = l->data;
		PanelObjectType  object_type;
		char            *object_type_str;

		if (!panel_compatibility_migrate_panel_id (client, key_type, id, panel_id_hash)) {
			g_free (l->data);
			continue;
		}

		key = panel_gconf_full_key (key_type, profile, id, "object_type");
		object_type_str = gconf_client_get_string (client, key, NULL);

		if (panel_profile_map_object_type_string (object_type_str, &object_type)) {
			switch (object_type) {
			case PANEL_OBJECT_DRAWER:
				panel_compatibility_migrate_drawer_settings (
						client, key_type, id, panel_id_hash);
				break;
			case PANEL_OBJECT_MENU:
				panel_compatibility_migrate_menu_button_settings (
						client, key_type, id);
				break;
			default:
				break;
			}
		}

		g_free (l->data);
	}
	g_slist_free (objects);
}

/* Major hack, but we now set toplevel_id_list in the defaults database,
 * so we need to figure out if its actually set in the users database.
 */
static gboolean
panel_compatibility_detect_needs_migration (const char *profile)
{
	GConfEngine *engine;
	GConfValue  *value;
	GError      *error = NULL;
	char        *source;
	const char  *key;
	gboolean     needs_migration = FALSE;

	source = g_strdup_printf ("xml:readwrite:%s/.gconf", g_get_home_dir ());

	if (!(engine = gconf_engine_get_for_address (source, &error))) {
#if 0
		g_warning ("Cannot get GConf source '%s': %s\n",
			   source, error->message);
#endif
		g_error_free (error);
		g_free (source);
		return FALSE;
	}

	g_free (source);

	key = panel_gconf_general_key (profile, "panel_id_list");
	if (!(value = gconf_engine_get_without_default (engine, key, NULL)))
		goto no_migration;

	gconf_value_free (value);

	key = panel_gconf_general_key (profile, "toplevel_id_list");
	value = gconf_engine_get_without_default (engine, key, &error);
	if (error) {
		g_warning ("Error reading GConf value from '%s': %s", key, error->message);
		g_error_free (error);
		goto no_migration;
	}

	if (value) {
		gconf_value_free (value);
		goto no_migration;
	}

	needs_migration = TRUE;

 no_migration:
	gconf_engine_unref (engine);

	return needs_migration;
}

/* If toplevel_id_list is unset, migrate all the panels in
 * panel_id_list to toplevels
 */
void
panel_compatibility_migrate_panel_id_list (GConfClient *client)
{
	GHashTable *panel_id_hash;
	const char *profile;
	const char *key;
	GSList     *panel_id_list;
	GSList     *toplevel_id_list = NULL;
	GSList     *l;

	profile = panel_profile_get_name ();

	if (!panel_compatibility_detect_needs_migration (profile))
		return;

	panel_id_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	key = panel_gconf_general_key (profile, "panel_id_list");
	panel_id_list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	for (l = panel_id_list; l; l = l->next) {
		char     *new_id;
		gboolean  is_drawer = FALSE;

		new_id = panel_compatibility_migrate_panel_settings (client,
								     toplevel_id_list,
								     l->data,
								     &is_drawer);

		/* Drawer toplevels don't belong on the toplevel list */
		if (!is_drawer)
			toplevel_id_list = g_slist_prepend (toplevel_id_list, new_id);

		g_hash_table_insert (panel_id_hash, l->data, new_id);
	}

	key = panel_gconf_general_key (profile, "toplevel_id_list");
	gconf_client_set_list (client, key, GCONF_VALUE_STRING, toplevel_id_list, NULL);

	g_slist_free (panel_id_list);
	g_slist_free (toplevel_id_list);

	panel_compatibility_migrate_objects (client, PANEL_GCONF_OBJECTS, panel_id_hash);
	panel_compatibility_migrate_objects (client, PANEL_GCONF_APPLETS, panel_id_hash);

	g_hash_table_destroy (panel_id_hash);
}
