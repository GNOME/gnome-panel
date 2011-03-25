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

#include <string.h>
#include <glib/gi18n.h>

#include <libpanel-util/panel-error.h>
#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-gtk.h>
#include <libpanel-util/panel-icon-chooser.h>
#include <libpanel-util/panel-show.h>

#include "panel-icon-names.h"
#include "panel-schemas.h"
#include "panel-widget.h"

#include "panel-properties-dialog.h"

typedef struct {
	PanelToplevel *toplevel;

	GSettings     *settings;
	GSettings     *settings_background;

	GtkWidget     *properties_dialog;

	GtkWidget     *orientation_combo;
	GtkWidget     *size_spin;
	GtkWidget     *hidebuttons_toggle;
	GtkWidget     *arrows_toggle;
	GtkWidget     *default_radio;
	GtkWidget     *color_radio;
	GtkWidget     *image_radio;
	GtkWidget     *color_widgets;
	GtkWidget     *image_widgets;
	GtkWidget     *color_button;
	GtkWidget     *image_chooser;
	GtkWidget     *opacity_scale;

	GtkWidget     *writability_warn_general;
	GtkWidget     *writability_warn_background;
} PanelPropertiesDialog;

static GQuark panel_properties_dialog_quark = 0;

enum {
	COLUMN_TEXT,
	COLUMN_ITEM,
	NUMBER_COLUMNS
};

typedef struct {
	const char       *name;
	PanelOrientation  orientation;
} OrientationComboItem;

static OrientationComboItem orientation_items [] = {
	{ NC_("Orientation", "Top"),    PANEL_ORIENTATION_TOP    },
	{ NC_("Orientation", "Bottom"), PANEL_ORIENTATION_BOTTOM },
	{ NC_("Orientation", "Left"),   PANEL_ORIENTATION_LEFT   },
	{ NC_("Orientation", "Right"),  PANEL_ORIENTATION_RIGHT  }
};

static void
panel_properties_size_spin_update_range (PanelPropertiesDialog *dialog)
{
	//TODO: we should also do this when the monitor size changes

	/* note: we might not be fully setup, so we have to do checks */
	if (!dialog->size_spin)
		return;

	gtk_spin_button_set_range (GTK_SPIN_BUTTON (dialog->size_spin),
				   panel_toplevel_get_minimum_size (dialog->toplevel),
				   panel_toplevel_get_maximum_size (dialog->toplevel));
}

/*************************\
 * Orientation combo box *
\*************************/

static void
panel_properties_dialog_orientation_update (PanelPropertiesDialog *dialog)
{
	PanelOrientation      orientation;
	GtkTreeModel         *model;
	GtkTreeIter           iter;
	OrientationComboItem *item;

	orientation = g_settings_get_enum (dialog->settings,
					   PANEL_TOPLEVEL_ORIENTATION_KEY);

	/* change the maximum size of the panel */
	panel_properties_size_spin_update_range (dialog);

	/* update the orientation combo box */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->orientation_combo));

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		gtk_tree_model_get (model, &iter, COLUMN_ITEM, &item, -1);
		if (item != NULL && item->orientation == orientation) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (dialog->orientation_combo),
						       &iter);
			return;
		}
	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
panel_properties_dialog_orientation_changed (PanelPropertiesDialog *dialog,
					     GtkComboBox           *combo_box)
{
	GtkTreeIter           iter;
	GtkTreeModel         *model;
	OrientationComboItem *item;

	if (!gtk_combo_box_get_active_iter (combo_box, &iter))
		return;

	model = gtk_combo_box_get_model (combo_box);
	gtk_tree_model_get (model, &iter, COLUMN_ITEM, &item, -1);
	if (item == NULL)
		return;

	g_settings_set_enum (dialog->settings,
			     PANEL_TOPLEVEL_ORIENTATION_KEY,
			     item->orientation);

	/* change the maximum size of the panel */
	panel_properties_size_spin_update_range (dialog);
}

static void
panel_properties_dialog_orientation_setting_changed (GSettings             *settings,
						     char                  *key,
						     PanelPropertiesDialog *dialog)
{
	panel_properties_dialog_orientation_update (dialog);
}

static void
panel_properties_dialog_setup_orientation_combo (PanelPropertiesDialog *dialog,
						 GtkBuilder            *gui)
{
	GtkWidget        *orientation_label;
	GtkListStore     *model;
	GtkTreeIter       iter;
	GtkCellRenderer  *renderer;
	int               i;

	dialog->orientation_combo = PANEL_GTK_BUILDER_GET (gui, "orientation_combo");
	g_return_if_fail (dialog->orientation_combo != NULL);
	orientation_label = PANEL_GTK_BUILDER_GET (gui, "orientation_label");
	g_return_if_fail (orientation_label != NULL);

	model = gtk_list_store_new (NUMBER_COLUMNS,
				    G_TYPE_STRING,
				    G_TYPE_POINTER);

	gtk_combo_box_set_model (GTK_COMBO_BOX (dialog->orientation_combo),
				 GTK_TREE_MODEL (model));

	for (i = 0; i < G_N_ELEMENTS (orientation_items); i++) {
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    COLUMN_TEXT, g_dpgettext2 (NULL, "Orientation", orientation_items [i].name),
				    COLUMN_ITEM, &(orientation_items [i]),
				    -1);
	}

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (dialog->orientation_combo),
				    renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (dialog->orientation_combo),
					renderer, "text", COLUMN_TEXT, NULL);

	panel_properties_dialog_orientation_update (dialog);

	g_signal_connect_swapped (dialog->orientation_combo, "changed",
				  G_CALLBACK (panel_properties_dialog_orientation_changed),
				  dialog);
	g_signal_connect (dialog->settings_background,
			  "changed::"PANEL_TOPLEVEL_ORIENTATION_KEY,
			  G_CALLBACK (panel_properties_dialog_orientation_setting_changed),
			  dialog);

	g_settings_bind_writable (dialog->settings, PANEL_TOPLEVEL_ORIENTATION_KEY,
				  orientation_label, "sensitive", FALSE);
	g_settings_bind_writable (dialog->settings, PANEL_TOPLEVEL_ORIENTATION_KEY,
				  dialog->orientation_combo, "sensitive", FALSE);

	if (!g_settings_is_writable (dialog->settings, PANEL_TOPLEVEL_ORIENTATION_KEY))
		gtk_widget_show (dialog->writability_warn_general);
}

/********************\
 * Size spin button *
\********************/

static void
panel_properties_dialog_size_spin_setup (PanelPropertiesDialog *dialog,
					 GtkBuilder            *gui)
{
	GtkWidget *size_label;
	GtkWidget *size_label_pixels;

	dialog->size_spin = PANEL_GTK_BUILDER_GET (gui, "size_spin");
	g_return_if_fail (dialog->size_spin != NULL);
	size_label = PANEL_GTK_BUILDER_GET (gui, "size_label");
	g_return_if_fail (size_label != NULL);
	size_label_pixels = PANEL_GTK_BUILDER_GET (gui, "size_label_pixels");
	g_return_if_fail (size_label_pixels != NULL);

	panel_properties_size_spin_update_range (dialog);

	g_settings_bind (dialog->settings, PANEL_TOPLEVEL_SIZE_KEY,
			 dialog->size_spin, "value",
			 G_SETTINGS_BIND_DEFAULT);

	g_settings_bind_writable (dialog->settings, PANEL_TOPLEVEL_SIZE_KEY,
				  size_label, "sensitive", FALSE);
	g_settings_bind_writable (dialog->settings, PANEL_TOPLEVEL_SIZE_KEY,
				  size_label_pixels, "sensitive", FALSE);

	if (!g_settings_is_writable (dialog->settings, PANEL_TOPLEVEL_SIZE_KEY))
		gtk_widget_show (dialog->writability_warn_general);
}

/******************\
 * Toggle buttons *
\******************/

static void
panel_properties_dialog_setup_toggle (PanelPropertiesDialog *dialog,
				      GtkBuilder            *gui,
				      const char            *toggle_name_in_ui,
				      const char            *settings_key,
				      gboolean               bind_sensitivity)
{
	GtkWidget          *toggle = PANEL_GTK_BUILDER_GET (gui, toggle_name_in_ui);
	GSettingsBindFlags  flags = G_SETTINGS_BIND_DEFAULT;

	if (!bind_sensitivity)
		flags |= G_SETTINGS_BIND_NO_SENSITIVITY;

	g_settings_bind (dialog->settings, settings_key,
			 toggle, "active",
			 flags);

	if (!g_settings_is_writable (dialog->settings, settings_key))
		gtk_widget_show (dialog->writability_warn_general);
}

static void
panel_properties_dialog_expand_toggle_setup (PanelPropertiesDialog *dialog,
					     GtkBuilder            *gui)
{
	panel_properties_dialog_setup_toggle (dialog, gui, "expand_toggle",
					      PANEL_TOPLEVEL_EXPAND_KEY,
					      TRUE);
}

static void
panel_properties_dialog_autohide_toggle_setup (PanelPropertiesDialog *dialog,
					       GtkBuilder            *gui)
{
	panel_properties_dialog_setup_toggle (dialog, gui, "autohide_toggle",
					      PANEL_TOPLEVEL_AUTO_HIDE_KEY,
					      TRUE);
}

static void
panel_properties_dialog_hidebuttons_toggle_setup (PanelPropertiesDialog *dialog,
						  GtkBuilder            *gui)
{
	panel_properties_dialog_setup_toggle (dialog, gui, "hidebuttons_toggle",
					      PANEL_TOPLEVEL_ENABLE_BUTTONS_KEY,
					      TRUE);

	dialog->hidebuttons_toggle = PANEL_GTK_BUILDER_GET (gui,
							    "hidebuttons_toggle");
}

static void
panel_properties_dialog_arrows_sensitivity_update (GSettings             *settings,
						   char                  *key,
						   PanelPropertiesDialog *dialog)
{
	gboolean sensitive;

	sensitive = (g_settings_get_boolean (settings,
					     PANEL_TOPLEVEL_ENABLE_BUTTONS_KEY) &&
		     g_settings_is_writable (settings,
					     PANEL_TOPLEVEL_ENABLE_ARROWS_KEY));

	gtk_widget_set_sensitive (dialog->arrows_toggle, sensitive);
}


static void
panel_properties_dialog_arrows_toggle_setup (PanelPropertiesDialog *dialog,
					     GtkBuilder            *gui)
{
	panel_properties_dialog_setup_toggle (dialog, gui, "arrows_toggle",
					      PANEL_TOPLEVEL_ENABLE_ARROWS_KEY,
					      FALSE);

	dialog->arrows_toggle = PANEL_GTK_BUILDER_GET (gui,
						       "arrows_toggle");

	panel_properties_dialog_arrows_sensitivity_update (dialog->settings,
							   NULL, dialog);

	g_signal_connect (dialog->settings,
			  "changed::"PANEL_TOPLEVEL_ENABLE_BUTTONS_KEY,
			  G_CALLBACK (panel_properties_dialog_arrows_sensitivity_update),
			  dialog);
	g_signal_connect (dialog->settings,
			  "writable-changed::"PANEL_TOPLEVEL_ENABLE_ARROWS_KEY,
			  G_CALLBACK (panel_properties_dialog_arrows_sensitivity_update),
			  dialog);
}

/********************\
 * Background image *
\********************/

static void
panel_properties_dialog_background_image_update (PanelPropertiesDialog *dialog)
{
	char *uri;

	uri = g_settings_get_string (dialog->settings_background,
				     PANEL_BACKGROUND_IMAGE_URI_KEY);

	if (PANEL_GLIB_STR_EMPTY (uri))
		gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (dialog->image_chooser));
	else
		gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (dialog->image_chooser),
					  uri);

	g_free (uri);
}

static void
panel_properties_dialog_image_chooser_changed (PanelPropertiesDialog *dialog)
{
	char *uri;

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog->image_chooser));
	if (!uri)
		uri = g_strdup ("");

	g_settings_set_string (dialog->settings_background,
			       PANEL_BACKGROUND_IMAGE_URI_KEY, uri);
	g_free (uri);
}

static void
panel_properties_dialog_image_chooser_setup (PanelPropertiesDialog *dialog,
					     GtkBuilder            *gui)
{
	dialog->image_chooser = PANEL_GTK_BUILDER_GET (gui, "image_chooser");
	panel_gtk_file_chooser_add_image_preview (GTK_FILE_CHOOSER (dialog->image_chooser));

	panel_properties_dialog_background_image_update (dialog);

	g_signal_connect_swapped (dialog->image_chooser, "file-set",
				  G_CALLBACK (panel_properties_dialog_image_chooser_changed),
				  dialog);

	g_settings_bind_writable (dialog->settings_background,
				  PANEL_BACKGROUND_IMAGE_URI_KEY,
				  dialog->image_chooser, "sensitive", FALSE);

	if (!g_settings_is_writable (dialog->settings_background,
				     PANEL_BACKGROUND_IMAGE_URI_KEY))
		gtk_widget_show (dialog->writability_warn_background);
}

/********************\
 * Background color *
\********************/

static void
panel_properties_dialog_background_color_get_rgba (PanelPropertiesDialog *dialog,
						   GdkRGBA               *color)
{
	char *color_str;

	color_str = g_settings_get_string (dialog->settings_background,
					   PANEL_BACKGROUND_COLOR_KEY);

	if (!gdk_rgba_parse (color, color_str))
		gdk_rgba_parse (color, PANEL_BACKGROUND_COLOR_DEFAULT);
	g_free (color_str);
}

static void
panel_properties_dialog_background_color_update_from_rgba (PanelPropertiesDialog *dialog,
							   GdkRGBA               *color)
{
	/* note: we might not be fully setup, so we have to do checks */
	if (dialog->opacity_scale)
		gtk_range_set_value (GTK_RANGE (dialog->opacity_scale),
				     color->alpha * 100.);
	if (dialog->color_button)
		gtk_color_button_set_rgba (GTK_COLOR_BUTTON (dialog->color_button),
					   color);
}

static void
panel_properties_dialog_background_color_set_from_rgba (PanelPropertiesDialog *dialog,
							GdkRGBA               *color)
{
	char *color_str;

	color_str = gdk_rgba_to_string (color);
	g_settings_set_string (dialog->settings_background,
			       PANEL_BACKGROUND_COLOR_KEY, color_str);
	g_free (color_str);

	/* make sure all widgets are consistent */
	panel_properties_dialog_background_color_update_from_rgba (dialog, color);
}

static void
panel_properties_dialog_background_color_update (PanelPropertiesDialog *dialog)
{
	GdkRGBA color;

	panel_properties_dialog_background_color_get_rgba (dialog, &color);
	panel_properties_dialog_background_color_update_from_rgba (dialog, &color);
}

static void
panel_properties_dialog_color_button_changed (PanelPropertiesDialog *dialog,
					      GtkColorButton        *color_button)
{
	GdkRGBA old_color;
	GdkRGBA new_color;

	panel_properties_dialog_background_color_get_rgba (dialog, &old_color);
	gtk_color_button_get_rgba (color_button, &new_color);
	new_color.alpha = old_color.alpha;
	panel_properties_dialog_background_color_set_from_rgba (dialog, &new_color);
}

static void
panel_properties_dialog_color_button_setup (PanelPropertiesDialog *dialog,
					    GtkBuilder            *gui)
{
	GtkWidget *color_label;

	dialog->color_button = PANEL_GTK_BUILDER_GET (gui, "color_button");
	g_return_if_fail (dialog->color_button != NULL);
	color_label = PANEL_GTK_BUILDER_GET (gui, "color_label");
	g_return_if_fail (color_label != NULL);

	panel_properties_dialog_background_color_update (dialog);

	g_signal_connect_swapped (dialog->color_button, "color_set",
				  G_CALLBACK (panel_properties_dialog_color_button_changed),
				  dialog);

	g_settings_bind_writable (dialog->settings_background,
				  PANEL_BACKGROUND_COLOR_KEY,
				  color_label, "sensitive", FALSE);
	g_settings_bind_writable (dialog->settings_background,
				  PANEL_BACKGROUND_COLOR_KEY,
				  dialog->color_button, "sensitive", FALSE);

	if (!g_settings_is_writable (dialog->settings_background,
				     PANEL_BACKGROUND_COLOR_KEY))
		gtk_widget_show (dialog->writability_warn_background);
}


static void
panel_properties_dialog_opacity_scale_changed (PanelPropertiesDialog *dialog)
{
	gdouble percentage;
        GdkRGBA color;

	percentage = gtk_range_get_value (GTK_RANGE (dialog->opacity_scale));

	if (percentage >= 98)
		percentage = 100;
	else if (percentage <= 2)
		percentage = 0;

	panel_properties_dialog_background_color_get_rgba (dialog, &color);
	color.alpha = (percentage / 100.);
	panel_properties_dialog_background_color_set_from_rgba (dialog, &color);
}

static void
panel_properties_dialog_opacity_scale_setup (PanelPropertiesDialog *dialog,
					     GtkBuilder            *gui)
{
	GtkWidget *opacity_label;
	GtkWidget *opacity_legend;

	dialog->opacity_scale = PANEL_GTK_BUILDER_GET (gui, "opacity_scale");
	g_return_if_fail (dialog->opacity_scale != NULL);
	opacity_label = PANEL_GTK_BUILDER_GET (gui, "opacity_label");
	g_return_if_fail (opacity_label != NULL);
	opacity_legend = PANEL_GTK_BUILDER_GET (gui, "opacity_legend");
	g_return_if_fail (opacity_legend != NULL);

	panel_properties_dialog_background_color_update (dialog);

	g_signal_connect_swapped (dialog->opacity_scale, "value_changed",
				  G_CALLBACK (panel_properties_dialog_opacity_scale_changed),
				  dialog);

	g_settings_bind_writable (dialog->settings_background,
				  PANEL_BACKGROUND_COLOR_KEY,
				  opacity_label, "sensitive", FALSE);
	g_settings_bind_writable (dialog->settings_background,
				  PANEL_BACKGROUND_COLOR_KEY,
				  opacity_legend, "sensitive", FALSE);
	g_settings_bind_writable (dialog->settings_background,
				  PANEL_BACKGROUND_COLOR_KEY,
				  dialog->opacity_scale, "sensitive", FALSE);

	if (!g_settings_is_writable (dialog->settings_background,
				     PANEL_BACKGROUND_COLOR_KEY))
		gtk_widget_show (dialog->writability_warn_background);
}

/*******************\
 * Background type *
\*******************/

static void
panel_properties_dialog_background_sensitivity_update (PanelPropertiesDialog *dialog,
						       PanelBackgroundType    background_type)
{
	gtk_widget_set_sensitive (dialog->color_widgets,
				  background_type == PANEL_BACK_COLOR);
	gtk_widget_set_sensitive (dialog->image_widgets,
				  background_type == PANEL_BACK_IMAGE);
}

static void
panel_properties_dialog_background_type_update (PanelPropertiesDialog *dialog)
{
	PanelBackgroundType  background_type;
	GtkWidget           *active_radio;

	background_type = g_settings_get_enum (dialog->settings_background,
					       PANEL_BACKGROUND_TYPE_KEY);

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

	panel_properties_dialog_background_sensitivity_update (dialog, background_type);
}

static void
panel_properties_dialog_background_radios_toggled (PanelPropertiesDialog *dialog,
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

	g_settings_set_enum (dialog->settings_background,
			     PANEL_BACKGROUND_TYPE_KEY,
			     background_type);

	panel_properties_dialog_background_sensitivity_update (dialog, background_type);
}
				
static void
panel_properties_dialog_background_radios_setup (PanelPropertiesDialog *dialog,
						 GtkBuilder            *gui)
{
	dialog->default_radio     = PANEL_GTK_BUILDER_GET (gui, "default_radio");
	dialog->color_radio       = PANEL_GTK_BUILDER_GET (gui, "color_radio");
	dialog->image_radio       = PANEL_GTK_BUILDER_GET (gui, "image_radio");
	dialog->color_widgets     = PANEL_GTK_BUILDER_GET (gui, "color_widgets");
	dialog->image_widgets     = PANEL_GTK_BUILDER_GET (gui, "image_widgets");

	panel_properties_dialog_background_type_update (dialog);

	g_signal_connect_swapped (dialog->default_radio, "toggled",
				  G_CALLBACK (panel_properties_dialog_background_radios_toggled),
				  dialog);
	g_signal_connect_swapped (dialog->color_radio, "toggled",
				  G_CALLBACK (panel_properties_dialog_background_radios_toggled),
				  dialog);
	g_signal_connect_swapped (dialog->image_radio, "toggled",
				  G_CALLBACK (panel_properties_dialog_background_radios_toggled),
				  dialog);

	g_settings_bind_writable (dialog->settings_background,
				  PANEL_BACKGROUND_TYPE_KEY,
				  dialog->default_radio, "sensitive", FALSE);
	g_settings_bind_writable (dialog->settings_background,
				  PANEL_BACKGROUND_TYPE_KEY,
				  dialog->color_radio, "sensitive", FALSE);
	g_settings_bind_writable (dialog->settings_background,
				  PANEL_BACKGROUND_TYPE_KEY,
				  dialog->image_radio, "sensitive", FALSE);

	if (!g_settings_is_writable (dialog->settings_background,
				     PANEL_BACKGROUND_TYPE_KEY))
		gtk_widget_show (dialog->writability_warn_background);
}

/*************************\
 * Update from GSettings *
\*************************/

static void
panel_properties_dialog_background_changed (GSettings             *settings,
					    char                  *key,
					    PanelPropertiesDialog *dialog)
{
	if (g_strcmp0 (key, PANEL_BACKGROUND_TYPE_KEY) == 0)
		panel_properties_dialog_background_type_update (dialog);
	else if (g_strcmp0 (key, PANEL_BACKGROUND_IMAGE_URI_KEY) == 0)
		panel_properties_dialog_background_image_update (dialog);
	else if (g_strcmp0 (key, PANEL_BACKGROUND_COLOR_KEY) == 0)
		panel_properties_dialog_background_color_update (dialog);
}

/******************************\
 * Code to make all this work *
\******************************/

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
				 "user-guide", "gospanel-28", NULL);
		break;
	default:
		break;
	}
}

static void
panel_properties_dialog_destroy (PanelPropertiesDialog *dialog)
{
	panel_toplevel_pop_autohide_disabler (PANEL_TOPLEVEL (dialog->toplevel));
	g_object_set_qdata (G_OBJECT (dialog->toplevel),
			    panel_properties_dialog_quark,
			    NULL);
}

static void
panel_properties_dialog_free (PanelPropertiesDialog *dialog)
{
	if (dialog->properties_dialog)
		gtk_widget_destroy (dialog->properties_dialog);
	dialog->properties_dialog = NULL;

	if (dialog->settings_background)
		g_object_unref (dialog->settings_background);
	dialog->settings_background = NULL;

	if (dialog->settings)
		g_object_unref (dialog->settings);
	dialog->settings = NULL;

	g_free (dialog);
}

static PanelPropertiesDialog *
panel_properties_dialog_new (PanelToplevel *toplevel,
			     GtkBuilder    *gui)
{
	PanelPropertiesDialog *dialog;
	char                  *toplevel_settings_path;

	dialog = g_new0 (PanelPropertiesDialog, 1);

	g_object_set_qdata_full (G_OBJECT (toplevel),
				 panel_properties_dialog_quark,
				 dialog,
				 (GDestroyNotify) panel_properties_dialog_free);

	dialog->toplevel = toplevel;

	g_object_get (toplevel, "settings-path", &toplevel_settings_path, NULL);
	dialog->settings = g_settings_new_with_path (PANEL_TOPLEVEL_SCHEMA,
						     toplevel_settings_path);
	dialog->settings_background = g_settings_get_child (dialog->settings,
							    PANEL_BACKGROUND_SCHEMA_CHILD);

	g_free (toplevel_settings_path);

	dialog->properties_dialog = PANEL_GTK_BUILDER_GET (gui, "panel_properties_dialog");
	g_signal_connect_swapped (dialog->properties_dialog, "response",
				  G_CALLBACK (panel_properties_dialog_response), dialog);
	g_signal_connect_swapped (dialog->properties_dialog, "destroy",
				  G_CALLBACK (panel_properties_dialog_destroy), dialog);

	gtk_window_set_screen (GTK_WINDOW (dialog->properties_dialog),
			       gtk_window_get_screen (GTK_WINDOW (toplevel)));

	dialog->writability_warn_general = PANEL_GTK_BUILDER_GET (gui, "writability_warn_general");
	dialog->writability_warn_background = PANEL_GTK_BUILDER_GET (gui, "writability_warn_background");

	panel_properties_dialog_setup_orientation_combo  (dialog, gui);
	panel_properties_dialog_size_spin_setup          (dialog, gui);
	panel_properties_dialog_expand_toggle_setup      (dialog, gui);
	panel_properties_dialog_autohide_toggle_setup    (dialog, gui);
	panel_properties_dialog_hidebuttons_toggle_setup (dialog, gui);
	panel_properties_dialog_arrows_toggle_setup      (dialog, gui);

	panel_properties_dialog_image_chooser_setup     (dialog, gui);
	panel_properties_dialog_color_button_setup      (dialog, gui);
	panel_properties_dialog_opacity_scale_setup     (dialog, gui);
	panel_properties_dialog_background_radios_setup (dialog, gui);

	g_signal_connect (dialog->settings_background, "changed",
			  G_CALLBACK (panel_properties_dialog_background_changed),
			  dialog);

	panel_toplevel_push_autohide_disabler (dialog->toplevel);
	panel_widget_register_open_dialog (panel_toplevel_get_panel_widget (dialog->toplevel),
					   dialog->properties_dialog);

	gtk_widget_show (dialog->properties_dialog);

	return dialog;
}

void
panel_properties_dialog_present (PanelToplevel *toplevel)
{
	PanelPropertiesDialog *dialog;
	GtkBuilder            *gui;
	GError                *error;

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

	gui = gtk_builder_new ();
	gtk_builder_set_translation_domain (gui, GETTEXT_PACKAGE);

	error = NULL;
	gtk_builder_add_from_file (gui,
				   BUILDERDIR "/panel-properties-dialog.ui",
				   &error);

        if (error) {
		char *secondary;

		secondary = g_strdup_printf (_("Unable to load file '%s': %s."),
					     BUILDERDIR"/panel-properties-dialog.ui",
					     error->message);
		panel_error_dialog (GTK_WINDOW (toplevel),
				    gtk_window_get_screen (GTK_WINDOW (toplevel)),
				    "cannot_display_properties_dialog", TRUE,
				    _("Could not display properties dialog"),
				    secondary);
		g_free (secondary);
		g_error_free (error);
		g_object_unref (gui);

		return;
	}

	dialog = panel_properties_dialog_new (toplevel, gui);

	g_object_unref (gui);
}
