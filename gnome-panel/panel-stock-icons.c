/*
 * panel-stock-icons.c panel stock icons registration
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

#include "config.h"

#include "panel-stock-icons.h"

#include <gtk/gtkstock.h>
#include <gtk/gtkiconfactory.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-desktop-item.h>

#include "panel-globals.h"

static GtkIconSize panel_menu_icon_size = 0;
static GtkIconSize panel_menu_bar_icon_size = 0;
static GtkIconSize panel_button_icon_size = 0;

GtkIconSize
panel_menu_icon_get_size (void)
{
	return panel_menu_icon_size;
}

GtkIconSize
panel_menu_bar_icon_get_size (void)
{
	return panel_menu_bar_icon_size;
}

GtkIconSize
panel_button_icon_get_size (void)
{
	return panel_button_icon_size;
}

typedef struct {
	char *stock_id;
	char *icon;
} PanelStockIcon;

static PanelStockIcon stock_icons [] = {
	{ PANEL_STOCK_RUN,                 "gnome-run" },
	{ PANEL_STOCK_SEARCHTOOL,          "gnome-searchtool" },
	{ PANEL_STOCK_SCREENSHOT,          "gnome-screenshot" },
	{ PANEL_STOCK_LOCKSCREEN,          "gnome-lockscreen" },
	{ PANEL_STOCK_LOGOUT,              "gnome-logout" },
	{ PANEL_STOCK_FORCE_QUIT,          "panel-force-quit" },
	{ PANEL_STOCK_GNOME_LOGO,          "gnome-logo-icon-transparent"},
	{ PANEL_STOCK_APPLICATIONS,	   "gnome-applications"},
	{ PANEL_STOCK_MAIN_MENU,           "gnome-main-menu"},
	{ PANEL_STOCK_DEBIAN,              "gnome-debian"},
	{ PANEL_STOCK_SUSE,                "gnome-suse"},
	{ PANEL_STOCK_CDE,                 "cdeappmenu"},
	{ PANEL_STOCK_ACCESSORIES,         "gnome-util"},
	{ PANEL_STOCK_AMUSEMENTS,          "gnome-amusements"},
	{ PANEL_STOCK_MULTIMEDIA,          "gnome-multimedia"},
	{ PANEL_STOCK_INTERNET,            "gnome-globe"},
	{ PANEL_STOCK_UTILITY,             "advanced-directory"},
	{ PANEL_STOCK_LAUNCHER,            "launcher-program" },
	{ PANEL_STOCK_ACTION,		   "gnome-other" },
	{ PANEL_STOCK_DRAWER,              "drawer" },
	{ PANEL_STOCK_APPLETS,             "gnome-applets" },
	{ PANEL_STOCK_DESKTOP,             "gnome-ccdesktop" },
};

static void
panel_init_stock_icons (GtkIconFactory *factory)
{
	GtkIconSource *source;
	int            i;


	source = gtk_icon_source_new ();

	for (i = 0; i < G_N_ELEMENTS (stock_icons); i++) {
		GtkIconSet *set;
		char       *filename;

		if (!stock_icons [i].icon)
			continue;

		/* Look up the icon at an arbitrary large size; if we are
		 * adding at only one size, we want the largest image
		 * available
		 */
		filename = gnome_desktop_item_find_icon (
				panel_icon_theme, stock_icons [i].icon, 1000, 0);
		if (!filename) {
			g_warning (_("Unable to load panel stock icon '%s'\n"), stock_icons [i].icon);

			/* FIXME: does this take into account the theme? */
			set = gtk_icon_factory_lookup_default (GTK_STOCK_MISSING_IMAGE);
			gtk_icon_factory_add (factory, stock_icons [i].stock_id, set);
			continue;
		}

		gtk_icon_source_set_filename (source, filename);
		g_free (filename);

		set = gtk_icon_set_new ();
		gtk_icon_set_add_source (set, source);

		gtk_icon_factory_add (factory, stock_icons [i].stock_id, set);
		gtk_icon_set_unref (set);
	}

	gtk_icon_source_free (source);

}

typedef struct {
	char *stock_id;
	char *stock_icon_id;
	char *label;
} PanelStockItem;

static PanelStockItem stock_items [] = {
	{ PANEL_STOCK_EXECUTE,    GTK_STOCK_EXECUTE,       N_("_Run") },
	{ PANEL_STOCK_FORCE_QUIT, PANEL_STOCK_FORCE_QUIT,  N_("_Force quit") },
	{ PANEL_STOCK_CLEAR,      GTK_STOCK_CLEAR,         N_("C_lear") }
};

static void
panel_init_stock_items (GtkIconFactory *factory)
{
	GtkStockItem *items;
	int           i;

	items = g_new (GtkStockItem, G_N_ELEMENTS (stock_items));

	for (i = 0; i < G_N_ELEMENTS (stock_items); i++) {
		GtkIconSet *icon_set;

		items [i].stock_id           = g_strdup (stock_items [i].stock_id);
		items [i].label              = g_strdup (stock_items [i].label);
		items [i].modifier           = 0;
		items [i].keyval             = 0;
		items [i].translation_domain = g_strdup (GETTEXT_PACKAGE);

		/* FIXME: does this take into account the theme? */
		icon_set = gtk_icon_factory_lookup_default (stock_items [i].stock_icon_id);
		gtk_icon_factory_add (factory, stock_items [i].stock_id, icon_set);
	}

	gtk_stock_add_static (items, G_N_ELEMENTS (stock_items));
}

void
panel_init_stock_icons_and_items (void)
{
	GtkIconFactory *factory;

	panel_menu_icon_size = gtk_icon_size_register ("panel-menu",
						       PANEL_DEFAULT_MENU_ICON_SIZE,
						       PANEL_DEFAULT_MENU_ICON_SIZE);

	panel_menu_bar_icon_size = gtk_icon_size_register ("panel-foobar", 20, 20);

	panel_button_icon_size = gtk_icon_size_register ("panel-button",
							 PANEL_DEFAULT_BUTTON_ICON_SIZE,
							 PANEL_DEFAULT_BUTTON_ICON_SIZE);

	factory = gtk_icon_factory_new ();
	gtk_icon_factory_add_default (factory);

	panel_init_stock_icons (factory);
	panel_init_stock_items (factory);

	g_object_unref (factory);
}
