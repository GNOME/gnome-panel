#ifndef MENU_UTIL_H
#define MENU_UTIL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

void panel_position_applet_menu (GtkMenu   *menu,
				 int       *x, 
				 int       *y,
				 gboolean  *push_in,
				 GtkWidget *widget);


void       panel_menu_position    (GtkMenu  *menu,
				   gint     *x,
				   gint     *y,
				   gboolean *push_in,
				   gpointer  data);

void       menu_item_menu_position   (GtkMenu  *menu,
				      gint     *x,
				      gint     *y,
				      gboolean *push_in,
				      gpointer  data);

char *     get_real_menu_path      (const char *arguments,
                                    gboolean main_menu);
char *     get_pixmap              (const char *menudir,
                                    gboolean main_menu);

GtkWidget *add_menu_separator     (GtkWidget *menu);

GtkWidget *stock_menu_item_new (const char *text,
				const char *stock_id,
				gboolean    force_image);

int        get_default_menu_flags (void);
gboolean   got_kde_menus (void);
gboolean   got_distro_menus (void);


G_END_DECLS

#endif
