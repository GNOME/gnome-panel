#ifndef PANEL_UTIL_H
#define PANEL_UTIL_H

#include <gnome.h>

BEGIN_GNOME_DECLS

typedef void (*UpdateFunction) (gpointer);

gboolean	string_is_in_list	(const GSList *list,
					 const char *text);

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

void		panel_show_help		(const char *path);

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

/* like strtok, but unescapes string as well and escaped separators
   don't count.  if empty is true, don't skip empty parts */
char *		strtok_with_escape	(char *string,
					 const char *seps,
					 gboolean empty);
/* return a newly allocated string that escapes / and 'special' */
char *		escape_string		(const char *string,
					 const char *special);

gboolean	convert_string_to_keysym_state (const char *string,
						guint *keysym,
						guint *state);
char *		convert_keysym_state_to_string (guint keysym,
						guint state);

void		panel_set_dialog_layer	(GtkWidget *dialog);
void		panel_reset_dialog_layers (void);
GtkWidget *	panel_error_dialog	(const char *format, ...) G_GNUC_PRINTF (1, 2);

gboolean	is_ext			(const char *file,
					 const char *ext);
int		strcasecmp_no_locale	(const char *s1,
					 const char *s2);

/* stolen from gnome-libs head as they are faster and don't use "stat" */
gboolean	panel_file_exists	(const char *filename);
char *		panel_is_program_in_path (const char *program);

int		find_applet		(GtkWidget *widget);

int		get_requisition_width	(GtkWidget *widget);
int		get_requisition_height	(GtkWidget *widget);

char *		panel_gnome_kde_help_path (const char *docpath);

gboolean	panel_is_url		(const char *url);

END_GNOME_DECLS

#endif /* PANEL_UTIL_H */
