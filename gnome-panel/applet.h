#ifndef APPLET_H
#define APPLET_H

#include <glib.h>
#include <gdk/gdk.h>
#include "panel-widget.h"
#include "panel-gconf.h"
#include "panel-enums.h"

G_BEGIN_DECLS


#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)
typedef struct {
	PanelObjectType  type;
	GtkWidget       *widget;

	GtkWidget       *menu;
	GtkWidget       *move_item;
	GList           *user_menu;

	gpointer         data;
	GDestroyNotify   data_destroy;

	char            *id;
} AppletInfo;

typedef gboolean (* CallbackEnabledFunc) (void);

typedef struct {
	char                *name;
	char                *stock_item;
	char                *text;

	CallbackEnabledFunc  is_enabled_func;

	int                  sensitive;
	AppletInfo          *info;

	GtkWidget           *menuitem;
	GtkWidget           *submenu;
} AppletUserMenu;

AppletInfo *panel_applet_register    (GtkWidget       *applet,
				      gpointer         data,
				      GDestroyNotify   data_destroy,
				      PanelWidget     *panel,
				      gboolean         locked,
				      gint             pos,
				      gboolean         exactpos,
				      PanelObjectType  type,
				      const char      *id);
void panel_applet_stop_loading (const char *id);

const char *panel_applet_get_id           (AppletInfo      *info);
const char *panel_applet_get_id_by_widget (GtkWidget       *widget);
AppletInfo *panel_applet_get_by_id        (const char      *id);
AppletInfo *panel_applet_get_by_type      (PanelObjectType  object_type, GdkScreen *screen);

GSList     *panel_applet_list_applets (void);

void        panel_applet_clean        (AppletInfo    *info);

void panel_applet_queue_applet_to_load (const char      *id,
					PanelObjectType  type,
					const char      *toplevel_id,
					int              position,
					gboolean         right_stick,
					gboolean         locked);
void panel_applet_load_queued_applets  (gboolean initial_load);
gboolean panel_applet_on_load_queue    (const char *id);


void            panel_applet_add_callback    (AppletInfo          *info,
					      const gchar         *callback_name,
					      const gchar         *stock_item,
					      const gchar         *menuitem_text,
					      CallbackEnabledFunc  is_enabled_func);

AppletUserMenu *panel_applet_get_callback    (GList       *user_menu,
					      const gchar *name);


void        panel_applet_save_position           (AppletInfo *applet_info,
						  const char *id,
						  gboolean    immediate);

int         panel_applet_get_position    (AppletInfo *applet);

/* True if all the keys relevant to moving are writable
   (position, toplevel_id, panel_right_stick) */
gboolean    panel_applet_can_freely_move (AppletInfo *applet);

/* True if the locked flag is writable */
gboolean    panel_applet_lockable (AppletInfo *applet);

GtkWidget  *panel_applet_create_menu (AppletInfo *info);

void        panel_applet_menu_set_recurse (GtkMenu     *menu,
					   const gchar *key,
					   gpointer     data);

gboolean    panel_applet_toggle_locked  (AppletInfo *info);

void panel_applet_position_menu (GtkMenu   *menu,
				 int       *x,
				 int       *y,
				 gboolean  *push_in,
				 GtkWidget *applet);
G_END_DECLS

#endif
