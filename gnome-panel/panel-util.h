#ifndef _PANEL_UTIL_H_
#define _PANEL_UTIL_H_

#include <gnome.h>

BEGIN_GNOME_DECLS

char *get_full_path(char *argv0);
void move_window(GtkWidget *widget, int x, int y);
int string_is_in_list(GList *list,char *text);
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

GList * my_g_list_swap_next(GList *list, GList *dl);
GList * my_g_list_swap_prev(GList *list, GList *dl);
/*maybe this should be a glib function?
 it resorts a single item in the list*/
GList * my_g_list_resort_item(GList *list, gpointer data, GCompareFunc func);

/*pop and free the first list link*/
GList * my_g_list_pop_first(GList *list);


/*this is used to do an immediate move instead of set_uposition, which
queues one*/
void move_resize_window(GtkWidget *widget, int x, int y, int w, int h);

/*this is used to do an immediate resize instead of set_usize, which
queues one*/
void resize_window(GtkWidget *widget, int w, int h);


END_GNOME_DECLS

#endif
