#ifndef APPLET_H
#define APPLET_H

#include <glib.h>
#include <gdk/gdktypes.h>
#include "panel-widget.h"
#include "panel-gconf.h"

G_BEGIN_DECLS

typedef enum {
	APPLET_DRAWER,
	APPLET_MENU,
	APPLET_LAUNCHER,
	APPLET_STATUS,
	APPLET_BONOBO,
	APPLET_ACTION,
	APPLET_LOGOUT, /* FIXME:                          */
	APPLET_LOCK,   /*  Both only for backwards compat */

} AppletType;

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)
typedef struct {
	AppletType      type;
	int             applet_id;
	GtkWidget      *widget;

	GtkWidget      *menu;
	int             menu_age;
	GList          *user_menu;

	gpointer        data;
	GDestroyNotify  data_destroy;

	guint		remove_idle;

	char           *gconf_key;
} AppletInfo;

typedef struct {
	gchar        *name;
	gchar        *stock_item;
	gchar        *text;

	gint          sensitive;
	AppletInfo   *info;

	GtkWidget    *menuitem;
	GtkWidget    *submenu;
} AppletUserMenu;

AppletInfo *panel_applet_register    (GtkWidget      *applet,
				      gpointer        data,
				      GDestroyNotify  data_destroy,
				      PanelWidget    *panel,
				      gint            pos,
				      gboolean        exactpos,
				      AppletType      type,
				      const char     *gconf_key);

void        panel_applet_clean       (AppletInfo *info,
				      gboolean    clean_gconf);

void        panel_applet_clean_gconf (AppletType  type,
				      const char *gconf_key,
				      gboolean    clean_gconf);

void            panel_applet_add_callback    (AppletInfo  *info,
					      const gchar *callback_name,
					      const gchar *stock_item,
					      const gchar *menuitem_text);

void            panel_applet_remove_callback (AppletInfo *info,
					      const char *callback_name);

void            panel_applet_remove_in_idle  (AppletInfo *info);

AppletUserMenu *panel_applet_get_callback    (GList       *user_menu,
					      const gchar *name);


void        panel_applet_load_applets_from_gconf (void);
void        panel_applet_save_to_gconf           (AppletInfo *applet_info);
void        panel_applet_save_position           (AppletInfo *applet_info,
						  const char *gconf_key,
						  gboolean    immediate);

void panel_applet_load_defaults_for_screen (PanelGConfKeyType  type,
					    const char        *profile,
					    int                screen);

int         panel_applet_get_position    (AppletInfo *applet);

void        panel_applet_menu_set_recurse (GtkMenu     *menu,
					   const gchar *key,
					   gpointer     data);

G_END_DECLS

#endif
