#ifndef GLOBAL_KEYS_H
#define GLOBAL_KEYS_H 1

#define XK_LEFT_WIN 115
#define XK_RIGHT_WIN 116
#define XK_RIGHT_MENU 117

#include <gdk/gdktypes.h>

void		panel_global_keys_setup		(void);
GdkFilterReturn	panel_global_keys_filter	(GdkXEvent *gdk_xevent,
						 GdkEvent *event,
						 gpointer data);

#endif
