/*
 * GNOME panel logout module.
 *
 * Original author unknown. CORBAized by Elliot Lee * 
 * de-CORBAized by George Lebl
 */

#ifndef LOGOUT_H
#define LOGOUT_H

#include <panel-widget.h>

BEGIN_GNOME_DECLS

void load_logout_applet(PanelWidget *panel, int pos, gboolean exactpos);
void load_lock_applet(PanelWidget *panel, int pos, gboolean exactpos);

END_GNOME_DECLS

#endif
