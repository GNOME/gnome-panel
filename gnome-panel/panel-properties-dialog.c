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
#include <libgnomeui/gnome-color-picker.h>
#include <libgnomeui/gnome-file-entry.h>
#include <libgnomeui/gnome-icon-entry.h>

#include "panel-profile.h"
#include "panel-gconf.h"
#include "panel-util.h"
#include "panel-globals.h"

typedef struct {
	PanelToplevel *toplevel;

	GtkWidget     *properties_dialog;

	GtkWidget     *general_table;
	GtkWidget     *general_vbox;
	GtkWidget     *name_entry;
	GtkWidget     *name_label;
	GtkWidget     *orientation_menu;
	GtkWidget     *orientation_label;
	GtkWidget     *size_widgets;
	GtkWidget     *size_spin;
	GtkWidget     *size_label;
	GtkWidget     *size_label_pixels;
	GtkWidget     *icon_align;
	GtkWidget     *icon_entry;
	GtkWidget     *icon_label;
  	GtkWidget     *expand_toggle;
	GtkWidget     *autohide_toggle;
	GtkWidget     *hidebuttons_toggle;
	GtkWidget     *arrows_toggle;
	GtkWidget     *default_radio;
	GtkWidget     *color_radio;
	GtkWidget     *image_radio;
	GtkWidget     *color_widgets;
	GtkWidget     *image_widgets;
	GtkWidget     *color_picker;
	GtkWidget     *color_label;
	GtkWidget     *image_entry;
	GtkWidget     *opacity_scale;
	GtkWidget     *opacity_label;
	GtkWidget     *opacity_legend;

	GtkWidget     *writability_warn_general;
	GtkWidget     *writability_warn_background;

	guint          toplevel_notify;
	guint          background_notify;

	/* The theme directory of the icon, see bug #119209 */
	char          *icon_theme_dir;
} PanelPropertiesDialog;

static GQuark panel_properties_dialog_quark = 0;

static void
panel_properties_dialog_free (PanelPropertiesDialog *dialog)
{
	GConfClient *client;

	client = panel_gconf_get_client ();

	if (dialog->toplevel_notify)
		gconf_client_notify_remove (client, dialog->toplevel_notify);
	dialog->toplevel_notify = 0;

	if (dialog->background_notify)
		gconf_client_notify_remove (client, dialog->background_notify);
	dialog->background_notify = 0;

	if (dialog->properties_dialog)
		gtk_widget_destroy (dialog->properties_dialog);
	dialog->properties_dialog = NULL;

	g_free (dialog->icon_theme_dir);
	dialog->icon_theme_dir = NULL;

	g_free (dialog);
}

static char *
get_name (PanelToplevel *toplevel,
	  gboolean       is_attached)
{
	char *name;

	if (!is_attached) {
		name = panel_profile_get_toplevel_name (toplevel);
		if (!name)
			name = g_strdup (panel_toplevel_get_description (toplevel));
	} else
		name = panel_profile_get_attached_tooltip (toplevel);

	return name;
}

static void
panel_properties_dialog_name_changed (PanelPropertiesDialog *dialog,
				      GtkEntry              *entry)
{
	const char *name;

	name = gtk_entry_get_text (entry);

	if (!panel_toplevel_get_is_attached (dialog->toplevel))
		panel_profile_set_toplevel_name (dialog->toplevel, name);
	else
		panel_profile_set_attached_tooltip (dialog->toplevel, name);
}

static void
panel_properties_dialog_setup_name_entry (PanelPropertiesDialog *dialog,
					  GladeXML              *gui)
{
	char     *name;
	gboolean  is_attached;
	gboolean  is_writable;

	dialog->name_entry = glade_xml_get_widget (gui, "name_entry");
	g_return_if_fail (dialog->name_entry != NULL);

	dialog->name_label = glade_xml_get_widget (gui, "name_label");
	g_return_if_fail (dialog->name_label != NULL);

	is_attached = panel_toplevel_get_is_attached (dialog->toplevel);

	name = get_name (dialog->toplevel, is_attached);
	gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), name ? name : "");
	g_free (name);

	g_signal_connect_swapped (dialog->name_entry, "changed",
				  G_CALLBACK (panel_properties_dialog_name_changed), dialog);

	is_writable = !is_attached ? panel_profile_is_writable_toplevel_name    (dialog->toplevel) :
	                             panel_profile_is_writable_attached_tooltip (dialog->toplevel);

	if (!is_writable) {
		gtk_widget_set_sensitive (dialog->name_entry, FALSE);
		gtk_widget_set_sensitive (dialog->name_label, FALSE);
		gtk_widget_show (dialog->writability_warn_general);
	}
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
	g_return_if_fail (dialog->orientation_menu != NULL);
	dialog->orientation_label = glade_xml_get_widget (gui, "orientation_label");
	g_return_if_fail (dialog->orientation_label != NULL);

	orientation = panel_profile_get_toplevel_orientation (dialog->toplevel);
	gtk_option_menu_set_history (GTK_OPTION_MENU (dialog->orientation_menu),
				     orientation_to_history (orientation));

	g_signal_connect_swapped (dialog->orientation_menu, "changed",
				  G_CALLBACK (panel_properties_dialog_orientation_changed),
				  dialog);

	if ( ! panel_profile_is_writable_toplevel_orientation (dialog->toplevel)) {
		gtk_widget_set_sensitive (dialog->orientation_menu, FALSE);
		gtk_widget_set_sensitive (dialog->orientation_label, FALSE);
		gtk_widget_show (dialog->writability_warn_general);
	}
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
	dialog->size_widgets = glade_xml_get_widget (gui, "size_widgets");
	g_return_if_fail (dialog->size_widgets != NULL);
	dialog->size_spin = glade_xml_get_widget (gui, "size_spin");
	g_return_if_fail (dialog->size_spin != NULL);
	dialog->size_label = glade_xml_get_widget (gui, "size_label");
	g_return_if_fail (dialog->size_label != NULL);
	dialog->size_label_pixels = glade_xml_get_widget (gui, "size_label_pixels");
	g_return_if_fail (dialog->size_label_pixels != NULL);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->size_spin),
				   panel_profile_get_toplevel_size (dialog->toplevel));

	g_signal_connect_swapped (dialog->size_spin, "value_changed",
				  G_CALLBACK (panel_properties_dialog_size_changed),
				  dialog);

	if ( ! panel_profile_is_writable_toplevel_size (dialog->toplevel)) {
		gtk_widget_set_sensitive (dialog->size_spin, FALSE);
		gtk_widget_set_sensitive (dialog->size_label, FALSE);
		gtk_widget_set_sensitive (dialog->size_label_pixels, FALSE);
		gtk_widget_show (dialog->writability_warn_general);
	}
}

static void
panel_properties_dialog_icon_changed (PanelPropertiesDialog *dialog,
				      GnomeIconEntry        *entry)
{
	const char *icon = NULL;
        char       *freeme = NULL;

        icon = gnome_icon_entry_get_filename (entry);

        /* Strip dir from the icon path if in the icon
         * theme directory.  See bug #119209
         */
        if (icon && g_path_is_absolute (icon)) {
                char *dir;

                dir = g_path_get_dirname (icon);

                if (dir && dialog->icon_theme_dir && strcmp (dir, dialog->icon_theme_dir) == 0)
                        icon = freeme = g_path_get_basename (icon);

                g_free (dir);
        }

        panel_profile_set_attached_custom_icon (dialog->toplevel, icon);

        g_free (freeme);
}

static void
panel_properties_dialog_setup_icon_entry (PanelPropertiesDialog *dialog,
					  GladeXML              *gui)
{
	char *custom_icon;

	dialog->icon_align = glade_xml_get_widget (gui, "icon_align");
	g_return_if_fail (dialog->icon_align != NULL);

	dialog->icon_entry = glade_xml_get_widget (gui, "icon_entry");
	g_return_if_fail (dialog->icon_entry != NULL);

	dialog->icon_label = glade_xml_get_widget (gui, "icon_label");
	g_return_if_fail (dialog->icon_label != NULL);

	dialog->icon_theme_dir = NULL;
	custom_icon = panel_profile_get_attached_custom_icon (dialog->toplevel);
	if (custom_icon != NULL &&
	    ! g_path_is_absolute (custom_icon)) {
		/* if the icon is not absolute path name it comes from the
		   theme, and as such we wish to store the theme directory
		   where it comes from.  See bug #119209 */
		char *icon = gnome_desktop_item_find_icon (panel_icon_theme,
							   custom_icon,
							   48 /* FIXME: preffered_size */,
							   0 /* flags */);
		if (icon != NULL)
			dialog->icon_theme_dir = g_path_get_dirname (icon);
		g_free (custom_icon);
		custom_icon = icon;
	}
	gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (dialog->icon_entry), custom_icon);
	g_free (custom_icon);

	if (dialog->icon_theme_dir == NULL) {
		/* use the default pixmap directory as the standard icon_theme_dir,
		 * since the standard directory is themed */
		g_object_get (G_OBJECT (dialog->icon_entry), "pixmap_subdir",
			      &(dialog->icon_theme_dir), NULL);
	}

	g_signal_connect_swapped (dialog->icon_entry, "changed",
				  G_CALLBACK (panel_properties_dialog_icon_changed), dialog);

	if (!panel_profile_is_writable_attached_custom_icon (dialog->toplevel)) {
		gtk_widget_set_sensitive (dialog->icon_entry, FALSE);
		gtk_widget_set_sensitive (dialog->icon_label, FALSE);
		gtk_widget_show (dialog->writability_warn_general);
	}
}

/* Note: this is only for toggle buttons on the general page, if needed for togglebuttons
   elsewhere you must make this respect the writability warning thing for the right page */
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
		if ( ! panel_profile_is_writable_toplevel_##p (dialog->toplevel)) {               \
			gtk_widget_set_sensitive (dialog->n, FALSE);                              \
			gtk_widget_show (dialog->writability_warn_general);			  \
		}										  \
	}

SETUP_TOGGLE_BUTTON ("expand_toggle",      expand_toggle,      expand)
SETUP_TOGGLE_BUTTON ("autohide_toggle",    autohide_toggle,    auto_hide)
SETUP_TOGGLE_BUTTON ("hidebuttons_toggle", hidebuttons_toggle, enable_buttons)
SETUP_TOGGLE_BUTTON ("arrows_toggle",      arrows_toggle,      enable_arrows)

static void
panel_properties_dialog_color_changed (PanelPropertiesDialog *dialog,
				       guint16                red,
				       guint16                green,
				       guint16                blue)
{
	PangoColor pango_color;

	pango_color.red   = red;
	pango_color.green = green;
	pango_color.blue  = blue;

	panel_profile_set_background_pango_color (dialog->toplevel, &pango_color);
}

static void
panel_properties_dialog_setup_color_picker (PanelPropertiesDialog *dialog,
					    GladeXML              *gui)
{
	PanelColor color;

	dialog->color_picker = glade_xml_get_widget (gui, "color_picker");
	g_return_if_fail (dialog->color_picker != NULL);
	dialog->color_label = glade_xml_get_widget (gui, "color_label");
	g_return_if_fail (dialog->color_label != NULL);

	panel_profile_get_background_color (dialog->toplevel, &color);

	gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (dialog->color_picker),
				    color.gdk.red,
				    color.gdk.green,
				    color.gdk.blue,
				    65535);

	g_signal_connect_swapped (dialog->color_picker, "color_set",
				  G_CALLBACK (panel_properties_dialog_color_changed),
				  dialog);

	if ( ! panel_profile_is_writable_background_color (dialog->toplevel)) {
		gtk_widget_set_sensitive (dialog->color_picker, FALSE);
		gtk_widget_set_sensitive (dialog->color_label, FALSE);
		gtk_widget_show (dialog->writability_warn_background);
	}
}

static void
panel_properties_dialog_image_changed (PanelPropertiesDialog *dialog)
{
	GtkWidget *entry;

	entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (dialog->image_entry));

	panel_profile_set_background_image (dialog->toplevel,
					    gtk_entry_get_text (GTK_ENTRY (entry)));
}

static void
panel_properties_dialog_setup_image_entry (PanelPropertiesDialog *dialog,
					   GladeXML              *gui)
{
	GtkWidget *entry;
	char      *image;

	dialog->image_entry = glade_xml_get_widget (gui, "image_entry");

	image = panel_profile_get_background_image (dialog->toplevel);

	if (image) {
		entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (dialog->image_entry));
		gtk_entry_set_text (GTK_ENTRY (entry), image);

		g_free (image);
	}

	g_signal_connect_swapped (dialog->image_entry, "changed",
				  G_CALLBACK (panel_properties_dialog_image_changed),
				  dialog);

	if ( ! panel_profile_is_writable_background_image (dialog->toplevel)) {
		gtk_widget_set_sensitive (dialog->image_entry, FALSE);
		gtk_widget_show (dialog->writability_warn_background);
	}
}

static void
panel_properties_dialog_opacity_changed (PanelPropertiesDialog *dialog)
{
	gdouble percentage;
	guint16 opacity;

	percentage = gtk_range_get_value (GTK_RANGE (dialog->opacity_scale));

	if (percentage >= 98)
		percentage = 100;
	else if (percentage <= 2)
		percentage = 0;

	opacity = (percentage / 100) * 65535;

	panel_profile_set_background_opacity (dialog->toplevel, opacity);
}

static void
panel_properties_dialog_setup_opacity_scale (PanelPropertiesDialog *dialog,
					     GladeXML              *gui)
{
	guint16 opacity;
	gdouble percentage;

	dialog->opacity_scale = glade_xml_get_widget (gui, "opacity_scale");
	g_return_if_fail (dialog->opacity_scale != NULL);
	dialog->opacity_label = glade_xml_get_widget (gui, "opacity_label");
	g_return_if_fail (dialog->opacity_label != NULL);
	dialog->opacity_legend = glade_xml_get_widget (gui, "opacity_legend");
	g_return_if_fail (dialog->opacity_legend != NULL);

	opacity = panel_profile_get_background_opacity (dialog->toplevel);

	percentage = (opacity * 100.0) / 65535;

	gtk_range_set_value (GTK_RANGE (dialog->opacity_scale), percentage);

	g_signal_connect_swapped (dialog->opacity_scale, "value_changed",
				  G_CALLBACK (panel_properties_dialog_opacity_changed),
				  dialog);

	if ( ! panel_profile_is_writable_background_opacity (dialog->toplevel)) {
		gtk_widget_set_sensitive (dialog->opacity_scale, FALSE);
		gtk_widget_set_sensitive (dialog->opacity_label, FALSE);
		gtk_widget_set_sensitive (dialog->opacity_legend, FALSE);
		gtk_widget_show (dialog->writability_warn_background);
	}
}

static void
panel_properties_dialog_upd_sensitivity (PanelPropertiesDialog *dialog,
					 PanelBackgroundType    background_type)
{
	gtk_widget_set_sensitive (dialog->color_widgets,
				  background_type == PANEL_BACK_COLOR);
	gtk_widget_set_sensitive (dialog->image_widgets,
				  background_type == PANEL_BACK_IMAGE);
}

static void
panel_properties_dialog_background_toggled (PanelPropertiesDialog *dialog,
					    GtkWidget             *radio)
{
	PanelBackgroundType background_type = PANEL_BACK_NONE;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio)))
		return;

	if (radio == dialog->default_radio)
		background_type = PANEL_BACK_NONE;

	else if (radio == dialog->color_radio)
		background_type = PANEL_BACK_COLOR;

	else if (radio == dialog->image_radio)
		background_type = PANEL_BACK_IMAGE;

	panel_properties_dialog_upd_sensitivity (dialog, background_type);

	panel_profile_set_background_type (dialog->toplevel, background_type);
}
				
static void
panel_properties_dialog_setup_background_radios (PanelPropertiesDialog *dialog,
						 GladeXML              *gui)
{
	PanelBackgroundType  background_type;
	GtkWidget           *active_radio;

	dialog->default_radio     = glade_xml_get_widget (gui, "default_radio");
	dialog->color_radio       = glade_xml_get_widget (gui, "color_radio");
	dialog->image_radio       = glade_xml_get_widget (gui, "image_radio");
	dialog->color_widgets     = glade_xml_get_widget (gui, "color_widgets");
	dialog->image_widgets     = glade_xml_get_widget (gui, "image_widgets");

	background_type = panel_profile_get_background_type (dialog->toplevel);
	switch (background_type) {
	case PANEL_BACK_NONE:
		active_radio = dialog->default_radio;
		break;
	case PANEL_BACK_COLOR:
		active_radio = dialog->color_radio;
		break;
	case PANEL_BACK_IMAGE:
		active_radio = dialog->image_radio;
		break;
	default:
		active_radio = NULL;
		g_assert_not_reached ();
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_radio), TRUE);

	panel_properties_dialog_upd_sensitivity (dialog, background_type);

	g_signal_connect_swapped (dialog->default_radio, "toggled",
				  G_CALLBACK (panel_properties_dialog_background_toggled),
				  dialog);
	g_signal_connect_swapped (dialog->color_radio, "toggled",
				  G_CALLBACK (panel_properties_dialog_background_toggled),
				  dialog);
	g_signal_connect_swapped (dialog->image_radio, "toggled",
				  G_CALLBACK (panel_properties_dialog_background_toggled),
				  dialog);

	if ( ! panel_profile_is_writable_background_type (dialog->toplevel)) {
		gtk_widget_set_sensitive (dialog->default_radio, FALSE);
		gtk_widget_set_sensitive (dialog->color_radio, FALSE);
		gtk_widget_set_sensitive (dialog->image_radio, FALSE);
		gtk_widget_show (dialog->writability_warn_background);
	}
}

static void
panel_properties_update_arrows_toggle_visible (PanelPropertiesDialog *dialog,
					       GtkToggleButton       *toggle)
{
	if (gtk_toggle_button_get_active (toggle))
		gtk_widget_set_sensitive (dialog->arrows_toggle,
					  panel_profile_is_writable_toplevel_enable_arrows (dialog->toplevel));
	else
		gtk_widget_set_sensitive (dialog->arrows_toggle, FALSE);
}

static void
panel_properties_dialog_response (PanelPropertiesDialog *dialog,
				  int                    response,
				  GtkWidget             *properties_dialog)
{
	switch (response) {
	case GTK_RESPONSE_CLOSE:
		gtk_widget_destroy (properties_dialog);
		break;
	case GTK_RESPONSE_HELP:
		panel_show_help (gtk_window_get_screen (GTK_WINDOW (properties_dialog)),
				 "user-guide.xml", "gospanel-28");
		break;
	default:
		break;
	}
}

static void
panel_properties_dialog_destroy (PanelPropertiesDialog *dialog)
{
	panel_toplevel_unblock_auto_hide (PANEL_TOPLEVEL (dialog->toplevel));
	g_object_set_qdata (G_OBJECT (dialog->toplevel),
			    panel_properties_dialog_quark,
			    NULL);
}

static void
panel_properties_dialog_update_name (PanelPropertiesDialog *dialog,
				     GConfValue            *value)
{
	const char *text = NULL;

	if (value && value->type == GCONF_VALUE_STRING)
		text = gconf_value_get_string (value);

	if (text)
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
panel_properties_dialog_toplevel_notify (GConfClient           *client,
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

static void
panel_properties_dialog_update_background_type (PanelPropertiesDialog *dialog,
						GConfValue            *value)
{
	PanelBackgroundType  background_type;
	GtkWidget           *active_radio;

	if (!value || value->type != GCONF_VALUE_STRING)
		return;

	if (!panel_profile_map_background_type_string (gconf_value_get_string (value),
						       &background_type))
		return;

	switch (background_type) {
	case PANEL_BACK_NONE:
		active_radio = dialog->default_radio;
		break;
	case PANEL_BACK_COLOR:
		active_radio = dialog->color_radio;
		break;
	case PANEL_BACK_IMAGE:
		active_radio = dialog->image_radio;
		break;
	default:
		active_radio = NULL;
		g_assert_not_reached ();
		break;
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_radio), TRUE);
}

static void
panel_properties_dialog_update_background_color (PanelPropertiesDialog *dialog,
						 GConfValue            *value)
{
	PangoColor color = { 0, };
	guint16    red, green, blue;

	if (!value || value->type != GCONF_VALUE_STRING)
		return;
	
	pango_color_parse (&color, gconf_value_get_string (value));

	gnome_color_picker_get_i16 (GNOME_COLOR_PICKER (dialog->color_picker),
				    &red, &green, &blue, NULL);
	if (red != color.red || green != color.green || blue != color.blue)
		gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (dialog->color_picker),
					    color.red,
					    color.green,
					    color.blue,	
					    65535);
}

static void
panel_properties_dialog_update_background_opacity (PanelPropertiesDialog *dialog,
						   GConfValue            *value)
{
	gdouble percentage;

	if (!value || value->type != GCONF_VALUE_INT)
		return;

	percentage = ((gdouble) (gconf_value_get_int (value) * 100)) / 65535;

	if ((int) gtk_range_get_value (GTK_RANGE (dialog->opacity_scale)) != (int) percentage)
		gtk_range_set_value (GTK_RANGE (dialog->opacity_scale), percentage);
}

static void
panel_properties_dialog_update_background_image (PanelPropertiesDialog *dialog,
						 GConfValue            *value)
{
	GtkEntry   *entry;
	const char *old_text;
	const char *text;

	if (!value || value->type != GCONF_VALUE_STRING)
		return;

	entry = GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (dialog->image_entry)));

	text = gconf_value_get_string (value);
	old_text = gtk_entry_get_text (entry);

	if (text && (!old_text || strcmp (text, old_text)))
		gtk_entry_set_text (entry, text);
}

static void
panel_properties_dialog_background_notify (GConfClient           *client,
					   guint                  cnxn_id,
					   GConfEntry            *entry,
					   PanelPropertiesDialog *dialog)
{
	GConfValue *value;
	const char *key;

	key = panel_gconf_basename (gconf_entry_get_key (entry));

	value = gconf_entry_get_value (entry);

	if (!strcmp (key, "type"))
		panel_properties_dialog_update_background_type (dialog, value);
	else if (!strcmp (key, "color"))
		panel_properties_dialog_update_background_color (dialog, value);
	else if (!strcmp (key, "opacity"))
		panel_properties_dialog_update_background_opacity (dialog, value);
	else if (!strcmp (key, "image"))
		panel_properties_dialog_update_background_image (dialog, value);
}

static void
panel_properties_dialog_remove_orientation_menu (PanelPropertiesDialog *dialog)
{
	GtkContainer *container = GTK_CONTAINER (dialog->general_table);
	GtkTable     *table     = GTK_TABLE (dialog->general_table);

	g_object_ref (dialog->size_label);
	g_object_ref (dialog->size_widgets);
	g_object_ref (dialog->icon_label);
	g_object_ref (dialog->icon_align);

	gtk_container_remove (container, dialog->orientation_label);
	gtk_container_remove (container, dialog->orientation_menu);
	gtk_container_remove (container, dialog->size_label);
	gtk_container_remove (container, dialog->size_widgets);
	gtk_container_remove (container, dialog->icon_label);
	gtk_container_remove (container, dialog->icon_align);

	gtk_table_attach_defaults (table, dialog->size_label,   0, 1, 1, 2);
	gtk_table_attach_defaults (table, dialog->size_widgets, 1, 2, 1, 2);
	gtk_table_attach_defaults (table, dialog->icon_label,   0, 1, 2, 3);
	gtk_table_attach_defaults (table, dialog->icon_align,   1, 2, 2, 3);

	dialog->orientation_label = NULL;
	dialog->orientation_menu = NULL;
	g_object_unref (dialog->size_label);
	g_object_unref (dialog->size_widgets);
	g_object_unref (dialog->icon_label);
	g_object_unref (dialog->icon_align);

	gtk_table_resize (table, 3, 2);
}

static void
panel_properties_dialog_remove_icon_entry (PanelPropertiesDialog *dialog)
{
	GtkContainer *container = GTK_CONTAINER (dialog->general_table);

	gtk_container_remove (container, dialog->icon_label);
	gtk_container_remove (container, dialog->icon_align);

	dialog->icon_label = NULL;
	dialog->icon_align = NULL;
	dialog->icon_entry = NULL;

	gtk_table_resize (GTK_TABLE (dialog->general_table), 3, 2);
}

static void
panel_properties_dialog_remove_toggles (PanelPropertiesDialog *dialog)
{
	GtkContainer *container = GTK_CONTAINER (dialog->general_vbox);

	gtk_container_remove (container, dialog->autohide_toggle);
	gtk_container_remove (container, dialog->expand_toggle);

	dialog->autohide_toggle = NULL;
	dialog->expand_toggle   = NULL;
}

static void
panel_properties_dialog_update_for_attached (PanelPropertiesDialog *dialog,
					     gboolean               attached)
{
	if (!attached)
		panel_properties_dialog_remove_icon_entry (dialog);
	else {
		panel_properties_dialog_remove_toggles (dialog);
		panel_properties_dialog_remove_orientation_menu (dialog);
	}
}

static PanelPropertiesDialog *
panel_properties_dialog_new (PanelToplevel *toplevel,
			     GladeXML      *gui)
{
	PanelPropertiesDialog *dialog;

	dialog = g_new0 (PanelPropertiesDialog, 1);

	g_object_set_qdata_full (G_OBJECT (toplevel),
				 panel_properties_dialog_quark,
				 dialog,
				 (GDestroyNotify) panel_properties_dialog_free);

	dialog->toplevel = toplevel;

	dialog->properties_dialog = glade_xml_get_widget (gui, "panel_properties_dialog");
	g_signal_connect_swapped (dialog->properties_dialog, "response",
				  G_CALLBACK (panel_properties_dialog_response), dialog);
	g_signal_connect_swapped (dialog->properties_dialog, "destroy",
				  G_CALLBACK (panel_properties_dialog_destroy), dialog);

	gtk_window_set_screen (GTK_WINDOW (dialog->properties_dialog),
			       gtk_window_get_screen (GTK_WINDOW (toplevel)));

	dialog->writability_warn_general = glade_xml_get_widget (gui, "writability_warn_general");
	dialog->writability_warn_background = glade_xml_get_widget (gui, "writability_warn_background");

	dialog->general_vbox  = glade_xml_get_widget (gui, "general_vbox");
	dialog->general_table = glade_xml_get_widget (gui, "general_table");

	panel_properties_dialog_setup_name_entry         (dialog, gui);
	panel_properties_dialog_setup_orientation_menu   (dialog, gui);
	panel_properties_dialog_setup_size_spin          (dialog, gui);
	panel_properties_dialog_setup_icon_entry         (dialog, gui);
	panel_properties_dialog_setup_expand_toggle      (dialog, gui);
	panel_properties_dialog_setup_autohide_toggle    (dialog, gui);
	panel_properties_dialog_setup_hidebuttons_toggle (dialog, gui);
	panel_properties_dialog_setup_arrows_toggle      (dialog, gui);

	panel_properties_update_arrows_toggle_visible (
		dialog, GTK_TOGGLE_BUTTON (dialog->hidebuttons_toggle));
	g_signal_connect_swapped (dialog->hidebuttons_toggle, "toggled",
				  G_CALLBACK (panel_properties_update_arrows_toggle_visible),
				  dialog);

	dialog->toplevel_notify =
		panel_profile_toplevel_notify_add (
			dialog->toplevel,
			NULL,
			(GConfClientNotifyFunc) panel_properties_dialog_toplevel_notify,
			dialog);

	panel_properties_dialog_setup_color_picker      (dialog, gui);
	panel_properties_dialog_setup_image_entry       (dialog, gui);
	panel_properties_dialog_setup_opacity_scale     (dialog, gui);
	panel_properties_dialog_setup_background_radios (dialog, gui);

	dialog->background_notify =
		panel_profile_toplevel_notify_add (
			dialog->toplevel,
			"background",
			(GConfClientNotifyFunc) panel_properties_dialog_background_notify,
			dialog);

	panel_properties_dialog_update_for_attached (dialog,
						     panel_toplevel_get_is_attached (dialog->toplevel));

	panel_toplevel_block_auto_hide (dialog->toplevel);

	gtk_widget_show (dialog->properties_dialog);

	return dialog;
}

void
panel_properties_dialog_present (PanelToplevel *toplevel)
{
	PanelPropertiesDialog *dialog;
	GladeXML              *gui;

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

	gui = glade_xml_new (GLADEDIR "/panel-properties-dialog.glade",
			     "panel_properties_dialog",
			     NULL);

	dialog = panel_properties_dialog_new (toplevel, gui);

	g_object_unref (gui);
}
