/* fish.c:
 *
 * Copyright (C) 1998-2002 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *      George Lebl  <jirka@5z.com>
 *      Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>
#include <string.h>

#include <glib-object.h>
#include <gtk/gtk.h>
#include <panel-applet.h>
#include <panel-applet-gconf.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/libgnomeui.h>
#include <libart_lgpl/libart.h>
#include <glade/glade-xml.h>

#include "egg-screen-help.h"

#define FISH_APPLET(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), \
			fish_applet_get_type(),          \
			FishApplet))
#define FISH_IS_APPLET(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
			   FISH_TYPE_APPLET))

#define N_FISH_PREFS 6

typedef struct {
	PanelApplet        applet;

	GConfClient       *client;

	char              *name;
	char              *image;
	char              *command;
	int                n_frames;
	gdouble            speed;
	gboolean           rotate;

	PanelAppletOrient  orientation;

	GtkWidget         *frame;
	GtkWidget         *drawing_area;
	GdkRectangle       prev_allocation;
	GdkPixmap         *pixmap;
	guint              timeout;
	int                current_frame;

	GdkPixbuf         *pixbuf;

	GtkWidget         *about_dialog;

	GtkWidget         *preferences_dialog;
	GtkWidget         *name_entry;
	GtkWidget         *pixmap_entry;
	GtkWidget         *image_entry;
	GtkWidget         *command_entry;
	GtkWidget         *frames_spin;
	GtkWidget         *speed_spin;
	GtkWidget         *rotate_toggle;

	GtkWidget         *fortune_dialog;
	GtkWidget         *fortune_view;
	GtkWidget         *fortune_label;
	GtkTextBuffer	  *fortune_buffer;

	gboolean           april_fools;

	guint              listeners [N_FISH_PREFS];
} FishApplet;

typedef struct {
	PanelAppletClass klass;
} FishAppletClass;


static gboolean load_fish_image          (FishApplet *fish);
static void     update_pixmap            (FishApplet *fish);
static void     something_fishy_going_on (FishApplet *fish,
					  const char *message);

static GType fish_applet_get_type (void);

static GObjectClass *parent_class;

static int fools_day        = 0;
static int fools_month      = 0;
static int fools_hour_start = 0;
static int fools_hour_end   = 0;

static void
show_help (FishApplet *fish, const char *link_id)
{
	GError *error = NULL;

	egg_help_display_desktop_on_screen (
		NULL, "fish-applet-2", "fish-applet-2", link_id,
		gtk_widget_get_screen (GTK_WIDGET (fish)),
		&error);

	if (error) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (
				NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("There was an error displaying help: %s"),
				error->message);

		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_widget_get_screen (GTK_WIDGET (fish)));
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

static void
name_value_changed (GtkEntry   *entry,
		    FishApplet *fish)
{
	const char *text;

	text = gtk_entry_get_text (entry);

	if (!text || !text [0])
		return;

	panel_applet_gconf_set_string (
		PANEL_APPLET (fish), "name", text, NULL);
}

static void
image_value_changed (GtkEntry   *entry,
		     FishApplet *fish)
{	char *path;

	path = gnome_file_entry_get_full_path (
			GNOME_FILE_ENTRY (fish->pixmap_entry), TRUE);
	if (!path || !path [0]) {
		g_free (path);
		return;
	}

	panel_applet_gconf_set_string (PANEL_APPLET (fish), "image", path, NULL);

	g_free (path);
}

static void
command_value_changed (GtkEntry   *entry,
		       FishApplet *fish)
{
	const char *text;

	text = gtk_entry_get_text (entry);

	if (!text || !text [0])
		return;

	if (!strncmp (text, "ps ", 3)  ||
	    !strcmp  (text, "ps")      ||
	    !strncmp (text, "who ", 4) ||
	    !strcmp  (text, "who")     ||
	    !strcmp  (text, "uptime")  ||
	    !strncmp (text, "tail ", 5)) {
		char       *message;
		const char *warning_format =
				_("Warning:  The command "
				  "appears to be something actually useful.\n"
				   "Since this is a useless applet, you "
				   "may not want to do this.\n"
				   "We strongly advise you against "
				   "using %s for anything\n"
				   "which would make the applet "
				   "\"practical\" or useful.");

		message = g_strdup_printf (warning_format, fish->name);

		something_fishy_going_on (fish, message);

		g_free (message);

		return;
	}

	panel_applet_gconf_set_string (
		PANEL_APPLET (fish), "command", text, NULL);
}

static void
n_frames_value_changed (GtkSpinButton *button,
			FishApplet    *fish)
{
        panel_applet_gconf_set_int (
			PANEL_APPLET (fish), "frames",
			gtk_spin_button_get_value_as_int (button), NULL);
}

static void
speed_value_changed (GtkSpinButton *button,
		     FishApplet    *fish)
{
        panel_applet_gconf_set_float (
			PANEL_APPLET (fish), "speed",
			gtk_spin_button_get_value (button), NULL);
}

static void
rotate_value_changed (GtkToggleButton *toggle,
		      FishApplet      *fish)
{
	panel_applet_gconf_set_bool (
			PANEL_APPLET (fish), "rotate",
			gtk_toggle_button_get_active (toggle), NULL);
}

static gboolean
delete_event (GtkWidget  *widget,
	      FishApplet *fish)
{
	gtk_widget_hide (widget);
}

static gboolean
close_dialog (GtkWidget  *widget,
	      FishApplet *fish)
{
	gtk_widget_hide (fish->preferences_dialog);
}

static void
handle_response (GtkWidget  *widget,
		 int         id,
		 FishApplet *fish)
{
	if (id == GTK_RESPONSE_HELP) {
		show_help (fish, "fish-settings");
		return;
	}

	gtk_widget_hide (fish->preferences_dialog);
}

static void 
display_preferences_dialog (BonoboUIComponent *uic,
			    FishApplet        *fish,
			    const char        *verbname)
{
	GladeXML  *xml;
	GtkWidget *button;

	if (fish->preferences_dialog) {
		gtk_window_set_screen (GTK_WINDOW (fish->preferences_dialog),
				       gtk_widget_get_screen (GTK_WIDGET (fish)));
		gtk_window_present (GTK_WINDOW (fish->preferences_dialog));
		return;
	}

	xml = glade_xml_new (FISH_GLADEDIR "/fish.glade", NULL, NULL);
	fish->preferences_dialog = glade_xml_get_widget (xml, "fish_preferences_dialog");

	g_object_add_weak_pointer (G_OBJECT (fish->preferences_dialog),
				   (void**) &fish->preferences_dialog);

	gtk_window_set_wmclass (GTK_WINDOW (fish->preferences_dialog),
				"fish", "Fish");
	gtk_dialog_set_default_response (
		GTK_DIALOG (fish->preferences_dialog), GTK_RESPONSE_OK);

	gnome_window_icon_set_from_file (
		GTK_WINDOW (fish->preferences_dialog),
		GNOME_ICONDIR "/gnome-fish.png");

	fish->name_entry = glade_xml_get_widget (xml, "name_entry");
	gtk_entry_set_text (GTK_ENTRY (fish->name_entry), fish->name);

	g_signal_connect (fish->name_entry, "changed",
			  G_CALLBACK (name_value_changed), fish);


	fish->pixmap_entry = glade_xml_get_widget (xml, "image_entry");
	fish->image_entry = gnome_file_entry_gtk_entry (
				GNOME_FILE_ENTRY (fish->pixmap_entry));
	gtk_entry_set_text (GTK_ENTRY (fish->image_entry), fish->image);

	g_signal_connect (fish->image_entry, "changed",
			  G_CALLBACK (image_value_changed), fish);


	fish->command_entry = glade_xml_get_widget (xml, "command_entry");
	gtk_entry_set_text (GTK_ENTRY (fish->command_entry), fish->command);

	g_signal_connect (fish->command_entry, "changed",
			  G_CALLBACK (command_value_changed), fish);


	fish->frames_spin = glade_xml_get_widget (xml, "frames_spin");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (fish->frames_spin),
				   fish->n_frames);

	g_signal_connect (fish->frames_spin, "value_changed",
			  G_CALLBACK (n_frames_value_changed), fish);


	fish->speed_spin = glade_xml_get_widget (xml, "speed_spin");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (fish->speed_spin), fish->speed);

	g_signal_connect (fish->speed_spin, "value_changed",
			  G_CALLBACK (speed_value_changed), fish);


	fish->rotate_toggle = glade_xml_get_widget (xml, "rotate_toggle");
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (fish->rotate_toggle), fish->rotate);

	g_signal_connect (fish->rotate_toggle, "toggled",
			  G_CALLBACK (rotate_value_changed), fish);


	g_signal_connect (fish->preferences_dialog, "delete_event",
			  G_CALLBACK (delete_event), fish);
	g_signal_connect (fish->preferences_dialog, "response",
			  G_CALLBACK (handle_response), fish);

	button = glade_xml_get_widget (xml, "done_button");
        g_signal_connect (button, "clicked",
			  (GCallback) close_dialog, fish);

	g_signal_connect (fish->preferences_dialog, "response",
			  G_CALLBACK (handle_response), fish);

	gtk_window_set_screen (GTK_WINDOW (fish->preferences_dialog),
			       gtk_widget_get_screen (GTK_WIDGET (fish)));
	gtk_window_present (GTK_WINDOW (fish->preferences_dialog));

	g_object_unref (xml);
}

static void
display_help_dialog (BonoboUIComponent *uic,
		     FishApplet        *fish,
		     const char        *verbname)
{
	show_help (fish, NULL);
}

static void
display_about_dialog (BonoboUIComponent *uic,
		      FishApplet        *fish,
		      const char        *verbname)
{
	const char *author_format = _("%s the Fish");
	const char *about_format = _("%s has no use what-so-ever. "
				     "It only takes up disk space and "
				     "compilation time, and if loaded it also "
				     "takes up precious panel space and memory. "
				     "If anyone is found using it, he "
				     "should be promptly sent for a psychiatric "
				     "evaluation.");
	char        *authors [3];
	GdkPixbuf   *pixbuf;
	GError      *error = NULL;
	char        *file;
	char        *descr;

	if (fish->about_dialog) {
		gtk_window_set_screen (GTK_WINDOW (fish->about_dialog),
				       gtk_widget_get_screen (GTK_WIDGET (fish)));
		gtk_window_present (GTK_WINDOW (fish->about_dialog));
		return;
	}

	authors [0] = g_strdup_printf (author_format, fish->name);
	authors [1] = _("(with minor help from George)");
	authors [2] = NULL;

	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
					  "gnome-fish.png", FALSE, NULL);
	pixbuf = gdk_pixbuf_new_from_file (file, &error);
	g_free (file);

	if (error) {
		g_warning (G_STRLOC ": cannot open %s: %s", file, error->message);
		g_error_free (error);
	}

	descr = g_strdup_printf (about_format, fish->name);
		
	fish->about_dialog =
		gnome_about_new (_("Fish"),
				 "3.4.7.4ac19",
				 "Copyright \xc2\xa9 1998-2002 Free Software Foundation, Inc.",
				 descr,
				 (const char **) authors,
				 NULL,
				 NULL,
				 pixbuf);

	g_free (descr);
	g_free (authors [0]);

	if (pixbuf)
		gdk_pixbuf_unref (pixbuf);

	gtk_window_set_wmclass (
		GTK_WINDOW (fish->about_dialog), "fish", "Fish");
	gtk_window_set_screen (GTK_WINDOW (fish->about_dialog),
			       gtk_widget_get_screen (GTK_WIDGET (fish)));
	gnome_window_icon_set_from_file (GTK_WINDOW (fish->about_dialog),
					 GNOME_ICONDIR " /gnome-fish.png");

	g_signal_connect (fish->about_dialog, "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &fish->about_dialog);
	gtk_widget_show (fish->about_dialog);
}

static void
set_ally_name_desc (GtkWidget  *widget,
		    FishApplet *fish)
{
	const char *name_format = _("%s the GNOME Fish");
	const char *desc_format = _("%s the GNOME Fish, a contemporary oracle");
	AtkObject  *obj;
	char       *desc, *name;

	obj = gtk_widget_get_accessible (widget);
	/* Return immediately if GAIL is not loaded */
	if (!GTK_IS_ACCESSIBLE (obj))
		return;

	name = g_strdup_printf (name_format, fish->name);
	atk_object_set_name (obj, name);
	g_free (name);       

	desc = g_strdup_printf (desc_format, fish->name);
	atk_object_set_description (obj, desc);
	g_free (desc);
}

static void
something_fishy_going_on (FishApplet *fish,
			  const char *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 message);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_screen (GTK_WINDOW (dialog),
			       gtk_widget_get_screen (GTK_WIDGET (fish)));
	gtk_widget_show (dialog);
}

static char *
locate_fortune_command (FishApplet *fish)
{
	char *retval = NULL;

	if (!fish->command) {
		something_fishy_going_on (
			fish, _("Unable to get the name of the command to execute"));
		return NULL;
	}

	if (g_path_is_absolute (fish->command))
		retval = g_strdup (fish->command);
	else
		retval = g_find_program_in_path (fish->command);

	if (!retval)
		retval = g_find_program_in_path ("fortune");

	if (!retval && g_file_test ("/usr/games/fortune", G_FILE_TEST_EXISTS))
		retval = g_strdup ("/usr/games/fortune");

	if (!retval)
		something_fishy_going_on (
			fish, _("Unable to locate the command to execute"));

	return retval;
}

static void
handle_fortune_response (GtkWidget  *widget,
			 int         id,
			 FishApplet *fish)
{
	gtk_widget_hide (fish->fortune_dialog);
}

static void
update_fortune_dialog (FishApplet *fish) 
{
	char *label_text;
	char *text;

	if (!fish->fortune_dialog || !fish->name)
		return;

	/* xgettext:no-c-format */
	text = g_strdup_printf (_("%s the Fish"), fish->name);
	gtk_window_set_title (GTK_WINDOW (fish->fortune_dialog), text);
	g_free (text);

	/* xgettext:no-c-format */
	label_text = g_strdup_printf (_("%s the GNOME Fish Says:"), fish->name);

	text = g_strdup_printf ("<big><big>%s</big></big>", label_text);
	gtk_label_set_markup (GTK_LABEL (fish->fortune_label), text);
	g_free (text);

	g_free (label_text);
}

static void
insert_fortune_text (FishApplet *fish,
		     const char *text)
{
	GtkTextIter iter;

	gtk_text_buffer_get_iter_at_offset (fish->fortune_buffer, &iter, -1);

	gtk_text_buffer_insert (fish->fortune_buffer, &iter,
				text, -1);
}

static void
clear_fortune_text (FishApplet *fish)
{
	GtkTextIter begin, end;

	gtk_text_buffer_get_iter_at_offset (fish->fortune_buffer, &begin, 0);
	gtk_text_buffer_get_iter_at_offset (fish->fortune_buffer, &end, -1);

	gtk_text_buffer_delete (fish->fortune_buffer, &begin, &end);

	/* insert an empty line */
	insert_fortune_text (fish, "\n");
}

static void 
display_fortune_dialog (FishApplet *fish)
{
	GError *error = NULL;
	char   *fortune_command;
	char   *output = NULL;
	char   *utf8_output;

	fortune_command = locate_fortune_command (fish);
	if (!fortune_command)
		return;

	if (!fish->fortune_dialog) {
		GtkWidget *scrolled;
		GdkScreen *screen;
		int        screen_width;
		int        screen_height;
      
		fish->fortune_dialog = 
			gtk_dialog_new_with_buttons (
				"", NULL, 0,
				GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
				NULL);

		gtk_dialog_set_default_response (
			GTK_DIALOG (fish->fortune_dialog), GTK_RESPONSE_CLOSE);

		g_signal_connect (fish->fortune_dialog, "delete_event",
				  G_CALLBACK (delete_event), fish);
		g_signal_connect (fish->fortune_dialog, "response",
				  G_CALLBACK (handle_fortune_response), fish);

		gtk_window_set_wmclass (GTK_WINDOW (fish->fortune_dialog), "fish", "Fish");
		gnome_window_icon_set_from_file (GTK_WINDOW (fish->fortune_dialog),
						 GNOME_ICONDIR"/gnome-fish.png");

		screen = gtk_widget_get_screen (GTK_WIDGET (fish));

		screen_width  = gdk_screen_get_width (screen);
		screen_height = gdk_screen_get_height (screen);

		gtk_window_set_screen (GTK_WINDOW (fish->fortune_dialog), screen);

		gtk_window_set_default_size (GTK_WINDOW (fish->fortune_dialog),
					     MIN (600, screen_width  * 0.9),
					     MIN (350, screen_height * 0.9));

		fish->fortune_view = gtk_text_view_new ();
		gtk_text_view_set_editable (GTK_TEXT_VIEW (fish->fortune_view), FALSE);
		gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (fish->fortune_view), FALSE);
		gtk_text_view_set_left_margin (GTK_TEXT_VIEW (fish->fortune_view), 10);
		gtk_text_view_set_right_margin (GTK_TEXT_VIEW (fish->fortune_view), 10);
		fish->fortune_buffer =
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (fish->fortune_view));

		scrolled = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
						GTK_POLICY_AUTOMATIC,
						GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
						     GTK_SHADOW_IN);

		gtk_container_add (GTK_CONTAINER (scrolled), fish->fortune_view);

		fish->fortune_label = gtk_label_new ("");

		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (fish->fortune_dialog)->vbox), 
				    fish->fortune_label,
				    FALSE, FALSE, GNOME_PAD);

		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (fish->fortune_dialog)->vbox), 
				    scrolled,
				    TRUE, TRUE, GNOME_PAD);

		update_fortune_dialog (fish);

		gtk_widget_show_all (fish->fortune_dialog);
	}

	set_ally_name_desc (fish->fortune_view, fish);
	gtk_window_set_screen (GTK_WINDOW (fish->fortune_dialog),
			       gtk_widget_get_screen (GTK_WIDGET (fish)));
	gtk_window_present (GTK_WINDOW (fish->fortune_dialog));

	clear_fortune_text (fish);

	g_spawn_command_line_sync (fortune_command, &output, NULL, NULL, &error);
	if (error) {
		char *message;

		message = g_strdup_printf (_("Unable to execute '%s'\n\nDetails: %s"),
					   fortune_command, error->message);
		something_fishy_going_on (fish, message);
		g_free (message);
		g_error_free (error);
	}
	
	g_free (fortune_command);

	/* The output is not guarantied to be in UTF-8 format, most
	 * likely it's just in ASCII-7 or in the user locale
	  */
	if (!g_utf8_validate (output, -1, NULL))
		utf8_output = g_locale_to_utf8 (output, -1, NULL, NULL, NULL);
	else
		utf8_output = g_strdup (output);

	if (utf8_output)
		insert_fortune_text (fish, utf8_output);
	else
		insert_fortune_text (fish, 
				     _("You do not have fortune installed "
				       "or you have not specified a program "
				       "to run.\n\nPlease refer to fish "
				       "properties dialog."));

	g_free (output);
	g_free (utf8_output);
}

static void
name_changed_notify (GConfClient *client,
		     guint        cnxn_id,
		     GConfEntry  *entry,
		     FishApplet  *fish)
{
	const char *value;

	if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
		return;

	value = gconf_value_get_string (entry->value);

	if (!value [0] || (fish->name && !strcmp (fish->name, value)))
		return;

	if (fish->name)
		g_free (fish->name);
	fish->name = g_strdup (value);

	update_fortune_dialog (fish);

	if (fish->name_entry &&
	    strcmp (gtk_entry_get_text (GTK_ENTRY (fish->name_entry)), fish->name))
		gtk_entry_set_text (GTK_ENTRY (fish->name_entry), fish->name);
}

static void
image_changed_notify (GConfClient *client,
		      guint        cnxn_id,
		      GConfEntry  *entry,
		      FishApplet  *fish)
{
	const char *value;

	if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
		return;

	value = gconf_value_get_string (entry->value);

	if (!value [0] || (fish->image && !strcmp (fish->image, value)))
		return;

	if (fish->image)
		g_free (fish->image);
	fish->image = g_strdup (value);

	load_fish_image (fish);
	update_pixmap (fish);

	if (fish->image_entry &&
	    strcmp (gtk_entry_get_text (GTK_ENTRY (fish->image_entry)), fish->image))
		gtk_entry_set_text (GTK_ENTRY (fish->image_entry), fish->image);
}

static void
command_changed_notify (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			FishApplet  *fish)
{
	const char *value;

	if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
		return;
	
	value = gconf_value_get_string (entry->value);

	if (!value [0] || (fish->command && !strcmp (fish->command, value)))
		return;

	if (fish->command)
		g_free (fish->command);
	fish->command = g_strdup (value);

	if (fish->command_entry &&
	    strcmp (gtk_entry_get_text (GTK_ENTRY (fish->command_entry)), fish->command))
		gtk_entry_set_text (GTK_ENTRY (fish->command_entry), fish->command);
}

static void
n_frames_changed_notify (GConfClient *client,
			 guint        cnxn_id,
			 GConfEntry  *entry,
			 FishApplet  *fish)
{
	int value;

	if (!entry->value || entry->value->type != GCONF_VALUE_INT)
		return;

	value = gconf_value_get_int (entry->value);

	if (fish->n_frames == value)
		return;

	fish->n_frames = value;

	if (fish->n_frames <= 0)
		fish->n_frames = 1;

	update_pixmap (fish);

	if (fish->frames_spin &&
	    gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (fish->frames_spin)) != fish->n_frames)
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (fish->frames_spin), fish->n_frames);
}

static char *
get_location (void)
{
	static char  location [256];
	char         buffer [256];
	FILE        *zone;
	int          i, len, count;
	
	/* Old method : works for glibc < 2.2 */
	zone = fopen("/etc/timezone", "r");
	if (zone) {
		fscanf (zone, "%255s", location);
		fclose (zone);
		return location;
	} 

	/* New method : works for glibc 2.2 */
	len = readlink ("/etc/localtime", buffer, sizeof (buffer));
	if (len <= 0)
		return NULL;

	for (i = len, count = 0; (i > 0) && (count != 2); i--)
		if (buffer [i] == '/')
			count++;

	if (count != 2)
		return NULL;

	memcpy (location, &buffer [i + 2], len - i - 2);

	return location;
}

static void 
init_fools_day (void)
{
	const char *spanish_timezones [] = {
		"Europe/Madrid",
		"Africa/Ceuta",
		"Atlantic/Canary",
		"America/Mexico_City",
		"Mexico/BajaSur",
		"Mexico/BajaNorte",
		"Mexico/General",
		NULL
	};
	char *location;
	int  i;
	
	if (!(location = get_location ()))
		return;

	fools_day        = 1;  /* 1st */
	fools_month      = 3;  /* April */
	fools_hour_start = 0;  /* Midnight */
	fools_hour_end   = 12; /* Apparently jokes should stop at midday */
	
	for (i = 0; spanish_timezones [i]; i++)
		if (!g_strcasecmp (spanish_timezones [i], location)) {
			/* Hah!, We are in Spain or Mexico
			 * Spanish fool's day is 28th December
			 */
			fools_day = 28;
			fools_month = 11;
			return;
		}
}

static void
check_april_fools (FishApplet *fish)
{
	struct tm *tm;
	time_t     now;

	time (&now);
	tm = localtime (&now);

	if (fish->april_fools && 
	    (tm->tm_mon  != fools_month ||
	     tm->tm_mday != fools_day   ||
	     tm->tm_hour >= fools_hour_end)) {
		fish->april_fools = FALSE;
		update_pixmap (fish);
	} else if (tm->tm_mon  == fools_month    &&
		 tm->tm_mday == fools_day        &&
		 tm->tm_hour >= fools_hour_start &&
		 tm->tm_hour <= fools_hour_end) {
		fish->april_fools = TRUE;
		update_pixmap (fish);
	}
}

static gboolean
timeout_handler (gpointer data)
{
	FishApplet *fish = (FishApplet *) data;

	check_april_fools (fish);

	if (fish->april_fools)
		return TRUE;

	fish->current_frame++;
	if (fish->current_frame >= fish->n_frames)
		fish->current_frame = 0;

	gtk_widget_queue_draw (fish->drawing_area);

	return TRUE;
}

static void
setup_timeout (FishApplet *fish)
{
	if (fish->timeout)
		g_source_remove (fish->timeout);

	fish->timeout = g_timeout_add (fish->speed * 1000,
				       timeout_handler,
				       fish);
}

static void
speed_changed_notify (GConfClient *client,
		      guint        cnxn_id,
		      GConfEntry  *entry,
		      FishApplet  *fish)
{
	gdouble value;

	if (!entry->value || entry->value->type != GCONF_VALUE_FLOAT)
		return;

	value = gconf_value_get_float (entry->value);

	if (fish->speed == value)
		return;
	fish->speed = value;

	setup_timeout (fish);

	if (fish->speed_spin &&
	    gtk_spin_button_get_value (GTK_SPIN_BUTTON (fish->frames_spin)) != fish->speed)
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (fish->speed_spin), fish->speed);
}

static void
rotate_changed_notify (GConfClient *client,
		       guint        cnxn_id,
		       GConfEntry  *entry,
		       FishApplet  *fish)
{
	gboolean value;

	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);

	if (fish->rotate == value)
		return;
	fish->rotate = value;

	if (fish->orientation == PANEL_APPLET_ORIENT_LEFT ||
	    fish->orientation == PANEL_APPLET_ORIENT_RIGHT)
		update_pixmap (fish);

	if (fish->rotate_toggle &&
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (fish->rotate_toggle)) != fish->rotate)
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (fish->rotate_toggle), fish->rotate);
}

static void
setup_gconf (FishApplet *fish)
{
	PanelApplet *applet = (PanelApplet *) fish;
	char        *key;
	int          i = 0;

	key = panel_applet_gconf_get_full_key (applet, "name");
	fish->listeners [i++] = gconf_client_notify_add (
					fish->client, key,
					(GConfClientNotifyFunc) name_changed_notify,
					fish, NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (applet, "image");
	fish->listeners [i++] = gconf_client_notify_add (
					fish->client, key,
					(GConfClientNotifyFunc) image_changed_notify,
					fish, NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (applet, "command");
	fish->listeners [i++] = gconf_client_notify_add (
					fish->client, key,
					(GConfClientNotifyFunc) command_changed_notify,
					fish, NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (applet, "frames");
	fish->listeners [i++] = gconf_client_notify_add (
					fish->client, key,
					(GConfClientNotifyFunc) n_frames_changed_notify,
					fish, NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (applet, "speed");
	fish->listeners [i++] = gconf_client_notify_add (
					fish->client, key,
					(GConfClientNotifyFunc) speed_changed_notify,
					fish, NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (applet, "rotate");
	fish->listeners [i++] = gconf_client_notify_add (
					fish->client, key,
					(GConfClientNotifyFunc) rotate_changed_notify,
					fish, NULL, NULL);
	g_free (key);

	g_assert (i == N_FISH_PREFS);
}

static gboolean
load_fish_image (FishApplet *fish)
{
	GdkPixbuf *pixbuf;
	GError    *error = NULL;
	char      *path = NULL;

	if (!fish->image)
		return FALSE;

	if (g_path_is_absolute (fish->image))
		path = g_strdup (fish->image);

	else {
		path = gnome_program_locate_file (
					NULL, GNOME_FILE_DOMAIN_PIXMAP,
					fish->image, FALSE, NULL);
		if (!path)
			path = gnome_program_locate_file (
					NULL, GNOME_FILE_DOMAIN_APP_PIXMAP,
					fish->image, FALSE, NULL);
	}

	if (!path) {
		g_warning ("Cannot locate '%s'", fish->image);
		return FALSE;
	}

	pixbuf = gdk_pixbuf_new_from_file (path, &error);
	if (error) {
		g_warning ("Cannot load '%s': %s", path, error->message);
		g_error_free (error);
		g_free (path);
		return FALSE;
	}

	if (fish->pixbuf)
		g_object_unref (fish->pixbuf);
	fish->pixbuf = pixbuf;

	g_free (path);

	return TRUE;
}

static void
update_pixmap (FishApplet *fish)
{
	GtkWidget *widget = fish->drawing_area;
	GdkGC     *gc;
	int        width  = -1;
	int        height = -1;
	int        pixbuf_width = -1;
	int        pixbuf_height = -1;
	gboolean   rotate = FALSE;
	double     affine [6];
	guchar    *rgb;

	if (!GTK_WIDGET_REALIZED (widget) ||
	    widget->allocation.width <= 0 ||
	    widget->allocation.height <= 0)
		return;

	if (fish->rotate &&
	    (fish->orientation == PANEL_APPLET_ORIENT_LEFT ||
	     fish->orientation == PANEL_APPLET_ORIENT_RIGHT))
		rotate = TRUE;

	if (fish->pixbuf || load_fish_image (fish)) {
		pixbuf_width  = gdk_pixbuf_get_width  (fish->pixbuf);
		pixbuf_height = gdk_pixbuf_get_height (fish->pixbuf);

		if (fish->orientation == PANEL_APPLET_ORIENT_UP ||
		    fish->orientation == PANEL_APPLET_ORIENT_DOWN) {
			height = widget->allocation.height;
			width  = pixbuf_width * ((gdouble) height / pixbuf_height);
			widget->requisition.width = width / fish->n_frames;
		} else {
			if (!rotate) {
				width = widget->allocation.width * fish->n_frames;
				height = pixbuf_height * ((gdouble) width / pixbuf_width);
				widget->requisition.height = height;
			} else {
				width = widget->allocation.width;
				height = pixbuf_width * ((gdouble) width / pixbuf_height);
				widget->requisition.height = height / fish->n_frames;
			}
		}
	} else {
		if (rotate) {
			width  = widget->allocation.width;
			height = widget->allocation.height * fish->n_frames;
		} else {
			height = widget->allocation.height;
			width  = widget->allocation.width * fish->n_frames;
		}
	}

	g_assert (width != -1 && height != -1);

	if (width == 0 || height == 0)
		return;

	if (fish->pixmap)
		g_object_unref (fish->pixmap);
	fish->pixmap = gdk_pixmap_new (widget->window, width, height, -1);

	if (!fish->pixbuf)
		return;

	gtk_widget_queue_resize (widget);

	g_assert (pixbuf_width != -1 && pixbuf_height != -1);

	affine [1] = affine [2] = affine [4] = affine [5] = 0;

	if (!rotate) {
		affine [0] = width  / (double) pixbuf_width;
		affine [3] = height / (double) pixbuf_height;
	} else {
		double tmp [6];

		affine [0] = height / (double) pixbuf_width;
		affine [3] = width  / (double) pixbuf_height;
		
		art_affine_rotate (tmp, 270);
		art_affine_multiply (affine, affine, tmp);
                art_affine_translate (tmp, 0, height);
                art_affine_multiply (affine, affine, tmp);
	}

	if (fish->april_fools) {
                double tmp [6];

                art_affine_rotate (tmp, 180);
                art_affine_multiply (affine, affine, tmp);
                art_affine_translate (tmp, width, height);
                art_affine_multiply (affine, affine, tmp);
        }

	rgb = g_new0 (guchar, width * height * 3);

	if (gdk_pixbuf_get_has_alpha (fish->pixbuf))
		art_rgb_rgba_affine (rgb, 0, 0, width, height, width * 3,
				     gdk_pixbuf_get_pixels (fish->pixbuf),
                                     pixbuf_width, pixbuf_height,
                                     gdk_pixbuf_get_rowstride (fish->pixbuf),
                                     affine,
                                     ART_FILTER_NEAREST,
                                     NULL);
        else
                art_rgb_affine (rgb, 0, 0, width, height, width * 3,
                                gdk_pixbuf_get_pixels (fish->pixbuf),
                                pixbuf_width, pixbuf_height,
                                gdk_pixbuf_get_rowstride (fish->pixbuf),
                                affine,
                                ART_FILTER_NEAREST,
                                NULL);


	if (fish->april_fools)
                art_rgb_run_alpha (rgb, 255, 128, 0, 70, width * height);

	gc = gdk_gc_new (fish->pixmap);

	gdk_draw_rgb_image (fish->pixmap, gc,
			    0, 0, width, height,
                            GDK_RGB_DITHER_NORMAL,
                            rgb, width * 3);

	g_object_unref (gc);
        g_free (rgb);
}

static gboolean
fish_applet_expose_event (GtkWidget      *widget,
			  GdkEventExpose *event,
			  FishApplet     *fish)
{
	int width, height;
	int src_x, src_y;

	g_return_val_if_fail (fish->pixmap != NULL, FALSE);

	g_assert (fish->n_frames > 0);

	gdk_drawable_get_size (fish->pixmap, &width, &height);

	src_x = event->area.x;
	src_y = event->area.y;

	if (fish->rotate &&
	    (fish->orientation == PANEL_APPLET_ORIENT_LEFT ||
	     fish->orientation == PANEL_APPLET_ORIENT_RIGHT))
		src_y += ((height * fish->current_frame) / fish->n_frames);
	else
		src_x += ((width * fish->current_frame) / fish->n_frames);

	gdk_draw_pixmap (widget->window,
			 widget->style->fg_gc [GTK_WIDGET_STATE (widget)],
			 fish->pixmap,
			 src_x, src_y,
			 event->area.x, event->area.y,
			 event->area.width, event->area.height);

        return FALSE;
}

static void
fish_applet_size_allocate (GtkWidget     *widget,
			   GtkAllocation *allocation,
			   FishApplet    *fish)
{
	if (widget->allocation.width  != fish->prev_allocation.width ||
	    widget->allocation.height != fish->prev_allocation.height)
		update_pixmap (fish);

	fish->prev_allocation = *allocation;
}

static void
fish_applet_realize (GtkWidget  *widget,
		     FishApplet *fish)
{
	if (!fish->pixmap)
		update_pixmap (fish);
}

static void
fish_applet_unrealize (GtkWidget  *widget,
		       FishApplet *fish)
{
	if (fish->pixmap)
		g_object_unref (fish->pixmap);
	fish->pixmap = NULL;
}

static void
fish_applet_change_orient (PanelApplet       *applet,
			   PanelAppletOrient  orientation)
{
	FishApplet *fish = (FishApplet *) applet;

	if (fish->orientation == orientation)
		return;

	fish->orientation = orientation;

	if (fish->pixmap)
		update_pixmap (fish);
}

static void
change_water (FishApplet *fish)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
			NULL, 0, GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			_("The water needs changing!\n"
			  "(Look at today's date)"));
	gtk_window_set_wmclass (GTK_WINDOW (dialog), "fish", "Fish");
	gtk_window_set_screen (GTK_WINDOW (dialog),
			       gtk_widget_get_screen (GTK_WIDGET (fish)));

	gtk_widget_show_all (dialog);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
}

static gboolean
handle_keypress (GtkWidget   *widget,
		 GdkEventKey *event,
		 FishApplet  *fish)
{
	switch (event->keyval) {
	case GDK_space:
	case GDK_KP_Space:
	case GDK_Return:
	case GDK_KP_Enter:
	case GDK_ISO_Enter:
	case GDK_3270_Enter:
		if (fish->april_fools) {
			change_water (fish);
			return TRUE;
		}

		display_fortune_dialog (fish);
		break;
	default:
		return FALSE;
		break;
	}

	return TRUE;
}

static gboolean 
handle_button_press (GtkWidget      *widget,
		     GdkEventButton *event,
		     FishApplet     *fish)
{
	if (event->button != 1)
		return FALSE; 

	if (fish->april_fools) {
		change_water (fish);
		return TRUE;
	}

	display_fortune_dialog (fish);

	return TRUE; 
}

static void
set_tooltip (FishApplet *fish)
{
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new ();
	g_object_ref (tooltips);
	gtk_object_sink (GTK_OBJECT (tooltips));

	g_object_set_data (G_OBJECT (fish), "tooltips", tooltips);

	gtk_tooltips_set_tip (tooltips, GTK_WIDGET (fish),
			      _("GNOME Fish, the fortune teller"), NULL);
}

static void
destroy_tooltip (FishApplet *fish)
{
	GtkTooltips *tooltips;

	tooltips = g_object_get_data (G_OBJECT (fish), "tooltips");
	if (tooltips) {
		g_object_unref (tooltips);
		g_object_set_data (G_OBJECT (fish), "tooltips", NULL);
	}
}

static void
setup_fish_widget (FishApplet *fish)
{
	GtkWidget *widget = (GtkWidget *) fish;

	fish->frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (fish->frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (widget), fish->frame);

	fish->drawing_area = gtk_drawing_area_new ();
	gtk_container_add (GTK_CONTAINER (fish->frame), fish->drawing_area);

	g_signal_connect (fish->drawing_area, "realize",
			  G_CALLBACK (fish_applet_realize), fish);
	g_signal_connect (fish->drawing_area, "unrealize",
			  G_CALLBACK (fish_applet_unrealize), fish);
	g_signal_connect (fish->drawing_area, "size-allocate",
			  G_CALLBACK (fish_applet_size_allocate), fish);
	g_signal_connect (fish->drawing_area, "expose-event",
			  G_CALLBACK (fish_applet_expose_event), fish);

	gtk_widget_set_events (
		fish->drawing_area,
		gtk_widget_get_events (fish->drawing_area) | GDK_BUTTON_PRESS_MASK);
	g_signal_connect (fish->drawing_area, "button_press_event",
			  G_CALLBACK (handle_button_press), fish);

	load_fish_image (fish);

	update_pixmap (fish);

	setup_timeout (fish);

	set_tooltip (fish);

	g_signal_connect (fish, "key_press_event",
			  G_CALLBACK (handle_keypress), fish);

	gtk_widget_show_all (widget);
}

static const BonoboUIVerb fish_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("FishPreferences", display_preferences_dialog),
	BONOBO_UI_UNSAFE_VERB ("FishHelp",        display_help_dialog),
	BONOBO_UI_UNSAFE_VERB ("FishAbout",       display_about_dialog),

        BONOBO_UI_VERB_END
};

static gboolean
fish_applet_fill (FishApplet *fish)
{
	PanelApplet *applet = (PanelApplet *) fish;
	GError      *error = NULL;

	fish->orientation = panel_applet_get_orient (applet);

	panel_applet_add_preferences (
		applet, "/schemas/apps/fish_applet/prefs", NULL);

	setup_gconf (fish);

	fish->name = panel_applet_gconf_get_string (applet, "name", &error);
	if (error) {
		g_warning ("Error getting 'name' preference: %s", error->message);
		g_error_free (error);
		error = NULL;
	}
	if (!fish->name)
		fish->name = g_strdup ("Wanda"); /* Fallback */

	fish->image = panel_applet_gconf_get_string (applet, "image", &error);
	if (error) {
		g_warning ("Error getting 'image' preference: %s", error->message);
		g_error_free (error);
		error = NULL;
	}
	if (!fish->image)
		fish->image = g_strdup ("fish/fishanim.png"); /* Fallback */

	fish->command = panel_applet_gconf_get_string (applet, "command", &error);
	if (error) {
		g_warning ("Error getting 'command' preference: %s", error->message);
		g_error_free (error);
		error = NULL;
	}
	if (!fish->command)
		fish->command = g_strdup ("fortune"); /* Fallback */

	fish->n_frames = panel_applet_gconf_get_int (applet, "frames", &error);
	if (error) {
		g_warning ("Error getting 'frames' preference: %s", error->message);
		g_error_free (error);
		error = NULL;

		fish->n_frames = 3; /* Fallback */
	}
	if (fish->n_frames <= 0)
		fish->n_frames = 1;

	fish->speed = panel_applet_gconf_get_float (applet, "speed", &error);
	if (error) {
		g_warning ("Error getting 'speed' preference: %s", error->message);
		g_error_free (error);
		error = NULL;

		fish->speed = 1.0; /* Fallback */
	}

	fish->rotate = panel_applet_gconf_get_bool (applet, "rotate", &error);
	fish->rotate = FALSE;
	if (error) {
		g_warning ("Error getting 'rotate' preference: %s", error->message);
		g_error_free (error);
		error = NULL;

		fish->rotate = TRUE; /* Fallback */
	}

	panel_applet_setup_menu_from_file (
		applet, NULL, "GNOME_FishApplet.xml",
		NULL, fish_menu_verbs, fish);

	setup_fish_widget (fish);

	return TRUE;
}

static gboolean
fishy_factory (PanelApplet *applet,
	       const char  *iid,
	       gpointer     data)
{
	gboolean retval = FALSE;

	if (!strcmp (iid, "OAFIID:GNOME_FishApplet"))
		retval = fish_applet_fill (FISH_APPLET (applet));

	return retval;
}

static void
fish_applet_destroy (GtkObject *object)
{
	FishApplet *fish = (FishApplet *) object;
	int         i;

	if (fish->timeout)
		g_source_remove (fish->timeout);
	fish->timeout = 0;

	for (i = 0; i < N_FISH_PREFS; i++) {
		if (fish->client)
			gconf_client_notify_remove (
				fish->client, fish->listeners [i]);
		fish->listeners [i] = 0;
	}

	if (fish->name)
		g_free (fish->name);
	fish->name = NULL;

	if (fish->image)
		g_free (fish->image);
	fish->image = NULL;

	if (fish->command)
		g_free (fish->command);
	fish->command = NULL;

	if (fish->client)
		g_object_unref (fish->client);
	fish->client = NULL;

	if (fish->pixmap)
		g_object_unref (fish->pixmap);
	fish->pixmap = NULL;

	if (fish->pixbuf)
		g_object_unref (fish->pixbuf);
	fish->pixbuf = NULL;

	if (fish->about_dialog)
		gtk_widget_destroy (fish->about_dialog);
	fish->about_dialog = NULL;

	if (fish->preferences_dialog)
		gtk_widget_destroy (fish->preferences_dialog);
	fish->preferences_dialog = NULL;

	if (fish->fortune_dialog)
		gtk_widget_destroy (fish->fortune_dialog);
	fish->fortune_dialog = NULL;

	destroy_tooltip (fish);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
fish_applet_instance_init (FishApplet      *fish,
			   FishAppletClass *klass)
{
	int i;

	fish->client = gconf_client_get_default ();

	fish->name     = NULL;
	fish->image    = NULL;
	fish->command  = NULL;
	fish->n_frames = 1;
	fish->speed    = 0.0;
	fish->rotate   = FALSE;

	fish->orientation = PANEL_APPLET_ORIENT_UP;

	fish->frame         = NULL;
	fish->drawing_area  = NULL;
	fish->pixmap        = NULL;
	fish->timeout       = 0;
	fish->current_frame = 0;

	fish->prev_allocation.x      = -1;
	fish->prev_allocation.y      = -1;
	fish->prev_allocation.width  = -1;
	fish->prev_allocation.height = -1;

	fish->pixbuf = NULL;

	fish->about_dialog = NULL;

	fish->preferences_dialog = NULL;
	fish->name_entry         = NULL;
	fish->pixmap_entry       = NULL;
	fish->image_entry        = NULL;
	fish->command_entry      = NULL;
	fish->frames_spin        = NULL;
	fish->speed_spin         = NULL;
	fish->rotate_toggle      = NULL;

	fish->fortune_dialog = NULL;
	fish->fortune_view   = NULL;
	fish->fortune_label  = NULL;
	fish->fortune_buffer = NULL;

	for (i = 0; i < N_FISH_PREFS; i++)
		fish->listeners [i] = 0;

	fish->april_fools = FALSE;

	panel_applet_set_flags (PANEL_APPLET (fish),
				PANEL_APPLET_EXPAND_MINOR);
}

static void
fish_applet_class_init (FishAppletClass *klass)
{
	PanelAppletClass *applet_class    = (PanelAppletClass *) klass;
	GtkObjectClass   *gtkobject_class = (GtkObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	applet_class->change_orient = fish_applet_change_orient;

	gtkobject_class->destroy = fish_applet_destroy;

	init_fools_day ();
}

static GType
fish_applet_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelAppletClass),
			NULL, NULL,
			(GClassInitFunc) fish_applet_class_init,
			NULL, NULL,
			sizeof (FishApplet),
			0,
			(GInstanceInitFunc) fish_applet_instance_init,
			NULL
		};

		type = g_type_register_static (
				PANEL_TYPE_APPLET, "FishApplet", &info, 0);
	}

	return type;
}

PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_FishApplet_Factory",
			     fish_applet_get_type (),
			     "That-stupid-fish",
			     "0",
			     fishy_factory,
			     NULL)
