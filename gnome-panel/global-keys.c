#include <config.h>

#include <X11/keysym.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include "global-keys.h"
#include "applet.h"
#include "panel-include.h"

extern GSList *panel_list;

static int
watch_xerrors(Display *disp, XErrorEvent *errev)
{
	return 0;
}

void
panel_global_keys_setup(void)
{
	XErrorHandler old_err_handler;
	
	old_err_handler = XSetErrorHandler (watch_xerrors);
	XGrabKey (GDK_DISPLAY(), XK_LEFT_WIN, AnyModifier, GDK_ROOT_WINDOW(),
		  True, GrabModeAsync, GrabModeAsync);
	XSetErrorHandler (old_err_handler);
}

GdkFilterReturn
panel_global_keys_filter(GdkXEvent *gdk_xevent, GdkEvent *event)
{
	static int winkey_depth = 0, num_subkeys = 0;
	XKeyEvent *kev = gdk_xevent;
	
	switch (kev->type) {
	case KeyPress:
		if(kev->keycode == XK_LEFT_WIN) {
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
			switch(ks) {
			case XK_R:
			case XK_r: 
			{
				char * const mystr[] = {"gnome-run"};
				gnome_execute_async (NULL, 1, mystr);
			}
			break;
			default:
				num_subkeys--;
				break;
			}
			return GDK_FILTER_REMOVE;
		}
		break;
	case KeyRelease:
		if(kev->keycode == XK_LEFT_WIN) {
			winkey_depth--;
			if(!num_subkeys) {
				extern PanelWidget *current_panel;
				GtkWidget *menu;
				GtkWidget *panel;
				GtkWidget *menuitem;
				PanelData *pd;
				extern int base_panels;

				if (!current_panel) {
					panel = GTK_WIDGET (((PanelData *)panel_list->data)->panel);
					current_panel = BASEP_WIDGET (panel)->panel;
				} else {
					panel = PANEL_WIDGET (current_panel)->panel_parent;
				}
				pd = gtk_object_get_user_data (GTK_OBJECT (panel));
				menu = panel_menu_get (pd);
				menuitem = gtk_object_get_data (GTK_OBJECT (menu),
								"remove_item");
				
				if (!IS_DRAWER_WIDGET (panel))
					gtk_widget_set_sensitive(menuitem, base_panels > 1);
				pd->menu_age = 0;

				gtk_menu_popup(GTK_MENU (menu), NULL, NULL,
                                               NULL,
                                               NULL, 0,
                                               ((GdkEventKey *)event)->time);
			}
			return GDK_FILTER_REMOVE;
		}
		break;
	}
	
	return GDK_FILTER_CONTINUE;
}
