/*
 * panel-bindings.c:
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
#include <libgnome/gnome-i18n.h>

#include "panel-gconf.h"
#include "eggaccelerators.h"

#define BINDINGS_PREFIX "/apps/metacity/window_keybindings"

typedef struct {
	char            *key;
	char            *signal;
	guint            keyval;
	GdkModifierType  modifiers;
} PanelBinding;

static GSList   *toplevels = NULL;
static gboolean  initialised = FALSE;

static PanelBinding bindings [] = {
	{ "activate_window_menu", "popup-panel-menu", 0, 0 },
	{ "toggle_maximized",     "toggle-expand",    0, 0 },
	{ "maximize",             "expand",           0, 0 },
	{ "unmaximize",           "unexpand",         0, 0 },
	{ "toggle_shaded",        "toggle-hidden",    0, 0 },
	{ "begin_move",           "begin-move",       0, 0 },
	{ "begin_resize",         "begin-resize",     0, 0 },
};

static void
panel_binding_set_from_string (PanelBinding *binding,
			       const char   *str)
{
	EggVirtualModifierType modifiers;
	
	g_assert (binding->keyval == 0);
	g_assert (binding->modifiers == 0);

	if (!str || !str [0] || !strcmp (str, "disabled")) {
		binding->keyval = 0;
		binding->modifiers = 0;
		return;
	}

	if (!egg_accelerator_parse_virtual (str, &binding->keyval, &modifiers)) {
		g_warning ("Enable to parse binding '%s'\n", str);
		return;
	}

	if (binding->keyval == 0)
		return;

	egg_keymap_resolve_virtual_modifiers (
		gdk_keymap_get_default (), modifiers, &binding->modifiers);

	if (binding->modifiers &&
	    !(binding->modifiers & gtk_accelerator_get_default_mod_mask ())) {
		binding->keyval = 0;
		binding->modifiers = 0;
	}
}

static void
panel_binding_changed (GConfClient  *client,
		       guint         cnxn_id,
		       GConfEntry   *entry,
		       PanelBinding *binding)
{
	GConfValue *value;
	GSList     *l;

	if (binding->keyval)
		for (l = toplevels; l; l = l->next)
			panel_toplevel_unset_binding (l->data,
						      binding->keyval,
						      binding->modifiers);

	binding->keyval    = 0;
	binding->modifiers = 0;

	value = gconf_entry_get_value (entry);

	if (!value || value->type != GCONF_VALUE_STRING)
		return;

	panel_binding_set_from_string (binding, gconf_value_get_string (value));

	if (!binding->keyval)
		return;

	for (l = toplevels; l; l = l->next)
		panel_toplevel_set_binding (l->data,
					    binding->keyval,
					    binding->modifiers,
					    binding->signal);
}

static void
panel_binding_watch (PanelBinding *binding,
		     const char   *key)
{
	GConfClient *client;
	GError      *error = NULL;

	client = gconf_client_get_default ();

	gconf_client_notify_add (client, key,
				(GConfClientNotifyFunc) panel_binding_changed,
				binding, NULL, &error);
	if (error) {
		g_warning (_("Error watching gconf key '%s': %s"), key, error->message);
		g_error_free (error);
	}

	g_object_unref (client);
}

static void
panel_bindings_initialise (void)
{
	GConfClient *client;
	GError      *error;
	int          i;

	client = gconf_client_get_default ();

	error = NULL;
	gconf_client_add_dir (client, BINDINGS_PREFIX,
			      GCONF_CLIENT_PRELOAD_ONELEVEL, &error);
	if (error) {
		g_warning (_("Error loading gconf dir '%s': %s"),
			   BINDINGS_PREFIX, error->message),
		g_error_free (error);
	}

	for (i = 0; i < G_N_ELEMENTS (bindings); i++) {
		const char *key;
		char       *str;

		key = panel_gconf_sprintf ("%s/%s", BINDINGS_PREFIX, bindings [i].key);

		error = NULL;
		str = gconf_client_get_string (client, key, &error);
		if (error) {
			g_warning (_("error getting value for '%s': %s"),
				   error->message, key);
			continue;
		}

		g_print ("%s: %s\n", key, str);

		panel_binding_set_from_string (&bindings [i], str);
		panel_binding_watch (&bindings [i], key);

		g_free (str);
	}

	g_object_unref (client);
}

void
panel_bindings_register_toplevel (PanelToplevel *toplevel)
{
	int i;

	if (!initialised)
		panel_bindings_initialise ();

	for (i = 0; i < G_N_ELEMENTS (bindings); i++) {
		if (!bindings [i].keyval)
			continue;

		panel_toplevel_set_binding (toplevel,
					    bindings [i].keyval,
					    bindings [i].modifiers,
					    bindings [i].signal);
	}

	toplevels = g_slist_prepend (toplevels, toplevel);
}

void
panel_bindings_unregister_toplevel (PanelToplevel *toplevel,
				    gboolean       unset_bindings)
{
	int i;

	if (!initialised)
		return;

	toplevels = g_slist_remove (toplevels, toplevel);

	if (!unset_bindings)
		return;

	for (i = 0; i < G_N_ELEMENTS (bindings); i++) {
		if (!bindings [i].keyval)
			continue;

		panel_toplevel_unset_binding (toplevel,
					      bindings [i].keyval,
					      bindings [i].modifiers);
	}
}
