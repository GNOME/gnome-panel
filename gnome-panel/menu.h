#ifndef MENU_H
#define MENU_H

#include <panel-widget.h>
#include "applet.h"

BEGIN_GNOME_DECLS

/*compatibility only*/
typedef enum {
	X_MAIN_MENU_BOTH,
	X_MAIN_MENU_USER,
	X_MAIN_MENU_SYSTEM
} X_MainMenuType;

enum {
	MAIN_MENU_USER = 1<<0,
	MAIN_MENU_USER_SUB = 1<<1,
	MAIN_MENU_SYSTEM = 1<<2,
	MAIN_MENU_SYSTEM_SUB = 1<<3,
	MAIN_MENU_DISTRIBUTION = 1<<4,
	MAIN_MENU_DISTRIBUTION_SUB = 1<<5,
	MAIN_MENU_KDE = 1<<6,
	MAIN_MENU_KDE_SUB = 1<<7,
	/* keep this for compatibility. */
	MAIN_MENU_OBSOLETE_DEBIAN = 1<<8,
	MAIN_MENU_OBSOLETE_DEBIAN_SUB = 1<<9,
	MAIN_MENU_APPLETS = 1<<10,
	MAIN_MENU_APPLETS_SUB = 1<<11,
	MAIN_MENU_PANEL = 1<<12,
	MAIN_MENU_PANEL_SUB = 1<<13,
	MAIN_MENU_DESKTOP = 1<<14,
	MAIN_MENU_DESKTOP_SUB = 1<<15
};

typedef struct _Menu Menu;
struct _Menu {
	GtkWidget *button;
	GtkWidget *menu;
	gboolean custom_icon;
	char *custom_icon_file;
	char *path;
	int main_menu_flags;
	gboolean global_main;
	int age;
	AppletInfo *info;
};

void load_menu_applet(char *params, int main_menu_flags, gboolean global_main,
		      gboolean custom_icon, char *custom_icon_file,
		      PanelWidget *panel, int pos, gboolean exactpos);
void add_menu_widget (Menu *menu, PanelWidget *panel, GSList *menudirl,
		      gboolean main_menu, gboolean fake_subs);

void set_menu_applet_orient(Menu *menu, PanelOrientType orient);

void setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title);
void make_panel_submenu (GtkWidget *menu, gboolean fake_submenus, gboolean is_basep);

GtkWidget * create_panel_root_menu(PanelWidget *panel, gboolean tearoff);

void menu_properties(Menu *menu);

void panel_lock (GtkWidget *widget, void *data);

/*to be called on startup to load in some of the directories*/
void init_menus(void);

void save_tornoff(void);
void load_tornoff(void);

GtkWidget * create_menu_at (GtkWidget *menu,
			    const char *menudir,
			    gboolean applets,
			    const char *dir_name,
			    const char *pixmap_name,
			    gboolean fake_submenus,
			    gboolean force,
			    gboolean title);
GtkWidget * create_fake_menu_at (const char *menudir,
				 gboolean applets,
				 const char *dir_name,
				 const char *pixmap_name,
				 gboolean title);

void submenu_to_display(GtkWidget *menuw, gpointer data);
gboolean menu_need_reread(GtkWidget *menuw);

void setup_internal_applet_drag (GtkWidget *menuitem, char *applet_type);
GtkWidget * create_root_menu(GtkWidget *root_menu,
			     gboolean fake_submenus, int flags,
			     gboolean tearoff, gboolean is_basep,
			     gboolean title);



#define MENU_PATH "menu_path"

#define MENU_PROPERTIES "menu_properties"

#define MENU_TYPES "types_menu"
#define MENU_TYPE_EDGE "type-edge"
#define MENU_TYPE_ALIGNED "type-aligned"
#define MENU_TYPE_SLIDING "type-sliding"
#define MENU_TYPE_FLOATING "type-floating"

#define MENU_MODES "modes_menu"
#define MENU_MODE_EXPLICIT_HIDE "mode-explicit-hide"
#define MENU_MODE_AUTO_HIDE "mode-auto-hide"

#define MENU_HIDEBUTTONS "hidebuttons_menu"
#define MENU_HIDEBUTTONS_PIXMAP "hidebuttons-pixmap"
#define MENU_HIDEBUTTONS_PLAIN "hidebuttons-plain"
#define MENU_HIDEBUTTONS_NONE "hidebuttons-none"

/* perhaps into basep-widget.h? */
enum {
	HIDEBUTTONS_PIXMAP,
	HIDEBUTTONS_PLAIN,
	HIDEBUTTONS_NONE
};


#define MENU_SIZES "sizes_menu"
#define MENU_SIZE_ULTRA_TINY "size-ultra-tiny"
#define MENU_SIZE_TINY "size-tiny"
#define MENU_SIZE_SMALL "size-small"
#define MENU_SIZE_STANDARD "size-standard"
#define MENU_SIZE_LARGE "size-large"
#define MENU_SIZE_HUGE "size-huge"
#define MENU_SIZE_RIDICULOUS "size-ridiculous"

#define MENU_BACKS "background_menu"
#define MENU_BACK_NONE "back-none"
#define MENU_BACK_PIXMAP "back-pixmap"
#define MENU_BACK_COLOR "back-color"

#define MENU_ORIENTS "orients_menu"
#define MENU_ORIENT_HORIZONTAL "orient-horizontal"
#define MENU_ORIENT_VERTICAL "orient-vertical"

END_GNOME_DECLS

#endif
