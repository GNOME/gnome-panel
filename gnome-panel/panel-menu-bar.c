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

#include "panel-util.h"
#include "panel-background.h"
#include "panel-action-button.h"
#include "applet.h"
#include "menu.h"
#include "panel-menu-items.h"
#include "panel-globals.h"
#include "panel-profile.h"
#include "panel-lockdown.h"
#include "panel-stock-icons.h"
#include "panel-typebuiltins.h"

#define PANEL_MENU_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_MENU_BAR, PanelMenuBarPrivate))

struct _PanelMenuBarPrivate {
	AppletInfo  *info;
	PanelWidget *panel;

	GtkWidget   *applications_menu;
	GtkWidget   *applications_item;
	GtkWidget   *places_item;
	GtkWidget   *desktop_item;

	PanelOrientation orientation;
};

enum {
	PROP_0,
	PROP_ORIENTATION,
};

static GObjectClass *parent_class;

static void
panel_menu_bar_instance_init (PanelMenuBar      *menubar,
			      PanelMenuBarClass *klass)
{
	GtkWidget *image;

	menubar->priv = PANEL_MENU_BAR_GET_PRIVATE (menubar);

	menubar->priv->info = NULL;

	menubar->priv->applications_menu = create_applications_menu ("applications.menu", NULL);

	menubar->priv->applications_item = gtk_image_menu_item_new_with_label (_("Applications"));
	image = gtk_image_new_from_icon_name (PANEL_GNOME_LOGO_ICON,
					      panel_menu_bar_icon_get_size ());
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menubar->priv->applications_item),
				       image);

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar->priv->applications_item),
				   menubar->priv->applications_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar),
			       menubar->priv->applications_item);

	menubar->priv->places_item = panel_place_menu_item_new (FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar),
			       menubar->priv->places_item);

	menubar->priv->desktop_item = panel_desktop_menu_item_new (FALSE, TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar),
			       menubar->priv->desktop_item);
}

static void
panel_menu_bar_get_property (GObject	*object,
			     guint	 prop_id,
			     GValue	*value,
			     GParamSpec *pspec)
{
	PanelMenuBar *menubar;

	g_return_if_fail (PANEL_IS_MENU_BAR (object));

	menubar = PANEL_MENU_BAR (object);

	switch (prop_id) {
	case PROP_ORIENTATION:
		g_value_set_enum (value, menubar->priv->orientation);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_menu_bar_set_property (GObject	  *object,
			     guint	   prop_id,
			     const GValue *value,
			     GParamSpec	  *pspec)
{
	PanelMenuBar *menubar;

	g_return_if_fail (PANEL_IS_MENU_BAR (object));

	menubar = PANEL_MENU_BAR (object);

	switch (prop_id) {
	case PROP_ORIENTATION:
		panel_menu_bar_set_orientation (menubar, g_value_get_enum (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_menu_bar_parent_set (GtkWidget *widget,
			   GtkWidget *previous_parent)
{
	PanelMenuBar *menubar = PANEL_MENU_BAR (widget);

	g_assert (!widget->parent || PANEL_IS_WIDGET (widget->parent));

	menubar->priv->panel = (PanelWidget *) widget->parent;

	if (menubar->priv->applications_menu)
		panel_applet_menu_set_recurse (GTK_MENU (menubar->priv->applications_menu),
					       "menu_panel",
					       menubar->priv->panel);
	if (menubar->priv->places_item)
		panel_place_menu_item_set_panel (menubar->priv->places_item,
						 menubar->priv->panel);
	if (menubar->priv->desktop_item)
		panel_desktop_menu_item_set_panel (menubar->priv->desktop_item,
						   menubar->priv->panel);
}

static void
panel_menu_bar_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
	GtkAllocation old_allocation;

	old_allocation.x      = widget->allocation.x;
	old_allocation.y      = widget->allocation.y;
	old_allocation.width  = widget->allocation.width;
	old_allocation.height = widget->allocation.height;

	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

	if (old_allocation.x      == allocation->x &&
	    old_allocation.y      == allocation->y &&
	    old_allocation.width  == allocation->width &&
	    old_allocation.height == allocation->height)
		return;

	panel_menu_bar_change_background (PANEL_MENU_BAR (widget));
}

static void
panel_menu_bar_class_init (PanelMenuBarClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass   *) klass;
	GtkWidgetClass *widget_class  = (GtkWidgetClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	gobject_class->get_property = panel_menu_bar_get_property;
        gobject_class->set_property = panel_menu_bar_set_property;

	widget_class->parent_set = panel_menu_bar_parent_set;
	widget_class->size_allocate = panel_menu_bar_size_allocate;

	g_type_class_add_private (klass, sizeof (PanelMenuBarPrivate));

	g_object_class_install_property (
		gobject_class,
		PROP_ORIENTATION,
		g_param_spec_enum ("orientation",
				   "Orientation",
				   "The PanelMenuBar orientation",
				   PANEL_TYPE_ORIENTATION,
				   PANEL_ORIENTATION_TOP,
				   G_PARAM_READWRITE));

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

static gboolean
panel_menu_bar_on_expose (GtkWidget      *widget,
			  GdkEventExpose *event,
			  gpointer        data)
{
	PanelMenuBar *menubar = data;

	if (GTK_WIDGET_HAS_FOCUS (menubar))
		gtk_paint_focus (widget->style,
				 widget->window, 
				 GTK_WIDGET_STATE (menubar),
				 NULL,
				 widget,
				 "menubar-applet",
				 0, 0, -1, -1);
	return FALSE;
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

	if (panel_is_program_in_path ("gmenu-simple-editor")) {
		panel_applet_add_callback (menubar->priv->info,
					   "edit",
					   NULL,
					   _("_Edit Menus"),
					   NULL);
	}

	g_signal_connect_after (menubar, "focus-in-event",
				G_CALLBACK (gtk_widget_queue_draw), menubar);
	g_signal_connect_after (menubar, "focus-out-event",
				G_CALLBACK (gtk_widget_queue_draw), menubar);
	g_signal_connect_after (menubar, "expose-event",
				G_CALLBACK (panel_menu_bar_on_expose), menubar);
	GTK_WIDGET_SET_FLAGS (menubar, GTK_CAN_FOCUS);

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

	screen = gtk_widget_get_screen (GTK_WIDGET (menubar));

	if (!strcmp (callback_name, "help")) {
		panel_show_help (screen, "user-guide.xml", "gospanel-37");

	} else if (!strcmp (callback_name, "edit")) {
		GError *error = NULL;
		char   *argv [2] = {"gmenu-simple-editor", NULL};

		if (!gdk_spawn_on_screen (screen, NULL, argv, NULL,
					  G_SPAWN_SEARCH_PATH,
					  NULL, NULL, NULL, &error)) {
			panel_error_dialog (screen,
					    "cannot_exec_gmenu-simple-editor", TRUE,
					    _("Cannot execute '%s'"),
					    "%s",
					    "gmenu-simple-editor",
					    error->message);
			g_error_free (error);
		}
	}
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

static void
panel_menu_bar_set_no_background (PanelMenuBar *menubar)
{
	GtkRcStyle *rc_style;

	gtk_widget_set_style (GTK_WIDGET (menubar), NULL);
	rc_style = gtk_rc_style_new ();
	gtk_widget_modify_style (GTK_WIDGET (menubar), rc_style);
	gtk_rc_style_unref (rc_style);

}

static void
panel_menu_bar_set_image_background (PanelMenuBar *menubar)
{
	const GdkPixmap  *bg_pixmap;
	GdkPixmap        *pixmap;
	GtkStyle         *style;
	GdkGC            *gc;

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
	g_object_unref (style);

	g_object_unref (pixmap);
}

static void
panel_menu_bar_set_color_background (PanelMenuBar *menubar)
{
	const PanelColor *color;

	color = panel_background_get_color (&menubar->priv->panel->background);
	if (color->alpha != 0xffff) {
		panel_menu_bar_set_image_background (menubar);
		return;
	}

	gtk_widget_modify_bg (GTK_WIDGET (menubar), GTK_STATE_NORMAL, &color->gdk);
}

void
panel_menu_bar_change_background (PanelMenuBar *menubar)
{
	PanelBackgroundType type;

	panel_menu_bar_set_no_background (menubar);

	type = panel_background_get_type (&menubar->priv->panel->background);

	switch (type) {
	case PANEL_BACK_NONE:
		break;
	case PANEL_BACK_COLOR:
		panel_menu_bar_set_color_background (menubar);
		break;
	case PANEL_BACK_IMAGE:
		panel_menu_bar_set_image_background (menubar);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
set_item_text_angle_and_alignment (GtkWidget *item,
				   double     text_angle,
				   float      xalign,
				   float      yalign)
{
  GtkWidget *label;

  label = GTK_BIN (item)->child;

  gtk_label_set_angle (GTK_LABEL (label), text_angle);

  gtk_misc_set_alignment (GTK_MISC (label), xalign, yalign);
}

static void
panel_menu_bar_update_orientation (PanelMenuBar *menubar)
{
	GtkPackDirection pack_direction;
	double           text_angle;
	float            text_xalign;
	float            text_yalign;

	pack_direction = GTK_PACK_DIRECTION_LTR;
	text_angle = 0.0;
	text_xalign = 0.0;
	text_yalign = 0.5;

	switch (menubar->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
	case PANEL_ORIENTATION_BOTTOM:
		break;
	case PANEL_ORIENTATION_LEFT:
		pack_direction = GTK_PACK_DIRECTION_BTT;
		text_angle = 90.0;
		text_xalign = 0.5;
		text_yalign = 0.0;
		break;
	case PANEL_ORIENTATION_RIGHT:
		pack_direction = GTK_PACK_DIRECTION_TTB;
		text_angle = 270.0;
		text_xalign = 0.5;
		text_yalign = 0.0;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	gtk_menu_bar_set_pack_direction (GTK_MENU_BAR (menubar), pack_direction);
	gtk_menu_bar_set_child_pack_direction (GTK_MENU_BAR (menubar), pack_direction);

	set_item_text_angle_and_alignment (menubar->priv->applications_item,
					   text_angle,
					   text_xalign,
					   text_yalign);
	set_item_text_angle_and_alignment (menubar->priv->places_item,
					   text_angle,
					   text_xalign,
					   text_yalign);
	set_item_text_angle_and_alignment (menubar->priv->desktop_item,
					   text_angle,
					   text_xalign,
					   text_yalign);
}

void
panel_menu_bar_set_orientation (PanelMenuBar     *menubar,
				PanelOrientation  orientation)
{
        g_return_if_fail (PANEL_IS_MENU_BAR (menubar));

        if (menubar->priv->orientation == orientation)
                return;

        menubar->priv->orientation = orientation;

        panel_menu_bar_update_orientation (menubar);

        g_object_notify (G_OBJECT (menubar), "orientation");
}

PanelOrientation
panel_menu_bar_get_orientation (PanelMenuBar *menubar)
{
        g_return_val_if_fail (PANEL_IS_MENU_BAR (menubar), 0);

        return menubar->priv->orientation;
}
