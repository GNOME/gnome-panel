/*
 * GNOME panel launcher module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 * CORBAized by George Lebl
 * de-CORBAized by George Lebl
 */

#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <panel-widget.h>

BEGIN_GNOME_DECLS

typedef struct {
	int                applet_id;
	GtkWidget         *button;
	int                signal_click_tag;
	GtkObject	  *dedit;
	GnomeDesktopEntry *dentry;
} Launcher;

void load_launcher_applet(char *params, PanelWidget *panel, int pos);
void launcher_properties(Launcher *launcher);

void ask_about_launcher(void);

END_GNOME_DECLS

#endif
