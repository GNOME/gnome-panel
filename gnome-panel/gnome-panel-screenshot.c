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

/* FIXME: currently undone things */
#undef HAVE_PAPER_WIDTH

/* THERE ARE NO FEATURE REQUESTS ALLOWED */
/* IF YOU WANT YOUR OWN FEATURE -- WRITE THE DAMN THING YOURSELF (-:*/

#include <config.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <png.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <locale.h>
#ifdef HAVE_GNOME_PRINT
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-preview.h>
#include <libgnomeprint/gnome-print-dialog.h>
#include <libgnomeprint/gnome-print-master-preview.h>
#endif
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/WinUtil.h>

#ifdef HAVE_X11_EXTENSIONS_SHAPE_H
#include <X11/extensions/shape.h>
#endif

#include <libart_lgpl/art_rgb_affine.h>

#ifdef HAVE_PAPER_WIDTH
#include <stdio.h>
#include <langinfo.h>
#endif

/* How far down the window tree will we search when looking for top-level
 * windows? Some window managers doubly-reparent the client, so account
 * for that, and add some slop.
 */
#define MAXIMUM_WM_REPARENTING_DEPTH 4

static GladeXML *xml = NULL;
static GtkWidget *toplevel = NULL;
static GtkWidget *preview = NULL;
static GdkPixbuf *screenshot = NULL;
static GdkPixbuf *preview_image = NULL;
static char *web_dir;
static char *desktop_dir;
static const char *home_dir;
static char *class_name = NULL;
static pid_t temporary_pid = 0;
static char *temporary_file = NULL;
static int  status;

static GtkTargetEntry drag_types[] =
	{ { "x-special/gnome-icon-list", 0, 0 },
	  { "text/uri-list", 0, 0 } };

/* some prototypes for the glade autoconnecting sutff */
void on_save_rbutton_toggled (GtkWidget *toggle, gpointer data);
void on_preview_expose_event (GtkWidget *drawing_area,
			      GdkEventExpose *event,
			      gpointer data);
void on_preview_configure_event (GtkWidget *drawing_area,
				 GdkEventConfigure *event,
				 gpointer data);
void on_ok_button_clicked (GtkWidget *widget, gpointer data);
void on_cancel_button_clicked (GtkWidget *widget, gpointer data);
void on_help_button_clicked (GtkWidget *widget, gpointer data);
int on_save_entry_key_press_event (GtkWidget *widget, GdkEventKey *key);
int on_toplevel_key_press_event (GtkWidget *widget, GdkEventKey *key);

/* some local prototypes */
static gchar * add_file_to_path (const gchar *path);

/* helper functions */
/* This code is copied from gdk-pixbuf-HEAD.  It does no memory management and
 * is very hard-coded.  Please do not use it anywhere else. */
static gboolean
save_to_file_internal (FILE *fp, const char *file, char **error)
{
	png_structp png_ptr;
	png_infop info_ptr;
	guchar *ptr;
	guchar *pixels;
	int x, y, j;
	png_bytep row_ptr, data = NULL;
	png_color_8 sig_bit;
	int w, h, rowstride;
	int has_alpha;
	int bpc;
	int p;
	
	*error = NULL;

	bpc = gdk_pixbuf_get_bits_per_sample (screenshot);
	w = gdk_pixbuf_get_width (screenshot);
	h = gdk_pixbuf_get_height (screenshot);
	rowstride = gdk_pixbuf_get_rowstride (screenshot);
	has_alpha = gdk_pixbuf_get_has_alpha (screenshot);
	pixels = gdk_pixbuf_get_pixels (screenshot);

	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING,
					   NULL, NULL, NULL);

	if (png_ptr == NULL) {
		*error = _("Unable to initialize png structure.\n"
			   "You probably have a bad version of libpng "
			   "on your system");
		return FALSE;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (info_ptr == NULL) {
		*error = _("Unable to create png info.\n"
			   "You probably have a bad version of libpng "
			   "on your system");
		return FALSE;
	}

	if (setjmp (png_ptr->jmpbuf)) {
		*error = _("Unable to set png info.\n"
			   "You probably have a bad version of libpng "
			   "on your system");
		return FALSE;
	}

	png_init_io (png_ptr, fp);

	png_set_IHDR (png_ptr, info_ptr, w, h, bpc,
		      has_alpha ? PNG_COLOR_TYPE_RGB_ALPHA :
		      PNG_COLOR_TYPE_RGB,
		      PNG_INTERLACE_NONE,
		      PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	p = has_alpha ? 4 : 3;
	data = malloc (w * p * sizeof (char));

	if (data == NULL) {
		*error = _("Insufficient memory to save the screenshot.\n"
			   "Please free up some resources and try again.");
		return FALSE;
	}

	sig_bit.red = bpc;
	sig_bit.green = bpc;
	sig_bit.blue = bpc;
	sig_bit.alpha = bpc;
	png_set_sBIT (png_ptr, info_ptr, &sig_bit);
	png_write_info (png_ptr, info_ptr);
	png_set_shift (png_ptr, &sig_bit);
	png_set_packing (png_ptr);
	
	ptr = pixels;
	for (y = 0; y < h; y++) {
		for (j = 0, x = 0; x < w; x++) {
			data[x*p] = ptr[x*p];
			data[x*p + 1] = ptr[x*p + 1];
			data[x*p + 2] = ptr[x*p + 2];
			if (has_alpha)
				data[x*p + 3] = ptr[x*p + 3];			
		}
		row_ptr = (png_bytep)data;
		png_write_rows (png_ptr, &row_ptr, 1);
		ptr += rowstride;
	}

       if (data)
               free (data);

       png_write_end (png_ptr, info_ptr);
       png_destroy_write_struct (&png_ptr, (png_infopp) NULL);

       return TRUE;
}

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
save_to_file (FILE *fp, const gchar *file, gboolean gui_errors)
{
	GtkWidget *dialog;
	char *error = NULL;

	if (fp == NULL) {
		fp = nibble_on_file (file);
	}

	if ( ! save_to_file_internal (fp, file, &error)) {
		if (gui_errors && error) {
			dialog = gtk_message_dialog_new
				(GTK_WINDOW (toplevel),
				 0 /* flags */,
				 GTK_MESSAGE_ERROR,
				 GTK_BUTTONS_OK,
				 "%s", error);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		}
		fclose (fp);
		unlink (file);
		return FALSE;
	} else {
		fclose (fp);
		return TRUE;
	}
}

static void
start_temporary (void)
{
	char *dir;
	char *file = NULL;
	mode_t old_mask;

	if (temporary_file != NULL) {
		if (access (temporary_file, F_OK) == 0)
			return;

		/* Note: nautilus is a wanker and will happily do a move when
		 * we explicitly told him that we just support copy, so in case
		 * this file is missing, we let nautilus have it and hope
		 * he chokes on it */

		dir = g_path_get_dirname (temporary_file);
		rmdir (dir);
		g_free (dir);

		g_free (temporary_file = NULL);
		temporary_file = NULL;

		/* just paranoia */
		if (temporary_pid > 0)
			kill (temporary_pid, SIGKILL);
	}

	/* make a temporary dirname */
	dir = NULL;
	do {
		if (dir != NULL)
			g_free (dir);
		dir = g_strdup_printf ("/tmp/gnome-panel-screenshot-%d",
				       rand ());
	} while (mkdir (dir, 0700) < 0);

	file = add_file_to_path (dir);

	g_free (dir);

	old_mask = umask(077);

	temporary_pid = fork ();

	if (temporary_pid == 0) {
		FILE *fp = fopen (file, "w");
		if (fp == NULL ||
		    ! save_to_file (fp, file, FALSE)) {
			_exit (1);
		} else {
			_exit (0);
		}
	}

	/* can't fork? don't dispair, do synchroniously */
	else if (temporary_pid < 0) {
		FILE *fp = fopen (file, "w");
		if (fp == NULL ||
		    ! save_to_file (fp, file, TRUE)) {
			g_free (file);
			temporary_pid = 0;

			umask(old_mask);
			return;
		}
		temporary_pid = 0;

	} else if ( temporary_pid > 0)
		while ((waitpid (temporary_pid, &status, 0) == -1) && errno == EINTR);

	umask(old_mask);
	temporary_file = file;
}

static gboolean
ensure_temporary (void)
{

	start_temporary ();

	if (temporary_file == NULL)
		return FALSE;

	if (temporary_pid == 0)
		return TRUE;

	temporary_pid = 0;

	if (WIFEXITED (status) &&
	    WEXITSTATUS (status) == 0) {
		return TRUE;
	} else {
		g_free (temporary_file);
		temporary_file = NULL;
		temporary_pid = 0;
		return FALSE;
	}
}

static void
cleanup_temporary (void)
{
	char *file = temporary_file;
	pid_t pid = temporary_pid;

	temporary_file = NULL;
	temporary_pid = 0;

	if (pid > 0) {
		if (kill (pid, SIGTERM) == 0)
			waitpid (pid, NULL, 0);
	}
	
	if (file != NULL) {
		char *dir;

		unlink (file);

		dir = g_path_get_dirname (file);
		rmdir (dir);
		g_free (dir);
	}

	g_free (file);
}

#ifdef HAVE_GNOME_PRINT
static GdkPixbuf *
rotate_image (GdkPixbuf *image)
{
	GdkPixbuf *retval;
	double affine[6];
	gint width, height;

	width = gdk_pixbuf_get_width (image);
	height = gdk_pixbuf_get_height (image);

	retval = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, height, width);
	affine[0] = 0.0;    /* = cos(90) */
	affine[2] = 1.0;    /* = sin(90) */
	affine[3] = 0.0;    /* = cos(90) */
	affine[1] = -1.0;   /* = -sin(90) */
	affine[4] = 0;      /* x translation */
	affine[5] = width;  /* y translation */

	art_rgb_affine (gdk_pixbuf_get_pixels (retval),
			0, 0,
			height, width,
			gdk_pixbuf_get_rowstride (retval),
			gdk_pixbuf_get_pixels (image),
			width, height,
			gdk_pixbuf_get_rowstride (image),
			affine,
			ART_FILTER_NEAREST,
			NULL);

	return retval;
}
#endif

#ifdef HAVE_GNOME_PRINT
static void
print_page (GnomePrintContext *context, const GnomePaper *paper)
{
	GdkPixbuf *printed_image;
	gint real_width = gnome_paper_pswidth (paper);
	gint real_height = gnome_paper_psheight (paper);
	gint pix_width;
	gint pix_height;
	gint width, height;

	/* always make sure that it is taller then wide, under the potentially
	 * mistaken assumption that all paper is this way too. */
	if (gdk_pixbuf_get_width (screenshot) > gdk_pixbuf_get_height (screenshot)) {
		printed_image = rotate_image (screenshot);
	} else {
		gdk_pixbuf_ref (screenshot);
		printed_image = screenshot;
	}

	pix_width = gdk_pixbuf_get_width (printed_image);
	pix_height = gdk_pixbuf_get_height (printed_image);

	width = real_width - 2 * gnome_paper_tmargin (paper);
	height = real_height - 2 * gnome_paper_rmargin (paper);

	if (((gdouble) pix_height/pix_width) >
	    ((gdouble)width/height)) {
		/* We scale to the top */
		width = height * (gdouble)pix_width/pix_height;
	} else {
		/* We scale to the sides of the page */
		height = width * (gdouble)pix_height/pix_width;
	}

	gnome_print_beginpage (context, "1");

	gnome_print_gsave (context);
	gnome_print_translate (context, (real_width-width)/2.0, (real_height - height)/2.0);
	gnome_print_scale (context, width, height);
	gnome_print_pixbuf (context, printed_image);
	gnome_print_grestore (context);
  
	gnome_print_showpage (context);
	gnome_print_context_close (context);
	g_object_unref (G_OBJECT (printed_image));
}
#endif

#ifdef HAVE_GNOME_PRINT
static gboolean
print_pixbuf (void)
{
	GnomePrintDialog *print_dialog;
	GnomePrintContext *context;
	GnomePrintMaster *gpm = NULL;
	GnomePrintMasterPreview *gpmp;
	const GnomePaper *paper;
	gint do_preview = FALSE;
	gint copies, collate;
	gint result;
	GdkCursor *cursor;
#ifdef HAVE_PAPER_WIDTH
	GnomeUnit *unit;
	guint width, height;
#endif

	print_dialog = GNOME_PRINT_DIALOG (gnome_print_dialog_new (_("Print Screenshot"), GNOME_PRINT_DIALOG_COPIES));
	gnome_dialog_set_parent (GNOME_DIALOG (print_dialog), GTK_WINDOW (toplevel));
	do {
		result = gnome_dialog_run (GNOME_DIALOG (print_dialog)); 
		switch (result) {
		case GNOME_PRINT_CANCEL:
			gnome_dialog_close (GNOME_DIALOG (print_dialog));
		case -1:
			return FALSE;
		case GNOME_PRINT_PREVIEW:
			do_preview = TRUE;
			break;
		default:
			do_preview = FALSE;
			break;
		}

		/* set up the gnome_print_master */
		gpm = gnome_print_master_new ();
		gnome_print_dialog_get_copies (print_dialog, &copies, &collate);
		gnome_print_master_set_copies (gpm, copies, collate);
		gnome_print_master_set_printer (gpm, gnome_print_dialog_get_printer (print_dialog));

#ifdef HAVE_PAPER_WIDTH
		unit = gnome_unit_with_name ("Millimeter");

		width = (unsigned int)(size_t)nl_langinfo (_NL_PAPER_WIDTH);
		height = (unsigned int)(size_t)nl_langinfo (_NL_PAPER_HEIGHT);

		g_print ("%f %f\n", gnome_paper_convert_to_points (width, unit),
			 gnome_paper_convert_to_points (height, unit));
		paper = gnome_paper_with_size (gnome_paper_convert_to_points (width, unit),
					       gnome_paper_convert_to_points (height, unit));
#else
		paper = gnome_paper_with_name (gnome_paper_name_default ());
#endif

		gnome_print_master_set_paper (gpm, paper);
		context = gnome_print_master_get_context (gpm);

		print_page (context, paper);

		if (do_preview == FALSE) {
			gnome_dialog_close (GNOME_DIALOG (print_dialog));
			gnome_print_master_print (gpm);
			gnome_print_master_close (gpm);
			return TRUE;
		}
		gpmp = gnome_print_master_preview_new (gpm, _("Screenshot Print Preview"));
		g_signal_connect (G_OBJECT (gpmp), "destroy", 
				  G_CALLBACK (gtk_main_quit), NULL);
		gtk_widget_set_sensitive (GTK_WIDGET (print_dialog), FALSE);
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (GTK_WIDGET (print_dialog)->window, cursor);
		gdk_cursor_unref (cursor);

		gtk_widget_show (GTK_WIDGET (gpmp));
		gtk_window_set_modal (GTK_WINDOW (gpmp), TRUE);
		gtk_window_set_transient_for (GTK_WINDOW (gpmp), GTK_WINDOW (print_dialog));
		gtk_main ();
		gtk_widget_set_sensitive (GTK_WIDGET (print_dialog), TRUE);
		gdk_window_set_cursor (GTK_WIDGET (print_dialog)->window, NULL);
	} while (TRUE);
}
#endif

static gchar *
add_file_to_path (const gchar *path)
{
	char *retval;
	char *tmp;
	int   i = 1;

	if (class_name) {
		/* translators: this is the file that gets made up with the screenshot if a specific window is taken */
		tmp = g_strdup_printf (_("%s%cScreenshot-%s.png"), path,
				       G_DIR_SEPARATOR, class_name);
		retval = g_filename_from_utf8 (tmp, -1, NULL, NULL, NULL);
		g_free (tmp);
	}
	else {
		/* translators: this is the file that gets made up with the screenshot if the entire screen is taken */
		tmp = g_strdup_printf (_("%s%cScreenshot.png"), path,
				       G_DIR_SEPARATOR);
		retval = g_filename_from_utf8 (tmp, -1, NULL, NULL, NULL);
		g_free (tmp);
	}
	
	do {
		struct stat s;

		if (stat (retval, &s) &&
		    errno == ENOENT)
			return retval;

		g_free (retval);

		if (class_name) {
			/* translators: this is the file that gets made up with the screenshot if a specific window is taken */
			tmp = g_strdup_printf (_("%s%cScreenshot-%s-%d.png"), path,
					       G_DIR_SEPARATOR, class_name, i);
			retval = g_filename_from_utf8 (tmp, -1, NULL, NULL, NULL);
			g_free (tmp);
		}
		else {
			/* translators: this is the file that gets made up with the screenshot if the entire screen is taken */
			tmp = g_strdup_printf (_("%s%cScreenshot-%d.png"), path,
					       G_DIR_SEPARATOR, i);
			retval = g_filename_from_utf8 (tmp, -1, NULL, NULL, NULL);
			g_free (tmp);
		}
		
		i++;
	} while (TRUE);
}

/* Callbacks */
void
on_save_rbutton_toggled (GtkWidget *toggle, gpointer data)
{
	GtkWidget *save_fileentry = glade_xml_get_widget (xml, "save_fileentry");

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)))
		gtk_widget_set_sensitive (save_fileentry, TRUE);
	else
		gtk_widget_set_sensitive (save_fileentry, FALSE);
}

void
on_preview_expose_event (GtkWidget      *drawing_area,
			 GdkEventExpose *event,
			 gpointer        data)
{
	gdk_draw_pixbuf (drawing_area->window,
			 drawing_area->style->white_gc,
			 preview_image,
			 event->area.x,
			 event->area.y,
			 event->area.x,
			 event->area.y,
			 event->area.width,
			 event->area.height,
			 GDK_RGB_DITHER_NORMAL,
			 0, 0);
}

void
on_preview_configure_event (GtkWidget         *drawing_area,
			    GdkEventConfigure *event,
			    gpointer           data)
{
	if (preview_image)
		g_object_unref (G_OBJECT (preview_image));

	preview_image = gdk_pixbuf_scale_simple (screenshot,
						 event->width,
						 event->height,
						 GDK_INTERP_BILINEAR);
}

static void
setup_busy (gboolean busy)
{
	GdkCursor *cursor;

	if (busy) {
		/* Change cursor to busy */
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (toplevel->window, cursor);
		gdk_cursor_unref (cursor);
	} else {
		gdk_window_set_cursor (toplevel->window, NULL);
	}

	/* block expose on the, since we don't want to redraw the preview
	 * in the draw. It'd make no sense and would just generate X traffic */
	g_signal_handlers_block_by_func
		(G_OBJECT (preview),
		 G_CALLBACK (on_preview_expose_event),
		 NULL);

	gtk_widget_set_sensitive (toplevel, ! busy);
	gtk_widget_queue_draw (toplevel);

	g_signal_handlers_unblock_by_func
		(G_OBJECT (preview),
		 G_CALLBACK (on_preview_expose_event),
		 NULL);

	gdk_flush ();

}

static gboolean
gimme_file (const char *filename)
{
	FILE *fp;

	fp = nibble_on_file (filename);
	if (fp == NULL)
		return FALSE;

	/* if there is a temporary in the works
	 * gimme it */
	if (temporary_file != NULL)
		ensure_temporary ();

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
		return save_to_file (fp, filename, TRUE);
	}
}

void
on_ok_button_clicked (GtkWidget *widget,
		      gpointer   data)
{
	GtkWidget *button;
	gchar *file;

	setup_busy (TRUE);

	button = glade_xml_get_widget (xml, "save_rbutton");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
		GtkWidget  *entry;
		GtkWidget  *fileentry;
		const char *tmp;

		entry = glade_xml_get_widget (xml, "save_entry");
		fileentry = glade_xml_get_widget (xml, "save_fileentry");
 		tmp = gtk_entry_get_text (GTK_ENTRY (entry));
 		file = g_filename_from_utf8 (tmp, -1, NULL, NULL, NULL);
 		if (gimme_file (file)) {
			gnome_entry_prepend_history (GNOME_ENTRY (gnome_file_entry_gnome_entry (GNOME_FILE_ENTRY (fileentry))),
						     TRUE, gtk_entry_get_text (GTK_ENTRY (entry)));
			gtk_main_quit ();
		}
		g_free (file);
		setup_busy (FALSE);

		return;
	}

	button = glade_xml_get_widget (xml, "desktop_rbutton");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
		file = add_file_to_path (desktop_dir);
		if (gimme_file (file)) {
			gtk_main_quit ();
		}
		g_free (file);
		setup_busy (FALSE);
		return;
	}
#ifdef HAVE_GNOME_PRINT
	button = glade_xml_get_widget (xml, "print_rbutton");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
		if (print_pixbuf ()) {
			gtk_main_quit ();
		}
		setup_busy (FALSE);
		return;
	}
#endif

	file = add_file_to_path (web_dir);
	if ( ! gimme_file (file)) {
		g_free (file);
		setup_busy (FALSE);
		return;
	}

	g_free (file);
	gtk_main_quit ();

	setup_busy (FALSE);
}

void
on_cancel_button_clicked (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

void
on_help_button_clicked (GtkWidget *widget, gpointer data) 
{
	
	GError *error = NULL;

        gnome_help_display_desktop (NULL, "user-guide", 
				    "wgoseditmainmenu.xml", "goseditmainmenu-53", 
				    &error);
	
	if (error) {
		GtkWidget *dialog;

                dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_ERROR,
                        GTK_BUTTONS_OK,
                        _("There was an error displaying help: \n%s"),
                        error->message);

                g_signal_connect (G_OBJECT (dialog),
                        "response",
                        G_CALLBACK (gtk_widget_destroy), NULL);
                gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
                gtk_widget_show (dialog);
                g_error_free (error);

        }

}

int
on_save_entry_key_press_event (GtkWidget    	*widget, 
			       GdkEventKey	*key)
{
	if (key->keyval == GDK_Return)
		on_ok_button_clicked (widget, NULL);
	
	return FALSE;
}

int
on_toplevel_key_press_event (GtkWidget *widget,
			     GdkEventKey *key)
{
	if (key->keyval != GDK_Escape)
		return FALSE;

	gtk_main_quit ();
	return TRUE;
}

/* This function is partly stolen from eel, it was written by John Harper */
static Window
find_toplevel_window (int depth, Window xid, gboolean *keep_going)
{
	static Atom wm_state = 0;

	Atom actual_type;
	int actual_format;
	gulong nitems, bytes_after;
	gulong *prop;

	Window root, parent, *children, window;
	int nchildren, i;

	if (wm_state == 0) {
		wm_state = XInternAtom (GDK_DISPLAY (), "WM_STATE", False);
	}

	/* Check if the window is a top-level client window.
	 * Windows will have a WM_STATE property iff they're top-level.
	 */
	if (XGetWindowProperty (GDK_DISPLAY (), xid, wm_state, 0, 1,
				False, AnyPropertyType, &actual_type,
				&actual_format, &nitems, &bytes_after,
				(guchar **) &prop) == Success
	    && prop != NULL && actual_format == 32 && prop[0] == NormalState)
	{
		/* Found a top-level window */

		if (prop != NULL) {
			XFree (prop);
		}

		*keep_going = FALSE;

		return xid;
	}

	/* Not found a top-level window yet, so keep recursing. */
	if (depth < MAXIMUM_WM_REPARENTING_DEPTH) {
		if (XQueryTree (GDK_DISPLAY (), xid, &root,
				&parent, &children, &nchildren) != 0)
		{
			window = 0;

			for (i = 0; *keep_going && i < nchildren; i++) {
				window = find_toplevel_window (depth + 1,
							       children[i],
							       keep_going);
			}

			if (children != NULL) {
				XFree (children);
			}

			if (! *keep_going) {
				return window;
			}
		}
	}

	return 0;
}

static gboolean
take_window_shot (void)
{
	GdkWindow *window, *toplevel_window;
	Display *disp;
	Window w, root, child, toplevel;
	int unused;
	guint mask;
	gint x_orig, y_orig;
	gint x = 0, y = 0;
	gint width, height;
	XClassHint class_hint;
	gchar *result = NULL;
	gboolean keep_going;

#ifdef HAVE_X11_EXTENSIONS_SHAPE_H
	XRectangle *rectangles;
	GdkPixbuf *tmp;
	int rectangle_count, rectangle_order, i;
#endif

	
	disp = GDK_DISPLAY ();
	w = GDK_ROOT_WINDOW ();
	
	XQueryPointer (disp, w, &root, &child,
		       &unused,
		       &unused,
		       &unused,
		       &unused,
		       &mask);

	if (child == None) {
                window = gdk_get_default_root_window ();
	} else {

                window = gdk_window_foreign_new (child);
		if (window == NULL)
			return FALSE;

		keep_going = TRUE;

		toplevel = find_toplevel_window (0, child, &keep_going);

		/* Get the Class Hint */
		if (toplevel && (XGetClassHint (GDK_DISPLAY (), toplevel, &class_hint) != 0)) {
			if (class_hint.res_class)
				result = class_hint.res_class;

			XFree (class_hint.res_name);
		}

		/* Force window to be shown */
		toplevel_window	 = gdk_window_foreign_new (toplevel);
		gdk_window_show (toplevel_window);
	}

	gdk_drawable_get_size (window, &width, &height);
	gdk_window_get_origin (window, &x_orig, &y_orig);

	
	if (x_orig < 0) {
		x = - x_orig;
		width = width + x_orig;
		x_orig = 0;
	}
	if (y_orig < 0) {
		y = - y_orig;
		height = height + y_orig;
		y_orig = 0;
	}

	if (x_orig + width > gdk_screen_width ())
		width = gdk_screen_width () - x_orig;
	if (y_orig + height > gdk_screen_height ())
		height = gdk_screen_height () - y_orig;


#ifdef HAVE_X11_EXTENSIONS_SHAPE_H
	tmp = gdk_pixbuf_get_from_drawable (NULL, window, NULL,
					    x, y, 0, 0,
					    width, height);

	rectangles = XShapeGetRectangles (GDK_DISPLAY (), GDK_WINDOW_XWINDOW (window),
					  ShapeBounding, &rectangle_count, &rectangle_order);
	if (rectangle_count > 0) {
		screenshot = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
		gdk_pixbuf_fill (screenshot, 0);
	
		for (i = 0; i < rectangle_count; i++) {
			for (y = rectangles[i].y; y < rectangles[i].y + rectangles[i].height; y++) {
				guchar *src_pixels, *dest_pixels;
				
				src_pixels = gdk_pixbuf_get_pixels (tmp) +
					y * gdk_pixbuf_get_rowstride(tmp) +
					rectangles[i].x * (gdk_pixbuf_get_has_alpha (tmp) ? 4 : 3);
				dest_pixels = gdk_pixbuf_get_pixels (screenshot) +
					y * gdk_pixbuf_get_rowstride (screenshot) +
					rectangles[i].x * 4;
				
				for (x = rectangles[i].x; x < rectangles[i].x + rectangles[i].width; x++) {
					*dest_pixels++ = *src_pixels ++;
					*dest_pixels++ = *src_pixels ++;
					*dest_pixels++ = *src_pixels ++;
					*dest_pixels++ = 255;
				}
			}
		}
		g_object_unref (tmp);
	}
	else {
		screenshot = tmp;
	}
#else /* HAVE_X11_EXTENSIONS_SHAPE_H */
	screenshot = gdk_pixbuf_get_from_drawable (NULL, window, NULL,
						   x, y, 0, 0,
						   width, height);
#endif /* HAVE_X11_EXTENSIONS_SHAPE_H */

	class_name = result;

	return TRUE;
}

static void
take_screen_shot (void)
{
	gint width, height;

	width = gdk_screen_width ();
	height = gdk_screen_height ();

	screenshot = gdk_pixbuf_get_from_drawable (NULL, gdk_get_default_root_window (),
						   NULL, 0, 0, 0, 0,
						   width, height);
}

static void
drag_data_get (GtkWidget          *widget,
	       GdkDragContext     *context,
	       GtkSelectionData   *selection_data,
	       guint               info,
	       guint               time,
	       gpointer            data)
{
	char *string;

	if ( ! ensure_temporary ()) {
		/*FIXME: cancel the drag*/
		return;
	}

	string = g_strdup_printf ("file:%s\r\n", temporary_file);
	gtk_selection_data_set (selection_data,
				selection_data->target,
				8, string, strlen (string)+1);
	g_free (string);
}

static void
got_signal (int sig)
{
	cleanup_temporary ();
	
	/* whack thyself */
	signal (sig, SIG_DFL); 
	kill (getpid (), sig); 
}

static void
drag_begin (GtkWidget *widget, GdkDragContext *context)
{
	static GdkPixmap *pixmap;
	GdkBitmap *mask;

	gdk_pixbuf_render_pixmap_and_mask
		(preview_image, &pixmap, &mask,
		 128);
	
	gtk_drag_set_icon_pixmap
		(context, gdk_rgb_get_colormap (), pixmap, mask, 0, 0);
	
	start_temporary ();
}

/* To make sure there is only one screenshot taken at a time,
 * (Imagine key repeat for the print screen key) we hold a selection
 * until we are done taking the screenshot
 */
static GtkWidget *selection_window;

#define SELECTION_NAME "_GNOME_PANEL_SCREENSHOT"

static gboolean
get_lock (void)
{
        Atom selection_atom = gdk_x11_get_xatom_by_name (SELECTION_NAME);
	GdkCursor *cursor;
	gboolean result = FALSE;

	XGrabServer (GDK_DISPLAY ());
        if (XGetSelectionOwner (GDK_DISPLAY(), selection_atom) != None)
                goto out;

	selection_window = gtk_invisible_new ();
	gtk_widget_show (selection_window);

	if (!gtk_selection_owner_set (selection_window,
				      gdk_atom_intern (SELECTION_NAME, FALSE),
				      GDK_CURRENT_TIME)) {
		gtk_widget_destroy (selection_window);
		selection_window = NULL;
		goto out;
	}

	cursor = gdk_cursor_new (GDK_WATCH);
	gdk_pointer_grab (selection_window->window, FALSE, 0, NULL,
			  cursor, GDK_CURRENT_TIME);
	gdk_cursor_unref (cursor);

	result = TRUE;

 out:
	XUngrabServer (GDK_DISPLAY ());
	gdk_flush ();

        return result;
}

static void
release_lock (void)
{
	if (selection_window) {
		gtk_widget_destroy (selection_window);
		selection_window = NULL;
	}

	gdk_flush ();
}

/* main */
int
main (int argc, char *argv[])
{
	GtkWidget *save_entry;
	GtkWidget *frame;
	GConfClient *gconf_client;
	GnomeClient *client;
	struct stat s;
	gchar *file, *window_icon;
	gboolean window = FALSE;
	gint width, height; 
	guint delay = 0;
	gboolean normal_web_dir = TRUE;
	gchar *utf8_name;
	
	struct poptOption opts[] = {
		{"window", '\0', POPT_ARG_NONE, &window, 0, N_("Grab a window instead of the entire screen"), NULL},
		{"delay", '\0', POPT_ARG_INT, &delay, 0, N_("Take screenshot after specified delay [in seconds]"), NULL},
		{NULL, '\0', 0, NULL, 0, NULL, NULL}
	};

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
	client = gnome_master_client ();
	gnome_client_set_restart_style (client, GNOME_RESTART_NEVER);
	
	if (delay > 0) {
		sleep (delay);	
	}

	if (!get_lock ()) {
		g_printerr ("gnome-panel-screenshot is already running\n");
		exit (1);
	}
	
	if (window) {
		if ( ! take_window_shot ()) {
			release_lock ();
			exit (1);
		}
	} else {
		take_screen_shot ();
	}

	if (g_file_test ("gnome-panel-screenshot.glade", G_FILE_TEST_EXISTS)) {
		xml = glade_xml_new ("gnome-panel-screenshot.glade", NULL, NULL);
	}
	if (xml == NULL) {
		xml = glade_xml_new (GLADEDIR "/gnome-panel-screenshot.glade",
				     NULL, NULL);
	}
	if (xml == NULL) {
		GtkWidget *dialog;
		release_lock ();
		dialog = gtk_message_dialog_new
			(NULL /* parent */,
			 0 /* flags */,
			 GTK_MESSAGE_ERROR,
			 GTK_BUTTONS_OK,
			 _("Glade file for the screenshot program is missing.\n"
			   "Please check your installation of gnome-panel"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		exit (1);
	}
	glade_xml_signal_autoconnect (xml);

#ifndef HAVE_GNOME_PRINT
	{
		GtkWidget *button = glade_xml_get_widget (xml, "print_rbutton");
		if (button != NULL)
			gtk_widget_hide (button);
	}
#endif

	if (screenshot == NULL) {
		GtkWidget *dialog;
		release_lock ();
		dialog = gtk_message_dialog_new
			(NULL /* parent */,
			 0 /* flags */,
			 GTK_MESSAGE_ERROR,
			 GTK_BUTTONS_OK,
			_("Unable to take a screenshot of "
			   "the current desktop."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		exit (1);
	}

	width = gdk_pixbuf_get_width (screenshot);
	height = gdk_pixbuf_get_height (screenshot);

	width /= 5;
	height /= 5;

	toplevel = glade_xml_get_widget (xml, "toplevel");
	frame = glade_xml_get_widget (xml, "aspect_frame");
	preview = glade_xml_get_widget (xml, "preview");
	save_entry = glade_xml_get_widget (xml, "save_entry");

	window_icon = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
						 "gnome-screenshot.png", TRUE, NULL);
	if (window_icon) {
		gnome_window_icon_set_from_file (GTK_WINDOW (toplevel), window_icon);
		g_free (window_icon);
	}

	
	gtk_window_set_default_size (GTK_WINDOW (toplevel), width * 2, -1);
	gtk_widget_set_size_request (preview, width, height);
	gtk_aspect_frame_set (GTK_ASPECT_FRAME (frame), 0.0, 0.5,
			      gdk_pixbuf_get_width (screenshot)/
			      (gfloat) gdk_pixbuf_get_height (screenshot),
			      FALSE);
	if (window)
		gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

	gconf_client = gconf_client_get_default ();

	home_dir = g_get_home_dir ();	
	web_dir = gconf_client_get_string (gconf_client, "/apps/gnome_panel_screenshot/web_dir", NULL);
	if (!web_dir || !web_dir[0]) {
		g_free (web_dir);
		web_dir = g_strconcat (home_dir, G_DIR_SEPARATOR_S, "public_html", NULL);
	} else
		normal_web_dir = FALSE;

	if (gconf_client_get_bool (gconf_client, "/apps/nautilus/preferences/desktop_is_home_dir", NULL))
		desktop_dir = g_strdup (home_dir);
	else
		desktop_dir = g_strconcat (home_dir, G_DIR_SEPARATOR_S,
					   "Desktop", NULL);
	g_object_unref (gconf_client);
	
	file = add_file_to_path (home_dir);
	utf8_name = g_filename_to_utf8 (file, -1, NULL, NULL, NULL);
	gtk_entry_set_text (GTK_ENTRY (save_entry), utf8_name);
	g_free (file);
	g_free (utf8_name);

	if (!stat (web_dir, &s) && S_ISDIR (s.st_mode)) {
		GtkWidget *cbutton;
		char      *str = NULL;		

		cbutton = glade_xml_get_widget (xml, "web_rbutton");
		gtk_widget_show (cbutton);

		if (!normal_web_dir)
			str = g_strdup_printf ("Save screenshot to _web page (save in %s)",
					       web_dir);
		gtk_button_set_label (GTK_BUTTON (cbutton), str);
		g_free (str);
	}

	/* setup dnd */
	/* just in case some wanker like nautilus took our image */
	g_signal_connect (G_OBJECT (preview), "drag_begin",
			  G_CALLBACK (drag_begin), NULL);
	g_signal_connect (G_OBJECT (preview), "drag_data_get",
			  G_CALLBACK (drag_data_get), NULL);
	gtk_drag_source_set (preview,
			     GDK_BUTTON1_MASK|GDK_BUTTON3_MASK,
			     drag_types, 2,
			     GDK_ACTION_COPY);

	gtk_widget_grab_focus (save_entry);
	gtk_editable_select_region (GTK_EDITABLE (save_entry), 0, -1);
	g_signal_connect (G_OBJECT (save_entry), "key_press_event",
			  G_CALLBACK (on_save_entry_key_press_event),
			  NULL);

	gtk_widget_show_now (toplevel);
	release_lock ();

	/*
	 * Start working on the temporary file in a fork, now this is
	 * a little evil since we might save a file the user might cancel
	 * and we'll jsut end up deleting it and/or killing the forked
	 * process.  But it makes it snappy and makes dnd not hang.  Go
	 * figure.
	 */
	start_temporary ();

	signal (SIGINT, got_signal);
	signal (SIGTERM, got_signal);

	gtk_main ();

	cleanup_temporary ();

	return 0;
}
