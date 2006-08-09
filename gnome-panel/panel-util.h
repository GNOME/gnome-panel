#ifndef PANEL_UTIL_H
#define PANEL_UTIL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef void (*UpdateFunction) (gpointer);

/* TRUE if string is NULL or the first character is '\0' */
#define		string_empty(s)		((s)==NULL||((char *)(s))[0]=='\0')

#define		sure_string(s)		((const char *)((s)!=NULL?(s):""))

void            panel_util_launch_from_key_file (GKeyFile                *keyfile,
						 GList                   *file_list,
						 GdkScreen               *screen,
						 GError                 **error);
void            panel_launch_desktop_file  (const char                   *desktop_file,
					    const char                   *fallback_exec,
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

GSList *panel_g_slist_make_unique (GSList       *list,
				   GCompareFunc  compare,
				   gboolean      free_data);

GtkWidget      *panel_error_dialog      (GtkWindow  *parent,
					 GdkScreen  *screen,
					 const char *class,
					 gboolean    auto_destroy,
					 const char *primary_text,
					 const char *secondary_text);

int		panel_find_applet_index	(GtkWidget *widget);

void		panel_push_window_busy	(GtkWidget *window);
void		panel_pop_window_busy	(GtkWidget *window);

gboolean	panel_is_program_in_path (const char *program);

gboolean	panel_is_uri_writable	(const char *uri);
gboolean	panel_uri_exists	(const char *uri);

void            panel_lock_screen                  (GdkScreen    *screen);
void            panel_lock_screen_action           (GdkScreen    *screen,
                                                    const char   *action);
gboolean        panel_lock_screen_action_available (const char   *action);

char *          panel_util_icon_remove_extension (const char *icon);
char *          panel_find_icon         (GtkIconTheme  *icon_theme,
					 const char    *icon_name,
					 int            size);
GdkPixbuf *     panel_load_icon         (GtkIconTheme  *icon_theme,
					 const char    *icon_name,
					 int            size,
					 int            desired_width,
					 int            desired_height,
					 char         **error_msg);
GdkPixbuf *	missing_pixbuf		(int size);

char *panel_make_full_path   (const char *dir,
			      const char *filename);
char *panel_make_unique_uri (const char *dir,
			     const char *suffix);
char *panel_lookup_in_data_dirs (const char *basename);

const char *panel_util_utf8_strstrcase (const char *haystack,
					const char *needle);

GdkPixbuf *panel_util_cairo_rgbdata_to_pixbuf (unsigned char *data,
					       int            width,
					       int            height);

GKeyFile *panel_util_key_file_new_desktop  (void);
gboolean  panel_util_key_file_to_file      (GKeyFile       *keyfile,
					    const gchar    *file,
					    GError        **error);
gboolean panel_util_key_file_load_from_uri (GKeyFile       *keyfile,
					    const gchar    *uri,
					    GKeyFileFlags   flags,
					    GError        **error);
gboolean panel_util_key_file_get_boolean   (GKeyFile       *keyfile,
					    const gchar    *key,
					    gboolean        default_value);
#define panel_util_key_file_get_string(key_file, key) \
	 g_key_file_get_string (key_file, "Desktop Entry", key, NULL)
#define panel_util_key_file_get_locale_string(key_file, key) \
	 g_key_file_get_locale_string(key_file, "Desktop Entry", key, NULL, NULL)
#define panel_util_key_file_set_boolean(key_file, key, value) \
	 g_key_file_set_boolean (key_file, "Desktop Entry", key, value)
#define panel_util_key_file_set_string(key_file, key, value) \
	 g_key_file_set_string (key_file, "Desktop Entry", key, value)
void    panel_util_key_file_set_locale_string (GKeyFile    *keyfile,
					       const gchar *key,
					       const gchar *value);
#define panel_util_key_file_remove_key(key_file, key) \
	g_key_file_remove_key (key_file, "Desktop Entry", key, NULL)
void panel_util_key_file_remove_locale_key (GKeyFile    *keyfile,
					    const gchar *key);

G_END_DECLS

#endif /* PANEL_UTIL_H */
