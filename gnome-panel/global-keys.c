#include <config.h>

#include <X11/keysym.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>

#include <libgnome/libgnome.h>

#include "global-keys.h"

#include "applet.h"
#include "foobar-widget.h"
#include "gnome-run.h"
#include "panel.h"
#include "menu.h"
#include "panel-util.h"
#include "egg-screen-exec.h"
#include "eggaccelerators.h"

extern GlobalConfig global_config;
extern GSList *panels;

#define N_BITS 32 /*all modifier masks fit into it */

typedef struct {
	guint mods;
	guint key;
} ModAndKey;
typedef void (*BitsCallback) (guint value, ModAndKey *mod_and_key);

static guint
get_ignored_mods (void)
{
  guint ignored_mods;

  ignored_mods = 0;
  egg_keymap_resolve_virtual_modifiers (gdk_keymap_get_default (),
                                        EGG_VIRTUAL_NUM_LOCK_MASK |
                                        EGG_VIRTUAL_SCROLL_LOCK_MASK,
                                        &ignored_mods);

  return ignored_mods;
}

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
        guint ignored_mods;
        int other_mods;

        ignored_mods = get_ignored_mods ();
        
        other_mods = ignored_mods & ~mod;
    
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
        guint ignored_mods;
        int other_mods;

        /* FIXME this is broken as the modifiers may have changed
         * between the grab and the ungrab.
         */
        ignored_mods = get_ignored_mods ();
        
        other_mods = ignored_mods & ~mod;
    
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
	    global_config.menu_key.keysym) {
		lastkey_menu = XKeysymToKeycode(GDK_DISPLAY(),
						global_config.menu_key.keysym);
		laststate_menu = global_config.menu_key.state;
		if (lastkey_menu != 0)
			grab_key (laststate_menu, lastkey_menu);
	} else {
		lastkey_menu = 0;
	}

	if (global_config.keys_enabled && 
	    global_config.run_key.keysym) {
		lastkey_run = XKeysymToKeycode (GDK_DISPLAY (),
						global_config.run_key.keysym);
		laststate_run = global_config.run_key.state;
		if (lastkey_run != 0 &&
		    (lastkey_menu != lastkey_run ||
		     laststate_menu != laststate_run))
			grab_key (laststate_run, lastkey_run);
	} else {
		lastkey_run = 0;
	}

	if (global_config.keys_enabled && 
	    global_config.screenshot_key.keysym) {
		lastkey_screenshot = XKeysymToKeycode
			(GDK_DISPLAY (), global_config.screenshot_key.keysym);
		laststate_screenshot = global_config.screenshot_key.state;
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
	    global_config.window_screenshot_key.keysym) {
		lastkey_window_screenshot = XKeysymToKeycode
			(GDK_DISPLAY (), global_config.window_screenshot_key.keysym);
		laststate_window_screenshot = global_config.window_screenshot_key.state;
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
	if (gdk_pointer_grab (gdk_get_default_root_window (), FALSE, 
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
			  GdkEvent  *event,
			  GdkScreen *screen)
{
	XEvent *xevent = (XEvent *)gdk_xevent;
	guint keycode, state;
	guint menu_keycode, menu_state;
	guint run_keycode, run_state;
	guint screenshot_keycode, screenshot_state;
	guint window_screenshot_keycode, window_screenshot_state;
        guint ignored_mods;

	g_return_val_if_fail (GDK_IS_SCREEN (screen), GDK_FILTER_CONTINUE);
        
	if(xevent->type != KeyPress)
		return GDK_FILTER_CONTINUE;

        ignored_mods = get_ignored_mods ();
        
	keycode = xevent->xkey.keycode;
	state = xevent->xkey.state;

	menu_keycode = XKeysymToKeycode (GDK_DISPLAY (),
					 global_config.menu_key.keysym);
	menu_state = global_config.menu_key.state;

	run_keycode = XKeysymToKeycode (GDK_DISPLAY (),
					global_config.run_key.keysym);
	run_state = global_config.run_key.state;

	screenshot_keycode = XKeysymToKeycode (GDK_DISPLAY (),
					       global_config.screenshot_key.keysym);
	screenshot_state = global_config.screenshot_key.state;

	window_screenshot_keycode =
		XKeysymToKeycode (GDK_DISPLAY (),
				  global_config.window_screenshot_key.keysym);
	window_screenshot_state = global_config.window_screenshot_key.state;

	if (keycode == menu_keycode &&
	    (state & (~ignored_mods)) == menu_state) {
		PanelWidget *panel_widget;
		GtkWidget   *panel;
		GtkWidget   *menu;

		/* check if anybody else has a grab */
		if (check_for_grabs ())
			return GDK_FILTER_CONTINUE;

		panel_widget = panels->data;
		menu = create_panel_root_menu (panel_widget);
		panel = panel_widget->panel_parent;

		if (BASEP_IS_WIDGET (panel)) {
			BASEP_WIDGET (panel)->autohide_inhibit = TRUE;
			basep_widget_autohide (BASEP_WIDGET (panel));
		}

		gtk_menu_set_screen (GTK_MENU (menu), screen);
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
				NULL, NULL, 0, GDK_CURRENT_TIME);
		return GDK_FILTER_REMOVE;
	} else if (keycode == run_keycode &&
		   (state & (~ignored_mods)) == run_state) {
		/* check if anybody else has a grab */
		if (check_for_grabs ()) {
			return GDK_FILTER_CONTINUE;
		}

		show_run_dialog (screen);
		return GDK_FILTER_REMOVE;
	} else if (keycode == screenshot_keycode &&
		   (state & (~ignored_mods)) == screenshot_state) {
		char *argv [2];
		char *proggie;

		/* check if anybody else has a grab */
		if (check_for_grabs ()) {
			return GDK_FILTER_CONTINUE;
		}

		proggie = g_find_program_in_path  ("gnome-panel-screenshot");
		if (proggie == NULL) {
			panel_error_dialog (
				screen,
				"cannot_find_ss_program",
				_("Can't find the screenshot program"));
			return GDK_FILTER_REMOVE;
		}
		argv[0] = proggie;
		argv[1] = NULL;

		if (egg_screen_execute_async (screen, g_get_home_dir (), 1, argv) < 0)
			panel_error_dialog (screen,
					    "cannot_exec_ss_program",
					    _("Can't execute the screenshot program"));

		g_free (proggie);

		return GDK_FILTER_REMOVE;
	} else if (keycode == window_screenshot_keycode &&
		   (state & (~ignored_mods)) == window_screenshot_state) {
		char *argv [3];
		char *proggie;

		/* check if anybody else has a grab */
		if (check_for_grabs ()) {
			return GDK_FILTER_CONTINUE;
		}

		proggie = g_find_program_in_path  ("gnome-panel-screenshot");
		if (proggie == NULL) {
			panel_error_dialog (
				screen,
				"cannot_find_ss_program",
				_("Can't find the screenshot program"));
			return GDK_FILTER_REMOVE;
		}
		argv[0] = proggie;
		argv[1] = "--window";
		argv[2] = NULL;

		if (egg_screen_execute_async (screen, g_get_home_dir (), 2, argv) < 0)
			panel_error_dialog (screen,
					    "cannot_exec_ss_program",
					    _("Can't execute the screenshot program"));

		g_free (proggie);

		return GDK_FILTER_REMOVE;
	}

	return GDK_FILTER_CONTINUE;
}
