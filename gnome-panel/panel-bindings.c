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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-bindings.h"

#include <string.h>
#include <glib/gi18n.h>

#include "panel-schemas.h"
#include "panel-toplevel.h"
#include "panel-xutils.h"

#define DEFAULT_MOUSE_MODIFIER GDK_MOD1_MASK

typedef struct {
	guint            keyval;
	GdkModifierType  modifiers;
} KeyBinding;

typedef struct {
	const gchar *key;
	const gchar *signal;
	GSList      *list;
} PanelBinding;

static gboolean initialised = FALSE;
static GSettings *preferences = NULL;
static GSettings *keybindings = NULL;

static PanelBinding bindings [] = {
	{ GNOME_DESKTOP_WM_KEYBINDINGS_ACTIVATE_WINDOW_MENU_KEY, "popup-panel-menu", NULL },
	{ GNOME_DESKTOP_WM_KEYBINDINGS_TOGGLE_MAXIMIZED_KEY,     "toggle-expand",    NULL },
	{ GNOME_DESKTOP_WM_KEYBINDINGS_MAXIMIZE_KEY,             "expand",           NULL },
	{ GNOME_DESKTOP_WM_KEYBINDINGS_UNMAXIMIZE_KEY,           "unexpand",         NULL },
	{ GNOME_DESKTOP_WM_KEYBINDINGS_TOGGLE_SHADED_KEY,        "toggle-hidden",    NULL },
	{ GNOME_DESKTOP_WM_KEYBINDINGS_BEGIN_MOVE_KEY,           "begin-move",       NULL },
	{ GNOME_DESKTOP_WM_KEYBINDINGS_BEGIN_RESIZE_KEY,         "begin-resize",     NULL },
};

static guint mouse_button_modifier_keymask = DEFAULT_MOUSE_MODIFIER;

static void
panel_binding_set_from_string (PanelBinding *binding, const char *str)
{
	KeyBinding *tmp;

	if (!str || !str [0] || !strcmp (str, "disabled")) {
		g_slist_free_full (binding->list, g_free);
		binding->list = NULL;
		return;
	}

	tmp = g_new(KeyBinding, 1);
	tmp->keyval = 0;
	tmp->modifiers = 0;

	gtk_accelerator_parse (str, &tmp->keyval, &tmp->modifiers);

	if (tmp->keyval == 0 && tmp->modifiers == 0) {
		g_free (tmp);
		g_warning ("Unable to parse binding '%s'\n", str);
		return;
	} else {
		binding->list = g_slist_append (binding->list, tmp);
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
panel_binding_clear_entry (PanelBinding *binding, GtkBindingSet *binding_set)
{
	KeyBinding *b;
	GSList     *l;

	binding_set = get_binding_set (binding_set);

	for (l = binding->list; l != NULL; l = l->next) {
		b = l->data;
		gtk_binding_entry_remove (binding_set, b->keyval, b->modifiers);
	}

	g_slist_free_full (binding->list, g_free);
	binding->list = NULL;
}

static void
panel_binding_set_entry (PanelBinding *binding, GtkBindingSet *binding_set)
{
	KeyBinding *b;
	GSList     *l;

	binding_set = get_binding_set (binding_set);

	for (l = binding->list; l != NULL; l = l->next) {
		b = l->data;
		gtk_binding_entry_add_signal (binding_set, b->keyval, b->modifiers, binding->signal, 0);
	}
}

static void
panel_binding_changed (GSettings *settings, const gchar *key, PanelBinding *binding)
{
	gchar **array;
	gint    i;

	if (binding->list != NULL)
		panel_binding_clear_entry (binding, NULL);

	array = g_settings_get_strv (settings, key);

	if (array) {
		for (i = 0; array[i] != NULL; i++) {
			panel_binding_set_from_string (binding, array[i]);
		}

		g_strfreev (array);
	}

	if (!binding->list)
		return;

	panel_binding_set_entry (binding, NULL);
}

static void
panel_binding_watch (PanelBinding *binding, const char *key)
{
	gchar *signal_name = g_strdup_printf ("changed::%s", key);
	g_signal_connect (keybindings, signal_name, G_CALLBACK (panel_binding_changed), binding);
	g_free (signal_name);
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
panel_bindings_mouse_modifier_changed (GSettings *settings, const gchar *key)
{
	panel_bindings_mouse_modifier_set_from_string (g_settings_get_string (settings, key));
}

static void
panel_bindings_initialise (void)
{
	guint i;
	guint j;
	gchar *str;
	gchar **array;

	if (initialised)
		return;

	preferences = g_settings_new (GNOME_DESKTOP_WM_PREFERENCES_SCHEMA);
	keybindings = g_settings_new (GNOME_DESKTOP_WM_KEYBINDINGS_SCHEMA);

	for (i = 0; i < G_N_ELEMENTS (bindings); i++) {
		array = g_settings_get_strv (keybindings, bindings [i].key);

		if (array) {
			for (j = 0; array[j] != NULL; j++) {
				panel_binding_set_from_string (&bindings [i], array[j]);
			}

			g_strfreev (array);
		}

		panel_binding_watch (&bindings [i], bindings [i].key);
	}

	/* mouse button modifier */
	g_signal_connect (preferences, "changed::" GNOME_DESKTOP_WM_PREFERENCES_MOUSE_BUTTON_MODIFIER_KEY,
	                  G_CALLBACK (panel_bindings_mouse_modifier_changed),
	                  NULL);

	str = g_settings_get_string (preferences, GNOME_DESKTOP_WM_PREFERENCES_MOUSE_BUTTON_MODIFIER_KEY);
	panel_bindings_mouse_modifier_set_from_string (str);
	g_free (str);

	initialised = TRUE;
}

void
panel_bindings_set_entries (GtkBindingSet *binding_set)
{
	guint i;

	if (!initialised)
		panel_bindings_initialise ();

	for (i = 0; i < G_N_ELEMENTS (bindings); i++) {
		if (!bindings [i].list)
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
