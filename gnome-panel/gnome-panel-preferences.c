/*   gnome-panel-preferences: crapplet for global panel properties
 *
 *   Copyright (C) 1999 Free Software Foundation
 *   Copyright (C) 2000 Helix Code, Inc.
 *   Copyright (C) 2000 Eazel, Inc.
 *   Copyright (C) 2002 Sun Microsystems Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * Authors: George Lebl <jirka@5z.com>
 *          Jacob Berkman <jacob@helixcode.com>
 *          Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>
#include <string.h>

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>

#include "panel-gconf.h"
#include "panel-types.h"

#define GLADE_FILE   GLADEDIR "/gnome-panel-preferences.glade"

static GConfEnumStringPair global_properties_speed_type_enum_map [] = {
	{ PANEL_SPEED_SLOW,   "panel-speed-slow" },
	{ PANEL_SPEED_MEDIUM, "panel-speed-medium" },
	{ PANEL_SPEED_FAST,   "panel-speed-fast" },
};

/* Hooked up by glade */
void preferences_response (GtkWindow *window,
			   int        button,
			   gpointer   data);

static void
update_sensitive_for_checkbox (GladeXML *gui,
			       char     *key,
			       int       checked)
{
	GtkWidget *associate = NULL;

	if (!strcmp (key, "enable_animations"))
                associate = glade_xml_get_widget (gui, "panel_animation_hbox");

        if (associate)
                gtk_widget_set_sensitive (associate, checked);
}

static void
bool_value_changed_notify (GConfClient     *client,
			   guint            cnxn_id,
			   GConfEntry      *entry,
			   GtkToggleButton *toggle)
{
	gboolean value;

	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);

	if (gtk_toggle_button_get_active (toggle) != value) {
		char *basename;

		gtk_toggle_button_set_active (toggle, value);

		basename = g_path_get_basename (gconf_entry_get_key (entry));
		update_sensitive_for_checkbox (
			g_object_get_data (G_OBJECT (toggle), "glade-xml"),
			basename, value);
		g_free (basename);
	}
}

static void
checkbox_clicked (GtkWidget *widget,
		  char      *key)
{
	GConfClient *client;
	int          checked;

	client = gconf_client_get_default ();

	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	gconf_client_set_bool (client,
			       panel_gconf_global_key (key),
			       checked, NULL);

	update_sensitive_for_checkbox (
			g_object_get_data (G_OBJECT (widget), "glade-xml"),
			key, checked);

	g_object_unref (client);
}

static void
load_checkboxes (GladeXML    *gui,
		 GConfClient *client)
{
	static char *checkboxes [] = {
		"drawer_autoclose",
        	"enable_animations",
		NULL
	};
	int i;

	for (i = 0; checkboxes [i]; i++) {
		GtkWidget  *checkbox;
		const char *key;
		int         checked;

		checkbox = glade_xml_get_widget (gui, checkboxes [i]);
		g_object_set_data (G_OBJECT (checkbox), "glade-xml", gui);

		key = panel_gconf_global_key (checkboxes [i]);

		gconf_client_notify_add (
			client, key,
			(GConfClientNotifyFunc) bool_value_changed_notify,
			checkbox, NULL, NULL);

		checked = gconf_client_get_bool (client, key, NULL);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbox), checked);

		g_signal_connect (checkbox, "clicked",
				  G_CALLBACK (checkbox_clicked), checkboxes [i]);
		update_sensitive_for_checkbox (gui, checkboxes [i], checked);
	}
}

static void
animation_menu_changed (GtkWidget *widget)
{
	GConfClient *client;

	client = gconf_client_get_default ();

	gconf_client_set_string (client,
				 panel_gconf_global_key ("panel_animation_speed"),
				 gconf_enum_to_string (global_properties_speed_type_enum_map,
						       gtk_option_menu_get_history (GTK_OPTION_MENU (widget))),
				 NULL);	

	g_object_unref (client);
}

static void
enum_value_changed_notify (GConfClient   *client,
			   guint          cnxn_id,
			   GConfEntry    *entry,
			   GtkOptionMenu *option)
{
	const char *value;
	int         speed = PANEL_SPEED_SLOW;

	if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
		return;

	value = gconf_value_get_string (entry->value);
	 
	gconf_string_to_enum (
		global_properties_speed_type_enum_map, value, &speed);

	if (gtk_option_menu_get_history (GTK_OPTION_MENU (option)) != speed)
		gtk_option_menu_set_history (GTK_OPTION_MENU (option), speed);
}

static void
load_animation_menu (GladeXML    *gui,
		     GConfClient *client)
{
	GtkWidget  *option;
	char       *tmpstr;
	int         speed = PANEL_SPEED_SLOW;
	const char *key;

	option = glade_xml_get_widget (gui, "panel_animation_speed");

	key = panel_gconf_global_key ("panel_animation_speed");

	gconf_client_notify_add (
		client, key,
		(GConfClientNotifyFunc) enum_value_changed_notify,
		option, NULL, NULL);
		
	tmpstr = gconf_client_get_string (client, key, NULL);
	gconf_string_to_enum (
		global_properties_speed_type_enum_map, tmpstr, &speed);
	g_free (tmpstr);

	gtk_option_menu_set_history (GTK_OPTION_MENU (option), speed);

	g_signal_connect (option, "changed",
			  G_CALLBACK (animation_menu_changed), NULL);
}

static void
load_config_into_gui (GladeXML *gui)
{
	GConfClient *client;
	GError      *error = NULL;

	client = gconf_client_get_default ();

	gconf_client_add_dir (client, "/apps/panel/global",
			      GCONF_CLIENT_PRELOAD_NONE, &error);
	if (error) {
		g_warning ("Failed to monitor '/apps/panel/global': %s", error->message);
		g_error_free (error);
	}

	load_checkboxes (gui, client);
	load_animation_menu (gui, client);

	g_object_unref (client);
}

void
preferences_response (GtkWindow *window,
		      int        button,
		      gpointer   data)
{
	GError *error = NULL;

	switch (button) {
		case GTK_RESPONSE_CLOSE:
			gtk_main_quit ();
			break;

		case GTK_RESPONSE_HELP:
			gnome_help_display_desktop (NULL, "user-guide",
				"wgoscustlookandfeel.xml", "goscustdesk-10", &error);
			if (error) {
				GtkWidget *dialog;

				dialog = gtk_message_dialog_new (window,
								 GTK_DIALOG_DESTROY_WITH_PARENT,
								 GTK_MESSAGE_ERROR,
								 GTK_BUTTONS_CLOSE,
								 _("There was an error displaying help: \n%s"),
								 error->message);

				g_signal_connect (G_OBJECT (dialog), "response",
						  G_CALLBACK (gtk_widget_destroy),
						  NULL);

				gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
				gtk_widget_show (dialog);
				g_error_free (error);
			}

			break;

		default:
			break;
	}
}

static void
error_dialog (const char *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (NULL, 0,
					 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
					 message);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (gtk_main_quit), NULL);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_widget_show (dialog);

}

static gboolean
check_lockdown (void)
{
	GConfClient *client;
	gboolean     locked_down;

	client = gconf_client_get_default ();

	locked_down = gconf_client_get_bool (
				client, panel_gconf_global_key ("lock-down"), NULL);
	if (locked_down)
		error_dialog (_("The system administrator has disallowed\n"
			        "modification of the panel configuration"));

	g_object_unref (client);

	return locked_down;
}

int
main (int argc, char **argv)
{
	GladeXML *gui = NULL;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-panel-preferences", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);

	if (!check_lockdown ()) {
		GtkWidget *dialog;
		char      *panel_icon;

		gui = glade_xml_new (
				GLADE_FILE, "gnome_panel_preferences_dialog", NULL);
		if (!gui) {
			char *error;

			error = g_strdup_printf (_("Error loading glade file %s"), GLADE_FILE);
			error_dialog (error);
			g_free (error);
		
			gtk_main ();
		}

		glade_xml_signal_autoconnect (gui);

		dialog = glade_xml_get_widget (
				gui, "gnome_panel_preferences_dialog");

		load_config_into_gui (gui);

		panel_icon = gnome_program_locate_file (
					NULL, GNOME_FILE_DOMAIN_PIXMAP,
					"gnome-panel.png", TRUE, NULL);
		if (panel_icon) {
			gnome_window_icon_set_from_file (
				GTK_WINDOW (dialog), panel_icon);
			g_free (panel_icon);
		}

		gtk_widget_show (dialog);
	}

	gtk_main ();

	if (gui)
		g_object_unref (gui);

	return 0;
}
