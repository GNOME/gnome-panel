#ifndef MENU_UTIL_H
#define MENU_UTIL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

void       applet_menu_position   (GtkMenu  *menu,
				   gint     *x,
				   gint     *y,
				   gboolean *push_in,
				   gpointer  data);
void       applet_menu_position_outside   (GtkMenu  *menu,
					   gint     *x,
					   gint     *y,
					   gboolean *push_in,
					   gpointer  data);

void       panel_menu_position    (GtkMenu  *menu,
				   gint     *x,
				   gint     *y,
				   gboolean *push_in,
				   gpointer  data);

char *     get_real_menu_path      (const char *arguments,
                                    gboolean main_menu);
char *     get_pixmap              (const char *menudir,
                                    gboolean main_menu);

GtkWidget *add_menu_separator     (GtkWidget *menu);

int        get_default_menu_flags (void);
gboolean   got_kde_menus (void);
gboolean   got_distro_menus (void);


G_END_DECLS

#endif
