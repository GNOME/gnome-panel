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

/* we exclude shift, GDK_CONTROL_MASK and GDK_MOD1_MASK since we know what 
   these modifiers mean */
#define ALL_MODS (0x2000 /*Xkb modifier*/ | GDK_LOCK_MASK  | \
	GDK_MOD2_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK) 
    
static void
grab_key (guint mod, guint key)
{
	ModAndKey data;
        int other_mods = ALL_MODS & ~mod;
    
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
	int other_mods = ALL_MODS & ~mod;    
    
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
	gboolean same;

	same = (lastkey_menu == lastkey_run &&
		laststate_menu == laststate_run);

	gdk_error_trap_push();
	if (lastkey_menu)
		ungrab_key (laststate_menu, lastkey_menu);
	if (lastkey_run && !same)
		ungrab_key (laststate_run, lastkey_run);
	
	if (global_config.keys_enabled && 
	    global_config.menu_keysym) {
		lastkey_menu = XKeysymToKeycode(GDK_DISPLAY(),
						global_config.menu_keysym);
		laststate_menu = global_config.menu_state;
		if(lastkey_menu)
		grab_key (laststate_menu, lastkey_menu);
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
			grab_key (laststate_run, lastkey_run);
	} else
		lastkey_run = 0;
	gdk_flush ();
	gdk_error_trap_pop();
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

	if (keycode == menu_keycode &&
	    (state & menu_state) == menu_state) {
		PanelWidget *panel;
		GtkWidget *menu, *basep;
		/* check if anybody else has a grab */
		if (gdk_pointer_grab (GDK_ROOT_PARENT(), FALSE, 
				      0, NULL, NULL, GDK_CURRENT_TIME)
		    != GrabSuccess) {
			return GDK_FILTER_CONTINUE;
		} else {
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
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
		   (state & run_state) == run_state) {
		/* check if anybody else has a grab */
		if (gdk_pointer_grab (GDK_ROOT_PARENT(), FALSE, 
				      0, NULL, NULL, GDK_CURRENT_TIME)
		    != GrabSuccess) {
			return GDK_FILTER_CONTINUE;
		} else {
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
		}

		show_run_dialog ();
		return GDK_FILTER_REMOVE;
	}

	return GDK_FILTER_CONTINUE;
}
