#ifndef MENU_PROPERTIES_H
#define MENU_PROPERTIES_H

BEGIN_GNOME_DECLS

void		menu_properties		(Menu *menu);

char *		get_real_menu_path	(const char *arguments);
char *		get_pixmap		(const char *menudir,
					 gboolean main_menu);

END_GNOME_DECLS

#endif

