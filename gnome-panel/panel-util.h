#ifndef PANEL_UTIL_H
#define PANEL_UTIL_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgnome/gnome-desktop-item.h>

#include "panel-widget.h"
#include "panel-config-global.h"

G_BEGIN_DECLS

typedef enum {
	PANEL_STRETCH_NONE   = 0,
	PANEL_STRETCH_TOP    = 1 << 0,
	PANEL_STRETCH_RIGHT  = 1 << 1,
	PANEL_STRETCH_BOTTOM = 1 << 2,
	PANEL_STRETCH_LEFT   = 1 << 3,
} PanelStretchFlags;

typedef void (*UpdateFunction) (gpointer);

/* TRUE if string is NULL or the first character is '\0' */
#define		string_empty(s)		((s)==NULL||((char *)(s))[0]=='\0')

#define		sure_string(s)		((const char *)((s)!=NULL?(s):""))

GtkWidget *	create_text_entry	(GtkWidget *table,
					 const char *history_id,
					 int row,
					 const char *label,
					 const char *text,
					 UpdateFunction func,
					 gpointer data);
GtkWidget *	create_icon_entry	(GtkWidget *table,
					 const char *history_id,
					 int cols,
					 int cole,
					 const char *label,
					 const char *subdir,
					 const char *text,
					 UpdateFunction func,
					 gpointer data);

int             panel_ditem_launch         (const GnomeDesktopItem       *item,
					    GList                        *file_list,
					    GnomeDesktopItemLaunchFlags   flags,
					    GdkScreen                    *screen,
					    GError                      **error);
GdkScreen      *panel_screen_from_number   (int           screen);



void		panel_show_help		(GdkScreen  *screen,
					 const char *path,
					 const char *linkid);

GList *		panel_g_list_swap_next	(GList *list,
					 GList *dl);
GList *		panel_g_list_swap_prev	(GList *list,
					 GList *dl);
/*maybe this should be a glib function?
 it resorts a single item in the list*/
GList *		panel_g_list_resort_item(GList *list,
					 gpointer data,
					 GCompareFunc func);

void		panel_g_list_deep_free	(GList *list);
void		panel_g_slist_deep_free	(GSList *list);

void		panel_set_frame_colors	(PanelWidget *panel,
					 GtkWidget *frame,
					 GtkWidget *but1,
					 GtkWidget *but2,
					 GtkWidget *but3,
					 GtkWidget *but4);

gboolean        panel_parse_accelerator (GlobalConfigKey *key);

char *		convert_keysym_state_to_string (guint keysym,
						GdkModifierType state);

GtkWidget      *panel_error_dialog      (GdkScreen  *screen,
					 const char *class,
					 const char *format,
					 ...) G_GNUC_PRINTF (3, 4);
GtkWidget      *panel_info_dialog       (GdkScreen  *screen,
					 const char *class,
					 const char *format,
					 ...) G_GNUC_PRINTF (3, 4);

gboolean	is_ext			(const char *file,
					 const char *ext);
gboolean	is_ext2			(const char *file,
					 const char *ext1,
					 const char *ext2);

int		panel_find_applet	(GtkWidget *widget);

int		get_requisition_width	(GtkWidget *widget);
int		get_requisition_height	(GtkWidget *widget);

typedef enum {
	PANEL_HELP_ERROR_NO_DOCPATH /* No docpath sent */,
	PANEL_HELP_ERROR_NOT_FOUND /* Document not found */
} PanelHelpError;

#define PANEL_HELP_ERROR panel_help_error_quark ()
GQuark panel_help_error_quark (void);

gboolean	panel_show_gnome_kde_help (GdkScreen   *screen,
					   const char  *docpath,
					   GError     **error);

gboolean	panel_is_url		(const char *url);

void		panel_push_window_busy	(GtkWidget *window);
void		panel_pop_window_busy	(GtkWidget *window);

gboolean	panel_is_program_in_path (const char *program);

char *		panel_pixmap_discovery	(const char *name,
					 gboolean fallback);

void		panel_stretch_events_to_toplevel (GtkWidget         *widget,
						  PanelStretchFlags  flags);

void		panel_signal_connect_while_alive        (gpointer     object,
							 const gchar *signal,
							 GCallback    func,
							 gpointer     func_data,
							 gpointer     alive_object);
void		panel_signal_connect_object_while_alive (gpointer     object,
							 const gchar *signal,
							 GCallback    func,
							 gpointer     alive_object);

gboolean	panel_ensure_dir	(const char *dirname);

gboolean	panel_is_uri_writable	(const char *uri);
gboolean	panel_uri_exists	(const char *uri);

void            panel_lock_screen       (GdkScreen *screen);


/* GnomeVFS reading utils */

typedef struct _ReadBuf ReadBuf;

int		readbuf_getc		(ReadBuf *rb);
/* Note, does not include the trailing \n */
char *		readbuf_gets		(char *buf, 
					 gsize bufsize,
					 ReadBuf *rb);
ReadBuf *	readbuf_open		(const char *uri);
/* unused for now */
gboolean	readbuf_rewind		(ReadBuf *rb);
void		readbuf_close		(ReadBuf *rb);

/* Accessibility support routines */
void panel_set_atk_name_desc (GtkWidget *widget,
			      char      *name,
			      char      *desc);
void panel_set_atk_relation  (GtkWidget *widget,
			      GtkLabel  *label);

GdkPixbuf *	missing_pixbuf		(int size);


G_END_DECLS

#endif /* PANEL_UTIL_H */
