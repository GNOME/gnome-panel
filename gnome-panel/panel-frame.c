/*
 * panel-frame.c: A frame which only draws certain edges.
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

#include "panel-frame.h"

#include <libgnome/gnome-i18n.h>

#include "panel-typebuiltins.h"

static GObjectClass *parent_class;

enum {
	PROP_0,
	PROP_EDGES
};

static void
panel_frame_size_request (GtkWidget      *widget,
			  GtkRequisition *requisition)
{
	PanelFrame *frame = (PanelFrame *) widget;
	GtkBin     *bin   = (GtkBin *) widget;

	requisition->width = 1;
	requisition->height = 1;

	if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
		gtk_widget_size_request (bin->child, requisition);

	requisition->width  += GTK_CONTAINER (widget)->border_width;
	requisition->height += GTK_CONTAINER (widget)->border_width;

	if (frame->edges & PANEL_EDGE_TOP)
		requisition->height += widget->style->xthickness;
	if (frame->edges & PANEL_EDGE_BOTTOM)
		requisition->height += widget->style->xthickness;
	if (frame->edges & PANEL_EDGE_LEFT)
		requisition->width += widget->style->ythickness;
	if (frame->edges & PANEL_EDGE_RIGHT)
		requisition->width += widget->style->ythickness;
}

static void
panel_frame_size_allocate (GtkWidget     *widget,
			   GtkAllocation *allocation)
{
	PanelFrame    *frame = (PanelFrame *) widget;
	GtkBin        *bin   = (GtkBin *) widget;
	GtkAllocation  child_allocation;
	int            border_width;

	widget->allocation = *allocation;

	border_width = GTK_CONTAINER (widget)->border_width;

	child_allocation.x      = allocation->x + border_width;
	child_allocation.y      = allocation->y + border_width;
	child_allocation.width  = allocation->width  - 2 * border_width;
	child_allocation.height = allocation->height - 2 * border_width;

	if (frame->edges & PANEL_EDGE_LEFT) {
		child_allocation.x     += widget->style->xthickness;
		child_allocation.width -= widget->style->xthickness;
	}

	if (frame->edges & PANEL_EDGE_TOP) {
		child_allocation.y      += widget->style->ythickness;
		child_allocation.height -= widget->style->ythickness;
	}

	if (frame->edges & PANEL_EDGE_RIGHT)
		child_allocation.width -= widget->style->xthickness;

	if (frame->edges & PANEL_EDGE_BOTTOM)
		child_allocation.height -= widget->style->ythickness;

	if (GTK_WIDGET_MAPPED (widget) &&
	    (child_allocation.x != bin->child->allocation.x ||
	     child_allocation.y != bin->child->allocation.y ||
	     child_allocation.width  != bin->child->allocation.width ||
	     child_allocation.height != bin->child->allocation.height))
		gdk_window_invalidate_rect (widget->window, &widget->allocation, FALSE);

	if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
		gtk_widget_size_allocate (bin->child, &child_allocation);
}

void
panel_frame_draw (GtkWidget      *widget,
		  PanelFrameEdge  edges)
{
	GdkGC *dark, *light, *black;
	int    x, y, width, height;
	int    xthickness, ythickness;

	if (edges == PANEL_EDGE_NONE)
		return;

	dark  = widget->style->dark_gc [widget->state];
	light = widget->style->light_gc [widget->state];
	black = widget->style->black_gc;

	xthickness = widget->style->xthickness;
	ythickness = widget->style->ythickness;

	x      = widget->allocation.x;
	y      = widget->allocation.y;
	width  = widget->allocation.width;
	height = widget->allocation.height;

	/* Copied from gtk_default_draw_shadow() */

	if (edges & PANEL_EDGE_BOTTOM && ythickness > 0) {
		if (ythickness > 1) {
			gdk_draw_line (widget->window, dark,
				       x, y + height - 2,
				       x + width - 1, y + height - 2);
			gdk_draw_line (widget->window, black,
				       x, y + height - 1,
				       x + width - 1, y + height - 1);
		} else
			gdk_draw_line (widget->window, dark,
				       x, y + height - 1,
				       x + width - 1, y + height - 1);
	}

	if (edges & PANEL_EDGE_RIGHT && xthickness > 0) {
		if (xthickness > 1) {
			gdk_draw_line (widget->window, dark,
				       x + width - 2, y,
				       x + width - 2, y + height - 1);

			gdk_draw_line (widget->window, black,
				       x + width - 1, y,
				       x + width - 1, y + height - 1);
		} else
			gdk_draw_line (widget->window, dark,
				       x + width - 1, y,
				       x + width - 1, y + height - 1);
	}

	if (edges & PANEL_EDGE_TOP && ythickness > 0) {
		gdk_draw_line (widget->window, light,
			       x, y, x + width - 1, y);

		if (ythickness > 1)
			gdk_draw_line (widget->window,
				       widget->style->bg_gc [widget->state],
				       x, y + 1, x + width - 1, y + 1);
	}

	if (edges & PANEL_EDGE_LEFT && xthickness > 0) {
		gdk_draw_line (widget->window, light,
			       x, y, x, y + height - 1);

		if (xthickness > 1)
			gdk_draw_line (widget->window,
				       widget->style->bg_gc [widget->state],
				       x + 1, y, x + 1, y + height - 1);
	}
}

static gboolean
panel_frame_expose (GtkWidget      *widget,
		    GdkEventExpose *event)
{
	PanelFrame *frame = (PanelFrame *) widget;
	gboolean    retval = FALSE;

	if (!GTK_WIDGET_DRAWABLE (widget))
		return retval;

	if (GTK_WIDGET_CLASS (parent_class)->expose_event)
		retval = GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);

	panel_frame_draw (widget, frame->edges);

	return retval;
}

static void
panel_frame_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
	PanelFrame *frame;

	g_return_if_fail (PANEL_IS_FRAME (object));

	frame = PANEL_FRAME (object);

	switch (prop_id) {
	case PROP_EDGES:
		panel_frame_set_edges (frame, g_value_get_enum (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_frame_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
	PanelFrame *frame;

	g_return_if_fail (PANEL_IS_FRAME (object));

	frame = PANEL_FRAME (object);

	switch (prop_id) {
	case PROP_EDGES:
		g_value_set_enum (value, frame->edges);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_frame_class_init (PanelFrameClass *klass)
{
	GObjectClass   *gobject_class   = (GObjectClass    *) klass;
	GtkWidgetClass *widget_class    = (GtkWidgetClass  *) klass;

	parent_class = g_type_class_peek_parent (klass);

	gobject_class->set_property = panel_frame_set_property;
        gobject_class->get_property = panel_frame_get_property;

	widget_class->size_request  = panel_frame_size_request;
	widget_class->size_allocate = panel_frame_size_allocate;
	widget_class->expose_event  = panel_frame_expose;

	g_object_class_install_property (
		gobject_class,
		PROP_EDGES,
		g_param_spec_enum (
			"edges",
			"Edges",
			"Which edges to draw",
			PANEL_TYPE_FRAME_EDGE,
			PANEL_EDGE_NONE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
panel_frame_instance_init (PanelFrame      *frame,
			      PanelFrameClass *klass)
{
	frame->edges = PANEL_EDGE_NONE;
}

GType
panel_frame_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelFrameClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_frame_class_init,
			NULL,
			NULL,
			sizeof (PanelFrame),
			0,
			(GInstanceInitFunc) panel_frame_instance_init,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_BIN, "PanelFrame", &info, 0);
	}

	return type;
}

GtkWidget *
panel_frame_new (PanelFrameEdge edges)
{
	return g_object_new (PANEL_TYPE_FRAME, "edges", edges, NULL);
}

void
panel_frame_set_edges (PanelFrame     *frame,
		       PanelFrameEdge  edges)
{
	g_return_if_fail (PANEL_IS_FRAME (frame));

	if (frame->edges == edges)
		return;

	frame->edges = edges;

	gtk_widget_queue_resize (GTK_WIDGET (frame));

	g_object_notify (G_OBJECT (frame), "edges");
}

PanelFrameEdge
panel_frame_get_edges (PanelFrame *frame)
{
	g_return_val_if_fail (PANEL_IS_FRAME (frame), 0);

	return frame->edges;
}
