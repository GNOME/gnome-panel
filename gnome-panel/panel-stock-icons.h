/*
 * panel-stock-icons.h: panel stock icons registration
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
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

#ifndef __PANEL_STOCK_ICONS_H__
#define __PANEL_STOCK_ICONS_H__

#include <glib/gmacros.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

/* themeable size - "panel-menu" */
#define PANEL_DEFAULT_MENU_ICON_SIZE 	24
#define PANEL_DEFAULT_BUTTON_ICON_SIZE  12

/* stock icons */
#define PANEL_STOCK_RUN                 "panel-run"
#define PANEL_STOCK_SEARCHTOOL          "panel-searchtool"
#define PANEL_STOCK_SCREENSHOT          "panel-screenshot"
#define PANEL_STOCK_LOCKSCREEN          "panel-lockscreen"
#define PANEL_STOCK_LOGOUT              "panel-logout"
#define PANEL_STOCK_FORCE_QUIT          "panel-force-quit"
#define PANEL_STOCK_GNOME_LOGO          "panel-gnome-logo"
#define PANEL_STOCK_APPLICATIONS        "panel-applications"
#define PANEL_STOCK_MAIN_MENU           "panel-main-menu"
#define PANEL_STOCK_DEBIAN              "panel-debian"
#define PANEL_STOCK_SUSE                "panel-suse"
#define PANEL_STOCK_CDE                 "panel-cde"
#define PANEL_STOCK_ACCESSORIES         "panel-accessories"
#define PANEL_STOCK_AMUSEMENTS          "panel-amusements"
#define PANEL_STOCK_MULTIMEDIA          "panel-multimedia"
#define PANEL_STOCK_INTERNET            "panel-internet"
#define PANEL_STOCK_UTILITY             "panel-utility"
#define PANEL_STOCK_CORNER_PANEL        "panel-corner-type"
#define PANEL_STOCK_EDGE_PANEL          "panel-edge-type"
#define PANEL_STOCK_FLOATING_PANEL      "panel-floating-type"
#define PANEL_STOCK_SLIDING_PANEL       "panel-sliding-type"
#define PANEL_STOCK_MENU_PANEL          "panel-menu-type"
#define PANEL_STOCK_LAUNCHER            "panel-launcher"
#define PANEL_STOCK_ACTION		"panel-action"
#define PANEL_STOCK_DRAWER              "panel-drawer"
#define PANEL_STOCK_APPLETS             "panel-applet"
#define PANEL_STOCK_DESKTOP             "panel-desktop"

/* stock items  - no point in theme the icons one these,
 * they use stock gtk icons and just modify the text
 * for the stock item.
 */
#define PANEL_STOCK_EXECUTE             "panel-execute"
#define PANEL_STOCK_CLEAR               "panel-clear"

void        panel_init_stock_icons_and_items (void);
GtkIconSize panel_menu_icon_get_size         (void);
GtkIconSize panel_menu_bar_icon_get_size     (void);
GtkIconSize panel_button_icon_get_size       (void);

G_END_DECLS

#endif /* __PANEL_STOCK_ICONS_H__ */
