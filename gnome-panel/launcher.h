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
	AppletInfo        *info;
	GtkWidget         *button;
	GtkObject	  *dedit;
	GnomeDesktopEntry *dentry;
	GnomeDesktopEntry *revert_dentry;

	GtkWidget         *prop_dialog;
} Launcher;

/* If launchers are loaded from ANYWHERE but the normal panel
 * launcher location, do HOARD the file (with launcher_hoard),
 * otherwise things may be removed from disk when they shouldn't
 * etc.  Also always hoard if an applet which owns a desktop already
 * exists.*/
Launcher *	load_launcher_applet_full	(const char *params,
						 GnomeDesktopEntry *dentry,
						 PanelWidget *panel,
						 int pos,
						 gboolean exactpos);
Launcher *	load_launcher_applet		(const char *params,
						 PanelWidget *panel,
						 int pos,
						 gboolean exactpos);
Launcher *	load_launcher_applet_from_info	(const char *name,
						 const char *comment,
						 char **exec,
						 int execn,
						 const char *icon,
						 PanelWidget *panel,
						 int pos,
						 gboolean exactpos);
Launcher *	load_launcher_applet_from_info_url(const char *name,
						   const char *comment,
						   const char *url,
						   const char *icon,
						   PanelWidget *panel,
						   int pos,
						   gboolean exactpos);
void		launcher_properties		(Launcher *launcher);

void		ask_about_launcher		(const char *file,
						 PanelWidget *panel,
						 int pos,
						 gboolean exactpos);

/* if location == NULL make a new filename, convert to gnome style and
 * save */
void		launcher_save			(Launcher *launcher);
/* always make a new unique filename, useful for converting OLD configs */
void		launcher_hoard			(Launcher *launcher);

Launcher *	find_launcher			(const char *path);

/* make file name from base */
char *		launcher_file_name		(const char *base);

END_GNOME_DECLS

#endif
