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
#include <glib/gi18n.h>

#include <libpanel-util/panel-list.h>

#include "applet.h"
#include "panel-compatibility.h"
#include "panel-gconf.h"
#include "panel.h"
#include "panel-widget.h"
#include "panel-util.h"
#include "panel-multiscreen.h"
#include "panel-toplevel.h"
#include "panel-lockdown.h"

typedef struct {
	GdkScreen       *screen;
	int              monitor;
	int              size;
	int              x;
	int              x_right;
	gboolean         x_centered;
	int              y;
	int              y_bottom;
	gboolean         y_centered;
	PanelOrientation orientation;

	guint screen_changed : 1;
	guint monitor_changed : 1;
	guint size_changed : 1;
	guint x_changed : 1;
	guint x_right_changed : 1;
	guint x_centered_changed : 1;
	guint y_changed : 1;
	guint y_bottom_changed : 1;
	guint y_centered_changed : 1;
	guint orientation_changed : 1;
} ToplevelLocationChange;

typedef const char *(*PanelProfileGetIdFunc)   (gpointer           object);
typedef gboolean    (*PanelProfileOnLoadQueue) (const char        *id);
typedef void        (*PanelProfileLoadFunc)    (GConfClient       *client,
						const char        *profile_dir, 
						PanelGConfKeyType  type,
						const char        *id);
typedef void        (*PanelProfileDestroyFunc) (const char        *id);

static GConfEnumStringPair panel_orientation_map [] = {
	{ PANEL_ORIENTATION_TOP,    "top"    },
	{ PANEL_ORIENTATION_BOTTOM, "bottom" },
	{ PANEL_ORIENTATION_LEFT,   "left"   },
	{ PANEL_ORIENTATION_RIGHT,  "right"  },
	{ 0,                        NULL     }
};

static GConfEnumStringPair panel_animation_speed_map [] = {
	{ PANEL_ANIMATION_SLOW,   "slow"   },
	{ PANEL_ANIMATION_MEDIUM, "medium" },
	{ PANEL_ANIMATION_FAST,   "fast"   },
	{ 0,                      NULL     }
};

static GConfEnumStringPair panel_background_type_map [] = {
	{ PANEL_BACK_NONE,  "gtk"   },
	{ PANEL_BACK_COLOR, "color" },
	{ PANEL_BACK_IMAGE, "image" },
	{ 0,                NULL    }
};

static GConfEnumStringPair panel_object_type_map [] = {
	{ PANEL_OBJECT_DRAWER,    "drawer-object" },
	{ PANEL_OBJECT_MENU,      "menu-object" },
	{ PANEL_OBJECT_LAUNCHER,  "launcher-object" },
	{ PANEL_OBJECT_APPLET,    "external-applet" },
	{ PANEL_OBJECT_ACTION,    "action-applet" },
	{ PANEL_OBJECT_MENU_BAR,  "menu-bar" },
	{ PANEL_OBJECT_SEPARATOR, "separator" },
	/* The following is for backwards compatibility with 2.30.x and earlier */
	{ PANEL_OBJECT_APPLET,    "bonobo-applet" },
	/* The following two are for backwards compatibility with 2.0.x */
	{ PANEL_OBJECT_LOCK,      "lock-object" },
	{ PANEL_OBJECT_LOGOUT,    "logout-object" },
	{ 0,                      NULL }
};

static GQuark toplevel_id_quark = 0;
static GQuark queued_changes_quark = 0;
static GQuark commit_timeout_quark = 0;

static void panel_profile_object_id_list_update (GConfClient       *client,
						 GConfValue        *value,
						 PanelGConfKeyType  type);

gboolean
panel_profile_map_orientation_string (const char       *str,
				      PanelOrientation *orientation)
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

const char *
panel_profile_map_orientation (PanelOrientation orientation)
{
	return gconf_enum_to_string (panel_orientation_map, orientation);
}

gboolean
panel_profile_map_speed_string (const char          *str,
				PanelAnimationSpeed *speed)
{
	int mapped;

	g_return_val_if_fail (speed != NULL, FALSE);

	if (!str)
		return FALSE;

	if (!gconf_string_to_enum (panel_animation_speed_map, str, &mapped))
		return FALSE;

	*speed = mapped;

	return TRUE;
}

gboolean
panel_profile_map_background_type_string (const char          *str,
					  PanelBackgroundType *background_type)
{
	int mapped;

	g_return_val_if_fail (background_type != NULL, FALSE);

	if (!str)
		return FALSE;

	if (!gconf_string_to_enum (panel_background_type_map, str, &mapped))
		return FALSE;

	*background_type = mapped;

	return TRUE;
}

const char *
panel_profile_map_background_type (PanelBackgroundType  background_type)
{
	return gconf_enum_to_string (panel_background_type_map, background_type);
}

gboolean
panel_profile_map_object_type_string (const char       *str,
				      PanelObjectType  *object_type)
{
	int mapped;

	g_return_val_if_fail (object_type != NULL, FALSE);

	if (!str)
		return FALSE;

	if (!gconf_string_to_enum (panel_object_type_map, str, &mapped))
		return FALSE;

	*object_type = mapped;

	return TRUE;
}

static void
panel_profile_set_toplevel_id (PanelToplevel *toplevel,
			       const char    *id)
{
	if (!toplevel_id_quark)
		toplevel_id_quark = g_quark_from_static_string ("panel-toplevel-id");

	g_object_set_qdata_full (G_OBJECT (toplevel),
				 toplevel_id_quark,
				 g_strdup (id),
				 g_free);
}

const char *
panel_profile_get_toplevel_id (PanelToplevel *toplevel)
{
	if (!toplevel_id_quark)
		return NULL;

	return g_object_get_qdata (G_OBJECT (toplevel), toplevel_id_quark);
}

PanelToplevel *
panel_profile_get_toplevel_by_id (const char *toplevel_id)
{
	GSList *toplevels, *l;

	if (!toplevel_id || !toplevel_id [0])
		return NULL;

	toplevels = panel_toplevel_list_toplevels ();
	for (l = toplevels; l; l = l->next)
		if (!strcmp (panel_profile_get_toplevel_id (l->data), toplevel_id))
			return l->data;

	return NULL;
}

char *
panel_profile_find_new_id (PanelGConfKeyType type)
{
	GConfClient *client;
	GSList      *l, *existing_ids;
	const char  *key;
	char        *retval = NULL;
	char        *prefix;
	char        *dir;
	int          i;

	client  = panel_gconf_get_client ();

	switch (type) {
	case PANEL_GCONF_TOPLEVELS:
		prefix = "panel";
		dir = "toplevels";
		break;
	case PANEL_GCONF_OBJECTS:
		prefix = "object";
		dir = "objects";
		break;
	case PANEL_GCONF_APPLETS:
		prefix = "applet";
		dir = "applets";
		break;
	default:
		prefix = dir = NULL;
		g_assert_not_reached ();
		break;
	}

	key = panel_gconf_sprintf (PANEL_CONFIG_DIR "/%s", dir);
	existing_ids = gconf_client_all_dirs (client, key, NULL);

	for (i = 0; !retval; i++) {
		retval = g_strdup_printf ("%s_%d", prefix, i);

		for (l = existing_ids; l; l = l->next)
			if (!strcmp (panel_gconf_basename (l->data), retval)) {
				g_free (retval);
				retval = NULL;
				break;
			}
	}

	g_assert (retval != NULL);

	for (l = existing_ids; l; l = l->next)
		g_free (l->data);
	g_slist_free (existing_ids);

	return retval;
}

static void
panel_profile_set_queued_changes (PanelToplevel  *toplevel,
				  GConfChangeSet *changes)
{
	if (!queued_changes_quark)
		queued_changes_quark = g_quark_from_static_string ("panel-queued-changes");

	g_object_set_qdata_full (G_OBJECT (toplevel),
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
panel_profile_remove_commit_timeout (guint timeout)
{
	g_source_remove (timeout);
}

static void
panel_profile_set_commit_timeout (PanelToplevel *toplevel,
				  guint          timeout)
{
	GDestroyNotify destroy_notify;

	if (!commit_timeout_quark)
		commit_timeout_quark = g_quark_from_static_string ("panel-queued-timeout");

	if (timeout)
		destroy_notify = (GDestroyNotify) panel_profile_remove_commit_timeout;
	else
		destroy_notify = NULL;

	g_object_set_qdata_full (G_OBJECT (toplevel),
				 commit_timeout_quark,
				 GUINT_TO_POINTER (timeout),
				 destroy_notify);
}

static guint
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

	return panel_gconf_full_key (PANEL_GCONF_TOPLEVELS, id, key);
}

#define TOPLEVEL_IS_WRITABLE_FUNC(k, p, s)                            \
	gboolean                                                      \
	panel_profile_is_writable_##p##_##s (PanelToplevel *toplevel) \
	{                                                             \
		GConfClient *client;                                  \
		const char  *key;                                     \
		client = panel_gconf_get_client ();                   \
		key = panel_profile_get_toplevel_key (toplevel, k);   \
		return gconf_client_key_is_writable (client, key, NULL); \
	}

void
panel_profile_set_background_type (PanelToplevel       *toplevel,
				   PanelBackgroundType  background_type)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_profile_get_toplevel_key (toplevel, "background/type");
	gconf_client_set_string (client,
				 key,
				 panel_profile_map_background_type (background_type),
			         NULL);
}

PanelBackgroundType
panel_profile_get_background_type (PanelToplevel *toplevel)
{
	PanelBackgroundType  background_type;
	GConfClient         *client;
	const char          *key;
	char                *str;

	client = panel_gconf_get_client ();

	key = panel_profile_get_toplevel_key (toplevel, "background/type");
	str = gconf_client_get_string (client, key, NULL);

	if (!str || !panel_profile_map_background_type_string (str, &background_type))
		background_type = PANEL_BACK_NONE;

	g_free (str);

	return background_type;
}

TOPLEVEL_IS_WRITABLE_FUNC ("background/type", background, type)

void
panel_profile_set_background_color (PanelToplevel *toplevel,
				    PanelColor    *color)
{
	panel_profile_set_background_gdk_color (toplevel, &color->gdk);
	panel_profile_set_background_opacity (toplevel, color->alpha);
}

void
panel_profile_get_background_color (PanelToplevel *toplevel,
				    PanelColor    *color)
{
	panel_profile_get_background_gdk_color (toplevel, &(color->gdk));
	color->alpha = panel_profile_get_background_opacity (toplevel);
}

TOPLEVEL_IS_WRITABLE_FUNC ("background/color", background, color)

void
panel_profile_set_background_gdk_color (PanelToplevel *toplevel,
					GdkColor      *gdk_color)
{
	GConfClient *client;
	const char  *key;
	char        *color_str;

	client = panel_gconf_get_client ();

	color_str = g_strdup_printf ("#%02x%02x%02x",
				     gdk_color->red   / 256,
				     gdk_color->green / 256,
				     gdk_color->blue  / 256);

	key = panel_profile_get_toplevel_key (toplevel, "background/color");
	gconf_client_set_string (client, key, color_str, NULL);

	g_free (color_str);
}

void
panel_profile_get_background_gdk_color (PanelToplevel *toplevel,
					GdkColor      *gdk_color)
{
	GConfClient *client;
	const char  *key;
	char        *color_str;

	client = panel_gconf_get_client ();

	key = panel_profile_get_toplevel_key (toplevel, "background/color");
	color_str = gconf_client_get_string (client, key, NULL);
	if (!color_str || !gdk_color_parse (color_str, gdk_color)) {
		gdk_color->red   = 0;
		gdk_color->green = 0;
		gdk_color->blue  = 0;
	}

	g_free (color_str);
}

void
panel_profile_set_background_opacity (PanelToplevel *toplevel,
				      guint16        opacity)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_profile_get_toplevel_key (toplevel, "background/opacity");
	gconf_client_set_int (client, key, opacity, NULL);
}

guint16
panel_profile_get_background_opacity (PanelToplevel *toplevel)
{
	GConfClient *client;
	const char  *key;
	guint16      opacity;

	client = panel_gconf_get_client ();

	key = panel_profile_get_toplevel_key (toplevel, "background/opacity");
	opacity = gconf_client_get_int (client, key, NULL);

	return opacity;
}

TOPLEVEL_IS_WRITABLE_FUNC ("background/opacity", background, opacity)

void
panel_profile_set_background_image (PanelToplevel *toplevel,
				    const char    *image)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_profile_get_toplevel_key (toplevel, "background/image");

	if (image && image [0])
		gconf_client_set_string (client, key, image, NULL);
	else
		gconf_client_unset (client, key, NULL);
}

char *
panel_profile_get_background_image (PanelToplevel *toplevel)
{
	GConfClient *client;
	const char  *key;
	char        *retval;

	client = panel_gconf_get_client ();

	key = panel_profile_get_toplevel_key (toplevel, "background/image");
	retval = gconf_client_get_string (client, key, NULL);

	return retval;
}

TOPLEVEL_IS_WRITABLE_FUNC ("background/image", background, image)

void
panel_profile_set_toplevel_name (PanelToplevel *toplevel,
				 const char    *name)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_profile_get_toplevel_key (toplevel, "name");

	if (name && name [0])
		gconf_client_set_string (client, key, name, NULL);
	else
		gconf_client_unset (client, key, NULL);
}

char *
panel_profile_get_toplevel_name (PanelToplevel *toplevel)
{
	GConfClient *client;
	const char  *key;
	char        *retval;

	client = panel_gconf_get_client ();

	key = panel_profile_get_toplevel_key (toplevel, "name");
	retval = gconf_client_get_string (client, key, NULL);

	return retval;
}

TOPLEVEL_IS_WRITABLE_FUNC ("name", toplevel, name)

void
panel_profile_set_toplevel_orientation (PanelToplevel    *toplevel,
					PanelOrientation  orientation)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_profile_get_toplevel_key (toplevel, "orientation");
	gconf_client_set_string (client,
				 key,
				 panel_profile_map_orientation (orientation),
				 NULL);
}

PanelOrientation
panel_profile_get_toplevel_orientation (PanelToplevel *toplevel)
{
	PanelOrientation  orientation;
	GConfClient      *client;
	const char       *key;
	char             *str;

	client = panel_gconf_get_client ();

	key = panel_profile_get_toplevel_key (toplevel, "orientation");
	str = gconf_client_get_string (client, key, NULL);

	if (!panel_profile_map_orientation_string (str, &orientation))
	    orientation = panel_toplevel_get_orientation (toplevel);

	g_free (str);

	return orientation;
}

TOPLEVEL_IS_WRITABLE_FUNC ("orientation", toplevel, orientation)

#define TOPLEVEL_GET_SET_FUNCS(k, p, t, s, a)                         \
	void                                                          \
	panel_profile_set_##p##_##s (PanelToplevel *toplevel, a s)    \
	{                                                             \
		GConfClient *client;                                  \
		const char  *key;                                     \
		client = panel_gconf_get_client ();                   \
		key = panel_profile_get_toplevel_key (toplevel, k);   \
		gconf_client_set_##t (client, key, s, NULL);          \
	}                                                             \
	a                                                             \
	panel_profile_get_##p##_##s (PanelToplevel *toplevel)         \
	{                                                             \
		GConfClient *client;                                  \
		const char  *key;                                     \
		a retval;                                             \
		client = panel_gconf_get_client ();                   \
		key = panel_profile_get_toplevel_key (toplevel, k);   \
		retval = gconf_client_get_##t (client, key, NULL);    \
		return retval;                                        \
	}                                                             \
        TOPLEVEL_IS_WRITABLE_FUNC(k, p, s)

TOPLEVEL_GET_SET_FUNCS ("size",               toplevel,   int,  size,           int)
TOPLEVEL_GET_SET_FUNCS ("expand",             toplevel,   bool, expand,         gboolean)
TOPLEVEL_GET_SET_FUNCS ("auto_hide",          toplevel,   bool, auto_hide,      gboolean)
TOPLEVEL_GET_SET_FUNCS ("enable_buttons",     toplevel,   bool, enable_buttons, gboolean)
TOPLEVEL_GET_SET_FUNCS ("enable_arrows",      toplevel,   bool, enable_arrows,  gboolean)
TOPLEVEL_GET_SET_FUNCS ("background/fit",     background, bool, fit,            gboolean)
TOPLEVEL_GET_SET_FUNCS ("background/stretch", background, bool, stretch,        gboolean)
TOPLEVEL_GET_SET_FUNCS ("background/rotate",  background, bool, rotate,         gboolean)

static const char *
panel_profile_get_attached_object_key (PanelToplevel *toplevel,
				       const char    *key)
{
	GtkWidget  *attach_widget;
	const char *id;

	attach_widget = panel_toplevel_get_attach_widget (toplevel);

	id = panel_applet_get_id_by_widget (attach_widget);

	if (!id)
		return NULL;

	return panel_gconf_full_key (PANEL_GCONF_OBJECTS, id, key);
}

void
panel_profile_set_attached_custom_icon (PanelToplevel *toplevel,
					const char    *custom_icon)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_profile_get_attached_object_key (toplevel, "use_custom_icon");
	if (key)
		gconf_client_set_bool (client, key, custom_icon != NULL, NULL);

	key = panel_profile_get_attached_object_key (toplevel, "custom_icon");
	if (key)
		gconf_client_set_string (client, key, sure_string (custom_icon), NULL);
}

char *
panel_profile_get_attached_custom_icon (PanelToplevel *toplevel)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_profile_get_attached_object_key (toplevel, "use_custom_icon");
	if (!key || !gconf_client_get_bool (client, key, NULL))
		return NULL;

	key = panel_profile_get_attached_object_key (toplevel, "custom_icon");

	return key ? gconf_client_get_string (client, key, NULL) : NULL;
}

gboolean
panel_profile_is_writable_attached_custom_icon (PanelToplevel *toplevel)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_profile_get_attached_object_key (toplevel, "use_custom_icon");
	if (!key)
		return TRUE;

	if (!gconf_client_key_is_writable (client, key, NULL))
		return FALSE;

	key = panel_profile_get_attached_object_key (toplevel, "custom_icon");

	return key ? gconf_client_key_is_writable (client, key, NULL) : TRUE;
}

void
panel_profile_set_attached_tooltip (PanelToplevel *toplevel,
				    const char    *tooltip)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_profile_get_attached_object_key (toplevel, "tooltip");
	if (key)
		gconf_client_set_string (client, key, tooltip, NULL);
}

char *
panel_profile_get_attached_tooltip (PanelToplevel *toplevel)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_profile_get_attached_object_key (toplevel, "tooltip");

	return key ? gconf_client_get_string (client, key, NULL) : NULL;
}

gboolean
panel_profile_is_writable_attached_tooltip (PanelToplevel *toplevel)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_profile_get_attached_object_key (toplevel, "tooltip");

	return key ? gconf_client_key_is_writable (client, key, NULL) : TRUE;
}

static PanelBackgroundType
get_background_type (GConfClient *client,
		     const char  *toplevel_dir)
{
	PanelBackgroundType  background_type;
	GError              *error = NULL;
	const char          *key;
	char                *type_str;

	key = panel_gconf_sprintf ("%s/background/type", toplevel_dir);
	type_str = gconf_client_get_string (client, key, &error);
	if (error) {
		g_warning (_("Error reading GConf string value '%s': %s"),
			   key, error->message);
		g_error_free (error);
		return PANEL_BACK_NONE;
	}

	if (!type_str || !panel_profile_map_background_type_string (type_str, &background_type))
		background_type = PANEL_BACK_NONE;

	g_free (type_str);
	
	return background_type;
}

static void
get_background_color (GConfClient *client,
		      const char  *toplevel_dir,
		      PanelColor  *color)
{
	GError     *error;
	const char *key;
	char       *color_str;

	error = NULL;
	key = panel_gconf_sprintf ("%s/background/color", toplevel_dir);
	color_str = gconf_client_get_string (client, key, &error);
	if (error) {
		g_warning (_("Error reading GConf string value '%s': %s"),
			   key, error->message);
		g_error_free (error);
	} else if (!color_str || !gdk_color_parse (color_str, &(color->gdk))) {
		color->gdk.red   = 0;
		color->gdk.green = 0;
		color->gdk.blue  = 0;
	}

	g_free (color_str);

	error = NULL;
	key = panel_gconf_sprintf ("%s/background/opacity", toplevel_dir);
	color->alpha = gconf_client_get_int (client, key, &error);
	if (error) {
		g_warning (_("Error reading GConf integer value '%s': %s"),
			   key, error->message);
		g_error_free (error);
		color->alpha = 65535; /* fallback to fully opaque */
	}
}

static char *
get_background_image (GConfClient  *client,
		      const char   *toplevel_dir,
		      gboolean     *fit,
		      gboolean     *stretch,
		      gboolean     *rotate)
{
	const char *key;
	GError     *error = NULL;
	char       *image;

	key = panel_gconf_sprintf ("%s/background/image", toplevel_dir);
	image = gconf_client_get_string (client, key, &error);
	if (error) {
		g_warning (_("Error reading GConf string value '%s': %s"),
			   key, error->message);
		g_error_free (error);
	}

	key = panel_gconf_sprintf ("%s/background/fit", toplevel_dir);
	*fit = gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_sprintf ("%s/background/stretch", toplevel_dir);
	*stretch = gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_sprintf ("%s/background/rotate", toplevel_dir);
	*rotate = gconf_client_get_bool (client, key, NULL);

	return image;
}

static void
panel_profile_load_background (PanelToplevel *toplevel,
			       GConfClient   *client,
			       const char    *toplevel_dir)
{
	PanelWidget         *panel_widget;
	PanelBackground     *background;
	PanelBackgroundType  background_type;
	PanelColor           color;
	char                *image;
	gboolean             fit;
	gboolean             stretch;
	gboolean             rotate;

	panel_widget = panel_toplevel_get_panel_widget (toplevel);
	background = &panel_widget->background;

	background_type = get_background_type (client, toplevel_dir);

	get_background_color (client, toplevel_dir, &color);

	image = get_background_image (client, toplevel_dir, &fit, &stretch, &rotate);

	panel_background_set (background,
			      background_type,
			      &color,
			      image,
			      fit,
			      stretch,
			      rotate);

	g_free (image);
}

static gboolean
panel_profile_commit_toplevel_changes (PanelToplevel *toplevel)
{
	GConfChangeSet *queued_changes;

	queued_changes = panel_profile_get_queued_changes (G_OBJECT (toplevel));
	if (queued_changes)
		gconf_client_commit_change_set (
				panel_gconf_get_client (),
				queued_changes, FALSE, NULL);

	panel_profile_set_queued_changes (toplevel, NULL);
	panel_profile_set_commit_timeout (toplevel, 0);

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
		panel_profile_set_queued_changes (toplevel, queued_changes);
	}

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

	if (change->orientation_changed)
		gconf_change_set_set_string (
			queued_changes,
			panel_profile_get_toplevel_key (toplevel, "orientation"),
			gconf_enum_to_string (panel_orientation_map, change->orientation));

	if (!panel_toplevel_get_expand (toplevel)) {
		if (change->x_changed)
			gconf_change_set_set_int (
				queued_changes,
				panel_profile_get_toplevel_key (toplevel, "x"),
				change->x);

		if (change->x_right_changed)
			gconf_change_set_set_int (
				queued_changes,
				panel_profile_get_toplevel_key (toplevel, "x_right"),
				change->x_right);

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

		if (change->y_bottom_changed)
			gconf_change_set_set_int (
				queued_changes,
				panel_profile_get_toplevel_key (toplevel, "y_bottom"),
				change->y_bottom);

		if (change->y_centered_changed)
			gconf_change_set_set_bool (
				queued_changes,
				panel_profile_get_toplevel_key (toplevel, "y_centered"),
				change->y_centered);
	}

	commit_timeout = panel_profile_get_commit_timeout (G_OBJECT (toplevel));
	if (!commit_timeout) {
		commit_timeout =
			g_timeout_add (500,
				       (GSourceFunc) panel_profile_commit_toplevel_changes,
				       toplevel);
		panel_profile_set_commit_timeout (toplevel, commit_timeout);
	}
}

#define TOPLEVEL_LOCATION_CHANGED_HANDLER(c)                                      \
	static void                                                               \
	panel_profile_toplevel_##c##_changed (PanelToplevel *toplevel)            \
	{                                                                         \
		ToplevelLocationChange change = { NULL };                           \
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
		ToplevelLocationChange change = { NULL };                           \
		int                    x, y, x_right, y_bottom;                   \
		change.c##_changed = TRUE;                                        \
		panel_toplevel_get_position (toplevel,                            \
					     &x, &x_right,                        \
					     &y, &y_bottom);                      \
		change.c = c;                                                     \
		panel_profile_queue_toplevel_location_change (toplevel, &change); \
	}

TOPLEVEL_POSITION_CHANGED_HANDLER(x)
TOPLEVEL_POSITION_CHANGED_HANDLER(x_right)
TOPLEVEL_POSITION_CHANGED_HANDLER(y)
TOPLEVEL_POSITION_CHANGED_HANDLER(y_bottom)

static void
panel_profile_toplevel_screen_changed (PanelToplevel *toplevel)
{
	ToplevelLocationChange change = { NULL };

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
	g_signal_connect (toplevel, "notify::x-right",
			  G_CALLBACK (panel_profile_toplevel_x_right_changed), NULL);
	g_signal_connect (toplevel, "notify::x-centered",
			  G_CALLBACK (panel_profile_toplevel_x_centered_changed), NULL);
	g_signal_connect (toplevel, "notify::y",
			  G_CALLBACK (panel_profile_toplevel_y_changed), NULL);
	g_signal_connect (toplevel, "notify::y-bottom",
			  G_CALLBACK (panel_profile_toplevel_y_bottom_changed), NULL);
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
	PanelOrientation orientation;

	if (!str || !panel_profile_map_orientation_string (str, &orientation))
		return;

	panel_toplevel_set_orientation (toplevel, orientation);
}

static void
set_animation_speed_from_string (PanelToplevel *toplevel,
				 const char    *str)
{
	PanelAnimationSpeed animation_speed;

	if (!str || !panel_profile_map_speed_string (str, &animation_speed))
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
			if (value->type == GCONF_VALUE_STRING)                          \
				set_##n##_from_string (toplevel,                        \
						       gconf_value_get_string (value)); \
		}

#define UPDATE_INT(k, n)                                                                \
		if (!strcmp (key, k)) {                                                 \
			if (value->type == GCONF_VALUE_INT)                             \
				panel_toplevel_set_##n (toplevel,                       \
							gconf_value_get_int (value));   \
		}

#define UPDATE_BOOL(k, n)                                                               \
		if (!strcmp (key, k)) {                                                 \
			if (value->type == GCONF_VALUE_BOOL)                            \
				panel_toplevel_set_##n (toplevel,                       \
							gconf_value_get_bool (value));  \
		}

#define UPDATE_POS(k, n, n2)                                                            \
		if (!strcmp (key, k)) {                                                 \
			if (value->type == GCONF_VALUE_INT) {                           \
				int x, x_right, y, y_bottom;                            \
				panel_toplevel_get_position (toplevel, &x, &x_right,    \
							     &y, &y_bottom);            \
				panel_toplevel_set_##n (                                \
					toplevel,                                       \
					gconf_value_get_int (value),                    \
					n2,                                             \
					panel_toplevel_get_##n##_centered (toplevel));  \
			}                                                               \
		}

#define UPDATE_POS2(k, n, n2)                                                           \
		if (!strcmp (key, k)) {                                                 \
			if (value->type == GCONF_VALUE_INT) {                           \
				int x, x_right, y, y_bottom;                            \
				panel_toplevel_get_position (toplevel, &x, &x_right,    \
							     &y, &y_bottom);            \
				panel_toplevel_set_##n (                                \
					toplevel,                                       \
					n,                                              \
					gconf_value_get_int (value),                    \
					panel_toplevel_get_##n##_centered (toplevel));  \
			}                                                               \
		}

#define UPDATE_CENTERED(k, n, n2)                                                       \
		if (!strcmp (key, k)) {                                                 \
			if (value->type == GCONF_VALUE_BOOL) {                          \
				int x, x_right, y, y_bottom;                            \
				panel_toplevel_get_position (toplevel, &x, &x_right,    \
							     &y, &y_bottom);            \
				panel_toplevel_set_##n (                                \
					toplevel, n, n2, gconf_value_get_bool (value)); \
			}                                                               \
		}

	if (!strcmp (key, "screen")) {
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
	else UPDATE_POS ("x", x, x_right)
	else UPDATE_POS ("y", y, y_bottom)
	else UPDATE_POS2 ("x_right", x, x_right)
	else UPDATE_POS2 ("y_bottom", y, y_bottom)
	else UPDATE_CENTERED ("x_centered", x, x_right)
	else UPDATE_CENTERED ("y_centered", y, y_bottom)
	else UPDATE_BOOL ("auto_hide", auto_hide)
	else UPDATE_BOOL ("enable_animations", animate)
	else UPDATE_BOOL ("enable_buttons", enable_buttons)
	else UPDATE_BOOL ("enable_arrows", enable_arrows)
	else UPDATE_INT ("hide_delay", hide_delay)
	else UPDATE_INT ("unhide_delay", unhide_delay)
	else UPDATE_INT ("auto_hide_size", auto_hide_size)
	else UPDATE_STRING ("animation_speed", animation_speed)
}

static void
panel_profile_background_change_notify (GConfClient   *client,
					guint          cnxn_id,
					GConfEntry    *entry,
					PanelToplevel *toplevel)
{
	PanelWidget     *panel_widget;
	PanelBackground *background;
	GConfValue      *value;
	const char      *key;

	key = panel_gconf_basename (gconf_entry_get_key (entry));

	if (!(value = gconf_entry_get_value (entry)))
		return;

	panel_widget = panel_toplevel_get_panel_widget (toplevel);
	background = &panel_widget->background;

	if (!strcmp (key, "type")) {
		if (value->type == GCONF_VALUE_STRING) {
			PanelBackgroundType  background_type;

			if (panel_profile_map_background_type_string (
						gconf_value_get_string (value),
						&background_type)) {
				panel_background_set_type (background, background_type);
				panel_toplevel_update_edges (toplevel);
			}
		}
	} else if (!strcmp (key, "color")) {
		if (value->type == GCONF_VALUE_STRING) {
			GdkColor    gdk_color;
			const char *str;

			str = gconf_value_get_string (value);

			if (gdk_color_parse (str, &gdk_color))
				panel_background_set_gdk_color (background, &gdk_color);
		}
	} else if (!strcmp (key, "opacity")) {
		if (value->type == GCONF_VALUE_INT)
			panel_background_set_opacity (background,
						      gconf_value_get_int (value));
	} else if (!strcmp (key, "image")) {
		if (value->type == GCONF_VALUE_STRING)
			panel_background_set_image (background,
						    gconf_value_get_string (value));
	} else if (!strcmp (key, "fit")) {
		if (value->type == GCONF_VALUE_BOOL)
			panel_background_set_fit (background,
						  gconf_value_get_bool (value));
	} else if (!strcmp (key, "stretch")) {
		if (value->type == GCONF_VALUE_BOOL)
			panel_background_set_stretch (background,
						      gconf_value_get_bool (value));
	} else if (!strcmp (key, "rotate")) {
		if (value->type == GCONF_VALUE_BOOL)
			panel_background_set_rotate (background,
						     gconf_value_get_bool (value));
	}
}

static void
panel_profile_disconnect_toplevel (PanelToplevel *toplevel,
				   gpointer       data)
{
	GConfClient *client;
	guint        notify_id = GPOINTER_TO_UINT (data);

	client = panel_gconf_get_client ();

	gconf_client_notify_remove (client, notify_id);
}

guint
panel_profile_toplevel_notify_add (PanelToplevel         *toplevel,
				   const char            *key,
				   GConfClientNotifyFunc  func,
				   gpointer               data)
{
	GConfClient *client;
	const char  *tmp;
	guint        retval;

	client = panel_gconf_get_client ();

	if (!key)
		tmp = panel_gconf_sprintf (PANEL_CONFIG_DIR "/toplevels/%s",
					   panel_profile_get_toplevel_id (toplevel));
	else
		tmp = panel_gconf_sprintf (PANEL_CONFIG_DIR "/toplevels/%s/%s",
					   panel_profile_get_toplevel_id (toplevel),
					   key);

	retval = gconf_client_notify_add (client, tmp, func, data, NULL, NULL);

	return retval;
}

static void
panel_profile_save_id_list (PanelGConfKeyType  type,
			    GSList            *list,
			    gboolean           resave)
{
	GConfClient *client;
	const char  *key;
	const char  *id_list;

	g_assert (!(resave && list != NULL));

	client = panel_gconf_get_client ();

	id_list = panel_gconf_key_type_to_id_list (type);

	key = panel_gconf_general_key (id_list);
	if (resave)
		list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);
	else {
		/* Make sure the elements in the list appear only once. We only
		 * do it when we save a list with new elements. */
		list = panel_g_slist_make_unique (list,
						  (GCompareFunc) strcmp,
						  TRUE);
	}

	gconf_client_set_list (client, key, GCONF_VALUE_STRING, list, NULL);

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);
}

static inline void
panel_profile_save_other_id_lists (PanelGConfKeyType type)
{
	if (type != PANEL_GCONF_TOPLEVELS)
		panel_profile_save_id_list (PANEL_GCONF_TOPLEVELS, NULL, TRUE);

	if (type != PANEL_GCONF_OBJECTS)
		panel_profile_save_id_list (PANEL_GCONF_OBJECTS, NULL, TRUE);

	if (type != PANEL_GCONF_APPLETS)
		panel_profile_save_id_list (PANEL_GCONF_APPLETS, NULL, TRUE);
}

void
panel_profile_add_to_list (PanelGConfKeyType  type,
			   const char        *id)
{
	GConfClient *client;
	GSList      *list;
	const char  *key;
	const char  *id_list;
	char        *new_id;

	client = panel_gconf_get_client ();

	id_list = panel_gconf_key_type_to_id_list (type);

	key = panel_gconf_general_key (id_list);
	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	new_id = id ? g_strdup (id) : panel_profile_find_new_id (type);

	list = g_slist_append (list, new_id);

	panel_profile_save_id_list (type, list, FALSE);
	panel_profile_save_other_id_lists (type);
}

void
panel_profile_remove_from_list (PanelGConfKeyType  type,
				const char        *id)
{
	GConfClient *client;
	GSList      *list, *l;
	const char  *key;
	const char  *id_list;

	client = panel_gconf_get_client ();

	id_list = panel_gconf_key_type_to_id_list (type);

	key = panel_gconf_general_key (id_list);
	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	/* Remove all occurrence of id and not just the first. We're more solid
	 * this way (see bug #137308 for example). */
	l = list;
	while (l) {
		GSList *next;

		next = l->next;

		if (!strcmp (id, l->data)) {
			g_free (l->data);
			list = g_slist_delete_link (list, l);
		}

		l = next;
	}

	panel_profile_save_id_list (type, list, FALSE);
	panel_profile_save_other_id_lists (type);
}

static gboolean
panel_profile_id_list_is_writable (PanelGConfKeyType type)
{
	GConfClient *client;
	const char  *key;
	const char  *id_list;

	client = panel_gconf_get_client ();

	id_list = panel_gconf_key_type_to_id_list (type);

	key = panel_gconf_general_key (id_list);
	return gconf_client_key_is_writable (client, key, NULL);
}

gboolean
panel_profile_id_lists_are_writable (void)
{
  return
    panel_profile_id_list_is_writable (PANEL_GCONF_TOPLEVELS) &&
    panel_profile_id_list_is_writable (PANEL_GCONF_APPLETS)   &&
    panel_profile_id_list_is_writable (PANEL_GCONF_OBJECTS);
}

static gboolean
panel_profile_find_empty_spot (GdkScreen *screen,
			       PanelOrientation *orientation,
			       int *monitor)
{
	GSList *li;
	int i;
	int *filled_spots;
	gboolean found_a_spot = FALSE;

	*monitor = 0;
	*orientation = PANEL_ORIENTATION_TOP;

	filled_spots = g_new0 (int, panel_multiscreen_monitors (screen));

	for (li = panel_toplevel_list_toplevels (); li != NULL; li = li->next) {
		PanelToplevel *toplevel = li->data;
		GdkScreen *toplevel_screen = gtk_window_get_screen (GTK_WINDOW (toplevel));
		int toplevel_monitor = panel_toplevel_get_monitor (toplevel);

		if (toplevel_screen != screen ||
		    panel_toplevel_get_is_attached (toplevel) ||
		    toplevel_monitor < 0)
			continue;

		filled_spots[toplevel_monitor] |= panel_toplevel_get_orientation (toplevel);
	}

	for (i = 0; i < panel_multiscreen_monitors (screen); i++) {
		/* These are ordered based on "priority" of the
		   orientation when picking it */
		if ( ! (filled_spots[i] & PANEL_ORIENTATION_TOP)) {
			*orientation = PANEL_ORIENTATION_TOP;
			*monitor = i;
			found_a_spot = TRUE;
			break;
		} else if ( ! (filled_spots[i] & PANEL_ORIENTATION_BOTTOM)) {
			*orientation = PANEL_ORIENTATION_BOTTOM;
			*monitor = i;
			found_a_spot = TRUE;
			break;
		} else if ( ! (filled_spots[i] & PANEL_ORIENTATION_RIGHT)) {
			*orientation = PANEL_ORIENTATION_RIGHT;
			*monitor = i;
			found_a_spot = TRUE;
			break;
		} else if ( ! (filled_spots[i] & PANEL_ORIENTATION_LEFT)) {
			*orientation = PANEL_ORIENTATION_LEFT;
			*monitor = i;
			found_a_spot = TRUE;
			break;
		}
	}

	g_free (filled_spots);

	return found_a_spot;
}

void
panel_profile_create_toplevel (GdkScreen *screen)
{
	GConfClient     *client;
	const char      *key;
	char            *id;
	char            *dir;
	PanelOrientation orientation;
	int              monitor;
       
	g_return_if_fail (screen != NULL);

	client = panel_gconf_get_client ();

	id = panel_profile_find_new_id (PANEL_GCONF_TOPLEVELS);

	dir = g_strdup_printf (PANEL_CONFIG_DIR "/toplevels/%s", id);
	panel_gconf_associate_schemas_in_dir (client, dir, PANEL_SCHEMAS_DIR "/toplevels");
	g_free (dir);

	key = panel_gconf_full_key (PANEL_GCONF_TOPLEVELS, id, "screen");
	gconf_client_set_int (client, key, gdk_screen_get_number (screen), NULL);

	if (panel_profile_find_empty_spot (screen, &orientation, &monitor)) {
		key = panel_gconf_full_key (PANEL_GCONF_TOPLEVELS, id, "monitor");
		gconf_client_set_int (client, key, monitor, NULL);

		key = panel_gconf_full_key (PANEL_GCONF_TOPLEVELS, id, "orientation");
		gconf_client_set_string (client, key, panel_profile_map_orientation (orientation), NULL);
	}
	
	panel_profile_add_to_list (PANEL_GCONF_TOPLEVELS, id);

	g_free (id);
}

static void
panel_profile_delete_toplevel_objects (const char        *toplevel_id,
				       PanelGConfKeyType  key_type)
{
	GConfClient *client;
	const char  *key;
	GSList      *new_list = NULL,*list, *l;

	client = panel_gconf_get_client ();

	key = panel_gconf_general_key (panel_gconf_key_type_to_id_list (key_type));
	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	for (l = list; l; l = l->next) {
		char *id = l->data;
		char *parent_toplevel_id;

		key = panel_gconf_full_key (key_type, id, "toplevel_id");
		parent_toplevel_id = gconf_client_get_string (client, key, NULL);

		if (parent_toplevel_id && !strcmp (toplevel_id, parent_toplevel_id)) {
			g_free (id);
			g_free (parent_toplevel_id);
			continue;
		}

		new_list = g_slist_prepend (new_list, id);

		g_free (parent_toplevel_id);
	}
	g_slist_free (list);

	key = panel_gconf_general_key (panel_gconf_key_type_to_id_list (key_type));
	gconf_client_set_list (client, key, GCONF_VALUE_STRING, new_list, NULL);

	for (l = new_list; l; l = l->next)
		g_free (l->data);
	g_slist_free (new_list);
}

void
panel_profile_delete_toplevel (PanelToplevel *toplevel)
{
	const char *toplevel_id;

	toplevel_id = panel_profile_get_toplevel_id (toplevel);

	panel_profile_delete_toplevel_objects (toplevel_id, PANEL_GCONF_OBJECTS);
	panel_profile_delete_toplevel_objects (toplevel_id, PANEL_GCONF_APPLETS);

	panel_profile_remove_from_list (PANEL_GCONF_TOPLEVELS, toplevel_id);
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
#if 0
		g_warning (_("Panel '%s' is set to be displayed on screen %d which "
			     "is not currently available. Not loading this panel."),
			   toplevel_dir, screen_n);
#endif
		return NULL;
	}

	return gdk_display_get_screen (display, screen_n);
}

PanelToplevel *
panel_profile_load_toplevel (GConfClient       *client,
			     const char        *profile_dir,
			     PanelGConfKeyType  type,
			     const char        *toplevel_id)
{
	PanelToplevel *toplevel;
	GdkScreen     *screen;
	GError        *error;
	const char    *key;
	char          *toplevel_dir;
	guint          notify_id;

	if (!toplevel_id || !toplevel_id [0])
		return NULL;

	toplevel_dir = g_strdup_printf ("%s/toplevels/%s", profile_dir, toplevel_id);

	if (!gconf_client_dir_exists (client, toplevel_dir, NULL))
		panel_gconf_associate_schemas_in_dir (
			client, toplevel_dir, PANEL_SCHEMAS_DIR "/toplevels");

	gconf_client_add_dir (client,
			      toplevel_dir,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);

	key = panel_gconf_sprintf ("%s/background", toplevel_dir);
	gconf_client_add_dir (client,
			      key,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);

	if (!(screen = get_toplevel_screen (client, toplevel_dir))) {
		gconf_client_remove_dir (client, key, NULL);
		gconf_client_remove_dir (client, toplevel_dir, NULL);
		g_free (toplevel_dir);
		return NULL;
	}

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
		} else if (error) {                                                 \
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

#define GET_POSITION(a, b, c, fn)                                                   \
	{                                                                           \
		gboolean centered;                                                  \
		int      position;                                                  \
		int      position2;                                                 \
		key = panel_gconf_sprintf ("%s/" c, toplevel_dir);                  \
		centered = gconf_client_get_bool (client, key, &error);             \
		if (!error) {                                                       \
			key = panel_gconf_sprintf ("%s/" a, toplevel_dir);          \
			position = gconf_client_get_int (client, key, &error);      \
		}                                                                   \
		if (!error) {                                                       \
			GConfValue *value;                                          \
			key = panel_gconf_sprintf ("%s/" b, toplevel_dir);          \
			/* we need to do this since the key was added in 2.19 and   \
			 * the default value returned when the key is not set       \
			 * (for people coming from older versions) is 0, which      \
			 * is not what we want. */                                  \
			value = gconf_client_get_without_default (client, key, &error);\
			if (value && value->type == GCONF_VALUE_INT)                \
				position2 = gconf_value_get_int (value);            \
			else                                                        \
				position2 = -1;                                     \
                                                                                    \
			if (value)                                                  \
				gconf_value_free (value);                           \
		}                                                                   \
		if (!error)                                                         \
			panel_toplevel_set_##fn (toplevel, position, position2,     \
						 centered);                         \
		else {                                                              \
			g_warning (_("Error reading GConf integer value '%s': %s"), \
				   key, error->message);                            \
			g_error_free (error);                                       \
		}                                                                   \
	}

	GET_POSITION ("x", "x_right", "x_centered", x);
	GET_POSITION ("y", "y_bottom", "y_centered", y);

	panel_profile_load_background (toplevel, client, toplevel_dir);

	panel_profile_set_toplevel_id (toplevel, toplevel_id);

	panel_profile_connect_to_toplevel (toplevel);

	notify_id = panel_profile_toplevel_notify_add (
			toplevel,
			NULL,
			(GConfClientNotifyFunc) panel_profile_toplevel_change_notify,
			toplevel);
	g_signal_connect (toplevel, "destroy",
			  G_CALLBACK (panel_profile_disconnect_toplevel),
			  GUINT_TO_POINTER (notify_id));

	notify_id = panel_profile_toplevel_notify_add (
			toplevel,
			"background",
			(GConfClientNotifyFunc) panel_profile_background_change_notify,
			toplevel);
	g_signal_connect (toplevel, "destroy",
			  G_CALLBACK (panel_profile_disconnect_toplevel),
			  GUINT_TO_POINTER (notify_id));

	g_free (toplevel_dir);

	panel_setup (toplevel);

	return toplevel;
}

static void
panel_profile_load_and_show_toplevel (GConfClient       *client,
				      const char        *profile_dir,
				      PanelGConfKeyType  type,
				      const char        *toplevel_id)
{
	PanelToplevel *toplevel;
	const char    *id_list;
	const char    *key;
	GConfValue    *value;
	gboolean       loading_queued_applets;

	toplevel = panel_profile_load_toplevel (client, profile_dir, type, toplevel_id);
	if (!toplevel)
		return;

	gtk_widget_show (GTK_WIDGET (toplevel));

	loading_queued_applets = FALSE;

	/* reload list of objects to get those that might be on the new
	 * toplevel */
	id_list = panel_gconf_key_type_to_id_list (PANEL_GCONF_OBJECTS);
	key = panel_gconf_sprintf ("%s/general/%s", profile_dir, id_list);
	value = gconf_client_get (client, key, NULL);
	if (value) {
		panel_profile_object_id_list_update (client, value,
						     PANEL_GCONF_OBJECTS);
		loading_queued_applets = TRUE;
		gconf_value_free (value);
	}

	id_list = panel_gconf_key_type_to_id_list (PANEL_GCONF_APPLETS);
	key = panel_gconf_sprintf ("%s/general/%s", profile_dir, id_list);
	value = gconf_client_get (client, key, NULL);
	if (value) {
		panel_profile_object_id_list_update (client, value,
						     PANEL_GCONF_APPLETS);
		loading_queued_applets = TRUE;
		gconf_value_free (value);
	}

	if (!loading_queued_applets)
		panel_applet_load_queued_applets (FALSE);
}

static void
panel_profile_load_and_show_toplevel_startup (GConfClient       *client,
					      const char        *profile_dir,
					      PanelGConfKeyType  type,
					      const char        *toplevel_id)
{
	PanelToplevel *toplevel;

	toplevel = panel_profile_load_toplevel (client, profile_dir, type, toplevel_id);
	if (toplevel)
		gtk_widget_show (GTK_WIDGET (toplevel));
}

static void
panel_profile_destroy_toplevel (const char *id)
{
	PanelToplevel *toplevel;

	if (!(toplevel = panel_profile_get_toplevel_by_id (id)))
		return;

	gtk_widget_destroy (GTK_WIDGET (toplevel));
}

char *
panel_profile_prepare_object_with_id (PanelObjectType  object_type,
				      const char      *toplevel_id,
				      int              position,
				      gboolean         right_stick)
{
	PanelGConfKeyType  key_type;
	GConfClient       *client;
	const char        *key;
	char              *id;
	char              *dir;

	key_type = (object_type == PANEL_OBJECT_APPLET) ? PANEL_GCONF_APPLETS : PANEL_GCONF_OBJECTS;

	client  = panel_gconf_get_client ();

	id = panel_profile_find_new_id (key_type);

	dir = g_strdup_printf (PANEL_CONFIG_DIR "/%s/%s",
			       (key_type == PANEL_GCONF_APPLETS) ? "applets" : "objects",
			       id);
	panel_gconf_associate_schemas_in_dir (client, dir, PANEL_SCHEMAS_DIR "/objects");

	key = panel_gconf_full_key (key_type, id, "object_type");
	gconf_client_set_string (client,
				 key,
				 gconf_enum_to_string (panel_object_type_map, object_type),
				 NULL);

	key = panel_gconf_full_key (key_type, id, "toplevel_id");
	gconf_client_set_string (client, key, toplevel_id, NULL);

	key = panel_gconf_full_key (key_type, id, "position");
	gconf_client_set_int (client, key, position, NULL);

	key = panel_gconf_full_key (key_type, id, "panel_right_stick");
	gconf_client_set_bool (client, key, right_stick, NULL);

	g_free (dir);

	return id;
}

char *
panel_profile_prepare_object (PanelObjectType  object_type,
			      PanelToplevel   *toplevel,
			      int              position,
			      gboolean         right_stick)
{
	return panel_profile_prepare_object_with_id (object_type,
						     panel_profile_get_toplevel_id (toplevel),
						     position,
						     right_stick);
}

void
panel_profile_delete_object (AppletInfo *applet_info)
{
	PanelGConfKeyType  type;
	const char        *id;

	type = (applet_info->type) == PANEL_OBJECT_APPLET ? PANEL_GCONF_APPLETS :
							    PANEL_GCONF_OBJECTS;
	id = panel_applet_get_id (applet_info);

	panel_profile_remove_from_list (type, id);
}

static void
panel_profile_load_object (GConfClient       *client,
			   const char        *profile_dir,
			   PanelGConfKeyType  type,
			   const char        *id)
{
	PanelObjectType  object_type;
	char            *object_dir;
	const char      *key;
	char            *type_string;
	char            *toplevel_id;
	int              position;
	gboolean         right_stick;
	gboolean         locked;

	object_dir = g_strdup_printf ("%s/%s/%s",
				      profile_dir,
				      type == PANEL_GCONF_OBJECTS ? "objects" : "applets",
				      id);

	gconf_client_add_dir (client, object_dir, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	key = panel_gconf_sprintf ("%s/object_type", object_dir);
	type_string = gconf_client_get_string (client, key, NULL);
        
	if (!panel_profile_map_object_type_string (type_string, &object_type)) {
		g_free (type_string);
		gconf_client_remove_dir (client, object_dir, NULL);
		g_free (object_dir);
		return;
	}
	
	key = panel_gconf_sprintf ("%s/position", object_dir);
	position = gconf_client_get_int (client, key, NULL);
	
	key = panel_gconf_sprintf ("%s/toplevel_id", object_dir);
	toplevel_id = gconf_client_get_string (client, key, NULL);

	key = panel_gconf_sprintf ("%s/panel_right_stick", object_dir);
	right_stick = gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_sprintf ("%s/locked", object_dir);
	locked = gconf_client_get_bool (client, key, NULL);

	panel_applet_queue_applet_to_load (id,
					   object_type,
					   toplevel_id,
					   position,
					   right_stick,
					   locked);

	g_free (toplevel_id);
	g_free (type_string);
	g_free (object_dir);
}

static void
panel_profile_destroy_object (const char *id)
{
	AppletInfo *info;

	info = panel_applet_get_by_id (id);

	panel_applet_clean (info);
}

static void
panel_profile_delete_dir (GConfClient       *client,
			  PanelGConfKeyType  type,
			  const char        *id)
{
	const char *key;
	char       *type_str;

	switch (type) {
	case PANEL_GCONF_TOPLEVELS:
		type_str = "toplevels";
		break;
	case PANEL_GCONF_OBJECTS:
		type_str = "objects";
		break;
	case PANEL_GCONF_APPLETS:
		type_str = "applets";
		break;
	default:
		type_str = NULL;
		g_assert_not_reached ();
		break;
	}

	if (type == PANEL_GCONF_TOPLEVELS) {
		key = panel_gconf_sprintf (PANEL_CONFIG_DIR "/%s/%s/background",
					   type_str, id);
		gconf_client_remove_dir (client, key, NULL);
	}

	key = panel_gconf_sprintf (PANEL_CONFIG_DIR "/%s/%s",
				   type_str, id);
	gconf_client_remove_dir (client, key, NULL);

	gconf_client_recursive_unset (client,
				      key,
				      GCONF_UNSET_INCLUDING_SCHEMA_NAMES,
				      NULL);
}

static gboolean
panel_profile_object_exists (GSList                *list,
			     const char            *id,
			     PanelProfileGetIdFunc  get_id_func)
{
	GSList *l;

	if (!list || !id)
		return FALSE;

	for (l = list; l; l = l->next) {
		const char *check_id;

		check_id = get_id_func (l->data);
		g_assert (check_id != NULL);

		if (!strcmp (check_id, id))
			return TRUE;
	}

	return FALSE;
}

static gboolean
panel_profile_id_exists (GSList     *id_list,
			 const char *id)
{
	GSList *l;

	if (!id_list || !id)
		return FALSE;

	for (l = id_list; l; l = l->next) {
		const char *check_id = gconf_value_get_string (l->data);

		if (!strcmp (id, check_id))
			return TRUE;
	}

	return FALSE;
}

static void
panel_profile_load_added_ids (GConfClient            *client,
			      PanelGConfKeyType       type,
			      GSList                 *list,
			      GSList                 *id_list,
			      PanelProfileGetIdFunc   get_id_func,
			      PanelProfileLoadFunc    load_handler,
			      PanelProfileOnLoadQueue on_load_queue)
{
	GSList *added_ids = NULL;
	GSList *l;

	for (l = id_list; l; l = l->next) {
		const char *id = gconf_value_get_string (l->data);

		if (!panel_profile_object_exists (list, id, get_id_func) &&
		    (on_load_queue == NULL || !on_load_queue (id)))
			added_ids = g_slist_prepend (added_ids, g_strdup (id));
	}

	for (l = added_ids; l; l = l->next) {
		char *id;
		id = (char *) l->data;

		if (id && id[0])
			load_handler (client, PANEL_CONFIG_DIR, type, id);

		g_free (l->data);
		l->data = NULL;
	}

	g_slist_free (added_ids);
}

static void
panel_profile_delete_removed_ids (GConfClient             *client,
				  PanelGConfKeyType        type,
				  GSList                  *list,
				  GSList                  *id_list,
				  PanelProfileGetIdFunc    get_id_func,
				  PanelProfileDestroyFunc  destroy_handler)
{
	GSList *removed_ids = NULL;
	GSList *l;

	for (l = list; l; l = l->next) {
		const char *id;

		id = get_id_func (l->data);

		if (!panel_profile_id_exists (id_list, id))
			removed_ids = g_slist_prepend (removed_ids, g_strdup (id));
	}

	for (l = removed_ids; l; l = l->next) {
		const char *id = l->data;

		panel_profile_delete_dir (client, type, id);
		destroy_handler (id);

		g_free (l->data);
		l->data = NULL;
	}
	g_slist_free (removed_ids);
}

static void
panel_profile_toplevel_id_list_notify (GConfClient *client,
				       guint        cnxn_id,
				       GConfEntry  *entry)
{
	GConfValue *value;
	GSList     *l, *existing_toplevels;
	GSList     *toplevel_ids;

	if (!(value = gconf_entry_get_value (entry)))
		return;

	if (value->type != GCONF_VALUE_LIST ||
	    gconf_value_get_list_type (value) != GCONF_VALUE_STRING) {
		gconf_value_free (value);
		return;
	}

	toplevel_ids = g_slist_copy (gconf_value_get_list (value));
	toplevel_ids = panel_g_slist_make_unique (toplevel_ids,
						  panel_gconf_value_strcmp,
						  FALSE);

	existing_toplevels = NULL;
	for (l = panel_toplevel_list_toplevels (); l; l = l->next) {
		PanelToplevel *toplevel = l->data;

		/* Attached toplevels aren't on the id list */
		if (panel_toplevel_get_is_attached (toplevel))
			continue;

		existing_toplevels = g_slist_prepend (existing_toplevels, toplevel);
	}

	panel_profile_load_added_ids (client,
				      PANEL_GCONF_TOPLEVELS,
				      existing_toplevels,
				      toplevel_ids,
				      (PanelProfileGetIdFunc) panel_profile_get_toplevel_id,
				      (PanelProfileLoadFunc) panel_profile_load_and_show_toplevel,
				      (PanelProfileOnLoadQueue) NULL);

	panel_profile_delete_removed_ids (client,
					  PANEL_GCONF_TOPLEVELS,
					  existing_toplevels,
					  toplevel_ids,
					  (PanelProfileGetIdFunc) panel_profile_get_toplevel_id,
					  (PanelProfileDestroyFunc) panel_profile_destroy_toplevel);

	g_slist_free (existing_toplevels);
	g_slist_free (toplevel_ids);
}

static void
panel_profile_object_id_list_update (GConfClient       *client,
				     GConfValue        *value,
				     PanelGConfKeyType  type)
{
	GSList *existing_applets;
	GSList *sublist = NULL, *l;
	GSList *object_ids;

	if (value->type != GCONF_VALUE_LIST ||
	    gconf_value_get_list_type (value) != GCONF_VALUE_STRING) {
		gconf_value_free (value);
		return;
	}

	object_ids = g_slist_copy (gconf_value_get_list (value));
	object_ids = panel_g_slist_make_unique (object_ids,
						panel_gconf_value_strcmp,
						FALSE);

	existing_applets = panel_applet_list_applets ();

	for (l = existing_applets; l; l = l->next) {
		AppletInfo *info = l->data;

		if ((type == PANEL_GCONF_APPLETS && info->type == PANEL_OBJECT_APPLET) ||
		    (type == PANEL_GCONF_OBJECTS && info->type != PANEL_OBJECT_APPLET))
			sublist = g_slist_prepend (sublist, info);
	}

	panel_profile_load_added_ids (client,
				      type,
				      sublist,
				      object_ids,
				      (PanelProfileGetIdFunc) panel_applet_get_id,
				      (PanelProfileLoadFunc) panel_profile_load_object,
				      (PanelProfileOnLoadQueue) panel_applet_on_load_queue);

	panel_profile_delete_removed_ids (client,
					  type,
					  sublist,
					  object_ids,
					  (PanelProfileGetIdFunc) panel_applet_get_id,
					  (PanelProfileDestroyFunc) panel_profile_destroy_object);

	g_slist_free (sublist);
	g_slist_free (object_ids);

	panel_applet_load_queued_applets (FALSE);
}

static void
panel_profile_object_id_list_notify (GConfClient *client,
				     guint        cnxn_id,
				     GConfEntry  *entry,
				     gpointer     data)
{
	GConfValue        *value;
	PanelGConfKeyType  type = GPOINTER_TO_INT (data);

	if (!(value = gconf_entry_get_value (entry)))
		return;

	panel_profile_object_id_list_update (client, value, type);
}

static void
panel_profile_load_list (GConfClient           *client,
			 const char            *profile_dir,
			 PanelGConfKeyType      type,
			 PanelProfileLoadFunc   load_handler,
			 GConfClientNotifyFunc  notify_handler)
{

	const char *key;
	GSList     *list;
	GSList     *l;
	const char *id_list;

	id_list = panel_gconf_key_type_to_id_list (type);

	key = panel_gconf_sprintf ("%s/general/%s", profile_dir, id_list);

	gconf_client_notify_add (client, key, notify_handler,
				 GINT_TO_POINTER (type),
				 NULL, NULL);

	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);
	list = panel_g_slist_make_unique (list,
					  (GCompareFunc) strcmp,
					  TRUE);

	for (l = list; l; l = l->next) {
		char *id;
		id = (char *) l->data;

		if (id && id[0])
			load_handler (client, profile_dir, type, id);

		g_free (l->data);
		l->data = NULL;
	}
	g_slist_free (list);
}

static GSList *
panel_profile_copy_defaults_for_screen (GConfClient       *client,
					const char        *profile_dir,
					int                screen_n,
					PanelGConfKeyType  type)
{
	GSList     *default_ids, *l;
	GSList     *new_ids = NULL;
	const char *key;
	const char *id_list, *type_str;

	id_list = panel_gconf_key_type_to_id_list (type);

	switch (type) {
	case PANEL_GCONF_TOPLEVELS:
		type_str    = "toplevels";
		break;
	case PANEL_GCONF_OBJECTS:
		type_str    = "objects";
		break;
	case PANEL_GCONF_APPLETS:
		type_str    = "applets";
		break;
	default:
		type_str = NULL;
		g_assert_not_reached ();
		break;
	}

	key = panel_gconf_sprintf (PANEL_DEFAULTS_DIR "/general/%s", id_list);
	default_ids = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	for (l = default_ids; l; l = l->next) {
		char *default_id = l->data;
		char *new_id;
		char *src_dir;
		char *dest_dir;

		new_id = g_strdup_printf ("%s_screen%d", default_id, screen_n);

		src_dir  = g_strdup_printf (PANEL_DEFAULTS_DIR "/%s/%s", type_str, default_id);
		dest_dir = g_strdup_printf ("%s/%s/%s", profile_dir, type_str, new_id);

		panel_gconf_copy_dir (client, src_dir, dest_dir);

		new_ids = g_slist_prepend (new_ids, new_id);

		g_free (src_dir);
		g_free (dest_dir);
		g_free (l->data);
	}
	g_slist_free (default_ids);

	return new_ids;
}

static void
panel_profile_append_new_ids (GConfClient       *client,
			      PanelGConfKeyType  type,
			      GSList            *new_ids)
{
	GSList     *list, *l;
	const char *key;
	const char *id_list;

	id_list = panel_gconf_key_type_to_id_list (type);

	key = panel_gconf_general_key (id_list);
	list = gconf_client_get_list (client, key, GCONF_VALUE_STRING, NULL);

	for (l = new_ids; l; l = l->next)
		list = g_slist_append (list, l->data);

	g_slist_free (new_ids);

	gconf_client_set_list (client, key, GCONF_VALUE_STRING, list, NULL);
	
	for (l = list; l; l = l->next)
		g_free (l->data);
	g_slist_free (list);
}

static void
panel_profile_copy_default_objects_for_screen (GConfClient       *client,
					       const char        *profile_dir,
					       int                screen_n,
					       PanelGConfKeyType  type)
{
	GSList *new_objects, *l, *next;

	new_objects = panel_profile_copy_defaults_for_screen (client, profile_dir, screen_n, type);

	for (l = new_objects; l; l = next) {
		char       *object_id = l->data;
		const char *key;
		char       *toplevel_id;
		char       *new_toplevel_id;

		next = l->next;

		key = panel_gconf_full_key (type, object_id, "toplevel_id");
		toplevel_id = gconf_client_get_string (client, key, NULL);
		if (!toplevel_id) {
			new_objects = g_slist_remove_link (new_objects, l);
			g_free (l->data);
			g_slist_free_1 (l);
			continue;
		}

		new_toplevel_id = g_strdup_printf ("%s_screen%d", toplevel_id, screen_n);
		gconf_client_set_string (client, key, new_toplevel_id, NULL);

		g_free (toplevel_id);
		g_free (new_toplevel_id);
	}

	panel_profile_append_new_ids (client, type, new_objects);
}

/* FIXME:
 *   We might want to do something more sophisticated like hardcode
 *   the default panel setup as the fallback panels.
 */
static GSList *
panel_profile_create_fallback_toplevel_list (GConfClient *client,
					     const char  *profile_dir)
{
	char *id;
	char *dir;

	id = panel_profile_find_new_id (PANEL_GCONF_TOPLEVELS);

	dir = g_strdup_printf ("%s/toplevels/%s", profile_dir, id);
	panel_gconf_associate_schemas_in_dir (client, dir, PANEL_SCHEMAS_DIR "/toplevels");
	g_free (dir);

	return g_slist_prepend (NULL, id);
}

static void
panel_profile_load_defaults_on_screen (GConfClient *client,
				       const char  *profile_dir,
				       GdkScreen   *screen)
{
	GSList *new_toplevels, *l;
	int     screen_n;

	screen_n = gdk_screen_get_number (screen);

	new_toplevels = panel_profile_copy_defaults_for_screen (
				client, profile_dir, screen_n, PANEL_GCONF_TOPLEVELS);
	if (!new_toplevels) {
		g_warning ("Failed to load default panel configuration. panel-default-setup.entries "
			   "hasn't been installed using gconftool-2 --load ?\n");
		new_toplevels = panel_profile_create_fallback_toplevel_list (client, profile_dir);
	}

	for (l = new_toplevels; l; l = l->next) {
		char       *toplevel_id = l->data;
		const char *key;

		key = panel_gconf_full_key (PANEL_GCONF_TOPLEVELS,
					    toplevel_id,
					    "screen");
		gconf_client_set_int (client, key, screen_n, NULL);
	}

	panel_profile_append_new_ids (client, PANEL_GCONF_TOPLEVELS, new_toplevels);

	panel_profile_copy_default_objects_for_screen (
				client, profile_dir, screen_n, PANEL_GCONF_OBJECTS);
	panel_profile_copy_default_objects_for_screen (
				client, profile_dir, screen_n, PANEL_GCONF_APPLETS);
}

static void
panel_profile_ensure_toplevel_per_screen (GConfClient *client,
					  const char  *profile_dir)
{
	GSList     *toplevels;
	GSList     *empty_screens = NULL;
	GSList     *l;
	GdkDisplay *display;
	int         n_screens, i;

	toplevels = panel_toplevel_list_toplevels ();

	display = gdk_display_get_default ();

	n_screens = gdk_display_get_n_screens (display);
	for (i = 0; i < n_screens; i++) {
		GdkScreen *screen;

		screen = gdk_display_get_screen (display, i);

		for (l = toplevels; l; l = l->next)
			if (gtk_window_get_screen (l->data) == screen)
				break;

		if (!l)
			empty_screens = g_slist_prepend (empty_screens, screen);
	}

	for (l = empty_screens; l; l = l->next)
		panel_profile_load_defaults_on_screen (client, profile_dir, l->data);

	g_slist_free (empty_screens);
}

void
panel_profile_load (void)
{
	GConfClient *client;

	client  = panel_gconf_get_client ();

	gconf_client_add_dir (client, PANEL_CONFIG_DIR "/general", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	panel_compatibility_maybe_copy_old_config (client);

	panel_compatibility_migrate_panel_id_list (client);

	panel_profile_load_list (client,
				 PANEL_CONFIG_DIR,
				 PANEL_GCONF_TOPLEVELS,
				 panel_profile_load_and_show_toplevel_startup,
				 (GConfClientNotifyFunc) panel_profile_toplevel_id_list_notify);
	panel_profile_load_list (client,
				 PANEL_CONFIG_DIR,
				 PANEL_GCONF_OBJECTS,
				 panel_profile_load_object,
				 (GConfClientNotifyFunc) panel_profile_object_id_list_notify);
	panel_profile_load_list (client,
				 PANEL_CONFIG_DIR,
				 PANEL_GCONF_APPLETS,
				 panel_profile_load_object,
				 (GConfClientNotifyFunc) panel_profile_object_id_list_notify);

	panel_profile_ensure_toplevel_per_screen (client, PANEL_CONFIG_DIR);

	panel_applet_load_queued_applets (TRUE);
}

static gboolean
get_program_listing_setting (const char *setting)
{
	GConfClient *client;
	const char  *key;
	gboolean     retval;

	client = panel_gconf_get_client ();

	key = panel_gconf_general_key (setting);
	retval = gconf_client_get_bool (client, key, NULL);

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

gboolean
panel_profile_get_enable_autocompletion (void)
{
	return get_program_listing_setting ("enable_autocompletion");
}

void
panel_profile_set_show_program_list (gboolean show_program_list)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_gconf_general_key ("show_program_list");
	gconf_client_set_bool (client, key, show_program_list, NULL);
}

gboolean
panel_profile_is_writable_show_program_list (void)
{
	GConfClient *client;
	const char  *key;

	client = panel_gconf_get_client ();

	key = panel_gconf_general_key ("show_program_list");
	return gconf_client_key_is_writable (client, key, NULL);
}

gboolean
panel_profile_can_be_moved_freely (PanelToplevel *toplevel)
{
	const char *key;
	GConfClient *client;

	if (panel_lockdown_get_locked_down () ||
	    !panel_profile_is_writable_toplevel_orientation (toplevel))
		return FALSE;

	client = panel_gconf_get_client ();

	key = panel_profile_get_toplevel_key (toplevel, "screen");
	if (!gconf_client_key_is_writable (client, key, NULL))
		return FALSE;

	key = panel_profile_get_toplevel_key (toplevel, "monitor");
	if (!gconf_client_key_is_writable (client, key, NULL))
		return FALSE;

	/* For expanded panels we don't really have to check 
	   x and y */
	if (panel_toplevel_get_expand (toplevel))
		return TRUE;

	key = panel_profile_get_toplevel_key (toplevel, "x");
	if (!gconf_client_key_is_writable (client, key, NULL))
		return FALSE;
	key = panel_profile_get_toplevel_key (toplevel, "x_right");
	if (!gconf_client_key_is_writable (client, key, NULL))
		return FALSE;
	key = panel_profile_get_toplevel_key (toplevel, "x_centered");
	if (!gconf_client_key_is_writable (client, key, NULL))
		return FALSE;

	key = panel_profile_get_toplevel_key (toplevel, "y");
	if (!gconf_client_key_is_writable (client, key, NULL))
		return FALSE;
	key = panel_profile_get_toplevel_key (toplevel, "y_bottom");
	if (!gconf_client_key_is_writable (client, key, NULL))
		return FALSE;
	key = panel_profile_get_toplevel_key (toplevel, "y_centered");
	if (!gconf_client_key_is_writable (client, key, NULL))
		return FALSE;

	return TRUE;
}
