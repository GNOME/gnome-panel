#ifndef MENU_UTIL_H
#define MENU_UTIL_H

G_BEGIN_DECLS

void applet_menu_position (GtkMenu *menu, gint *x, gint *y, gpointer data);
void panel_menu_position (GtkMenu *menu, gint *x, gint *y, gpointer data);
GtkWidget * add_menu_separator (GtkWidget *menu);
int get_default_menu_flags (void) G_GNUC_CONST;

G_END_DECLS

#endif
