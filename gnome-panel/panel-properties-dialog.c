/*
 * panel-properties-dialog.c:
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

 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-properties-dialog.h"

#include <string.h>
#include <glade/glade-xml.h>

#include "panel-profile.h"
#include "panel-gconf.h"
#include "panel-util.h"

typedef struct {
	PanelToplevel *toplevel;

	GtkWidget     *properties_dialog;

	GtkWidget     *name_entry;
	GtkWidget     *orientation_menu;
	GtkWidget     *size_spin;
	GtkWidget     *expand_toggle;
	GtkWidget     *autohide_toggle;
	GtkWidget     *hidebuttons_toggle;
	GtkWidget     *arrows_toggle;

	guint          gconf_notify;
} PanelPropertiesDialog;

static GQuark panel_properties_dialog_quark = 0;

static void
panel_properties_dialog_free (PanelPropertiesDialog *dialog)
{
	GConfClient *client;

	client = gconf_client_get_default ();

	if (dialog->gconf_notify)
		gconf_client_notify_remove (client, dialog->gconf_notify);
	dialog->gconf_notify = 0;

	g_object_unref (client);

	if (dialog->properties_dialog)
		gtk_widget_destroy (dialog->properties_dialog);
	dialog->properties_dialog = NULL;

	g_free (dialog);
}

static char *
get_name (PanelToplevel *toplevel)
{
	char *name;

	name = panel_profile_get_toplevel_name (toplevel);
	if (!name)
		name = g_strdup (panel_toplevel_get_description (toplevel));

	return name;
}

static void
panel_properties_dialog_name_changed (PanelPropertiesDialog *dialog,
				      GtkEntry              *entry)
{
	panel_profile_set_toplevel_name (dialog->toplevel, gtk_entry_get_text (entry));
}

static void
panel_properties_dialog_setup_name_entry (PanelPropertiesDialog *dialog,
					  GladeXML              *gui)
{
	char *name;

	dialog->name_entry = glade_xml_get_widget (gui, "name_entry");

	name = get_name (dialog->toplevel);
	gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), name);
	g_free (name);

	g_signal_connect_swapped (dialog->name_entry, "changed",
				  G_CALLBACK (panel_properties_dialog_name_changed), dialog);
}

/* Sucky: menu order is Top, Bottom, Left, Right */
typedef enum {
	HISTORY_TOP = 0,
	HISTORY_BOTTOM,
	HISTORY_LEFT,
	HISTORY_RIGHT
} OrientationMenuOrder;

static int
orientation_to_history (PanelOrientation orientation)
{
	switch (orientation) {
	case PANEL_ORIENTATION_TOP:
		return HISTORY_TOP;
	case PANEL_ORIENTATION_BOTTOM:
		return HISTORY_BOTTOM;
	case PANEL_ORIENTATION_LEFT:
		return HISTORY_LEFT;
	case PANEL_ORIENTATION_RIGHT:
		return HISTORY_RIGHT;
	}

	g_assert_not_reached ();

	return 0;
}

static PanelOrientation
history_to_orientation (int history)
{
	switch (history) {
	case HISTORY_TOP:
		return PANEL_ORIENTATION_TOP;
	case HISTORY_BOTTOM:
		return PANEL_ORIENTATION_BOTTOM;
	case HISTORY_LEFT:
		return PANEL_ORIENTATION_LEFT;
	case HISTORY_RIGHT:
		return PANEL_ORIENTATION_RIGHT;
	}

	g_assert_not_reached ();

	return 0;

}

static void
panel_properties_dialog_orientation_changed (PanelPropertiesDialog *dialog,
					     GtkOptionMenu         *option_menu)
{
	PanelOrientation orientation;

	orientation = history_to_orientation (gtk_option_menu_get_history (option_menu));

	panel_profile_set_toplevel_orientation (dialog->toplevel, orientation);
}

static void
panel_properties_dialog_setup_orientation_menu (PanelPropertiesDialog *dialog,
						GladeXML              *gui)
{
	PanelOrientation orientation;

	dialog->orientation_menu = glade_xml_get_widget (gui, "orientation_menu");

	orientation = panel_profile_get_toplevel_orientation (dialog->toplevel);
	gtk_option_menu_set_history (GTK_OPTION_MENU (dialog->orientation_menu),
				     orientation_to_history (orientation));

	g_signal_connect_swapped (dialog->orientation_menu, "changed",
				  G_CALLBACK (panel_properties_dialog_orientation_changed),
				  dialog);
}

static void
panel_properties_dialog_size_changed (PanelPropertiesDialog *dialog,
				      GtkSpinButton         *spin_button)
{
	panel_profile_set_toplevel_size (dialog->toplevel,
					 gtk_spin_button_get_value_as_int (spin_button));
}

static void
panel_properties_dialog_setup_size_spin (PanelPropertiesDialog *dialog,
					 GladeXML              *gui)
{
	dialog->size_spin = glade_xml_get_widget (gui, "size_spin");

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->size_spin),
				   panel_profile_get_toplevel_size (dialog->toplevel));

	g_signal_connect_swapped (dialog->size_spin, "value_changed",
				  G_CALLBACK (panel_properties_dialog_size_changed),
				  dialog);
}

#define SETUP_TOGGLE_BUTTON(wid, n, p)                                                            \
	static void                                                                               \
	panel_properties_dialog_##n (PanelPropertiesDialog *dialog,                               \
				     GtkToggleButton       *n)                                    \
	{                                                                                         \
		panel_profile_set_toplevel_##p (dialog->toplevel,                                 \
						gtk_toggle_button_get_active (n));                \
	}                                                                                         \
	static void                                                                               \
	panel_properties_dialog_setup_##n (PanelPropertiesDialog *dialog,                         \
					   GladeXML              *gui)                            \
	{                                                                                         \
		dialog->n = glade_xml_get_widget (gui, wid);                                      \
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->n),                      \
					      panel_profile_get_toplevel_##p (dialog->toplevel)); \
		g_signal_connect_swapped (dialog->n, "toggled",                                   \
					  G_CALLBACK (panel_properties_dialog_##n), dialog);      \
	}

SETUP_TOGGLE_BUTTON ("expand_toggle",      expand_toggle,      expand)
SETUP_TOGGLE_BUTTON ("autohide_toggle",    autohide_toggle,    auto_hide)
SETUP_TOGGLE_BUTTON ("hidebuttons_toggle", hidebuttons_toggle, enable_buttons)
SETUP_TOGGLE_BUTTON ("arrows_toggle",      arrows_toggle,      enable_arrows)

static void
panel_properties_update_arrows_toggle_visible (PanelPropertiesDialog *dialog,
					       GtkToggleButton       *toggle)
{
	gtk_widget_set_sensitive (dialog->arrows_toggle,
				  gtk_toggle_button_get_active (toggle));
}

static void
panel_properties_dialog_response (PanelPropertiesDialog *dialog,
				  int                    response,
				  GtkWidget             *properties_dialog)
{
	switch (response) {
	case GTK_RESPONSE_CLOSE:
		gtk_widget_hide (properties_dialog);
		break;
	case GTK_RESPONSE_HELP:
		panel_show_help (gtk_window_get_screen (GTK_WINDOW (properties_dialog)),
				 "wgospanel.xml", "gospanel-28");
		break;
	default:
		break;
	}
}

static void
panel_properties_dialog_update_name (PanelPropertiesDialog *dialog,
				     GConfValue            *value)
{
	const char *text = NULL;

	if (value && value->type == GCONF_VALUE_STRING)
		text = gconf_value_get_string (value);

	gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), text);
}

static void
panel_properties_dialog_update_orientation (PanelPropertiesDialog *dialog,
					    GConfValue            *value)
{
	PanelOrientation orientation;

	if (!value || value->type != GCONF_VALUE_STRING)
		return;

	if (!panel_profile_map_orientation_string (gconf_value_get_string (value), &orientation))
		return;

	gtk_option_menu_set_history (GTK_OPTION_MENU (dialog->orientation_menu),
				     orientation_to_history (orientation));
}

static void
panel_properties_dialog_update_size (PanelPropertiesDialog *dialog,
				     GConfValue            *value)
{
	if (!value || value->type != GCONF_VALUE_INT)
		return;

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->size_spin),
				   gconf_value_get_int (value));
}

static void
panel_properties_dialog_notify (GConfClient           *client,
				guint                  cnxn_id,
				GConfEntry            *entry,
				PanelPropertiesDialog *dialog)
{
	GConfValue *value;
	const char *key;

	key = panel_gconf_basename (gconf_entry_get_key (entry));

	value = gconf_entry_get_value (entry);

#define UPDATE_TOGGLE(p, n)                                                                        \
	if (!strcmp (key, p)) {                                                                    \
		if (value && value->type == GCONF_VALUE_BOOL) {                                    \
			gboolean val = gconf_value_get_bool (value);                               \
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->n)) != val)   \
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->n), val); \
		}                                                                                  \
	}

	if (!strcmp (key, "name"))
		panel_properties_dialog_update_name (dialog, value);
	else if (!strcmp (key, "orientation"))
		panel_properties_dialog_update_orientation (dialog, value);
	else if (!strcmp (key, "size"))
		panel_properties_dialog_update_size (dialog, value);
	else UPDATE_TOGGLE ("expand",         expand_toggle)
	else UPDATE_TOGGLE ("auto_hide",      autohide_toggle)
	else UPDATE_TOGGLE ("enable_buttons", hidebuttons_toggle)
	else UPDATE_TOGGLE ("enable_arrows",  arrows_toggle)
}

static PanelPropertiesDialog *
panel_properties_dialog_new (PanelToplevel *toplevel,
			     GladeXML      *gui)
{
	PanelPropertiesDialog *dialog;

	dialog = g_new (PanelPropertiesDialog, 1);

	g_object_set_qdata_full (G_OBJECT (toplevel),
				 panel_properties_dialog_quark,
				 dialog,
				 (GDestroyNotify) panel_properties_dialog_free);

	dialog->toplevel = toplevel;

	dialog->properties_dialog = glade_xml_get_widget (gui, "panel_properties_dialog");
	g_signal_connect_swapped (dialog->properties_dialog, "response",
				  G_CALLBACK (panel_properties_dialog_response), dialog);

	gtk_window_set_screen (GTK_WINDOW (dialog->properties_dialog),
			       gtk_window_get_screen (GTK_WINDOW (toplevel)));

	panel_properties_dialog_setup_name_entry         (dialog, gui);
	panel_properties_dialog_setup_orientation_menu   (dialog, gui);
	panel_properties_dialog_setup_size_spin          (dialog, gui);
	panel_properties_dialog_setup_expand_toggle      (dialog, gui);
	panel_properties_dialog_setup_autohide_toggle    (dialog, gui);
	panel_properties_dialog_setup_hidebuttons_toggle (dialog, gui);
	panel_properties_dialog_setup_arrows_toggle      (dialog, gui);

	panel_properties_update_arrows_toggle_visible (
		dialog, GTK_TOGGLE_BUTTON (dialog->hidebuttons_toggle));
	g_signal_connect_swapped (dialog->hidebuttons_toggle, "toggled",
				  G_CALLBACK (panel_properties_update_arrows_toggle_visible),
				  dialog);

	dialog->gconf_notify = panel_profile_toplevel_notify_add (
					dialog->toplevel,
					(GConfClientNotifyFunc) panel_properties_dialog_notify,
					dialog);

	gtk_widget_show (dialog->properties_dialog);
}

void
panel_properties_dialog_present (PanelToplevel *toplevel)
{
	PanelPropertiesDialog *dialog;
	GladeXML              *gui;
	char                  *glade_file;

	if (!panel_properties_dialog_quark)
		panel_properties_dialog_quark =
			g_quark_from_static_string ("panel-properties-dialog");

	dialog = g_object_get_qdata (G_OBJECT (toplevel), panel_properties_dialog_quark);
	if (dialog) {
		gtk_window_set_screen (GTK_WINDOW (dialog->properties_dialog),
				       gtk_window_get_screen (GTK_WINDOW (toplevel)));
		gtk_window_present (GTK_WINDOW (dialog->properties_dialog));
		return;
	}

	if (g_file_test ("panel-properties-dialog.glade", G_FILE_TEST_EXISTS))
		glade_file = "panel-properties-dialog.glade";
	else
		glade_file = GLADEDIR "/panel-properties-dialog.glade";

	gui = glade_xml_new (glade_file, "panel_properties_dialog", NULL);

	dialog = panel_properties_dialog_new (toplevel, gui);

	g_object_unref (gui);
}
