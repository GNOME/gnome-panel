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

#define N_BITS 32 /*all modifier masks fit into it */

typedef struct {
	guint mods;
	guint key;
} ModAndKey;
typedef void (*BitsCallback) (guint value, ModAndKey *mod_and_key);


static void
all_combinations (guint mask_to_traverse,
		  BitsCallback callback,
		  ModAndKey *mod_and_key)
{
	int indexes[N_BITS];/*indexes of bits we need to flip*/
	int i, bit, bits_set_cnt;
	int uppervalue;

	bit = 0;
	for (i = 0; i < N_BITS; i++) {
		if (mask_to_traverse & (1<<i))
			indexes[bit++]=i;
	}

	bits_set_cnt = bit;

	uppervalue = 1<<bits_set_cnt;
	for (i = 0; i < uppervalue; i++) {
		int j, result = 0;

		for (j = 0; j < bits_set_cnt; j++) {
			if (i & (1<<j))
				result |= (1<<indexes[j]);
		}
		callback (result, mod_and_key);
	}
}

static void
do_grab_key (guint mod, ModAndKey* data)
{
	XGrabKey (GDK_DISPLAY(), data->key, (mod | data->mods),
		  GDK_ROOT_WINDOW(), True, GrabModeAsync, GrabModeAsync);
}

static void
grab_key (guint mod, guint key)
{
	ModAndKey data;
        int other_mods = IGNORED_MODS & ~mod;
    
	data.mods = mod;
        data.key = key;
    
	all_combinations (other_mods, do_grab_key, &data);
}

static void
do_ungrab_key (guint mod, ModAndKey* data)
{
	XUngrabKey(GDK_DISPLAY(), data->key, (mod | data->mods),
		   GDK_ROOT_WINDOW());
}

static void 
ungrab_key (guint mod,guint key)
{
	ModAndKey data;
	int other_mods = IGNORED_MODS & ~mod;    
    
	data.mods = mod;
	data.key = key;
    
	all_combinations(other_mods, do_ungrab_key, &data);
}

void
panel_global_keys_setup (void)
{
	static guint lastkey_menu = 0;
	static guint laststate_menu = 0;
	static guint lastkey_run = 0;
	static guint laststate_run = 0;
	static guint lastkey_screenshot = 0;
	static guint laststate_screenshot = 0;
	static guint lastkey_window_screenshot = 0;
	static guint laststate_window_screenshot = 0;

	/* FIXME: the if trees are horrible, this shoul dbe cleaned up with
	 * lists or something */

	gdk_error_trap_push();
	if (lastkey_menu != 0) {
		ungrab_key (laststate_menu, lastkey_menu);
	}
	if (lastkey_run != 0 &&
	    (lastkey_menu != lastkey_run ||
	     laststate_menu != laststate_run)) {
		ungrab_key (laststate_run, lastkey_run);
	}
	if (lastkey_run != 0 &&
	    (lastkey_menu != lastkey_screenshot ||
	     laststate_menu != laststate_screenshot) &&
	    (lastkey_run != lastkey_screenshot ||
	     laststate_run != laststate_screenshot)) {
		ungrab_key (laststate_screenshot, lastkey_screenshot);
	}
	if (lastkey_run != 0 &&
	    (lastkey_menu != lastkey_window_screenshot ||
	     laststate_menu != laststate_window_screenshot) &&
	    (lastkey_run != lastkey_window_screenshot ||
	     laststate_run != laststate_window_screenshot) &&
	    (lastkey_screenshot != lastkey_window_screenshot ||
	     laststate_screenshot != laststate_window_screenshot)) {
		ungrab_key (laststate_window_screenshot,
			    lastkey_window_screenshot);
	}
	
	if (global_config.keys_enabled && 
	    global_config.menu_keysym) {
		lastkey_menu = XKeysymToKeycode(GDK_DISPLAY(),
						global_config.menu_keysym);
		laststate_menu = global_config.menu_state;
		if (lastkey_menu != 0)
			grab_key (laststate_menu, lastkey_menu);
	} else {
		lastkey_menu = 0;
	}

	if (global_config.keys_enabled && 
	    global_config.run_keysym) {
		lastkey_run = XKeysymToKeycode (GDK_DISPLAY (),
						global_config.run_keysym);
		laststate_run = global_config.run_state;
		if (lastkey_run != 0 &&
		    (lastkey_menu != lastkey_run ||
		     laststate_menu != laststate_run))
			grab_key (laststate_run, lastkey_run);
	} else {
		lastkey_run = 0;
	}

	if (global_config.keys_enabled && 
	    global_config.screenshot_keysym) {
		lastkey_screenshot = XKeysymToKeycode
			(GDK_DISPLAY (), global_config.screenshot_keysym);
		laststate_screenshot = global_config.screenshot_state;
		if (lastkey_screenshot != 0 &&
		    (lastkey_menu != lastkey_screenshot ||
		     laststate_menu != laststate_screenshot) &&
		    (lastkey_run != lastkey_screenshot ||
		     laststate_run != laststate_screenshot))
			grab_key (laststate_screenshot, lastkey_screenshot);
	} else {
		lastkey_screenshot = 0;
	}

	if (global_config.keys_enabled && 
	    global_config.window_screenshot_keysym) {
		lastkey_window_screenshot = XKeysymToKeycode
			(GDK_DISPLAY (), global_config.window_screenshot_keysym);
		laststate_window_screenshot = global_config.window_screenshot_state;
		if (lastkey_window_screenshot != 0 &&
		    (lastkey_menu != lastkey_window_screenshot ||
		     laststate_menu != laststate_window_screenshot) &&
		    (lastkey_run != lastkey_window_screenshot ||
		     laststate_run != laststate_window_screenshot) &&
		    (lastkey_screenshot != lastkey_window_screenshot ||
		     laststate_screenshot != laststate_window_screenshot))
			grab_key (laststate_window_screenshot,
				  lastkey_window_screenshot);
	} else {
		lastkey_window_screenshot = 0;
	}

	gdk_flush ();
	gdk_error_trap_pop();
}

static gboolean
check_for_grabs (void)
{
	if (gdk_pointer_grab (GDK_ROOT_PARENT(), FALSE, 
			      0, NULL, NULL, GDK_CURRENT_TIME)
	    != GrabSuccess) {
		return TRUE;
	} else {
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
		return FALSE;
	}
}

GdkFilterReturn
panel_global_keys_filter (GdkXEvent *gdk_xevent,
			  GdkEvent *event,
			  gpointer data)
{
	XEvent *xevent = (XEvent *)gdk_xevent;
	guint keycode, state;
	guint menu_keycode, menu_state;
	guint run_keycode, run_state;
	guint screenshot_keycode, screenshot_state;
	guint window_screenshot_keycode, window_screenshot_state;

	if(xevent->type != KeyPress)
		return GDK_FILTER_CONTINUE;

	keycode = xevent->xkey.keycode;
	state = xevent->xkey.state;

	menu_keycode = XKeysymToKeycode (GDK_DISPLAY (),
					 global_config.menu_keysym);
	menu_state = global_config.menu_state;

	run_keycode = XKeysymToKeycode (GDK_DISPLAY (),
					global_config.run_keysym);
	run_state = global_config.run_state;

	screenshot_keycode = XKeysymToKeycode (GDK_DISPLAY (),
					       global_config.screenshot_keysym);
	screenshot_state = global_config.screenshot_state;

	window_screenshot_keycode =
		XKeysymToKeycode (GDK_DISPLAY (),
				  global_config.window_screenshot_keysym);
	window_screenshot_state = global_config.window_screenshot_state;

	if (keycode == menu_keycode &&
	    (state & USED_MODS) == menu_state) {
		PanelWidget *panel;
		GtkWidget *menu, *basep;
		/* check if anybody else has a grab */
		if (check_for_grabs ()) {
			return GDK_FILTER_CONTINUE;
		}

		panel = panels->data;
		menu = make_popup_panel_menu (panel);
		basep = panel->panel_parent;
		if (IS_BASEP_WIDGET(basep)) {
			BASEP_WIDGET(basep)->autohide_inhibit = TRUE;
			basep_widget_autohide (BASEP_WIDGET (basep));
		}
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
				NULL, NULL, 0, GDK_CURRENT_TIME);
		return GDK_FILTER_REMOVE;
	} else if (keycode == run_keycode &&
		   (state & USED_MODS) == run_state) {
		/* check if anybody else has a grab */
		if (check_for_grabs ()) {
			return GDK_FILTER_CONTINUE;
		}

		show_run_dialog ();
		return GDK_FILTER_REMOVE;
	} else if (keycode == screenshot_keycode &&
		   (state & USED_MODS) == screenshot_state) {
		char *argv[2];
		char *proggie;

		/* check if anybody else has a grab */
		if (check_for_grabs ()) {
			return GDK_FILTER_CONTINUE;
		}

		proggie = gnome_is_program_in_path ("gnome-panel-screenshot");
		if (proggie == NULL) {
			panel_error_dialog (_("Can't find the screenshot "
					      "program"));
			return GDK_FILTER_REMOVE;
		}
		argv[0] = proggie;
		argv[1] = NULL;

		if (gnome_execute_async (g_get_home_dir (), 1, argv)<0)
			panel_error_dialog (_("Can't execute the screenshot "
					      "program"));

		g_free (proggie);

		return GDK_FILTER_REMOVE;
	} else if (keycode == window_screenshot_keycode &&
		   (state & USED_MODS) == window_screenshot_state) {
		char *argv[3];
		char *proggie;

		/* check if anybody else has a grab */
		if (check_for_grabs ()) {
			return GDK_FILTER_CONTINUE;
		}

		proggie = gnome_is_program_in_path ("gnome-panel-screenshot");
		if (proggie == NULL) {
			panel_error_dialog (_("Can't find the screenshot "
					      "program"));
			return GDK_FILTER_REMOVE;
		}
		argv[0] = proggie;
		argv[1] = "--window";
		argv[2] = NULL;

		if (gnome_execute_async (g_get_home_dir (), 2, argv)<0)
			panel_error_dialog (_("Can't execute the screenshot "
					      "program"));

		g_free (proggie);

		return GDK_FILTER_REMOVE;
	}

	return GDK_FILTER_CONTINUE;
}
