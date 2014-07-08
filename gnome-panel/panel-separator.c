/*
 * panel-separator.c: panel "Separator" module
 *
 * Copyright (C) 2005 Carlos Garcia Campos <carlosgc@gnome.org>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Carlos Garcia Campos <carlosgc@gnome.org>
 */

#include <config.h>

#include "panel-background.h"
#include "panel-layout.h"

#include "panel-separator.h"

#define PANEL_SEPARATOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_SEPARATOR, PanelSeparatorPrivate))

struct _PanelSeparatorPrivate {
	AppletInfo     *info;
	PanelWidget    *panel;

	GtkOrientation  orientation;
	GtkWidget      *separator;

	unsigned char   force_background_redraw: 1;
};

G_DEFINE_TYPE (PanelSeparator, panel_separator, GTK_TYPE_EVENT_BOX)

static void
panel_separator_get_preferred_width (GtkWidget *widget,
				     gint      *minimal_width,
				     gint      *natural_width)
{
	PanelSeparator *separator;
	int             size;

	separator = PANEL_SEPARATOR (widget);

	size = panel_toplevel_get_size (separator->priv->panel->toplevel);

	if (separator->priv->orientation == GTK_ORIENTATION_VERTICAL)
		*minimal_width = *natural_width = size;
	else {
		/* set width based on css styling & width of GtkSeparator */
		GtkStyleContext *context;
		GtkStateFlags    state;
		GtkWidget       *child;
		GtkBorder        padding;
		GtkBorder        border;
		gint             width;

		context = gtk_widget_get_style_context (widget);
		state = gtk_widget_get_state_flags (widget);

		child = gtk_bin_get_child (GTK_BIN (widget));

		if (child && gtk_widget_get_visible (child))
			gtk_widget_get_preferred_width (child,
							minimal_width,
							natural_width);
		else
			*minimal_width = *natural_width = 0;

		gtk_style_context_get_border (context, state, &border);
		gtk_style_context_get_padding (context, state, &padding);

		width = border.left + border.right + padding.left + padding.right;
		*minimal_width += width;
		*natural_width += width;
	}
}

static void
panel_separator_get_preferred_height (GtkWidget *widget,
				      gint      *minimal_height,
				      gint      *natural_height)
{
	PanelSeparator *separator;
	int             size;

	separator = PANEL_SEPARATOR (widget);

	size = panel_toplevel_get_size (separator->priv->panel->toplevel);

	if (separator->priv->orientation == GTK_ORIENTATION_VERTICAL) {
		/* set height based on css styling & height of GtkSeparator */
		GtkStyleContext *context;
		GtkStateFlags    state;
		GtkWidget       *child;
		GtkBorder        padding;
		GtkBorder        border;
		gint             height;

		context = gtk_widget_get_style_context (widget);
		state = gtk_widget_get_state_flags (widget);

		child = gtk_bin_get_child (GTK_BIN (widget));

		if (child && gtk_widget_get_visible (child))
			gtk_widget_get_preferred_height (child,
							 minimal_height,
							 natural_height);
		else
			*minimal_height = *natural_height = 0;

		gtk_style_context_get_border (context, state, &border);
		gtk_style_context_get_padding (context, state, &padding);

		height = border.top + border.bottom + padding.top + padding.bottom;
		*minimal_height += height;
		*natural_height += height;
	} else
		*minimal_height = *natural_height = size;
}

static void
panel_separator_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
	PanelSeparator  *separator = PANEL_SEPARATOR (widget);
	GtkAllocation    old_allocation;
	GtkStyleContext *context;
	GtkStateFlags    state;
	GtkBorder        padding;
	GtkBorder        border;
	GtkAllocation    new_allocation;
	GtkWidget       *child;
	PanelBackground *background;

	gtk_widget_get_allocation (widget, &old_allocation);

	GTK_WIDGET_CLASS (panel_separator_parent_class)->size_allocate (widget, allocation);

	/* We cannot return early here if allocation == old_allocation because
	 * the event_box base class just sized the child and we need to
	 * recalculate and resize the child (the actual GtkSeparator).
	 *
	 * Preferably, the event_box base class would offer a method of moving
	 * the private event window, in which case, calling its size_allocate
	 * would not be necessary. But, alas, no. */

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);

	gtk_style_context_get_border (context, state, &border);
	gtk_style_context_get_padding (context, state, &padding);

	new_allocation.x = border.left + padding.left;
	new_allocation.y = border.top + padding.top;
	new_allocation.width = allocation->width -
		(border.left + border.right + padding.left + padding.right);
	new_allocation.height = allocation->height -
		(border.top + border.bottom + padding.top + padding.bottom);
	new_allocation.width = MAX (1, new_allocation.width);
	new_allocation.height = MAX (1, new_allocation.height);

	child = gtk_bin_get_child (GTK_BIN (widget));
	if (child && gtk_widget_get_visible (child))
		gtk_widget_size_allocate (child, &new_allocation);

	if (!separator->priv->force_background_redraw &&
	    (old_allocation.x      == allocation->x &&
	     old_allocation.y      == allocation->y &&
	     old_allocation.width  == allocation->width &&
	     old_allocation.height == allocation->height))
		return;

	separator->priv->force_background_redraw = FALSE;

	background = &separator->priv->panel->background;

	if (background->type == PANEL_BACK_NONE ||
	   (background->type == PANEL_BACK_COLOR && !background->has_alpha))
		return;

	panel_separator_change_background (separator);
}

static void
panel_separator_parent_set (GtkWidget *widget,
			   GtkWidget *previous_parent)
{
	PanelSeparator *separator;
	GtkWidget      *parent;

	separator = PANEL_SEPARATOR (widget);

	parent = gtk_widget_get_parent (widget);
	g_assert (!parent || PANEL_IS_WIDGET (parent));

	separator->priv->panel = (PanelWidget *) parent;
}

static void
panel_separator_class_init (PanelSeparatorClass *klass)
{
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	widget_class->get_preferred_width  = panel_separator_get_preferred_width;
	widget_class->get_preferred_height = panel_separator_get_preferred_height;
	widget_class->size_allocate        = panel_separator_size_allocate;
	widget_class->parent_set           = panel_separator_parent_set;

	g_type_class_add_private (klass, sizeof (PanelSeparatorPrivate));
}

static void
panel_separator_init (PanelSeparator *separator)
{
	GtkStyleContext *context;

	separator->priv = PANEL_SEPARATOR_GET_PRIVATE (separator);

	separator->priv->info  = NULL;
	separator->priv->panel = NULL;
	separator->priv->orientation = GTK_ORIENTATION_HORIZONTAL;
	separator->priv->separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
	separator->priv->force_background_redraw = FALSE;

	gtk_container_add (GTK_CONTAINER (separator),
			   separator->priv->separator);
	gtk_widget_show (separator->priv->separator);

	context = gtk_widget_get_style_context (GTK_WIDGET (separator));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_HORIZONTAL);
}

void
panel_separator_set_orientation (PanelSeparator   *separator,
				 PanelOrientation  orientation)
{
	GtkOrientation orient = GTK_ORIENTATION_HORIZONTAL;
	GtkOrientation orient_separator = GTK_ORIENTATION_VERTICAL;
	GtkStyleContext *context;

	g_return_if_fail (PANEL_IS_SEPARATOR (separator));

	switch (orientation) {
	case PANEL_ORIENTATION_TOP:
	case PANEL_ORIENTATION_BOTTOM:
		orient = GTK_ORIENTATION_HORIZONTAL;
		orient_separator = GTK_ORIENTATION_VERTICAL;
		break;
	case PANEL_ORIENTATION_RIGHT:
	case PANEL_ORIENTATION_LEFT:
		orient = GTK_ORIENTATION_VERTICAL;
		orient_separator = GTK_ORIENTATION_HORIZONTAL;
		break;
	}

	if (orient == separator->priv->orientation)
		return;

	/* Use css class "horizontal"/"vertical" for theming */
	context = gtk_widget_get_style_context (GTK_WIDGET (separator));
	if (orient == GTK_ORIENTATION_HORIZONTAL) {
		gtk_style_context_add_class (context, GTK_STYLE_CLASS_HORIZONTAL);
		gtk_style_context_remove_class (context, GTK_STYLE_CLASS_VERTICAL);
	} else {
		gtk_style_context_add_class (context, GTK_STYLE_CLASS_VERTICAL);
		gtk_style_context_remove_class (context, GTK_STYLE_CLASS_HORIZONTAL);
	}

	separator->priv->orientation = orient;
	gtk_orientable_set_orientation (GTK_ORIENTABLE (separator->priv->separator),
					orient_separator);

	separator->priv->force_background_redraw = TRUE;
	gtk_widget_queue_resize (GTK_WIDGET (separator));
}

void
panel_separator_load (PanelWidget *panel,
		      const char  *id,
		      GSettings   *settings)
{
	PanelSeparator *separator;

	separator = g_object_new (PANEL_TYPE_SEPARATOR, NULL);

	separator->priv->info = panel_applet_register (GTK_WIDGET (separator),
						       panel,
						       PANEL_OBJECT_SEPARATOR,
						       id, settings,
						       NULL, NULL
						       );

	if (!separator->priv->info) {
		gtk_widget_destroy (GTK_WIDGET (separator));
		return;
	}

	panel_widget_set_applet_expandable (panel, GTK_WIDGET (separator),
					    FALSE, TRUE);
	panel_widget_set_applet_size_constrained (panel,
						  GTK_WIDGET (separator), TRUE);
}

void
panel_separator_create (PanelToplevel       *toplevel,
			PanelObjectPackType  pack_type,
			int                  pack_index)
{
	panel_layout_object_create (PANEL_OBJECT_SEPARATOR, NULL,
				    panel_toplevel_get_id (toplevel),
				    pack_type, pack_index);
}

void
panel_separator_change_background (PanelSeparator *separator)
{
	panel_background_change_background_on_widget (&separator->priv->panel->background,
						      GTK_WIDGET (separator));
}
