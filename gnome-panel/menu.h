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

/* this is defined in menu-properties.c */
typedef struct _MenuDialogInfo MenuDialogInfo;

typedef struct _Menu Menu;
struct _Menu {
	GtkWidget		*button;
	GtkWidget		*menu;
	gboolean		 custom_icon;
	char			*custom_icon_file;

	gboolean		 main_menu;
	/* If this is the main menu then path is irrelevant */
	char			*path;

	gboolean		 global_main;
	/* if global_main is on then main_menu_flags are irrelevant */
	int			 main_menu_flags;

	int			 age;
	GtkWidget		*prop_dialog;
	MenuDialogInfo		*dialog_info;
	AppletInfo		*info;
};

void		load_menu_applet	(const char  *path,
					 gboolean     main_menu,
					 int          main_menu_flags,
					 gboolean     global_main,
					 gboolean     custom_icon,
					 const char  *custom_icon_file,
					 PanelWidget *panel,
					 int          pos,
					 gboolean     exactpos,
					 const char  *gconf_key);

void		add_menu_widget		(Menu *menu,
					 PanelWidget *panel,
					 GSList *menudirl,
					 gboolean fake_subs);

void		set_menu_applet_orient	(Menu *menu,
					 PanelOrient orient);

void		setup_menuitem		(GtkWidget   *menuitem,
					 GtkIconSize  icon_size,
					 GtkWidget   *pixmap,
					 const char  *title);

GtkWidget      *create_panel_context_menu (PanelWidget *panel);
GtkWidget      *create_panel_root_menu    (PanelWidget *panel);

void		menu_properties		(Menu *menu);

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
					 gboolean     is_basep,
					 gboolean     extra_items);

/* some gtk code cut-n-paste action */
void		our_gtk_menu_position	(GtkMenu *menu);

void            menu_save_to_gconf   (Menu       *menu,
				      const char *gconf_key);

void            menu_load_from_gconf (PanelWidget *panel_widget,
				      gint         position,
				      const char  *gconf_key);

/* perhaps into basep-widget.h? */
enum {
	HIDEBUTTONS_PIXMAP,
	HIDEBUTTONS_PLAIN,
	HIDEBUTTONS_NONE
};

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


G_END_DECLS

#endif
