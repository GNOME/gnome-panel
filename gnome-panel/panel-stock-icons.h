/*
 * panel-stock-icons.h
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
#define PANEL_DEFAULT_MENU_ICON_SIZE 	20 
#define PANEL_DEFAULT_BUTTON_ICON_SIZE  12

/* stock icons */
#define PANEL_STOCK_RUN                 "panel-run"
#define PANEL_STOCK_SEARCHTOOL          "panel-searchtool"
#define PANEL_STOCK_SCREENSHOT          "panel-screenshot"
#define PANEL_STOCK_LOCKSCREEN          "panel-lockscreen"
#define PANEL_STOCK_LOGOUT              "panel-logout"
#define PANEL_STOCK_GNOME_LOGO          "panel-gnome-logo"
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
#define PANEL_STOCK_DRAWER              "panel-drawer"
#define PANEL_STOCK_APPLETS             "panel-applet"
#define PANEL_STOCK_DESKTOP             "panel-desktop"
#define PANEL_STOCK_KDE                 "panel-kde"
#define PANEL_STOCK_ARROW_RIGHT         "panel-arrow-right"
#define PANEL_STOCK_ARROW_LEFT          "panel-arrow-left"
#define PANEL_STOCK_ARROW_UP            "panel-arrow-up"
#define PANEL_STOCK_ARROW_DOWN          "panel-arrow-down"

/* stock items  - no point in theme the icons one these,
 * they use stock gtk icons and just modify the text
 * for the stock item.
 */
#define PANEL_STOCK_EXECUTE             "panel-execute"

void        panel_init_stock_icons_and_items (void);
GtkIconSize panel_menu_icon_get_size         (void);
GtkIconSize panel_foobar_icon_get_size       (void);
GtkIconSize panel_button_icon_get_size       (void);

G_END_DECLS

#endif /* __PANEL_STOCK_ICONS_H__ */
