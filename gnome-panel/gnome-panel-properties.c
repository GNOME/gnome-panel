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

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>
#include <libart_lgpl/art_filterlevel.h>
#include <libart_lgpl/art_alphagamma.h>

#include <gconf/gconf-client.h>

/*This array is the names of the checkboxes in the gnome-panel-properties.glade
  file. The names also correspond directly to the gconf-keys which they 
  represent.  If you add a new checkbox to the interface make sure its name is 
  the same as the gconf-key it is to edit and make sure you add its name to 
  this array.*/

gchar* checkboxes[] = {
	"drawer-auto-close",
	"auto-raise-panel",
	"confirm-panel-remove",
	"avoid-panel-overlap",
	"keep-menus-in-memory",
	"disable-animations",
	NULL
	};

/*
 * GEGL Wants Winners,
 * GEGL Wants Solutions,
 * GEGL Wants TV,
 * GEGL Wants Repeated Code...
 *
 * See rgb-stuff.c.
 */

void transform_pixbuf(guchar *dst, int x0, int y0, int x1, int y1, int drs,
                      GdkPixbuf *pixbuf, double affine[6],
                      int level, ArtAlphaGamma *ag);

void transform_pixbuf(guchar *dst, int x0, int y0, int x1, int y1, int drs,
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
/*
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
	gdk_window_remove_filter (GDK_ROOT_PARENT (),
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


	gdk_keyboard_grab (GDK_ROOT_PARENT(), FALSE, GDK_CURRENT_TIME);
	gdk_window_add_filter (GDK_ROOT_PARENT(), grab_key_filter, data);

	gtk_window_set_policy (GTK_WINDOW (grab_dialog), FALSE, FALSE, TRUE);
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
*/

static void
checkbox_clicked (GtkWidget *widget, gpointer data)
{
	gchar *key = (gchar*)data;
	
	gconf_client_set_bool(gconf_client_get_default(),key,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)),NULL);
}

static void
disable_animations_clicked (GtkWidget *widget, gpointer data)
{
	GtkWidget *vbox = GTK_WIDGET(data);
	int disable = GTK_TOGGLE_BUTTON(widget)->active;

	gtk_widget_set_sensitive(vbox,disable);
}

static void
animation_speed_changed (GtkWidget *widget, gpointer data)
{
	gconf_client_set_int(gconf_client_get_default(),
		"/apps/panel/global/panel-animation-speed",
		gtk_option_menu_get_history(GTK_OPTION_MENU(widget)),NULL);	
}

static void
hide_delay_changed (GtkWidget *widget, gpointer data)
{
	gconf_client_set_int(gconf_client_get_default(),
		"/apps/panel/global/panel-hide-delay",
		gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)),NULL);
}

static void
show_delay_changed (GtkWidget *widget, gpointer data)
{
	gconf_client_set_int(gconf_client_get_default(),
		"/apps/panel/global/panel-show-delay",
		gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)),NULL);
}

static void
load_booleans_for_checkboxes(GladeXML *gui, GConfClient *client)
{
	GtkWidget *checkbox;
	GtkWidget *anim_vbox;
	gchar *key;
	int i=0;

	while(checkboxes[i]!=NULL){
		checkbox= glade_xml_get_widget(gui,checkboxes[i]);
		key = g_strdup_printf("/apps/panel/global/%s",checkboxes[i]);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), 
			gconf_client_get_bool(client,key,NULL));
		g_signal_connect(G_OBJECT(checkbox),"clicked",
			G_CALLBACK(checkbox_clicked),key);
		i++;
	}
	/*if (key) {
		g_free(key);
		key = NULL;
	}*/

	checkbox = glade_xml_get_widget(gui,"disable-animations");
	anim_vbox =  glade_xml_get_widget(gui,"animation-vbox");
	gtk_widget_set_sensitive(anim_vbox,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox)));
	g_signal_connect(G_OBJECT(checkbox),"clicked",
                        G_CALLBACK(disable_animations_clicked),anim_vbox);
}

static void
load_config_into_gui(GladeXML *gui, GConfClient *client)
{
	
	GtkWidget *option, *hide_delay, *show_delay;

	load_booleans_for_checkboxes(gui,client);

	/* animation speed selection */
        option = glade_xml_get_widget(gui,"panel-animation-speed");
        gtk_option_menu_set_history(GTK_OPTION_MENU(option),
                gconf_client_get_int(client,
                "/apps/panel/global/panel-animation-speed",NULL));
        g_signal_connect(G_OBJECT(option),"changed",
                        G_CALLBACK(animation_speed_changed),NULL);

	/* hide delay */
	hide_delay = glade_xml_get_widget(gui,"panel-hide-delay");
	gtk_spin_button_configure(GTK_SPIN_BUTTON(hide_delay),
		GTK_ADJUSTMENT(gtk_adjustment_new(30.0,30.0,
		3000.0,10.0,10.0,0.0)),10.0,0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(hide_delay),
		(double)gconf_client_get_int(client,
		"/apps/panel/global/panel-hide-delay",NULL));
	g_signal_connect(G_OBJECT(hide_delay),"value-changed",
		G_CALLBACK(hide_delay_changed),NULL);

	/* show delay */	
	show_delay = glade_xml_get_widget(gui,"panel-show-delay");
	gtk_spin_button_configure(GTK_SPIN_BUTTON(show_delay),
		GTK_ADJUSTMENT(gtk_adjustment_new(0.0,0.0,
		3000.0,10.0,10.0,0.0)),10.0,0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(show_delay),
		(double)gconf_client_get_int(client,
		"/apps/panel/global/panel-show-delay",NULL));
	g_signal_connect(G_OBJECT(show_delay),"value-changed",
		G_CALLBACK(show_delay_changed),NULL);
	
}

static void
setup_the_ui(GtkWidget *main_window, GConfClient* client)
{
	GladeXML *gui;
	gchar *glade_file;
	GtkWidget *notebook;

	glade_file = GLADEDIR "/gnome-panel-properties.glade2";

	gui = glade_xml_new(glade_file, "main_notebook",NULL);
	if (!gui) {
		g_warning("Error loading `%s'",glade_file);
		return;
	}
	glade_xml_signal_autoconnect(gui);

	notebook=glade_xml_get_widget(gui,"main_notebook");

	g_signal_connect (G_OBJECT (notebook), "event",
                          G_CALLBACK (config_event),
                          notebook);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main_window)->vbox),notebook,
		TRUE,TRUE,0);

	load_config_into_gui(gui, client);
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
	GConfClient *gconf_client;

	bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	gnome_program_init("gnome-panel-properties",VERSION,
                LIBGNOMEUI_MODULE, argc, argv,
		GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);

  	main_window = gtk_dialog_new();

	gtk_dialog_add_button (GTK_DIALOG(main_window),
		GTK_STOCK_OK, GTK_RESPONSE_OK);

	 gtk_signal_connect(GTK_OBJECT(main_window), "response",
                     GTK_SIGNAL_FUNC(main_dialog_response), main_window);

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

		gtk_widget_set_usize(main_window,350,350);
	}
	else
	{
		setup_the_ui(main_window, gconf_client);
	}

	gtk_widget_show_all(main_window);

	gtk_main();

	return 0;
}
