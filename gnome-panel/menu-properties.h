#ifndef MENU_PROPERTIES_H
#define MENU_PROPERTIES_H

BEGIN_GNOME_DECLS

void menu_properties(Menu *menu);

char * get_real_menu_path(char *arguments, char *menu_base);
char * get_pixmap(char *menudir, int main_menu);

END_GNOME_DECLS

#endif

