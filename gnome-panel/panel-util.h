#ifndef PANEL_UTIL_H
#define PANEL_UTIL_H

#include <gtk/gtk.h>

#include "panel-widget.h"

G_BEGIN_DECLS


typedef void (*UpdateFunction) (gpointer);

/* return TRUE if string is NULL or the first character is '\0' */
gboolean	string_empty		(const char *string);
#define		sure_string(s)		((s)!=NULL?(s):"")

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

void		panel_show_help		(const char *path,
					 const char *linkid);

GList *		my_g_list_swap_next	(GList *list,
					 GList *dl);
GList *		my_g_list_swap_prev	(GList *list,
					 GList *dl);
/*maybe this should be a glib function?
 it resorts a single item in the list*/
GList *		my_g_list_resort_item	(GList *list,
					 gpointer data,
					 GCompareFunc func);

void		set_frame_colors	(PanelWidget *panel,
					 GtkWidget *frame,
					 GtkWidget *but1,
					 GtkWidget *but2,
					 GtkWidget *but3,
					 GtkWidget *but4);

/* recursively remove directory, if just_clean is true, then
   the directory itself is also removed, otherwise just it's
   contents, it does not give any error output and will fail
   silently, but it will remove as much as possible */
void		remove_directory	(const char *dirname,
					 gboolean just_clean);

gboolean	convert_string_to_keysym_state (const char *string,
						guint *keysym,
						GdkModifierType *state);
char *		convert_keysym_state_to_string (guint keysym,
						GdkModifierType state);

GtkWidget *	panel_error_dialog	(const char *class,
					 const char *format,
					 ...) G_GNUC_PRINTF (2, 3);
GtkWidget *	panel_info_dialog	(const char *class,
					 const char *format,
					 ...) G_GNUC_PRINTF (2, 3);

gboolean	is_ext			(const char *file,
					 const char *ext);
gboolean	is_ext2			(const char *file,
					 const char *ext1,
					 const char *ext2);

int		find_applet		(GtkWidget *widget);

int		get_requisition_width	(GtkWidget *widget);
int		get_requisition_height	(GtkWidget *widget);

char *		panel_gnome_kde_help_path (const char *docpath);

gboolean	panel_is_url		(const char *url);

void		panel_push_window_busy	(GtkWidget *window);
void		panel_pop_window_busy	(GtkWidget *window);

gboolean	panel_is_program_in_path (const char *program);

char *		panel_pixmap_discovery	(const char *name,
					 gboolean fallback);

void		panel_stretch_events_to_toplevel (GtkWidget *widget,
						 gboolean top,
						 gboolean right,
						 gboolean bottom,
						 gboolean left);


void		panel_signal_connect_while_alive (GObject     *object,
						  const gchar *signal,
						  GCallback    func,
						  gpointer     func_data,
						  GObject     *alive_object);
void		panel_signal_connect_object_while_alive (GObject      *object,
							 const gchar  *signal,
							 GCallback     func,
							 GObject      *alive_object);


gboolean	panel_ensure_dir	(const char *dirname);

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


/* FIXME : make battery stuff public as appropriate */


G_END_DECLS

#endif /* PANEL_UTIL_H */
