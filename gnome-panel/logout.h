/*
 * GNOME panel logout module.
 *
 * Original author unknown. CORBAized by Elliot Lee * 
 * de-CORBAized by George Lebl
 */

#ifndef LOGOUT_H
#define LOGOUT_H

#include <panel-widget.h>

G_BEGIN_DECLS

void load_logout_applet (PanelWidget *panel,
			 int          pos,
			 gboolean     exactpos,
			 const char  *gconf_key);

void load_lock_applet   (PanelWidget *panel,
			 int          pos,
			 gboolean     exactpos,
			 const char  *gconf_key);

G_END_DECLS

#endif
