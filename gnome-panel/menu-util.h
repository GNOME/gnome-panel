#ifndef MENU_UTIL_H
#define MENU_UTIL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

void       applet_menu_position   (GtkMenu  *menu,
				   gint     *x,
				   gint     *y,
				   gboolean *push_in,
				   gpointer  data);

void       panel_menu_position    (GtkMenu  *menu,
				   gint     *x,
				   gint     *y,
				   gboolean *push_in,
				   gpointer  data);

GtkWidget *add_menu_separator     (GtkWidget *menu);

int        get_default_menu_flags (void);
gboolean   got_kde_menus (void);
gboolean   got_distro_menus (void);

G_END_DECLS

#endif
