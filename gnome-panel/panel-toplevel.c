/*
 * panel-toplevel.c: The panel's toplevel window object.
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2004 Rob Adams
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

#include "panel-toplevel.h"

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-program.h>

#include "panel-profile.h"
#include "panel-frame.h"
#include "panel-xutils.h"
#include "panel-multiscreen.h"
#include "panel-a11y.h"
#include "panel-typebuiltins.h"
#include "panel-marshal.h"
#include "panel-widget.h"
#include "panel-bindings.h"
#include "panel-struts.h"
#include "xstuff.h"
#include "panel-config-global.h"
#include "panel-lockdown.h"

#define PANEL_TOPLEVEL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_TOPLEVEL, PanelToplevelPrivate))

#define DEFAULT_SIZE              48
#define DEFAULT_AUTO_HIDE_SIZE    6
#define DEFAULT_HIDE_DELAY        500
#define DEFAULT_UNHIDE_DELAY      500
#define MINIMUM_WIDTH             100
#define SNAP_TOLERANCE            20
#define DEFAULT_ARROW_SIZE        20
#define HANDLE_SIZE               10
#define N_ATTACH_TOPLEVEL_SIGNALS 5
#define N_ATTACH_WIDGET_SIGNALS   5

typedef enum {
	PANEL_GRAB_OP_NONE,
	PANEL_GRAB_OP_MOVE,
	PANEL_GRAB_OP_RESIZE,
	PANEL_GRAB_OP_RESIZE_UP,
	PANEL_GRAB_OP_RESIZE_DOWN,
	PANEL_GRAB_OP_RESIZE_LEFT,
	PANEL_GRAB_OP_RESIZE_RIGHT
} PanelGrabOpType;

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

	guint                   hide_timeout;
	guint                   unhide_timeout;

	GdkRectangle            geometry;
	PanelFrameEdge          edges;

	int                     original_width;
	int                     original_height;

	PanelGrabOpType         grab_op;

	/* The offset within the panel from which the panel
	 * drag was initiated relative to the screen origin.
	 */
	int                     drag_offset_x;
	int                     drag_offset_y;

	/* Saved state before for cancelled grab op */
	int                     orig_monitor;
	int                     orig_x;
	int                     orig_y;
	int                     orig_size;
	int                     orig_orientation;

	/* relative to the monitor origin */
	int                     animation_end_x;
	int                     animation_end_y;
	int                     animation_end_width;
	int                     animation_end_height;
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
	gint			n_autohide_disablers;

	/* Auto-hide blocking */
	guint                   block_auto_hide;

	guint                   auto_hide : 1;
	guint                   animate : 1;
	guint                   buttons_enabled : 1;
	guint                   arrows_enabled : 1;

	/* The co-ordinates are relative to center screen */
	guint                   x_centered : 1;
	guint                   y_centered : 1;

	/* The panel is not lined up with th screen edge */
	guint                   floating : 1;

	/* We are currently animating a hide/show */
	guint                   animating : 1;

	/* This is a keyboard initiated grab operation */
	guint                   grab_is_keyboard : 1;

	/* The x-y co-ordinates temporarily specify the panel center */
	guint                   position_centered : 1;

	/* The toplevel is "attached" to another widget */
	guint                   attached : 1;

	/* Hidden temporarily because the attach_toplevel was hidden */
	guint                   attach_hidden : 1;

	/* More saved grab op state */
	guint                   orig_x_centered : 1;
	guint                   orig_y_centered : 1;

	/* flag to see if we have already done geometry updating,
	   if not then we're still loading and can ignore many things */
	guint                   updated_geometry_initial : 1;
};

enum {
	HIDE_SIGNAL,
	UNHIDE_SIGNAL,
	POPUP_PANEL_MENU_SIGNAL,
	TOGGLE_EXPAND_SIGNAL,
	EXPAND_SIGNAL,
	UNEXPAND_SIGNAL,
	TOGGLE_HIDDEN_SIGNAL,
	BEGIN_MOVE_SIGNAL,
	BEGIN_RESIZE_SIGNAL,
	LAST_SIGNAL
};

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
	PROP_ARROWS_ENABLED
};

static GObjectClass *parent_class;
static guint         toplevel_signals [LAST_SIGNAL] = { 0 };
static GSList       *toplevel_list = NULL;

GSList *
panel_toplevel_list_toplevels (void)
{
	return toplevel_list;
}

/* Is this the last un-attached toplevel? */
gboolean
panel_toplevel_is_last_unattached (PanelToplevel *toplevel)
{
	GSList *l;

	if (panel_toplevel_get_is_attached (toplevel))
		return FALSE;

	for (l = toplevel_list; l; l = l->next) {
		PanelToplevel *t = l->data;

		if (t != toplevel && !panel_toplevel_get_is_attached (t))
			return FALSE;
	}

	return TRUE;
}


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
				     int           *x,
				     int           *y,
				     int           *width,
				     int           *height)
{
	GdkScreen *screen;

	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), NULL);
	g_return_val_if_fail (width != NULL && height != NULL, NULL);

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));

	if (x) *x = panel_multiscreen_x (screen, toplevel->priv->monitor);
	if (y) *y = panel_multiscreen_y (screen, toplevel->priv->monitor);

	if (width)  *width  = panel_multiscreen_width  (screen, toplevel->priv->monitor);
	if (height) *height = panel_multiscreen_height (screen, toplevel->priv->monitor);

	return screen;
}

static GdkCursorType
panel_toplevel_grab_op_cursor (PanelToplevel   *toplevel,
			       PanelGrabOpType  grab_op)
{
	GdkCursorType retval = -1;

	switch (grab_op) {
	case PANEL_GRAB_OP_MOVE:
	case PANEL_GRAB_OP_RESIZE:
		if (toplevel->priv->grab_is_keyboard)
			retval = GDK_CROSS;
		else
			retval = GDK_FLEUR;
		break;
		break;
	case PANEL_GRAB_OP_RESIZE_UP:
		retval = GDK_TOP_SIDE;
		break;
	case PANEL_GRAB_OP_RESIZE_DOWN:
		retval = GDK_BOTTOM_SIDE;
		break;
	case PANEL_GRAB_OP_RESIZE_LEFT:
		retval = GDK_LEFT_SIDE;
		break;
	case PANEL_GRAB_OP_RESIZE_RIGHT:
		retval = GDK_RIGHT_SIDE;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return retval;
}

static void
panel_toplevel_init_resize_drag_offsets (PanelToplevel   *toplevel,
					 PanelGrabOpType  grab_op)
{
	toplevel->priv->drag_offset_x = 0;
	toplevel->priv->drag_offset_y = 0;

	switch (grab_op) {
	case PANEL_GRAB_OP_RESIZE_DOWN:
		toplevel->priv->drag_offset_y = toplevel->priv->geometry.y;
		break;
	case PANEL_GRAB_OP_RESIZE_UP:
		toplevel->priv->drag_offset_y =
			toplevel->priv->geometry.y + toplevel->priv->geometry.height;
		break;
	case PANEL_GRAB_OP_RESIZE_RIGHT:
		toplevel->priv->drag_offset_x = toplevel->priv->geometry.x;
		break;
	case PANEL_GRAB_OP_RESIZE_LEFT:
		toplevel->priv->drag_offset_x =
			toplevel->priv->geometry.x + toplevel->priv->geometry.width;
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
panel_toplevel_warp_pointer (PanelToplevel *toplevel)
{
	GtkWidget    *widget;
	GdkRectangle  geometry;
	int           x, y;

	widget = GTK_WIDGET (toplevel);

	geometry = toplevel->priv->geometry;

	x = y = 0;

	switch (toplevel->priv->grab_op) {
	case PANEL_GRAB_OP_MOVE:
	case PANEL_GRAB_OP_RESIZE:
		x = (geometry.width  / 2);
		y = (geometry.height / 2);
		break;
	case PANEL_GRAB_OP_RESIZE_UP:
		x = (geometry.width  / 2);
		break;
	case PANEL_GRAB_OP_RESIZE_DOWN:
		x = (geometry.width  / 2);
		y = geometry.height;
		break;
	case PANEL_GRAB_OP_RESIZE_LEFT:
		y = (geometry.height / 2);
		break;
	case PANEL_GRAB_OP_RESIZE_RIGHT:
		x = geometry.width;
		y = (geometry.height / 2);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	if (toplevel->priv->grab_op == PANEL_GRAB_OP_MOVE ||
	    toplevel->priv->grab_op == PANEL_GRAB_OP_RESIZE) {
		toplevel->priv->drag_offset_x = x;
		toplevel->priv->drag_offset_y = y;
	} else
		panel_toplevel_init_resize_drag_offsets (toplevel, toplevel->priv->grab_op);

	panel_warp_pointer (widget->window, x, y);
}

static void
panel_toplevel_begin_attached_move (PanelToplevel *toplevel,
				    gboolean       is_keyboard,
				    guint32        time_)
{
	PanelWidget *attached_panel_widget;

	attached_panel_widget = panel_toplevel_get_panel_widget (toplevel->priv->attach_toplevel);

	panel_widget_applet_drag_start (attached_panel_widget,
					toplevel->priv->attach_widget,
					is_keyboard ? PW_DRAG_OFF_CENTER : PW_DRAG_OFF_CURSOR,
					time_);
}

static void
panel_toplevel_begin_grab_op (PanelToplevel   *toplevel,
			      PanelGrabOpType  op_type,
			      gboolean         grab_keyboard,
			      guint32          time_)
{
	GtkWidget     *widget;
	GdkCursorType  cursor_type;
	GdkCursor     *cursor;

	if (toplevel->priv->state != PANEL_STATE_NORMAL ||
	    toplevel->priv->grab_op != PANEL_GRAB_OP_NONE)
		return;

	if (panel_lockdown_get_locked_down ())
		return;

	/* If any of the position/orientation are not writable,
	   then we can't really move freely */
	if (op_type == PANEL_GRAB_OP_MOVE &&
	    ! panel_profile_can_be_moved_freely (toplevel))
		return;

	/* If size is not writable, then we can't resize */
	if ((op_type == PANEL_GRAB_OP_RESIZE || 
	     op_type == PANEL_GRAB_OP_RESIZE_UP || 
	     op_type == PANEL_GRAB_OP_RESIZE_DOWN || 
	     op_type == PANEL_GRAB_OP_RESIZE_LEFT || 
	     op_type == PANEL_GRAB_OP_RESIZE_RIGHT) &&
	    ! panel_profile_is_writable_toplevel_size (toplevel))
		return;

	if (toplevel->priv->attached && op_type == PANEL_GRAB_OP_MOVE) {
		panel_toplevel_begin_attached_move (toplevel, grab_keyboard, time_);
		return;
	}

	widget = GTK_WIDGET (toplevel);

	toplevel->priv->grab_op          = op_type;
	toplevel->priv->grab_is_keyboard = grab_keyboard;

	toplevel->priv->orig_monitor     = toplevel->priv->monitor;
	toplevel->priv->orig_x           = toplevel->priv->x;
	toplevel->priv->orig_x_centered  = toplevel->priv->x_centered;
	toplevel->priv->orig_y           = toplevel->priv->y;
	toplevel->priv->orig_y_centered  = toplevel->priv->y_centered;
	toplevel->priv->orig_size        = toplevel->priv->size;
	toplevel->priv->orig_orientation = toplevel->priv->orientation;

	gtk_grab_add (widget);

	if (toplevel->priv->grab_is_keyboard)
		panel_toplevel_warp_pointer (toplevel);

	cursor_type = panel_toplevel_grab_op_cursor (
				toplevel, toplevel->priv->grab_op);

	cursor = gdk_cursor_new (cursor_type);
	gdk_pointer_grab (widget->window, FALSE, 
			  GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
			  NULL, cursor, time_);
	gdk_cursor_unref (cursor);

	if (grab_keyboard)
		gdk_keyboard_grab (widget->window, FALSE, time_);
}

static void
panel_toplevel_end_grab_op (PanelToplevel *toplevel,
			    guint32        time_)
{
	GtkWidget *widget;

	g_return_if_fail (toplevel->priv->grab_op != PANEL_GRAB_OP_NONE);

	widget = GTK_WIDGET (toplevel);

	toplevel->priv->grab_op          = PANEL_GRAB_OP_NONE;
	toplevel->priv->grab_is_keyboard = FALSE;

	gtk_grab_remove (widget);

	gdk_pointer_ungrab (time_);
	gdk_keyboard_ungrab (time_);
}

static void
panel_toplevel_cancel_grab_op (PanelToplevel *toplevel,
			       guint32        time_)
{
	panel_toplevel_set_orientation (toplevel, toplevel->priv->orig_orientation);
	panel_toplevel_set_monitor (toplevel, toplevel->priv->orig_monitor);
	panel_toplevel_set_size (toplevel, toplevel->priv->orig_size);
	panel_toplevel_set_x (toplevel,
			      toplevel->priv->orig_x,
			      toplevel->priv->orig_x_centered);
	panel_toplevel_set_y (toplevel,
			      toplevel->priv->orig_y,
			      toplevel->priv->orig_y_centered);
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
	new_y_centered = toplevel->priv->y_centered;

	panel_toplevel_get_monitor_geometry (toplevel, NULL, NULL, &monitor_width, &monitor_height);

	switch (toplevel->priv->grab_op) {
	case PANEL_GRAB_OP_RESIZE_UP:
		new_size = toplevel->priv->drag_offset_y - y;
		new_size = CLAMP (new_size, 0, monitor_height / 4);
		new_y -= (new_size - toplevel->priv->size);
		break;
	case PANEL_GRAB_OP_RESIZE_DOWN:
		new_size = y - toplevel->priv->drag_offset_y;
		new_size = CLAMP (new_size, 0, monitor_height / 4);
		break;
	case PANEL_GRAB_OP_RESIZE_LEFT:
		new_size = toplevel->priv->drag_offset_x - x;
		new_size = CLAMP (new_size, 0, monitor_width / 4);
		new_x -= (new_size - toplevel->priv->size);
		break;
	case PANEL_GRAB_OP_RESIZE_RIGHT:
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

	panel_toplevel_set_x (toplevel, new_x, new_x_centered);
	panel_toplevel_set_y (toplevel, new_y, new_y_centered);
	panel_toplevel_set_size (toplevel, new_size);
}

static void
panel_toplevel_calc_new_orientation (PanelToplevel *toplevel,
				     int            pointer_x,
				     int            pointer_y)
{
	PanelOrientation  new_orientation;
	GdkScreen        *screen;
	int               hborder, vborder;
	int               monitor;
	int               monitor_width, monitor_height;
	int               new_x, new_y;

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));

	monitor = gdk_screen_get_monitor_at_point (screen, pointer_x, pointer_y);

	if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK)
		vborder = hborder = (3 * toplevel->priv->geometry.height) >> 1;
	else
		vborder = hborder = (3 * toplevel->priv->geometry.width)  >> 1;

	new_x = pointer_x - panel_multiscreen_x (screen, monitor);
	new_y = pointer_y - panel_multiscreen_y (screen, monitor);
	monitor_width = panel_multiscreen_width (screen, monitor);
	monitor_height = panel_multiscreen_height (screen, monitor);

	new_orientation = toplevel->priv->orientation;

	switch (toplevel->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
		if (new_y > (monitor_height - hborder))
			new_orientation = PANEL_ORIENTATION_BOTTOM;

		else if (new_y > hborder) {
			if (new_x > (monitor_width - vborder))
				new_orientation = PANEL_ORIENTATION_RIGHT;
			else if (new_x < vborder)
				new_orientation = PANEL_ORIENTATION_LEFT;
		} 
		break;
	case PANEL_ORIENTATION_BOTTOM:
		if (new_y < hborder)
			new_orientation = PANEL_ORIENTATION_TOP;

		else if (new_y < (monitor_height - hborder)) {
			if (new_x > (monitor_width - vborder))
				new_orientation = PANEL_ORIENTATION_RIGHT;
			else if (new_x < vborder)
				new_orientation = PANEL_ORIENTATION_LEFT;
		} 
		break;
	case PANEL_ORIENTATION_LEFT:
		if (new_x > (monitor_width - vborder))
			new_orientation = PANEL_ORIENTATION_RIGHT;

		else if (new_x > vborder) {
			if (new_y > (monitor_height - hborder))
				new_orientation = PANEL_ORIENTATION_BOTTOM;
			else if (new_y < hborder)
				new_orientation = PANEL_ORIENTATION_TOP;
		} 
		break;
	case PANEL_ORIENTATION_RIGHT:
		if (new_x < vborder)
			new_orientation = PANEL_ORIENTATION_LEFT;

		else if (new_x < (monitor_width - vborder)) {
			if (new_y > (monitor_height - hborder))
				new_orientation = PANEL_ORIENTATION_BOTTOM;
			else if (new_y < hborder)
				new_orientation = PANEL_ORIENTATION_TOP;
		}
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	panel_toplevel_set_monitor (toplevel, monitor);
	panel_toplevel_set_orientation (toplevel, new_orientation);
}

static void
panel_toplevel_move_to (PanelToplevel *toplevel,
			int            new_x,
			int            new_y)
{
	GdkScreen        *screen;
	PanelOrientation  new_orientation;
	gboolean          x_centered, y_centered;
	int               screen_width, screen_height;
	int               monitor_width, monitor_height;
	int               width, height;
	int               new_monitor;
	int               x, y;

	screen = panel_toplevel_get_screen_geometry (
			toplevel, &screen_width, &screen_height);

	width  = toplevel->priv->geometry.width;
	height = toplevel->priv->geometry.height;

	new_x = CLAMP (new_x, 0, screen_width  - width);
	new_y = CLAMP (new_y, 0, screen_height - height);

	new_orientation = toplevel->priv->orientation;

	if (new_x <= SNAP_TOLERANCE &&
	    toplevel->priv->orientation & PANEL_VERTICAL_MASK)
		new_orientation = PANEL_ORIENTATION_LEFT;

	else if ((new_x + width) >= (screen_width - SNAP_TOLERANCE) &&
		 toplevel->priv->orientation & PANEL_VERTICAL_MASK)
		new_orientation = PANEL_ORIENTATION_RIGHT;

	if (new_y <= SNAP_TOLERANCE &&
	    toplevel->priv->orientation & PANEL_HORIZONTAL_MASK)
		new_orientation = PANEL_ORIENTATION_TOP;

	else if ((new_y + height) >= (screen_height - SNAP_TOLERANCE) &&
		 toplevel->priv->orientation & PANEL_HORIZONTAL_MASK)
		new_orientation = PANEL_ORIENTATION_BOTTOM;

	new_monitor = gdk_screen_get_monitor_at_point (screen, new_x, new_y);

	panel_toplevel_get_monitor_geometry (
			toplevel, NULL, NULL, &monitor_width, &monitor_height);

	x_centered = toplevel->priv->x_centered;
	y_centered = toplevel->priv->y_centered;

	x = new_x - panel_multiscreen_x (screen, new_monitor);
	y = new_y - panel_multiscreen_y (screen, new_monitor);

	if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK) {
		y_centered = FALSE;
		if (new_y == 0 || new_y == (screen_height - height))
			x_centered = abs (x - ((monitor_width - width) / 2))
								<= SNAP_TOLERANCE;
		else
			x_centered = FALSE;
	} else {
		x_centered = FALSE;
		if (new_x == 0 || new_x == (screen_width - width))
			y_centered = abs (y - ((monitor_height - height) / 2))
								<= SNAP_TOLERANCE;
		else
			y_centered = FALSE;
	}

	if (x_centered)
		x -= (monitor_width  - width) / 2;
	if (y_centered)
		y -= (monitor_height - height) / 2;

	panel_toplevel_set_monitor (toplevel, new_monitor);
	panel_toplevel_set_orientation (toplevel, new_orientation);
	panel_toplevel_set_x (toplevel, x, x_centered);
	panel_toplevel_set_y (toplevel, y, y_centered);
}

static void
panel_toplevel_move_to_pointer (PanelToplevel *toplevel,
				int            pointer_x,
				int            pointer_y)
{
	int new_x, new_y;

	new_x = pointer_x - toplevel->priv->drag_offset_x;
	new_y = pointer_y - toplevel->priv->drag_offset_y;

	panel_toplevel_move_to (toplevel, new_x, new_y);
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
panel_toplevel_warp_pointer_increment (PanelToplevel *toplevel,
				       int            keyval,
				       int            increment)
{
	GdkScreen *screen;
	GdkWindow *root_window;
	int        new_x, new_y;

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));
	root_window = gdk_screen_get_root_window (screen);

	gdk_window_get_pointer (root_window, &new_x, &new_y, NULL);

	switch (keyval) {
	case GDK_Up:
	case GDK_KP_Up:
		new_y -= increment;
		break;
	case GDK_Left:
	case GDK_KP_Left:
		new_x -= increment;
		break;
	case GDK_Down:
	case GDK_KP_Down:
		new_y += increment;
		break;
	case GDK_Right:
	case GDK_KP_Right:
		new_x += increment;
		break;
	default:
		g_assert_not_reached ();
		return FALSE;
	}

	panel_warp_pointer (root_window, new_x, new_y);

	return TRUE;
}

static gboolean
panel_toplevel_move_keyboard_floating (PanelToplevel *toplevel,
				       GdkEventKey   *event)
{
#define SMALL_INCREMENT  1
#define NORMAL_INCREMENT 10

	int increment = NORMAL_INCREMENT;

	if ((event->state & gtk_accelerator_get_default_mod_mask ()) == GDK_SHIFT_MASK)
		increment = SMALL_INCREMENT;

	return panel_toplevel_warp_pointer_increment (
				toplevel, event->keyval, increment);

#undef SMALL_INCREMENT
#undef NORMAL_INCREMENT
}

static gboolean
panel_toplevel_move_keyboard_expanded (PanelToplevel *toplevel,
				       GdkEventKey   *event)
{
	PanelOrientation new_orientation;

	switch (event->keyval) {
	case GDK_Up:
	case GDK_KP_Up:
		new_orientation = PANEL_ORIENTATION_TOP;
		break;
	case GDK_Left:
	case GDK_KP_Left:
		new_orientation = PANEL_ORIENTATION_LEFT;
		break;
	case GDK_Down:
	case GDK_KP_Down:
		new_orientation = PANEL_ORIENTATION_BOTTOM;
		break;
	case GDK_Right:
	case GDK_KP_Right:
		new_orientation = PANEL_ORIENTATION_RIGHT;
		break;
	default:
		g_assert_not_reached ();
		return FALSE;
	}

	panel_toplevel_set_orientation (toplevel, new_orientation);

	return TRUE;
}

static gboolean
panel_toplevel_initial_resize_keypress (PanelToplevel *toplevel,
					GdkEventKey   *event)
{
	PanelGrabOpType grab_op;

	switch (event->keyval) {
	case GDK_Up:
	case GDK_KP_Up:
		if (!(toplevel->priv->orientation & PANEL_HORIZONTAL_MASK))
			return FALSE;
		grab_op = PANEL_GRAB_OP_RESIZE_UP;
		break;
	case GDK_Left:
	case GDK_KP_Left:
		if (!(toplevel->priv->orientation & PANEL_VERTICAL_MASK))
			return FALSE;
		grab_op = PANEL_GRAB_OP_RESIZE_LEFT;
		break;
	case GDK_Down:
	case GDK_KP_Down:
		if (!(toplevel->priv->orientation & PANEL_HORIZONTAL_MASK))
			return FALSE;
		grab_op = PANEL_GRAB_OP_RESIZE_DOWN;
		break;
	case GDK_Right:
	case GDK_KP_Right:
		if (!(toplevel->priv->orientation & PANEL_VERTICAL_MASK))
			return FALSE;
		grab_op = PANEL_GRAB_OP_RESIZE_RIGHT;
		break;
	default:
		g_assert_not_reached ();
		return FALSE;
	}

	panel_toplevel_end_grab_op (toplevel, event->time);
	panel_toplevel_begin_grab_op (toplevel, grab_op, TRUE, event->time);

	return TRUE;
}

static gboolean
panel_toplevel_handle_grab_op_key_event (PanelToplevel *toplevel,
					 GdkEventKey   *event)
{
	gboolean retval = FALSE;

	switch (event->keyval) {
	case GDK_Up:
	case GDK_KP_Up:
	case GDK_Left:
	case GDK_KP_Left:
	case GDK_Down:
	case GDK_KP_Down:
	case GDK_Right:
	case GDK_KP_Right:
		switch (toplevel->priv->grab_op) {
		case PANEL_GRAB_OP_MOVE:
			if (toplevel->priv->expand)
				retval = panel_toplevel_move_keyboard_expanded (
									toplevel, event);
			else
				retval = panel_toplevel_move_keyboard_floating (
									toplevel, event);
			break;
		case PANEL_GRAB_OP_RESIZE:
			retval = panel_toplevel_initial_resize_keypress (toplevel, event);
			break;
		case PANEL_GRAB_OP_RESIZE_UP:
		case PANEL_GRAB_OP_RESIZE_DOWN:
		case PANEL_GRAB_OP_RESIZE_LEFT:
		case PANEL_GRAB_OP_RESIZE_RIGHT:
			retval = panel_toplevel_warp_pointer_increment (
						toplevel, event->keyval, 1);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
		break;
	case GDK_Escape: 
		panel_toplevel_cancel_grab_op (toplevel, event->time);
	case GDK_Return: /* drop through*/
	case GDK_KP_Enter:
	case GDK_space:
	case GDK_KP_Space:
		panel_toplevel_end_grab_op (toplevel, event->time);
		retval = TRUE;
	default: /* drop through*/
		break;
	}

	return retval;
}

static gboolean
panel_toplevel_handle_grab_op_motion_event (PanelToplevel  *toplevel,
					    GdkEventMotion *event)
{
	switch (toplevel->priv->grab_op) {
	case PANEL_GRAB_OP_MOVE:
		if (toplevel->priv->expand)
			panel_toplevel_calc_new_orientation (
					toplevel, event->x_root, event->y_root);

		else if ((event->state & gtk_accelerator_get_default_mod_mask ()) == GDK_CONTROL_MASK)
			panel_toplevel_rotate_to_pointer (
					toplevel, event->x_root, event->y_root);

		else
			panel_toplevel_move_to_pointer (
					toplevel, event->x_root, event->y_root);
		return TRUE;
	case PANEL_GRAB_OP_RESIZE_UP:
	case PANEL_GRAB_OP_RESIZE_DOWN:
	case PANEL_GRAB_OP_RESIZE_LEFT:
	case PANEL_GRAB_OP_RESIZE_RIGHT:
		panel_toplevel_resize_to_pointer (toplevel, event->x_root, event->y_root);
		return TRUE;
	default:
		break;
	}

	return FALSE;
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
			(y > SNAP_TOLERANCE) && (y < (screen_height - toplevel->priv->geometry.height - SNAP_TOLERANCE));
	else
		toplevel->priv->floating =
			(x > SNAP_TOLERANCE) && (x < (screen_width - toplevel->priv->geometry.width - SNAP_TOLERANCE));
}

static void 
panel_toplevel_push_autohide_disabler (PanelToplevel *toplevel)
{
	g_return_if_fail (toplevel != NULL);

	toplevel->priv->n_autohide_disablers++;
}

static void
panel_toplevel_pop_autohide_disabler (PanelToplevel *toplevel)
{
	g_return_if_fail (toplevel != NULL);

	g_return_if_fail (toplevel->priv->n_autohide_disablers > 0);

	toplevel->priv->n_autohide_disablers--;		
}

static gboolean
panel_toplevel_get_autohide_disabled (PanelToplevel *toplevel)
{
	return toplevel->priv->n_autohide_disablers > 0 ? TRUE : FALSE;
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
	AtkObject *obj;
	GtkWidget *arrow;
	int        arrow_size;
	
	button = gtk_button_new ();
	obj = gtk_widget_get_accessible (button);
	atk_object_set_name (obj, _("Hide Panel"));
	GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_DEFAULT);

	gtk_widget_style_get (GTK_WIDGET (toplevel),
			      "arrow-size", &arrow_size,
			      NULL);

	switch (arrow_type) {
	case GTK_ARROW_UP:
		gtk_widget_set_size_request (button, -1, arrow_size);
		break;
	case GTK_ARROW_DOWN:
		gtk_widget_set_size_request (button, -1, arrow_size);
		break;
	case GTK_ARROW_LEFT:
		gtk_widget_set_size_request (button, arrow_size, -1);
		break;
	case GTK_ARROW_RIGHT:
		gtk_widget_set_size_request (button, arrow_size, -1);
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

	if (toplevel->priv->attached) {
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
}

static void
panel_toplevel_update_hide_buttons (PanelToplevel *toplevel)
{
	if (toplevel->priv->buttons_enabled)
		panel_toplevel_update_buttons_showing (toplevel);
	else {
		g_object_set (
			G_OBJECT (toplevel->priv->hide_button_top),
			"visible", toplevel->priv->state == PANEL_STATE_HIDDEN_DOWN,
			NULL);
		g_object_set (
			G_OBJECT (toplevel->priv->hide_button_bottom),
			"visible", toplevel->priv->state == PANEL_STATE_HIDDEN_UP,
			NULL);
		g_object_set (
			G_OBJECT (toplevel->priv->hide_button_left),
			"visible", toplevel->priv->state == PANEL_STATE_HIDDEN_RIGHT,
			NULL);
		g_object_set (
			G_OBJECT (toplevel->priv->hide_button_right),
			"visible", toplevel->priv->state == PANEL_STATE_HIDDEN_LEFT,
			NULL);
	}

	if (toplevel->priv->arrows_enabled) {
		int arrow_size;

		gtk_widget_show (GTK_BIN (toplevel->priv->hide_button_top)->child);
		gtk_widget_show (GTK_BIN (toplevel->priv->hide_button_bottom)->child);
		gtk_widget_show (GTK_BIN (toplevel->priv->hide_button_left)->child);
		gtk_widget_show (GTK_BIN (toplevel->priv->hide_button_right)->child);

		gtk_widget_style_get (GTK_WIDGET (toplevel),
				      "arrow-size", &arrow_size,
				      NULL);

		gtk_widget_set_size_request (toplevel->priv->hide_button_top,
					     -1, arrow_size);
		gtk_widget_set_size_request (toplevel->priv->hide_button_bottom,
					     -1, arrow_size);
		gtk_widget_set_size_request (toplevel->priv->hide_button_left,
					     arrow_size, -1);
		gtk_widget_set_size_request (toplevel->priv->hide_button_right,
					     arrow_size, -1);
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
	GdkDisplay *display;
	GdkScreen  *screen;
	GtkWidget  *widget;
	int         x, y;

	display = gdk_display_get_default ();
	widget  = GTK_WIDGET (toplevel);

	if (!GTK_WIDGET_REALIZED (widget))
		return FALSE;

	screen = NULL;
	x = y = -1;
	gdk_display_get_pointer (display, &screen, &x, &y, NULL);

	if (screen != gtk_window_get_screen (GTK_WINDOW (toplevel)))
		return FALSE;

	if (x == -1 || y == -1)
		return FALSE;

	if (x < toplevel->priv->geometry.x || x >= (toplevel->priv->geometry.x + toplevel->priv->geometry.width) ||
	    y < toplevel->priv->geometry.y || y >= (toplevel->priv->geometry.y + toplevel->priv->geometry.height))
		return FALSE;

	return TRUE;
}

static gboolean
panel_toplevel_update_struts (PanelToplevel *toplevel, gboolean end_of_animation)
{
	PanelOrientation  orientation;
	GdkScreen        *screen;
	gboolean          geometry_changed = FALSE;
	int               strut, strut_start, strut_end;
	int               x, y, width, height;
	int               monitor_x, monitor_y;
	int               monitor_width, monitor_height;

	if (!toplevel->priv->updated_geometry_initial)
		return FALSE;

	if (toplevel->priv->attached) {
		panel_struts_unregister_strut (toplevel);
		panel_struts_set_window_hint (toplevel);
		return FALSE;
	}

	screen = panel_toplevel_get_monitor_geometry (toplevel,
						      &monitor_x,
						      &monitor_y,
						      &monitor_width,
						      &monitor_height);

	if (end_of_animation) {
		x = toplevel->priv->animation_end_x;
		y = toplevel->priv->animation_end_y;
		x += panel_multiscreen_x (screen, toplevel->priv->monitor);
		x += panel_multiscreen_y (screen, toplevel->priv->monitor);
		if (toplevel->priv->animation_end_width != -1)
			width = toplevel->priv->animation_end_width;
		else
			width = toplevel->priv->geometry.width;
		if (toplevel->priv->animation_end_height != -1)
			height = toplevel->priv->animation_end_height;
		else
			height = toplevel->priv->geometry.height;
	} else {
		x      = toplevel->priv->geometry.x;
		y      = toplevel->priv->geometry.y;
		width  = toplevel->priv->geometry.width;
		height = toplevel->priv->geometry.height;
	}

	orientation = toplevel->priv->orientation;

	strut = strut_start = strut_end = 0;

	if (orientation & PANEL_HORIZONTAL_MASK) {
		if (y <= monitor_y) {
			orientation = PANEL_ORIENTATION_TOP;
			strut = y + height - monitor_y;
		} else if (y >= monitor_y + monitor_height - height) {
			orientation = PANEL_ORIENTATION_BOTTOM;
			strut = monitor_y + monitor_height - y;
		}

		if (strut > 0) {
			strut_start = MAX (x, monitor_x);
			strut_end = MIN (x + width, monitor_x + monitor_width) - 1;
		}
	} else {
		if (x <= monitor_x) {
			orientation = PANEL_ORIENTATION_LEFT;
			strut = x + width - monitor_x;
		} else if (x >= monitor_x + monitor_width - width) {
			orientation = PANEL_ORIENTATION_RIGHT;
			strut = monitor_x + monitor_width - x;
		}

		if (strut > 0) {
			strut_start = MAX (y, monitor_y);
			strut_end = MIN (y + height, monitor_y + monitor_height) - 1;

		}
	}

	if (orientation != toplevel->priv->orientation) {
		toplevel->priv->orientation = orientation;
		g_object_notify (G_OBJECT (toplevel), "orientation");
	}

	if (toplevel->priv->auto_hide && strut > 0)
		strut = toplevel->priv->auto_hide_size;

	if (strut > 0)
		geometry_changed = panel_struts_register_strut (toplevel,
								screen,
								toplevel->priv->monitor,
								orientation,
								strut,
								strut_start,
								strut_end);
	else
		panel_struts_unregister_strut (toplevel);

	panel_struts_set_window_hint (toplevel);

	return geometry_changed;
}

void
panel_toplevel_update_edges (PanelToplevel *toplevel)
{
	GtkWidget       *widget;
	PanelFrameEdge   edges;
	PanelFrameEdge   inner_edges;
	PanelFrameEdge   outer_edges;
	PanelBackground *background;
	int              monitor_width, monitor_height;
	int              width, height;
	gboolean         inner_frame = FALSE;

	widget = GTK_WIDGET (toplevel);

	panel_toplevel_get_monitor_geometry (
			toplevel, NULL, NULL, &monitor_width, &monitor_height);

	width  = toplevel->priv->geometry.width;
	height = toplevel->priv->geometry.height;

	edges = PANEL_EDGE_NONE;

	background = &toplevel->priv->panel_widget->background;

	/* We don't want any bevels with a color/image background */
	if (panel_background_effective_type (background) == PANEL_BACK_NONE) {
		if (toplevel->priv->geometry.y > 0)
			edges |= PANEL_EDGE_TOP;

		if (toplevel->priv->geometry.x > 0)
			edges |= PANEL_EDGE_LEFT;

		if (toplevel->priv->geometry.y < (monitor_height - height))
			edges |= PANEL_EDGE_BOTTOM;

		if (toplevel->priv->geometry.x < (monitor_width - width))
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

static const char *
panel_toplevel_construct_description (PanelToplevel *toplevel)
{
	int orientation, type;

	static const char *description[4][4] = {
		{
	/* translators: these string will be shown in MetaCity's switch window
	 * popup when you pass the focus to a panel */
			N_("Top Expanded Edge Panel"),
			N_("Top Centered Panel"),
	     		N_("Top Floating Panel"),
	     		N_("Top Edge Panel"),
		},
		
		{
			N_("Bottom Expanded Edge Panel"),
	     		N_("Bottom Centered Panel"),
	     		N_("Bottom Floating Panel"),
	     		N_("Bottom Edge Panel"),
		},

		{
			N_("Left Expanded Edge Panel"),
			N_("Left Centered Panel"),
			N_("Left Floating Panel"),
			N_("Left Edge Panel"),
		},

		{
			N_("Right Expanded Edge Panel"),
			N_("Right Centered Panel"),
			N_("Right Floating Panel"),
			N_("Right Edge Panel"),
		},
	};

	switch (toplevel->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
		orientation = 0;
		break;
	case PANEL_ORIENTATION_BOTTOM:
		orientation = 1;
		break;
	case PANEL_ORIENTATION_LEFT:
		orientation = 2;
		break;
	case PANEL_ORIENTATION_RIGHT:
		orientation = 3;
		break;
	default:
		orientation = 0;
		g_assert_not_reached ();
		break;
	}

	if (toplevel->priv->expand)
		type = 0;
	else if (toplevel->priv->x_centered ||
		 toplevel->priv->y_centered)
		type = 1;
	else if (toplevel->priv->floating)
		type = 2;
	else
		type = 3;
	
	return description[orientation][type];
}

static void
panel_toplevel_update_description (PanelToplevel *toplevel)
{
	const char *description;

	description = panel_toplevel_construct_description (toplevel);

	if (toplevel->priv->description &&
	    !strcmp (toplevel->priv->description, description))
		return;

	if (toplevel->priv->description)
		g_free (toplevel->priv->description);
	toplevel->priv->description = g_strdup (_(description));

	if (!toplevel->priv->name)
		gtk_window_set_title (GTK_WINDOW (toplevel),
				      toplevel->priv->description);

	panel_a11y_set_atk_name_desc (
		GTK_WIDGET (toplevel->priv->panel_widget),
		toplevel->priv->name ? toplevel->priv->name :
				       _(toplevel->priv->description),
		_(toplevel->priv->description));
}

static void
panel_toplevel_update_attached_position (PanelToplevel *toplevel,
					 gboolean       hidden,
					 int           *x,
					 int           *y,
					 int           *w,
					 int           *h)
{
	PanelOrientation  attach_orientation;
	GdkRectangle      toplevel_box;
	GdkRectangle      parent_box;
	GdkRectangle      attach_box;
	GdkScreen        *screen;
	int               x_origin = 0, y_origin = 0;
	int               monitor_x, monitor_y;
	int               monitor_width, monitor_height;

	if (!GTK_WIDGET_REALIZED (toplevel->priv->attach_toplevel) ||
	    !GTK_WIDGET_REALIZED (toplevel->priv->attach_widget))
		return;

	toplevel_box = toplevel->priv->geometry;
	parent_box   = toplevel->priv->attach_toplevel->priv->geometry;
	attach_box   = GTK_WIDGET (toplevel->priv->attach_widget)->allocation;

	if (attach_box.x != -1) {
		gdk_window_get_origin (GTK_WIDGET (toplevel->priv->attach_widget)->window,
				       &x_origin, &y_origin);

		attach_box.x += x_origin;
		attach_box.y += y_origin;
	} else {
		/* attach_widget isn't allocated. Put the toplevel
		 * off screen.
		 */
		attach_box.x = -toplevel_box.width;
		attach_box.y = -toplevel_box.height;
	}

	attach_orientation = panel_toplevel_get_orientation (
					toplevel->priv->attach_toplevel);

	if (attach_orientation & PANEL_HORIZONTAL_MASK)
		*x = attach_box.x + attach_box.width / 2  - toplevel_box.width  / 2;
	else
		*y = attach_box.y + attach_box.height / 2 - toplevel_box.height / 2;

	switch (attach_orientation) {
	case PANEL_ORIENTATION_TOP:
		*y = parent_box.y;
		if (!hidden)
			*y += parent_box.height;
		else
			*h = parent_box.height;
		break;
	case PANEL_ORIENTATION_BOTTOM:
		*y = parent_box.y;
		if (!hidden)
			*y -= toplevel_box.height;
		else
			*h = parent_box.height;
		break;
	case PANEL_ORIENTATION_LEFT:
		*x = parent_box.x;
		if (!hidden)
			*x += parent_box.width;
		else
			*w = parent_box.width;
		break;
	case PANEL_ORIENTATION_RIGHT:
		*x = parent_box.x;
		if (!hidden)
			*x -= toplevel_box.width;
		else
			*w = parent_box.width;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	screen = panel_toplevel_get_monitor_geometry (toplevel,
						      &monitor_x,
						      &monitor_y,
						      &monitor_width,
						      &monitor_height);
                                                                                
	*x -= monitor_x;
	*y -= monitor_y;

	if (toplevel->priv->orientation & PANEL_VERTICAL_MASK)
		*x = CLAMP (*x, 0, monitor_width  - toplevel->priv->original_width);
	else
		*y = CLAMP (*y, 0, monitor_height - toplevel->priv->original_height);
}

static void
panel_toplevel_update_normal_position (PanelToplevel *toplevel,
				       int           *x,
				       int           *y,
				       int           *w,
				       int           *h)
{
	GtkWidget *widget;
	int        monitor_width, monitor_height;
	int        width, height;

	g_assert (x != NULL && y != NULL);

	if (toplevel->priv->attached) {
		panel_toplevel_update_attached_position (toplevel, FALSE, x, y, w, h);
		return;
	}

	widget = GTK_WIDGET (toplevel);

	panel_toplevel_get_monitor_geometry (
			toplevel, NULL, NULL, &monitor_width, &monitor_height);

	width  = toplevel->priv->original_width;
	height = toplevel->priv->original_height;

	*x = CLAMP (*x, 0, monitor_width  - width);
	*y = CLAMP (*y, 0, monitor_height - height);

	if (*x <= SNAP_TOLERANCE)
		*x = 0;
	else if ((*x + width) >= (monitor_width - SNAP_TOLERANCE))
		*x = monitor_width - width;

	if (*y <= SNAP_TOLERANCE)
		*y = 0;
	else if ((*y + height) >= (monitor_height - SNAP_TOLERANCE))
		*y = monitor_height - height;
}

static void
panel_toplevel_update_auto_hide_position (PanelToplevel *toplevel,
					  int           *x,
					  int           *y,
					  int           *w,
					  int           *h)
{
	int width, height;
	int monitor_width, monitor_height;
	int auto_hide_size;

	g_assert (x != NULL && y != NULL);

	if (toplevel->priv->floating) {
		panel_toplevel_update_normal_position (toplevel, x, y, w, h);
		return;
	}

	panel_toplevel_get_monitor_geometry (
			toplevel, NULL, NULL, &monitor_width, &monitor_height);

	width  = toplevel->priv->original_width;
	height = toplevel->priv->original_height;

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
		*y = - (height - auto_hide_size);
		break;
	case PANEL_ORIENTATION_BOTTOM:
		*y = monitor_height - auto_hide_size;
		break;
	case PANEL_ORIENTATION_LEFT:
		*x = - (width - auto_hide_size);
		break;
	case PANEL_ORIENTATION_RIGHT:
		*x = monitor_width - auto_hide_size;
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

/* FIXME: this is wrong for Xinerama. In the Xinerama case
 *        I think if hiding it requires it to go onto the
 *        next monitor then it should just move it on to
 *        the next monitor and set its state back to normal
 */
static void
panel_toplevel_update_hidden_position (PanelToplevel *toplevel,
				       int           *x,
				       int           *y,
				       int           *w,
				       int           *h)
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
		panel_toplevel_update_attached_position (toplevel, TRUE, x, y, w, h);
		return;
	}

	panel_toplevel_get_monitor_geometry (
			toplevel, NULL, NULL, &monitor_width, &monitor_height);

	width  = toplevel->priv->original_width;
	height = toplevel->priv->original_height;

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

/*
 * This is "almost" like the double sine movement
 * from the original panel except that it uses
 * a cubic (twice again).  I suppose it looks less
 * mathematical now :) -- _v_
 */
static int
get_delta (int  src,
	   int  dest,
	   long start_time,
	   long end_time,
	   long cur_time)
{
	double x, n, d, percentage;

	n = cur_time - start_time;
	d = end_time - start_time;

	if (abs (dest - src) <= 1 || n >= d)
		return dest - src;

	/* The cubic is: p(x) = (-2) x^2 (x-1.5) */
	/* running p(p(x)) to make it more "pronounced",
	 * effectively making it a ninth-degree polynomial */

	x = n/d;
	x = -2 * (x*x) * (x-1.5);
	/* run it again */
	percentage = -2 * (x*x) * (x-1.5);

	percentage = CLAMP (percentage, 0.0, 1.0);

	return ((dest - src) * percentage);
}

static void
panel_toplevel_update_animating_position (PanelToplevel *toplevel)
{
	GdkScreen *screen;
	GTimeVal   time_val;
	long       current_time;
	int        deltax, deltay, deltaw = 0, deltah = 0;
	int        monitor_offset_x, monitor_offset_y;

	g_get_current_time (&time_val);

	current_time =  (time_val.tv_sec - toplevel->priv->animation_start_time.tv_sec)
					* 1000000 + time_val.tv_usec;

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));

	monitor_offset_x = panel_multiscreen_x (screen, toplevel->priv->monitor);
	monitor_offset_y = panel_multiscreen_y (screen, toplevel->priv->monitor);

	if (toplevel->priv->animation_end_width != -1)
		deltaw = get_delta (toplevel->priv->geometry.width,
				    toplevel->priv->animation_end_width,
				    toplevel->priv->animation_start_time.tv_usec,
				    toplevel->priv->animation_end_time,
				    current_time);

	if (toplevel->priv->animation_end_height != -1)
		deltah = get_delta (toplevel->priv->geometry.height,
				    toplevel->priv->animation_end_height,
				    toplevel->priv->animation_start_time.tv_usec,
				    toplevel->priv->animation_end_time,
				    current_time);

	deltax = get_delta (toplevel->priv->geometry.x - monitor_offset_x,
			    toplevel->priv->animation_end_x,
			    toplevel->priv->animation_start_time.tv_usec,
			    toplevel->priv->animation_end_time,
			    current_time);

	deltay = get_delta (toplevel->priv->geometry.y - monitor_offset_y,
			    toplevel->priv->animation_end_y,
			    toplevel->priv->animation_start_time.tv_usec,
			    toplevel->priv->animation_end_time,
			    current_time);

	if (deltaw != 0 && abs (deltaw) > abs (deltax))
		deltax = deltaw;
	if (deltah != 0 && abs (deltah) > abs (deltay))
		deltay = deltah;

	toplevel->priv->geometry.x += deltax;
	toplevel->priv->geometry.y += deltay;

	toplevel->priv->geometry.width += deltaw;
	toplevel->priv->geometry.height += deltah;

	if (toplevel->priv->geometry.x - monitor_offset_x == toplevel->priv->animation_end_x &&
	    toplevel->priv->geometry.y - monitor_offset_y == toplevel->priv->animation_end_y) {
		toplevel->priv->animating = FALSE;

		if (toplevel->priv->attached && panel_toplevel_get_is_hidden (toplevel))
			gtk_widget_unmap (GTK_WIDGET (toplevel));
		else
			gtk_widget_queue_resize (GTK_WIDGET (toplevel));

		if (toplevel->priv->state == PANEL_STATE_NORMAL)
			g_signal_emit (toplevel, toplevel_signals [UNHIDE_SIGNAL], 0);
	}
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
		toplevel, NULL, NULL, &monitor_width, &monitor_height);

	monitor_x = panel_multiscreen_x (screen, toplevel->priv->monitor);
	monitor_y = panel_multiscreen_y (screen, toplevel->priv->monitor);

	x = monitor_x + toplevel->priv->x;
	y = monitor_y + toplevel->priv->y;

	switch (toplevel->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
		x = monitor_x;
		y = monitor_y;
		break;
	case PANEL_ORIENTATION_LEFT:
		x = monitor_x;
		y = monitor_y;
		break;
	case PANEL_ORIENTATION_BOTTOM:
		x = monitor_x;
		y = monitor_y + monitor_height - toplevel->priv->geometry.height;
		break;
	case PANEL_ORIENTATION_RIGHT:
		x = monitor_x + monitor_width - toplevel->priv->geometry.width;
		y = monitor_y;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	monitor = gdk_screen_get_monitor_at_point (screen, x, y);

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
	int        w, h;
	int        screen_width, screen_height;
	int        monitor_width, monitor_height;

	widget = GTK_WIDGET (toplevel);

	screen = panel_toplevel_get_screen_geometry (
			toplevel, &screen_width, &screen_height);

	panel_toplevel_get_monitor_geometry (
			toplevel, NULL, NULL, &monitor_width, &monitor_height);

	if (toplevel->priv->animating) {
		panel_toplevel_update_animating_position (toplevel);
		return;
	}


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

	w = h = -1;

	if (toplevel->priv->state == PANEL_STATE_NORMAL)
		panel_toplevel_update_normal_position (toplevel, &x, &y, &w, &h);

	else if (toplevel->priv->state == PANEL_STATE_AUTO_HIDDEN)
		panel_toplevel_update_auto_hide_position (toplevel, &x, &y, &w, &h);

	else 
		panel_toplevel_update_hidden_position (toplevel, &x, &y, &w, &h);

	x += panel_multiscreen_x (screen, toplevel->priv->monitor);
	y += panel_multiscreen_y (screen, toplevel->priv->monitor);

	toplevel->priv->geometry.x = x;
	toplevel->priv->geometry.y = y;

	if (w != -1)
		toplevel->priv->geometry.width = w;
	if (h != -1)
		toplevel->priv->geometry.height = h;

	panel_toplevel_update_struts (toplevel, FALSE);
	if (toplevel->priv->state == PANEL_STATE_NORMAL ||
	    toplevel->priv->state == PANEL_STATE_AUTO_HIDDEN) {
		panel_struts_update_toplevel_geometry (toplevel,
						       &toplevel->priv->geometry.x,
						       &toplevel->priv->geometry.y,
						       &toplevel->priv->geometry.width,
						       &toplevel->priv->geometry.height);
	} else {
		panel_struts_update_toplevel_geometry (toplevel,
						       &toplevel->priv->geometry.x,
						       &toplevel->priv->geometry.y,
						       NULL, NULL);
	}

	panel_toplevel_update_edges (toplevel);
	panel_toplevel_update_description (toplevel);
}

static int
calculate_minimum_height (GtkWidget        *widget,
			  PanelOrientation  orientation)
{
	PangoContext     *context;
	PangoFontMetrics *metrics;
	int               focus_width = 0;
	int               focus_pad = 0;
	int               ascent;
	int               descent;
	int               thickness;
  
	context = gtk_widget_get_pango_context (widget);
	metrics = pango_context_get_metrics (context,
					     widget->style->font_desc,
					     pango_context_get_language (context));
  
	ascent  = pango_font_metrics_get_ascent  (metrics);
	descent = pango_font_metrics_get_descent (metrics);
  
	pango_font_metrics_unref (metrics);

	gtk_widget_style_get (widget,
			      "focus-line-width", &focus_width,
			      "focus-padding", &focus_pad,
			      NULL);

	thickness = orientation & PANEL_HORIZONTAL_MASK ?
		widget->style->ythickness :
		widget->style->xthickness;

	return PANGO_PIXELS (ascent + descent) + 2 * (focus_width + focus_pad + thickness);
}

static void
panel_toplevel_update_size (PanelToplevel  *toplevel,
			    GtkRequisition *requisition)
{
	GtkWidget *widget;
	int        monitor_width, monitor_height;
	int        width, height;
	int        minimum_height;

	if (toplevel->priv->animating)
		return;

	widget = GTK_WIDGET (toplevel);

	panel_toplevel_get_monitor_geometry (
			toplevel, NULL, NULL, &monitor_width, &monitor_height);

	width  = requisition->width;
	height = requisition->height;

	minimum_height = calculate_minimum_height (GTK_WIDGET (toplevel),
						   toplevel->priv->orientation);

	if (toplevel->priv->orientation & PANEL_HORIZONTAL_MASK) {
		height = MAX (MAX (height, toplevel->priv->size), minimum_height);

		if (toplevel->priv->expand)
			width  = monitor_width;
		else if (!toplevel->priv->buttons_enabled)
			width = MAX (MINIMUM_WIDTH, width + 2 * HANDLE_SIZE);
		else
			width  = MAX (MINIMUM_WIDTH, width);
	} else {
		width = MAX (MAX (width, toplevel->priv->size), minimum_height);

		if (toplevel->priv->expand)
			height = monitor_height;
		else if (!toplevel->priv->buttons_enabled)
			height = MAX (MINIMUM_WIDTH, height + 2 * HANDLE_SIZE);
		else
			height = MAX (MINIMUM_WIDTH, height);
	}

	if (toplevel->priv->edges & PANEL_EDGE_TOP)
		height += widget->style->ythickness;
	if (toplevel->priv->edges & PANEL_EDGE_BOTTOM)
		height += widget->style->ythickness;
	if (toplevel->priv->edges & PANEL_EDGE_LEFT)
		width += widget->style->ythickness;
	if (toplevel->priv->edges & PANEL_EDGE_RIGHT)
		width += widget->style->ythickness;

	toplevel->priv->geometry.width  = CLAMP (width,  0, monitor_width);
	toplevel->priv->geometry.height = CLAMP (height, 0, monitor_height);
	toplevel->priv->original_width  = toplevel->priv->geometry.width;
	toplevel->priv->original_height = toplevel->priv->geometry.height;
}

static void
panel_toplevel_update_geometry (PanelToplevel  *toplevel,
				GtkRequisition *requisition)
{
	toplevel->priv->updated_geometry_initial = TRUE;
	panel_toplevel_update_size (toplevel, requisition);
	panel_toplevel_update_position (toplevel);
}

static void
panel_toplevel_attach_widget_destroyed (PanelToplevel *toplevel)
{
	panel_toplevel_detach (toplevel);
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
panel_toplevel_attach_widget_parent_set (PanelToplevel *toplevel,
					 GtkWidget     *previous_parent,
					 GtkWidget     *attach_widget)
{
	GtkWidget *panel_widget;

	panel_widget = GTK_WIDGET (attach_widget)->parent;
	if (!panel_widget)
		return;

	g_assert (PANEL_IS_WIDGET (panel_widget));

	toplevel->priv->attach_toplevel = PANEL_WIDGET (panel_widget)->toplevel;
	panel_toplevel_update_attach_orientation (toplevel);
	gtk_widget_queue_resize (GTK_WIDGET (toplevel));
}

static void
panel_toplevel_attach_toplevel_hiding (PanelToplevel *toplevel)
{
	if (!panel_toplevel_get_is_hidden (toplevel)) {
		panel_toplevel_hide (toplevel, FALSE, -1);

		toplevel->priv->attach_hidden = TRUE;
	}
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

	for (i = 0; i < N_ATTACH_TOPLEVEL_SIGNALS; i++) {
		if (!toplevel->priv->attach_toplevel_signals [i])
			continue;

		g_signal_handler_disconnect (
			toplevel->priv->attach_toplevel,
			toplevel->priv->attach_toplevel_signals [i]);
		toplevel->priv->attach_toplevel_signals [i] = 0;
	}

	for (i = 0; i < N_ATTACH_WIDGET_SIGNALS; i++) {
		if (!toplevel->priv->attach_widget_signals [i])
			continue;
		
		g_signal_handler_disconnect (
			toplevel->priv->attach_widget,
			toplevel->priv->attach_widget_signals [i]);
		toplevel->priv->attach_widget_signals [i] = 0;
	}
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
	signals [i++] = g_signal_connect_swapped (
		toplevel->priv->attach_widget, "parent-set",
		G_CALLBACK (panel_toplevel_attach_widget_parent_set), toplevel);
	signals [i++] = g_signal_connect_swapped (
		toplevel->priv->attach_widget, "show",
		G_CALLBACK (gtk_widget_show), toplevel);
	signals [i++] = g_signal_connect_swapped (
		toplevel->priv->attach_widget, "hide",
		G_CALLBACK (gtk_widget_hide), toplevel);

	g_assert (i == N_ATTACH_WIDGET_SIGNALS);
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
	
	panel_toplevel_reverse_arrows (toplevel);
	panel_toplevel_set_expand (toplevel, FALSE);
	panel_toplevel_update_attach_orientation (toplevel);
	panel_toplevel_update_hide_buttons (toplevel);

	if (toplevel->priv->state == PANEL_STATE_NORMAL)
		panel_toplevel_push_autohide_disabler (toplevel->priv->attach_toplevel);

	gtk_widget_queue_resize (GTK_WIDGET (toplevel));
}

void
panel_toplevel_detach (PanelToplevel *toplevel)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (!toplevel->priv->attached)
		return;

	if (toplevel->priv->state == PANEL_STATE_NORMAL)
		panel_toplevel_pop_autohide_disabler (toplevel->priv->attach_toplevel);

	panel_toplevel_disconnect_attached (toplevel);
	
	panel_toplevel_reverse_arrows (toplevel);

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

PanelToplevel *
panel_toplevel_get_attach_toplevel (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), NULL);

	return toplevel->priv->attach_toplevel;
}

GtkWidget *
panel_toplevel_get_attach_widget (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), NULL);

	return toplevel->priv->attach_widget;
}

static gboolean
panel_toplevel_popup_panel_menu (PanelToplevel *toplevel)
{
	gboolean retval = FALSE;

	g_signal_emit_by_name (toplevel, "popup_menu", &retval);

	return retval;
}

static gboolean
panel_toplevel_toggle_expand (PanelToplevel *toplevel)
{
	panel_toplevel_set_expand (toplevel, !toplevel->priv->expand);

	return TRUE;
}

static gboolean
panel_toplevel_expand (PanelToplevel *toplevel)
{
	panel_toplevel_set_expand (toplevel, TRUE);

	return TRUE;
}

static gboolean
panel_toplevel_unexpand (PanelToplevel *toplevel)
{
	panel_toplevel_set_expand (toplevel, FALSE);

	return TRUE;
}

static gboolean
panel_toplevel_toggle_hidden (PanelToplevel *toplevel)
{
	if (toplevel->priv->state == PANEL_STATE_NORMAL)
		panel_toplevel_hide (toplevel, toplevel->priv->auto_hide, -1);
	else
		panel_toplevel_unhide (toplevel);

	return FALSE;
}

static gboolean
panel_toplevel_begin_move (PanelToplevel *toplevel)
{
	if (toplevel->priv->grab_op != PANEL_GRAB_OP_NONE)
		return FALSE;

	panel_toplevel_begin_grab_op (
			toplevel, PANEL_GRAB_OP_MOVE, TRUE, GDK_CURRENT_TIME);

	return TRUE;
}

static gboolean
panel_toplevel_begin_resize (PanelToplevel *toplevel)
{
	if (toplevel->priv->grab_op != PANEL_GRAB_OP_NONE)
		return FALSE;

	panel_toplevel_begin_grab_op (
			toplevel, PANEL_GRAB_OP_RESIZE, TRUE, GDK_CURRENT_TIME);

	return TRUE;
}

static void
panel_toplevel_move_resize_window (PanelToplevel *toplevel,
				   gboolean       move,
				   gboolean       resize)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (toplevel);

	g_assert (GTK_WIDGET_REALIZED (widget));

	if (move && resize)
		gdk_window_move_resize (widget->window,
					toplevel->priv->geometry.x,
					toplevel->priv->geometry.y,
					toplevel->priv->geometry.width,
					toplevel->priv->geometry.height);
	else if (move)
		gdk_window_move (widget->window,
				 toplevel->priv->geometry.x,
				 toplevel->priv->geometry.y);
	else if (resize)
		gdk_window_resize (widget->window,
				   toplevel->priv->geometry.width,
				   toplevel->priv->geometry.height);
}

static void
panel_toplevel_realize (GtkWidget *widget)
{
	PanelToplevel *toplevel = (PanelToplevel *) widget;

	gtk_window_set_decorated (GTK_WINDOW (widget), FALSE);
	gtk_window_stick (GTK_WINDOW (widget));

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		GTK_WIDGET_CLASS (parent_class)->realize (widget);

	panel_struts_set_window_hint (toplevel);
	panel_xutils_set_window_type (widget->window, PANEL_XUTILS_TYPE_DOCK);
	xstuff_set_no_group (widget->window);

	gdk_window_set_geometry_hints (widget->window, NULL, GDK_HINT_POS);

	panel_toplevel_move_resize_window (toplevel, TRUE, TRUE);
}

static void
panel_toplevel_disconnect_timeouts (PanelToplevel *toplevel)
{
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
	PanelToplevel *toplevel = (PanelToplevel *) widget;
	
	if (toplevel->priv->attached) {
		panel_toplevel_disconnect_attached (toplevel);
		toplevel->priv->attached = FALSE;
	}

	panel_toplevel_disconnect_timeouts (toplevel);

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
	GtkBin        *bin;
	GdkRectangle   old_geometry;
	int            position_changed = FALSE;
	int            size_changed = FALSE;

	toplevel = PANEL_TOPLEVEL (widget);
	bin = GTK_BIN (widget);

	if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
		gtk_widget_size_request (bin->child, requisition);

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

	panel_toplevel_move_resize_window (toplevel, position_changed, size_changed);
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

static gboolean
panel_toplevel_button_press_event (GtkWidget      *widget,
				   GdkEventButton *event)
{
	PanelToplevel *toplevel;
	GtkWidget     *event_widget;

	g_return_val_if_fail (PANEL_IS_TOPLEVEL (widget), FALSE);

	toplevel = PANEL_TOPLEVEL (widget);

	if (event->button != 1 && event->button != 2)
		return FALSE;

	if (toplevel->priv->animating)
		return FALSE;

	gdk_window_get_user_data (event->window, (gpointer)&event_widget);
	g_assert (GTK_IS_WIDGET (event_widget));
	gtk_widget_translate_coordinates (event_widget,
					  widget,
					  event->x,
					  event->y,
					  &toplevel->priv->drag_offset_x,
					  &toplevel->priv->drag_offset_y);

	panel_toplevel_begin_grab_op (toplevel, PANEL_GRAB_OP_MOVE, FALSE, event->time);

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

	if (toplevel->priv->grab_op == PANEL_GRAB_OP_NONE)
		return FALSE;

	if (toplevel->priv->grab_is_keyboard)
		return FALSE;

	panel_toplevel_end_grab_op (toplevel, event->time);

	return TRUE;
}

static gboolean
panel_toplevel_key_press_event (GtkWidget   *widget,
				GdkEventKey *event)
{
	PanelToplevel *toplevel = (PanelToplevel *) widget;

	if (toplevel->priv->grab_op != PANEL_GRAB_OP_NONE &&
	    panel_toplevel_handle_grab_op_key_event (toplevel, event))
		return TRUE;

	if (GTK_WIDGET_CLASS (parent_class)->key_press_event)
		return GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);

	return FALSE;
}

static gboolean
panel_toplevel_motion_notify_event (GtkWidget      *widget,
				    GdkEventMotion *event)
{
	if (gdk_event_get_screen ((GdkEvent *)event) == 
	    gtk_window_get_screen (GTK_WINDOW (widget)))
		return panel_toplevel_handle_grab_op_motion_event (
				PANEL_TOPLEVEL (widget), event);
	else
		return FALSE;
}

static gboolean
panel_toplevel_animation_timeout (PanelToplevel *toplevel)
{
	gtk_widget_queue_resize (GTK_WIDGET (toplevel));

	if (!toplevel->priv->animating) {
		toplevel->priv->animation_end_x              = 0xdead;
		toplevel->priv->animation_end_y              = 0xdead;
		toplevel->priv->animation_end_width          = 0xdead;
		toplevel->priv->animation_end_height         = 0xdead;
		toplevel->priv->animation_start_time.tv_sec  = 0xdead;
		toplevel->priv->animation_start_time.tv_usec = 0xdead;
		toplevel->priv->animation_end_time           = 0xdead;
		toplevel->priv->animation_timeout            = 0;
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
	int        deltax, deltay, deltaw = 0, deltah = 0;
	int        cur_x = -1, cur_y = -1;
	long       t1, t2;

	screen = panel_toplevel_get_monitor_geometry (
				toplevel, NULL, NULL, &monitor_width, &monitor_height);

	toplevel->priv->animation_end_x      = toplevel->priv->x;
	toplevel->priv->animation_end_y      = toplevel->priv->y;
	toplevel->priv->animation_end_width  = -1;
	toplevel->priv->animation_end_height = -1;

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
						       &toplevel->priv->animation_end_y,
						       &toplevel->priv->animation_end_width,
						       &toplevel->priv->animation_end_height);

	else if (toplevel->priv->state == PANEL_STATE_AUTO_HIDDEN)
		panel_toplevel_update_auto_hide_position (toplevel,
							  &toplevel->priv->animation_end_x,
							  &toplevel->priv->animation_end_y,
							  &toplevel->priv->animation_end_width,
							  &toplevel->priv->animation_end_height);
	else
		panel_toplevel_update_hidden_position (toplevel,
						       &toplevel->priv->animation_end_x,
						       &toplevel->priv->animation_end_y,
						       &toplevel->priv->animation_end_width,
						       &toplevel->priv->animation_end_height);
	
	panel_toplevel_update_struts (toplevel, TRUE);
	panel_struts_update_toplevel_geometry (toplevel,
					       &toplevel->priv->animation_end_x,
					       &toplevel->priv->animation_end_y,
					       &toplevel->priv->animation_end_width,
					       &toplevel->priv->animation_end_height);
	panel_toplevel_update_struts (toplevel, FALSE);

	gdk_window_get_origin (GTK_WIDGET (toplevel)->window, &cur_x, &cur_y);

	cur_x -= panel_multiscreen_x (screen, toplevel->priv->monitor);
	cur_y -= panel_multiscreen_y (screen, toplevel->priv->monitor);

	deltax = toplevel->priv->animation_end_x - cur_x;
	deltay = toplevel->priv->animation_end_y - cur_y;

	if (toplevel->priv->animation_end_width != -1)
		deltaw = toplevel->priv->animation_end_width -
			GTK_WIDGET (toplevel)->requisition.width;
	
	if (toplevel->priv->animation_end_height != -1)
		deltah = toplevel->priv->animation_end_height -
			GTK_WIDGET (toplevel)->requisition.height;

	if (deltax == 0 && deltay == 0 && deltaw == 0 && deltah == 0) {
		toplevel->priv->animation_end_x      = -1;
		toplevel->priv->animation_end_y      = -1;
		toplevel->priv->animation_end_width  = -1;
		toplevel->priv->animation_end_height = -1;
		return;
	}

	if (toplevel->priv->attached) {
		/* Re-map unmapped attached toplevels */
		if (!GTK_WIDGET_MAPPED (toplevel))
			gtk_widget_map (GTK_WIDGET (toplevel));

		gtk_window_present (GTK_WINDOW (toplevel->priv->attach_toplevel));
	}

	g_get_current_time (&toplevel->priv->animation_start_time);

	t1 = panel_toplevel_get_animation_time (toplevel, deltax);
	t2 = panel_toplevel_get_animation_time (toplevel, deltay);
	
	t1 = MAX (t1, t2);
	
	t2 = panel_toplevel_get_animation_time (toplevel, deltaw);
	
	t1 = MAX (t1, t2);

	t2 = panel_toplevel_get_animation_time (toplevel, deltah);
	
	toplevel->priv->animation_end_time = MAX (t1, t2);

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

	if (toplevel->priv->attach_toplevel)
		panel_toplevel_pop_autohide_disabler (toplevel->priv->attach_toplevel);

	if (auto_hide)
		toplevel->priv->state = PANEL_STATE_AUTO_HIDDEN;
	else {
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

		panel_toplevel_update_hide_buttons (toplevel);
	}

	if (toplevel->priv->animate && GTK_WIDGET_REALIZED (toplevel))
		panel_toplevel_start_animation (toplevel);
	else if (toplevel->priv->attached)
		gtk_widget_hide (GTK_WIDGET (toplevel));

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

	panel_toplevel_update_hide_buttons (toplevel);

	if (toplevel->priv->attach_toplevel)
		panel_toplevel_push_autohide_disabler (toplevel->priv->attach_toplevel);

	if (toplevel->priv->animate && GTK_WIDGET_REALIZED (toplevel))
		panel_toplevel_start_animation (toplevel);
	else if (toplevel->priv->attached)
		gtk_widget_show (GTK_WIDGET (toplevel));

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

	if (!panel_toplevel_contains_pointer (toplevel) &&
	    toplevel->priv->auto_hide) {
		toplevel->priv->unhide_timeout = 0;
		return FALSE;
	}

	panel_toplevel_unhide (toplevel);

	return FALSE;
}

void
panel_toplevel_block_auto_hide (PanelToplevel *toplevel)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	toplevel->priv->block_auto_hide++;
}

void
panel_toplevel_unblock_auto_hide (PanelToplevel *toplevel)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));
	g_return_if_fail (toplevel->priv->block_auto_hide > 0);

	toplevel->priv->block_auto_hide--;

	panel_toplevel_queue_auto_hide (toplevel);
}

void
panel_toplevel_queue_auto_hide (PanelToplevel *toplevel)
{
	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	if (!toplevel->priv->auto_hide ||
	    panel_toplevel_contains_pointer (toplevel) ||
	    panel_toplevel_get_autohide_disabled (toplevel))
	  return;

	if (toplevel->priv->unhide_timeout)
		g_source_remove (toplevel->priv->unhide_timeout);
	toplevel->priv->unhide_timeout = 0;

	if (toplevel->priv->hide_timeout ||
	    toplevel->priv->state != PANEL_STATE_NORMAL)
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

	if (toplevel->priv->auto_hide && event->detail != GDK_NOTIFY_INFERIOR)
		panel_toplevel_queue_auto_unhide (toplevel);

	if (GTK_WIDGET_CLASS (parent_class)->enter_notify_event)
		return GTK_WIDGET_CLASS (parent_class)->enter_notify_event (widget, event);

	return FALSE;
}

static gboolean
panel_toplevel_leave_notify_event (GtkWidget        *widget,
				   GdkEventCrossing *event)
{
	PanelToplevel *toplevel;

	g_return_val_if_fail (PANEL_IS_TOPLEVEL (widget), FALSE);

	toplevel = PANEL_TOPLEVEL (widget);

	if (toplevel->priv->auto_hide && event->detail != GDK_NOTIFY_INFERIOR)
		panel_toplevel_queue_auto_hide (toplevel);

	if (GTK_WIDGET_CLASS (parent_class)->leave_notify_event)
		return GTK_WIDGET_CLASS (parent_class)->leave_notify_event (widget, event);

	return FALSE;
}

static gboolean
panel_toplevel_focus_in_event (GtkWidget     *widget,
			       GdkEventFocus *event)
{
	PanelToplevel *toplevel = PANEL_TOPLEVEL (widget);
	
	if (toplevel->priv->state == PANEL_STATE_AUTO_HIDDEN)
		panel_toplevel_unhide (toplevel);

	if (GTK_WIDGET_CLASS (parent_class)->focus_in_event)
		return GTK_WIDGET_CLASS (parent_class)->focus_in_event (widget, event);

	return FALSE;
}

static gboolean
panel_toplevel_focus_out_event (GtkWidget     *widget,
				GdkEventFocus *event)
{
	PanelToplevel *toplevel = PANEL_TOPLEVEL (widget);

	/* It appears that sometimes we don't get a leave notify event,
	   but just a focus in/out, so queue the autohide in that case.
	   If the pointer is inside the panel then obviously we won't hide */
	if (toplevel->priv->auto_hide)
		panel_toplevel_queue_auto_hide (toplevel);

	if (GTK_WIDGET_CLASS (parent_class)->focus_out_event)
		return GTK_WIDGET_CLASS (parent_class)->focus_out_event (widget, event);

	return FALSE;
}

static void
panel_toplevel_style_set (GtkWidget *widget,
			  GtkStyle  *previous_style)
{
	panel_toplevel_update_hide_buttons (PANEL_TOPLEVEL (widget));

	if (GTK_WIDGET_CLASS (parent_class)->style_set)
		GTK_WIDGET_CLASS (parent_class)->style_set (widget, previous_style);
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
		panel_toplevel_set_x (toplevel,
				      g_value_get_int (value),
				      toplevel->priv->x_centered);
		break;
	case PROP_X_CENTERED:
		panel_toplevel_set_x (toplevel,
				      toplevel->priv->x,
				      g_value_get_boolean (value));
		break;
	case PROP_Y:
		panel_toplevel_set_y (toplevel,
				      g_value_get_int (value),
				      toplevel->priv->y_centered);
		break;
	case PROP_Y_CENTERED:
		panel_toplevel_set_y (toplevel,
				      toplevel->priv->y,
				      g_value_get_boolean (value));
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

	panel_struts_unregister_strut (toplevel);

	toplevel_list = g_slist_remove (toplevel_list, toplevel);

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
	widget_class->key_press_event      = panel_toplevel_key_press_event;
	widget_class->motion_notify_event  = panel_toplevel_motion_notify_event;
	widget_class->enter_notify_event   = panel_toplevel_enter_notify_event;
	widget_class->leave_notify_event   = panel_toplevel_leave_notify_event;
	widget_class->screen_changed       = panel_toplevel_screen_changed;
	widget_class->focus_in_event       = panel_toplevel_focus_in_event;
	widget_class->focus_out_event      = panel_toplevel_focus_out_event;
	widget_class->style_set            = panel_toplevel_style_set;

	container_class->check_resize = panel_toplevel_check_resize;

	klass->hiding           = NULL;
	klass->unhiding         = NULL;
	klass->popup_panel_menu = panel_toplevel_popup_panel_menu;
	klass->toggle_expand    = panel_toplevel_toggle_expand;
	klass->expand           = panel_toplevel_expand;
	klass->unexpand         = panel_toplevel_unexpand;
	klass->toggle_hidden    = panel_toplevel_toggle_hidden;
	klass->begin_move       = panel_toplevel_begin_move;
	klass->begin_resize     = panel_toplevel_begin_resize;

	g_type_class_add_private (klass, sizeof (PanelToplevelPrivate));

	g_object_class_install_property (
		gobject_class,
		PROP_NAME,
		g_param_spec_string (
			"name",
			"Name",
			"The name of this panel",
			NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_EXPAND,
		g_param_spec_boolean (
			"expand",
			"Expand",
			"Expand to take up the full monitor width/height",
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_ORIENTATION,
		g_param_spec_enum (
			"orientation",
			"Orientation",
			"The orientation of the panel",
			PANEL_TYPE_ORIENTATION,
			PANEL_ORIENTATION_TOP,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_SIZE,
		g_param_spec_int (
			"size",
			"Size",
			"The height (or width when vertical) of the panel",
			0,
			G_MAXINT,
			DEFAULT_SIZE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));


	g_object_class_install_property (
		gobject_class,
		PROP_X,
		g_param_spec_int (
			"x",
			"X position",
			"The X position of the panel",
			0,
			G_MAXINT,
			0,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_X_CENTERED,
		g_param_spec_boolean (
			"x-centered",
			"X centered",
			"The x co-ordinate is relative to center screen",
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_Y,
		g_param_spec_int (
			"y",
			"Y position",
			"The Y position of the panel",
			0,
			G_MAXINT,
			0,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_Y_CENTERED,
		g_param_spec_boolean (
			"y-centered",
			"Y centered",
			"The y co-ordinate is relative to center screen",
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_MONITOR,
		g_param_spec_int (
			"monitor",
			"Xinerama monitor",
			"The monitor (in terms of Xinerama) which the panel is on",
			0,
			G_MAXINT,
			0,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_AUTOHIDE,
		g_param_spec_boolean (
			"auto-hide",
			"Auto hide",
			"Automatically hide the panel when the mouse leaves the panel",
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_HIDE_DELAY,
		g_param_spec_int (
			"hide-delay",
			"Hide delay",
			"The number of milliseconds to delay before automatically hiding",
			0,
			G_MAXINT,
			DEFAULT_HIDE_DELAY,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_UNHIDE_DELAY,
		g_param_spec_int (
			"unhide-delay",
			"Un-hide delay",
			"The number of milliseconds to delay before automatically un-hiding",
			0,
			G_MAXINT,
			DEFAULT_UNHIDE_DELAY,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_AUTOHIDE_SIZE,
		g_param_spec_int (
			"auto-hide-size",
			"Auto-hide size",
			"The number of pixels visible when the panel has been automatically hidden",
			1,
			G_MAXINT,
			DEFAULT_AUTO_HIDE_SIZE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_ANIMATE,
		g_param_spec_boolean (
			"animate",
			"Animate",
			"Enable hiding/showing animations",
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_ANIMATION_SPEED,
		g_param_spec_enum (
			"animation-speed",
			"Animation Speed",
			"The speed at which to animate panel hiding/showing",
			PANEL_TYPE_ANIMATION_SPEED,
			PANEL_ANIMATION_MEDIUM,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_BUTTONS_ENABLED,
		g_param_spec_boolean (
			"buttons-enabled",
			"Buttons Enabled",
			"Enable hide/show buttons",
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_ARROWS_ENABLED,
		g_param_spec_boolean (
			"arrows-enabled",
			"Arrows Enabled",
			"Enable arrows on hide/show buttons",
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	gtk_widget_class_install_style_property (
		widget_class,
		g_param_spec_int (
			"arrow-size",
			"Arrow Size",
			"The size of the arrows on the hide/show buttons",
			0,
			G_MAXINT,
			DEFAULT_ARROW_SIZE,
			G_PARAM_READABLE));

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

	toplevel_signals [TOGGLE_EXPAND_SIGNAL] =
		g_signal_new ("toggle-expand",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (PanelToplevelClass, toggle_expand),
			      NULL,
			      NULL,
			      panel_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN,
			      0);

	toplevel_signals [EXPAND_SIGNAL] =
		g_signal_new ("expand",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (PanelToplevelClass, expand),
			      NULL,
			      NULL,
			      panel_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN,
			      0);

	toplevel_signals [UNEXPAND_SIGNAL] =
		g_signal_new ("unexpand",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (PanelToplevelClass, unexpand),
			      NULL,
			      NULL,
			      panel_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN,
			      0);

	toplevel_signals [TOGGLE_HIDDEN_SIGNAL] =
		g_signal_new ("toggle-hidden",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (PanelToplevelClass, toggle_hidden),
			      NULL,
			      NULL,
			      panel_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN,
			      0);

	toplevel_signals [BEGIN_MOVE_SIGNAL] =
		g_signal_new ("begin-move",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (PanelToplevelClass, begin_move),
			      NULL,
			      NULL,
			      panel_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN,
			      0);

	toplevel_signals [BEGIN_RESIZE_SIGNAL] =
		g_signal_new ("begin-resize",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (PanelToplevelClass, begin_resize),
			      NULL,
			      NULL,
			      panel_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN,
			      0);

	gtk_binding_entry_add_signal (binding_set, GDK_F10, GDK_CONTROL_MASK,
                                     "popup_panel_menu", 0);

	panel_bindings_set_entries (binding_set);

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
				      !toplevel->priv->expand,
				      toplevel->priv->orientation & PANEL_HORIZONTAL_MASK ?
						GTK_ORIENTATION_HORIZONTAL :
						GTK_ORIENTATION_VERTICAL,	
				      toplevel->priv->size);

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

	toplevel->priv = PANEL_TOPLEVEL_GET_PRIVATE (toplevel);

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

	toplevel->priv->hide_timeout   = 0;
	toplevel->priv->unhide_timeout = 0;

	toplevel->priv->geometry.x      = -1;
	toplevel->priv->geometry.y      = -1;
	toplevel->priv->geometry.width  = -1;
	toplevel->priv->geometry.height = -1;

	toplevel->priv->original_width  = -1;
	toplevel->priv->original_height = -1;

	toplevel->priv->grab_op = PANEL_GRAB_OP_NONE;

	toplevel->priv->drag_offset_x = 0;
	toplevel->priv->drag_offset_y = 0;

	toplevel->priv->animation_end_x              = 0;
	toplevel->priv->animation_end_y              = 0;
	toplevel->priv->animation_end_width          = 0;
	toplevel->priv->animation_end_height         = 0;
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
	toplevel->priv->n_autohide_disablers = 0;

	for (i = 0; i < N_ATTACH_TOPLEVEL_SIGNALS; i++)
		toplevel->priv->attach_toplevel_signals [i] = 0;
	for (i = 0; i < N_ATTACH_WIDGET_SIGNALS; i++)
		toplevel->priv->attach_widget_signals [i] = 0;

	toplevel->priv->block_auto_hide   = 0;
	toplevel->priv->auto_hide         = FALSE;
	toplevel->priv->buttons_enabled   = TRUE;
	toplevel->priv->arrows_enabled    = TRUE;
	toplevel->priv->x_centered        = FALSE;
	toplevel->priv->y_centered        = FALSE;
	toplevel->priv->animating         = FALSE;
	toplevel->priv->grab_is_keyboard  = FALSE;
	toplevel->priv->position_centered = FALSE;
	toplevel->priv->attached          = FALSE;
	toplevel->priv->attach_hidden     = FALSE;
	toplevel->priv->updated_geometry_initial = FALSE;

	gtk_widget_add_events (GTK_WIDGET (toplevel),
			       GDK_BUTTON_PRESS_MASK |
			       GDK_BUTTON_RELEASE_MASK |
			       GDK_POINTER_MOTION_MASK |
			       GDK_ENTER_NOTIFY_MASK |
			       GDK_LEAVE_NOTIFY_MASK);

	panel_toplevel_setup_widgets (toplevel);
	panel_toplevel_update_description (toplevel);
	
	toplevel_list = g_slist_prepend (toplevel_list, toplevel);
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

PanelWidget *
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

G_CONST_RETURN char *
panel_toplevel_get_description (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), NULL);

	return toplevel->priv->description;
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
			panel_toplevel_set_x (toplevel, 0, TRUE);
		else
			panel_toplevel_set_y (toplevel, 0, TRUE);
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

gboolean
panel_toplevel_get_is_floating (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), TRUE);

	return toplevel->priv->floating;
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
	if (rotate && !toplevel->priv->position_centered && !toplevel->priv->expand &&
	    toplevel->priv->updated_geometry_initial) {
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

	if (toplevel->priv->auto_hide_size == auto_hide_size)
		return;

	toplevel->priv->auto_hide_size = auto_hide_size;

	if (toplevel->priv->state == PANEL_STATE_AUTO_HIDDEN) {
		if (panel_toplevel_update_struts (toplevel, FALSE))
			gtk_widget_queue_resize (GTK_WIDGET (toplevel));
	}

	g_object_notify (G_OBJECT (toplevel), "auto-hide-size");
}

int
panel_toplevel_get_auto_hide_size (PanelToplevel *toplevel)
{
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (toplevel), DEFAULT_AUTO_HIDE_SIZE);

	return toplevel->priv->auto_hide_size;
}

void
panel_toplevel_set_x (PanelToplevel *toplevel,
		      int            x,
		      gboolean       x_centered)
{
	gboolean changed = FALSE;

	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	x_centered = x_centered != FALSE;

	g_object_freeze_notify (G_OBJECT (toplevel));

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

	if (changed)
		gtk_widget_queue_resize (GTK_WIDGET (toplevel));

	g_object_thaw_notify (G_OBJECT (toplevel));
}

void
panel_toplevel_set_y (PanelToplevel *toplevel,
		      int            y,
		      gboolean       y_centered)
{
	gboolean changed = FALSE;

	g_return_if_fail (PANEL_IS_TOPLEVEL (toplevel));

	y_centered = y_centered != FALSE;

	g_object_freeze_notify (G_OBJECT (toplevel));

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

	if (changed)
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

	if (panel_toplevel_update_struts (toplevel, FALSE))
		gtk_widget_queue_resize (GTK_WIDGET (toplevel));

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
