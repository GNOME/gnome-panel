#ifndef GLOBAL_KEYS_H
#define GLOBAL_KEYS_H 1

/* we exclude shift, GDK_CONTROL_MASK and GDK_MOD1_MASK since we know what 
   these modifiers mean */
/* these are the mods whose combinations are bound by the keygrabbing code */
#define IGNORED_MODS (0x2000 /*Xkb modifier*/ | GDK_LOCK_MASK  | \
	GDK_MOD2_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK) 
/* these are the ones we actually use for global keys, we always only check
 * for these set */
#define USED_MODS (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)
    

/* WTF are these doing here */
/*#define XK_LEFT_WIN 115
#define XK_RIGHT_WIN 116
#define XK_RIGHT_MENU 117*/

#include <gdk/gdktypes.h>

void		panel_global_keys_setup		(void);
GdkFilterReturn	panel_global_keys_filter	(GdkXEvent *gdk_xevent,
						 GdkEvent *event,
						 gpointer data);

#endif
