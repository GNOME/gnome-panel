/*   gnome-panel-properties: crapplet for global panel properties
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
#include "global-keys.h"
#include "panel-util.h"

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>
#include <libart_lgpl/art_filterlevel.h>
#include <libart_lgpl/art_alphagamma.h>

#include <gconf/gconf-client.h>

#define GNOME_PANEL_PROPERTIES_DEBUG 1

/* Just so we can link with panel-util.c for the convert keys stuff*/
GSList *applets;

/* Ugly globals to help reduce code size */
GladeXML *glade_gui;
GConfClient *gconf_client;

static GConfEnumStringPair global_properties_type_enum_map [] = {
	{ PANEL_SPEED_SLOW,   "panel-speed-slow" },
	{ PANEL_SPEED_MEDIUM, "panel-speed-medium" },
	{ PANEL_SPEED_FAST,   "panel-speed-fast" },	
	{ LAYER_NORMAL, "panel-normal-layer" },
	{ LAYER_BELOW, "panel-below-layer" },
	{ LAYER_ABOVE, "panel-above-layer" },
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

static GtkWidget *grab_dialog;

static gboolean 
is_modifier (guint keycode)
{
	gint i;
	gint map_size;
	XModifierKeymap *mod_keymap;
	gboolean retval = FALSE;

	mod_keymap = XGetModifierMapping (gdk_display);

	map_size = 8 * mod_keymap->max_keypermod;
	i = 0;
	while (i < map_size) {
		
		if (keycode == mod_keymap->modifiermap[i]) {
			retval = TRUE;
			break;
		}
		++i;
	}

	XFreeModifiermap (mod_keymap);

	return retval;
}

static GdkFilterReturn
grab_key_filter (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	XEvent *xevent = (XEvent *)gdk_xevent;
	GtkEntry *entry;
	char *key;
	guint keycode, state;
	char buf[10];
	KeySym keysym;

	if (xevent->type != KeyPress && xevent->type != KeyRelease)
		return GDK_FILTER_CONTINUE;
	
	entry = GTK_ENTRY (data);

	keycode = xevent->xkey.keycode;

	if (is_modifier (keycode))
		return GDK_FILTER_CONTINUE;

	state = xevent->xkey.state & USED_MODS;

	XLookupString (&xevent->xkey, buf, 0, &keysym, NULL);
  
	key = convert_keysym_state_to_string (keysym,
					      state);

	gtk_entry_set_text (entry, key != NULL ? key : "");
	g_free (key);

	gdk_keyboard_ungrab (GDK_CURRENT_TIME);
	gtk_widget_destroy (grab_dialog);
	gdk_window_remove_filter (gdk_get_default_root_window (),
				  grab_key_filter, data);

	return GDK_FILTER_REMOVE;
}

static void
grab_button_pressed (GtkButton *button, gpointer data)
{
	GtkWidget *frame;
	GtkWidget *box;
	GtkWidget *label;
	grab_dialog = gtk_window_new (GTK_WINDOW_POPUP);

	gdk_keyboard_grab (gdk_get_default_root_window (), FALSE, GDK_CURRENT_TIME);
	gdk_window_add_filter (gdk_get_default_root_window (), grab_key_filter, data);

	g_object_set (G_OBJECT (grab_dialog),
		      "allow_grow", FALSE,
		      "allow_shrink", FALSE,
		      "resizable", FALSE,
		      NULL);
	gtk_window_set_position (GTK_WINDOW (grab_dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (grab_dialog), TRUE);

	frame = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (grab_dialog), frame);

	box = gtk_hbox_new (0, 0);
	gtk_container_set_border_width (GTK_CONTAINER (box), 20);
	gtk_container_add (GTK_CONTAINER (frame), box);

	label = gtk_label_new (_("Press a key..."));
	gtk_container_add (GTK_CONTAINER (box), label);
	
	gtk_widget_show_all (grab_dialog);
	return;
}

static void
update_sensitive_for_checkbox(gchar *key, int checked)
{
	GtkWidget *associate = NULL;

	if (strcmp(key,"/apps/panel/global/enable-animations") == 0)
                associate = glade_xml_get_widget(glade_gui,"animation-vbox");
        if (strcmp(key,"/apps/panel/global/enable-key-bindings") == 0)
                associate = glade_xml_get_widget(glade_gui,"kb-table");
        if (associate !=NULL)
                gtk_widget_set_sensitive(associate,checked);
}

static void
checkbox_clicked (GtkWidget *widget, gpointer data)
{
	int checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gchar *key = (gchar*)data;

	gconf_client_set_bool(gconf_client,key,checked,NULL);
	update_sensitive_for_checkbox(key, checked);
}

static void
option_menu_changed (GtkWidget *widget, gpointer data)
{
	gchar *key = (gchar *)data;

	gconf_client_set_string (gconf_client,key,
				 gconf_enum_to_string (global_properties_type_enum_map,
						       gtk_option_menu_get_history (GTK_OPTION_MENU (widget)) ),
				 NULL);	
}

static void
entry_changed (GtkWidget *widget, gpointer data)
{
        gchar *key = (gchar *)data;

        gconf_client_set_string(gconf_client,key,
                gtk_entry_get_text(GTK_ENTRY(widget)),NULL);
}

static void
load_checkboxes (void)
{
	gchar* checkboxes[] = {"drawer-autoclose", "auto-raise-panel",
        	"enable-animations", "enable-key-bindings",
		"auto-update-menus", NULL };
	int i = 0;

	while(checkboxes[i]!=NULL){
		GtkWidget *checkbox;
		gchar *key;
		int checked;
		checkbox= glade_xml_get_widget(glade_gui,checkboxes[i]);
		key = g_strdup_printf("/apps/panel/global/%s",checkboxes[i]);
		checked = gconf_client_get_bool(gconf_client,key,NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), 
			checked);
		g_signal_connect_data(G_OBJECT(checkbox),"clicked",
			G_CALLBACK(checkbox_clicked), key,
			(GClosureNotify)g_free, 
			0 /* connect_flags */);
		update_sensitive_for_checkbox(key, checked);
		i++;
	}
}

static void
load_option_menus (void)
{
	gchar *optionmenus[] = {"panel-animation-speed", "panel-window-layer",
				 NULL };
	int i = 0;

	while(optionmenus[i]!=NULL){
		GtkWidget *option;
		gchar *key;
		gint retval;

        	option = glade_xml_get_widget(glade_gui,optionmenus[i]);
        	key = g_strdup_printf("/apps/panel/global/%s",optionmenus[i]);
		gconf_string_to_enum (global_properties_type_enum_map,
				      gconf_client_get_string (gconf_client, key, NULL),
				      &retval);

        	gtk_option_menu_set_history(GTK_OPTION_MENU(option),
					    retval);

        	g_signal_connect_data (G_OBJECT (option), "changed",
				       G_CALLBACK (option_menu_changed),
				       key,
				       (GClosureNotify)g_free,
				       0 /* connect_flags */);
		i++;
	}
}

static void
load_key_bindings()
{
        gchar *entries[] = {"menu-key", "run-key", NULL };
        int i = 0;

        while(entries[i]!=NULL){
                GtkWidget *button, *entry;
		gchar *button_name, *key;

                entry = glade_xml_get_widget(glade_gui,entries[i]);
		key = g_strdup_printf("/apps/panel/global/%s",entries[i]);
		gtk_entry_set_text(GTK_ENTRY(entry),
			gconf_client_get_string(gconf_client, key, NULL));
		button_name = g_strdup_printf("grab-%s", entries[i]);
		button = glade_xml_get_widget(glade_gui, button_name);
                g_signal_connect(G_OBJECT(button),"clicked",
                        G_CALLBACK(grab_button_pressed), entry);
        	g_signal_connect_data (G_OBJECT (entry), "changed",
				       G_CALLBACK (entry_changed),
				       key,
				       (GClosureNotify)g_free,
				       0 /* connect_flags */);
                i++;
                g_free (button_name);
        }
}

static void
load_config_into_gui()
{
	load_checkboxes();
	load_option_menus();
	load_key_bindings();
}

static void
setup_the_ui(GtkWidget *main_window)
{
	gchar *glade_file;
	GtkWidget *notebook;

	glade_file = GLADEDIR "/gnome-panel-properties.glade2";

	glade_gui = glade_xml_new(glade_file, "main_notebook",NULL);
	if (!glade_gui) {
		g_warning("Error loading `%s'",glade_file);
		return;
	}
	glade_xml_signal_autoconnect(glade_gui);

	notebook=glade_xml_get_widget(glade_gui,"main_notebook");

	g_signal_connect (G_OBJECT (notebook), "event",
                          G_CALLBACK (config_event),
                          notebook);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main_window)->vbox),notebook,
		TRUE,TRUE,0);

	load_config_into_gui();
}

static void
main_dialog_response(GtkWindow *window, int button, gpointer data)
{
	switch (button) {
		case GTK_RESPONSE_OK:
			gtk_main_quit();
			break;
		default:
			break;
	}
}

int
main (int argc, char **argv)
{
  	GtkWidget *main_window;

	bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	gnome_program_init("gnome-panel-properties",VERSION,
                LIBGNOMEUI_MODULE, argc, argv,
		GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);

  	main_window = gtk_dialog_new();

	gtk_dialog_add_button (GTK_DIALOG(main_window),
		GTK_STOCK_OK, GTK_RESPONSE_OK);

	g_signal_connect (G_OBJECT(main_window), "response",
			  G_CALLBACK (main_dialog_response),
			  main_window);

	g_signal_connect (G_OBJECT (main_window), "destroy",
			  G_CALLBACK (gtk_main_quit),
			  NULL);

	gconf_client = gconf_client_get_default();

	/* Ahhh, yes the infamous commie mode, don't allow running of this,
	 * just display a label */

	if(gconf_client_get_bool(gconf_client,
		"/apps/panel/global/lock-down",NULL)) 
	{
		GtkWidget *label;

		label = gtk_label_new (_("The system administrator has "
					 "disallowed\n modification of the "
					 "panel configuration"));
		gtk_box_pack_start (GTK_BOX(GTK_DIALOG(main_window)->vbox),
			label,TRUE,TRUE,0);

		gtk_widget_set_size_request (main_window, 350, 350);
	}
	else
	{
		setup_the_ui(main_window);
	}

	gtk_window_set_title(GTK_WINDOW(main_window),
		_("Panel Global Properties"));

	gtk_widget_show_all(main_window);

	gtk_main();

	return 0;
}
