/* fish.c:
 *
 * Copyright (C) 1998-2002 Free Software Foundation, Inc.
 * Copyright (C) 2002-2005 Vincent Untz
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      George Lebl  <jirka@5z.com>
 *      Mark McLoughlin <mark@skynet.ie>
 *      Vincent Untz <vuntz@gnome.org>
 */

#include <config.h>

#include <math.h>
#include <string.h>
#include <time.h>

#include <cairo.h>
#include <cairo-xlib.h>

#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "fish-applet.h"

#define FISH_ICON "gnome-panel-fish"
#define FISH_RESOURCE_PATH "/org/gnome/panel/applet/fish/"

#define FISH_SCHEMA      "org.gnome.gnome-panel.applet.fish"
#define FISH_NAME_KEY    "name"
#define FISH_IMAGE_KEY   "image"
#define FISH_COMMAND_KEY "command"
#define FISH_SPEED_KEY   "speed"
#define FISH_ROTATE_KEY  "rotate"
#define FISH_NAME_DEFAULT  "Wanda"
#define FISH_IMAGE_DEFAULT "wanda.fish"
#define FISH_SPEED_DEFAULT 0.3

#define LOCKDOWN_SCHEMA          "org.gnome.desktop.lockdown"
#define LOCKDOWN_COMMANDLINE_KEY "disable-command-line"

struct _FishApplet
{
	GpApplet           applet;

	GSettings         *settings;
	GSettings         *lockdown_settings;

	char              *name;
	char              *image;
	char              *command;
	int                n_frames;
	gdouble            speed;
	gboolean           rotate;

	GtkWidget         *frame;
	GtkWidget         *drawing_area;
	GtkRequisition     requisition;
	GtkAllocation      prev_allocation;
	cairo_surface_t   *surface;
	guint              timeout;
	int                current_frame;
	gboolean           in_applet;

	GdkPixbuf         *pixbuf;

	GtkWidget         *preferences_dialog;

	GtkWidget         *fortune_dialog;
	GtkWidget         *fortune_view;
	GtkWidget         *fortune_label;
	GtkWidget         *fortune_cmd_label;
	GtkTextBuffer	  *fortune_buffer;

	unsigned int       source_id;
	GIOChannel        *io_channel;

	gboolean           april_fools;
};

static gboolean load_fish_image          (FishApplet *fish);
static void     update_surface           (FishApplet *fish);
static void     something_fishy_going_on (FishApplet *fish,
					  const char *message);
static void     display_fortune_dialog   (FishApplet *fish);
static void     set_tooltip              (FishApplet *fish);

G_DEFINE_TYPE (FishApplet, fish_applet, GP_TYPE_APPLET)

static int fools_day        = 0;
static int fools_month      = 0;
static int fools_hour_start = 0;
static int fools_hour_end   = 0;

static char *
get_image_path (FishApplet *fish)
{
	char *path;

	if (g_path_is_absolute (fish->image))
		path = g_strdup (fish->image);
	else
		path = g_strdup_printf ("%s/%s", FISH_ICONDIR, fish->image);

	return path;
}

static void
command_value_changed (GtkEntry   *entry,
		       FishApplet *fish)
{
	const char *text;

	text = gtk_entry_get_text (entry);

	if (!text || !text [0]) {
		g_settings_set_string (fish->settings, FISH_COMMAND_KEY, "");
		return;
	}

	if (!strncmp (text, "ps ", 3)  ||
	    !strcmp  (text, "ps")      ||
	    !strncmp (text, "who ", 4) ||
	    !strcmp  (text, "who")     ||
	    !strcmp  (text, "uptime")  ||
	    !strncmp (text, "tail ", 5)) {
		static gboolean message_given = FALSE;
		char       *message;

		if ( ! message_given) {
			message = g_strdup_printf (_("Warning: The command "
			                             "appears to be something actually useful.\n"
			                             "Since this is a useless applet, you "
			                             "may not want to do this.\n"
			                             "We strongly advise you against "
			                             "using %s for anything\n"
			                             "which would make the applet "
			                             "\"practical\" or useful."), fish->name);

			something_fishy_going_on (fish, message);

			g_free (message);

			message_given = TRUE;
		}
	}

	g_settings_set_string (fish->settings, FISH_COMMAND_KEY, text);
}

static gboolean
delete_event (GtkWidget  *widget,
	      FishApplet *fish)
{
	gtk_widget_hide (widget);

	return TRUE;
}

static void
handle_response (GtkWidget  *widget,
		 int         id,
		 FishApplet *fish)
{
	if (id == GTK_RESPONSE_HELP) {
		gp_applet_show_help (GP_APPLET (fish), "fish-settings");
		return;
	}

	gtk_widget_hide (fish->preferences_dialog);
}

static void
fill_combo_box_text (GtkComboBoxText *combo)
{
	gtk_combo_box_text_append (combo, "wanda.fish", "Wanda");
	gtk_combo_box_text_append (combo, "monkey.fish", "Monkey");
	gtk_combo_box_text_append (combo, "oldwanda.fish", "Old Wanda");
	gtk_combo_box_text_append (combo, "fishanim.fish", "Fish Anim");
	gtk_combo_box_text_append (combo, "footguy.fish", "Foot Guy");
}

static void
display_preferences_dialog (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
	FishApplet *fish = FISH_APPLET (user_data);
	GtkBuilder *builder;
	GtkWidget  *box;
	GtkWidget  *name_entry;
	GtkWidget  *command_entry;
	GtkWidget  *image;
	GtkWidget  *speed_spin;
	GtkWidget  *rotate_toggle;
	GtkWidget  *button;

	if (fish->preferences_dialog) {
		gtk_window_set_screen (GTK_WINDOW (fish->preferences_dialog),
				       gtk_widget_get_screen (GTK_WIDGET (fish)));
		gtk_window_present (GTK_WINDOW (fish->preferences_dialog));
		return;
	}

	builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	gtk_builder_add_from_resource (builder, FISH_RESOURCE_PATH "fish.ui", NULL);

	fish->preferences_dialog = GTK_WIDGET (gtk_builder_get_object (builder, "fish_preferences_dialog"));

	g_object_add_weak_pointer (G_OBJECT (fish->preferences_dialog),
				   (void**) &fish->preferences_dialog);

	gtk_window_set_icon_name (GTK_WINDOW (fish->preferences_dialog),
				  FISH_ICON);
	gtk_dialog_set_default_response (
		GTK_DIALOG (fish->preferences_dialog), GTK_RESPONSE_OK);

	name_entry = GTK_WIDGET (gtk_builder_get_object (builder, "name_entry"));
	box = GTK_WIDGET (gtk_builder_get_object (builder, "name_box"));
	g_settings_bind (fish->settings, FISH_NAME_KEY,
			 name_entry, "text",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind_writable (fish->settings, FISH_NAME_KEY,
				  box, "sensitive",
				  FALSE);

	command_entry = GTK_WIDGET (gtk_builder_get_object (builder, "command_entry"));
	box = GTK_WIDGET (gtk_builder_get_object (builder, "command_box"));
	g_settings_bind (fish->settings, FISH_COMMAND_KEY,
			 command_entry, "text",
			 G_SETTINGS_BIND_GET);
	g_signal_connect (command_entry, "changed",
			  G_CALLBACK (command_value_changed), fish);
	g_settings_bind_writable (fish->settings, FISH_COMMAND_KEY,
				  box, "sensitive",
				  FALSE);

	g_settings_bind (fish->lockdown_settings, LOCKDOWN_COMMANDLINE_KEY,
			 box, "visible",
			 G_SETTINGS_BIND_DEFAULT|G_SETTINGS_BIND_INVERT_BOOLEAN);

	image = GTK_WIDGET (gtk_builder_get_object (builder, "image_comboboxtext"));
	fill_combo_box_text (GTK_COMBO_BOX_TEXT (image));
	g_settings_bind (fish->settings, FISH_IMAGE_KEY,
	                 GTK_COMBO_BOX (image), "active-id",
	                 G_SETTINGS_BIND_DEFAULT);

	speed_spin = GTK_WIDGET (gtk_builder_get_object (builder, "speed_spin"));
	box = GTK_WIDGET (gtk_builder_get_object (builder, "speed_box"));
	g_settings_bind (fish->settings, FISH_SPEED_KEY,
			 speed_spin, "value",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind_writable (fish->settings, FISH_SPEED_KEY,
				  box, "sensitive",
				  FALSE);

	rotate_toggle = GTK_WIDGET (gtk_builder_get_object (builder, "rotate_toggle"));
	g_settings_bind (fish->settings, FISH_ROTATE_KEY,
			 rotate_toggle, "active",
			 G_SETTINGS_BIND_DEFAULT);

	g_signal_connect (fish->preferences_dialog, "delete_event",
			  G_CALLBACK (delete_event), fish);
	g_signal_connect (fish->preferences_dialog, "response",
			  G_CALLBACK (handle_response), fish);

	button = GTK_WIDGET (gtk_builder_get_object (builder, "done_button"));
        g_signal_connect_swapped (button, "clicked",
				  (GCallback) gtk_widget_hide,
				  fish->preferences_dialog);

	gtk_window_set_screen (GTK_WINDOW (fish->preferences_dialog),
			       gtk_widget_get_screen (GTK_WIDGET (fish)));
	gtk_window_set_resizable (GTK_WINDOW (fish->preferences_dialog), FALSE);
	gtk_window_present (GTK_WINDOW (fish->preferences_dialog));

	g_object_unref (builder);
}

static void
set_ally_name_desc (GtkWidget  *widget,
		    FishApplet *fish)
{
	AtkObject  *obj;
	char       *desc, *name;

	obj = gtk_widget_get_accessible (widget);
	/* Return immediately if GAIL is not loaded */
	if (!GTK_IS_ACCESSIBLE (obj))
		return;

	name = g_strdup_printf (_("%s the Fish"), fish->name);
	atk_object_set_name (obj, name);
	g_free (name);

	desc = g_strdup_printf (_("%s the Fish, a contemporary oracle"), fish->name);
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
					 "%s", message);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	gtk_window_set_icon_name (GTK_WINDOW (dialog), FISH_ICON);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_screen (GTK_WINDOW (dialog),
			       gtk_widget_get_screen (GTK_WIDGET (fish)));
	gtk_widget_show (dialog);
}

static gboolean
locate_fortune_command (FishApplet   *fish,
                        char       ***argvp)
{
	char *prog = NULL;

	if (fish->command
	    && g_shell_parse_argv (fish->command, NULL, argvp, NULL)) {
		prog = g_find_program_in_path ((*argvp)[0]);
		if (prog) {
			g_free (prog);
			return TRUE;
		}

		g_strfreev (*argvp);
		*argvp = NULL;
	}

	prog = g_find_program_in_path ("fortune");
	if (prog) {
		g_free (prog);
		if (g_shell_parse_argv ("fortune", NULL, argvp, NULL))
			return FALSE;
	}

	if (g_file_test ("/usr/games/fortune", G_FILE_TEST_IS_EXECUTABLE)
	    && g_shell_parse_argv ("/usr/games/fortune", NULL, argvp, NULL))
		return FALSE;

	something_fishy_going_on (fish,
				  _("Unable to locate the command to execute"));

	return FALSE;
}

#define FISH_RESPONSE_SPEAK 1
static inline void
fish_close_channel (FishApplet *fish)
{
	if (fish->io_channel) {
		g_io_channel_shutdown (fish->io_channel, TRUE, NULL);
		g_io_channel_unref (fish->io_channel);
	}
	fish->io_channel = NULL;
}

static void
handle_fortune_response (GtkWidget  *widget,
			 int         id,
			 FishApplet *fish)
{
	if (id == FISH_RESPONSE_SPEAK)
		display_fortune_dialog (fish);
	else {
		/* if there is still a pipe, close it: if we hide the widget,
		 * the * output can't be seen */
		if (fish->source_id)
			g_source_remove (fish->source_id);
		fish->source_id = 0;
		fish_close_channel (fish);
		gtk_widget_hide (fish->fortune_dialog);
	}
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
	label_text = g_strdup_printf (_("%s the Fish Says:"), fish->name);

	text = g_strdup_printf ("<big><big>%s</big></big>", label_text);
	gtk_label_set_markup (GTK_LABEL (fish->fortune_label), text);
	g_free (text);

	g_free (label_text);

	set_ally_name_desc (fish->fortune_view, fish);
}

static void
insert_fortune_text (FishApplet *fish,
		     const char *text)
{
	GtkTextIter iter;

	gtk_text_buffer_get_iter_at_offset (fish->fortune_buffer, &iter, -1);

	gtk_text_buffer_insert_with_tags_by_name (fish->fortune_buffer, &iter,
						  text, -1, "monospace_tag",
						  NULL);

	while (gtk_events_pending ())
	  gtk_main_iteration ();
}

static void
clear_fortune_text (FishApplet *fish)
{
	GtkTextIter begin, end;

	gtk_text_buffer_get_iter_at_offset (fish->fortune_buffer, &begin, 0);
	gtk_text_buffer_get_iter_at_offset (fish->fortune_buffer, &end, -1);

	gtk_text_buffer_delete (fish->fortune_buffer, &begin, &end);
	gtk_text_buffer_remove_tag_by_name (fish->fortune_buffer,
					    "monospace_tag", &begin, &end);

	/* insert an empty line */
	insert_fortune_text (fish, "\n");
}

static gboolean
fish_read_output (GIOChannel   *source,
		  GIOCondition  condition,
		  gpointer      data)
{
	char        output[4096];
	char       *utf8_output;
	gsize       bytes_read;
	GError     *error = NULL;
	GIOStatus   status;
	FishApplet *fish;

	fish = (FishApplet *) data;

	if (!(condition & G_IO_IN)) {
		fish->source_id = 0;
		fish_close_channel (fish);
		return FALSE;
	}

	status = g_io_channel_read_chars (source, output, 4096, &bytes_read,
					  &error);

	if (error) {
		char *message;

		message = g_strdup_printf (_("Unable to read output from command\n\nDetails: %s"),
					   error->message);
		something_fishy_going_on (fish, message);
		g_free (message);
		g_error_free (error);
		fish->source_id = 0;
		fish_close_channel (fish);
		return FALSE;
	}

	if (status == G_IO_STATUS_AGAIN)
		return TRUE;

	if (bytes_read > 0) {
		/* The output is not guarantied to be in UTF-8 format, most
		 * likely it's just in ASCII-7 or in the user locale
		 */
		if (!g_utf8_validate (output, -1, NULL))
			utf8_output = g_locale_to_utf8 (output, bytes_read,
							NULL, NULL, NULL);
		else
			utf8_output = g_strndup (output, bytes_read);

		if (utf8_output)
			insert_fortune_text (fish, utf8_output);

		g_free (utf8_output);
	}

	if (status == G_IO_STATUS_EOF) {
		fish->source_id = 0;
		fish_close_channel (fish);
	}
	return (status != G_IO_STATUS_EOF);
}

/*
 * Set the DISPLAY variable, to be use by g_spawn_async.
 */
static void
set_environment (gpointer user_data)
{
	GdkDisplay *display;

	display = gdk_display_get_default ();

	if (!g_setenv ("DISPLAY", gdk_display_get_name (display), TRUE))
		g_warning ("Failed to set DISPLAY environment variable");
}

static void
display_fortune_dialog (FishApplet *fish)
{
	GError      *error = NULL;
	gboolean     user_command;
	int          output;
	const char  *charset;
	char       **argv;

	/* if there is still a pipe, close it */
	if (fish->source_id)
		g_source_remove (fish->source_id);
	fish->source_id = 0;
	fish_close_channel (fish);

	argv = NULL;
	user_command = locate_fortune_command (fish, &argv);

	if (argv == NULL)
		return;

	if (!fish->fortune_dialog) {
		GtkWidget *scrolled;
		GtkWidget *vbox;

		fish->fortune_dialog =
			gtk_dialog_new_with_buttons (
				"", NULL, 0,
				_("_Speak again"), FISH_RESPONSE_SPEAK,
				_("_Close"), GTK_RESPONSE_CLOSE,
				NULL);

		gtk_window_set_icon_name (GTK_WINDOW (fish->fortune_dialog),
					  FISH_ICON);

		gtk_dialog_set_default_response (
			GTK_DIALOG (fish->fortune_dialog), GTK_RESPONSE_CLOSE);

		g_signal_connect (fish->fortune_dialog, "delete_event",
				  G_CALLBACK (delete_event), fish);
		g_signal_connect (fish->fortune_dialog, "response",
				  G_CALLBACK (handle_fortune_response), fish);

		gtk_window_set_default_size (GTK_WINDOW (fish->fortune_dialog), 600, 350);

		fish->fortune_view = gtk_text_view_new ();
		gtk_text_view_set_editable (GTK_TEXT_VIEW (fish->fortune_view), FALSE);
		gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (fish->fortune_view), FALSE);
		gtk_text_view_set_left_margin (GTK_TEXT_VIEW (fish->fortune_view), 10);
		gtk_text_view_set_right_margin (GTK_TEXT_VIEW (fish->fortune_view), 10);
		fish->fortune_buffer =
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (fish->fortune_view));

		gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (fish->fortune_buffer),
					    "monospace_tag", "family",
					    "Monospace", NULL);

		scrolled = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
						GTK_POLICY_AUTOMATIC,
						GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
						     GTK_SHADOW_IN);

		gtk_container_add (GTK_CONTAINER (scrolled), fish->fortune_view);

		fish->fortune_label = gtk_label_new ("");
		gtk_label_set_ellipsize (GTK_LABEL (fish->fortune_label),
					 PANGO_ELLIPSIZE_MIDDLE);
		fish->fortune_cmd_label = gtk_label_new ("");
		gtk_label_set_xalign (GTK_LABEL (fish->fortune_cmd_label), 0.0);

		vbox = gtk_dialog_get_content_area (GTK_DIALOG (fish->fortune_dialog));
		gtk_box_pack_start (GTK_BOX (vbox),
				    fish->fortune_label,
				    FALSE, FALSE, 6);

		gtk_box_pack_start (GTK_BOX (vbox),
				    scrolled,
				    TRUE, TRUE, 6);

		gtk_box_pack_start (GTK_BOX (vbox),
				    fish->fortune_cmd_label,
				    FALSE, FALSE, 6);

		update_fortune_dialog (fish);

		/* We don't show_all for the dialog since fortune_cmd_label
		 * might need to be hidden
		 * The dialog will be shown with gtk_window_present later */
		gtk_widget_show (scrolled);
		gtk_widget_show (fish->fortune_view);
		gtk_widget_show (fish->fortune_label);
	}

	if (!user_command) {
		char *command;
		char * text;

		command = g_markup_printf_escaped ("<tt>%s</tt>", argv[0]);
		text = g_strdup_printf (_("The configured command is not "
					  "working and has been replaced by: "
					  "%s"), command);
		gtk_label_set_markup (GTK_LABEL (fish->fortune_cmd_label),
				      text);
		g_free (command);
		g_free (text);
		gtk_widget_show (fish->fortune_cmd_label);
	} else {
		gtk_widget_hide (fish->fortune_cmd_label);
	}

	clear_fortune_text (fish);

	g_spawn_async_with_pipes (NULL, /* working directory */
				  argv,
				  NULL, /* envp */
				  G_SPAWN_SEARCH_PATH|G_SPAWN_STDERR_TO_DEV_NULL,
				  set_environment,
				  NULL,
				  NULL, /* child pid */
				  NULL, /* stdin */
				  &output,
				  NULL, /* stderr */
				  &error);

	if (error) {
		char *message;

		message = g_strdup_printf (_("Unable to execute '%s'\n\nDetails: %s"),
					   argv[0], error->message);
		something_fishy_going_on (fish, message);
		g_free (message);
		g_error_free (error);
		g_strfreev (argv);
		return;
	}

	fish->io_channel = g_io_channel_unix_new (output);
	/* set the correct encoding if the locale is not using UTF-8 */
	if (!g_get_charset (&charset))
		g_io_channel_set_encoding(fish->io_channel, charset, &error);
	if (error) {
		char *message;

		message = g_strdup_printf (_("Unable to read from '%s'\n\nDetails: %s"),
					   argv[0], error->message);
		something_fishy_going_on (fish, message);
		g_free (message);
		g_error_free (error);
		g_strfreev (argv);
		return;
	}

	g_strfreev (argv);

	fish->source_id = g_io_add_watch (fish->io_channel,
					  G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
					  fish_read_output, fish);

	gtk_window_set_screen (GTK_WINDOW (fish->fortune_dialog),
			       gtk_widget_get_screen (GTK_WIDGET (fish)));
	gtk_window_present (GTK_WINDOW (fish->fortune_dialog));
}

static char *
get_location (void)
{
	static char  location [256];
	char        *buffer;
	FILE        *zone;
	int          i, len, count;

	/* Old method : works for glibc < 2.2 */
	zone = fopen("/etc/timezone", "r");
	if (zone) {
		count = fscanf (zone, "%255s", location);
		fclose (zone);
		/* if we could read it, we return what we got */
		if (count == 1)
			return location;
	}

	/* New method : works for glibc 2.2 */
	/* FIXME: this is broken for many distros, see the clock code */
	buffer = g_file_read_link ("/etc/localtime", NULL);
	if (!buffer)
		return NULL;

	len = strlen (buffer);
	for (i = len, count = 0; (i > 0) && (count != 2); i--)
		if (buffer [i] == '/')
			count++;

	if (count != 2) {
		g_free (buffer);
		return NULL;
	}

	memcpy (location, &buffer [i + 2], len - i - 2);
	g_free (buffer);

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
		if (!g_ascii_strcasecmp (spanish_timezones [i], location)) {
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
		update_surface (fish);
	} else if (tm->tm_mon  == fools_month    &&
		 tm->tm_mday == fools_day        &&
		 tm->tm_hour >= fools_hour_start &&
		 tm->tm_hour <= fools_hour_end) {
		fish->april_fools = TRUE;
		update_surface (fish);
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

static gboolean
load_fish_image (FishApplet *fish)
{
	GdkPixbuf *pixbuf;
	GError    *error = NULL;
	char      *path = NULL;

	if (!fish->image)
		return FALSE;

	path = get_image_path (fish);

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

static gboolean
update_surface_in_idle (gpointer data)
{
	update_surface (FISH_APPLET (data));
	return FALSE;
}

static void
update_surface (FishApplet *fish)
{
	GtkWidget     *widget = fish->drawing_area;
	GtkRequisition prev_requisition;
	GtkAllocation  allocation;
	int            width  = -1;
	int            height = -1;
	int            pixbuf_width = -1;
	int            pixbuf_height = -1;
	gboolean       rotate = FALSE;
	cairo_t       *cr;
	cairo_matrix_t matrix;
	cairo_pattern_t *pattern;

	gtk_widget_get_allocation (widget, &allocation);

	if (!gtk_widget_get_realized (widget) ||
	    allocation.width <= 0 ||
	    allocation.height <= 0)
		return;

	if (!fish->pixbuf && !load_fish_image (fish))
		return;

	if (fish->rotate && gp_applet_get_orientation (GP_APPLET (fish)) == GTK_ORIENTATION_VERTICAL)
		rotate = TRUE;

	pixbuf_width  = gdk_pixbuf_get_width  (fish->pixbuf);
	pixbuf_height = gdk_pixbuf_get_height (fish->pixbuf);

	prev_requisition = fish->requisition;

	if (gp_applet_get_orientation (GP_APPLET (fish)) == GTK_ORIENTATION_HORIZONTAL) {
		height = allocation.height;
		width  = pixbuf_width * ((gdouble) height / pixbuf_height);

		fish->requisition.width = width / fish->n_frames;
		fish->requisition.height = height;
	} else {
		if (!rotate) {
			width = allocation.width * fish->n_frames;
			height = pixbuf_height * ((gdouble) width / pixbuf_width);
			fish->requisition.width = allocation.width;
			fish->requisition.height = height;
		} else {
			width = allocation.width;
			height = pixbuf_width * ((gdouble) width / pixbuf_height);
			fish->requisition.width = width;
			fish->requisition.height = height / fish->n_frames;
		}
	}

	if (prev_requisition.width  != fish->requisition.width ||
	    prev_requisition.height != fish->requisition.height) {
		gtk_widget_set_size_request (widget,
					     fish->requisition.width,
					     fish->requisition.height);
	}

	g_assert (width != -1 && height != -1);

	if (width == 0 || height == 0)
		return;

	if (fish->surface)
		cairo_surface_destroy (fish->surface);
	fish->surface = gdk_window_create_similar_surface (
			               gtk_widget_get_window (widget),
			               CAIRO_CONTENT_COLOR_ALPHA,
			               width, height);

	gtk_widget_queue_resize (widget);

	g_assert (pixbuf_width != -1 && pixbuf_height != -1);

	cr = cairo_create (fish->surface);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_paint (cr);

	gdk_cairo_set_source_pixbuf (cr, fish->pixbuf, 0, 0);
	pattern = cairo_get_source (cr);
	cairo_pattern_set_filter (pattern, CAIRO_FILTER_BEST);

	cairo_matrix_init_identity (&matrix);

	if (fish->april_fools) {
		cairo_matrix_translate (&matrix,
					pixbuf_width - 1, pixbuf_height - 1);
		cairo_matrix_rotate (&matrix, M_PI);
	}

	if (rotate) {
		if (gp_applet_get_position (GP_APPLET (fish)) == GTK_POS_LEFT) {
			cairo_matrix_translate (&matrix, pixbuf_width - 1, 0);
			cairo_matrix_rotate (&matrix, M_PI * 0.5);
		} else {
			cairo_matrix_translate (&matrix, 0, pixbuf_height - 1);
			cairo_matrix_rotate (&matrix, M_PI * 1.5);
		}
		cairo_matrix_scale (&matrix,
				    (double) (pixbuf_height - 1) / width,
				    (double) (pixbuf_width - 1) / height);
	} else {
		cairo_matrix_scale (&matrix,
				    (double) (pixbuf_width - 1) / width,
				    (double) (pixbuf_height - 1) / height);
	}

	cairo_pattern_set_matrix (pattern, &matrix);

	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	if (fish->april_fools) {
		cairo_set_source_rgb (cr, 1, 0.5, 0);
		cairo_paint_with_alpha (cr, 0.25);
	}

	cairo_destroy (cr);
}

static gboolean
fish_applet_draw (GtkWidget  *widget,
		  cairo_t    *cr,
		  FishApplet *fish)
{
	int width, height;
	int src_x, src_y;

	if (fish->surface == NULL)
		return GDK_EVENT_PROPAGATE;

	g_assert (fish->n_frames > 0);

	width = cairo_xlib_surface_get_width (fish->surface);
	height = cairo_xlib_surface_get_height (fish->surface);

	src_x = 0;
	src_y = 0;

	if (fish->rotate) {
		if (gp_applet_get_position (GP_APPLET (fish)) == GTK_POS_LEFT)
			src_y = ((height * (fish->n_frames - 1 - fish->current_frame)) / fish->n_frames);
		else if (gp_applet_get_position (GP_APPLET (fish)) == GTK_POS_RIGHT)
			src_y = ((height * fish->current_frame) / fish->n_frames);
		else
			src_x = ((width * fish->current_frame) / fish->n_frames);
	} else
		src_x = ((width * fish->current_frame) / fish->n_frames);

        cairo_save (cr);
        cairo_set_source_surface (cr, fish->surface, -src_x, -src_y);
        cairo_paint (cr);
        cairo_restore (cr);

        return FALSE;
}

static void
fish_applet_size_allocate (GtkWidget     *widget,
			   GtkAllocation *allocation,
			   FishApplet    *fish)
{
	if (allocation->width  == fish->prev_allocation.width &&
	    allocation->height == fish->prev_allocation.height)
		return;

	fish->prev_allocation = *allocation;
	g_idle_add (update_surface_in_idle, fish);
}

static void
fish_applet_realize (GtkWidget  *widget,
		     FishApplet *fish)
{
	if (!fish->surface)
		update_surface (fish);
}

static void
fish_applet_unrealize (GtkWidget  *widget,
		       FishApplet *fish)
{
	if (fish->surface)
		cairo_surface_destroy (fish->surface);
	fish->surface = NULL;
}

static void
change_water (FishApplet *fish)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
			NULL, 0, GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			_("The water needs changing"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("Look at today's date!"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), FISH_ICON);
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
	case GDK_KEY_space:
	case GDK_KEY_KP_Space:
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
	case GDK_KEY_ISO_Enter:
	case GDK_KEY_3270_Enter:
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
fish_enter_notify (GtkWidget        *widget,
		   GdkEventCrossing *event)
{
  FishApplet *fish;
  GtkWidget  *event_widget;

  fish = FISH_APPLET (widget);
  event_widget = gtk_get_event_widget ((GdkEvent*) event);

  if ((event_widget == widget) &&
      (event->detail != GDK_NOTIFY_INFERIOR))
      fish->in_applet = TRUE;

  return FALSE;
}

static gboolean
fish_leave_notify (GtkWidget        *widget,
		   GdkEventCrossing *event)
{
  FishApplet *fish;
  GtkWidget  *event_widget;

  fish = FISH_APPLET (widget);
  event_widget = gtk_get_event_widget ((GdkEvent*) event);

  if ((event_widget == widget) &&
      (event->detail != GDK_NOTIFY_INFERIOR))
      fish->in_applet = FALSE;

  return FALSE;
}

static gboolean
handle_button_release (FishApplet     *fish,
		       GdkEventButton *event)
{
	if (!fish->in_applet || event->button != 1)
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
	char       *desc;

	desc = g_markup_printf_escaped (_("%s the Fish, the fortune teller"), fish->name);
	gtk_widget_set_tooltip_markup (GTK_WIDGET (fish), desc);
	g_free (desc);

	g_object_bind_property (fish, "enable-tooltips",
	                        fish, "has-tooltip",
	                        G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
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
	g_signal_connect (fish->drawing_area, "draw",
			  G_CALLBACK (fish_applet_draw), fish);

	gtk_widget_add_events (widget, GDK_ENTER_NOTIFY_MASK |
				       GDK_LEAVE_NOTIFY_MASK |
				       GDK_BUTTON_RELEASE_MASK);

	g_signal_connect_swapped (widget, "enter_notify_event",
				  G_CALLBACK (fish_enter_notify), fish);
	g_signal_connect_swapped (widget, "leave_notify_event",
				  G_CALLBACK (fish_leave_notify), fish);
	g_signal_connect_swapped (widget, "button_release_event",
				  G_CALLBACK (handle_button_release), fish);

	gtk_widget_add_events (fish->drawing_area, GDK_BUTTON_RELEASE_MASK);
	g_signal_connect_swapped (fish->drawing_area, "button_release_event",
				  G_CALLBACK (handle_button_release), fish);

	load_fish_image (fish);

	update_surface (fish);

	g_signal_connect (fish, "key_press_event",
			  G_CALLBACK (handle_keypress), fish);

	gtk_widget_show_all (fish->frame);
}

static const GActionEntry fish_menu_actions [] = {
	{ "preferences", display_preferences_dialog, NULL, NULL, NULL },
	{ NULL }
};

static void
fish_applet_update_name (FishApplet *fish,
			 const char *name)
{
	if (name[0] == '\0')
		name = FISH_NAME_DEFAULT;
	fish->name = g_strdup (name);

	update_fortune_dialog (fish);
	set_tooltip (fish);
	set_ally_name_desc (GTK_WIDGET (fish), fish);
}

static void
fish_applet_update_image (FishApplet *fish,
			  const char *image)
{
	char     *keyfile_path;
	GKeyFile *keyfile;

	if (image[0] == '\0')
		image = FISH_IMAGE_DEFAULT;
	keyfile_path = g_build_filename (FISH_ICONDIR, image, NULL);

	keyfile = g_key_file_new ();
	if (!g_key_file_load_from_file (keyfile, keyfile_path,
					G_KEY_FILE_NONE, NULL)) {
		if (g_strcmp0 (image, FISH_IMAGE_DEFAULT) == 0) {
			g_critical ("Cannot load default image ('%s')", image);
			g_assert_not_reached ();
		}

		g_key_file_free (keyfile);
		g_free (keyfile_path);

		fish_applet_update_image (fish, FISH_IMAGE_DEFAULT);
		return;
	}

	fish->image = g_key_file_get_string (keyfile, "Fish Animation",
					     "image", NULL);
	fish->n_frames = g_key_file_get_integer (keyfile, "Fish Animation",
						 "frames", NULL);
	if (fish->n_frames <= 0)
		fish->n_frames = 1;

	load_fish_image (fish);
	update_surface (fish);

	g_key_file_free (keyfile);
	g_free (keyfile_path);
}

static void
fish_applet_update_command (FishApplet *fish,
			    const char *command)
{
	g_free (fish->command);
	fish->command = g_strdup (command);
}

static void
fish_applet_update_speed (FishApplet *fish,
			  gdouble     speed)
{
	if (speed <= 0)
		speed = FISH_SPEED_DEFAULT;
	fish->speed = speed;
	setup_timeout (fish);
}

static void
fish_applet_update_rotate (FishApplet *fish,
			   gboolean    rotate)
{
	fish->rotate = rotate;

	if (gp_applet_get_orientation (GP_APPLET (fish)) == GTK_ORIENTATION_VERTICAL)
		update_surface (fish);
}

static void
fish_applet_settings_changed (GSettings  *settings,
			      char       *key,
			      FishApplet *fish)
{
	char     *value_str = NULL;
	gdouble   value_double;
	gboolean  value_boolean;

	/* key == NULL is a locak hack to mean all keys */

	if (!key || g_strcmp0 (key, FISH_NAME_KEY) == 0) {
		value_str = g_settings_get_string (settings, FISH_NAME_KEY);
		fish_applet_update_name (fish, value_str);
		g_free (value_str);
	}
	if (!key || g_strcmp0 (key, FISH_IMAGE_KEY) == 0) {
		value_str = g_settings_get_string (settings, FISH_IMAGE_KEY);
		fish_applet_update_image (fish, value_str);
		g_free (value_str);
	}
	if (!key || g_strcmp0 (key, FISH_COMMAND_KEY) == 0) {
		value_str = g_settings_get_string (settings, FISH_COMMAND_KEY);
		fish_applet_update_command (fish, value_str);
		g_free (value_str);
	}
	if (!key || g_strcmp0 (key, FISH_SPEED_KEY) == 0) {
		value_double = g_settings_get_double (settings, FISH_SPEED_KEY);
		fish_applet_update_speed (fish, value_double);
	}
	if (!key || g_strcmp0 (key, FISH_ROTATE_KEY) == 0) {
		value_boolean = g_settings_get_boolean (settings, FISH_ROTATE_KEY);
		fish_applet_update_rotate (fish, value_boolean);
	}
}

static void
fish_applet_constructed (GObject *object)
{
	FishApplet *fish = FISH_APPLET (object);
	GpApplet *applet = GP_APPLET (fish);
	GAction *action;

	G_OBJECT_CLASS (fish_applet_parent_class)->constructed (object);

	fish->settings = gp_applet_settings_new (applet, FISH_SCHEMA);
	fish->lockdown_settings = g_settings_new (LOCKDOWN_SCHEMA);

	gp_applet_setup_menu_from_resource (applet,
	                                    FISH_RESOURCE_PATH "fish-menu.ui",
	                                    fish_menu_actions);

	action = gp_applet_menu_lookup_action (applet, "preferences");
	g_object_bind_property (applet, "locked-down",
				action, "enabled",
				G_BINDING_DEFAULT|G_BINDING_INVERT_BOOLEAN|G_BINDING_SYNC_CREATE);

	setup_fish_widget (fish);

	g_signal_connect (fish->settings, "changed",
	                  G_CALLBACK (fish_applet_settings_changed), fish);
	fish_applet_settings_changed (fish->settings, NULL, fish);
}

static void
fish_applet_dispose (GObject *object)
{
	FishApplet *fish = (FishApplet *) object;

	if (fish->timeout)
		g_source_remove (fish->timeout);
	fish->timeout = 0;

	if (fish->name)
		g_free (fish->name);
	fish->name = NULL;

	if (fish->image)
		g_free (fish->image);
	fish->image = NULL;

	if (fish->command)
		g_free (fish->command);
	fish->command = NULL;

	if (fish->settings)
		g_object_unref (fish->settings);
	fish->settings = NULL;

	if (fish->lockdown_settings)
		g_object_unref (fish->lockdown_settings);
	fish->lockdown_settings = NULL;

	if (fish->surface)
		cairo_surface_destroy (fish->surface);
	fish->surface = NULL;

	if (fish->pixbuf)
		g_object_unref (fish->pixbuf);
	fish->pixbuf = NULL;

	if (fish->preferences_dialog)
		gtk_widget_destroy (fish->preferences_dialog);
	fish->preferences_dialog = NULL;

	if (fish->fortune_dialog)
		gtk_widget_destroy (fish->fortune_dialog);
	fish->fortune_dialog = NULL;

	if (fish->source_id)
		g_source_remove (fish->source_id);
	fish->source_id = 0;

	fish_close_channel (fish);

	G_OBJECT_CLASS (fish_applet_parent_class)->dispose (object);
}

static void
fish_applet_placement_changed (GpApplet        *applet,
                               GtkOrientation   orientation,
                               GtkPositionType  position)
{
	FishApplet *fish;

	fish = FISH_APPLET (applet);

	if (fish->surface)
		update_surface (fish);
}

static void
fish_applet_class_init (FishAppletClass *fish_class)
{
	GObjectClass *object_class;
	GpAppletClass *applet_class;

	object_class = G_OBJECT_CLASS (fish_class);
	applet_class = GP_APPLET_CLASS (fish_class);

	object_class->constructed = fish_applet_constructed;
	object_class->dispose = fish_applet_dispose;

	applet_class->placement_changed = fish_applet_placement_changed;

	init_fools_day ();
}

static void
fish_applet_init (FishApplet *fish)
{
	gp_applet_set_flags (GP_APPLET (fish), GP_APPLET_FLAGS_EXPAND_MINOR);
}
