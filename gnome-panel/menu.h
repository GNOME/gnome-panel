#ifndef MENU_H
#define MENU_H

#include <panel-widget.h>
#include "applet.h"

G_BEGIN_DECLS

/* FIXME and WARNING:
 *   !!! DO NOT CHANGE THESE VALUES !!!!
 *
 * These values are stupidly stored in gconf. See "main_menu_flags",
 * So changing these will screw old configurations.
 *
 * We have to fix this shite.
 */
enum {
	MAIN_MENU_SYSTEM = 1<<2,
	MAIN_MENU_SYSTEM_SUB = 1<<3,
	MAIN_MENU_DISTRIBUTION = 1<<4,
	MAIN_MENU_DISTRIBUTION_SUB = 1<<5,
	MAIN_MENU_KDE = 1<<6,
	MAIN_MENU_KDE_SUB = 1<<7,
	MAIN_MENU_PANEL_SUB = 1<<13,
	MAIN_MENU_DESKTOP = 1<<14,
	MAIN_MENU_DESKTOP_SUB = 1<<15
};

void		setup_menuitem		(GtkWidget   *menuitem,
					 GtkIconSize  icon_size,
					 GtkWidget   *pixmap,
					 const char  *title);

GtkWidget      *create_menu_at          (GtkWidget     *menu,
					 const char    *menudir,
					 gboolean       launcher_add,
					 const char    *dir_name,
					 gboolean       fake_submenus,
					 gboolean       force);

GtkWidget      *create_panel_context_menu (PanelWidget *panel);
GtkWidget      *create_panel_root_menu    (PanelWidget *panel);

void		panel_menuitem_lock_screen (GtkWidget *widget);

/*to be called on startup to load in some of the directories*/
void		init_menus		(void);

gboolean	menu_need_reread	(GtkWidget *menuw);

void		setup_internal_applet_drag (GtkWidget *menuitem,
					    const char *applet_type);
GtkWidget *	create_root_menu	(GtkWidget   *root_menu,
					 PanelWidget *panel,
					 gboolean     fake_submenus,
					 int          flags,
					 gboolean     extra_items);

/* some gtk code cut-n-paste action */
void		our_gtk_menu_position	(GtkMenu *menu);

GtkWidget *	panel_menu_new		(void);
void		panel_load_menu_image_deferred (GtkWidget   *image_menu_item,
						GtkIconSize  icon_size,
						const char  *stock_id,
						const char  *image_filename,
						const char  *fallback_image_filename,
						gboolean     force_image);
/* Note, bind the following on 'show' or some such */
void		panel_make_sure_menu_within_screen (GtkMenu *menu);

GdkPixbuf *	panel_make_menu_icon (const char *icon,
				      const char *fallback,
				      int size,
				      gboolean *long_operation);

gboolean
menu_dummy_button_press_event (GtkWidget      *menuitem,
			       GdkEventButton *event,
			       gpointer       data);


G_END_DECLS

#endif
