#ifndef __MENU_DITEM_H__
#define __MENU_DITEM_H__

#include <glib/gmacros.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

GtkWidget *panel_edit_dentry   (const char *loc,
				const char *dir,
				GdkScreen  *screen);
GtkWidget *panel_edit_direntry (const char *dir,
				const char *dir_name,
				GdkScreen  *screen);
GtkWidget *panel_new_launcher  (const char *item_loc,
				GdkScreen  *screen);

G_END_DECLS

#endif /* __MENU_DITEM_H__ */
