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

void panel_launcher_create           (PanelToplevel *toplevel,
				      int            position,
				      const char    *location);
void panel_launcher_create_from_info (PanelToplevel *toplevel,
				      int            position,
				      gboolean       exec_info,
				      const char    *exec_or_uri,
				      const char    *name,
				      const char    *comment,
				      const char    *icon);

void		launcher_properties		(Launcher  *launcher,
						 GdkScreen *screen);

void            launcher_load_from_gconf        (PanelWidget *panel_widget,
						 gint         position,
						 const char  *id);

void		ask_about_launcher		(const char *file,
						 PanelWidget *panel,
						 int pos,
						 gboolean exactpos);

Launcher *	find_launcher			(const char *path);

void            launcher_show_help              (Launcher  *launcher,
						 GdkScreen *screen);

void            launcher_properties_destroy     (Launcher *launcher);

G_END_DECLS

#endif
