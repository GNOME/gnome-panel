#ifndef PANEL_UTIL_H
#define PANEL_UTIL_H

#include <gnome.h>

BEGIN_GNOME_DECLS

gboolean string_is_in_list(GSList *list,char *text);
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
			      char *subdir,
			      char *text,
			      GtkWidget *w);

void panel_pbox_help_cb (GtkWidget *, gint, gpointer);

GList * my_g_list_swap_next(GList *list, GList *dl);
GList * my_g_list_swap_prev(GList *list, GList *dl);
/*maybe this should be a glib function?
 it resorts a single item in the list*/
GList * my_g_list_resort_item(GList *list, gpointer data, GCompareFunc func);

void set_frame_colors(PanelWidget *panel, GtkWidget *frame,
		      GtkWidget *but1, GtkWidget *but2, GtkWidget *but3,
		      GtkWidget *but4);

/* recursively remove directory, if just_clean is true, then
   the directory itself is also removed, otherwise just it's
   contents, it does not give any error output and will fail
   silently, but it will remove as much as possible */
void remove_directory(char *dirname, gboolean just_clean);

/* like strtok, but unescapes string as well and escaped separators
   don't count.  if empty is true, don't skip empty parts */
char *strtok_with_escape(char *string, const char *seps, gboolean empty);
/* return a newly allocated string that escapes / and 'special' */
char *escape_string(const char *string, const char *special);

gboolean convert_string_to_keysym_state(char *string,
					guint *keysym,
					guint *state);
char * convert_keysym_state_to_string(guint keysym,
				      guint state);

void panel_set_dialog_layer(GtkWidget *dialog);
GtkWidget *panel_error_dialog(char *format, ...) G_GNUC_PRINTF (1, 2);


END_GNOME_DECLS

#endif /* PANEL_UTIL_H */
