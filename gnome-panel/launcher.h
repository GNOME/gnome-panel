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

	gulong             destroy_handler;

	/* If we can't hoard, and it's not hoarded
	   already, then we can't write this launcher */
	gboolean           non_writable;
} Launcher;

void panel_launcher_create           (PanelToplevel *toplevel,
				      int            position,
				      const char    *location);
void panel_launcher_create_with_id   (const char    *toplevel_id,
				      int            position,
				      const char    *location);
void panel_launcher_create_copy      (PanelToplevel *toplevel,
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
						 gboolean     locked,
						 gint         position,
						 const char  *id);

void            panel_launcher_delete           (Launcher *launcher);

void		ask_about_launcher		(const char *file,
						 PanelWidget *panel,
						 int pos,
						 gboolean exactpos);

Launcher *	find_launcher			(const char *path);

void            launcher_show_help              (Launcher  *launcher,
						 GdkScreen *screen);

void            launcher_properties_destroy     (Launcher *launcher);

void            panel_launcher_set_dnd_enabled  (Launcher *launcher,
						 gboolean  dnd_enabled);


G_END_DECLS

#endif
