/*
 * panel-user-menu.c: user status menu
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

#include "applet.h"
#include "panel-layout.h"
#include "panel-menu-bar-object.h"
#include "panel-menu-items.h"

#include "panel-user-menu.h"

G_DEFINE_TYPE (PanelUserMenu, panel_user_menu, PANEL_TYPE_MENU_BAR_OBJECT)

#define PANEL_USER_MENU_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_USER_MENU, PanelUserMenuPrivate))

struct _PanelUserMenuPrivate {
	AppletInfo  *info;
	PanelWidget *panel;

	GtkWidget   *desktop_item;
};

static void
panel_user_menu_init (PanelUserMenu *usermenu)
{
	usermenu->priv = PANEL_USER_MENU_GET_PRIVATE (usermenu);

	usermenu->priv->info = NULL;

	usermenu->priv->desktop_item = panel_desktop_menu_item_new (TRUE, TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (usermenu),
			       usermenu->priv->desktop_item);
	gtk_widget_show (usermenu->priv->desktop_item);
}

static void
panel_user_menu_parent_set (GtkWidget *widget,
			   GtkWidget *previous_parent)
{
	PanelUserMenu *usermenu = PANEL_USER_MENU (widget);
	GtkWidget    *parent;

	parent = gtk_widget_get_parent (widget);
	g_assert (!parent || PANEL_IS_WIDGET (parent));

	usermenu->priv->panel = (PanelWidget *) parent;

	if (usermenu->priv->desktop_item)
		panel_desktop_menu_item_set_panel (usermenu->priv->desktop_item,
						   usermenu->priv->panel);
}

static void
panel_user_menu_class_init (PanelUserMenuClass *klass)
{
	GtkWidgetClass *widget_class  = (GtkWidgetClass *) klass;

	widget_class->parent_set = panel_user_menu_parent_set;

	g_type_class_add_private (klass, sizeof (PanelUserMenuPrivate));
}

void
panel_user_menu_load (PanelWidget *panel,
		     const char  *id,
		     GSettings   *settings)
{
	PanelUserMenu *usermenu;

	g_return_if_fail (panel != NULL);

	usermenu = g_object_new (PANEL_TYPE_USER_MENU, NULL);

	usermenu->priv->info = panel_applet_register (
					GTK_WIDGET (usermenu), panel,
					PANEL_OBJECT_USER_MENU, id,
					settings,
					NULL, NULL);
	if (!usermenu->priv->info) {
		gtk_widget_destroy (GTK_WIDGET (usermenu));
		return;
	}

	panel_menu_bar_object_object_load_finish (PANEL_MENU_BAR_OBJECT (usermenu),
						  panel);
}

void
panel_user_menu_create (PanelToplevel       *toplevel,
		       PanelObjectPackType  pack_type,
		       int                  pack_index)
{
	panel_layout_object_create (PANEL_OBJECT_USER_MENU, NULL,
				    panel_toplevel_get_id (toplevel),
				    pack_type, pack_index);
}
