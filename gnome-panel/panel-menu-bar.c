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
#include <glib/gi18n.h>

#include "panel-util.h"
#include "panel-action-button.h"
#include "panel-stock-icons.h"
#include "panel-recent.h"
#include "applet.h"
#include "menu.h"
#include "menu-util.h"
#include "panel-globals.h"
#include "panel-profile.h"
#include "panel-lockdown.h"

#define PANEL_MENU_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_MENU_BAR, PanelMenuBarPrivate))

#define MENU_FLAGS (MAIN_MENU_SYSTEM)

struct _PanelMenuBarPrivate {
	AppletInfo            *info;
	PanelWidget           *panel;

	GtkWidget             *applications_menu;
	GtkWidget             *actions_menu;
	GtkWidget             *actions_item;
};

static GObjectClass *parent_class;
static void panel_menu_bar_append_actions_menu (PanelMenuBar *menubar);

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

	create_root_menu (menu, menubar->priv->panel, TRUE, MENU_FLAGS, FALSE);
}

static void
panel_menu_bar_append_action_item (PanelMenuBar          *menubar,
				   PanelActionButtonType  action_type,
				   GCallback              callback)
{
	GtkWidget *item;

	item = gtk_image_menu_item_new ();
	setup_stock_menu_item (item,
			       panel_menu_icon_get_size (),
			       panel_action_get_stock_icon (action_type),
			       panel_action_get_text (action_type),
			       TRUE);

	gtk_tooltips_set_tip (panel_tooltips,
			      item,
			      panel_action_get_tooltip (action_type),
			      NULL);

	gtk_menu_shell_append (GTK_MENU_SHELL (menubar->priv->actions_menu), item);
	g_signal_connect (item, "activate", callback, NULL);
	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);
	setup_internal_applet_drag (item,
				    panel_action_get_drag_id (action_type));
}

static void
panel_menu_bar_recreate_actions_menu (PanelMenuBar *menubar)
{
	if (menubar->priv->actions_menu) {
		gtk_widget_destroy (menubar->priv->actions_menu);
		menubar->priv->actions_menu = NULL;

		panel_menu_bar_append_actions_menu (menubar);
	}
}

static void
panel_menu_bar_append_actions_menu (PanelMenuBar *menubar)
{
	GtkWidget *item;
	gboolean   enable_log_out;
	gboolean   enable_lock_screen;

	if (!menubar->priv->actions_menu) {
		menubar->priv->actions_menu = panel_create_menu ();

		/* intercept all right button clicks makes sure they don't
	   	go to the object itself */
		g_signal_connect (G_OBJECT (menubar->priv->actions_menu), "button_press_event",
			  	G_CALLBACK (menu_dummy_button_press_event), NULL);

		g_signal_connect (menubar->priv->actions_menu, "destroy",
			  	G_CALLBACK (gtk_widget_destroyed),
			  	&menubar->priv->actions_menu);
	}

	if (!panel_lockdown_get_disable_command_line ()) {
		panel_menu_bar_append_action_item (menubar,
						   PANEL_ACTION_RUN,
						   G_CALLBACK (panel_action_run_program));

		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menubar->priv->actions_menu), item);
		gtk_widget_show (item);
	}

	if (panel_is_program_in_path  ("gnome-search-tool"))
		panel_menu_bar_append_action_item (menubar,
						   PANEL_ACTION_SEARCH,
						   G_CALLBACK (panel_action_search));

	panel_recent_append_documents_menu (menubar->priv->actions_menu);

	if (panel_is_program_in_path ("gnome-screenshot")) {
		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menubar->priv->actions_menu), item);
		gtk_widget_show (item);

		panel_menu_bar_append_action_item (menubar,
						   PANEL_ACTION_SCREENSHOT,
						   G_CALLBACK (panel_action_screenshot));
	}

	enable_log_out = !panel_lockdown_get_disable_log_out ();
	enable_lock_screen =
		(!panel_lockdown_get_disable_lock_screen () &&
		 panel_is_program_in_path  ("xscreensaver"));
		

	if (enable_log_out || enable_lock_screen) {
		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menubar->priv->actions_menu), item);
		gtk_widget_show (item);
	}

	if (enable_lock_screen)
		panel_menu_bar_append_action_item (menubar,
						   PANEL_ACTION_LOCK,
						   G_CALLBACK (panel_action_lock_screen));

	if (enable_log_out) {
		panel_menu_bar_append_action_item (menubar,
						   PANEL_ACTION_LOGOUT,
						   G_CALLBACK (panel_action_logout));
	}

	if (!menubar->priv->actions_item) {
		menubar->priv->actions_item = gtk_menu_item_new_with_label (_("Actions"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menubar->priv->actions_item);
	}
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar->priv->actions_item),
				   menubar->priv->actions_menu);
}

static void
panel_menu_bar_instance_init (PanelMenuBar      *menubar,
			      PanelMenuBarClass *klass)
{
	GtkWidget *item;
	GtkWidget *image;

	menubar->priv = PANEL_MENU_BAR_GET_PRIVATE (menubar);

	menubar->priv->info = NULL;

	item = gtk_image_menu_item_new_with_label (_("Applications"));
        image = gtk_image_new_from_stock (
			PANEL_STOCK_GNOME_LOGO,
			panel_menu_bar_icon_get_size ());
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

	menubar->priv->applications_menu =
		create_root_menu (NULL, NULL, TRUE, MENU_FLAGS, FALSE);

	/* intercept all right button clicks makes sure they don't
	   go to the object itself */
	g_signal_connect (G_OBJECT (menubar->priv->applications_menu), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

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
	menubar->priv->actions_menu = NULL;
	menubar->priv->actions_item = NULL;
	panel_menu_bar_append_actions_menu (menubar);

	panel_lockdown_notify_add (G_CALLBACK (panel_menu_bar_recreate_actions_menu),
				   menubar);
}

static void
panel_menu_bar_parent_set (GtkWidget *widget,
			   GtkWidget *previous_parent)
{
	PanelMenuBar *menubar = PANEL_MENU_BAR (widget);

	g_assert (!widget->parent || PANEL_IS_WIDGET (widget->parent));

	menubar->priv->panel = (PanelWidget *) widget->parent;

	panel_applet_menu_set_recurse (GTK_MENU (menubar->priv->applications_menu),
				       "menu_panel",
				       menubar->priv->panel);
	panel_applet_menu_set_recurse (GTK_MENU (menubar->priv->actions_menu),
				       "menu_panel",
				       menubar->priv->panel);
}

static void
panel_menu_bar_class_init (PanelMenuBarClass *klass)
{
	GtkWidgetClass *widget_class  = (GtkWidgetClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	widget_class->parent_set = panel_menu_bar_parent_set;

	g_type_class_add_private (klass, sizeof (PanelMenuBarPrivate));

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

	panel_applet_add_callback (menubar->priv->info,
				   "help",
				   GTK_STOCK_HELP,
				   _("_Help"),
				   NULL);

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
	panel_profile_add_to_list (PANEL_GCONF_OBJECTS, id);
	g_free (id);
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

	panel_show_help (screen, "user-guide.xml", "gospanel-37");
}

void
panel_menu_bar_popup_menu (PanelMenuBar *menubar,
			   guint32       activate_time)
{
	GtkMenu *menu;
	GtkMenuShell *menu_shell;
	
	g_return_if_fail (PANEL_IS_MENU_BAR (menubar));

	menu = GTK_MENU (menubar->priv->applications_menu);

	/* 
	 * We need to call _gtk_menu_shell_activate() here as is done in 
	 * window_key_press_handler in gtkmenubar.c which pops up menu
	 * when F10 is pressed.
         *
	 * As that function is private its code is replicated here.
	 */
	menu_shell = GTK_MENU_SHELL (menubar);
	if (!menu_shell->active) {
		gtk_grab_add (GTK_WIDGET (menu_shell));
		menu_shell->have_grab = TRUE;
		menu_shell->active = TRUE;
	}
	gtk_menu_shell_select_item (menu_shell,
				    gtk_menu_get_attach_widget (menu));
}
