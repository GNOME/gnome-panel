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
	GtkWidget         *button;
	GtkObject	  *dedit;
	GnomeDesktopEntry *dentry;
} Launcher;

void load_launcher_applet(char *params, PanelWidget *panel, int pos);
void load_launcher_applet_from_info(char *name, char *comment,
				    char **exec, int execn, char *icon,
				    PanelWidget *panel, int pos);
void launcher_properties(Launcher *launcher);

void ask_about_launcher(char *file, PanelWidget *panel, int pos);

END_GNOME_DECLS

#endif
