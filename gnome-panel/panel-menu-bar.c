/*
 * panel-menu-bar.c: panel Applications/Places/Desktop menu bar
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 *	Vincent Untz <vincent@vuntz.net>
 */

#include <config.h>

#include "panel-menu-bar.h"

#include <string.h>
#include <glib/gi18n.h>

#include <libpanel-util/panel-error.h>
#include <libpanel-util/panel-launch.h>
#include <libpanel-util/panel-show.h>

#include "applet.h"
#include "menu.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-icon-names.h"
#include "panel-menu-bar-object.h"
#include "panel-menu-items.h"
#include "panel-util.h"

G_DEFINE_TYPE (PanelMenuBar, panel_menu_bar, PANEL_TYPE_MENU_BAR_OBJECT)

#define PANEL_MENU_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_MENU_BAR, PanelMenuBarPrivate))

struct _PanelMenuBarPrivate {
	AppletInfo  *info;
	PanelWidget *panel;

	GtkWidget   *image;
	GtkWidget   *applications_menu;
	GtkWidget   *applications_item;
	GtkWidget   *places_item;
};

/* themeable size - "panel-foobar" -- This is only used for the icon of the
 * Applications item in the menu bar */
#define PANEL_DEFAULT_MENU_BAR_ICON_SIZE       24

static GtkIconSize panel_menu_bar_icon_size = 0;

static GtkIconSize
panel_menu_bar_icon_get_size (void)
{
	return panel_menu_bar_icon_size;
}

static gboolean
panel_menu_bar_reinit_tooltip (GtkWidget    *widget,
			       PanelMenuBar *menubar)
{
	g_object_set (menubar->priv->applications_item,
		      "has-tooltip", TRUE, NULL);
	g_object_set (menubar->priv->places_item,
		      "has-tooltip", TRUE, NULL);

	return FALSE;
}

static gboolean
panel_menu_bar_hide_tooltip_and_focus (GtkWidget    *widget,
				       PanelMenuBar *menubar)
{
	/* remove focus that would be drawn on the currently focused child of
	 * the toplevel. See bug#308632. */
	gtk_window_set_focus (GTK_WINDOW (menubar->priv->panel->toplevel),
			      NULL);

	g_object_set (widget, "has-tooltip", FALSE, NULL);

	return FALSE;
}

static void
panel_menu_bar_setup_tooltip (PanelMenuBar *menubar)
{
	panel_util_set_tooltip_text (menubar->priv->applications_item,
				     _("Browse and run installed applications"));
	panel_util_set_tooltip_text (menubar->priv->places_item,
				     _("Access documents, folders and network places"));

	//FIXME: this doesn't handle the right-click case. Sigh.
	/* Hide tooltip if a menu is activated */
	g_signal_connect (menubar->priv->applications_item,
			  "activate",
			  G_CALLBACK (panel_menu_bar_hide_tooltip_and_focus),
			  menubar);
	g_signal_connect (menubar->priv->places_item,
			  "activate",
			  G_CALLBACK (panel_menu_bar_hide_tooltip_and_focus),
			  menubar);

	/* Reset tooltip when the menu bar is not used */
	g_signal_connect (GTK_MENU_SHELL (menubar),
			  "deactivate",
			  G_CALLBACK (panel_menu_bar_reinit_tooltip),
			  menubar);
}

static void
panel_menu_bar_init (PanelMenuBar *menubar)
{
	menubar->priv = PANEL_MENU_BAR_GET_PRIVATE (menubar);

	menubar->priv->info = NULL;

	menubar->priv->applications_menu = create_applications_menu ("applications.menu", NULL, TRUE);

	menubar->priv->applications_item = panel_image_menu_item_new ();
	gtk_menu_item_set_label (GTK_MENU_ITEM (menubar->priv->applications_item),
				 _("Applications"));
	menubar->priv->image = gtk_image_new_from_icon_name (PANEL_ICON_MAIN_MENU,
							     panel_menu_bar_icon_get_size ());

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar->priv->applications_item),
				   menubar->priv->applications_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar),
			       menubar->priv->applications_item);
	gtk_widget_show (menubar->priv->applications_item);

	menubar->priv->places_item = panel_place_menu_item_new (FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar),
			       menubar->priv->places_item);
	gtk_widget_show (menubar->priv->places_item);

	panel_menu_bar_setup_tooltip (menubar);
}

static void
panel_menu_bar_parent_set (GtkWidget *widget,
			   GtkWidget *previous_parent)
{
	PanelMenuBar *menubar = PANEL_MENU_BAR (widget);
	GtkWidget    *parent;

	parent = gtk_widget_get_parent (widget);
	g_assert (!parent || PANEL_IS_WIDGET (parent));

	menubar->priv->panel = (PanelWidget *) parent;

	if (menubar->priv->applications_menu)
		panel_applet_menu_set_recurse (GTK_MENU (menubar->priv->applications_menu),
					       "menu_panel",
					       menubar->priv->panel);
	if (menubar->priv->places_item)
		panel_place_menu_item_set_panel (menubar->priv->places_item,
						 menubar->priv->panel);
}

static void
panel_menu_bar_style_updated (GtkWidget *widget)
{
	PanelMenuBar *menubar = PANEL_MENU_BAR (widget);
	gboolean      visible;

	GTK_WIDGET_CLASS (panel_menu_bar_parent_class)->style_updated (widget);

	gtk_widget_style_get (widget, "icon-visible", &visible, NULL);

	if (visible)
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menubar->priv->applications_item),
					       menubar->priv->image);
	else
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menubar->priv->applications_item),
					       NULL);
}

static void
panel_menu_bar_class_init (PanelMenuBarClass *klass)
{
	GtkWidgetClass *widget_class  = (GtkWidgetClass *) klass;

	widget_class->parent_set = panel_menu_bar_parent_set;
	widget_class->style_updated = panel_menu_bar_style_updated;

	g_type_class_add_private (klass, sizeof (PanelMenuBarPrivate));

	gtk_widget_class_install_style_property (
		widget_class,
		g_param_spec_boolean ("icon-visible",
				      "Icon visible",
				      "Whether the menubar icon is visible",
				      FALSE,
				      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	if (panel_menu_bar_icon_size == 0)
		panel_menu_bar_icon_size = gtk_icon_size_register ("panel-foobar",
								   PANEL_DEFAULT_MENU_BAR_ICON_SIZE,
								   PANEL_DEFAULT_MENU_BAR_ICON_SIZE);
}

void
panel_menu_bar_load (PanelWidget *panel,
		     const char  *id,
		     GSettings   *settings)
{
	PanelMenuBar *menubar;

	g_return_if_fail (panel != NULL);

	menubar = g_object_new (PANEL_TYPE_MENU_BAR, NULL);

	menubar->priv->info = panel_applet_register (
					GTK_WIDGET (menubar), panel,
					PANEL_OBJECT_MENU_BAR, id,
					settings,
					NULL, NULL);
	if (!menubar->priv->info) {
		gtk_widget_destroy (GTK_WIDGET (menubar));
		return;
	}

	if (panel_is_program_in_path ("alacarte") ||
	    panel_is_program_in_path ("gmenu-simple-editor")) {
		panel_applet_add_callback (menubar->priv->info,
					   "edit",
					   NULL,
					   _("_Edit Menus"),
					   panel_lockdown_get_not_panels_locked_down_s);
	}

	panel_applet_add_callback (menubar->priv->info,
				   "help",
				   GTK_STOCK_HELP,
				   _("_Help"),
				   NULL);

	panel_menu_bar_object_object_load_finish (PANEL_MENU_BAR_OBJECT (menubar),
						  panel);
}

void
panel_menu_bar_create (PanelToplevel       *toplevel,
		       PanelObjectPackType  pack_type,
		       int                  pack_index)
{
	panel_layout_object_create (PANEL_OBJECT_MENU_BAR, NULL,
				    panel_toplevel_get_id (toplevel),
				    pack_type, pack_index);
}

void
panel_menu_bar_invoke_menu (PanelMenuBar *menubar,
			    const char   *callback_name)
{
	GdkScreen *screen;

	g_return_if_fail (PANEL_IS_MENU_BAR (menubar));
	g_return_if_fail (callback_name != NULL);

	screen = gtk_widget_get_screen (GTK_WIDGET (menubar));

	if (!strcmp (callback_name, "help")) {
		panel_show_help (screen, "user-guide", "menubar", NULL);

	} else if (!strcmp (callback_name, "edit")) {
		GError *error = NULL;

		panel_launch_desktop_file_with_fallback ("alacarte.desktop",
							 "alacarte",
							 screen, &error);
		if (error) {
			g_error_free (error);
			panel_launch_desktop_file_with_fallback (
						"gmenu-simple-editor.desktop",
						"gmenu-simple-editor",
						screen, NULL);
		}
	}
}

void
panel_menu_bar_popup_menu (PanelMenuBar *menubar,
			   guint32       activate_time)
{
	g_return_if_fail (PANEL_IS_MENU_BAR (menubar));

	gtk_menu_shell_select_item (GTK_MENU_SHELL (menubar),
				    menubar->priv->applications_item);
}
