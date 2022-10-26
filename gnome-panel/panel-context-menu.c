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

#include "panel-util.h"
#include "panel.h"
#include "gp-add-applet-window.h"
#include "gp-applet-manager.h"
#include "gp-properties-dialog.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-icon-names.h"

static void
panel_context_menu_create_new_panel (GtkWidget     *menuitem,
                                     PanelToplevel *toplevel)
{
  GpApplication *application;
  PanelLayout *layout;

  application = panel_toplevel_get_application (toplevel);
  layout = gp_application_get_layout (application);

  panel_layout_toplevel_create (layout, gtk_widget_get_screen (menuitem));
}

static gboolean
is_last_toplevel (PanelToplevel *toplevel)
{
  GpApplication *application;
  GList *toplevels;
  gboolean is_last;

  application = panel_toplevel_get_application (toplevel);
  toplevels = gp_application_get_toplevels (application);

  is_last = toplevels->next == NULL ? TRUE : FALSE;
  g_list_free (toplevels);

  return is_last;
}

static void
panel_context_menu_delete_panel (PanelToplevel *toplevel)
{
	if (is_last_toplevel (toplevel)) {
		panel_error_dialog (GTK_WINDOW (toplevel),
				    gtk_window_get_screen (GTK_WINDOW (toplevel)),
				    "cannot_delete_last_panel", TRUE,
				    _("Cannot delete this panel"),
				    _("You must always have at least one panel."));
		return;
	}

        panel_delete (toplevel);
}

static void
panel_context_menu_setup_delete_panel_item (GtkWidget     *menuitem,
                                            PanelToplevel *toplevel)
{
	GpApplication *application;
	PanelLockdown *lockdown;
	PanelLayout *layout;
	gboolean     sensitive;

	g_assert (PANEL_IS_TOPLEVEL (toplevel));

	application = panel_toplevel_get_application (toplevel);
	lockdown = gp_application_get_lockdown (application);
	layout = gp_application_get_layout (application);

	sensitive =
		!is_last_toplevel (toplevel) &&
		!panel_lockdown_get_panels_locked_down (lockdown) &&
		panel_layout_is_writable (layout);

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
      GpApplication *application;
      const gchar *toplevel_id;

      application = panel_toplevel_get_application (toplevel);
      toplevel_id = panel_toplevel_get_id (toplevel);

      dialog = gp_properties_dialog_new (application, toplevel_id);

      g_signal_connect (dialog, "destroy",
                        G_CALLBACK (dialog_destroy_cb),
                        toplevel);

      g_object_set_data_full (G_OBJECT (toplevel), "gp-properties-dialog",
                              dialog, (GDestroyNotify) gtk_widget_destroy);

      panel_toplevel_push_autohide_disabler (toplevel);
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

static GtkWidget *
add_menu_separator (GtkWidget *menu)
{
	GtkWidget *menuitem;

	menuitem = gtk_separator_menu_item_new ();
	gtk_widget_set_sensitive (menuitem, FALSE);
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	return menuitem;
}

static void
add_applet_dialog_destroy_cb (GtkWidget     *dialog,
                              PanelToplevel *toplevel)
{
  panel_toplevel_pop_autohide_disabler (toplevel);
  g_object_set_data (G_OBJECT (toplevel), "add-applet-dialog", NULL);
}

static void
add_to_panel_activate_cb (GtkMenuItem   *menuitem,
                          PanelToplevel *toplevel)
{
  GtkWidget *dialog;

  dialog = g_object_get_data (G_OBJECT (toplevel), "add-applet-dialog");

  if (dialog == NULL)
    {
      GpApplication *application;
      GpModuleManager *manager;

      application = panel_toplevel_get_application (toplevel);
      manager = gp_application_get_module_manager (application);

      dialog = gp_add_applet_window_new (manager, toplevel);

      g_signal_connect (dialog, "destroy",
                        G_CALLBACK (add_applet_dialog_destroy_cb),
                        toplevel);

      g_object_set_data_full (G_OBJECT (toplevel),
                              "add-applet-dialog",
                              dialog,
                              (GDestroyNotify) gtk_widget_destroy);

      panel_toplevel_push_autohide_disabler (toplevel);
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
panel_context_menu_build_edition (PanelWidget *panel_widget,
				  GtkWidget   *menu)
{
	GpApplication *application;
	PanelLayout *layout;
	GtkWidget *menuitem;

	application = panel_toplevel_get_application (panel_widget->toplevel);
	layout = gp_application_get_layout (application);

	menuitem = gtk_menu_item_new_with_mnemonic (_("_Add to Panel..."));
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (menuitem,
	                  "activate",
	                  G_CALLBACK (add_to_panel_activate_cb),
	                  panel_widget->toplevel);

	if (!panel_layout_is_writable (layout))
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
			  panel_widget->toplevel);
	gtk_widget_set_sensitive (menuitem, 
				  panel_layout_is_writable (layout));
}

static GtkWidget *
panel_create_menu (void)
{
	GtkWidget       *retval;
	GtkStyleContext *context;

	retval = gtk_menu_new ();
	gtk_widget_set_name (retval, "gnome-panel-main-menu");

	context = gtk_widget_get_style_context (retval);
	gtk_style_context_add_class (context, "gnome-panel-main-menu");

	return retval;
}

static gboolean
menu_dummy_button_press_event (GtkWidget      *menuitem,
			       GdkEventButton *event)
{
	if (event->button == 3)
		return TRUE;

	return FALSE;
}

static GtkWidget *
create_empty_menu (void)
{
	GtkWidget *retval;

	retval = panel_create_menu ();

	/* intercept all right button clicks makes sure they don't
	   go to the object itself */
	g_signal_connect (retval, "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	return retval;
}

GtkWidget *
panel_context_menu_create (PanelWidget *panel)
{
	GpApplication *application;
	PanelLockdown *lockdown;
	GtkWidget *retval;

	application = panel_toplevel_get_application (panel->toplevel);
	lockdown = gp_application_get_lockdown (application);

	if (panel_lockdown_get_panels_locked_down (lockdown))
		return NULL;

	retval = create_empty_menu ();

	gtk_widget_set_name (retval, "gnome-panel-context-menu");

	panel_context_menu_build_edition (panel, retval);

	return retval;
}
