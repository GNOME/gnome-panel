#ifndef APPLET_H
#define APPLET_H

#include <glib.h>
#include <gdk/gdk.h>

#include "panel-enums.h"
#include "panel-widget.h"

G_BEGIN_DECLS

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)
typedef struct {
	PanelObjectType  type;
	GtkWidget       *widget;
	GSettings       *settings;

	GtkWidget       *menu;
	GtkWidget       *edit_menu;
	GList           *user_menu;

	gpointer         data;
	GDestroyNotify   data_destroy;

	char            *id;
} AppletInfo;

typedef gboolean (* CallbackEnabledFunc) (void);

typedef struct {
	char                *name;
	char                *text;

	CallbackEnabledFunc  is_enabled_func;

	int                  sensitive;
	AppletInfo          *info;

	GtkWidget           *menuitem;
	GtkWidget           *submenu;
} AppletUserMenu;

AppletInfo *panel_applet_register    (GtkWidget       *applet,
				      PanelWidget     *panel,
				      PanelObjectType  type,
				      const char      *id,
				      GSettings       *settings,
				      gpointer         data,
				      GDestroyNotify   data_destroy);

const char *panel_applet_get_id           (AppletInfo      *info);

gboolean    panel_applet_activate_main_menu (guint32 activate_time);

GSList     *panel_applet_list_applets (void);

void        panel_applet_clean        (AppletInfo    *info);

void            panel_applet_add_callback    (AppletInfo          *info,
					      const gchar         *callback_name,
					      const gchar         *menuitem_text,
					      CallbackEnabledFunc  is_enabled_func);

void        panel_applet_save_position           (AppletInfo *applet_info,
						  const char *id,
						  gboolean    immediate);

/* True if all the keys relevant to moving are writable
   (position, toplevel_id, panel_right_stick) */
gboolean    panel_applet_can_freely_move (AppletInfo *applet);

G_END_DECLS

#endif
