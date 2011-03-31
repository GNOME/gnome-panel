/*
 * panel-menu-bar-object.c: a base class for menu bar objects
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

#include "panel-background.h"
#include "panel-typebuiltins.h"

#include "panel-menu-bar-object.h"

G_DEFINE_TYPE (PanelMenuBarObject, panel_menu_bar_object, GTK_TYPE_MENU_BAR)

#define PANEL_MENU_BAR_OBJECT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_MENU_BAR_OBJECT, PanelMenuBarObjectPrivate))

struct _PanelMenuBarObjectPrivate {
	PanelWidget *panel;

	PanelOrientation orientation;
};

enum {
	PROP_0,
	PROP_ORIENTATION,
};

static void panel_menu_bar_object_update_text_gravity (PanelMenuBarObject *menubar);
static void panel_menu_bar_object_update_orientation (PanelMenuBarObject *menubar);

static void
panel_menu_bar_object_init (PanelMenuBarObject *menubar)
{
        GtkStyleContext *context;
        GtkCssProvider *provider;

	menubar->priv = PANEL_MENU_BAR_OBJECT_GET_PRIVATE (menubar);

        provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_data (provider,
                                         "PanelMenuBarObject {\n"
                                         " border-width: 0px;\n"
                                         "}",
                                         -1, NULL);
        context = gtk_widget_get_style_context (GTK_WIDGET (menubar));
        gtk_style_context_add_provider (context,
                                        GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref (provider);
	gtk_style_context_add_class (context, "gnome-panel-menu-bar");

	menubar->priv->panel = NULL;
}

static void
panel_menu_bar_object_get_property (GObject	*object,
				    guint	 prop_id,
				    GValue	*value,
				    GParamSpec	*pspec)
{
	PanelMenuBarObject *menubar;

	g_return_if_fail (PANEL_IS_MENU_BAR_OBJECT (object));

	menubar = PANEL_MENU_BAR_OBJECT (object);

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
panel_menu_bar_object_set_property (GObject	  *object,
				    guint	   prop_id,
				    const GValue *value,
				    GParamSpec	  *pspec)
{
	PanelMenuBarObject *menubar;

	g_return_if_fail (PANEL_IS_MENU_BAR_OBJECT (object));

	menubar = PANEL_MENU_BAR_OBJECT (object);

	switch (prop_id) {
	case PROP_ORIENTATION:
		panel_menu_bar_object_set_orientation (menubar,
						       g_value_get_enum (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_menu_bar_object_size_allocate (GtkWidget     *widget,
				     GtkAllocation *allocation)
{
	GtkAllocation    old_allocation;
	GtkAllocation    widget_allocation;
	PanelBackground *background;

	gtk_widget_get_allocation (widget, &widget_allocation);

	old_allocation.x      = widget_allocation.x;
	old_allocation.y      = widget_allocation.y;
	old_allocation.width  = widget_allocation.width;
	old_allocation.height = widget_allocation.height;

	GTK_WIDGET_CLASS (panel_menu_bar_object_parent_class)->size_allocate (widget, allocation);

	if (old_allocation.x      == allocation->x &&
	    old_allocation.y      == allocation->y &&
	    old_allocation.width  == allocation->width &&
	    old_allocation.height == allocation->height)
		return;

	background = &PANEL_MENU_BAR_OBJECT (widget)->priv->panel->background;

	if (background->type == PANEL_BACK_NONE ||
	   (background->type == PANEL_BACK_COLOR && !background->has_alpha))
		return;

	panel_menu_bar_object_change_background (PANEL_MENU_BAR_OBJECT (widget));
}

static void
panel_menu_bar_object_class_init (PanelMenuBarObjectClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass   *) klass;
	GtkWidgetClass *widget_class  = (GtkWidgetClass *) klass;

	gobject_class->get_property = panel_menu_bar_object_get_property;
        gobject_class->set_property = panel_menu_bar_object_set_property;

	widget_class->size_allocate = panel_menu_bar_object_size_allocate;

	g_type_class_add_private (klass, sizeof (PanelMenuBarObjectPrivate));

	g_object_class_install_property (
		gobject_class,
		PROP_ORIENTATION,
		g_param_spec_enum ("orientation",
				   "Orientation",
				   "The PanelMenuBarObject orientation",
				   PANEL_TYPE_ORIENTATION,
				   PANEL_ORIENTATION_TOP,
				   G_PARAM_READWRITE));
}

static gboolean
panel_menu_bar_object_on_draw (GtkWidget *widget,
			       cairo_t   *cr,
			       gpointer   data)
{
	PanelMenuBarObject *menubar = data;

	if (gtk_widget_has_focus (GTK_WIDGET (menubar))) {
                GtkStyleContext *context;

                context = gtk_widget_get_style_context (widget);
                gtk_style_context_save (context);
                gtk_style_context_set_state (context,
					     gtk_widget_get_state_flags (widget));

                cairo_save (cr);
		gtk_render_focus (context, cr,
                                  0, 0,
                                  gtk_widget_get_allocated_width (widget),
                                  gtk_widget_get_allocated_height (widget));
                cairo_restore (cr);

                gtk_style_context_restore (context);
        }

	return FALSE;
}

void
panel_menu_bar_object_object_load_finish (PanelMenuBarObject *menubar,
					  PanelWidget        *panel)
{
	menubar->priv->panel = panel;

	/* we didn't do this on "applet-added" since we didn't have the panel yet */
	panel_menu_bar_object_change_background (menubar);
        panel_menu_bar_object_update_orientation (menubar);
	panel_menu_bar_object_update_text_gravity (menubar);

	g_signal_connect (menubar, "screen-changed",
			  G_CALLBACK (panel_menu_bar_object_update_text_gravity),
			  NULL);

	g_signal_connect_after (menubar, "focus-in-event",
				G_CALLBACK (gtk_widget_queue_draw), menubar);
	g_signal_connect_after (menubar, "focus-out-event",
				G_CALLBACK (gtk_widget_queue_draw), menubar);
	g_signal_connect_after (menubar, "draw",
				G_CALLBACK (panel_menu_bar_object_on_draw), menubar);
	gtk_widget_set_can_focus (GTK_WIDGET (menubar), TRUE);

	panel_widget_set_applet_expandable (panel, GTK_WIDGET (menubar), FALSE, TRUE);
}

void
panel_menu_bar_object_change_background (PanelMenuBarObject *menubar)
{
	if (!menubar->priv->panel)
		return;

	panel_background_change_background_on_widget (&menubar->priv->panel->background,
						      GTK_WIDGET (menubar));
}

static void
set_item_text_gravity (GtkWidget *item)
{
	GtkWidget    *label;
	PangoLayout  *layout;
	PangoContext *context;

	label = gtk_bin_get_child (GTK_BIN (item));
	layout = gtk_label_get_layout (GTK_LABEL (label));
	context = pango_layout_get_context (layout);
	pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);
}

static void
panel_menu_bar_object_update_text_gravity (PanelMenuBarObject *menubar)
{
	GList *children, *l;

	children = gtk_container_get_children (GTK_CONTAINER (menubar));
	for (l = children; l != NULL; l = l->next)
		set_item_text_gravity (GTK_WIDGET (l->data));
	g_list_free (children);
}

static void
set_item_text_angle_and_alignment (GtkWidget *item,
				   double     text_angle,
				   float      xalign,
				   float      yalign)
{
	GtkWidget *label;

	label = gtk_bin_get_child (GTK_BIN (item));

	gtk_label_set_angle (GTK_LABEL (label), text_angle);

	gtk_misc_set_alignment (GTK_MISC (label), xalign, yalign);
}

static void
panel_menu_bar_object_update_orientation (PanelMenuBarObject *menubar)
{
	GtkPackDirection pack_direction;
	double           text_angle;
	float            text_xalign;
	float            text_yalign;
	GList           *children, *l;

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

	children = gtk_container_get_children (GTK_CONTAINER (menubar));
	for (l = children; l != NULL; l = l->next)
		set_item_text_angle_and_alignment (GTK_WIDGET (l->data),
						   text_angle,
						   text_xalign,
						   text_yalign);
	g_list_free (children);
}

void
panel_menu_bar_object_set_orientation (PanelMenuBarObject *menubar,
				       PanelOrientation    orientation)
{
        g_return_if_fail (PANEL_IS_MENU_BAR_OBJECT (menubar));

        if (menubar->priv->orientation == orientation)
                return;

        menubar->priv->orientation = orientation;

        panel_menu_bar_object_update_orientation (menubar);

        g_object_notify (G_OBJECT (menubar), "orientation");
}

PanelOrientation
panel_menu_bar_object_get_orientation (PanelMenuBarObject *menubar)
{
        g_return_val_if_fail (PANEL_IS_MENU_BAR_OBJECT (menubar), 0);

        return menubar->priv->orientation;
}
