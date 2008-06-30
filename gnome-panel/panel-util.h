#ifndef PANEL_UTIL_H
#define PANEL_UTIL_H

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef void (*UpdateFunction) (gpointer);

#define		sure_string(s)		((const char *)((s)!=NULL?(s):""))

void            panel_util_launch_from_key_file (GKeyFile                *keyfile,
						 GList                   *file_list,
						 GdkScreen               *screen,
						 GError                 **error);
void            panel_launch_desktop_file  (const char                   *desktop_file,
					    const char                   *fallback_exec,
					    GdkScreen                    *screen,
					    GError                      **error);

char *          panel_util_make_exec_uri_for_desktop (const char *exec);

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

char *panel_make_full_path   (const char *dir,
			      const char *filename);
char *panel_make_unique_desktop_path_from_name (const char *dir,
						const char *name);
char *panel_make_unique_desktop_uri (const char *dir,
				     const char *source);
char *panel_lookup_in_data_dirs (const char *basename);

const char *panel_util_utf8_strstrcase (const char *haystack,
					const char *needle);

GdkPixbuf *panel_util_cairo_rgbdata_to_pixbuf (unsigned char *data,
					       int            width,
					       int            height);

char *guess_icon_from_exec (GtkIconTheme *icon_theme,
			    GKeyFile     *key_file);

const char *panel_util_get_vfs_method_display_name (const char *method);
char *panel_util_get_label_for_uri (const char *text_uri);
char *panel_util_get_icon_for_uri (const char *text_uri);

void panel_util_set_tooltip_text (GtkWidget  *widget,
				  const char *text);

char *panel_util_get_icon_name_from_g_icon (GIcon *gicon);
GdkPixbuf *panel_util_get_pixbuf_from_g_loadable_icon (GIcon *gicon,
						       int    size);
GFile *panel_util_get_file_optional_homedir (const char *location);

G_END_DECLS

#endif /* PANEL_UTIL_H */
