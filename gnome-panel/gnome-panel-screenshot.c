/* simple-screenshot.c */
/* Copyright (C) 2001 Jonathan Blandford <jrb@alum.mit.edu>
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

/* THERE ARE NO FEATURE REQUESTS ALLOWED */
/* IF YOU WANT YOUR OWN FEATURE -- WRITE THE DAMN THING YOURSELF (-: */
/* MAYBE I LIED... -jrb */

#include <config.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <gdk/gdkx.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/WinUtil.h>

#include "screenshot-shadow.h"
#include "screenshot-utils.h"
#include "screenshot-save.h"
#include "screenshot-dialog.h"
#include "screenshot-xfer.h"

/* How far down the window tree will we search when looking for top-level
 * windows? Some window managers doubly-reparent the client, so account
 * for that, and add some slop.
 */
#define MAXIMUM_WM_REPARENTING_DEPTH 4

static GdkPixbuf *screenshot = NULL;

/* Global variables*/
static char *last_save_dir = NULL;
static char *window_title = NULL;
static char *temporary_file = NULL;
static gboolean drop_shadow = TRUE;
static gboolean save_immediately = FALSE;

/* Options */
static gboolean take_window_shot = FALSE;
static gboolean include_border = FALSE;
static char *border_effect = NULL;

/* some local prototypes */
static void display_help           (ScreenshotDialog *dialog);
static void save_done_notification (gpointer          data);

static void
display_help (ScreenshotDialog *dialog)
{
  GError *error = NULL;

  gnome_help_display_desktop (NULL, "user-guide", 
			      "user-guide.xml", "goseditmainmenu-53", 
			      &error);
	
  if (error)
    {
      GtkWidget *msg_dialog;

      msg_dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (screenshot_dialog_get_toplevel (dialog)),
						       GTK_DIALOG_DESTROY_WITH_PARENT,
						       GTK_MESSAGE_ERROR,
						       GTK_BUTTONS_OK,
						       "<span size=\"larger\" weight=\"bold\">%s</span>\n\n%s\n%s",
						       _("Error loading help"),
						       _("There was an error displaying the help pages for this dialog:"),
						       error->message);

      gtk_widget_show (msg_dialog);
      gtk_dialog_run (GTK_DIALOG (msg_dialog));
      gtk_widget_destroy (msg_dialog);
      g_error_free (error);
    }
}

/* We assume that uri is valid and has been tested elsewhere
 */
static char *
generate_filename_for_uri (const char *uri)
{
  char *retval;
  char *tmp;
  char *file_name;
  int i = 1;

  if (window_title)
    {
      /* translators: this is the name of the file that gets made up
       * with the screenshot if a specific window is taken */
      file_name = g_strdup_printf (_("Screenshot-%s.png"), window_title);
    }
  else
    {
      /* translators: this is the name of the file that gets made up
       * with the screenshot if the entire screen is taken */
      file_name = g_strdup (_("Screenshot.png"));
    }

  tmp = g_filename_from_utf8 (file_name, -1, NULL, NULL, NULL);
  retval = g_build_filename (uri, tmp, NULL);
  g_free (file_name);
  g_free (tmp);
	
  do
    {
      GnomeVFSFileInfo *info;
      GnomeVFSResult result;

      info = gnome_vfs_file_info_new ();
      result = gnome_vfs_get_file_info (retval, info, GNOME_VFS_FILE_INFO_DEFAULT | GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
      gnome_vfs_file_info_unref (info);

      switch (result)
	{
	case GNOME_VFS_ERROR_NOT_FOUND:
	  return retval;
	case GNOME_VFS_OK:
	  g_free (retval);
	  break;
	case GNOME_VFS_ERROR_PROTOCOL_ERROR:
	  /* try again?  I'm getting these errors sporadically */
	default:
	  g_warning ("ERR:%s:%s\n", retval, gnome_vfs_result_to_string (result));
	  g_free (retval);
	  return NULL;
	}

      /* We had a hit.  We need to make a new file */
      if (window_title)
	{
	  /* translators: this is the name of the file that gets
	   * made up with the screenshot if a specific window is
	   * taken */
	  file_name = g_strdup_printf (_("Screenshot-%s-%d.png"),
				       window_title, i);
	}
      else
	{
	  /* translators: this is the name of the file that gets
	   * made up with the screenshot if the entire screen is
	   * taken */
	  file_name = g_strdup_printf (_("Screenshot-%d.png"), i);
	}

      tmp = g_filename_from_utf8 (file_name, -1, NULL, NULL, NULL);
      retval = g_build_filename (uri, tmp, NULL);
      g_free (file_name);
      g_free (tmp);

      i++;
    }
  while (TRUE);
}

static gboolean
try_to_save (ScreenshotDialog *dialog,
	     const char       *target)
{
  GnomeVFSURI *source_uri;
  GnomeVFSURI *target_uri;

  g_assert (temporary_file);

  screenshot_dialog_set_busy (dialog, TRUE);

  source_uri = gnome_vfs_uri_new (temporary_file);
  target_uri = gnome_vfs_uri_new (target);

  screenshot_xfer_uri (source_uri,
		       target_uri,
		       screenshot_dialog_get_toplevel (dialog));

  gnome_vfs_uri_unref (source_uri);
  gnome_vfs_uri_unref (target_uri);

  screenshot_dialog_set_busy (dialog, FALSE);

  return TRUE;
}

static void
save_done_notification (gpointer data)
{
  ScreenshotDialog *dialog = data;

  temporary_file = g_strdup (screenshot_save_get_filename ());
  screenshot_dialog_enable_dnd (dialog);

  if (save_immediately)
    {
      GtkWidget *toplevel;

      toplevel = screenshot_dialog_get_toplevel (dialog);
      gtk_dialog_response (GTK_DIALOG (toplevel), GTK_RESPONSE_OK);
    }
}

static void
run_dialog (ScreenshotDialog *dialog)
{
  GtkWidget *toplevel;
  int result;
  int keep_going;
  char *uri;

  toplevel = screenshot_dialog_get_toplevel (dialog);

  do
    {
      keep_going = FALSE;
      result = gtk_dialog_run (GTK_DIALOG (toplevel));
      switch (result)
	{
	case GTK_RESPONSE_HELP:
	  display_help (dialog);
	  keep_going = TRUE;
	  break;
	case GTK_RESPONSE_OK:
	  uri = screenshot_dialog_get_uri (dialog);
	  if (temporary_file == NULL)
	    {
	      save_immediately = TRUE;
	      screenshot_dialog_set_busy (dialog, TRUE);
	      keep_going = TRUE;
	    }
	  else
	    {
	      /* We've saved the temporary file.  Lets try to copy it to the
	       * correct location */
	      if (! try_to_save (dialog, uri))
		keep_going = TRUE;
	    }
	  break;
	default:
	  break;
	}
    }
  while (keep_going);
}

static void
prepare_screenshot (void)
{
  ScreenshotDialog *dialog;
  Window win;
  char *initial_uri;

  if (!screenshot_grab_lock ())
    exit (0);

  if (take_window_shot)
    {
      win = screenshot_find_current_window (include_border);
      if (win == None)
	{
	  take_window_shot = FALSE;
	  win = GDK_ROOT_WINDOW ();
	}
      else
	{
	  gchar *tmp;

	  window_title = screenshot_get_window_title (win);
	  tmp = screenshot_sanitize_filename (window_title);
	  g_free (window_title);
	  window_title = tmp;
	}
    }
  else
    {
      win = GDK_ROOT_WINDOW ();
    }

  screenshot = screenshot_get_pixbuf (win);

  if (take_window_shot && drop_shadow)
    {
      GdkPixbuf *old = screenshot;
    
      screenshot = screenshot_add_shadow (screenshot);
      g_object_unref (old);
    }

  screenshot_release_lock ();

  if (screenshot == NULL)
    {
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (NULL, /* parent */
				       0, /* flags */
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK,
				       _("Unable to take a screenshot of the current desktop."));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      exit (1);
    }

  
  /* If uri isn't local, we should do this async before taking the sshot */
  initial_uri = generate_filename_for_uri (last_save_dir);
  dialog = screenshot_dialog_new (screenshot, initial_uri, take_window_shot);
  g_free (initial_uri);

  screenshot_save_start (screenshot, save_done_notification, dialog);

  run_dialog (dialog);

}

static gboolean
prepare_screenshot_timeout (gpointer data)
{
  prepare_screenshot ();

  return FALSE;
}

/* Load options */
static void
load_options (void)
{
  GnomeClient *client;
  GConfClient *gconf_client;

  client = gnome_master_client ();
  gnome_client_set_restart_style (client, GNOME_RESTART_NEVER);

  gconf_client = gconf_client_get_default ();

  /* Find various dirs */
  last_save_dir = gconf_client_get_string (gconf_client, "/apps/gnome_panel_screenshot/last_save_directory", NULL);
  if (!last_save_dir || !last_save_dir[0])
    {
      if (gconf_client_get_bool (gconf_client, "/apps/nautilus/preferences/desktop_is_home_dir", NULL))
	last_save_dir = g_strconcat (g_get_home_dir (), NULL);
      else
	last_save_dir = g_strconcat (g_get_home_dir (), G_DIR_SEPARATOR_S, "Desktop", NULL);
    }
  else if (last_save_dir[0] == '~')
    {
      char *tmp = gnome_vfs_expand_initial_tilde (last_save_dir);
      g_free (last_save_dir);
      last_save_dir = tmp;
    }

  include_border = gconf_client_get_bool (gconf_client, "/apps/gnome_panel_screenshot/include_border", NULL);
  border_effect = gconf_client_get_string (gconf_client, "/apps/gnome_panel_screenshot/border_effect", NULL);

  g_object_unref (gconf_client);
}

/* main */
int
main (int argc, char *argv[])
{
  gboolean window_arg = FALSE;
  gboolean include_border_arg = FALSE;
  gchar *border_effect_arg = NULL;
  guint delay_arg = 0;
	
  struct poptOption opts[] =
    {
      {"window", '\0', POPT_ARG_NONE, NULL, 0, N_("Grab a window instead of the entire screen"), NULL},
      {"include-border", '\0', POPT_ARG_NONE, NULL, 0, N_("Include the window border with the screenshot"), NULL},
      {"delay", '\0', POPT_ARG_INT, NULL, 0, N_("Take screenshot after specified delay [in seconds]"), NULL},
      {"border-effect", '\0', POPT_ARG_STRING, NULL, 0, N_("Effect to add to the window border"), NULL},
      {NULL, '\0', 0, NULL, 0, NULL, NULL}
    };

  opts[0].arg = &window_arg;
  opts[1].arg = &include_border_arg;
  opts[2].arg = &delay_arg;
  opts[3].arg = &border_effect_arg;

  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init ("gnome-panel-screenshot", VERSION,
		      LIBGNOMEUI_MODULE,
		      argc, argv,
		      GNOME_PARAM_POPT_TABLE, opts,
		      GNOME_PROGRAM_STANDARD_PROPERTIES,
		      NULL);
  glade_gnome_init();
  gtk_window_set_default_icon_name ("applets-screenshooter");

  load_options ();
  /* allow the command line to override options */
  if (window_arg)
    take_window_shot = TRUE;
  
  if (include_border_arg)
    include_border = TRUE;
  if (border_effect_arg)
    {
      g_free (border_effect);
      border_effect = border_effect_arg;
    }

  if (delay_arg > 0)
    {
      g_timeout_add (delay_arg * 1000, 
		     prepare_screenshot_timeout,
		     NULL);
    }
  else
    {
      prepare_screenshot ();
    }

  return 0;
}



#if 0
/* nibble on the file a bit and return the file pointer
 * if it tastes good */
static FILE *
nibble_on_file (const char *file)
{
	GtkWidget *dialog;
	FILE *fp;
	mode_t old_mask;

	if (file == NULL)
		return NULL;

	if (access (file, F_OK) == 0) {
		int response;
		char *utf8_name = g_filename_to_utf8 (file, -1, NULL, NULL, NULL);

		dialog = gtk_message_dialog_new
			(GTK_WINDOW (toplevel),
			 0 /* flags */,
			 GTK_MESSAGE_QUESTION,
			 GTK_BUTTONS_YES_NO,
			 _("File %s already exists. Overwrite?"),
			 utf8_name);
		g_free (utf8_name);

		response = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		if (response != GTK_RESPONSE_YES)
			return NULL;
	}

	old_mask = umask(022);

	fp = fopen (file, "w");
	if (fp == NULL) {
		char *utf8_name = g_filename_to_utf8 (file, -1, NULL, NULL, NULL);
		dialog = gtk_message_dialog_new
			(GTK_WINDOW (toplevel),
			 0 /* flags */,
			 GTK_MESSAGE_ERROR,
			 GTK_BUTTONS_OK,
			 _("Unable to create the file:\n"
			   "\"%s\"\n"
			   "Please check your permissions of "
			   "the parent directory"), utf8_name);
		g_free (utf8_name);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		umask(old_mask);
		return NULL;
	}
	umask(old_mask);
	return fp;
}

static gboolean
gimme_file (char *filename)
{
	FILE *fp;

	g_strstrip (filename);
	fp = nibble_on_file (filename);
	if (fp == NULL) {
		return FALSE;
	}

	/* if there is a temporary in the works
	 * gimme it */
	if (temporary_file != NULL)
		;//ensure_temporary ();FIXME

	/* if we actually got a temporary, move or copy it */
	if (temporary_file != NULL) {
		char buf[4096];
		int bytes;
		int infd, outfd;

		/* we'll we're gonna reopen this sucker */
		fclose (fp);

		if (rename (temporary_file, filename) == 0) {
			chmod (filename, 0644);
			return TRUE;
		}
		infd = open (temporary_file, O_RDONLY);
		if (infd < 0) {
			/* Eeeeek! this can never happen, but we're paranoid */
			return FALSE;
		}

		outfd = open (filename, O_CREAT|O_TRUNC|O_WRONLY, 0644);
		if (outfd < 0) {
			GtkWidget *dialog;
			char *utf8_name = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
			dialog = gtk_message_dialog_new
				(GTK_WINDOW (toplevel),
				 0 /* flags */,
				 GTK_MESSAGE_ERROR,
				 GTK_BUTTONS_OK,
				 _("Unable to create the file:\n"
				   "\"%s\"\n"
				   "Please check your permissions of "
				   "the parent directory"), utf8_name);
			g_free (utf8_name);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			close (infd);
			return FALSE;
		}

		while ((bytes = read (infd, buf, sizeof (buf))) > 0) {
			if (write (outfd, buf, bytes) != bytes) {
				GtkWidget *dialog;
				char *utf8_name = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
				close (infd);
				close (outfd);
				unlink (filename);
				dialog = gtk_message_dialog_new
					(GTK_WINDOW (toplevel),
					 0 /* flags */,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 _("Not enough room to write file %s"),
					 utf8_name);
				g_free (utf8_name);
				gtk_dialog_run (GTK_DIALOG (dialog));
				gtk_widget_destroy (dialog);
				return FALSE;
			}
		}

		close (infd);
		close (outfd);

		return TRUE;
	} else {
		//FIXME: return save_to_file (fp, filename, TRUE);
	}
	return FALSE;
}
#endif

