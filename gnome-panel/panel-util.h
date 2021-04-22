#ifndef PANEL_UTIL_H
#define PANEL_UTIL_H

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define		sure_string(s)		((const char *)((s)!=NULL?(s):""))

void		panel_push_window_busy	(GtkWidget *window);
void		panel_pop_window_busy	(GtkWidget *window);

char       *panel_launcher_get_filename        (const char *location);

char *panel_make_full_path   (const char *dir,
			      const char *filename);
char *panel_make_unique_desktop_uri (const char *dir,
				     const char *source);

char *panel_util_get_icon_name_from_g_icon (GIcon *gicon);
char *guess_icon_from_exec (GtkIconTheme *icon_theme,
			    GKeyFile     *key_file);

char *panel_util_get_label_for_uri (const char *text_uri);
char *panel_util_get_icon_for_uri (const char *text_uri);

GFile *panel_util_get_file_optional_homedir (const char *location);

void panel_util_key_event_is_popup (GdkEventKey *event,
				    gboolean    *is_popup,
				    gboolean    *is_popup_modifier);

void panel_util_key_event_is_popup_panel (GdkEventKey *event,
					  gboolean    *is_popup,
					  gboolean    *is_popup_modifier);

int panel_util_get_window_scaling_factor (void);

G_END_DECLS

#endif /* PANEL_UTIL_H */
