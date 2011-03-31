/*
 * panel-user-menu.h: user status menu
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

#ifndef __PANEL_USER_MENU_H__
#define __PANEL_USER_MENU_H__

#include <gtk/gtk.h>

#include "panel-menu-bar-object.h"
#include "panel-widget.h"

G_BEGIN_DECLS

#define PANEL_TYPE_USER_MENU         (panel_user_menu_get_type ())
#define PANEL_USER_MENU(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_USER_MENU, PanelUserMenu))
#define PANEL_USER_MENU_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_USER_MENU, PanelUserMenuClass))
#define PANEL_IS_USER_MENU(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_USER_MENU))
#define PANEL_IS_USER_MENU_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_USER_MENU))
#define PANEL_USER_MENU_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_USER_MENU, PanelUserMenuClass))

typedef struct _PanelUserMenu        PanelUserMenu;
typedef struct _PanelUserMenuClass   PanelUserMenuClass;
typedef struct _PanelUserMenuPrivate PanelUserMenuPrivate;

struct _PanelUserMenu{
	PanelMenuBarObject    usermenu;

	PanelUserMenuPrivate  *priv;
};

struct _PanelUserMenuClass {
	PanelMenuBarObjectClass usermenu_class;
};

GType      panel_user_menu_get_type  (void) G_GNUC_CONST;

void       panel_user_menu_create           (PanelToplevel       *toplevel,
					     PanelObjectPackType  pack_type,
					     int                  pack_index);

void       panel_user_menu_load             (PanelWidget  *panel,
					     const char   *id,
					     GSettings    *settings);

G_END_DECLS

#endif /* __PANEL_USER_MENU_H__ */
