/*
 * panel-compatibility.c: panel backwards compatibility support
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-compatibility.h"

#include <libgnome/gnome-i18n.h>

#include "panel-menu-bar.h"
#include "panel-applet-frame.h"
#include "panel-globals.h"

/* Incompatibilities:
 *
 *   Toplevels:
 *     + toplevel_id_list instead of panel_id_list.
 *     + the schemas for toplevels and panels are completely different.
 *
 *   Drawers:
 *     + we ignore the old "parameters" setting.
 *     + s/unique-drawer-panel-id/attached_panel_id/
 *     + s/pixmap/custom_icon/
 *     + we should use the "usr_custom_icon" setting.
 *
 *   Menu buttons:
 *     + we ignore "main-menu", "global-main" and "main-menu-flags".
 *     + s/custom-icon/use_custom_icon/
 *     + s/custom-icon-file/custom_icon/
 *     + s/path/menu_path/
 *     + we now have use_menu_path. Need to figure out how this
 *       relates to the old main_menu and global_main flags.
 *
 *   Global config
 *     + need to figure out what to do about the old global config
 *       settings that now apply to individual panels
 */

#ifdef FIXME_FOR_NEW_TOPLEVEL
static void
panel_compatibility_warn (const char *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
			NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			message);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_widget_show (dialog);
}
#endif

GtkWidget *
panel_compatibility_load_menu_panel (const char *panel_id,
				     int         screen,
				     int         monitor)
{
#ifdef FIXME_FOR_NEW_TOPLEVEL
	PanelColor  color = { { 0, 0, 0, 0 }, 0xffff };
	GtkWidget  *retval;

	panel_compatibility_warn (_("This is your lucky day. I've just broken compatibility with GNOME 2.0 and 2.2 by converting your menu panel into an edge panel with two applets. If you log back into GNOME 2.0/2.2 you will find that instead of having a menu panel you will have an edge panel on the top of your screen without an Applications/Actions menu or a Window Menu. This will will be fixed before GNOME 2.4 gets anywhere near release."));

	/* A menu panel was like a x-small edge panel at the
	 * top of the screen.
	 */
	retval = edge_widget_new (panel_id,
				  screen,
				  monitor,
				  BORDER_TOP,
				  BASEP_EXPLICIT_HIDE,
				  BASEP_SHOWN,
				  PANEL_SIZE_X_SMALL,
				  FALSE,
				  FALSE,
				  PANEL_BACK_NONE,
				  NULL,
				  FALSE,
				  FALSE,
				  FALSE,
				  &color);

	g_object_set_data (G_OBJECT (BASEP_WIDGET (retval)->panel),
			   "load-compatibility-applets", GINT_TO_POINTER (1));

	return retval;
#else
	return NULL;
#endif /* FIXME_FOR_NEW_TOPLEVEL */
}

void
panel_compatibility_load_applets (void)
{
	GSList *l;

	for (l = panels; l; l = l->next) {
		PanelWidget *panel = l->data;

		if (!g_object_get_data (G_OBJECT (panel), "load-compatibility-applets"))
			continue;

		g_object_set_data (G_OBJECT (panel), "load-compatibility-applets", NULL);

		/* A menu panel contained a menu bar on the far left
	         * and a window menu on the far right.
		 */
		/* FIXME_FOR_NEW_CONFIG : panel_menu_bar_load (panel, 0, TRUE, NULL); */

		panel_applet_frame_create (panel->toplevel,
					   panel->size - 10,
					   "OAFIID:GNOME_WindowMenuApplet");
	}
}
