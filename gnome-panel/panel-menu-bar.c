/*
 * panel-menu-bar.c: panel Applications/Actions menu bar
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
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-menu-bar.h"

#include <string.h>
#include <libgnome/gnome-i18n.h>

#include "panel-util.h"
#include "panel-action-button.h"
#include "panel-stock-icons.h"
#include "panel-recent.h"
#include "applet.h"
#include "menu.h"
#include "menu-util.h"
#include "panel-globals.h"
#include "panel-profile.h"

#define MENU_FLAGS (MAIN_MENU_SYSTEM | MAIN_MENU_DISTRIBUTION_SUB)

enum {
	PROP_0,
};

struct _PanelMenuBarPrivate {
	AppletInfo            *info;
	PanelWidget           *panel;

	GtkWidget             *applications_menu;
	GtkWidget             *actions_menu;
};

static GObjectClass *parent_class;

static void
panel_menu_bar_show_applications_menu (PanelMenuBar *menubar,
				       GtkWidget    *menu)
{
	gtk_menu_set_screen (GTK_MENU (menu),
			     gtk_widget_get_screen (GTK_WIDGET (menubar)));

	if (!menu_need_reread (menu))
		return;

	while (GTK_MENU_SHELL (menu)->children)
		gtk_widget_destroy (GTK_MENU_SHELL (menu)->children->data);

	create_root_menu (menu, NULL, TRUE, MENU_FLAGS, FALSE);
}

static void
panel_menu_bar_append_action_item (PanelMenuBar *menubar,
				   GtkWidget    *menu,
				   const char   *title,
				   const char   *stock_id,
				   const char   *tooltip,
				   const char   *drag_id,
				   GCallback     callback)
{
	GtkWidget *item;

	item = stock_menu_item_new (title, stock_id, FALSE);

	gtk_tooltips_set_tip (panel_tooltips, item, tooltip, NULL);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (item, "activate", callback, NULL);
	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);
	setup_internal_applet_drag (item, drag_id);
}

static void
panel_menu_bar_append_actions_menu (PanelMenuBar *menubar)
{
	GtkWidget *item;

	menubar->priv->actions_menu = panel_create_menu ();

	g_signal_connect (menubar->priv->actions_menu, "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &menubar->priv->actions_menu);

	panel_menu_bar_append_action_item (
			menubar,
			menubar->priv->actions_menu,
			_("_Run Application..."),
			PANEL_STOCK_RUN,
			_("Run an Application by entering a command"),
			"ACTION:run:NEW",
			G_CALLBACK (panel_action_run_program));

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar->priv->actions_menu), item);

	if (panel_is_program_in_path  ("gnome-search-tool"))
		panel_menu_bar_append_action_item (
			menubar,
			menubar->priv->actions_menu,
			_("_Search for Files..."),
			PANEL_STOCK_SEARCHTOOL,
			_("Find files, folders, and documents on your computer"),
			"ACTION:search:NEW",
			G_CALLBACK (panel_action_search));

	panel_recent_append_documents_menu (menubar->priv->actions_menu);

	if (panel_is_program_in_path ("gnome-panel-screenshot")) {
		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menubar->priv->actions_menu), item);

		panel_menu_bar_append_action_item (
			menubar,
			menubar->priv->actions_menu,
			_("_Take Screenshot..."),
			PANEL_STOCK_SCREENSHOT,
			_("Take a screenshot of your desktop"),
			"ACTION:screenshot:NEW",
			G_CALLBACK (panel_action_screenshot));
	}

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar->priv->actions_menu), item);

	if (panel_is_program_in_path  ("xscreensaver"))
		panel_menu_bar_append_action_item (
			menubar,
			menubar->priv->actions_menu,
			_("_Lock Screen"), 
			PANEL_STOCK_LOCKSCREEN, 
			_("Protect your computer from unauthorized use"),
			"ACTION:lock:NEW",
			G_CALLBACK (panel_action_lock_screen));

	panel_menu_bar_append_action_item (
			menubar,
			menubar->priv->actions_menu,
			_("_Log Out"),
			PANEL_STOCK_LOGOUT,
			_("Quit from the GNOME desktop"),
			"ACTION:logout:NEW",
			G_CALLBACK (panel_action_logout));

	item = gtk_menu_item_new_with_label (_("Actions"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menubar->priv->actions_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);
}

static void
panel_menu_bar_instance_init (PanelMenuBar      *menubar,
			      PanelMenuBarClass *klass)
{
	GtkWidget *item;
	GtkWidget *image;

	menubar->priv = g_new0 (PanelMenuBarPrivate, 1);

	menubar->priv->info = NULL;

	item = gtk_image_menu_item_new_with_label (_("Applications"));
        image = gtk_image_new_from_stock (
			PANEL_STOCK_GNOME_LOGO,
			panel_menu_bar_icon_get_size ()),
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

	menubar->priv->applications_menu =
		create_root_menu (NULL, NULL, TRUE, MENU_FLAGS, FALSE);

	g_signal_connect (menubar->priv->applications_menu, "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &menubar->priv->applications_menu);

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item),
				   menubar->priv->applications_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);

	g_signal_connect_swapped (menubar->priv->applications_menu, "show",
				  G_CALLBACK (panel_menu_bar_show_applications_menu), menubar);
	g_signal_connect_swapped (menubar->priv->applications_menu, "hide",
				  G_CALLBACK (gtk_menu_shell_deselect), menubar);
	
	panel_menu_bar_append_actions_menu (menubar);
}

static void
panel_menu_bar_finalize (GObject *object)
{
	PanelMenuBar *menubar = PANEL_MENU_BAR (object);

	menubar->priv->info = NULL;

	g_free (menubar->priv);
	menubar->priv = NULL;

	parent_class->finalize (object);
}

static void
panel_menu_bar_parent_set (GtkWidget *widget,
			   GtkWidget *previous_parent)
{
	PanelMenuBar *menubar = PANEL_MENU_BAR (widget);

	if (widget->parent && !PANEL_IS_WIDGET (widget->parent)) {
		g_warning ("Hold it! Wait one second! This isn't a PanelWidget!");
		return;
	}

	menubar->priv->panel = (PanelWidget *) widget->parent;

	g_object_set_data (G_OBJECT (menubar->priv->applications_menu),
			   "menu_panel", menubar->priv->panel);
	g_object_set_data (G_OBJECT (menubar->priv->actions_menu),
			   "menu_panel", menubar->priv->panel);
}

static void
panel_menu_bar_class_init (PanelMenuBarClass *klass,
			   gpointer           dummy)
{
	GObjectClass   *gobject_class = (GObjectClass   *) klass;
	GtkWidgetClass *widget_class  = (GtkWidgetClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = panel_menu_bar_finalize;

	widget_class->parent_set = panel_menu_bar_parent_set;

	gtk_rc_parse_string (
		"style \"panel-menubar-style\"\n"
		"{\n"
		"  GtkMenuBar::shadow-type = none\n"
		"  GtkMenuBar::internal-padding = 0\n"
		"}\n"
		"class \"PanelMenuBar\" style \"panel-menubar-style\"");
}

GType
panel_menu_bar_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelMenuBarClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_menu_bar_class_init,
			NULL,
			NULL,
			sizeof (PanelMenuBar),
			0,
			(GInstanceInitFunc) panel_menu_bar_instance_init,
			NULL
		};

		type = g_type_register_static (
				GTK_TYPE_MENU_BAR, "PanelMenuBar", &info, 0);
	}

	return type;
}

static void
panel_menu_bar_load (PanelWidget *panel,
		     gboolean     locked,
		     int          position,
		     gboolean     exactpos,
		     const char  *id)
{
	PanelMenuBar *menubar;

	g_return_if_fail (panel != NULL);

	menubar = g_object_new (PANEL_TYPE_MENU_BAR, NULL);

	menubar->priv->info = panel_applet_register (
					GTK_WIDGET (menubar), NULL, NULL,
					panel, locked, position, exactpos,
					PANEL_OBJECT_MENU_BAR, id);
	if (!menubar->priv->info) {
		gtk_widget_destroy (GTK_WIDGET (menubar));
		return;
	}

	panel_applet_add_callback (
		menubar->priv->info, "help", GTK_STOCK_HELP, _("_Help"));

	panel_widget_set_applet_expandable (panel, GTK_WIDGET (menubar), FALSE, TRUE);
}

void
panel_menu_bar_load_from_gconf (PanelWidget *panel,
				gboolean     locked,
				int          position,
				gboolean     exactpos,
				const char  *id)
{
	panel_menu_bar_load (panel, locked, position, exactpos, id);
}

void
panel_menu_bar_create (PanelToplevel *toplevel,
		       int            position)
{
	char *id;

	id = panel_profile_prepare_object (PANEL_OBJECT_MENU_BAR, toplevel, position, FALSE);

	/* frees id */
	panel_profile_add_to_list (PANEL_GCONF_OBJECTS, id);
}

void
panel_menu_bar_invoke_menu (PanelMenuBar *menubar,
			    const char   *callback_name)
{
	GdkScreen *screen;

	g_return_if_fail (PANEL_IS_MENU_BAR (menubar));
	g_return_if_fail (callback_name != NULL);

	if (strcmp (callback_name, "help"))
		return;

	screen = gtk_widget_get_screen (GTK_WIDGET (menubar));

	panel_show_help (screen, "wgospanel.xml", NULL);
}

void
panel_menu_bar_popup_menu (PanelMenuBar *menubar,
			   guint32       activate_time)
{
	GtkMenu *menu;
	
	g_return_if_fail (PANEL_IS_MENU_BAR (menubar));

	menu = GTK_MENU (menubar->priv->applications_menu);

	gtk_menu_shell_select_item (GTK_MENU_SHELL (menubar),
				    gtk_menu_get_attach_widget (menu));
}
