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
#include "panel-background.h"
#include "panel-action-button.h"
#include "panel-stock-icons.h"
#include "panel-recent.h"
#include "applet.h"
#include "menu.h"
#include "panel-globals.h"
#include "panel-profile.h"
#include "panel-lockdown.h"

#define PANEL_MENU_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_MENU_BAR, PanelMenuBarPrivate))

struct _PanelMenuBarPrivate {
	AppletInfo            *info;
	PanelWidget           *panel;

	GtkWidget             *applications_menu;
	GtkWidget             *actions_menu;
	GtkWidget             *actions_item;
};

static GObjectClass *parent_class;
static GtkWidget *panel_menu_bar_create_actions_menu (PanelMenuBar *menubar);

static void
panel_menu_bar_show_menu (PanelMenuBar *menubar,
			  GtkWidget    *menu)
{
	gtk_menu_set_screen (GTK_MENU (menu),
			     gtk_widget_get_screen (GTK_WIDGET (menubar)));
}

static void
panel_menu_bar_connect_menu_signals (PanelMenuBar *menubar,
				     GtkWidget    *menu)
{
	/* intercept all right button clicks makes sure they don't
	   go to the object itself */
	g_signal_connect (menu, "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	g_signal_connect (menu, "destroy", G_CALLBACK (gtk_widget_destroyed),
			  &menu);

	g_signal_connect_swapped (menu, "show",
				  G_CALLBACK (panel_menu_bar_show_menu),
				  menubar);

	g_signal_connect_swapped (menu, "hide",
				  G_CALLBACK (gtk_menu_shell_deselect), 
				  menubar);
}

static void
panel_menu_bar_recreate_actions_menu (PanelMenuBar *menubar)
{
	if (menubar->priv->actions_menu) {
		gtk_widget_destroy (menubar->priv->actions_menu);
		menubar->priv->actions_menu = panel_menu_bar_create_actions_menu (menubar);
		panel_menu_bar_connect_menu_signals (menubar,
						     menubar->priv->actions_menu);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar->priv->actions_item),
					   menubar->priv->actions_menu);
	}
}

static GtkWidget *
panel_menu_bar_create_actions_menu (PanelMenuBar *menubar)
{
	GtkWidget *actions_menu;
	GtkWidget *item;
	gboolean   separator_inserted;
	gboolean   last_is_separator;

	actions_menu = panel_create_menu ();

	last_is_separator = FALSE;

	item = menu_create_action_item (PANEL_ACTION_RUN);
	if (item != NULL) {
		gtk_menu_shell_append (GTK_MENU_SHELL (actions_menu), item);
		add_menu_separator (actions_menu);
		last_is_separator = TRUE;
	}

	if (panel_is_program_in_path  ("gnome-search-tool")) {
		item = menu_create_action_item (PANEL_ACTION_SEARCH);
		if (item != NULL) {
			gtk_menu_shell_append (GTK_MENU_SHELL (actions_menu),
					       item);
			last_is_separator = FALSE;
		}
	}

	panel_recent_append_documents_menu (actions_menu);

	if (panel_is_program_in_path ("gnome-screenshot")) {
		item = menu_create_action_item (PANEL_ACTION_SCREENSHOT);
		if (item != NULL) {
			if (!last_is_separator)
				add_menu_separator (actions_menu);
			gtk_menu_shell_append (GTK_MENU_SHELL (actions_menu),
					       item);
			last_is_separator = FALSE;
		}
	}

	separator_inserted = FALSE;

	item = menu_create_action_item (PANEL_ACTION_LOCK);
	if (item != NULL) {
		add_menu_separator (actions_menu);
		gtk_menu_shell_append (GTK_MENU_SHELL (actions_menu), item);
		separator_inserted = TRUE;
	}

	if (panel_is_program_in_path ("xscreensaver")) {
		item = menu_create_action_item (PANEL_ACTION_LOGOUT);

		if (item != NULL) {
			if (!separator_inserted)
				add_menu_separator (actions_menu);
			gtk_menu_shell_append (GTK_MENU_SHELL (actions_menu),
					       item);
		}
	}

	return actions_menu;
}

static void
panel_menu_bar_instance_init (PanelMenuBar      *menubar,
			      PanelMenuBarClass *klass)
{
	GtkWidget *item;
	GtkWidget *image;

	menubar->priv = PANEL_MENU_BAR_GET_PRIVATE (menubar);

	menubar->priv->info = NULL;

	menubar->priv->applications_menu = create_applications_menu ("applications.menu", NULL);
	panel_menu_bar_connect_menu_signals (menubar,
					     menubar->priv->applications_menu);

	menubar->priv->actions_menu = panel_menu_bar_create_actions_menu (menubar);
	panel_menu_bar_connect_menu_signals (menubar,
					     menubar->priv->actions_menu);

	item = gtk_image_menu_item_new_with_label (_("Applications"));
	image = gtk_image_new_from_stock (PANEL_STOCK_GNOME_LOGO,
					  panel_menu_bar_icon_get_size ());
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item),
				   menubar->priv->applications_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);

	menubar->priv->actions_item = gtk_menu_item_new_with_label (_("Actions"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar->priv->actions_item),
				   menubar->priv->actions_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar),
			       menubar->priv->actions_item);

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
panel_menu_bar_realized (GtkWidget     *widget,
			 GtkAllocation *allocation,
			 gpointer       user_data)
{
	g_signal_handlers_disconnect_by_func (widget,
					      G_CALLBACK (panel_menu_bar_realized),
					      NULL);

	panel_menu_bar_change_background (PANEL_MENU_BAR (widget));
}

static void
panel_menu_bar_class_init (PanelMenuBarClass *klass)
{
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

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
	
	/* FIXME: eeeeek, bad hack
	 * If the panel background is an image (or translucent color), we
	 * can't have the right image now because the menubar has no
	 * size. So we wait for the first size-allocate signal... */
	g_signal_connect (GTK_WIDGET (menubar), "size-allocate",
			  G_CALLBACK (panel_menu_bar_realized), NULL);
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

void panel_menu_bar_change_background (PanelMenuBar *menubar)
{
	GtkRcStyle       *rc_style;
	GtkStyle         *style;
	const PanelColor *color;
	GdkGC            *gc;
	GdkPixmap        *pixmap;
	const GdkPixmap  *bg_pixmap;

	/* reset style */
	gtk_widget_set_style (GTK_WIDGET (menubar), NULL);
	rc_style = gtk_rc_style_new ();
	gtk_widget_modify_style (GTK_WIDGET (menubar), rc_style);
	g_object_unref (rc_style);

	switch (panel_background_get_type (&menubar->priv->panel->background)) {
	case PANEL_BACK_NONE:
		break;
	case PANEL_BACK_COLOR:
		color = panel_background_get_color (&menubar->priv->panel->background);
		if (color->alpha == 0xffff) {
			gtk_widget_modify_bg (GTK_WIDGET (menubar),
					      GTK_STATE_NORMAL, &(color->gdk));
			break;
		}
		/* else, we have an image, so don't break */
	case PANEL_BACK_IMAGE:
		bg_pixmap = panel_background_get_pixmap (&menubar->priv->panel->background);
		if (!bg_pixmap)
			return;

		gc = gdk_gc_new (GTK_WIDGET (menubar)->window);
		g_return_if_fail (GDK_IS_GC (gc));

		pixmap = gdk_pixmap_new (GTK_WIDGET (menubar)->window,
					 GTK_WIDGET (menubar)->allocation.width,
					 GTK_WIDGET (menubar)->allocation.height,
					 -1);

		gdk_draw_drawable (GDK_DRAWABLE (pixmap),
				   gc, 
				   GDK_DRAWABLE (bg_pixmap),
				   GTK_WIDGET (menubar)->allocation.x,
				   GTK_WIDGET (menubar)->allocation.y,
				   0, 0,
				   GTK_WIDGET (menubar)->allocation.width,
				   GTK_WIDGET (menubar)->allocation.height);

		g_object_unref (gc);

		style = gtk_style_copy (GTK_WIDGET (menubar)->style);
		if (style->bg_pixmap[GTK_STATE_NORMAL])
			g_object_unref (style->bg_pixmap[GTK_STATE_NORMAL]);
		style->bg_pixmap[GTK_STATE_NORMAL] = g_object_ref (pixmap);
		gtk_widget_set_style (GTK_WIDGET (menubar), style);

		g_object_unref (pixmap);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}
