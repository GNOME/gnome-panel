/*
 * panel-context-menu.c: context menu for the panels
 *
 * Copyright (C) 2004 Vincent Untz
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Most of the original code come from menu.c
 *
 * Authors:
 *	Vincent Untz <vincent@vuntz.net>
 *
 */

#include <config.h>

#include "panel-context-menu.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <libpanel-util/panel-error.h>
#include <libpanel-util/panel-show.h>

#include "free-the-fish.h"
#include "panel-util.h"
#include "panel.h"
#include "menu.h"
#include "gp-properties-dialog.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-addto-dialog.h"
#include "panel-icon-names.h"

static void
panel_context_menu_create_new_panel (GtkWidget *menuitem)
{
	panel_layout_toplevel_create (gtk_widget_get_screen (menuitem));
}

static void
panel_context_menu_delete_panel (PanelToplevel *toplevel)
{
	if (panel_toplevel_is_last (toplevel)) {
		panel_error_dialog (GTK_WINDOW (toplevel),
				    gtk_window_get_screen (GTK_WINDOW (toplevel)),
				    "cannot_delete_last_panel", TRUE,
				    _("Cannot delete this panel"),
				    _("You must always have at least one panel."));
		return;
	}

        panel_delete (toplevel);
}

static gboolean
panel_context_menu_check_for_screen (GtkWidget *w,
				     GdkEvent *ev,
				     gpointer data)
{
	static int times = 0;
	if (!w) {
		times = 0;
		return FALSE;
	}
	if (ev->type != GDK_KEY_PRESS)
		return FALSE;
	if (ev->key.keyval == GDK_KEY_f ||
	    ev->key.keyval == GDK_KEY_F) {
		times++;
		if (times == 3) {
			times = 0;
			free_the_fish ();
		}
	}
	return FALSE;
}

static void
panel_context_menu_setup_delete_panel_item (GtkWidget     *menuitem,
                                            PanelToplevel *toplevel)
{
	gboolean     sensitive;

	panel_context_menu_check_for_screen (NULL, NULL, NULL);
	g_assert (PANEL_IS_TOPLEVEL (toplevel));

	sensitive =
		!panel_toplevel_is_last (toplevel) &&
		!panel_lockdown_get_panels_locked_down_s () &&
		panel_layout_is_writable ();

	gtk_widget_set_sensitive (menuitem, sensitive);
}

static void
dialog_destroy_cb (GtkWidget     *dialog,
                   PanelToplevel *toplevel)
{
  panel_toplevel_pop_autohide_disabler (toplevel);
  g_object_set_data (G_OBJECT (toplevel), "gp-properties-dialog", NULL);
}

static void
present_properties_dialog (GtkWidget     *widget,
                           PanelToplevel *toplevel)
{
  GtkWidget *dialog;

  dialog = g_object_get_data (G_OBJECT (toplevel), "gp-properties-dialog");

  if (dialog == NULL)
    {
      const gchar *toplevel_id;

      toplevel_id = panel_toplevel_get_id (toplevel);
      dialog = gp_properties_dialog_new (toplevel_id);

      g_signal_connect (dialog, "destroy",
                        G_CALLBACK (dialog_destroy_cb),
                        toplevel);

      g_object_set_data_full (G_OBJECT (toplevel), "gp-properties-dialog",
                              dialog, (GDestroyNotify) gtk_widget_destroy);

      panel_toplevel_push_autohide_disabler (toplevel);
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
panel_context_menu_build_edition (PanelWidget *panel_widget,
				  GtkWidget   *menu)
{
	GtkWidget *menuitem;

	menuitem = gtk_menu_item_new_with_mnemonic (_("_Add to Panel..."));
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
        g_signal_connect (G_OBJECT (menuitem), "activate",
	      	       	  G_CALLBACK (panel_addto_present), panel_widget);

	if (!panel_layout_is_writable ())
		gtk_widget_set_sensitive (menuitem, FALSE);

	menuitem = gtk_menu_item_new_with_mnemonic (_("_Properties"));
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (menuitem, "activate",
	                  G_CALLBACK (present_properties_dialog),
	                  panel_widget->toplevel);

	menuitem = gtk_menu_item_new_with_mnemonic (_("_Delete This Panel"));
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect_swapped (G_OBJECT (menuitem), "activate",
				  G_CALLBACK (panel_context_menu_delete_panel),
				  panel_widget->toplevel);
	g_signal_connect (G_OBJECT (menuitem), "show",
			  G_CALLBACK (panel_context_menu_setup_delete_panel_item),
			  panel_widget->toplevel);

	add_menu_separator (menu);

	menuitem = gtk_menu_item_new_with_mnemonic (_("_New Panel"));
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (panel_context_menu_create_new_panel), 
			  NULL);
	gtk_widget_set_sensitive (menuitem, 
				  panel_layout_is_writable ());
}

GtkWidget *
panel_context_menu_create (PanelWidget *panel)
{
	GtkWidget *retval;

	if (panel_lockdown_get_panels_locked_down_s ())
		return NULL;

	retval = create_empty_menu ();

	gtk_widget_set_name (retval, "gnome-panel-context-menu");

	panel_context_menu_build_edition (panel, retval);

	g_signal_connect (retval, "event",
			  G_CALLBACK (panel_context_menu_check_for_screen),
			  NULL);

	return retval;
}
