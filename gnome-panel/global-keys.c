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
extern GSList *panels;

static int
watch_xerrors(Display *disp, XErrorEvent *errev)
{
	return 0;
}

void
panel_global_keys_setup(void)
{
	static guint lastkey_menu = 0;
	static guint laststate_menu = 0;
	static guint lastkey_run = 0;
	static guint laststate_run = 0;
	XErrorHandler old_err_handler;
	gboolean same;

	same = (lastkey_menu == lastkey_run &&
		laststate_menu == laststate_run);

	old_err_handler = XSetErrorHandler (watch_xerrors);
	if (lastkey_menu)
		XUngrabKey (GDK_DISPLAY(), lastkey_menu, laststate_menu,
			    GDK_ROOT_WINDOW());
	if (lastkey_run && !same)
		XUngrabKey (GDK_DISPLAY(), lastkey_run, laststate_run,
			    GDK_ROOT_WINDOW());
	
	if (global_config.keys_enabled && 
	    global_config.menu_keysym) {
		lastkey_menu = XKeysymToKeycode(GDK_DISPLAY(),
						global_config.menu_keysym);
		laststate_menu = global_config.menu_state;
		if(lastkey_menu)
			XGrabKey (GDK_DISPLAY(), lastkey_menu, laststate_menu, 
				  GDK_ROOT_WINDOW(), True,
				  GrabModeAsync, GrabModeAsync);
	} else
		lastkey_menu = 0;

	if (global_config.keys_enabled && 
	    global_config.run_keysym) {
		lastkey_run = XKeysymToKeycode(GDK_DISPLAY(),
						global_config.run_keysym);
		laststate_run = global_config.run_state;
		if(lastkey_run &&
		   (lastkey_menu != lastkey_run ||
		    laststate_menu != laststate_run))
			XGrabKey (GDK_DISPLAY(), lastkey_run, laststate_run, 
				  GDK_ROOT_WINDOW(), True,
				  GrabModeAsync, GrabModeAsync);
	} else
		lastkey_run = 0;
	XSetErrorHandler (old_err_handler);
}

GdkFilterReturn
panel_global_keys_filter(GdkXEvent *gdk_xevent, GdkEvent *event)
{
	XKeyEvent *kev = gdk_xevent;
	switch (kev->type) {
#if 0
	case KeyPress:
		g_message ("PRESS (%ld) %d [%x]: %s", 
			   (long)event->key.keyval,
			   (int)kev->keycode, (int)kev->keycode,
			   gdk_keyval_name (event->key.keyval));
		/* gdk has already translated to keysym for us */
		if(event->key.keyval == global_config.menu_keysym &&
		   event->key.state == global_config.menu_state) {
			/* check if anybody else has a grab */
			if (gdk_pointer_grab (GDK_ROOT_PARENT(), FALSE, 
					      0, NULL, NULL, GDK_CURRENT_TIME)
			    != GrabSuccess) {
				return GDK_FILTER_CONTINUE;
			} else {
				gdk_pointer_ungrab (GDK_CURRENT_TIME);
			}

#ifdef PANEL_DEBUG
			g_message ("(%ld) %d [%x]: %s", 
				   event->key.keyval,
				   kev->keycode, kev->keycode,
				   gdk_keyval_name (event->key.keyval));
#endif
			winkey_depth++;
			num_subkeys = 0;
			return GDK_FILTER_REMOVE;
		} else if(winkey_depth > 0 &&
			  event->key.keyval == global_config.run_keysym &&
			  event->key.state == global_config.run_state) {
			/* Do Win+key hotkeys */

			num_subkeys++;
			show_run_dialog ();
			return GDK_FILTER_REMOVE;
		}
		break;
#endif
	case KeyRelease:
		g_message ("RELEASE (%ld) %d [%x]: %s", 
			   (long)event->key.keyval,
			   (int)kev->keycode, (int)kev->keycode,
			   gdk_keyval_name (event->key.keyval));
		if(event->key.keyval == global_config.menu_keysym &&
		   event->key.state == global_config.menu_state) {
			/* check if anybody else has a grab */
			if (gdk_pointer_grab (GDK_ROOT_PARENT(), FALSE, 
					      0, NULL, NULL, GDK_CURRENT_TIME)
			    != GrabSuccess) {
				return GDK_FILTER_CONTINUE;
			} else {
				gdk_pointer_ungrab (GDK_CURRENT_TIME);
			}

			{
				PanelWidget *panel = panels->data;
				GtkWidget *menu = 
					make_popup_panel_menu (panel);
				GtkWidget *basep = panel->panel_parent;
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
		} else if(event->key.keyval == global_config.run_keysym &&
			  event->key.state == global_config.run_state) {
			/* Do Win+key hotkeys */

			show_run_dialog ();
			return GDK_FILTER_REMOVE;
		}
		break;
	}
	
	return GDK_FILTER_CONTINUE;
}
