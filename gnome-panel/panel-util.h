#ifndef PANEL_UTIL_H
#define PANEL_UTIL_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgnome/gnome-desktop-item.h>

#include "panel-widget.h"
#include "panel-config-global.h"

G_BEGIN_DECLS

typedef void (*UpdateFunction) (gpointer);

/* TRUE if string is NULL or the first character is '\0' */
#define		string_empty(s)		((s)==NULL||((char *)(s))[0]=='\0')

#define		sure_string(s)		((const char *)((s)!=NULL?(s):""))

int             panel_ditem_launch         (const GnomeDesktopItem       *item,
					    GList                        *file_list,
					    GnomeDesktopItemLaunchFlags   flags,
					    GdkScreen                    *screen,
					    GError                      **error);

void		panel_show_help		(GdkScreen  *screen,
					 const char *path,
					 const char *linkid);

GList *panel_g_list_insert_before (GList        *list,
				   GList        *sibling,
				   GList        *link);
GList *panel_g_list_insert_after  (GList        *list,
				   GList        *sibling,
				   GList        *link);
GList *panel_g_list_swap_next     (GList        *list,
				   GList        *dl);
GList *panel_g_list_swap_prev     (GList        *list,
				   GList        *dl);
GList *panel_g_list_resort_item   (GList        *list,
				   gpointer      data,
				   GCompareFunc  func);
void   panel_g_slist_deep_free    (GSList       *list);


GtkWidget      *panel_error_dialog      (GdkScreen  *screen,
					 const char *class,
					 const char *primary_format,
					 const char *secondary_format,
					 ...);

int		panel_find_applet_index	(GtkWidget *widget);

void		panel_push_window_busy	(GtkWidget *window);
void		panel_pop_window_busy	(GtkWidget *window);

gboolean	panel_is_program_in_path (const char *program);

char *		panel_pixmap_discovery	(const char *name,
					 gboolean fallback);

gboolean	panel_is_uri_writable	(const char *uri);
gboolean	panel_uri_exists	(const char *uri);

void            panel_lock_screen       (GdkScreen *screen);


/* GnomeVFS reading utils */

typedef struct _ReadBuf ReadBuf;

/* Note, does not include the trailing \n */
char *		readbuf_gets		(char *buf, 
					 gsize bufsize,
					 ReadBuf *rb);
ReadBuf *	readbuf_open		(const char *uri);
/* unused for now */
gboolean	readbuf_rewind		(ReadBuf *rb);
void		readbuf_close		(ReadBuf *rb);

GdkPixbuf *	missing_pixbuf		(int size);

char *panel_make_full_path   (const char *dir,
			      const char *filename);
char *panel_make_unique_uri (const char *dir,
			     const char *suffix);

G_END_DECLS

#endif /* PANEL_UTIL_H */
