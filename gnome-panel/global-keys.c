#include <config.h>

#include <X11/keysym.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include "global-keys.h"
#include "applet.h"
#include "panel-include.h"
#include "gnome-run.h"
#include "panel_config_global.h"

extern GlobalConfig global_config;
extern GSList *panel_list;
extern PanelWidget *current_panel;

static int
watch_xerrors(Display *disp, XErrorEvent *errev)
{
	return 0;
}

void
panel_global_keys_setup(void)
{
	static guint lastkey = 0;
	XErrorHandler old_err_handler;

	old_err_handler = XSetErrorHandler (watch_xerrors);
	if (lastkey)
		XUngrabKey (GDK_DISPLAY(), lastkey, AnyModifier,
			    GDK_ROOT_WINDOW());
	
	if (global_config.keys_enabled) {
		lastkey = global_config.menu_keycode;
		XGrabKey (GDK_DISPLAY(), lastkey, AnyModifier, 
			  GDK_ROOT_WINDOW(), True,
			  GrabModeAsync, GrabModeAsync);
	} else
		lastkey = 0;
	XSetErrorHandler (old_err_handler);
}

GdkFilterReturn
panel_global_keys_filter(GdkXEvent *gdk_xevent, GdkEvent *event)
{
	static int winkey_depth = 0, num_subkeys = 0;
	XKeyEvent *kev = gdk_xevent;
	switch (kev->type) {
	case KeyPress:
		if(kev->keycode == global_config.menu_keycode) {
#ifdef PANEL_DEBUG
			g_message ("(%ld) %d [%x]: %s", 
				   XKeycodeToKeysym (GDK_DISPLAY(),
						     kev->keycode, 1),
				   kev->keycode, kev->keycode,
				   gdk_keyval_name (kev->keycode));
#endif
			winkey_depth++;
			num_subkeys = 0;
			return GDK_FILTER_REMOVE;
		}
		else if(winkey_depth > 0) {
			KeySym ks;
			
			ks = XKeycodeToKeysym (GDK_DISPLAY (), 
					       kev->keycode, 0);
			
			num_subkeys++;
			
			/* Do Win+key hotkeys */
			if (kev->keycode == global_config.run_keycode)
				show_run_dialog ();
			else
				num_subkeys--;
			return GDK_FILTER_REMOVE;
		}
		break;
	case KeyRelease:
		if(kev->keycode == global_config.menu_keycode) {
			winkey_depth--;
			if(!num_subkeys) {
				GtkWidget *menu = 
					make_popup_panel_menu ();
				GtkWidget *basep = current_panel->panel_parent;
				if (IS_BASEP_WIDGET(basep)) {
					BASEP_WIDGET(basep)->autohide_inhibit 
						= TRUE;
					basep_widget_autohide (
						BASEP_WIDGET (basep));
				}
				gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
						NULL, NULL, 0, GDK_CURRENT_TIME);
			}
			return GDK_FILTER_REMOVE;
		}
		break;
	}
	
	return GDK_FILTER_CONTINUE;
}
