/*
 * panel-bindings.c: panel keybindings support module
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

#include "panel-bindings.h"

#include <string.h>
#include <glib/gi18n.h>

#include "panel-gconf.h"
#include "panel-toplevel.h"
#include "panel-xutils.h"

#define BINDINGS_PREFIX    "/apps/metacity/window_keybindings"
#define MOUSE_MODIFIER_DIR "/apps/metacity/general"
#define MOUSE_MODIFIER_KEY "/apps/metacity/general/mouse_button_modifier"
#define DEFAULT_MOUSE_MODIFIER GDK_MOD1_MASK

typedef struct {
	char            *key;
	char            *signal;
	guint            keyval;
	GdkModifierType  modifiers;
} PanelBinding;

static gboolean initialised = FALSE;

static PanelBinding bindings [] = {
	{ "activate_window_menu", "popup-panel-menu", 0, 0 },
	{ "toggle_maximized",     "toggle-expand",    0, 0 },
	{ "maximize",             "expand",           0, 0 },
	{ "unmaximize",           "unexpand",         0, 0 },
	{ "toggle_shaded",        "toggle-hidden",    0, 0 },
	{ "begin_move",           "begin-move",       0, 0 },
	{ "begin_resize",         "begin-resize",     0, 0 },
};

static guint mouse_button_modifier_keymask = DEFAULT_MOUSE_MODIFIER;

static void
panel_binding_set_from_string (PanelBinding *binding,
			       const char   *str)
{
	g_assert (binding->keyval == 0);
	g_assert (binding->modifiers == 0);

	if (!str || !str [0] || !strcmp (str, "disabled")) {
		binding->keyval = 0;
		binding->modifiers = 0;
		return;
	}

	gtk_accelerator_parse (str, &binding->keyval, &binding->modifiers);
	if (binding->keyval == 0 && binding->modifiers == 0) {
		g_warning ("Unable to parse binding '%s'\n", str);
		return;
	}
}

static inline GtkBindingSet *
get_binding_set (GtkBindingSet *binding_set)
{
	if (!binding_set) {
		PanelToplevelClass *toplevel_class;

		toplevel_class = g_type_class_peek (PANEL_TYPE_TOPLEVEL);
		if (!toplevel_class)
			return NULL;

		g_assert (PANEL_IS_TOPLEVEL_CLASS (toplevel_class));

		binding_set = gtk_binding_set_by_class (toplevel_class);
	}

	return binding_set;
}

static void
panel_binding_clear_entry (PanelBinding  *binding,
			   GtkBindingSet *binding_set)
{
	binding_set = get_binding_set (binding_set);

        gtk_binding_entry_remove (binding_set, binding->keyval, binding->modifiers);
}

static void
panel_binding_set_entry (PanelBinding  *binding,
			 GtkBindingSet *binding_set)
{
	binding_set = get_binding_set (binding_set);

        gtk_binding_entry_add_signal (binding_set,	
				      binding->keyval,
				      binding->modifiers,
				      binding->signal,
				      0);
}

static void
panel_binding_changed (GConfClient  *client,
		       guint         cnxn_id,
		       GConfEntry   *entry,
		       PanelBinding *binding)
{
	GConfValue *value;

	if (binding->keyval)
		panel_binding_clear_entry (binding, NULL);

	binding->keyval    = 0;
	binding->modifiers = 0;

	value = gconf_entry_get_value (entry);

	if (!value || value->type != GCONF_VALUE_STRING)
		return;

	panel_binding_set_from_string (binding, gconf_value_get_string (value));

	if (!binding->keyval)
		return;

	panel_binding_set_entry (binding, NULL);
}

static void
panel_binding_watch (PanelBinding *binding,
		     const char   *key)
{
	GError *error = NULL;

	gconf_client_notify_add (panel_gconf_get_client (), key,
				(GConfClientNotifyFunc) panel_binding_changed,
				binding, NULL, &error);
	if (error) {
		g_warning ("Error watching gconf key '%s': %s",
			   key, error->message);
		g_error_free (error);
	}
}

/*
 * Keep code in panel-applet-bindings.c in sync!
 */
static void
panel_bindings_mouse_modifier_set_from_string (const char *str)
{
	guint modifier_keysym;
	guint modifier_keymask;

	gtk_accelerator_parse (str, &modifier_keysym, &modifier_keymask);

	if (modifier_keysym == 0 && modifier_keymask == 0) {
		g_warning ("Unable to parse mouse modifier '%s'\n", str);
		return;
	}

	if (modifier_keymask)
		mouse_button_modifier_keymask = modifier_keymask;
	else
		mouse_button_modifier_keymask = DEFAULT_MOUSE_MODIFIER;
}

static void
panel_bindings_mouse_modifier_changed (GConfClient  *client,
				       guint         cnxn_id,
				       GConfEntry   *entry,
				       gpointer      user_data)
{
	GConfValue *value;
	const char *str;

	value = gconf_entry_get_value (entry);

	if (!value || value->type != GCONF_VALUE_STRING)
		return;

	str = gconf_value_get_string (value);
	panel_bindings_mouse_modifier_set_from_string (str);
}

static void
panel_bindings_initialise (void)
{
	GConfClient *client;
	GError      *error;
	int          i;
	char        *str;

	if (initialised)
		return;

	client = panel_gconf_get_client ();

	error = NULL;
	gconf_client_add_dir (client, BINDINGS_PREFIX,
			      GCONF_CLIENT_PRELOAD_ONELEVEL, &error);
	if (error) {
		g_warning ("Error loading gconf directory '%s': %s",
			   BINDINGS_PREFIX, error->message),
		g_error_free (error);
	}

	for (i = 0; i < G_N_ELEMENTS (bindings); i++) {
		const char *key;

		key = panel_gconf_sprintf ("%s/%s", BINDINGS_PREFIX, bindings [i].key);

		error = NULL;
		str = gconf_client_get_string (client, key, &error);
		if (error) {
			g_warning ("Error getting value for '%s': %s",
				   key, error->message);
			g_error_free (error);
			continue;
		}

		panel_binding_set_from_string (&bindings [i], str);
		panel_binding_watch (&bindings [i], key);

		g_free (str);
	}

	/* mouse button modifier */
	error = NULL;
	gconf_client_add_dir (client, MOUSE_MODIFIER_DIR,
			      GCONF_CLIENT_PRELOAD_NONE, &error);
	if (error) {
		g_warning ("Error loading gconf directory '%s': %s",
			   MOUSE_MODIFIER_DIR, error->message),
		g_error_free (error);
	}

	error = NULL;
	gconf_client_notify_add (client, MOUSE_MODIFIER_KEY,
				 panel_bindings_mouse_modifier_changed,
				 NULL, NULL, &error);
	if (error) {
		g_warning ("Error watching gconf key '%s': %s",
			   MOUSE_MODIFIER_KEY, error->message);
		g_error_free (error);
	}

	error = NULL;
	str = gconf_client_get_string (client, MOUSE_MODIFIER_KEY, &error);
	if (error) {
		g_warning ("Error getting value for '%s': %s",
			   MOUSE_MODIFIER_KEY, error->message);
		g_error_free (error);
	} else {
		panel_bindings_mouse_modifier_set_from_string (str);
		g_free (str);
	}

	initialised = TRUE;
}

void
panel_bindings_set_entries (GtkBindingSet *binding_set)
{
	int i;

	if (!initialised)
		panel_bindings_initialise ();

	for (i = 0; i < G_N_ELEMENTS (bindings); i++) {
		if (!bindings [i].keyval)
			continue;

		panel_binding_set_entry (&bindings [i], binding_set);
	}
}

guint
panel_bindings_get_mouse_button_modifier_keymask (void)
{
	guint mod;

	g_assert (mouse_button_modifier_keymask != 0);

	if (!initialised)
		panel_bindings_initialise ();

	mod = panel_get_real_modifier_mask (mouse_button_modifier_keymask);

	if (mod & gtk_accelerator_get_default_mod_mask ())
		return mod;
	else
		return panel_get_real_modifier_mask (DEFAULT_MOUSE_MODIFIER);
}
