#ifndef MENU_H
#define MENU_H

#include <panel-widget.h>
#include "applet.h"

G_BEGIN_DECLS

enum {
	/* FIXME: should we move this array or keep the bitvalues
	 * the same??? */
	MAIN_MENU_SYSTEM = 1<<2,
	MAIN_MENU_SYSTEM_SUB = 1<<3,
	MAIN_MENU_DISTRIBUTION = 1<<4,
	MAIN_MENU_DISTRIBUTION_SUB = 1<<5,
	MAIN_MENU_KDE = 1<<6,
	MAIN_MENU_KDE_SUB = 1<<7,
	MAIN_MENU_APPLETS = 1<<10,
	MAIN_MENU_APPLETS_SUB = 1<<11,
	MAIN_MENU_PANEL = 1<<12,
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

void		load_menu_applet	(const char *path,
					 gboolean main_menu,
					 int main_menu_flags,
					 gboolean global_main,
					 gboolean custom_icon,
					 const char *custom_icon_file,
					 PanelWidget *panel,
					 int pos,
					 gboolean exactpos);
void		add_menu_widget		(Menu *menu,
					 PanelWidget *panel,
					 GSList *menudirl,
					 gboolean fake_subs);

void		set_menu_applet_orient	(Menu *menu,
					 PanelOrient orient);

void		setup_menuitem		(GtkWidget *menuitem,
					 GtkWidget *pixmap,
					 const char *title);
void		make_panel_submenu	(GtkWidget *menu,
					 gboolean fake_submenus,
					 gboolean is_basep);

GtkWidget *	create_panel_root_menu	(PanelWidget *panel,
					 gboolean tearoff);

void		menu_properties		(Menu *menu);

void		panel_lock		(GtkWidget *widget,
					 gpointer data);

/*to be called on startup to load in some of the directories*/
void		init_menus		(void);

void		panel_menu_session_save_tornoffs (void);
void		panel_menu_session_load_tornoffs (void);

GtkWidget *	create_menu_at		(GtkWidget *menu,
					 const char *menudir,
					 gboolean applets,
					 gboolean launcher_add,
					 const char *dir_name,
					 const char *pixmap_name,
					 gboolean fake_submenus,
					 gboolean force);
GtkWidget *	create_fake_menu_at	(const char *menudir,
					 gboolean applets,
					 gboolean launcher_add,
					 const char *dir_name,
					 const char *pixmap_name);

void		submenu_to_display	(GtkWidget *menuw, gpointer data);
gboolean	menu_need_reread	(GtkWidget *menuw);

void		setup_internal_applet_drag (GtkWidget *menuitem,
					    const char *applet_type);
GtkWidget *	create_root_menu	(GtkWidget *root_menu,
					 gboolean fake_submenus,
					 int flags,
					 gboolean tearoff,
					 gboolean is_basep,
					 gboolean run_item);

/* some gtk code cut-n-paste action */
void		our_gtk_menu_position	(GtkMenu *menu);

/* perhaps into basep-widget.h? */
enum {
	HIDEBUTTONS_PIXMAP,
	HIDEBUTTONS_PLAIN,
	HIDEBUTTONS_NONE
};

#define PANEL_MENU_HAVE_ICONS_KEY "/desktop/gnome/menus/show-icons"
gboolean panel_menu_have_icons   (void);
#define PANEL_MENU_HAVE_TEAROFF_KEY "/desktop/gnome/interface/menus-have-tearoff"
gboolean panel_menu_have_tearoff (void);


G_END_DECLS

#endif
