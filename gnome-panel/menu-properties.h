#ifndef MENU_PROPERTIES_H
#define MENU_PROPERTIES_H

#include "menu.h"

G_BEGIN_DECLS

void		menu_properties		(Menu *menu);

char *		get_real_menu_path	(const char *arguments,
					 gboolean main_menu);
char *		get_pixmap		(const char *menudir,
					 gboolean main_menu);

G_END_DECLS

#endif

