/*
 * panel-applet-bindings.c: utility for key bindings.
 *
 * Copyright (c) 2011 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Vincent Untz <vuntz@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "panel-applet-bindings.h"

/******************************\
 * copied from panel-xutils.c *
\******************************/

static guint
panel_applet_bindings_get_real_modifier_mask (guint mask)
{
	guint real_mask;
	Display *display;
	int i, min_keycode, max_keycode, keysyms_per_keycode;
	int max_keycodes_per_modifier;
	KeySym *keysyms_for_keycodes;
	XModifierKeymap *modifier_keymap;

	real_mask = mask & ((Mod5Mask << 1) - 1);

	/* Already real */
	if (mask == real_mask) {
		return mask;
	}

	display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

	XDisplayKeycodes (display, &min_keycode, &max_keycode);
	keysyms_for_keycodes = XGetKeyboardMapping (display,
						    min_keycode,
						    max_keycode - min_keycode + 1,
						    &keysyms_per_keycode);

	modifier_keymap = XGetModifierMapping (display);
	max_keycodes_per_modifier = modifier_keymap->max_keypermod;

	/* Loop through all the modifiers and find out which "real"
	 * (Mod2..Mod5) modifiers Super, Hyper, and Meta are mapped to.
	 * Note, Mod1 is used by the Alt modifier */
	for (i = Mod2MapIndex * max_keycodes_per_modifier;
	     i < (Mod5MapIndex + 1) * max_keycodes_per_modifier;
	     i++) {
		int keycode;
		int j;
		KeySym *keysyms_for_keycode;
		int map_index;
		int map_mask;

		keycode = modifier_keymap->modifiermap[i];

		/* The array is sparse, there may be some
		 * empty entries.  Filter those out
		 * (along with any invalid entries) */
		if (keycode < min_keycode || keycode > max_keycode)
			continue;

		keysyms_for_keycode = keysyms_for_keycodes +
		                      (keycode - min_keycode) * keysyms_per_keycode;

		map_index = i / max_keycodes_per_modifier;

		g_assert (map_index <= Mod5MapIndex);

		map_mask = 1 << map_index;

		for (j = 0; j < keysyms_per_keycode; j++) {
			switch (keysyms_for_keycode[j]) {
				case XK_Super_L:
				case XK_Super_R:
					if (mask & GDK_SUPER_MASK)
						real_mask |= map_mask;
					break;
				case XK_Hyper_L:
				case XK_Hyper_R:
					if (mask & GDK_HYPER_MASK)
						real_mask |= map_mask;
					break;
				case XK_Meta_L:
				case XK_Meta_R:
					if (mask & GDK_META_MASK)
						real_mask |= map_mask;
					break;
				default:
					break;
			}
		}
	}

	XFreeModifiermap (modifier_keymap);
	XFree (keysyms_for_keycodes);

	return real_mask;
}

/*********************************\
 * adapted from panel-bindings.c *
\*********************************/

#define GNOME_DESKTOP_WM_PREFERENCES_SCHEMA                    "org.gnome.desktop.wm.preferences"
#define GNOME_DESKTOP_WM_PREFERENCES_MOUSE_BUTTON_MODIFIER_KEY "mouse-button-modifier"

#define DEFAULT_MOUSE_MODIFIER GDK_MOD1_MASK

static gboolean initialised = FALSE;
static GSettings *preferences = NULL;

static guint mouse_button_modifier_keymask = DEFAULT_MOUSE_MODIFIER;

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
panel_applet_bindings_init (void)
{
	char *str;

	if (initialised)
		return;

	preferences = g_settings_new (GNOME_DESKTOP_WM_PREFERENCES_SCHEMA);

	/* mouse button modifier */
	g_signal_connect (preferences, "changed::" GNOME_DESKTOP_WM_PREFERENCES_MOUSE_BUTTON_MODIFIER_KEY,
	                  G_CALLBACK (panel_bindings_mouse_modifier_changed),
	                  NULL);

	str = g_settings_get_string (preferences, GNOME_DESKTOP_WM_PREFERENCES_MOUSE_BUTTON_MODIFIER_KEY);
	panel_bindings_mouse_modifier_set_from_string (str);
	g_free (str);

	initialised = TRUE;
}

guint
panel_applet_bindings_get_mouse_button_modifier_keymask (void)
{
	guint mod;

	g_assert (mouse_button_modifier_keymask != 0);

	if (!initialised)
		panel_applet_bindings_init ();

	mod = panel_applet_bindings_get_real_modifier_mask (mouse_button_modifier_keymask);

	if (mod & gtk_accelerator_get_default_mod_mask ())
		return mod;
	else
		return panel_applet_bindings_get_real_modifier_mask (DEFAULT_MOUSE_MODIFIER);
}

/****************************\
 * copied from panel-util.c *
\****************************/

static void
panel_applet_bindings_key_event_is_binding (GdkEventKey *event,
					    GType        type,
					    const char  *signal_name,
					    gboolean    *is_popup,
					    gboolean    *is_popup_modifier)
{
	GtkBindingSet   *binding_set;
	GtkBindingEntry *binding_entry;
	gboolean         popup = FALSE;
	gboolean         popup_modifier = FALSE;
	guint            modifiers;
	char            *signal_dash;
	char            *signal_underscore;

	modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

	signal_dash = g_strdup (signal_name);
	g_strdelimit (signal_dash, "_", '-');
	signal_underscore = g_strdup (signal_name);
	g_strdelimit (signal_underscore, "-", '_');

	binding_set = gtk_binding_set_by_class (g_type_class_peek (type));

	for (binding_entry = binding_set->entries;
	     binding_entry != NULL;
	     binding_entry = binding_entry->set_next) {
		GtkBindingSignal *binding_signal;

		for (binding_signal = binding_entry->signals;
		     binding_signal != NULL;
		     binding_signal = binding_signal->next) {
			if (g_strcmp0 (binding_signal->signal_name, signal_dash) == 0 ||
			    g_strcmp0 (binding_signal->signal_name, signal_underscore) == 0) {
				if (binding_entry->keyval != event->keyval)
					break;

				popup = modifiers == binding_entry->modifiers;
				popup_modifier = modifiers == (panel_applet_bindings_get_mouse_button_modifier_keymask ()|binding_entry->modifiers);
				break;
			}
		}

		if (popup || popup_modifier)
			break;
	}

	if (is_popup)
		*is_popup = popup;
	if (is_popup_modifier)
		*is_popup_modifier = popup_modifier;

	g_free (signal_dash);
	g_free (signal_underscore);
}

void
panel_applet_bindings_key_event_is_popup (GdkEventKey *event,
					  gboolean    *is_popup,
					  gboolean    *is_popup_modifier)
{
	panel_applet_bindings_key_event_is_binding (event, GTK_TYPE_WIDGET, "popup-menu",
						    is_popup, is_popup_modifier);
}
