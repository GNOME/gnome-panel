#ifndef _PANEL_UTIL_H_
#define _PANEL_UTIL_H_

#include <gnome.h>

BEGIN_GNOME_DECLS

int string_is_in_list(GSList *list,char *text);
GtkWidget * create_text_entry(GtkWidget *table,
			      char *history_id,
			      int row,
			      char *label,
			      char *text,
			      GtkWidget *w);
GtkWidget * create_file_entry(GtkWidget *table,
			      char *history_id,
			      int row,
			      char *label,
			      char *text,
			      GtkWidget *w);
GtkWidget * create_pixmap_entry(GtkWidget *table,
				char *history_id,
				int row,
				char *label,
				char *text,
				GtkWidget *w,
				int pw, int ph /*preview size*/);
GtkWidget * create_icon_entry(GtkWidget *table,
			      char *history_id,
			      int cols,
			      int cole,
			      char *label,
			      char *text,
			      GtkWidget *w);

GList * my_g_list_swap_next(GList *list, GList *dl);
GList * my_g_list_swap_prev(GList *list, GList *dl);
/*maybe this should be a glib function?
 it resorts a single item in the list*/
GList * my_g_list_resort_item(GList *list, gpointer data, GCompareFunc func);

void set_frame_colors(PanelWidget *panel, GtkWidget *frame,
		      GtkWidget *but1, GtkWidget *but2, GtkWidget *but3,
		      GtkWidget *but4);

void remove_directory(char *dirname, int just_clean);


END_GNOME_DECLS

#endif
