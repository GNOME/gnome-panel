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

typedef struct {
	int                applet_id;
	GtkWidget         *button;
	gint               signal_click_tag;
	GnomeDesktopEntry *dentry;
} Launcher;

Launcher * create_launcher (char *parameters);
void launcher_properties(Launcher *launcher);

#endif
