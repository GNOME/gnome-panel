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

END_GNOME_DECLS

#endif
