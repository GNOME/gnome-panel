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

#include "applet.h"
#include "panel-widget.h"

#include <libgnome/gnome-desktop-item.h>

G_BEGIN_DECLS

#define PANEL_LAUNCHERS_PATH "panel2.d/default/launchers"

typedef struct {
	AppletInfo        *info;
	GtkWidget         *button;
	GtkWidget	  *dedit;
	GnomeDesktopItem *ditem;
	GnomeDesktopItem *revert_ditem;

	GtkWidget         *prop_dialog;

	gulong             icon_changed_signal;
	gulong             destroy_handler;
} Launcher;

/* If launchers are loaded from ANYWHERE but the normal panel
 * launcher location, do HOARD the file (with launcher_hoard),
 * otherwise things may be removed from disk when they shouldn't
 * etc.  Also always hoard if an applet which owns a desktop already
 * exists.*/
Launcher *	load_launcher_applet_full	(const char       *params,
						 GnomeDesktopItem *ditem,
						 PanelWidget      *panel,
						 int               pos,
						 gboolean          exactpos,
						 const char       *gconf_key);

Launcher *	load_launcher_applet		(const char       *params,
						 PanelWidget      *panel,
						 int              pos,
						 gboolean         exactpos,
						 const char      *gconf_key);

Launcher *	load_launcher_applet_from_info	(const char *name,
						 const char *comment,
						 const char *exec,
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

void		launcher_properties		(Launcher  *launcher,
						 GdkScreen *screen);

void            launcher_save_to_gconf          (Launcher   *launcher,
						 const char *gconf_key);

void            launcher_load_from_gconf        (PanelWidget *panel_widget,
						 gint         position,
						 const char  *gconf_key);

void		ask_about_launcher		(const char *file,
						 PanelWidget *panel,
						 int pos,
						 gboolean exactpos);

/* always make a new unique filename, useful for converting OLD configs */
void		launcher_hoard			(Launcher *launcher);

Launcher *	find_launcher			(const char *path);

void            launcher_show_help              (Launcher  *launcher,
						 GdkScreen *screen);

void            launcher_properties_destroy     (Launcher *launcher);

G_END_DECLS

#endif
