#ifndef __MENU_DITEM_H__
#define __MENU_DITEM_H__

#include <glib/gmacros.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

GtkWidget *panel_edit_dentry   (const char *loc,
				const char *dir);
GtkWidget *panel_edit_direntry (const char *dir,
				const char *dir_name);
GtkWidget *panel_new_launcher  (const char *item_loc);

G_END_DECLS

#endif /* __MENU_DITEM_H__ */
