/*   gnome-panel-preferences: crapplet for global panel properties
 *
 *   Copyright (C) 1999 Free Software Foundation
 *   Copyright 2000 Helix Code, Inc.
 *   Copyright 2000 Eazel, Inc.
 *   Authors: George Lebl <jirka@5z.com>
 *            Jacob Berkman <jacob@helixcode.com>
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
 */

#include <config.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <gdk/gdkx.h>
#include "panel-gconf.h"
#include "global-keys.h"
#include "panel-util.h"

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>
#include <libart_lgpl/art_filterlevel.h>
#include <libart_lgpl/art_alphagamma.h>

#include <gconf/gconf-client.h>

/* Just so we can link with panel-util.c for the convert keys stuff*/
GSList *applets;

/* Ugly globals to help reduce code size */
GladeXML *glade_gui;
GConfClient *gconf_client;

static GConfEnumStringPair global_properties_speed_type_enum_map [] = {
	{ PANEL_SPEED_SLOW,   "panel-speed-slow" },
	{ PANEL_SPEED_MEDIUM, "panel-speed-medium" },
	{ PANEL_SPEED_FAST,   "panel-speed-fast" },
};

/*
 * GEGL Wants Winners,
 * GEGL Wants Solutions,
 * GEGL Wants TV,
 * GEGL Wants Repeated Code...
 *
 * See rgb-stuff.c.
 */

static void
transform_pixbuf(guchar *dst, int x0, int y0, int x1, int y1, int drs,
		 GdkPixbuf *pixbuf, double affine[6],
		 int level, ArtAlphaGamma *ag)
{
        gint w, h, rs;

        rs = gdk_pixbuf_get_rowstride(pixbuf);
        h =  gdk_pixbuf_get_height(pixbuf);
        w =  gdk_pixbuf_get_width(pixbuf);

        if (gdk_pixbuf_get_has_alpha(pixbuf)) {
                art_rgb_rgba_affine(dst, x0, y0, x1, y1, drs,
                                    gdk_pixbuf_get_pixels(pixbuf),
                                    w, h, rs, affine, level, ag);
        } else {
                art_rgb_affine(dst, x0, y0, x1, y1, drs,
                               gdk_pixbuf_get_pixels(pixbuf),
                               w, h, rs, affine, level, ag);
        }
}

#include "nothing.cP"

static void
update_sensitive_for_checkbox (char *key,
			       int   checked)
{
	GtkWidget *associate = NULL;

	if (strcmp (key, "enable_animations") == 0)
                associate = glade_xml_get_widget (glade_gui, "animation-vbox");

	else if (strcmp (key, "enable_key_bindings") == 0)
                associate = glade_xml_get_widget (glade_gui,"kb-table");

        if (associate)
                gtk_widget_set_sensitive (associate, checked);
}

static void
checkbox_clicked (GtkWidget *widget,
		  char      *key)
{
	const char *full_key;
	int         checked;

	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	full_key = panel_gconf_global_key (key);

	gconf_client_set_bool (gconf_client, full_key, checked, NULL);

	update_sensitive_for_checkbox (key, checked);
}

static void
option_menu_changed (GtkWidget *widget,
		     char      *key)
{
	const char *full_key;

	full_key = panel_gconf_global_key (key);

	if (strcmp (key, "panel_animation_speed") == 0)
		gconf_client_set_string (gconf_client, full_key,
				         gconf_enum_to_string (global_properties_speed_type_enum_map,
			       		 		       gtk_option_menu_get_history (GTK_OPTION_MENU (widget)) ),
				 	 NULL);	
}

static void
load_checkboxes (void)
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

		checkbox= glade_xml_get_widget (glade_gui, checkboxes [i]);

		key = panel_gconf_global_key (checkboxes [i]);

		checked = gconf_client_get_bool (gconf_client, key, NULL);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbox), checked);

		g_signal_connect (checkbox, "clicked",
				  G_CALLBACK (checkbox_clicked), checkboxes [i]);
		update_sensitive_for_checkbox (checkboxes [i], checked);
	}
}

static void
load_option_menus (void)
{
	char *optionmenus[] = {
		"panel_animation_speed",
		NULL
	};
	int i;

	for (i = 0; optionmenus [i]; i++) {
		GtkWidget  *option;
		const char *key;
		int         retval = 0;

        	option = glade_xml_get_widget (glade_gui, optionmenus [i]);
        	key = panel_gconf_global_key (optionmenus [i]);
		
		if (strcmp (optionmenus[i], "panel_animation_speed") == 0)
			gconf_string_to_enum (global_properties_speed_type_enum_map,
			      		      gconf_client_get_string (gconf_client, key, NULL),
			                      &retval);

        	gtk_option_menu_set_history (GTK_OPTION_MENU (option), retval);

        	g_signal_connect (option, "changed", G_CALLBACK (option_menu_changed), optionmenus [i]);
	}
}

static void
load_config_into_gui (void)
{
	load_checkboxes ();
	load_option_menus ();
}

static void
setup_the_ui(GtkWidget *main_window)
{
	gchar *glade_file;
	GtkWidget *widget;
	gchar *icon_name;

	glade_file = GLADEDIR "/gnome-panel-preferences.glade";

	glade_gui = glade_xml_new(glade_file, "main_notebook",NULL);
	if (!glade_gui) {
		g_warning("Error loading %s",glade_file);
		return;
	}
	glade_xml_signal_autoconnect(glade_gui);

	widget = glade_xml_get_widget(glade_gui,"main_notebook");

	g_signal_connect (G_OBJECT (widget), "event",
                          G_CALLBACK (config_event),
                          widget);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main_window)->vbox),widget,
			   TRUE,TRUE,0);
#if 0
	icon_name = gnome_program_locate_file (NULL,
					       GNOME_FILE_DOMAIN_PIXMAP,
					       "gnome-panel.png",
					       TRUE,
					       NULL);
	/* FIXME: I really don't like the icon.  If we can get another one, we
	 * can put this back. */
	g_free (icon_name);
#endif
	icon_name = NULL;
		
	if (icon_name == NULL) {
		widget = glade_xml_get_widget(glade_gui, "icon_vbox");
		gtk_widget_hide (widget);
	} else {
		widget = glade_xml_get_widget(glade_gui, "panel_icon");
		gtk_image_set_from_file (GTK_IMAGE (widget), icon_name);
		g_free (icon_name);
	}
	load_config_into_gui();
}

static void
main_dialog_response(GtkWindow *window, int button, gpointer data)
{
	GError *error = NULL;
	switch (button) {
		case GTK_RESPONSE_CLOSE:
			gtk_main_quit();
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
								 ("There was an error displaying help: \n%s"),
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

int
main (int argc, char **argv)
{
  	GtkWidget  *main_window;
	const char *key;
	char       *panel_icon;

	bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	gnome_program_init("gnome-panel-preferences",VERSION,
                LIBGNOMEUI_MODULE, argc, argv,
		GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);

  	main_window = gtk_dialog_new();
	g_object_set (G_OBJECT (main_window), "has-separator", FALSE, NULL);
	gtk_dialog_add_buttons (GTK_DIALOG(main_window),
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		GTK_STOCK_HELP, GTK_RESPONSE_HELP,NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (main_window), GTK_RESPONSE_CLOSE);
	g_signal_connect (G_OBJECT(main_window), "response",
			  G_CALLBACK (main_dialog_response),
			  main_window);

	g_signal_connect (G_OBJECT (main_window), "destroy",
			  G_CALLBACK (gtk_main_quit),
			  NULL);

	gconf_client = gconf_client_get_default();

	/* Ahhh, yes the infamous commie mode, don't allow running of this,
	 * just display a label */

	key = panel_gconf_global_key ("lock-down");

	if(gconf_client_get_bool (gconf_client, key, NULL)) {
		GtkWidget *label;

		label = gtk_label_new (_("The system administrator has "
					 "disallowed\n modification of the "
					 "panel configuration"));
		gtk_box_pack_start (GTK_BOX(GTK_DIALOG(main_window)->vbox),
			label,TRUE,TRUE,0);

		gtk_widget_set_size_request (main_window, 350, 350);
	} else
		setup_the_ui(main_window);

	gtk_window_set_title (
		GTK_WINDOW (main_window), _("Panel Preferences"));

	panel_icon = gnome_program_locate_file (
			NULL, GNOME_FILE_DOMAIN_PIXMAP, "gnome-panel.png", TRUE, NULL);
	if (panel_icon) {
		gnome_window_icon_set_from_file (GTK_WINDOW (main_window), panel_icon);
		g_free (panel_icon);
	}

	gtk_widget_show(main_window);

	gtk_main();

	return 0;
}
