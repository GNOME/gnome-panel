/*
 * GNOME panel utils
 * (C) 1997, 1998, 1999, 2000 The Free Software Foundation
 * Copyright 2000 Helix Code, Inc.
 * Copyright 2000,2001 Eazel, Inc.
 * Copyright 2001 George Lebl
 * Copyright 2002 Sun Microsystems Inc.
 *
 * Authors: George Lebl
 *          Jacob Berkman
 *          Mark McLoughlin
 */

#include <config.h>
#include <string.h>
#include <glib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>

/* includes for the battery stuff */
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>

#include <libgnomeui/libgnomeui.h>
#include <libgnome/libgnome.h>

#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>

#include "panel-util.h"

#include "applet.h"
#include "nothing.h"
#include "panel.h"
#include "panel-a11y.h"
#include "xstuff.h"
#include "panel-globals.h"
#include "launcher.h"

int
panel_ditem_launch (const GnomeDesktopItem       *item,
		    GList                        *file_list,
		    GnomeDesktopItemLaunchFlags   flags,
		    GdkScreen                    *screen,
		    GError                      **error)
{
	int workspace;

	workspace = xstuff_get_current_workspace (screen);

	return gnome_desktop_item_launch_on_screen (
			item, file_list, flags, screen, workspace, error);
}

void
panel_show_help (GdkScreen  *screen,
		 const char *doc_name,
		 const char *linkid)
{
	GError *error = NULL;

	if (!gnome_help_display_desktop_on_screen (NULL, "user-guide", doc_name, linkid, screen, &error)) {
		panel_error_dialog (
			screen,
			"cannot_show_help",
			_("Cannot display help document"),
			"%s",
			error != NULL ? error->message : "");

		g_clear_error (&error);
	}
}

GList *
panel_g_list_insert_before (GList *list,
			    GList *sibling,
			    GList *link)
{
	if (!list) {
		g_return_val_if_fail (sibling == NULL, list);
		return link;
	} else if (sibling) {
		if (sibling->prev) {
			link->prev = sibling->prev;
			link->prev->next = link;
			link->next = sibling;
			sibling->prev = link;
			return list;
		} else {
			link->next = sibling;
			sibling->prev = link;
			g_return_val_if_fail (sibling == list, link);
			return link;
		}
	} else {
		GList *last;

		last = list;
		while (last->next)
			last = last->next;

		last->next = link;
		link->prev = last;
		return list;
	}
}

GList *
panel_g_list_insert_after (GList *list,
			   GList *sibling,
			   GList *link)
{
	if (!list) {
		g_return_val_if_fail (sibling == NULL, link);
		return link;
	} else if (sibling) {
		if (sibling->next) {
			link->next = sibling->next;
			link->next->prev = link;
			link->prev = sibling;
			sibling->next = link;
			return list;
		} else {
			sibling->next = link;
			link->prev = sibling;
			return list;
		}
			
	} else {
		link->next = list;
		list->prev = link;
		return link;
	}
}

GList *
panel_g_list_swap_next (GList *list, GList *dl)
{
	GList *t;

	if(!dl->next)
		return list;
	if(dl->prev)
		dl->prev->next = dl->next;
	t = dl->prev;
	dl->prev = dl->next;
	dl->next->prev = t;
	if(dl->next->next)
		dl->next->next->prev = dl;
	t = dl->next->next;
	dl->next->next = dl;
	dl->next = t;

	if(list == dl)
		return dl->prev;
	return list;
}

GList *
panel_g_list_swap_prev (GList *list, GList *dl)
{
	GList *t;

	if(!dl->prev)
		return list;
	if(dl->next)
		dl->next->prev = dl->prev;
	t = dl->next;
	dl->next = dl->prev;
	dl->prev->next = t;
	if(dl->prev->prev)
		dl->prev->prev->next = dl;
	t = dl->prev->prev;
	dl->prev->prev = dl;
	dl->prev = t;

	if(list == dl->next)
		return dl;
	return list;
}

/*maybe this should be a glib function?
 it resorts a single item in the list*/
GList *
panel_g_list_resort_item(GList *list, gpointer data, GCompareFunc func)
{
	GList *dl;

	if(!list)
		return NULL;

	dl = g_list_find(list,data);

	g_return_val_if_fail(dl!=NULL,list);

	while(dl->next &&
	      (*func)(dl->data,dl->next->data)>0)
		list = panel_g_list_swap_next (list, dl);
	while(dl->prev &&
	      (*func)(dl->data,dl->prev->data)<0)
		list = panel_g_list_swap_prev (list, dl);
	return list;
}

static GtkWidget *
panel_dialog (GdkScreen  *screen,
	      int         type,
	      const char *class,
	      const char *str)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
			NULL, 0, type, GTK_BUTTONS_OK,
			/* No need to translate this, this should NEVER happen */
			"Error with displaying error for dialog of class %s", class);
	gtk_widget_add_events (dialog, GDK_KEY_PRESS_MASK);
	g_signal_connect (dialog, "event",
			  G_CALLBACK (panel_dialog_window_event), NULL);
	gtk_label_set_markup (
		GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), str);

	gtk_window_set_wmclass (GTK_WINDOW (dialog), class, "Panel");
	gtk_window_set_screen (GTK_WINDOW (dialog), screen);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	gtk_widget_show_all (dialog);

	/* FIXME: this is ugly and makes it bad to run gtk_dialog_run
	 * after this function */
	g_signal_connect_swapped (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  G_OBJECT (dialog));

	return dialog;
}

GtkWidget *
panel_error_dialog (GdkScreen  *screen,
		    const char *class,
		    const char *primary_format,
		    const char *secondary_format,
		    ...)
{
	GtkWidget *w;
	char *sec = NULL, *format, *s;
	va_list ap;

	if (primary_format == NULL) {
		g_warning ("NULL dialog");
		s = g_strdup ("(null)");
	} else {
		if (secondary_format == NULL)
			format = (char *)primary_format;
		else {
			sec = g_strdup_printf (_("Details: %s"), secondary_format);

			format = g_strdup_printf ("<b>%s</b>\n\n%s",
						  primary_format,
						  sec);

			g_free (sec);
		}

		va_start (ap, secondary_format);
		s = g_strdup_vprintf (format, ap);
		va_end (ap);

		if (format != primary_format)
			g_free (format);
	}

	w = panel_dialog (screen, GTK_MESSAGE_ERROR, class, s);
	g_free (s);
	return w;
}

int
panel_find_applet_index (GtkWidget *widget)
{
	GSList *applet_list, *l;
	int     i;

	applet_list = panel_applet_list_applets ();

	for (i = 0, l = applet_list; l; i++, l = l->next) {
		AppletInfo *info = l->data;

		if (info->widget == widget)
			return i;
	}

	return i;
}

void
panel_push_window_busy (GtkWidget *window)
{
	int busy = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window),
						       "Panel:WindowBusy"));

	busy ++;

	if (busy == 1) {
		gtk_widget_set_sensitive (window, FALSE);
		if (window->window != NULL) {
			GdkCursor *cursor = gdk_cursor_new (GDK_WATCH);
			gdk_window_set_cursor (window->window, cursor);
			gdk_cursor_unref (cursor);
			gdk_flush ();
		}
	}

	g_object_set_data (G_OBJECT (window), "Panel:WindowBusy",
			   GINT_TO_POINTER (busy));
}

void
panel_pop_window_busy (GtkWidget *window)
{
	int busy = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window),
						       "Panel:WindowBusy"));
	busy --;

	if (busy <= 0) {
		gtk_widget_set_sensitive (window, TRUE);
		if (window->window != NULL)
			gdk_window_set_cursor (window->window, NULL);
		g_object_set_data (G_OBJECT (window),
				   "Panel:WindowBusy", NULL);
	} else {
		g_object_set_data (G_OBJECT (window), "Panel:WindowBusy",
				   GINT_TO_POINTER (busy));
	}
}

/*
 * GnomeVFS reading utils, that look like the libc buffered io stuff
 */
struct _ReadBuf {
	GnomeVFSHandle *handle;
	char *uri;
	gboolean eof;
	char buf[BUFSIZ];
	gsize size;
	gsize pos;
};

static int
readbuf_getc (ReadBuf *rb)
{
	if (rb->eof)
		return EOF;

	if (rb->size == 0 ||
	    rb->pos == rb->size) {
		GnomeVFSFileSize bytes_read;
		/* FIXME: handle other errors */
		if (gnome_vfs_read (rb->handle,
				    rb->buf,
				    BUFSIZ,
				    &bytes_read) != GNOME_VFS_OK) {
			rb->eof = TRUE;
			return EOF;
		}
		rb->size = bytes_read;
		rb->pos = 0;

		if (rb->size == 0) {
			rb->eof = TRUE;
			return EOF;
		}
	}

	return (int)rb->buf[rb->pos++];
}

/* Note, does not include the trailing \n */
char *
readbuf_gets (char *buf, gsize bufsize, ReadBuf *rb)
{
	int c;
	gsize pos;

	g_return_val_if_fail (rb != NULL, NULL);

	pos = 0;
	buf[0] = '\0';

	do {
		c = readbuf_getc (rb);
		if (c == EOF ||
		    c == '\n')
			break;
		buf[pos++] = c;
	} while (pos < bufsize-1);

	if (c == EOF &&
	    pos == 0)
		return NULL;

	buf[pos++] = '\0';

	return buf;
}

ReadBuf *
readbuf_open (const char *uri)
{
	GnomeVFSHandle *handle;
	ReadBuf *rb;

	g_return_val_if_fail (uri != NULL, NULL);

	if (gnome_vfs_open (&handle, uri,
			    GNOME_VFS_OPEN_READ) != GNOME_VFS_OK)
		return NULL;

	rb = g_new0 (ReadBuf, 1);
	rb->handle = handle;
	rb->uri = g_strdup (uri);
	rb->eof = FALSE;
	rb->size = 0;
	rb->pos = 0;

	return rb;
}

/* unused for now */
gboolean
readbuf_rewind (ReadBuf *rb)
{
	if (gnome_vfs_seek (rb->handle,
			    GNOME_VFS_SEEK_START, 0) == GNOME_VFS_OK)
		return TRUE;

	gnome_vfs_close (rb->handle);
	rb->handle = NULL;
	if (gnome_vfs_open (&rb->handle, rb->uri,
			    GNOME_VFS_OPEN_READ) == GNOME_VFS_OK)
		return TRUE;

	return FALSE;
}

void
readbuf_close (ReadBuf *rb)
{
	if (rb->handle != NULL)
		gnome_vfs_close (rb->handle);
	rb->handle = NULL;
	g_free (rb->uri);
	rb->uri = NULL;

	g_free (rb);
}

gboolean
panel_is_program_in_path (const char *program)
{
	char *tmp = g_find_program_in_path (program);
	if (tmp != NULL) {
		g_free (tmp);
		return TRUE;
	} else {
		return FALSE;
	}
}

char *
panel_pixmap_discovery (const char *name, gboolean fallback)
{
	char *pixmap;
	pixmap = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
					    name,
					    TRUE /* only_if_exists */,
					    NULL /* ret_locations */);
	if (pixmap == NULL)
		pixmap = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP, 
						    name,
						    TRUE /* only_if_exists */,
						    NULL /* ret_locations */);
	/* just as a fallback return something */
	if (pixmap == NULL && fallback)
		pixmap = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
						    name,
						    FALSE /* only_if_exists */,
						    NULL /* ret_locations */);
	return pixmap;
}

static gboolean
panel_ensure_dir (const char *dirname)
{
	char *parsed, *p;

	if (dirname == NULL)
		return FALSE;

	parsed = g_strdup (dirname);

	if (g_file_test (parsed, G_FILE_TEST_IS_DIR)) {
		g_free (parsed);
		return TRUE;
	}

	p = strchr (parsed, '/');
	if (p == parsed)
		p = strchr (p+1, '/');

	while (p != NULL) {
		*p = '\0';
		if (mkdir (parsed, 0700) != 0 &&
		    errno != EEXIST && errno != ENOSYS) {
			g_free (parsed);
			return FALSE;
		}
		*p = '/';
		p = strchr (p+1, '/');
	}

	if (mkdir (parsed, 0700) != 0 &&
	    errno != EEXIST && errno != ENOSYS) {
		g_free (parsed);
		return FALSE;
	}

	g_free (parsed);
	return TRUE;
}

void
panel_g_slist_deep_free	(GSList *list)
{
	GSList *li;
	for (li = list; li != NULL; li = li->next) {
		g_free (li->data);
		li->data = NULL;
	}
	g_slist_free (list);
}

static gboolean
internal_panel_is_uri_writable (const char *uri, gboolean recurse)
{
	GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();

	if (gnome_vfs_get_file_info
	    (uri, info, GNOME_VFS_FILE_INFO_DEFAULT) != GNOME_VFS_OK) {
		char *dir;
		gboolean ret;

		gnome_vfs_file_info_unref (info);

		if ( ! recurse)
			return FALSE;

		dir = g_path_get_dirname (uri);
		ret = internal_panel_is_uri_writable (dir, FALSE);
		g_free (dir);

		return ret;
	}

	if ( ! (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS)) {
		gnome_vfs_file_info_unref (info);
		/* assume writable, if no permissions */
		return TRUE;
	} 

	if (info->permissions & GNOME_VFS_PERM_OTHER_WRITE) {
		gnome_vfs_file_info_unref (info);
		return TRUE;
	}

	if (info->gid == getgid () &&
	    info->permissions & GNOME_VFS_PERM_GROUP_WRITE) {
		gnome_vfs_file_info_unref (info);
		return TRUE;
	}

	if (info->uid == getuid () &&
	    info->permissions & GNOME_VFS_PERM_USER_WRITE) {
		gnome_vfs_file_info_unref (info);
		return TRUE;
	}

	if (info->gid == getgid () &&
	    info->permissions & GNOME_VFS_PERM_GROUP_WRITE) {
		gnome_vfs_file_info_unref (info);
		return TRUE;
	}

	if (info->permissions & GNOME_VFS_PERM_GROUP_WRITE) {
		gid_t *groups;
		int i, n;

		/* get size */
		n = getgroups (0, NULL);

		if (n == 0) {
			/* no more groups */
			gnome_vfs_file_info_unref (info);
			return FALSE;
		}

		groups = g_new0 (gid_t, n);

		n = getgroups (n, groups);
		for (i = 0; i < n; i++) {
			if (info->gid == groups[i]) {
				/* ok */
				gnome_vfs_file_info_unref (info);
				return TRUE;
			}
		}
	}

	/* no more perimission stuff to try */
	gnome_vfs_file_info_unref (info);
	return FALSE;
}

gboolean
panel_is_uri_writable (const char *uri)
{
	g_return_val_if_fail (uri != NULL, FALSE);

	return internal_panel_is_uri_writable (uri, TRUE /* recurse */);
}

gboolean
panel_uri_exists (const char *uri)
{
	GnomeVFSURI *suri;
	gboolean ret;

	g_return_val_if_fail (uri != NULL, FALSE);

	suri = gnome_vfs_uri_new (uri);

	if (!suri) {
		return FALSE;
	}

	ret = gnome_vfs_uri_exists (suri);

	gnome_vfs_uri_unref (suri);

	return ret;
}

GdkPixbuf *
missing_pixbuf (int size)
{
	int i, j;
	int rowstride;
	guchar *pixels;
	GdkPixbuf *pb;

	pb = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			     FALSE /* has alpha */,
			     8 /* bits per sample */,
			     size, size);
	pixels = gdk_pixbuf_get_pixels (pb);
	rowstride = gdk_pixbuf_get_rowstride (pb);

	for (i = 0; i < size; i++) {
		for (j = 0; j < size; j++) {
			guchar *p = pixels + i * rowstride + 3 * j;
			if (i == j || size - i == j) {
				p[0] = 255;
				p[1] = 0;
				p[2] = 0;
			} else {
				p[0] = 255;
				p[1] = 255;
				p[2] = 255;
			}
		}
	}

	return pb;
}

void
panel_lock_screen (GdkScreen *screen)
{
#define XSCREENSAVER_LOCK_COMMAND "xscreensaver-command -lock"

	GError *error = NULL;

	g_return_if_fail (GDK_IS_SCREEN (screen));

	if (!gdk_spawn_command_line_on_screen (screen, XSCREENSAVER_LOCK_COMMAND, &error)) {
		panel_error_dialog (screen,
				    "cannot_exec_xscreensaver",
				    _("Cannot execute '%s'"),
				    "%s",
				    XSCREENSAVER_LOCK_COMMAND,
				    error->message);
		g_error_free (error);
	}
	
#undef XSCREENSAVER_LOCK_COMMAND
}


char *
panel_make_full_path (const char *dir,
		      const char *filename)
{
	char *retval;
	char *freeme = NULL;

	g_return_val_if_fail (filename != NULL, NULL);

	if (!dir) {
		freeme = gnome_util_home_file (PANEL_LAUNCHERS_PATH);
		dir = freeme;
	}

	/* Make sure the launcher directory exists */
	if (!g_file_test (dir, G_FILE_TEST_EXISTS))
		panel_ensure_dir (dir);

	retval = g_build_filename (dir, filename, NULL);

	g_free (freeme);

	return retval;
}

char *
panel_make_unique_uri (const char *dir,
		       const char *suffix)
{
#define NUM_OF_WORDS 12
	char *words[] = {
		"foo",
		"bar",
		"blah",
		"gegl",
		"frobate",
		"hadjaha",
		"greasy",
		"hammer",
		"eek",
		"larry",
		"curly",
		"moe",
		NULL};
	char     *uri;
	char     *path = NULL;
	gboolean  exists = TRUE;

	while (exists) {
		char *filename;
		int   rnd;
		int   word;

		rnd = g_random_int ();
		word = g_random_int () % NUM_OF_WORDS;

		filename = g_strdup_printf ("%s-%010x%s",
					    words [word],
					    (guint) rnd,
					    suffix);

		g_free (path);
		path = panel_make_full_path (dir, filename);
		exists = g_file_test (path, G_FILE_TEST_EXISTS);
		g_free (filename);
	}

	uri = gnome_vfs_get_uri_from_local_path (path);
	g_free (path);

	return uri;

#undef NUM_OF_WORDS
}
