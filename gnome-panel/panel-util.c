/*
 * GNOME panel utils
 * (C) 1997, 1998, 1999, 2000 The Free Software Foundation
 * Copyright 2000 Helix Code, Inc.
 * Copyright 2000,2001 Eazel, Inc.
 * Copyright 2001 George Lebl
 *
 * Authors: George Lebl
 *          Jacob Berkman
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

#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "panel-util.h"

#include "applet.h"
#include "nothing.h"
#include "panel-config-global.h"

extern GlobalConfig global_config;

extern GSList *applets;

void
panel_show_help (const char *doc_name, const char *linkid)
{
	gnome_help_display (doc_name,
			    linkid,
			    NULL);

	/* FIXME: handle error */
}

#ifdef FIXME
static char *
panel_gnome_help_path (const char *docpath)
{
	char *fullpath, *app, *p, *path, *uri;

	app = g_strdup (docpath);

	p = strchr (app, '/');

	if (p == NULL) {
		g_free (app);
		return NULL;
	}

	path = p+1;
	*p = '\0';

	fullpath = gnome_help_file_path (app, path);

	g_free (app);

	if ( ! g_file_test (fullpath, G_FILE_TEST_EXISTS)) {
		g_free (fullpath);
		fullpath = NULL;
	}

	uri = g_strconcat ("ghelp:", fullpath, NULL);
	g_free (fullpath);

	return uri;
}
#endif /* FIXME */

#ifdef FIXME
static char *
panel_kde_help_path (const char *docpath)
{
	const GList *li;

	if ( ! g_file_test (KDE_DOCDIR, G_FILE_TEST_EXISTS))
		return NULL;

	for (li = gnome_i18n_get_language_list ("LC_MESSAGES");
	     li != NULL;
	     li = li->next) {
		char *fullpath = g_strdup_printf ("%s/HTML/%s/%s",
						  KDE_DOCDIR,
						  (char *)li->data,
						  docpath);
		if (g_file_test (fullpath, G_FILE_TEST_EXISTS)) {
			char *uri = g_strconcat ("ghelp:", fullpath, NULL);
			g_free (fullpath);
			return uri;
		}
		g_free (fullpath);
	}

	return NULL;
}
#endif /* FIXME */

char *
panel_gnome_kde_help_path (const char *docpath)
{
#ifdef FIXME
	char *path;

	if (string_empty (docpath))
		return NULL;

	if (panel_is_url (docpath))
		return g_strdup (docpath);

	path = panel_gnome_help_path (docpath);

	if (path == NULL)
		path = panel_kde_help_path (docpath);

	return path;
#else
	return NULL;
#endif
}

static void
updated (GtkWidget *w, gpointer data)
{
	UpdateFunction func = g_object_get_data (G_OBJECT (w), "update_function");

	func (data);
}

GtkWidget *
create_text_entry(GtkWidget *table,
		  const char *history_id,
		  int row,
		  const char *label,
		  const char *text,
		  UpdateFunction func,
		  gpointer data)
{
	GtkWidget *wlabel;
	GtkWidget *entry;
	GtkWidget *t;

	wlabel = gtk_label_new(label);
	gtk_misc_set_alignment(GTK_MISC(wlabel), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), wlabel,
			 0, 1, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 GNOME_PAD_SMALL, GNOME_PAD_SMALL);
	gtk_widget_show(wlabel);

	entry = gnome_entry_new(history_id);
	t = gnome_entry_gtk_entry (GNOME_ENTRY (entry));
	if (text)
		gtk_entry_set_text(GTK_ENTRY(t), text);
	gtk_table_attach(GTK_TABLE(table), entry,
			 1, 2, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 GNOME_PAD_SMALL, GNOME_PAD_SMALL);

	if(func) {
		g_object_set_data (G_OBJECT (t), "update_function", func);
		g_signal_connect (G_OBJECT (t), "changed",
				  G_CALLBACK (updated), 
				  data);
	}
	return entry;
}

GtkWidget *
create_icon_entry(GtkWidget *table,
		  const char *history_id,
		  int cols, int cole,
		  const char *label,
		  const char *subdir,
		  const char *text,
		  UpdateFunction func,
		  gpointer data)
{
	GtkWidget *wlabel;
	GtkWidget *entry;

	wlabel = gtk_label_new(label);
	gtk_misc_set_alignment(GTK_MISC(wlabel), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), wlabel,
			 cols, cole, 2, 3,
			 GTK_SHRINK,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GNOME_PAD_SMALL, GNOME_PAD_SMALL);
	gtk_widget_show(wlabel);

	entry = gnome_icon_entry_new (history_id, _("Browse"));

	gnome_icon_entry_set_pixmap_subdir (GNOME_ICON_ENTRY(entry), subdir);
	if (text != NULL)
		gnome_icon_entry_set_filename (GNOME_ICON_ENTRY(entry), text);

	gtk_table_attach(GTK_TABLE(table), entry,
			 cols, cole, 1, 2,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 GNOME_PAD_SMALL, GNOME_PAD_SMALL);

	if(func) {
		g_object_set_data (G_OBJECT (entry), "update_function", func);
		g_signal_connect (G_OBJECT (entry), "changed",
				  G_CALLBACK (updated), 
				  data);
	}

	return entry;
}

GList *
my_g_list_swap_next (GList *list, GList *dl)
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
my_g_list_swap_prev (GList *list, GList *dl)
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
my_g_list_resort_item(GList *list, gpointer data, GCompareFunc func)
{
	GList *dl;

	if(!list)
		return NULL;

	dl = g_list_find(list,data);

	g_return_val_if_fail(dl!=NULL,list);

	while(dl->next &&
	      (*func)(dl->data,dl->next->data)>0)
		list=my_g_list_swap_next(list,dl);
	while(dl->prev &&
	      (*func)(dl->data,dl->prev->data)<0)
		list=my_g_list_swap_prev(list,dl);
	return list;
}

/*following code shamelessly stolen from gtk*/
static void
rgb_to_hls (gdouble *r,
	    gdouble *g,
	    gdouble *b)
{
  gdouble min;
  gdouble max;
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble h, l, s;
  gdouble delta;

  red = *r;
  green = *g;
  blue = *b;

  if (red > green)
    {
      if (red > blue)
	max = red;
      else
	max = blue;

      if (green < blue)
	min = green;
      else
	min = blue;
    }
  else
    {
      if (green > blue)
	max = green;
      else
	max = blue;

      if (red < blue)
	min = red;
      else
	min = blue;
    }

  l = (max + min) / 2;
  s = 0;
  h = 0;

  if (max != min)
    {
      if (l <= 0.5)
	s = (max - min) / (max + min);
      else
	s = (max - min) / (2 - max - min);

      delta = max -min;
      if (red == max)
	h = (green - blue) / delta;
      else if (green == max)
	h = 2 + (blue - red) / delta;
      else if (blue == max)
	h = 4 + (red - green) / delta;

      h *= 60;
      if (h < 0.0)
	h += 360;
    }

  *r = h;
  *g = l;
  *b = s;
}

static void
hls_to_rgb (gdouble *h,
	    gdouble *l,
	    gdouble *s)
{
  gdouble hue;
  gdouble lightness;
  gdouble saturation;
  gdouble m1, m2;
  gdouble r, g, b;

  lightness = *l;
  saturation = *s;

  if (lightness <= 0.5)
    m2 = lightness * (1 + saturation);
  else
    m2 = lightness + saturation - lightness * saturation;
  m1 = 2 * lightness - m2;

  if (saturation == 0)
    {
      *h = lightness;
      *l = lightness;
      *s = lightness;
    }
  else
    {
      hue = *h + 120;
      while (hue > 360)
	hue -= 360;
      while (hue < 0)
	hue += 360;

      if (hue < 60)
	r = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
	r = m2;
      else if (hue < 240)
	r = m1 + (m2 - m1) * (240 - hue) / 60;
      else
	r = m1;

      hue = *h;
      while (hue > 360)
	hue -= 360;
      while (hue < 0)
	hue += 360;

      if (hue < 60)
	g = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
	g = m2;
      else if (hue < 240)
	g = m1 + (m2 - m1) * (240 - hue) / 60;
      else
	g = m1;

      hue = *h - 120;
      while (hue > 360)
	hue -= 360;
      while (hue < 0)
	hue += 360;

      if (hue < 60)
	b = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
	b = m2;
      else if (hue < 240)
	b = m1 + (m2 - m1) * (240 - hue) / 60;
      else
	b = m1;

      *h = r;
      *l = g;
      *s = b;
    }
}

static void
gtk_style_shade (GdkColor *a,
		 GdkColor *b,
		 gdouble   k)
{
  gdouble red;
  gdouble green;
  gdouble blue;

  red = (gdouble) a->red / 65535.0;
  green = (gdouble) a->green / 65535.0;
  blue = (gdouble) a->blue / 65535.0;

  rgb_to_hls (&red, &green, &blue);

  green *= k;
  if (green > 1.0)
    green = 1.0;
  else if (green < 0.0)
    green = 0.0;

  blue *= k;
  if (blue > 1.0)
    blue = 1.0;
  else if (blue < 0.0)
    blue = 0.0;

  hls_to_rgb (&red, &green, &blue);

  b->red = red * 65535.0;
  b->green = green * 65535.0;
  b->blue = blue * 65535.0;
}

#define LIGHTNESS_MULT  1.3
#define DARKNESS_MULT   0.7

static void
set_color_back (GtkWidget *widget, PanelWidget *panel)
{
	GtkStyle *ns;
	int i;

	gtk_widget_set_style (widget, NULL);
	ns = gtk_style_copy (gtk_widget_get_style (widget));

	ns->bg[GTK_STATE_NORMAL] =
		panel->back_color;
	gtk_style_shade (&panel->back_color,
			 &ns->bg[GTK_STATE_PRELIGHT],1.5);
	gtk_style_shade (&panel->back_color,
			 &ns->bg[GTK_STATE_ACTIVE],0.8);
	ns->bg[GTK_STATE_INSENSITIVE] = 
		panel->back_color;

	for (i = 0; i < 5; i++) {
		gtk_style_shade (&ns->bg[i], &ns->light[i], LIGHTNESS_MULT);
		gtk_style_shade (&ns->bg[i], &ns->dark[i], DARKNESS_MULT);

		ns->mid[i].red = (ns->light[i].red + ns->dark[i].red) / 2;
		ns->mid[i].green = (ns->light[i].green + ns->dark[i].green) / 2;
		ns->mid[i].blue = (ns->light[i].blue + ns->dark[i].blue) / 2;
	}
	gtk_widget_set_style (widget, ns);
	g_object_unref (G_OBJECT (ns));
}

void
set_frame_colors (PanelWidget *panel, GtkWidget *frame,
		  GtkWidget *but1, GtkWidget *but2,
		  GtkWidget *but3, GtkWidget *but4)
{
	if (panel->back_type == PANEL_BACK_COLOR) {
		set_color_back (frame, panel);
		set_color_back (but1, panel);
		set_color_back (but2, panel);
		set_color_back (but3, panel);
		set_color_back (but4, panel);
	} else {
		gtk_widget_set_style (frame, NULL);
		gtk_widget_set_style (but1, NULL);
		gtk_widget_set_style (but2, NULL);
		gtk_widget_set_style (but3, NULL);
		gtk_widget_set_style (but4, NULL);
	}
}


void
remove_directory(const char *dirname, gboolean just_clean)
{
	DIR *dir;
	struct dirent *dent;
	char *oldcwd;

	dir = opendir (dirname);
	if(!dir) return;
	oldcwd = g_get_current_dir();

	chdir(dirname);
	while((dent = readdir (dir)) != NULL) {
		if(strcmp(dent->d_name,".")==0 ||
		   strcmp(dent->d_name,"..")==0)
			continue;
		if(g_file_test(dent->d_name,G_FILE_TEST_IS_DIR))
			remove_directory(dent->d_name, FALSE);
		else
			unlink(dent->d_name);
	}
	closedir(dir);
	chdir(oldcwd);

	if(!just_clean)
		rmdir(dirname);
	g_free(oldcwd);
}

gboolean
convert_string_to_keysym_state(const char *string,
			       guint *keysym,
			       GdkModifierType *state)
{
	g_return_val_if_fail (keysym != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	
	*state = 0;
	*keysym = 0;

	if (string_empty (string) ||
	    strcmp (string, "Disabled") == 0 ||
	    strcmp (string, _("Disabled")) == 0)
		return FALSE;

	gtk_accelerator_parse (string, keysym, state);

	if (*keysym == 0)
		return FALSE;

	return TRUE;
}

char *
convert_keysym_state_to_string(guint keysym,
			       GdkModifierType state)
{
	if (keysym == 0)
		return g_strdup (_("Disabled"));

	return gtk_accelerator_name (keysym, state);
}

GtkWidget *
panel_error_dialog (const char *class,
		    const char *format,
		    ...)
{
	GtkWidget *w;
	char *s;
	va_list ap;

	if (format == NULL) {
		g_warning ("NULL error dialog");
		s = g_strdup ("(null)");
	} else {
		va_start (ap, format);
		s = g_strdup_vprintf (format, ap);
		va_end (ap);
	}

	w = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR,
				    GTK_BUTTONS_OK, s);
	gtk_widget_add_events (w, GDK_KEY_PRESS_MASK);
	g_signal_connect (G_OBJECT (w), "event",
			  G_CALLBACK (panel_dialog_window_event), NULL);
	gtk_window_set_wmclass (GTK_WINDOW (w),
				class, "Panel");
	g_free (s);

	gtk_widget_show_all (w);

	g_signal_connect_swapped (G_OBJECT (w), "response",
				   G_CALLBACK (gtk_widget_destroy),
				   G_OBJECT (w));

	return w;
}

GtkWidget *
panel_info_dialog (const char *class,
		   const char *format,
		   ...)
{
	GtkWidget *w;
	char *s;
	va_list ap;

	if (format == NULL) {
		g_warning ("NULL info dialog");
		s = g_strdup ("(null)");
	} else {
		va_start (ap, format);
		s = g_strdup_vprintf (format, ap);
		va_end (ap);
	}

	w = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_INFO,
				    GTK_BUTTONS_OK, s);
	g_signal_connect (G_OBJECT (w), "event",
			  G_CALLBACK (panel_dialog_window_event), NULL);
	gtk_window_set_wmclass (GTK_WINDOW (w),
				class, "Panel");
	g_free (s);

	gtk_widget_show_all (w);

	g_signal_connect_swapped (G_OBJECT (w), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  G_OBJECT (w));

	return w;
}

gboolean
string_empty (const char *string)
{
	if (string == NULL ||
	    string[0] == '\0')
		return TRUE;
	else
		return FALSE;
}

gboolean
is_ext (const char *file, const char *ext)
{
	const char *p;

	if (file == NULL)
		return FALSE;

	p = strrchr (file, '.');

	if (p != NULL &&
	    strcmp (p, ext) == 0)
		return TRUE;
	else
		return FALSE;
}

gboolean
is_ext2 (const char *file,
	 const char *ext1,
	 const char *ext2)
{
	const char *p;

	if (file == NULL)
		return FALSE;

	p = strrchr (file, '.');

	if (p != NULL &&
	    (strcmp (p, ext1) == 0 ||
	     strcmp (p, ext2) == 0))
		return TRUE;
	else
		return FALSE;
}

int
find_applet (GtkWidget *widget)
{
	int i;
	GSList *li;

	for (i = 0, li = applets; li != NULL; i++, li = li->next) {
		AppletInfo *info = li->data;

		if (info->widget == widget)
			return i;
	}

	return i;
}

int
get_requisition_width (GtkWidget *widget)
{
	GtkRequisition req;

	gtk_widget_get_child_requisition (widget, &req);

	return req.width;
}

int
get_requisition_height (GtkWidget *widget)
{
	GtkRequisition req;

	gtk_widget_get_child_requisition (widget, &req);

	return req.height;
}

/* is url showable by gnome_url_show */
gboolean
panel_is_url (const char *url)
{
	if (strncmp (url, "http://", strlen ("http://")) == 0 ||
	    strncmp (url, "https://", strlen ("https://")) == 0 ||
	    strncmp (url, "gopher://", strlen ("gopher://")) == 0 ||
	    strncmp (url, "ftp://", strlen ("ftp://")) == 0 ||
	    strncmp (url, "file:", strlen ("file:")) == 0 ||
	    strncmp (url, "ghelp:", strlen ("ghelp:")) == 0 ||
	    strncmp (url, "help:", strlen ("help:")) == 0 ||
	    strncmp (url, "man:", strlen ("man:")) == 0 ||
	    strncmp (url, "info:", strlen ("info:")) == 0)
		return TRUE;
	else
		return FALSE;
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

int
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

typedef struct {
	gboolean top;
	gboolean right;
	gboolean bottom;
	gboolean left;
} StrechSides;

static void
stretch_widget_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes = {0};
	int attributes_mask;
	GtkWidget *toplevel;
	int x,y,w,h;
	GdkWindow *eventwin = g_object_get_data (G_OBJECT (widget),
						 "StrechEventWindow");
	StrechSides *sides = g_object_get_data (G_OBJECT (widget),
						"StrechSides");
	if (sides == NULL)
		return;

	if (eventwin != NULL)
		gdk_window_destroy (eventwin);

	toplevel = gtk_widget_get_toplevel (widget);

	if (toplevel == NULL ||
	    ! GTK_WIDGET_REALIZED (toplevel))
		return;
	
	gtk_widget_translate_coordinates (widget /* src_widget */,
					  toplevel /* dest_widget */,
					  0, 0 /* src */,
					  &x, &y /* dest */);

	w = widget->allocation.width;
	h = widget->allocation.height;

	if (sides->top) {
		h += y;
		y = 0;
	}
	if (sides->left) {
		w += x;
		x = 0;
	}
	if (sides->bottom)
		h = toplevel->allocation.height - y;
	if (sides->right)
		w = toplevel->allocation.width - x;

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = x;
	attributes.y = y;
	attributes.width = w;
	attributes.height = h;
	attributes.wclass = GDK_INPUT_ONLY;
	attributes.event_mask = (GDK_BUTTON_PRESS_MASK |
				 GDK_BUTTON_RELEASE_MASK |
				 GDK_POINTER_MOTION_MASK |
				 GDK_POINTER_MOTION_HINT_MASK |
				 GDK_KEY_PRESS_MASK |
				 GDK_ENTER_NOTIFY_MASK |
				 GDK_LEAVE_NOTIFY_MASK);
	attributes_mask = GDK_WA_X | GDK_WA_Y;

	eventwin = gdk_window_new (toplevel->window,
				   &attributes,
				   attributes_mask);
	gdk_window_set_user_data (eventwin, widget);

	g_object_set_data (G_OBJECT (widget),
			   "StrechEventWindow",
			   eventwin);
}

static void
stretch_widget_unrealize (GtkWidget *widget)
{
	GdkWindow *eventwin = g_object_get_data (G_OBJECT (widget),
						 "StrechEventWindow");
	if (eventwin == NULL)
		return;

	gdk_window_destroy (eventwin);
	g_object_set_data (G_OBJECT (widget),
			   "StrechEventWindow",
			   NULL);
}

/* Evil but otherwise it doesn't seem
 * to work.  There needs to be a cleaner
 * solution */
static gboolean
raise_in_idle (gpointer data)
{
	GdkWindow *eventwin = g_object_get_data (G_OBJECT (data),
						 "StrechEventWindow");

	g_object_unref (G_OBJECT (data));

	if (eventwin == NULL)
		return FALSE;

	gdk_window_raise (eventwin);
	return FALSE;
}


static void
stretch_widget_map (GtkWidget *widget)
{
	GdkWindow *eventwin = g_object_get_data (G_OBJECT (widget),
						 "StrechEventWindow");
	if (eventwin == NULL)
		return;

	if (GTK_WIDGET_MAPPED (widget)) {
		gdk_window_show (eventwin);
		gdk_window_raise (eventwin);
		g_idle_add (raise_in_idle,
			    g_object_ref (G_OBJECT (widget)));
	}
}

static void
stretch_widget_unmap (GtkWidget *widget)
{
	GdkWindow *eventwin = g_object_get_data (G_OBJECT (widget),
						 "StrechEventWindow");
	if (eventwin == NULL)
		return;

	gdk_window_hide (eventwin);
}


static void
stretch_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GdkWindow *eventwin = g_object_get_data (G_OBJECT (widget),
						 "StrechEventWindow");
	if (eventwin == NULL)
		return;

	/* somewhat evil */
	stretch_widget_unrealize (widget);
	stretch_widget_realize (widget);
	stretch_widget_map (widget);
}

static void
stretch_widget_hierarchy_changed (GtkWidget *widget,
				 GtkWidget *previous_toplevel)
{
	stretch_widget_unrealize (widget);
	stretch_widget_realize (widget);
	stretch_widget_map (widget);
}

void
panel_stretch_events_to_toplevel (GtkWidget *widget,
				 gboolean top,
				 gboolean right,
				 gboolean bottom,
				 gboolean left)
{
	StrechSides *sides;

	g_signal_connect_after (GTK_WIDGET (widget), "realize",
				G_CALLBACK (stretch_widget_realize),
				NULL);
	g_signal_connect (GTK_WIDGET (widget), "unrealize",
			  G_CALLBACK (stretch_widget_unrealize),
			  NULL);
	g_signal_connect_after (GTK_WIDGET (widget), "size_allocate",
				G_CALLBACK (stretch_widget_size_allocate),
				NULL);
	g_signal_connect_after (GTK_WIDGET (widget), "map",
				G_CALLBACK (stretch_widget_map),
				NULL);
	g_signal_connect_after (GTK_WIDGET (widget), "unmap",
				G_CALLBACK (stretch_widget_unmap),
				NULL);
	g_signal_connect_after (GTK_WIDGET (widget), "hierarchy_changed",
				G_CALLBACK (stretch_widget_hierarchy_changed),
				NULL);

	sides = g_new0 (StrechSides, 1);
	sides->top = top;
	sides->right = right;
	sides->bottom = bottom;
	sides->left = left;

	g_object_set_data_full (G_OBJECT (widget),
				"StrechSides",
				sides,
				(GDestroyNotify) g_free);
}

/* stolen from gtk */
void
panel_signal_connect_while_alive (GObject     *object,
				  const gchar *signal,
				  GCallback    func,
				  gpointer     func_data,
				  GObject     *alive_object)
{
	GClosure *closure;

	g_return_if_fail (G_IS_OBJECT (object));

	closure = g_cclosure_new (func, func_data, NULL);
	g_object_watch_closure (G_OBJECT (alive_object), closure);
	g_signal_connect_closure_by_id (object,
					g_signal_lookup (signal, G_OBJECT_TYPE (object)), 0,
					closure,
					FALSE);
}

void
panel_signal_connect_object_while_alive (GObject      *object,
					 const gchar  *signal,
					 GCallback     func,
					 GObject      *alive_object)
{
  g_return_if_fail (G_IS_OBJECT (object));
  
  g_signal_connect_closure_by_id (object,
				  g_signal_lookup (signal, G_OBJECT_TYPE (object)), 0,
				  g_cclosure_new_object_swap (func, G_OBJECT (alive_object)),
				  FALSE);
}

gboolean
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
		    errno != EEXIST) {
			g_free (parsed);
			return FALSE;
		}
		*p = '/';
		p = strchr (p+1, '/');
	}

	if (mkdir (parsed, 0700) != 0 &&
	    errno != EEXIST) {
		g_free (parsed);
		return FALSE;
	}

	g_free (parsed);
	return TRUE;
}

void
panel_g_list_deep_free	(GList *list)
{
	GList *li;
	for (li = list; li != NULL; li = li->next) {
		g_free (li->data);
		li->data = NULL;
	}
	g_list_free (list);
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

#if 0
/* hmmm, we need to use this I think to add a battery applet
 *  -George */
/* Linux Battery Stuff */

static gboolean
linux_battery_exists (void)
{
        FILE *fp;
        char buf[200] = "";
        int foo;

        if ( ! g_file_test ("/proc/apm", G_FILE_TEST_EXISTS))
                return FALSE;

        fp = fopen ("/proc/apm", "r");
        if (fp == NULL)
                return FALSE;

        if (fgets (buf, sizeof (buf), fp) == NULL) {
                fclose (fp);
                return FALSE;
        }
        fclose (fp);

        foo = -1;
        sscanf (buf,
                "%*s %*d.%*d %*x %*x %*x %*x %d%% %*d %*s\n",
                &foo);

        if (foo >= 0)
                return TRUE;
        else
                return FALSE;
}

static gboolean
battery_exists (void)
{
#ifndef __linux__
        return FALSE;
#else
        /* This is MUUUUCHO ugly, but apparently RH 7.0 with segv the program
         * reading /proc/apm on some laptops, and we don't want to crash, thus
         * we do the check in a forked process */
        int status;
        pid_t pid;

        pid = fork ();
        if (pid == 0) {
                struct sigaction sa = {{NULL}};

                sa.sa_handler = SIG_DFL;

                sigaction(SIGSEGV, &sa, NULL);
                sigaction(SIGFPE, &sa, NULL);
                sigaction(SIGBUS, &sa, NULL);

                if (linux_battery_exists ())
                        _exit (0);
                else
                        _exit (1);
        }

        status = 0;
        waitpid (pid, &status, 0);

        if ( ! WIFSIGNALED (status) &&
            WIFEXITED (status) &&
            WEXITSTATUS (status) == 0) {
                return TRUE;
        } else {
                return FALSE;
        }
#endif
}

#endif
