/*
 * panel-toplevel.c: The panel's toplevel window object.
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

/* TODO:
 *
 *  o Keynav - I want to be able to move and resize with the keyboard.
 *    Also should be able to toggle expand and maybe hide/unhide. These
 *    all should match the window managers keybindings for move/resize,
 *    maximise/un-maximise and roll up.
 *  o Saving to GConf, config GUI.
 *  o Initial position when a new panel is created.
 *  o Collision avoidance.
 *  o Search for FIXME_FOR_NEW_TOPLEVEL and fix
 *             
 * Random problems:
 *  o Handling background colors and pictures ? The frame and the buttons
 *    will have problems.
 *  o When flipping between buttons and handles we should center the
 *    position temporarily like with rotate so we don't get movement.
 *  o When toggling buttons on a centered panel it loses the centered
 *    property
 *
 * Drawers:
 *  o When dragging an attached toplevel, we need to set the anchor widget
 *    dragging instead - see panel_widget_applet_drag_start()
 *  o When attached we should only have a single hide button with the
 *    arrow pointing in the opposite direction.
 *  o Really need a re-sizing variation of the animation
 */

#include <config.h>

#include "panel-toplevel.h"

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-program.h>

#include "panel-frame.h"
#include "panel-xutils.h"
#include "panel-multiscreen.h"
#include "panel-a11y.h"
#include "panel-typebuiltins.h"
#include "panel-marshal.h"
#include "panel-widget.h"

#define DEFAULT_SIZE              48
#define RESIZE_GRAB_AREA_SIZE     3
#define DEFAULT_AUTO_HIDE_SIZE    6
#define DEFAULT_HIDE_DELAY        100
#define DEFAULT_UNHIDE_DELAY      100
#define MINIMUM_WIDTH             100
#define SNAP_TOLERANCE            20
#define HIDE_BUTTON_SIZE          20
#define HANDLE_SIZE               10
#define N_ATTACH_TOPLEVEL_SIGNALS 5
#define N_ATTACH_WIDGET_SIGNALS   2

enum {
	HIDE_SIGNAL,
	UNHIDE_SIGNAL,
	POPUP_PANEL_MENU_SIGNAL,
	LAST_SIGNAL
};

struct _PanelToplevelPrivate {
	gboolean                expand;
	PanelOrientation        orientation;
	int                     size;

	/* relative to the monitor origin */
	int                     x;
	int                     y;

	int                     monitor;

	int                     hide_delay;
	int                     unhide_delay;
	int                     auto_hide_size;
	PanelAnimationSpeed     animation_speed;

	PanelState              state;

	char                   *name;
	char                   *description;

	guint                   move_timeout;
	guint                   hide_timeout;
	guint                   unhide_timeout;

	GdkRectangle            geometry;
	PanelFrameEdge          edges;

	/* The offset within the panel from which the panel
	 * drag was initiated relative to the screen origin.
	 */
	int                     drag_offset_x;
	int                     drag_offset_y;

	PanelFrameEdge          resize_edge;

	/* relative to the monitor origin */
	int                     animation_end_x;
	int                     animation_end_y;
	GTimeVal                animation_start_time;
	long                    animation_end_time;
	guint                   animation_timeout;

	PanelWidget            *panel_widget;
	PanelFrame             *inner_frame;
	GtkWidget              *table;
	GtkWidget              *hide_button_top;
	GtkWidget              *hide_button_bottom;
	GtkWidget              *hide_button_left;
	GtkWidget              *hide_button_right;

	PanelToplevel          *attach_toplevel;
	gulong                  attach_toplevel_signals [N_ATTACH_TOPLEVEL_SIGNALS];
	GtkWidget              *attach_widget;
	gulong                  attach_widget_signals [N_ATTACH_WIDGET_SIGNALS];

	guint                   auto_hide : 1;
	guint                   animate : 1;
	guint                   buttons_enabled : 1;
	guint                   arrows_enabled : 1;

	/* The co-ordinates are relative to center screen */
	guint                   x_centered : 1;
	guint                   y_centered : 1;

	/* The panel is not lined up with th screen edge */
	guint                   floating : 1;

	/* The panel is being dragged */
	guint                   in_drag : 1;

	/* We are currently animating a hide/show */
	guint                   animating : 1;

	/* The drag is a rotation */
	guint                   drag_is_rotate : 1;

	/* The drag is a resize */
	guint                   drag_is_resize : 1;

	/* The x-y co-ordinates temporarily specify the panel center */
	guint                   position_centered : 1;

	/* Auto-hide blocking */
	guint                   block_auto_hide : 1;

	/* The toplevel is "attached" to another widget */
	guint                   attached : 1;

	/* Hidden temporarily because the attach_toplevel was hidden */
	guint                   attach_hidden : 1;
};

static GObjectClass *parent_class;
static guint         toplevel_signals [LAST_SIGNAL] = { 0 };

enum {
	PROP_0,
	PROP_NAME,
	PROP_EXPAND,
	PROP_ORIENTATION,
	PROP_SIZE,
	PROP_X,
	PROP_X_CENTERED,
	PROP_Y,
	PROP_Y_CENTERED,
	PROP_MONITOR,
	PROP_AUTOHIDE,
	PROP_HIDE_DELAY,
	PROP_UNHIDE_DELAY,
	PROP_AUTOHIDE_SIZE,
	PROP_ANIMATE,
	PROP_ANIMATION_SPEED,
	PROP_BUTTONS_ENABLED,
	PROP_ARROWS_ENABLED,
};

static GdkScreen *
panel_toplevel_get_screen_geometry (PanelToplevel *toplevel,
				    int           *width,
				    int           *height)
{
	GdkScreen *screen;

	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), NULL);
	g_return_val_if_fail (width != NULL && height != NULL, NULL);

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));

	*width  = gdk_screen_get_width (screen);
	*height = gdk_screen_get_height (screen);

	return screen;
}

static GdkScreen *
panel_toplevel_get_monitor_geometry (PanelToplevel *toplevel,
				     int           *width,
				     int           *height)
{
	GdkScreen *screen;

	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), NULL);
	g_return_val_if_fail (width != NULL && height != NULL, NULL);

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));

	*width  = panel_multiscreen_width (screen, toplevel->priv->monitor);
	*height = panel_multiscreen_height (screen, toplevel->priv->monitor);

	return screen;
}

static void
panel_toplevel_calc_floating (PanelToplevel *toplevel)
{
	GdkScreen *screen;
	int        screen_width, screen_height;
	int        x, y;

	if (toplevel->priv->expand) {
		toplevel->priv->floating = FALSE;
		return;
	}

	screen = panel_toplevel_get_screen_geometry (
			toplevel, &screen_width, &screen_height);

	x = panel_multiscreen_x (screen, toplevel->priv->monitor) + toplevel->priv->x;
	y = panel_multiscreen_y (screen, toplevel->priv->monitor) + toplevel->priv->y;

	if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK)
		toplevel->priv->floating =
			(y > 0) && (y < (screen_height - toplevel->priv->geometry.height));
	else
		toplevel->priv->floating =
			(x > 0) && (x < (screen_width - toplevel->priv->geometry.width));
}

static gboolean
panel_toplevel_hide_button_event (PanelToplevel  *toplevel,
				  GdkEventButton *event,
				  GtkButton      *button)
{
	if (event->button == 1)
		return FALSE;

	return gtk_widget_event (GTK_WIDGET (toplevel), (GdkEvent *) event);
}

static void
panel_toplevel_hide_button_clicked (PanelToplevel *toplevel,
				    GtkButton     *button)
{
	GtkArrowType arrow_type;

	if (toplevel->priv->animating ||
	    toplevel->priv->state == PANEL_STATE_AUTO_HIDDEN)
		return;

	arrow_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "arrow-type"));

	if (toplevel->priv->state == PANEL_STATE_NORMAL) {
		GtkDirectionType direction = -1;

		switch (arrow_type) {
		case GTK_ARROW_UP:
			direction = GTK_DIR_UP;
			break;
		case GTK_ARROW_DOWN:
			direction = GTK_DIR_DOWN;
			break;
		case GTK_ARROW_LEFT:
			direction = GTK_DIR_LEFT;
			break;
		case GTK_ARROW_RIGHT:
			direction = GTK_DIR_RIGHT;
			break;
		default:
			g_assert_not_reached ();
			break;
		}

		panel_toplevel_hide (toplevel, FALSE, direction);
	} else
		panel_toplevel_unhide (toplevel);
}

static GtkWidget *
panel_toplevel_add_hide_button (PanelToplevel *toplevel,
				GtkArrowType   arrow_type,
				int            left_attach,
				int            right_attach,
				int            top_attach,
				int            bottom_attach)
{
	GtkWidget *button;
	GtkWidget *arrow;
	
	button = gtk_button_new ();
	GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_DEFAULT);

	switch (arrow_type) {
	case GTK_ARROW_UP:
		gtk_widget_set_size_request (button, -1, HIDE_BUTTON_SIZE);
		break;
	case GTK_ARROW_DOWN:
		gtk_widget_set_size_request (button, -1, HIDE_BUTTON_SIZE);
		break;
	case GTK_ARROW_LEFT:
		gtk_widget_set_size_request (button, HIDE_BUTTON_SIZE, -1);
		break;
	case GTK_ARROW_RIGHT:
		gtk_widget_set_size_request (button, HIDE_BUTTON_SIZE, -1);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	arrow = gtk_arrow_new (arrow_type, GTK_SHADOW_NONE);
	gtk_misc_set_padding (GTK_MISC (arrow), 0, 0);
	gtk_container_add (GTK_CONTAINER (button), arrow);
	gtk_widget_show (arrow);

	g_object_set_data (G_OBJECT (button),
			   "gnome_disable_sound_events",
			   GINT_TO_POINTER  (TRUE));
	g_object_set_data (G_OBJECT (button),
			   "arrow-type",
			   GINT_TO_POINTER (arrow_type));

	g_signal_connect_swapped (button, "clicked",
				  G_CALLBACK (panel_toplevel_hide_button_clicked), toplevel);
	g_signal_connect_swapped (button, "button_press_event",
				  G_CALLBACK (panel_toplevel_hide_button_event), toplevel);
	g_signal_connect_swapped (button, "button_release_event",
				  G_CALLBACK (panel_toplevel_hide_button_event), toplevel);
				  
	gtk_table_attach (GTK_TABLE (toplevel->priv->table),
			  button,
			  left_attach,
			  right_attach,
			  top_attach,
			  bottom_attach,
			  GTK_FILL,
			  GTK_FILL,
			  0,
			  0);

	return button;
}

static void
panel_toplevel_update_buttons_showing (PanelToplevel *toplevel)
{
	if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK) {
		gtk_widget_hide (toplevel->priv->hide_button_top);
		gtk_widget_hide (toplevel->priv->hide_button_bottom);
		gtk_widget_show (toplevel->priv->hide_button_left);
		gtk_widget_show (toplevel->priv->hide_button_right);
	} else {
		gtk_widget_show (toplevel->priv->hide_button_top);
		gtk_widget_show (toplevel->priv->hide_button_bottom);
		gtk_widget_hide (toplevel->priv->hide_button_left);
		gtk_widget_hide (toplevel->priv->hide_button_right);
	}

	if (!toplevel->priv->attached)
		return;

	switch (panel_toplevel_get_orientation (toplevel->priv->attach_toplevel)) {
	case PANEL_ORIENTATION_TOP:
		gtk_widget_hide (toplevel->priv->hide_button_top);
		break;
	case PANEL_ORIENTATION_BOTTOM:
		gtk_widget_hide (toplevel->priv->hide_button_bottom);
		break;
	case PANEL_ORIENTATION_LEFT:
		gtk_widget_hide (toplevel->priv->hide_button_left);
		break;
	case PANEL_ORIENTATION_RIGHT:
		gtk_widget_hide (toplevel->priv->hide_button_right);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
panel_toplevel_update_hide_buttons (PanelToplevel *toplevel)
{
	if (toplevel->priv->buttons_enabled)
		panel_toplevel_update_buttons_showing (toplevel);
	else {
		gtk_widget_hide (toplevel->priv->hide_button_top);
		gtk_widget_hide (toplevel->priv->hide_button_bottom);
		gtk_widget_hide (toplevel->priv->hide_button_left);
		gtk_widget_hide (toplevel->priv->hide_button_right);
	}

	if (toplevel->priv->arrows_enabled) {
		gtk_widget_show (GTK_BIN (toplevel->priv->hide_button_top)->child);
		gtk_widget_show (GTK_BIN (toplevel->priv->hide_button_bottom)->child);
		gtk_widget_show (GTK_BIN (toplevel->priv->hide_button_left)->child);
		gtk_widget_show (GTK_BIN (toplevel->priv->hide_button_right)->child);

		gtk_widget_set_size_request (toplevel->priv->hide_button_top,
					     -1, HIDE_BUTTON_SIZE);
		gtk_widget_set_size_request (toplevel->priv->hide_button_top,
					     -1, HIDE_BUTTON_SIZE);
		gtk_widget_set_size_request (toplevel->priv->hide_button_top,
					     HIDE_BUTTON_SIZE, -1);
		gtk_widget_set_size_request (toplevel->priv->hide_button_top,
					     HIDE_BUTTON_SIZE, -1);
	} else {
		gtk_widget_hide (GTK_BIN (toplevel->priv->hide_button_top)->child);
		gtk_widget_hide (GTK_BIN (toplevel->priv->hide_button_bottom)->child);
		gtk_widget_hide (GTK_BIN (toplevel->priv->hide_button_left)->child);
		gtk_widget_hide (GTK_BIN (toplevel->priv->hide_button_right)->child);

		gtk_widget_set_size_request (toplevel->priv->hide_button_top,    -1, -1);
		gtk_widget_set_size_request (toplevel->priv->hide_button_bottom, -1, -1);
		gtk_widget_set_size_request (toplevel->priv->hide_button_left,   -1, -1);
		gtk_widget_set_size_request (toplevel->priv->hide_button_right,  -1, -1);
	}
}

static gboolean
panel_toplevel_contains_pointer (PanelToplevel *toplevel)
{
	GtkWidget *widget;
	int        x = -1, y = -1;

	widget = GTK_WIDGET (toplevel);

	if (!GTK_WIDGET_REALIZED (widget))
		return FALSE;

	gtk_widget_get_pointer (widget, &x, &y);

	if (x == -1 || y == -1)
		return FALSE;

	if (x < 0 || x >= widget->allocation.width ||
	    y < 0 || y >= widget->allocation.height)
		return FALSE;

	return TRUE;
}

static void
panel_toplevel_update_struts (PanelToplevel *toplevel)
{
	GtkWidget        *widget;
	PanelOrientation  orientation;
	int               left = 0, right = 0, bottom = 0, top = 0;
	int               screen_width, screen_height;
	int               width, height;

	widget = GTK_WIDGET (toplevel);

	if (!GTK_WIDGET_REALIZED (widget))
		return;

	if (toplevel->priv->attached) {
		panel_xutils_set_strut (widget->window, 0, 0, 0, 0);
		return;
	}

	panel_toplevel_get_screen_geometry (
			toplevel, &screen_width, &screen_height);

	width  = toplevel->priv->geometry.width;
	height = toplevel->priv->geometry.height;

	orientation = toplevel->priv->orientation;

	if (orientation & PANEL_HORIZONTAL_MASK) {
		if (toplevel->priv->geometry.y <= 0) {
			orientation = PANEL_ORIENTATION_TOP;
			top = height;
		} else if (toplevel->priv->geometry.y >= (screen_height - height)) {
			orientation = PANEL_ORIENTATION_BOTTOM;
			bottom = height;
		}
	} else {
		if (toplevel->priv->geometry.x <= 0) {
			orientation = PANEL_ORIENTATION_LEFT;
			left = width;
		} else if (toplevel->priv->geometry.x >= (screen_width - width)) {
			orientation = PANEL_ORIENTATION_RIGHT;
			right = width;
		}
	}

	if (orientation != toplevel->priv->orientation) {
		toplevel->priv->orientation = orientation;
		g_object_notify (G_OBJECT (toplevel), "orientation");
	}

	if (toplevel->priv->auto_hide) {
		g_assert (toplevel->priv->auto_hide_size > 0);

		top    = CLAMP (top,    0, toplevel->priv->auto_hide_size);
		bottom = CLAMP (bottom, 0, toplevel->priv->auto_hide_size);
		left   = CLAMP (left,   0, toplevel->priv->auto_hide_size);
		right  = CLAMP (right,  0, toplevel->priv->auto_hide_size);
	}

	panel_xutils_set_strut (widget->window, left, right, bottom, top);
}

static void
panel_toplevel_update_edges (PanelToplevel *toplevel)
{
	GtkWidget      *widget;
	PanelFrameEdge  edges;
	PanelFrameEdge  inner_edges;
	PanelFrameEdge  outer_edges;
	int             monitor_width, monitor_height;
	int             width, height;
	gboolean        inner_frame = FALSE;

	widget = GTK_WIDGET (toplevel);

	panel_toplevel_get_monitor_geometry (
			toplevel, &monitor_width, &monitor_height);

	width  = toplevel->priv->geometry.width;
	height = toplevel->priv->geometry.height;

	edges = PANEL_EDGE_NONE;

	if (toplevel->priv->geometry.y != 0)
		edges |= PANEL_EDGE_TOP;

	if (toplevel->priv->geometry.x != 0)
		edges |= PANEL_EDGE_LEFT;

	if (toplevel->priv->geometry.y != (monitor_height - height))
		edges |= PANEL_EDGE_BOTTOM;

	if (toplevel->priv->geometry.x != (monitor_width - width))
		edges |= PANEL_EDGE_RIGHT;

	if (GTK_WIDGET_VISIBLE (toplevel->priv->hide_button_left) ||
	    GTK_WIDGET_VISIBLE (toplevel->priv->hide_button_right)) {
		inner_frame = TRUE;
		edges |= PANEL_EDGE_LEFT | PANEL_EDGE_RIGHT;
	}

	if (GTK_WIDGET_VISIBLE (toplevel->priv->hide_button_top) ||
	    GTK_WIDGET_VISIBLE (toplevel->priv->hide_button_bottom)) {
		inner_frame = TRUE;
		edges |= PANEL_EDGE_TOP | PANEL_EDGE_BOTTOM;
	}

	if (!inner_frame) {
		inner_edges = PANEL_EDGE_NONE;
		outer_edges = edges;
	} else {
		inner_edges = edges;
		outer_edges = PANEL_EDGE_NONE;
	}

	panel_frame_set_edges (toplevel->priv->inner_frame, inner_edges);

	if (toplevel->priv->edges != outer_edges) {
		toplevel->priv->edges = outer_edges;
		gtk_widget_queue_resize (widget);
	}
}

static void
panel_toplevel_update_description (PanelToplevel *toplevel)
{
	char *orientation = NULL;
	char *type;
	char  description [255];

	switch (toplevel->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
		orientation = _("Top");
		break;
	case PANEL_ORIENTATION_BOTTOM:
		orientation = _("Bottom");
		break;
	case PANEL_ORIENTATION_LEFT:
		orientation = _("Left");
		break;
	case PANEL_ORIENTATION_RIGHT:
		orientation = _("Right");
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	if (toplevel->priv->floating)
		type = _("Floating");
	else if (toplevel->priv->x_centered ||
		 toplevel->priv->y_centered)
		type = _("Centered");
	else if (toplevel->priv->expand)
		type = _("Expanded Edge");
	else
		type = _("Edge");
	
	/* Translators: the first argument is the current orientation
	 * (Top, Bottom, Left or Right) and the second argument is the
	 * type of panel (Expanded Edge, Edge, Floating or Centered).
	 */
	g_snprintf (description, sizeof (description), _("%s %s Panel"), orientation, type);

	if (toplevel->priv->description &&
	    !strcmp (toplevel->priv->description, description))
		return;

	if (toplevel->priv->description)
		g_free (toplevel->priv->description);
	toplevel->priv->description = g_strdup (description);

	if (!toplevel->priv->name)
		gtk_window_set_title (GTK_WINDOW (toplevel),
				      toplevel->priv->description);

	panel_a11y_set_atk_name_desc (
		GTK_WIDGET (toplevel->priv->panel_widget),
		toplevel->priv->name ? toplevel->priv->name :
				       toplevel->priv->description,
		toplevel->priv->description);
}

static void
panel_toplevel_update_auto_hide_position (PanelToplevel *toplevel,
					  int           *x,
					  int           *y)
{
	GdkScreen *screen;
	int        width, height;
	int        screen_width, screen_height;
	int        monitor_width, monitor_height;
	int        monitor_x, monitor_y;
	int        auto_hide_size;

	g_assert (x != NULL && y != NULL);

	if (toplevel->priv->floating)
		return;

	screen = panel_toplevel_get_screen_geometry (
			toplevel, &screen_width, &screen_height);

	panel_toplevel_get_monitor_geometry (
			toplevel, &monitor_width, &monitor_height);

	monitor_x = panel_multiscreen_x (screen, toplevel->priv->monitor);
	monitor_y = panel_multiscreen_y (screen, toplevel->priv->monitor);

	width  = toplevel->priv->geometry.width;
	height = toplevel->priv->geometry.height;

	if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK)
		auto_hide_size = CLAMP (toplevel->priv->auto_hide_size,
					1, height / 2);
	else
		auto_hide_size = CLAMP (toplevel->priv->auto_hide_size,
					1, width / 2);

	/* paranoia */
	if (auto_hide_size <= 0)
		auto_hide_size = DEFAULT_AUTO_HIDE_SIZE;

	switch (toplevel->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
		if (monitor_x + *x + width >= screen_width)
			*x = monitor_width - auto_hide_size;
		else if (monitor_x + *x <= 0)
			*x = - (width  - auto_hide_size);
		*y = - (height - auto_hide_size);
		break;
	case PANEL_ORIENTATION_BOTTOM:
		if (monitor_x + *x <= 0)
			*x = - (width  - auto_hide_size);
		else if (monitor_x + *x + width >= screen_width)
			*x = monitor_width - auto_hide_size;
		*y = monitor_height - auto_hide_size;
		break;
	case PANEL_ORIENTATION_LEFT:
		*x = - (width - auto_hide_size);
		if (monitor_y + *y + height >= screen_height)
			*y = monitor_height - auto_hide_size;
		else if (monitor_y + *y <= 0)
			*y = - (height - auto_hide_size);
		break;
	case PANEL_ORIENTATION_RIGHT:
		*x = monitor_width - auto_hide_size;
		if (monitor_y + *y <= 0)
			*y = - (height - auto_hide_size);
		else if (monitor_y + *y + height >= screen_height)
			*y = monitor_height - auto_hide_size;
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
panel_toplevel_update_attached_position (PanelToplevel *toplevel,
					 gboolean       hidden,
					 int           *x,
					 int           *y)
{
	PanelOrientation  attach_orientation;
	GdkRectangle      toplevel_box;
	GdkRectangle      parent_box;
	GdkRectangle      attach_box;
	GdkScreen        *screen;
	int               x_origin = 0, y_origin = 0;

	if (!GTK_WIDGET_REALIZED (toplevel->priv->attach_toplevel) ||
	    !GTK_WIDGET_REALIZED (toplevel->priv->attach_widget))
		return;

	toplevel_box = toplevel->priv->geometry;
	parent_box   = toplevel->priv->attach_toplevel->priv->geometry;
	attach_box   = GTK_WIDGET (toplevel->priv->attach_widget)->allocation;

	gdk_window_get_origin (GTK_WIDGET (toplevel->priv->attach_widget)->window,
			       &x_origin, &y_origin);

	attach_box.x += x_origin;
	attach_box.y += y_origin;

	attach_orientation = panel_toplevel_get_orientation (
					toplevel->priv->attach_toplevel);

	if (attach_orientation & PANEL_HORIZONTAL_MASK)
		*x = (attach_box.x + attach_box.width / 2)  - toplevel_box.width  / 2;
	else
		*y = (attach_box.y + attach_box.height / 2) - toplevel_box.height / 2;

	switch (attach_orientation) {
	case PANEL_ORIENTATION_TOP:
		*y = parent_box.y + parent_box.height;
		if (hidden)
			*y -= toplevel_box.height;
		break;
	case PANEL_ORIENTATION_BOTTOM:
		*y = parent_box.y;
		if (!hidden)
			*y -= toplevel_box.height;
		break;
	case PANEL_ORIENTATION_LEFT:
		*x = parent_box.x + parent_box.width;
		if (hidden)
			*x -= toplevel_box.width;
		break;
	case PANEL_ORIENTATION_RIGHT:
		*x = parent_box.x;
		if (!hidden)
			*x -= toplevel_box.width;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));

	*x -= panel_multiscreen_x (screen, toplevel->priv->monitor);
	*y -= panel_multiscreen_y (screen, toplevel->priv->monitor);
}

/* FIXME: this is wrong for Xinerama. In the Xinerama case
 *        I think if hiding it requires it to go onto the
 *        next monitor then it should just move it on to
 *        the next monitor and set its state back to normal
 */
static void
panel_toplevel_update_hidden_position (PanelToplevel *toplevel,
				       int           *x,
				       int           *y)
{
	int width, height;
	int auto_hide_size;
	int monitor_height, monitor_width;

	g_assert (x != NULL && y != NULL);

	g_assert (toplevel->priv->state == PANEL_STATE_HIDDEN_UP   ||
		  toplevel->priv->state == PANEL_STATE_HIDDEN_DOWN ||
		  toplevel->priv->state == PANEL_STATE_HIDDEN_LEFT ||
		  toplevel->priv->state == PANEL_STATE_HIDDEN_RIGHT);

	if (toplevel->priv->attached) {
		panel_toplevel_update_attached_position (toplevel, TRUE, x, y);
		return;
	}

	panel_toplevel_get_monitor_geometry (
			toplevel, &monitor_width, &monitor_height);

	width  = toplevel->priv->geometry.width;
	height = toplevel->priv->geometry.height;

	auto_hide_size = toplevel->priv->auto_hide_size;
	if (auto_hide_size <= 0)
		auto_hide_size = DEFAULT_AUTO_HIDE_SIZE;

	switch (toplevel->priv->state) {
	case PANEL_STATE_HIDDEN_UP:
		*y = - (height - MAX (toplevel->priv->hide_button_top->allocation.height,
				      auto_hide_size));
		break;
	case PANEL_STATE_HIDDEN_DOWN:
		*y = monitor_height -
			MAX (toplevel->priv->hide_button_bottom->allocation.height,
			     auto_hide_size);
		break;
	case PANEL_STATE_HIDDEN_LEFT:
		*x = - (width - MAX (toplevel->priv->hide_button_left->allocation.width,
				    auto_hide_size));
		break;
	case PANEL_STATE_HIDDEN_RIGHT:
		*x = monitor_width -
			MAX (toplevel->priv->hide_button_right->allocation.width,
			     auto_hide_size);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

/* Copied directly from the original panel code.
 * Its supposed to give a sine-like movement.
 * _v_ likes math way too much for his own good :-)
 */

#include <math.h>

static int
move_step (int  src,
	   int  dest,
	   long start_time,
	   long end_time,
	   long cur_time)
{
	double n, d, percentage;

	n = cur_time - start_time;
	d = end_time - start_time;

	if (abs (src - dest) <= 1 || n >= d)
		return dest;

	percentage = sin (M_PI * (n / d) - M_PI / 2) / 2 + 0.5;
	percentage = sin (M_PI * percentage - M_PI / 2) / 2 + 0.5;

	percentage = CLAMP (percentage, 0.0, 1.0);

	return src + ((dest - src) * percentage);
}

static void
panel_toplevel_update_animating_position (PanelToplevel *toplevel,
					  int           *x,
					  int           *y)
{
	GdkScreen *screen;
	GTimeVal  time_val;
	long      current_time;

	g_assert (x != NULL && y != NULL);

	g_get_current_time (&time_val);

	current_time =  (time_val.tv_sec - toplevel->priv->animation_start_time.tv_sec)
					* 1000000 + time_val.tv_usec;


	gdk_window_get_origin (GTK_WIDGET (toplevel)->window, x, y);

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));

	*x -= panel_multiscreen_x (screen, toplevel->priv->monitor);
	*y -= panel_multiscreen_y (screen, toplevel->priv->monitor);

	*x = move_step (*x,
			toplevel->priv->animation_end_x,
			toplevel->priv->animation_start_time.tv_usec,
			toplevel->priv->animation_end_time,
			current_time);

	*y = move_step (*y,
			toplevel->priv->animation_end_y,
			toplevel->priv->animation_start_time.tv_usec,
			toplevel->priv->animation_end_time,
			current_time);

	if (*x == toplevel->priv->animation_end_x &&
	    *y == toplevel->priv->animation_end_y) {
		toplevel->priv->animating = FALSE;

		if (toplevel->priv->state == PANEL_STATE_NORMAL)
			g_signal_emit (toplevel, toplevel_signals [UNHIDE_SIGNAL], 0);
	}
}

static void
panel_toplevel_update_normal_position (PanelToplevel *toplevel,
				       int           *x,
				       int           *y)
{
	GtkWidget *widget;
	int        monitor_width, monitor_height;

	g_assert (x != NULL && y != NULL);

	if (toplevel->priv->attached) {
		panel_toplevel_update_attached_position (toplevel, FALSE, x, y);
		return;
	}

	widget = GTK_WIDGET (toplevel);

	panel_toplevel_get_monitor_geometry (
			toplevel, &monitor_width, &monitor_height);

	*x = CLAMP (*x, 0, monitor_width  - toplevel->priv->geometry.width);
	*y = CLAMP (*y, 0, monitor_height - toplevel->priv->geometry.height);
}

static void
panel_toplevel_update_expanded_position (PanelToplevel *toplevel)
{
	GtkWidget *widget;
	GdkScreen *screen;
	int        monitor_width, monitor_height;
	int        screen_width, screen_height;
	int        monitor_x, monitor_y;
	int        x, y;
	int        monitor;

	if (!toplevel->priv->expand)
		return;

	widget = GTK_WIDGET (toplevel);

	screen = panel_toplevel_get_screen_geometry (
			toplevel, &screen_width, &screen_height);

	panel_toplevel_get_monitor_geometry (
		toplevel, &monitor_width, &monitor_height);

	monitor_x = panel_multiscreen_x (screen, toplevel->priv->monitor);
	monitor_y = panel_multiscreen_y (screen, toplevel->priv->monitor);

	x = monitor_x + toplevel->priv->x;
	y = monitor_y + toplevel->priv->y;

	switch (toplevel->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
		x = monitor_x;
		if (toplevel->priv->state != PANEL_STATE_AUTO_HIDDEN)
			y = 0;
		break;
	case PANEL_ORIENTATION_LEFT:
		if (toplevel->priv->state != PANEL_STATE_AUTO_HIDDEN)
			x = 0;
		y = monitor_y;
		break;
	case PANEL_ORIENTATION_BOTTOM:
		x = monitor_x;
		if (toplevel->priv->state != PANEL_STATE_AUTO_HIDDEN)
			y = screen_height - toplevel->priv->geometry.height;
		break;
	case PANEL_ORIENTATION_RIGHT:
		if (toplevel->priv->state != PANEL_STATE_AUTO_HIDDEN)
			x = screen_width - toplevel->priv->geometry.width;
		y = monitor_y;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	monitor = panel_multiscreen_locate_coords (screen, x, y);

	panel_toplevel_set_monitor (toplevel, monitor);

	x -= panel_multiscreen_x (screen, monitor);
	y -= panel_multiscreen_y (screen, monitor);

	g_object_freeze_notify (G_OBJECT (toplevel));

	if (toplevel->priv->x != x) {
		toplevel->priv->x = x;
		g_object_notify (G_OBJECT (toplevel), "x");
	}

	if (toplevel->priv->y != y) {
		toplevel->priv->y = y;
		g_object_notify (G_OBJECT (toplevel), "y");
	}

	g_object_thaw_notify (G_OBJECT (toplevel));
}

static void
panel_toplevel_update_position (PanelToplevel *toplevel)
{
	GtkWidget *widget;
	GdkScreen *screen;
	int        x, y;
	int        screen_width, screen_height;
	int        monitor_width, monitor_height;

	widget = GTK_WIDGET (toplevel);

	screen = panel_toplevel_get_screen_geometry (
			toplevel, &screen_width, &screen_height);

	panel_toplevel_get_monitor_geometry (
			toplevel, &monitor_width, &monitor_height);

	if (toplevel->priv->position_centered) {
		toplevel->priv->position_centered = FALSE;

		g_object_freeze_notify (G_OBJECT (toplevel));

		if (!toplevel->priv->x_centered) {
			toplevel->priv->x -= toplevel->priv->geometry.width  / 2;
			g_object_notify (G_OBJECT (toplevel), "x");
		}

		if (!toplevel->priv->y_centered) {
			toplevel->priv->y -= toplevel->priv->geometry.height / 2;
			g_object_notify (G_OBJECT (toplevel), "y");
		}

		g_object_thaw_notify (G_OBJECT (toplevel));
	}

	panel_toplevel_update_expanded_position (toplevel);
	panel_toplevel_calc_floating (toplevel);

	x = toplevel->priv->x;
	y = toplevel->priv->y;

	if (!toplevel->priv->expand) {
		if (toplevel->priv->x_centered)
			x = (monitor_width - toplevel->priv->geometry.width) / 2;
		if (toplevel->priv->y_centered)
			y = (monitor_height - toplevel->priv->geometry.height) / 2;
	}

	if (toplevel->priv->animating)
		panel_toplevel_update_animating_position (toplevel, &x, &y);

	else if (toplevel->priv->state == PANEL_STATE_NORMAL)
		panel_toplevel_update_normal_position (toplevel, &x, &y);

	else if (toplevel->priv->state == PANEL_STATE_AUTO_HIDDEN)
		panel_toplevel_update_auto_hide_position (toplevel, &x, &y);

	else 
		panel_toplevel_update_hidden_position (toplevel, &x, &y);

	x += panel_multiscreen_x (screen, toplevel->priv->monitor);
	y += panel_multiscreen_y (screen, toplevel->priv->monitor);

	toplevel->priv->geometry.x = x;
	toplevel->priv->geometry.y = y;

	panel_toplevel_update_struts (toplevel);
	panel_toplevel_update_edges (toplevel);
	panel_toplevel_update_description (toplevel);
}

static void
panel_toplevel_update_size (PanelToplevel  *toplevel,
			    GtkRequisition *requisition)
{
	GtkWidget *widget;
	int        monitor_width, monitor_height;
	int        width, height;

	widget = GTK_WIDGET (toplevel);

	panel_toplevel_get_monitor_geometry (
			toplevel, &monitor_width, &monitor_height);

	width  = requisition->width;
	height = requisition->height;

	if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK) {
		height = MAX (requisition->height, toplevel->priv->size);

		if (toplevel->priv->expand)
			width  = monitor_width;
		else if (!toplevel->priv->buttons_enabled)
			width = MAX (MINIMUM_WIDTH, width + 2 * HANDLE_SIZE);
		else
			width  = MAX (MINIMUM_WIDTH, width);
	} else {
		width = MAX (requisition->width, toplevel->priv->size);

		if (toplevel->priv->expand)
			height = monitor_height;
		else if (!toplevel->priv->buttons_enabled)
			height = MAX (MINIMUM_WIDTH, height + 2 * HANDLE_SIZE);
		else
			height = MAX (MINIMUM_WIDTH, height);
	}

	width  += 2 * widget->style->xthickness;
	height += 2 * widget->style->ythickness;

	toplevel->priv->geometry.width  = CLAMP (width,  0, monitor_width);
	toplevel->priv->geometry.height = CLAMP (height, 0, monitor_height);
}

static void
panel_toplevel_update_geometry (PanelToplevel  *toplevel,
				GtkRequisition *requisition)
{
	panel_toplevel_update_size (toplevel, requisition);
	panel_toplevel_update_position (toplevel);
}

static void
panel_toplevel_attach_widget_destroyed (PanelToplevel *toplevel)
{
	toplevel->priv->attached = FALSE;

	toplevel->priv->attach_toplevel = NULL;
	toplevel->priv->attach_widget   = NULL;

	gtk_widget_queue_resize (GTK_WIDGET (toplevel));
}

static gboolean
panel_toplevel_attach_widget_configure (PanelToplevel *toplevel)
{
	gtk_widget_queue_resize (GTK_WIDGET (toplevel));

	return FALSE;
}

static void
panel_toplevel_update_attach_orientation (PanelToplevel *toplevel)
{
	PanelOrientation attach_orientation;
	PanelOrientation orientation;

	attach_orientation =
		panel_toplevel_get_orientation (toplevel->priv->attach_toplevel);

	orientation = toplevel->priv->orientation;

	switch (attach_orientation) {
	case PANEL_ORIENTATION_TOP:
		orientation = PANEL_ORIENTATION_LEFT;
		break;
	case PANEL_ORIENTATION_BOTTOM:
		orientation = PANEL_ORIENTATION_RIGHT;
		break;
	case PANEL_ORIENTATION_LEFT:
		orientation = PANEL_ORIENTATION_TOP;
		break;
	case PANEL_ORIENTATION_RIGHT:
		orientation = PANEL_ORIENTATION_BOTTOM;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	panel_toplevel_set_orientation (toplevel, orientation);
}

static void
panel_toplevel_attach_toplevel_hiding (PanelToplevel *toplevel)
{
	panel_toplevel_hide (toplevel, FALSE, -1);

	toplevel->priv->attach_hidden = TRUE;
}

static void
panel_toplevel_attach_toplevel_unhiding (PanelToplevel *toplevel)
{
	if (!toplevel->priv->attach_hidden)
		return;

	toplevel->priv->attach_hidden = FALSE;

	panel_toplevel_unhide (toplevel);
}

static void
panel_toplevel_reverse_arrow (PanelToplevel *toplevel,
			      GtkWidget     *button)
{
	GtkArrowType arrow_type;

	arrow_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "arrow-type"));

	switch (arrow_type) {
	case GTK_ARROW_UP:
		arrow_type = GTK_ARROW_DOWN;
		break;
	case GTK_ARROW_DOWN:
		arrow_type = GTK_ARROW_UP;
		break;
	case GTK_ARROW_LEFT:
		arrow_type = GTK_ARROW_RIGHT;
		break;
	case GTK_ARROW_RIGHT:
		arrow_type = GTK_ARROW_LEFT;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	g_object_set_data (G_OBJECT (button), "arrow-type", GINT_TO_POINTER (arrow_type));

	gtk_arrow_set (GTK_ARROW (GTK_BIN (button)->child), arrow_type, GTK_SHADOW_NONE);
}

static void
panel_toplevel_reverse_arrows (PanelToplevel *toplevel)
{
	panel_toplevel_reverse_arrow (toplevel, toplevel->priv->hide_button_top);
	panel_toplevel_reverse_arrow (toplevel, toplevel->priv->hide_button_bottom);
	panel_toplevel_reverse_arrow (toplevel, toplevel->priv->hide_button_left);
	panel_toplevel_reverse_arrow (toplevel, toplevel->priv->hide_button_right);
}

static void
panel_toplevel_disconnect_attached (PanelToplevel *toplevel)
{
	int i;

	for (i = 0; i < N_ATTACH_TOPLEVEL_SIGNALS; i++)
		g_signal_handler_disconnect (
			toplevel->priv->attach_toplevel,
			toplevel->priv->attach_toplevel_signals [i]);

	for (i = 0; i < N_ATTACH_WIDGET_SIGNALS; i++)
		g_signal_handler_disconnect (
			toplevel->priv->attach_widget,
			toplevel->priv->attach_widget_signals [i]);

	panel_toplevel_reverse_arrows (toplevel);
}

static void
panel_toplevel_connect_attached (PanelToplevel *toplevel)
{
	gulong *signals;
	int     i = 0;

	signals = toplevel->priv->attach_toplevel_signals;

	signals [i++] = g_signal_connect_swapped (
		toplevel->priv->attach_toplevel, "destroy",
		G_CALLBACK (panel_toplevel_attach_widget_destroyed), toplevel);
	signals [i++] = g_signal_connect_swapped (
		toplevel->priv->attach_toplevel, "notify::orientation",
		G_CALLBACK (panel_toplevel_update_attach_orientation), toplevel);
	signals [i++] = g_signal_connect_swapped (
		toplevel->priv->attach_toplevel, "configure-event",
		G_CALLBACK (panel_toplevel_attach_widget_configure), toplevel);
	signals [i++] = g_signal_connect_swapped (
		toplevel->priv->attach_toplevel, "hiding",
		G_CALLBACK (panel_toplevel_attach_toplevel_hiding), toplevel);
	signals [i++] = g_signal_connect_swapped (
		toplevel->priv->attach_toplevel, "unhiding",
		G_CALLBACK (panel_toplevel_attach_toplevel_unhiding), toplevel);

	g_assert (i == N_ATTACH_TOPLEVEL_SIGNALS);

	signals = toplevel->priv->attach_widget_signals;
	i = 0;

	signals [i++] = g_signal_connect_swapped (
		toplevel->priv->attach_widget, "destroy",
		G_CALLBACK (panel_toplevel_attach_widget_destroyed), toplevel);
	signals [i++] = g_signal_connect_swapped (
		toplevel->priv->attach_widget, "configure-event",
		G_CALLBACK (panel_toplevel_attach_widget_configure), toplevel);

	g_assert (i == N_ATTACH_WIDGET_SIGNALS);

	panel_toplevel_reverse_arrows (toplevel);
} 

void
panel_toplevel_attach_to_widget (PanelToplevel *toplevel,
				 PanelToplevel *attach_toplevel,
				 GtkWidget     *attach_widget)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));
	g_return_if_fail (PANEL_IS_TOPLEVEL (attach_toplevel));
	g_return_if_fail (GTK_IS_WIDGET (attach_widget));

	if (toplevel->priv->attached)
		panel_toplevel_disconnect_attached (toplevel);

	toplevel->priv->attached = TRUE;

	toplevel->priv->attach_toplevel = attach_toplevel;
	toplevel->priv->attach_widget   = attach_widget;

	panel_toplevel_connect_attached (toplevel);

	panel_toplevel_set_expand (toplevel, FALSE);
	panel_toplevel_update_attach_orientation (toplevel);

	gtk_widget_queue_resize (GTK_WIDGET (toplevel));
}

void
panel_toplevel_detach (PanelToplevel *toplevel)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (!toplevel->priv->attached)
		return;

	panel_toplevel_disconnect_attached (toplevel);

	toplevel->priv->attached = FALSE;

	toplevel->priv->attach_toplevel = NULL;
	toplevel->priv->attach_widget   = NULL;

	gtk_widget_queue_resize (GTK_WIDGET (toplevel));
}

gboolean
panel_toplevel_get_is_attached (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), FALSE);

	return toplevel->priv->attached;
}

static gboolean
panel_toplevel_popup_panel_menu (PanelToplevel *toplevel)
{
	gboolean retval = FALSE;

	g_signal_emit_by_name (toplevel, "popup_menu", &retval);

	return retval;
}

static void
panel_toplevel_realize (GtkWidget *widget)
{
	gtk_window_set_decorated (GTK_WINDOW (widget), FALSE);
	gtk_window_stick (GTK_WINDOW (widget));

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		GTK_WIDGET_CLASS (parent_class)->realize (widget);

	panel_toplevel_update_struts (PANEL_TOPLEVEL (widget));
	panel_xutils_set_window_type (widget->window, PANEL_XUTILS_TYPE_DOCK);
}

static void
panel_toplevel_disconnect_timeouts (PanelToplevel *toplevel)
{
	if (toplevel->priv->move_timeout)
		g_source_remove (toplevel->priv->move_timeout);
	toplevel->priv->move_timeout = 0;

	if (toplevel->priv->hide_timeout)
		g_source_remove (toplevel->priv->hide_timeout);
	toplevel->priv->hide_timeout = 0;

	if (toplevel->priv->unhide_timeout)
		g_source_remove (toplevel->priv->unhide_timeout);
	toplevel->priv->unhide_timeout = 0;

	if (toplevel->priv->animation_timeout)
		g_source_remove (toplevel->priv->animation_timeout);
	toplevel->priv->animation_timeout = 0;
}

static void
panel_toplevel_unrealize (GtkWidget *widget)
{
	panel_toplevel_disconnect_timeouts (PANEL_TOPLEVEL (widget));

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
panel_toplevel_destroy (GtkObject *widget)
{
	panel_toplevel_disconnect_timeouts (PANEL_TOPLEVEL (widget));

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (widget);
}

static void
panel_toplevel_check_resize (GtkContainer *container)
{
	GtkAllocation   allocation;
	GtkRequisition  requisition;
	GtkWidget      *widget;

	widget = GTK_WIDGET (container);

	if (!GTK_WIDGET_VISIBLE (widget))
		return;

	requisition.width  = -1;
	requisition.height = -1;

	gtk_widget_size_request (widget, &requisition);

	if (widget->allocation.width  != requisition.width ||
	    widget->allocation.height != requisition.height)
		return;

	allocation = widget->allocation;
	gtk_widget_size_allocate (widget, &allocation);
}

static void
panel_toplevel_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
	PanelToplevel *toplevel;
	GdkRectangle   old_geometry;
	int            position_changed = FALSE;
	int            size_changed = FALSE;

	toplevel = PANEL_TOPLEVEL (widget);

	if (GTK_WIDGET_CLASS (parent_class)->size_request)
		GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

	old_geometry = toplevel->priv->geometry;

	panel_toplevel_update_geometry (toplevel, requisition);

	requisition->width  = toplevel->priv->geometry.width;
	requisition->height = toplevel->priv->geometry.height;

	if (!GTK_WIDGET_REALIZED (widget))
		return;

	if (old_geometry.width  != toplevel->priv->geometry.width ||
	    old_geometry.height != toplevel->priv->geometry.height)
		size_changed = TRUE;

	if (old_geometry.x != toplevel->priv->geometry.x ||
	    old_geometry.y != toplevel->priv->geometry.y)
		position_changed = TRUE;

	if (size_changed && position_changed)
		gdk_window_move_resize (widget->window,
					toplevel->priv->geometry.x,
					toplevel->priv->geometry.y,
					toplevel->priv->geometry.width,
					toplevel->priv->geometry.height);
	else if (position_changed)
		gdk_window_move (widget->window,
				 toplevel->priv->geometry.x,
				 toplevel->priv->geometry.y);
	else if (size_changed)
		gdk_window_resize (widget->window,
				   toplevel->priv->geometry.width,
				   toplevel->priv->geometry.height);
}

static void
panel_toplevel_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
	PanelToplevel *toplevel = (PanelToplevel *) widget;
	GtkBin        *bin = (GtkBin *) widget;
	GtkAllocation  challoc;

	widget->allocation = *allocation;

	if (toplevel->priv->expand || toplevel->priv->buttons_enabled)
		challoc = *allocation;
	else {
		if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK) {
			challoc.x      = HANDLE_SIZE;
			challoc.y      = 0;
			challoc.width  = allocation->width - 2 * HANDLE_SIZE;
			challoc.height = allocation->height;
		} else {
			challoc.x      = 0;
			challoc.y      = HANDLE_SIZE;
			challoc.width  = allocation->width;
			challoc.height = allocation->height - 2 * HANDLE_SIZE;
		}
	}

	if (toplevel->priv->edges & PANEL_EDGE_TOP) {
		challoc.y += widget->style->ythickness;
		challoc.height -= widget->style->ythickness;
	}

	if (toplevel->priv->edges & PANEL_EDGE_LEFT) {
		challoc.x += widget->style->xthickness;
		challoc.width -= widget->style->xthickness;
	}

	if (toplevel->priv->edges & PANEL_EDGE_BOTTOM)
		challoc.height -= widget->style->ythickness;

	if (toplevel->priv->edges & PANEL_EDGE_RIGHT)
		challoc.width -= widget->style->xthickness;

	challoc.width  = MAX (1, challoc.width);
	challoc.height = MAX (1, challoc.height);

	if (GTK_WIDGET_MAPPED (widget) &&
	    (challoc.x != bin->child->allocation.x ||
	     challoc.y != bin->child->allocation.y ||
	     challoc.width  != bin->child->allocation.width ||
	     challoc.height != bin->child->allocation.height))
	 	gdk_window_invalidate_rect (widget->window, &widget->allocation, FALSE);

	if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
		gtk_widget_size_allocate (bin->child, &challoc);
}

static gboolean
panel_toplevel_expose (GtkWidget      *widget,
		       GdkEventExpose *event)
{
	PanelToplevel  *toplevel = (PanelToplevel *) widget;
	PanelFrameEdge  edges;
	gboolean        retval = FALSE;

	if (!GTK_WIDGET_DRAWABLE (widget))
		return retval;

	if (GTK_WIDGET_CLASS (parent_class)->expose_event)
		retval = GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);

	edges = toplevel->priv->edges;
	panel_frame_draw (widget, edges);

	if (toplevel->priv->expand || toplevel->priv->buttons_enabled)
		return retval;

	if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK) {
		int x, y, width, height;
		int xthickness, ythickness;

		x      = widget->allocation.x;
		y      = widget->allocation.y;
		width  = HANDLE_SIZE;
		height = widget->allocation.height;

		xthickness = widget->style->xthickness;
		ythickness = widget->style->ythickness;

		if (edges & PANEL_EDGE_TOP) {
			y += ythickness;
			height -= ythickness;
		}
		if (edges & PANEL_EDGE_BOTTOM)
			height -= ythickness;
		if (edges & PANEL_EDGE_LEFT)
			x += xthickness;

		gtk_paint_handle (widget->style, widget->window,
				  GTK_WIDGET_STATE (widget),
				  GTK_SHADOW_OUT,
				  &event->area, widget, "handlebox",
				  x, y, width, height,
				  GTK_ORIENTATION_VERTICAL);

		x = widget->allocation.width - HANDLE_SIZE;
		if (edges & PANEL_EDGE_RIGHT)
			x -= xthickness;

		gtk_paint_handle (widget->style, widget->window,
				  GTK_WIDGET_STATE (widget),
				  GTK_SHADOW_OUT,
				  &event->area, widget, "handlebox",
				  x, y, width, height,
				  GTK_ORIENTATION_VERTICAL);
	} else {
		int x, y, width, height;
		int xthickness, ythickness;

		x      = widget->allocation.x;
		y      = widget->allocation.y;
		width  = widget->allocation.width;
		height = HANDLE_SIZE;

		xthickness = widget->style->xthickness;
		ythickness = widget->style->ythickness;

		if (edges & PANEL_EDGE_LEFT) {
			x += xthickness;
			width -= xthickness;
		}
		if (edges & PANEL_EDGE_RIGHT)
			width -= xthickness;
		if (edges & PANEL_EDGE_TOP)
			y += ythickness;

		gtk_paint_handle (widget->style, widget->window,
				  GTK_WIDGET_STATE (widget),
				  GTK_SHADOW_OUT,
				  &event->area, widget, "handlebox",
				  x, y, width, height,
				  GTK_ORIENTATION_HORIZONTAL);

		y = widget->allocation.height - HANDLE_SIZE;
		if (edges & PANEL_EDGE_BOTTOM)
			y -= ythickness;

		gtk_paint_handle (widget->style, widget->window,
				  GTK_WIDGET_STATE (widget),
				  GTK_SHADOW_OUT,
				  &event->area, widget, "handlebox",
				  x, y, width, height,
				  GTK_ORIENTATION_HORIZONTAL);
	}

	return retval;
}

static PanelFrameEdge
panel_toplevel_get_resize_edge (PanelToplevel *toplevel,
				int            x,
				int            y)
{
	PanelFrameEdge edges;
	PanelFrameEdge retval;

	edges = toplevel->priv->edges;
	if (edges == PANEL_EDGE_NONE)
		edges = panel_frame_get_edges (toplevel->priv->inner_frame);

	retval = PANEL_EDGE_NONE;

	if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK) {
		if (edges & PANEL_EDGE_TOP && y <= RESIZE_GRAB_AREA_SIZE)
			retval = PANEL_EDGE_TOP;

		else if (edges & PANEL_EDGE_BOTTOM &&
			 toplevel->priv->geometry.height - y <= RESIZE_GRAB_AREA_SIZE)
			retval = PANEL_EDGE_BOTTOM;
	} else {
		if (edges & PANEL_EDGE_LEFT && x <= RESIZE_GRAB_AREA_SIZE)
			retval = PANEL_EDGE_LEFT;

		else if (edges & PANEL_EDGE_RIGHT &&
			 toplevel->priv->geometry.width - x <= RESIZE_GRAB_AREA_SIZE)
			retval = PANEL_EDGE_RIGHT;
	}

	return retval;
}

static GdkCursorType
panel_toplevel_edge_to_cursor (PanelFrameEdge edge)
{
	GdkCursorType retval = -1;

	switch (edge) {
	case PANEL_EDGE_TOP:
		retval = GDK_TOP_SIDE;
		break;
	case PANEL_EDGE_BOTTOM:
		retval = GDK_BOTTOM_SIDE;
		break;
	case PANEL_EDGE_LEFT:
		retval = GDK_LEFT_SIDE;
		break;
	case PANEL_EDGE_RIGHT:
		retval = GDK_RIGHT_SIDE;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return retval;
}

static gboolean
panel_toplevel_button_press_event (GtkWidget      *widget,
				   GdkEventButton *event)
{
	PanelToplevel  *toplevel;
	PanelFrameEdge  edge;
	GdkCursorType   cursor_type;
	GdkCursor      *cursor;

	g_return_val_if_fail (PANEL_IS_TOPLEVEL (widget), FALSE);

	toplevel = PANEL_TOPLEVEL (widget);

	if (event->button != 1 && event->button != 2)
		return FALSE;

	if (toplevel->priv->animating)
		return FALSE;

	gtk_grab_add (widget);

	toplevel->priv->in_drag = TRUE;

	cursor_type = GDK_FLEUR;

	edge = panel_toplevel_get_resize_edge (toplevel, event->x, event->y);
	if (edge != PANEL_EDGE_NONE) {
		cursor_type = panel_toplevel_edge_to_cursor (edge);
		g_assert (cursor_type != -1);

		toplevel->priv->drag_is_resize = TRUE;
		toplevel->priv->resize_edge    = edge;

		toplevel->priv->drag_offset_x = 0;
		toplevel->priv->drag_offset_y = 0;

		switch (toplevel->priv->resize_edge) {
		case PANEL_EDGE_BOTTOM:
			toplevel->priv->drag_offset_y = toplevel->priv->geometry.y;
			break;
		case PANEL_EDGE_TOP:
			toplevel->priv->drag_offset_y =
				toplevel->priv->geometry.y + toplevel->priv->geometry.height;
			break;
		case PANEL_EDGE_RIGHT:
			toplevel->priv->drag_offset_x = toplevel->priv->geometry.x;
			break;
		case PANEL_EDGE_LEFT:
			toplevel->priv->drag_offset_x =
				toplevel->priv->geometry.x + toplevel->priv->geometry.width;
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	} else
		gdk_window_get_pointer (widget->window,
					&toplevel->priv->drag_offset_x,
					&toplevel->priv->drag_offset_y,
					NULL);

	cursor = gdk_cursor_new (cursor_type);

	gdk_pointer_grab (widget->window, FALSE,
			  GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                          NULL, cursor, event->time);
        gdk_cursor_unref (cursor);

	return TRUE;
}

static gboolean
panel_toplevel_button_release_event (GtkWidget      *widget,
				     GdkEventButton *event)
{
	PanelToplevel *toplevel;

	if (event->button != 1 && event->button != 2)
		return FALSE;

	toplevel = PANEL_TOPLEVEL (widget);

	if (!toplevel->priv->in_drag)
		return FALSE;

	gtk_grab_remove (widget);
	gdk_pointer_ungrab (event->time);

	if (toplevel->priv->move_timeout)
		g_source_remove (toplevel->priv->move_timeout);
	toplevel->priv->move_timeout = 0;

	toplevel->priv->in_drag        = FALSE;
	toplevel->priv->drag_is_rotate = FALSE;
	toplevel->priv->drag_is_resize = FALSE;

	return TRUE;
}

static void
panel_toplevel_calc_new_orientation (PanelToplevel *toplevel,
				     int            pointer_x,
				     int            pointer_y)
{
	PanelOrientation  new_orientation;
	GdkScreen        *screen;
	int               screen_width, screen_height;
	int               hborder, vborder;
	int               monitor;

	screen = panel_toplevel_get_screen_geometry (
			toplevel, &screen_width, &screen_height);

	monitor = panel_multiscreen_locate_coords (screen, pointer_x, pointer_y);

	if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK)
		vborder = hborder = (3 * toplevel->priv->geometry.height) >> 1;
	else
		vborder = hborder = (3 * toplevel->priv->geometry.width)  >> 1;

	new_orientation = toplevel->priv->orientation;

	switch (toplevel->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
		if (pointer_y > (screen_height - hborder))
			new_orientation = PANEL_ORIENTATION_BOTTOM;

		else if (pointer_y > hborder) {
			if (pointer_x > (screen_width - vborder))
				new_orientation = PANEL_ORIENTATION_RIGHT;
			else if (pointer_x < vborder)
				new_orientation = PANEL_ORIENTATION_LEFT;
		} else
			panel_toplevel_set_monitor (toplevel, monitor);
		break;
	case PANEL_ORIENTATION_BOTTOM:
		if (pointer_y < hborder)
			new_orientation = PANEL_ORIENTATION_TOP;

		else if (pointer_y < (screen_height - hborder)) {
			if (pointer_x > (screen_width - vborder))
				new_orientation = PANEL_ORIENTATION_RIGHT;
			else if (pointer_x < vborder)
				new_orientation = PANEL_ORIENTATION_LEFT;
		} else
			panel_toplevel_set_monitor (toplevel, monitor);
		break;
	case PANEL_ORIENTATION_LEFT:
		if (pointer_x > (screen_width - vborder))
			new_orientation = PANEL_ORIENTATION_RIGHT;

		else if (pointer_x > vborder) {
			if (pointer_y > (screen_height - hborder))
				new_orientation = PANEL_ORIENTATION_BOTTOM;
			else if (pointer_y < hborder)
				new_orientation = PANEL_ORIENTATION_TOP;
		} else
			panel_toplevel_set_monitor (toplevel, monitor);
		break;
	case PANEL_ORIENTATION_RIGHT:
		if (pointer_x < vborder)
			new_orientation = PANEL_ORIENTATION_LEFT;

		else if (pointer_x < (screen_width - vborder)) {
			if (pointer_y > (screen_height - hborder))
				new_orientation = PANEL_ORIENTATION_BOTTOM;
			else if (pointer_y < hborder)
				new_orientation = PANEL_ORIENTATION_TOP;
		} else
			panel_toplevel_set_monitor (toplevel, monitor);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	panel_toplevel_set_orientation (toplevel, new_orientation);
}

static void
panel_toplevel_move_to_pointer (PanelToplevel *toplevel,
				int            pointer_x,
				int            pointer_y)
{
	PanelOrientation  new_orientation;
	GdkScreen        *screen;
	int               screen_width, screen_height;
	int               width, height;
	int               new_x, new_y;
	int               new_monitor;
	gboolean          x_centered = FALSE;
	gboolean          y_centered = FALSE;

	new_x = pointer_x - toplevel->priv->drag_offset_x;
	new_y = pointer_y - toplevel->priv->drag_offset_y;

	new_orientation = toplevel->priv->orientation;

	screen = panel_toplevel_get_screen_geometry (
			toplevel, &screen_width, &screen_height);

	width  = toplevel->priv->geometry.width;
	height = toplevel->priv->geometry.height;

	new_x = CLAMP (new_x, 0, screen_width  - width);
	new_y = CLAMP (new_y, 0, screen_height - height);

	if (new_x <= SNAP_TOLERANCE) {
		if (toplevel->priv->orientation & PANEL_VERTICAL_MASK)
			new_orientation = PANEL_ORIENTATION_LEFT;
		new_x = 0;
	} else if ((new_x + width) >= (screen_width - SNAP_TOLERANCE)) {
		if (toplevel->priv->orientation & PANEL_VERTICAL_MASK)
			new_orientation = PANEL_ORIENTATION_RIGHT;
		new_x = screen_width - width;
	}

	if (new_y <= SNAP_TOLERANCE) {
		if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK)
			new_orientation = PANEL_ORIENTATION_TOP;
		new_y = 0;
	} else if ((new_y + height) >= (screen_height - SNAP_TOLERANCE)) {
		if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK)
			new_orientation = PANEL_ORIENTATION_BOTTOM;
		new_y = screen_height - height;
	}

	if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK &&
	    (new_y == 0 || new_y == (screen_height - height))) {
		if (abs (new_x - ((screen_width - width) / 2)) <= SNAP_TOLERANCE) {
			new_x = 0;
			x_centered = TRUE;
		}
	} else if (toplevel->priv->orientation & PANEL_VERTICAL_MASK &&
		   (new_x == 0 || new_x == (screen_width - width))) {
		if (abs (new_y - ((screen_height - height) / 2)) <= SNAP_TOLERANCE) {
			new_y = 0;
			y_centered = TRUE;
		}
	}

	new_monitor = panel_multiscreen_locate_coords (screen, new_x, new_y);

	new_x -= panel_multiscreen_x (screen, new_monitor);
	new_y -= panel_multiscreen_y (screen, new_monitor);

	panel_toplevel_set_orientation (toplevel, new_orientation);
	panel_toplevel_set_monitor (toplevel, new_monitor);
	panel_toplevel_set_position (toplevel, new_x, x_centered, new_y, y_centered);
}

static void
panel_toplevel_rotate_to_pointer (PanelToplevel *toplevel,
				  int            pointer_x,
				  int            pointer_y)
{
	GtkWidget *widget;
	GdkScreen *screen;
	int        x_diff, y_diff;
	int        x, y;

	widget = GTK_WIDGET (toplevel);

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));

	x = toplevel->priv->geometry.x;
	y = toplevel->priv->geometry.y;

	x_diff = pointer_x - (x + toplevel->priv->geometry.width / 2);
	y_diff = pointer_y - (y + toplevel->priv->geometry.height / 2);

	if (((-y_diff > x_diff + SNAP_TOLERANCE) && x_diff > 0 && y_diff < 0) ||
	    (( y_diff < x_diff + SNAP_TOLERANCE) && x_diff < 0 && y_diff < 0))
		panel_toplevel_set_orientation (toplevel, PANEL_ORIENTATION_RIGHT);

	else if (((-x_diff < y_diff - SNAP_TOLERANCE) && x_diff > 0 && y_diff < 0) ||
	         (( x_diff > y_diff - SNAP_TOLERANCE) && x_diff > 0 && y_diff > 0))
		panel_toplevel_set_orientation (toplevel, PANEL_ORIENTATION_BOTTOM);

	else if ((( y_diff > x_diff + SNAP_TOLERANCE) && x_diff > 0 && y_diff > 0) ||
	         ((-y_diff < x_diff + SNAP_TOLERANCE) && x_diff < 0 && y_diff > 0))
		panel_toplevel_set_orientation (toplevel, PANEL_ORIENTATION_LEFT);

	else if (((-x_diff > y_diff - SNAP_TOLERANCE) && x_diff < 0 && y_diff > 0) ||
	         (( x_diff < y_diff - SNAP_TOLERANCE) && x_diff < 0 && y_diff < 0))
		panel_toplevel_set_orientation (toplevel, PANEL_ORIENTATION_TOP);
}

static gboolean
panel_toplevel_move_timeout_handler (GtkWidget *widget)
{
	PanelToplevel *toplevel;
	GdkWindow     *root_window;
	GdkScreen     *screen;
	int            pointer_x = -1, pointer_y = -1;

	g_return_val_if_fail (PANEL_IS_TOPLEVEL (widget), FALSE);

	if (!GTK_WIDGET_REALIZED (widget))
		return FALSE;

	toplevel = PANEL_TOPLEVEL (widget);

	toplevel->priv->move_timeout = 0;

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));
	root_window = gdk_screen_get_root_window (screen);
	gdk_window_get_pointer (root_window, &pointer_x, &pointer_y, NULL);

	if (toplevel->priv->expand)
		panel_toplevel_calc_new_orientation (toplevel, pointer_x, pointer_y);

	else if (!toplevel->priv->drag_is_rotate)
		panel_toplevel_move_to_pointer (toplevel, pointer_x, pointer_y);

	else
		panel_toplevel_rotate_to_pointer (toplevel, pointer_x, pointer_y);

	return FALSE;
}

static void
panel_toplevel_update_cursor_for_resize (PanelToplevel  *toplevel,
					 GdkEventMotion *event)
{
	PanelFrameEdge  resize_edge;
	GdkCursorType   cursor_type = -1;
	GdkCursor      *cursor;
	GtkWidget      *widget;

	widget = GTK_WIDGET (toplevel);

	resize_edge = panel_toplevel_get_resize_edge (toplevel, event->x, event->y);

	if (resize_edge == PANEL_EDGE_NONE) {
		gdk_window_set_cursor (widget->window, NULL);
		return;
	}

	cursor_type = panel_toplevel_edge_to_cursor (resize_edge);

	g_assert (cursor_type != -1);

	cursor = gdk_cursor_new (cursor_type);
	gdk_window_set_cursor (widget->window, cursor);
	gdk_cursor_unref (cursor);
}

static void
panel_toplevel_resize_to_pointer (PanelToplevel *toplevel,
				  int            x,
				  int            y)
{
	int new_size;
	int new_x, new_y;
	int new_x_centered, new_y_centered;
	int monitor_width, monitor_height;

	new_size       = toplevel->priv->size;
	new_x          = toplevel->priv->x;
	new_y          = toplevel->priv->y;
	new_x_centered = toplevel->priv->x_centered;
	new_y_centered = toplevel->priv->x_centered;

	panel_toplevel_get_monitor_geometry (toplevel, &monitor_width, &monitor_height);

	switch (toplevel->priv->resize_edge) {
	case PANEL_EDGE_TOP:
		new_size = toplevel->priv->drag_offset_y - y;
		new_size = CLAMP (new_size, 0, monitor_height / 4);
		new_y -= (new_size - toplevel->priv->size);
		break;
	case PANEL_EDGE_BOTTOM:
		new_size = y - toplevel->priv->drag_offset_y;
		new_size = CLAMP (new_size, 0, monitor_height / 4);
		break;
	case PANEL_EDGE_LEFT:
		new_size = toplevel->priv->drag_offset_x - x;
		new_size = CLAMP (new_size, 0, monitor_width / 4);
		new_x -= (new_size - toplevel->priv->size);
		break;
	case PANEL_EDGE_RIGHT:
		new_size = x - toplevel->priv->drag_offset_x;
		new_size = CLAMP (new_size, 0, monitor_width / 4);
		break;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	if (new_size == 0)
		return;

	panel_toplevel_set_position (
		toplevel, new_x, new_x_centered, new_y, new_y_centered);
	panel_toplevel_set_size (toplevel, new_size);
}

static gboolean
panel_toplevel_motion_notify_event (GtkWidget      *widget,
				    GdkEventMotion *event)
{
	PanelToplevel *toplevel;

	toplevel = PANEL_TOPLEVEL (widget);

	if (!toplevel->priv->in_drag) {
		panel_toplevel_update_cursor_for_resize (toplevel, event);
		
		return FALSE;
	}

	if (toplevel->priv->drag_is_resize) {
		panel_toplevel_resize_to_pointer (toplevel, event->x_root, event->y_root);
		return TRUE;
	}

	toplevel->priv->drag_is_rotate = (event->state & GDK_SHIFT_MASK);

	if (!toplevel->priv->move_timeout)
		toplevel->priv->move_timeout =
			g_timeout_add (
				50,
				(GSourceFunc) panel_toplevel_move_timeout_handler,
				widget);

	return TRUE;
}

static gboolean
panel_toplevel_animation_timeout (PanelToplevel *toplevel)
{
	gtk_widget_queue_resize (GTK_WIDGET (toplevel));

	if (!toplevel->priv->animating) {
		toplevel->priv->animation_end_x = 0xdead;
		toplevel->priv->animation_end_y = 0xdead;
		toplevel->priv->animation_start_time.tv_sec  = 0xdead;
		toplevel->priv->animation_start_time.tv_usec = 0xdead;
		toplevel->priv->animation_end_time = 0xdead;
		toplevel->priv->animation_timeout = 0;
	}

	return toplevel->priv->animating;
}

static long
panel_toplevel_get_animation_time (PanelToplevel *toplevel,
				   int            delta)
{
 /* The average number of miliseconds per pixel
  * to complete the animation.
  */
 /* FIXME: for auto_hide this should really be slower. Or
  *        maybe not ...
  */
#define ANIMATION_TIME_FAST    .4
#define ANIMATION_TIME_MEDIUM 1.2
#define ANIMATION_TIME_SLOW   2.0

	long t;

	t = toplevel->priv->animation_start_time.tv_usec;

	switch (toplevel->priv->animation_speed) {
	case PANEL_ANIMATION_SLOW:
		t += (abs (delta) * ANIMATION_TIME_SLOW * 1000);
		break;
	case PANEL_ANIMATION_MEDIUM:
		t += (abs (delta) * ANIMATION_TIME_MEDIUM * 1000);
		break;
	case PANEL_ANIMATION_FAST:
		t += (abs (delta) * ANIMATION_TIME_FAST * 1000);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return t;

#undef ANIMATION_TIME_FAST  
#undef ANIMATION_TIME_MEDIUM
#undef ANIMATION_TIME_SLOW
}

static void
panel_toplevel_start_animation (PanelToplevel *toplevel)
{
	GdkScreen *screen;
	int        monitor_width, monitor_height;
	int        deltax, deltay;
	int        cur_x = -1, cur_y = -1;
	long       t;

	screen = panel_toplevel_get_monitor_geometry (
				toplevel, &monitor_width, &monitor_height);

	toplevel->priv->animation_end_x = toplevel->priv->x;
	toplevel->priv->animation_end_y = toplevel->priv->y;

	if (!toplevel->priv->expand) {
		GtkWidget *widget;

		widget = GTK_WIDGET (toplevel);

		if (toplevel->priv->x_centered)
			toplevel->priv->animation_end_x =
				(monitor_width - toplevel->priv->geometry.width) / 2;
		if (toplevel->priv->y_centered)
			toplevel->priv->animation_end_y =
				(monitor_height - toplevel->priv->geometry.height) / 2;
	}

	if (toplevel->priv->state == PANEL_STATE_NORMAL)
		panel_toplevel_update_normal_position (toplevel,
						       &toplevel->priv->animation_end_x,
						       &toplevel->priv->animation_end_y);

	else if (toplevel->priv->state == PANEL_STATE_AUTO_HIDDEN)
		panel_toplevel_update_auto_hide_position (toplevel,
							  &toplevel->priv->animation_end_x,
							  &toplevel->priv->animation_end_y);
	else
		panel_toplevel_update_hidden_position (toplevel,
						       &toplevel->priv->animation_end_x,
						       &toplevel->priv->animation_end_y);

	gdk_window_get_origin (GTK_WIDGET (toplevel)->window, &cur_x, &cur_y);

	cur_x -= panel_multiscreen_x (screen, toplevel->priv->monitor);
	cur_y -= panel_multiscreen_y (screen, toplevel->priv->monitor);

	deltax = toplevel->priv->animation_end_x - cur_x;
	deltay = toplevel->priv->animation_end_y - cur_y;

	if (!deltax && !deltay) {
		toplevel->priv->animation_end_x = -1;
		toplevel->priv->animation_end_y = -1;
		return;
	}

	g_get_current_time (&toplevel->priv->animation_start_time);

	t = MAX (panel_toplevel_get_animation_time (toplevel, deltax),
		 panel_toplevel_get_animation_time (toplevel, deltay));

	toplevel->priv->animation_end_time = t;

	toplevel->priv->animating = TRUE;

	toplevel->priv->animation_timeout =
		g_timeout_add (20, (GSourceFunc) panel_toplevel_animation_timeout, toplevel);
}

void
panel_toplevel_hide (PanelToplevel    *toplevel,
		     gboolean          auto_hide,
		     GtkDirectionType  direction)
{

	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (toplevel->priv->state != PANEL_STATE_NORMAL)
		return;

	g_signal_emit (toplevel, toplevel_signals [HIDE_SIGNAL], 0);

	if (auto_hide)
		toplevel->priv->state = PANEL_STATE_AUTO_HIDDEN;
	else {
		if (toplevel->priv->attached)
			gtk_window_present (GTK_WINDOW (toplevel->priv->attach_toplevel));

		if (direction == -1) {
			if (toplevel->priv->orientation & PANEL_VERTICAL_MASK)
				direction = GTK_DIR_UP;
			else
				direction = GTK_DIR_LEFT;
		}

		switch (direction) {
		case GTK_DIR_UP:
			g_return_if_fail (toplevel->priv->orientation & PANEL_VERTICAL_MASK);
			toplevel->priv->state = PANEL_STATE_HIDDEN_UP;
			break;
		case GTK_DIR_DOWN:
			g_return_if_fail (toplevel->priv->orientation & PANEL_VERTICAL_MASK);
			toplevel->priv->state = PANEL_STATE_HIDDEN_DOWN;
			break;
		case GTK_DIR_LEFT:
			g_return_if_fail (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK);
			toplevel->priv->state = PANEL_STATE_HIDDEN_LEFT;
			break;
		case GTK_DIR_RIGHT:
			g_return_if_fail (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK);
			toplevel->priv->state = PANEL_STATE_HIDDEN_RIGHT;
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}

	if (toplevel->priv->animate)
		panel_toplevel_start_animation (toplevel);

	gtk_widget_queue_resize (GTK_WIDGET (toplevel));
}

static gboolean
panel_toplevel_auto_hide_timeout_handler (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), FALSE);

	if (toplevel->priv->block_auto_hide)
		return TRUE;

	/* keep coming back until the animation has finished.
	 * FIXME: we should really remove the timeout/idle
	 *        completely and re-instate it when the
	 *        animation has finished.
	 */
	if (toplevel->priv->animating)
		return TRUE;

	panel_toplevel_hide (toplevel, TRUE, -1);

	return FALSE;
}

void
panel_toplevel_unhide (PanelToplevel *toplevel)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (toplevel->priv->state == PANEL_STATE_NORMAL)
		return;

	toplevel->priv->state = PANEL_STATE_NORMAL;

	if (toplevel->priv->animate)
		panel_toplevel_start_animation (toplevel);

	gtk_widget_queue_resize (GTK_WIDGET (toplevel));

	if (!toplevel->priv->animate)
		g_signal_emit (toplevel, toplevel_signals [UNHIDE_SIGNAL], 0);
}

static gboolean
panel_toplevel_auto_unhide_timeout_handler (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), FALSE);

	/* keep coming back until the animation has finished.
	 * FIXME: we should really remove the timeout/idle
	 *        completely and re-instate it when the
	 *        animation has finished.
	 */
	if (toplevel->priv->animating)
		return TRUE;

	panel_toplevel_unhide (toplevel);

	return FALSE;
}

void
panel_toplevel_block_auto_hide (PanelToplevel *toplevel)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	toplevel->priv->block_auto_hide = TRUE;
}

void
panel_toplevel_unblock_auto_hide (PanelToplevel *toplevel)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	toplevel->priv->block_auto_hide = FALSE;

	panel_toplevel_queue_auto_hide (toplevel);
}

void
panel_toplevel_queue_auto_hide (PanelToplevel *toplevel)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (!toplevel->priv->auto_hide)
		return;

	if (toplevel->priv->hide_timeout)
		return;

	if (toplevel->priv->unhide_timeout)
		g_source_remove (toplevel->priv->unhide_timeout);
	toplevel->priv->unhide_timeout = 0;

	if (toplevel->priv->state != PANEL_STATE_NORMAL)
		return;

	if (toplevel->priv->block_auto_hide) {
		/* Since this will continue to be called until
		 * its unblocked again, we choose a sensible
	         * timeout.
		 */
		toplevel->priv->hide_timeout = 
			g_timeout_add (DEFAULT_HIDE_DELAY,
				       (GSourceFunc) panel_toplevel_auto_hide_timeout_handler,
				       toplevel);
		return;
	}

	if (panel_toplevel_contains_pointer (toplevel))
		return;

	if (toplevel->priv->hide_delay > 0)
		toplevel->priv->hide_timeout = 
			g_timeout_add (toplevel->priv->hide_delay,
				       (GSourceFunc) panel_toplevel_auto_hide_timeout_handler,
				       toplevel);
	else
		toplevel->priv->hide_timeout = 
			g_idle_add ((GSourceFunc) panel_toplevel_auto_hide_timeout_handler,
				    toplevel);
}

void
panel_toplevel_queue_auto_unhide (PanelToplevel *toplevel)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (toplevel->priv->unhide_timeout)
		return;

	if (toplevel->priv->hide_timeout)
		g_source_remove (toplevel->priv->hide_timeout);
	toplevel->priv->hide_timeout = 0;

	if (toplevel->priv->state != PANEL_STATE_AUTO_HIDDEN)
		return;

	if (toplevel->priv->unhide_delay > 0)
		toplevel->priv->unhide_timeout = 
			g_timeout_add (toplevel->priv->unhide_delay,
				       (GSourceFunc) panel_toplevel_auto_unhide_timeout_handler,
				       toplevel);
	else
		toplevel->priv->unhide_timeout = 
			g_idle_add ((GSourceFunc) panel_toplevel_auto_unhide_timeout_handler,
				    toplevel);
}

static gboolean
panel_toplevel_enter_notify_event (GtkWidget        *widget,
				   GdkEventCrossing *event)
{
	PanelToplevel *toplevel;

	g_return_val_if_fail (PANEL_IS_TOPLEVEL (widget), FALSE);

	toplevel = PANEL_TOPLEVEL (widget);

	if (!toplevel->priv->auto_hide)
		return FALSE;

	panel_toplevel_queue_auto_unhide (toplevel);

	return TRUE;
}

static gboolean
panel_toplevel_leave_notify_event (GtkWidget        *widget,
				   GdkEventCrossing *event)
{
	PanelToplevel *toplevel;

	g_return_val_if_fail (PANEL_IS_TOPLEVEL (widget), FALSE);

	toplevel = PANEL_TOPLEVEL (widget);

	if (!toplevel->priv->auto_hide)
		return FALSE;

	panel_toplevel_queue_auto_hide (toplevel);

	return TRUE;
}

static void
panel_toplevel_screen_changed (GtkWidget *widget,
			       GdkScreen *previous_screen)
{
	if (GTK_WIDGET_CLASS (parent_class)->screen_changed)
		GTK_WIDGET_CLASS (parent_class)->screen_changed (widget, previous_screen);

	gtk_widget_queue_resize (widget);
}

static void
panel_toplevel_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	PanelToplevel *toplevel;

	g_return_if_fail (PANEL_IS_TOPLEVEL (object));

	toplevel = PANEL_TOPLEVEL (object);

	switch (prop_id) {
	case PROP_NAME:
		panel_toplevel_set_name (toplevel, g_value_get_string (value));
		break;
	case PROP_EXPAND:
		panel_toplevel_set_expand (toplevel, g_value_get_boolean (value));
		break;
	case PROP_ORIENTATION:
		panel_toplevel_set_orientation (toplevel, g_value_get_enum (value));
		break;
	case PROP_SIZE:
		panel_toplevel_set_size (toplevel, g_value_get_int (value));
		break;
	case PROP_X:
		panel_toplevel_set_position (toplevel,
					     g_value_get_int (value),
					     toplevel->priv->x_centered,
					     -1,
					     toplevel->priv->y_centered);
		break;
	case PROP_X_CENTERED:
		panel_toplevel_set_position (toplevel,
					     -1, g_value_get_boolean (value),
					     -1, toplevel->priv->y_centered);
		break;
	case PROP_Y:
		panel_toplevel_set_position (toplevel,
					     -1,
					     toplevel->priv->x_centered,
					     g_value_get_int (value),
					     toplevel->priv->y_centered);
		break;
	case PROP_Y_CENTERED:
		panel_toplevel_set_position (toplevel,
					     -1, g_value_get_boolean (value),
					     -1, toplevel->priv->y_centered);
		break;
	case PROP_MONITOR:
		panel_toplevel_set_monitor (toplevel, g_value_get_int (value));
		break;
	case PROP_AUTOHIDE:
		panel_toplevel_set_auto_hide (toplevel, g_value_get_boolean (value));
		break;
	case PROP_HIDE_DELAY:
		panel_toplevel_set_hide_delay (toplevel, g_value_get_int (value));
		break;
	case PROP_UNHIDE_DELAY:
		panel_toplevel_set_unhide_delay (toplevel, g_value_get_int (value));
		break;
	case PROP_AUTOHIDE_SIZE:
		panel_toplevel_set_auto_hide_size (toplevel, g_value_get_int (value));
		break;
	case PROP_ANIMATE:
		panel_toplevel_set_animate (toplevel, g_value_get_boolean (value));
		break;
	case PROP_ANIMATION_SPEED:
		panel_toplevel_set_animation_speed (toplevel, g_value_get_enum (value));
		break;
	case PROP_BUTTONS_ENABLED:
		panel_toplevel_set_enable_buttons (toplevel, g_value_get_boolean (value));
		break;
	case PROP_ARROWS_ENABLED:
		panel_toplevel_set_enable_arrows (toplevel, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_toplevel_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	PanelToplevel *toplevel;

	g_return_if_fail (PANEL_IS_TOPLEVEL (object));

	toplevel = PANEL_TOPLEVEL (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, panel_toplevel_get_name (toplevel));
		break;
	case PROP_EXPAND:
		g_value_set_boolean (value, toplevel->priv->expand);
		break;
	case PROP_ORIENTATION:
		g_value_set_enum (value, toplevel->priv->orientation);
		break;
	case PROP_SIZE:
		g_value_set_int (value, toplevel->priv->size);
		break;
	case PROP_X:
		g_value_set_int (value, toplevel->priv->x);
		break;
	case PROP_X_CENTERED:
		g_value_set_boolean (value, toplevel->priv->x_centered);
		break;
	case PROP_Y:
		g_value_set_int (value, toplevel->priv->y);
		break;
	case PROP_Y_CENTERED:
		g_value_set_boolean (value, toplevel->priv->y_centered);
		break;
	case PROP_MONITOR:
		g_value_set_int (value, toplevel->priv->monitor);
		break;
	case PROP_AUTOHIDE:
		g_value_set_boolean (value, toplevel->priv->auto_hide);
		break;
	case PROP_HIDE_DELAY:
		g_value_set_int (value, toplevel->priv->hide_delay);
		break;
	case PROP_UNHIDE_DELAY:
		g_value_set_int (value, toplevel->priv->unhide_delay);
		break;
	case PROP_AUTOHIDE_SIZE:
		g_value_set_int (value, toplevel->priv->auto_hide_size);
		break;
	case PROP_ANIMATE:
		g_value_set_boolean (value, toplevel->priv->animate);
		break;
	case PROP_ANIMATION_SPEED:
		g_value_set_enum (value, toplevel->priv->animation_speed);
		break;
	case PROP_BUTTONS_ENABLED:
		g_value_set_boolean (value, toplevel->priv->buttons_enabled);
		break;
	case PROP_ARROWS_ENABLED:
		g_value_set_boolean (value, toplevel->priv->arrows_enabled);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_toplevel_finalize (GObject *object)
{
        PanelToplevel *toplevel = (PanelToplevel *) object;

	if (toplevel->priv->attached) {
		panel_toplevel_disconnect_attached (toplevel);

		toplevel->priv->attached = FALSE;

		toplevel->priv->attach_toplevel = NULL;
		toplevel->priv->attach_widget   = NULL;
	}

	if (toplevel->priv->description)
		g_free (toplevel->priv->description);
	toplevel->priv->description = NULL;

	if (toplevel->priv->name)
		g_free (toplevel->priv->name);
	toplevel->priv->name = NULL;

        g_free (toplevel->priv);
        toplevel->priv = NULL;

	if (parent_class->finalize)
		parent_class->finalize (object);
}

static void
panel_toplevel_class_init (PanelToplevelClass *klass)
{
	GObjectClass      *gobject_class   = (GObjectClass      *) klass;
	GtkObjectClass    *gtkobject_class = (GtkObjectClass    *) klass;
	GtkWidgetClass    *widget_class    = (GtkWidgetClass    *) klass;
	GtkContainerClass *container_class = (GtkContainerClass *) klass;
	GtkBindingSet     *binding_set;
	char              *icon_file;

        binding_set = gtk_binding_set_by_class (klass);

	parent_class = g_type_class_peek_parent (klass);

	gobject_class->set_property = panel_toplevel_set_property;
        gobject_class->get_property = panel_toplevel_get_property;
	gobject_class->finalize     = panel_toplevel_finalize;

	gtkobject_class->destroy = panel_toplevel_destroy;

	widget_class->realize              = panel_toplevel_realize;
	widget_class->unrealize            = panel_toplevel_unrealize;
	widget_class->size_request         = panel_toplevel_size_request;
	widget_class->size_allocate        = panel_toplevel_size_allocate;
	widget_class->expose_event         = panel_toplevel_expose;
	widget_class->button_press_event   = panel_toplevel_button_press_event;
	widget_class->button_release_event = panel_toplevel_button_release_event;
	widget_class->motion_notify_event  = panel_toplevel_motion_notify_event;
	widget_class->enter_notify_event   = panel_toplevel_enter_notify_event;
	widget_class->leave_notify_event   = panel_toplevel_leave_notify_event;
	widget_class->screen_changed       = panel_toplevel_screen_changed;

	container_class->check_resize = panel_toplevel_check_resize;

	klass->hiding           = NULL;
	klass->unhiding         = NULL;
	klass->popup_panel_menu = panel_toplevel_popup_panel_menu;

	g_object_class_install_property (
		gobject_class,
		PROP_NAME,
		g_param_spec_string (
			"name",
			_("Name"),
			_("The name of this panel"),
			NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_EXPAND,
		g_param_spec_boolean (
			"expand",
			_("Expand"),
			_("Expand to take up the full monitor width/height"),
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_ORIENTATION,
		g_param_spec_enum (
			"orientation",
			_("Orientation"),
			_("The orientation of the panel"),
			PANEL_TYPE_ORIENTATION,
			PANEL_ORIENTATION_TOP,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_SIZE,
		g_param_spec_int (
			"size",
			_("Size"),
			_("The height (or width when vertivcal) of the panel"),
			0,
			G_MAXINT,
			DEFAULT_SIZE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));


	g_object_class_install_property (
		gobject_class,
		PROP_X,
		g_param_spec_int (
			"x",
			_("X position"),
			_("The X position of the panel"),
			0,
			G_MAXINT,
			0,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_X_CENTERED,
		g_param_spec_boolean (
			"x-centered",
			_("X centered"),
			_("The x co-ordinate is relative to center screen"),
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_Y,
		g_param_spec_int (
			"y",
			_("Y position"),
			_("The Y position of the panel"),
			0,
			G_MAXINT,
			0,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_Y_CENTERED,
		g_param_spec_boolean (
			"y-centered",
			_("Y centered"),
			_("The y co-ordinate is relative to center screen"),
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_MONITOR,
		g_param_spec_int (
			"monitor",
			_("Xinerama monitor"),
			_("The monitor (in terms of Xinerama) which the panel is on"),
			0,
			G_MAXINT,
			0,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_AUTOHIDE,
		g_param_spec_boolean (
			"auto-hide",
			_("Auto hide"),
			_("Automatically hide the panel when the mouse leaves the panel"),
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_HIDE_DELAY,
		g_param_spec_int (
			"hide-delay",
			_("Hide delay"),
			_("The number of miliseconds to delay before automatically hiding"),
			0,
			G_MAXINT,
			DEFAULT_HIDE_DELAY,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_UNHIDE_DELAY,
		g_param_spec_int (
			"unhide-delay",
			_("Un-hide delay"),
			_("The number of miliseconds to delay before automatically un-hiding"),
			0,
			G_MAXINT,
			DEFAULT_UNHIDE_DELAY,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_AUTOHIDE_SIZE,
		g_param_spec_int (
			"auto-hide-size",
			_("Auto-hide size"),
			_("The number of pixels visible when the panel has been automatically hidden"),
			1,
			G_MAXINT,
			DEFAULT_AUTO_HIDE_SIZE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_ANIMATE,
		g_param_spec_boolean (
			"animate",
			_("Animate"),
			_("Enable hiding/showing animations"),
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_ANIMATION_SPEED,
		g_param_spec_enum (
			"animation-speed",
			_("Animation Speed"),
			_("The speed at which to animate panel hiding/showing"),
			PANEL_TYPE_ANIMATION_SPEED,
			PANEL_ANIMATION_MEDIUM,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_BUTTONS_ENABLED,
		g_param_spec_boolean (
			"buttons-enabled",
			_("Buttons Enabled"),
			_("Enable hide/show buttons"),
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_ARROWS_ENABLED,
		g_param_spec_boolean (
			"arrows-enabled",
			_("Arrows Enabled"),
			_("Enable arrows on hide/show buttons"),
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	toplevel_signals [HIDE_SIGNAL] =
		g_signal_new ("hiding",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (PanelToplevelClass, hiding),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	toplevel_signals [UNHIDE_SIGNAL] =
		g_signal_new ("unhiding",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (PanelToplevelClass, unhiding),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	toplevel_signals [POPUP_PANEL_MENU_SIGNAL] =
		g_signal_new ("popup-panel-menu",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (PanelToplevelClass, popup_panel_menu),
			      NULL,
			      NULL,
			      panel_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN,
			      0);

	gtk_binding_entry_add_signal (binding_set, GDK_F10, GDK_CONTROL_MASK,
                                     "popup_panel_menu", 0);

	icon_file = gnome_program_locate_file (
			NULL, GNOME_FILE_DOMAIN_PIXMAP, "gnome-panel.png", TRUE, NULL);
	if (icon_file) {
		gtk_window_set_default_icon_from_file (icon_file, NULL);
		g_free (icon_file);
	}
}

static void
panel_toplevel_setup_widgets (PanelToplevel *toplevel)
{
	GtkWidget *container;

	toplevel->priv->table = gtk_table_new (3, 3, FALSE);

	toplevel->priv->hide_button_top =
		panel_toplevel_add_hide_button (toplevel, GTK_ARROW_UP,    1, 2, 0, 1);

	toplevel->priv->hide_button_bottom =
		panel_toplevel_add_hide_button (toplevel, GTK_ARROW_DOWN,  1, 2, 2, 3);

	toplevel->priv->hide_button_left =
		panel_toplevel_add_hide_button (toplevel, GTK_ARROW_LEFT,  0, 1, 1, 2);

	toplevel->priv->hide_button_right =
		panel_toplevel_add_hide_button (toplevel, GTK_ARROW_RIGHT, 2, 3, 1, 2);

	if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK) {
		gtk_widget_show (toplevel->priv->hide_button_left);
		gtk_widget_show (toplevel->priv->hide_button_right);
	} else {
		gtk_widget_show (toplevel->priv->hide_button_top);
		gtk_widget_show (toplevel->priv->hide_button_bottom);
	}

	toplevel->priv->inner_frame = g_object_new (PANEL_TYPE_FRAME, NULL);

	gtk_table_attach (GTK_TABLE (toplevel->priv->table),
			  GTK_WIDGET (toplevel->priv->inner_frame),
			  1, 2,
			  1, 2,
			  GTK_FILL | GTK_EXPAND | GTK_SHRINK,
			  GTK_FILL | GTK_EXPAND | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (GTK_WIDGET (toplevel->priv->inner_frame));

	container = panel_widget_new (toplevel,
				      NULL,
				      !toplevel->priv->expand,
				      toplevel->priv->orientation & PANEL_HORIZONTAL_MASK ?
						GTK_ORIENTATION_HORIZONTAL :
						GTK_ORIENTATION_VERTICAL,	
				      toplevel->priv->size,
				      PANEL_BACK_NONE, NULL,
				      FALSE, FALSE, FALSE, NULL);

	toplevel->priv->panel_widget = PANEL_WIDGET (container);

	gtk_container_add (GTK_CONTAINER (toplevel->priv->inner_frame), container);
	gtk_widget_show (container);

	gtk_container_add (GTK_CONTAINER (toplevel), toplevel->priv->table);
	gtk_widget_show (toplevel->priv->table);

}

static void
panel_toplevel_instance_init (PanelToplevel      *toplevel,
			      PanelToplevelClass *klass)
{
	int i;

	toplevel->priv = g_new0 (PanelToplevelPrivate, 1);

	toplevel->priv->expand          = TRUE;
	toplevel->priv->orientation     = PANEL_ORIENTATION_BOTTOM;
	toplevel->priv->size            = DEFAULT_SIZE;
	toplevel->priv->x               = 0;
	toplevel->priv->y               = 0;
	toplevel->priv->monitor         = 0;
	toplevel->priv->hide_delay      = DEFAULT_HIDE_DELAY;
	toplevel->priv->unhide_delay    = DEFAULT_UNHIDE_DELAY;
	toplevel->priv->auto_hide_size  = DEFAULT_AUTO_HIDE_SIZE;
	toplevel->priv->animation_speed = PANEL_ANIMATION_MEDIUM;

	toplevel->priv->state = PANEL_STATE_NORMAL;

	toplevel->priv->name        = NULL;
	toplevel->priv->description = NULL;

	toplevel->priv->move_timeout   = 0;
	toplevel->priv->hide_timeout   = 0;
	toplevel->priv->unhide_timeout = 0;

	toplevel->priv->geometry.x      = -1;
	toplevel->priv->geometry.y      = -1;
	toplevel->priv->geometry.width  = -1;
	toplevel->priv->geometry.height = -1;

	toplevel->priv->drag_offset_x = 0;
	toplevel->priv->drag_offset_y = 0;

	toplevel->priv->animation_end_x              = 0;
	toplevel->priv->animation_end_y              = 0;
	toplevel->priv->animation_start_time.tv_sec  = 0;
	toplevel->priv->animation_start_time.tv_usec = 0;
	toplevel->priv->animation_end_time           = 0;
	toplevel->priv->animation_timeout            = 0;

	toplevel->priv->panel_widget       = NULL;
	toplevel->priv->inner_frame        = NULL;
	toplevel->priv->table              = NULL;
	toplevel->priv->hide_button_top    = NULL;
	toplevel->priv->hide_button_bottom = NULL;
	toplevel->priv->hide_button_left   = NULL;
	toplevel->priv->hide_button_right  = NULL;

	toplevel->priv->attach_toplevel = NULL;
	toplevel->priv->attach_widget   = NULL;

	for (i = 0; i < N_ATTACH_TOPLEVEL_SIGNALS; i++)
		toplevel->priv->attach_toplevel_signals [i] = 0;
	for (i = 0; i < N_ATTACH_WIDGET_SIGNALS; i++)
		toplevel->priv->attach_widget_signals [i] = 0;

	toplevel->priv->auto_hide         = FALSE;
	toplevel->priv->buttons_enabled   = TRUE;
	toplevel->priv->arrows_enabled    = TRUE;
	toplevel->priv->x_centered        = FALSE;
	toplevel->priv->y_centered        = FALSE;
	toplevel->priv->in_drag           = FALSE;
	toplevel->priv->animating         = FALSE;
	toplevel->priv->drag_is_rotate    = FALSE;
	toplevel->priv->drag_is_resize    = FALSE;
	toplevel->priv->position_centered = FALSE;
	toplevel->priv->block_auto_hide   = FALSE;
	toplevel->priv->attached          = FALSE;
	toplevel->priv->attach_hidden     = FALSE;

	gtk_widget_add_events (GTK_WIDGET (toplevel),
			       GDK_BUTTON_PRESS_MASK |
			       GDK_BUTTON_RELEASE_MASK |
			       GDK_POINTER_MOTION_MASK |
			       GDK_ENTER_NOTIFY_MASK |
			       GDK_LEAVE_NOTIFY_MASK);

	panel_toplevel_setup_widgets (toplevel);
	panel_toplevel_update_description (toplevel);
}

GType
panel_toplevel_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelToplevelClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_toplevel_class_init,
			NULL,
			NULL,
			sizeof (PanelToplevel),
			0,
			(GInstanceInitFunc) panel_toplevel_instance_init,
			NULL
		};

		type = g_type_register_static (
				GTK_TYPE_WINDOW, "PanelToplevel", &info, 0);
	}

	return type;
}

gpointer
panel_toplevel_get_panel_widget (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), NULL);

	return toplevel->priv->panel_widget;
}

static void
panel_toplevel_update_name (PanelToplevel *toplevel)
{
	char *title;

	g_assert (toplevel->priv->description != NULL);

	title = toplevel->priv->name ? toplevel->priv->name : toplevel->priv->description;

	gtk_window_set_title (GTK_WINDOW (toplevel), title);

	panel_a11y_set_atk_name_desc (
		GTK_WIDGET (toplevel->priv->panel_widget),
		title, toplevel->priv->description);
}

void
panel_toplevel_set_name (PanelToplevel *toplevel,
			 const char    *name)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (!toplevel->priv->name && (!name || !name [0]))
		return;

	if (toplevel->priv->name && name && name [0] &&
	    !strcmp (toplevel->priv->name, name))
		return;

	if (toplevel->priv->name)
		g_free (toplevel->priv->name);
	toplevel->priv->name = NULL;

	if (name && name [0])
		toplevel->priv->name = g_strdup (name);

	panel_toplevel_update_name (toplevel);

	g_object_notify (G_OBJECT (toplevel), "name");
}

G_CONST_RETURN char *
panel_toplevel_get_name (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), NULL);

	return toplevel->priv->name;
}

void
panel_toplevel_set_expand (PanelToplevel *toplevel,
			   gboolean       expand)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	expand = expand != FALSE;

	if (toplevel->priv->expand == expand)
		return;

	toplevel->priv->expand = expand;

	if (!toplevel->priv->expand) {
		if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK)
			panel_toplevel_set_position (toplevel, 0, TRUE, -1, FALSE);
		else
			panel_toplevel_set_position (toplevel, -1, FALSE, 0, TRUE);
	}

	gtk_widget_queue_resize (GTK_WIDGET (toplevel));

	panel_widget_set_packed (toplevel->priv->panel_widget, !toplevel->priv->expand);

	g_object_notify (G_OBJECT (toplevel), "expand");
}

gboolean
panel_toplevel_get_expand (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), TRUE);

	return toplevel->priv->expand;
}

void
panel_toplevel_set_orientation (PanelToplevel    *toplevel,
				PanelOrientation  orientation)
{
	GtkWidget *widget;
	gboolean   rotate;

	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (toplevel->priv->orientation == orientation)
		return;

	widget = GTK_WIDGET (toplevel);

	rotate = FALSE;
	if ((orientation & PANEL_HORIZONTAL_MASK) &&
	    (toplevel->priv->orientation & PANEL_VERTICAL_MASK))
		rotate = TRUE;
	else if ((orientation & PANEL_VERTICAL_MASK) &&
		 (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK))
		rotate = TRUE;

	/* rotate around the center */
	if (rotate && !toplevel->priv->position_centered && !toplevel->priv->expand) {
		toplevel->priv->position_centered = TRUE;

		g_object_freeze_notify (G_OBJECT (toplevel));

		if (!toplevel->priv->x_centered) {
			toplevel->priv->x += toplevel->priv->geometry.width  / 2;
			g_object_notify (G_OBJECT (toplevel), "x");
		}

		if (!toplevel->priv->y_centered) {
			toplevel->priv->y += toplevel->priv->geometry.height / 2;
			g_object_notify (G_OBJECT (toplevel), "y");
		}

		g_object_thaw_notify (G_OBJECT (toplevel));
	}

	toplevel->priv->orientation = orientation;

	panel_toplevel_update_hide_buttons (toplevel);

	panel_widget_set_orientation (	
		toplevel->priv->panel_widget,
		toplevel->priv->orientation & PANEL_HORIZONTAL_MASK ?
					GTK_ORIENTATION_HORIZONTAL :
					GTK_ORIENTATION_VERTICAL);

	gtk_widget_queue_resize (GTK_WIDGET (toplevel));

	g_object_notify (G_OBJECT (toplevel), "orientation");
}

PanelOrientation
panel_toplevel_get_orientation (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), GTK_ORIENTATION_HORIZONTAL);

	return toplevel->priv->orientation;
}

void
panel_toplevel_set_size (PanelToplevel *toplevel,
			 int            size)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));
	g_return_if_fail (size >= 0);

	if (toplevel->priv->size == size)
		return;

	toplevel->priv->size = size;

	panel_widget_set_size (toplevel->priv->panel_widget, toplevel->priv->size);

	gtk_widget_queue_resize (GTK_WIDGET (toplevel));

	g_object_notify (G_OBJECT (toplevel), "size");
}

int
panel_toplevel_get_size (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), DEFAULT_SIZE);

	return toplevel->priv->size;
}

void
panel_toplevel_set_auto_hide_size (PanelToplevel *toplevel,
				   int            auto_hide_size)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));
	g_return_if_fail (auto_hide_size > 0);

	if (toplevel->priv->auto_hide_size == auto_hide_size)
		return;

	toplevel->priv->auto_hide_size = auto_hide_size;

	if (toplevel->priv->state == PANEL_STATE_AUTO_HIDDEN)
		panel_toplevel_update_struts (toplevel);

	g_object_notify (G_OBJECT (toplevel), "auto-hide-size");
}

int
panel_toplevel_get_auto_hide_size (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), DEFAULT_AUTO_HIDE_SIZE);

	return toplevel->priv->auto_hide_size;
}

void
panel_toplevel_set_position (PanelToplevel *toplevel,
			     int            x,
			     gboolean       x_centered,
			     int            y,
			     gboolean       y_centered)
{
	gboolean changed = FALSE;

	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));
	g_return_if_fail (x >= 0 || x == -1);
	g_return_if_fail (y >= 0 || y == -1);

	x_centered = x_centered != FALSE;
	y_centered = y_centered != FALSE;

	g_object_freeze_notify (G_OBJECT (toplevel));

	if (x != -1) {
		if (toplevel->priv->x != x) {
			toplevel->priv->x = x;
			changed = TRUE;
			g_object_notify (G_OBJECT (toplevel), "x");
		}

		if (toplevel->priv->x_centered != x_centered) {
			toplevel->priv->x_centered = x_centered;
			changed = TRUE;
			g_object_notify (G_OBJECT (toplevel), "x-centered");
		}
	}

	if (y != -1) {
		if (toplevel->priv->y != y) {
			toplevel->priv->y = y;
			changed = TRUE;
			g_object_notify (G_OBJECT (toplevel), "y");
		}

		if (toplevel->priv->y_centered != y_centered) {
			toplevel->priv->y_centered = y_centered;
			changed = TRUE;
			g_object_notify (G_OBJECT (toplevel), "y-centered");
		}
	}

	gtk_widget_queue_resize (GTK_WIDGET (toplevel));

	g_object_thaw_notify (G_OBJECT (toplevel));
}

void
panel_toplevel_get_position (PanelToplevel *toplevel,
			     int           *x,
			     int           *y)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (x)
		*x = toplevel->priv->x;

	if (y)
		*y = toplevel->priv->y;
}

gboolean
panel_toplevel_get_x_centered (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), FALSE);

	return toplevel->priv->x_centered;
}

gboolean
panel_toplevel_get_y_centered (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), FALSE);

	return toplevel->priv->y_centered;
}

void
panel_toplevel_set_monitor (PanelToplevel *toplevel,
			    int            monitor)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (toplevel->priv->monitor == monitor)
		return;

	toplevel->priv->monitor = monitor;

	gtk_widget_queue_resize (GTK_WIDGET (toplevel));

	g_object_notify (G_OBJECT (toplevel), "monitor");
}

int
panel_toplevel_get_monitor (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), -1);

	return toplevel->priv->monitor;
}

void
panel_toplevel_set_auto_hide (PanelToplevel *toplevel,
			      gboolean       auto_hide)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	auto_hide = auto_hide != FALSE;

	if (toplevel->priv->auto_hide == auto_hide)
		return;

	toplevel->priv->auto_hide = auto_hide;

	if (toplevel->priv->auto_hide)
		panel_toplevel_queue_auto_hide (toplevel);
	else
		panel_toplevel_queue_auto_unhide (toplevel);

	panel_toplevel_update_struts (toplevel);

	g_object_notify (G_OBJECT (toplevel), "auto-hide");
}

gboolean
panel_toplevel_get_auto_hide (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), -1);

	return toplevel->priv->auto_hide;
}

void
panel_toplevel_set_hide_delay (PanelToplevel *toplevel,
			       int            hide_delay)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (toplevel->priv->hide_delay == hide_delay)
		return;

	toplevel->priv->hide_delay = hide_delay;

	g_object_notify (G_OBJECT (toplevel), "hide-delay");
}

int
panel_toplevel_get_hide_delay (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), -1);

	return toplevel->priv->hide_delay;
}

void
panel_toplevel_set_unhide_delay (PanelToplevel *toplevel,
				 int            unhide_delay)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (toplevel->priv->unhide_delay == unhide_delay)
		return;

	toplevel->priv->unhide_delay = unhide_delay;

	g_object_notify (G_OBJECT (toplevel), "unhide-delay");
}

int
panel_toplevel_get_unhide_delay (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), -1);

	return toplevel->priv->unhide_delay;
}

void
panel_toplevel_set_animate (PanelToplevel *toplevel,
			    gboolean       animate)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	animate = animate != FALSE;

	if (toplevel->priv->animate == animate)
		return;

	toplevel->priv->animate = animate;

	g_object_notify (G_OBJECT (toplevel), "animate");
}

gboolean
panel_toplevel_get_animate (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), FALSE);

	return toplevel->priv->animate;
}

void
panel_toplevel_set_animation_speed (PanelToplevel       *toplevel,
				    PanelAnimationSpeed  animation_speed)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (toplevel->priv->animation_speed == animation_speed)
		return;

	toplevel->priv->animation_speed = animation_speed;

	g_object_notify (G_OBJECT (toplevel), "animation-speed");
}

PanelAnimationSpeed
panel_toplevel_get_animation_speed (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), 0);

	return toplevel->priv->animation_speed;
}

void
panel_toplevel_set_enable_buttons (PanelToplevel *toplevel,
				   gboolean       enable_buttons)
{
	enable_buttons = enable_buttons != FALSE;

	if (toplevel->priv->buttons_enabled == enable_buttons)
		return;

	toplevel->priv->buttons_enabled = enable_buttons;

	panel_toplevel_update_hide_buttons (toplevel);

	g_object_notify (G_OBJECT (toplevel), "buttons-enabled");
}

gboolean
panel_toplevel_get_enable_buttons (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), FALSE);

	return toplevel->priv->buttons_enabled;
}

void
panel_toplevel_set_enable_arrows (PanelToplevel *toplevel,
				  gboolean       enable_arrows)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	enable_arrows = enable_arrows != FALSE;

	if (toplevel->priv->arrows_enabled == enable_arrows)
		return;

	toplevel->priv->arrows_enabled = enable_arrows;

	panel_toplevel_update_hide_buttons (toplevel);

	g_object_notify (G_OBJECT (toplevel), "arrows-enabled");
}

gboolean
panel_toplevel_get_enable_arrows (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), FALSE);

	return toplevel->priv->arrows_enabled;
}

void
panel_toplevel_rotate (PanelToplevel *toplevel,
		       gboolean       clockwise)
{
	PanelOrientation orientation;

	/* Relies on PanelOrientation definition:
	 *
	 * typedef enum {
	 *        PANEL_ORIENTATION_TOP    = 1 << 0,
	 *        PANEL_ORIENTATION_RIGHT  = 1 << 1,
	 *        PANEL_ORIENTATION_BOTTOM = 1 << 2,
	 *        PANEL_ORIENTATION_LEFT   = 1 << 3
	 * } PanelOrientation;
	 */

	orientation = toplevel->priv->orientation;

	if (clockwise)
		orientation <<= 1;
	else
		orientation >>= 1;

	if (orientation == 0)
		orientation = PANEL_ORIENTATION_LEFT;

	else if (orientation > PANEL_ORIENTATION_LEFT)
		orientation = PANEL_ORIENTATION_TOP;

	panel_toplevel_set_orientation (toplevel, orientation);
}

PanelState
panel_toplevel_get_state (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), 0);

	return toplevel->priv->state;
}

gboolean
panel_toplevel_get_is_hidden (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), FALSE);

	if (toplevel->priv->state == PANEL_STATE_HIDDEN_UP   ||
	    toplevel->priv->state == PANEL_STATE_HIDDEN_DOWN ||
	    toplevel->priv->state == PANEL_STATE_HIDDEN_LEFT ||
	    toplevel->priv->state == PANEL_STATE_HIDDEN_RIGHT)
		return TRUE;

	return FALSE;
}
