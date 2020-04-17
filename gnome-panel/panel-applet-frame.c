/*
 * panel-applet-frame.c: panel side container for applets
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
 * Copyright (C) 2016 - 2020 Alberts Muktupāvels
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
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>
#include <string.h>

#include <glib/gi18n.h>

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>

#include "panel-applets-manager.h"
#include "panel-bindings.h"
#include "panel.h"
#include "applet.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-object-loader.h"
#include "panel-schemas.h"

#include "panel-applet-frame.h"

#define PANEL_RESPONSE_DELETE      0
#define PANEL_RESPONSE_DONT_RELOAD 1
#define PANEL_RESPONSE_RELOAD      2

static void panel_applet_frame_activating_free (PanelAppletFrameActivating *frame_act);

static void panel_applet_frame_loading_failed  (const char  *iid,
					        PanelWidget *panel,
					        const char  *id);

struct _PanelAppletFrameActivating {
	PanelWidget *panel;
	char        *id;
	GSettings   *settings;
};

/* PanelAppletFrame implementation */

#define HANDLE_SIZE 10

struct _PanelAppletFramePrivate {
	PanelWidget     *panel;
	AppletInfo      *applet_info;

	PanelOrientation orientation;

	GpApplet        *applet;

	gchar           *iid;

	GtkAllocation    child_allocation;
	GdkRectangle     handle_rect;

	guint            has_handle : 1;

	gboolean         moving_focus_out;
};

enum {
	MOVE_FOCUS_OUT_OF_APPLET,
	LAST_SIGNAL
};

static guint panel_applet_frame_signals [LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (PanelAppletFrame, panel_applet_frame, GTK_TYPE_EVENT_BOX)

static gboolean
panel_applet_frame_draw (GtkWidget *widget,
                         cairo_t   *cr)
{
        PanelAppletFrame *frame = PANEL_APPLET_FRAME (widget);
	GtkStyleContext *context;
	GtkStateFlags     state;
	cairo_pattern_t  *bg_pattern;

        if (GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->draw)
                GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->draw (widget, cr);

	if (!frame->priv->has_handle)
		return FALSE;

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);
	gtk_style_context_save (context);
	gtk_style_context_set_state (context, state);

	cairo_save (cr);

	/* Set the pattern transform so as to correctly render a patterned
	 * background with the handle */
	gtk_style_context_get (context, state,
			       "background-image", &bg_pattern,
			       NULL);

	if (bg_pattern) {
		cairo_matrix_t ptm;

		cairo_matrix_init_translate (&ptm,
					     frame->priv->handle_rect.x,
					     frame->priv->handle_rect.y);
		cairo_matrix_scale (&ptm,
				    frame->priv->handle_rect.width,
				    frame->priv->handle_rect.height);
		cairo_pattern_set_matrix (bg_pattern, &ptm);
		cairo_pattern_destroy (bg_pattern);
	}

	gtk_render_handle (context, cr,
			   frame->priv->handle_rect.x,
			   frame->priv->handle_rect.y,
			   frame->priv->handle_rect.width,
			   frame->priv->handle_rect.height);

	cairo_restore (cr);

	gtk_style_context_restore (context);

        return FALSE;
}

static void
panel_applet_frame_get_preferred_width(GtkWidget *widget, gint *minimal_width, gint *natural_width)
{
	PanelAppletFrame *frame;
	GtkBin           *bin;
	GtkWidget        *child;
	guint             border_width;

	frame = PANEL_APPLET_FRAME (widget);
	bin = GTK_BIN (widget);

	if (!frame->priv->has_handle) {
		GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->get_preferred_width (widget, minimal_width, natural_width);
		return;
	}

	child = gtk_bin_get_child (bin);
	if (child && gtk_widget_get_visible (child))
		gtk_widget_get_preferred_width (child, minimal_width, natural_width);

	border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
	*minimal_width += border_width;
	*natural_width += border_width;

	switch (frame->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
	case PANEL_ORIENTATION_BOTTOM:
		*minimal_width += HANDLE_SIZE;
		*natural_width += HANDLE_SIZE;
		break;
	case PANEL_ORIENTATION_LEFT:
	case PANEL_ORIENTATION_RIGHT:
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
panel_applet_frame_get_preferred_height(GtkWidget *widget, gint *minimal_height, gint *natural_height)
{
	PanelAppletFrame *frame;
	GtkBin           *bin;
	GtkWidget        *child;
	guint             border_width;

	frame = PANEL_APPLET_FRAME (widget);
	bin = GTK_BIN (widget);

	if (!frame->priv->has_handle) {
		GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->get_preferred_height (widget, minimal_height, natural_height);
		return;
	}

	child = gtk_bin_get_child (bin);
	if (child && gtk_widget_get_visible (child))
		gtk_widget_get_preferred_height (child, minimal_height, natural_height);

	border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
	*minimal_height += border_width;
	*natural_height += border_width;

	switch (frame->priv->orientation) {
	case PANEL_ORIENTATION_LEFT:
	case PANEL_ORIENTATION_RIGHT:
		*minimal_height += HANDLE_SIZE;
		*natural_height += HANDLE_SIZE;
		break;
	case PANEL_ORIENTATION_TOP:
	case PANEL_ORIENTATION_BOTTOM:
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
panel_applet_frame_size_allocate (GtkWidget     *widget,
				  GtkAllocation *allocation)
{
	PanelAppletFrame *frame;
	GtkBin           *bin;
	GtkWidget        *child;
	GdkWindow        *window;
	GtkAllocation     new_allocation;
	GtkAllocation     widget_allocation;

	frame = PANEL_APPLET_FRAME (widget);
	bin = GTK_BIN (widget);

	gtk_widget_get_allocation (widget, &widget_allocation);

	if (!frame->priv->has_handle) {
		GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->size_allocate (widget,
										   allocation);
		return;
	}

	gtk_widget_set_allocation (widget, allocation);

	frame->priv->handle_rect.x = 0;
	frame->priv->handle_rect.y = 0;

	switch (frame->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
	case PANEL_ORIENTATION_BOTTOM:
		frame->priv->handle_rect.width  = HANDLE_SIZE;
		frame->priv->handle_rect.height = allocation->height;

		if (gtk_widget_get_direction (GTK_WIDGET (frame)) !=
		    GTK_TEXT_DIR_RTL) {
			frame->priv->handle_rect.x = 0;
			new_allocation.x = HANDLE_SIZE;
		} else {
			frame->priv->handle_rect.x = allocation->width - HANDLE_SIZE;
			new_allocation.x = 0;
		}

		new_allocation.y      = 0;
		new_allocation.width  = allocation->width - HANDLE_SIZE;
		new_allocation.height = allocation->height;
		break;
	case PANEL_ORIENTATION_LEFT:
	case PANEL_ORIENTATION_RIGHT:
		frame->priv->handle_rect.width  = allocation->width;
		frame->priv->handle_rect.height = HANDLE_SIZE;

		new_allocation.x      = 0;
		new_allocation.y      = HANDLE_SIZE;
		new_allocation.width  = allocation->width;
		new_allocation.height = allocation->height - HANDLE_SIZE;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	new_allocation.width  = MAX (1, new_allocation.width);
	new_allocation.height = MAX (1, new_allocation.height);

	window = gtk_widget_get_window (widget);

	/* If the child allocation changed, that means that the frame is drawn
	 * in a new place, so we must redraw the entire widget.
	 */
	if (gtk_widget_get_mapped (widget) &&
	    (new_allocation.x != frame->priv->child_allocation.x ||
	     new_allocation.y != frame->priv->child_allocation.y ||
	     new_allocation.width != frame->priv->child_allocation.width ||
	     new_allocation.height != frame->priv->child_allocation.height))
		gdk_window_invalidate_rect (window, &widget_allocation, FALSE);

	if (gtk_widget_get_realized (widget)) {
		guint border_width;

		border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
		gdk_window_move_resize (window,
			allocation->x + border_width,
			allocation->y + border_width,
			MAX (allocation->width - border_width * 2, 0),
			MAX (allocation->height - border_width * 2, 0));
	}

	child = gtk_bin_get_child (bin);
	if (child && gtk_widget_get_visible (child))
		gtk_widget_size_allocate (child, &new_allocation);
  
	frame->priv->child_allocation = new_allocation;
}

static inline gboolean
button_event_in_rect (GdkEventButton *event,
		      GdkRectangle   *rect)
{
	if (event->x >= rect->x &&
	    event->x <= (rect->x + rect->width) &&
	    event->y >= rect->y &&
	    event->y <= (rect->y + rect->height))
		return TRUE;

	return FALSE;
}

static void
popup_menu (GtkMenu  *menu,
            GpApplet *applet)
{
  gtk_menu_attach_to_widget (menu, GTK_WIDGET (applet), NULL);

  gp_applet_popup_menu_at_widget (applet,
                                  menu,
                                  GTK_WIDGET (applet),
                                  NULL);
}

static void
move_cb (GtkMenuItem      *menuitem,
         PanelAppletFrame *self)
{
  GtkWidget *widget;
  GtkWidget *parent;

  widget = GTK_WIDGET (self);
  parent = gtk_widget_get_parent (widget);

  if (!PANEL_IS_WIDGET (parent))
    return;

  panel_widget_applet_drag_start (PANEL_WIDGET (parent),
                                  widget,
                                  GDK_CURRENT_TIME);
}

static void
remove_cb (GtkMenuItem      *menuitem,
           PanelAppletFrame *self)
{
  AppletInfo *info;

  gp_applet_remove_from_panel (self->priv->applet);

  if (self->priv->applet_info == NULL)
    return;

  info = self->priv->applet_info;
  self->priv->applet_info = NULL;

  panel_layout_delete_object (panel_applet_get_id (info));
}

static void
frame_popup_edit_menu (PanelAppletFrame *self,
                       guint             button,
                       guint32           timestamp)
{
  GtkWidget *menu;
  GtkWidget *menuitem;
  gboolean movable;
  gboolean removable;

  menu = gtk_menu_new ();

  movable = FALSE;
  if (self->priv->applet_info != NULL)
    movable = panel_applet_can_freely_move (self->priv->applet_info);

  removable = panel_layout_is_writable ();

  menuitem = gtk_menu_item_new_with_mnemonic (_("_Move"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  g_signal_connect (menuitem, "activate", G_CALLBACK (move_cb), self);
  gtk_widget_set_sensitive (menuitem, movable);

  menuitem = gtk_menu_item_new_with_mnemonic (_("_Remove From Panel"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  g_signal_connect (menuitem, "activate", G_CALLBACK (remove_cb), self);
  gtk_widget_set_sensitive (menuitem, removable);

  popup_menu (GTK_MENU (menu), self->priv->applet);
}

static void
frame_popup_menu (PanelAppletFrame *self,
                  guint             button,
                  guint32           timestamp)
{
  GtkWidget *menu;

  menu = gp_applet_get_menu (self->priv->applet);

  if (menu == NULL)
    return;

  popup_menu (GTK_MENU (menu), self->priv->applet);
}

static gboolean
panel_applet_frame_button_changed (GtkWidget      *widget,
				   GdkEventButton *event)
{
	PanelAppletFrame *frame;
	guint             modifiers;

	frame = PANEL_APPLET_FRAME (widget);

	modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

	switch (event->button) {
	case 1:
	case 2:
		if (button_event_in_rect (event, &frame->priv->handle_rect)) {
			if ((event->type == GDK_BUTTON_PRESS ||
			     event->type == GDK_2BUTTON_PRESS) &&
			    modifiers == panel_bindings_get_mouse_button_modifier_keymask ()){
				panel_widget_applet_drag_start (
					frame->priv->panel, GTK_WIDGET (frame),
					event->time);
				return TRUE;
			} else if (event->type == GDK_BUTTON_RELEASE) {
				panel_widget_applet_drag_end (frame->priv->panel);
				return TRUE;
			}
		}
		break;
	case 3:
		if (event->type == GDK_BUTTON_PRESS ||
		    event->type == GDK_2BUTTON_PRESS) {
			GdkDisplay *display;
			GdkSeat *seat;

			display = gtk_widget_get_display (widget);
			seat = gdk_display_get_default_seat (display);
			gdk_seat_ungrab (seat);

			if (modifiers == panel_bindings_get_mouse_button_modifier_keymask ())
				frame_popup_edit_menu (frame, event->button, event->time);
			else
				frame_popup_menu (frame, event->button, event->time);

			return TRUE;
		} else if (event->type == GDK_BUTTON_RELEASE)
			return TRUE;
		break;
	default:
		break;
	}

	return FALSE;
}

static void
panel_applet_frame_finalize (GObject *object)
{
	PanelAppletFrame *frame = PANEL_APPLET_FRAME (object);

	panel_applets_manager_factory_deactivate (frame->priv->iid);

	g_free (frame->priv->iid);
	frame->priv->iid = NULL;

        G_OBJECT_CLASS (panel_applet_frame_parent_class)->finalize (object);
}

static void
panel_applet_frame_move_focus_out_of_applet (PanelAppletFrame *frame,
                                             GtkDirectionType  dir)
{
	GtkWidget *toplevel;

	frame->priv->moving_focus_out = TRUE;
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (frame));
	g_return_if_fail (toplevel);

	gtk_widget_child_focus (toplevel, dir);
	frame->priv->moving_focus_out = FALSE;
}

static gboolean
panel_applet_frame_focus (GtkWidget        *widget,
                          GtkDirectionType  dir)
{
	PanelAppletFrame *frame;

	g_return_val_if_fail (PANEL_IS_APPLET_FRAME (widget), FALSE);

	frame = PANEL_APPLET_FRAME (widget);

	if (frame->priv->moving_focus_out)
		return FALSE;

	return GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->focus (widget, dir);
}

static void
add_tab_bindings (GtkBindingSet    *binding_set,
                  GdkModifierType   modifiers,
                  GtkDirectionType  direction)
{
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Tab, modifiers,
				      "move_focus_out_of_applet", 1,
				      GTK_TYPE_DIRECTION_TYPE, direction);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Tab, modifiers,
				      "move_focus_out_of_applet", 1,
				      GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
panel_applet_frame_class_init (PanelAppletFrameClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
	GtkBindingSet *binding_set;

	gobject_class->finalize = panel_applet_frame_finalize;

	widget_class->draw                 = panel_applet_frame_draw;
	widget_class->get_preferred_width  = panel_applet_frame_get_preferred_width;
	widget_class->get_preferred_height = panel_applet_frame_get_preferred_height;
	widget_class->size_allocate        = panel_applet_frame_size_allocate;
	widget_class->button_press_event   = panel_applet_frame_button_changed;
	widget_class->button_release_event = panel_applet_frame_button_changed;
	widget_class->focus                = panel_applet_frame_focus;

	klass->move_focus_out_of_applet = panel_applet_frame_move_focus_out_of_applet;

	panel_applet_frame_signals [MOVE_FOCUS_OUT_OF_APPLET] =
                g_signal_new ("move_focus_out_of_applet",
                              G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (PanelAppletFrameClass, move_focus_out_of_applet),
                              NULL, NULL, NULL,
                              G_TYPE_NONE, 1, GTK_TYPE_DIRECTION_TYPE);

	binding_set = gtk_binding_set_by_class (gobject_class);
	add_tab_bindings (binding_set, 0, GTK_DIR_TAB_FORWARD);
	add_tab_bindings (binding_set, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
	add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
	add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
}

static void
panel_applet_frame_init (PanelAppletFrame *frame)
{
	GtkStyleContext *context;

	frame->priv = panel_applet_frame_get_instance_private (frame);

	frame->priv->panel       = NULL;
	frame->priv->orientation = PANEL_ORIENTATION_TOP;
	frame->priv->applet_info = NULL;
	frame->priv->has_handle  = FALSE;

	context = gtk_widget_get_style_context (GTK_WIDGET (frame));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_HORIZONTAL);
}

static void
update_flags (PanelAppletFrame *self)
{
  GpAppletFlags flags;
  gboolean major;
  gboolean minor;

  flags = gp_applet_get_flags (self->priv->applet);

  major = (flags & GP_APPLET_FLAGS_EXPAND_MAJOR) != 0;
  minor = (flags & GP_APPLET_FLAGS_EXPAND_MINOR) != 0;

  self->priv->has_handle = (flags & GP_APPLET_FLAGS_HAS_HANDLE) != 0;

  panel_widget_set_applet_expandable (self->priv->panel,
                                      GTK_WIDGET (self),
                                      major,
                                      minor);
}

static void
update_size_hints (PanelAppletFrame *self)
{
  guint n_elements;
  gint *size_hints;

  size_hints = gp_applet_get_size_hints (self->priv->applet, &n_elements);

  if (self->priv->has_handle)
    {
      gint extra_size;
      guint i;

      extra_size = HANDLE_SIZE + 1;

      for (i = 0; i < n_elements; i++)
        size_hints[i] += extra_size;
    }

  /* It takes the ownership of size-hints array */
  panel_widget_set_applet_size_hints (self->priv->panel,
                                      GTK_WIDGET (self),
                                      size_hints,
                                      n_elements);
}

static void
panel_applet_frame_init_properties (PanelAppletFrame *self)
{
  update_flags (self);
  update_size_hints (self);
}

static void
panel_applet_frame_sync_menu_state (PanelLockdown *lockdown,
				    gpointer       user_data)
{
	PanelAppletFrame *frame = PANEL_APPLET_FRAME (user_data);
	gboolean          locked_down;
	GpLockdownFlags   lockdowns;

	locked_down = panel_lockdown_get_panels_locked_down_s ();
	lockdowns = panel_lockdown_get_flags_s (frame->priv->iid);

	gp_applet_set_locked_down (frame->priv->applet, locked_down);
	gp_applet_set_lockdowns (frame->priv->applet, lockdowns);
}

static void
frame_change_orientation (PanelAppletFrame *self,
                          PanelOrientation  panel_orientation)
{
  GtkOrientation orientation;
  GtkPositionType position;

  switch (panel_orientation)
    {
      case PANEL_ORIENTATION_BOTTOM:
        orientation = GTK_ORIENTATION_HORIZONTAL;
        position = GTK_POS_BOTTOM;
        break;
      case PANEL_ORIENTATION_LEFT:
        orientation = GTK_ORIENTATION_VERTICAL;
        position = GTK_POS_LEFT;
        break;
      case PANEL_ORIENTATION_RIGHT:
        orientation = GTK_ORIENTATION_VERTICAL;
        position = GTK_POS_RIGHT;
        break;
      case PANEL_ORIENTATION_TOP:
        orientation = GTK_ORIENTATION_HORIZONTAL;
        position = GTK_POS_TOP;
      default:
        break;
    }

  gp_applet_set_orientation (self->priv->applet, orientation);
  gp_applet_set_position (self->priv->applet, position);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

void
panel_applet_frame_change_orientation (PanelAppletFrame *frame,
				       PanelOrientation  orientation)
{
	GtkStyleContext *context;

	if (orientation == frame->priv->orientation)
		return;

	frame->priv->orientation = orientation;

	context = gtk_widget_get_style_context (GTK_WIDGET (frame));
	if (orientation & PANEL_HORIZONTAL_MASK) {
		gtk_style_context_add_class (context, GTK_STYLE_CLASS_HORIZONTAL);
		gtk_style_context_remove_class (context, GTK_STYLE_CLASS_VERTICAL);
	} else {
		gtk_style_context_add_class (context, GTK_STYLE_CLASS_VERTICAL);
		gtk_style_context_remove_class (context, GTK_STYLE_CLASS_HORIZONTAL);
	}
	gtk_widget_reset_style (GTK_WIDGET (frame));

	frame_change_orientation (frame, orientation);
}

void
panel_applet_frame_set_panel (PanelAppletFrame *frame,
			      PanelWidget      *panel)
{
	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));
	g_return_if_fail (PANEL_IS_WIDGET (panel));

	frame->priv->panel = panel;
}

static void
flags_changed_cb (GpApplet         *applet,
                  PanelAppletFrame *self)
{
  update_flags (self);
}

static void
size_hints_changed_cb (GpApplet         *applet,
                       PanelAppletFrame *self)
{
  update_size_hints (self);
}

void
_panel_applet_frame_set_applet (PanelAppletFrame *self,
                                GpApplet         *applet)
{
  self->priv->applet = applet;

  g_signal_connect (applet,
                    "flags-changed",
                    G_CALLBACK (flags_changed_cb),
                    self);

  g_signal_connect (applet,
                    "size-hints-changed",
                    G_CALLBACK (size_hints_changed_cb),
                    self);
}

void
_panel_applet_frame_set_iid (PanelAppletFrame *frame,
			     const gchar      *iid)
{
	if (frame->priv->iid)
		g_free (frame->priv->iid);
	frame->priv->iid = g_strdup (iid);
}

void
_panel_applet_frame_activated (PanelAppletFrame           *frame,
			       PanelAppletFrameActivating *frame_act,
			       GError                     *error)
{
	AppletInfo *info;

	g_assert (frame->priv->iid != NULL);

	if (error != NULL) {
		g_warning ("Failed to load applet %s:\n%s",
			   frame->priv->iid, error->message);
		g_error_free (error);

		panel_applet_frame_loading_failed (frame->priv->iid,
						   frame_act->panel,
						   frame_act->id);
		panel_applet_frame_activating_free (frame_act);
		gtk_widget_destroy (GTK_WIDGET (frame));

		return;
	}

	frame->priv->panel = frame_act->panel;
	gtk_widget_show (GTK_WIDGET (frame));

	info = panel_applet_register (GTK_WIDGET (frame),
	                              frame->priv->panel,
	                              frame_act->id,
	                              frame_act->settings);
	frame->priv->applet_info = info;

	panel_widget_set_applet_size_constrained (frame->priv->panel,
						  GTK_WIDGET (frame), TRUE);

	panel_lockdown_on_notify (panel_lockdown_get (),
				  NULL,
				  G_OBJECT (frame),
				  panel_applet_frame_sync_menu_state,
				  frame);
	panel_applet_frame_sync_menu_state (panel_lockdown_get (), frame);

	panel_applet_frame_init_properties (frame);

	panel_object_loader_stop_loading (frame_act->id);
	panel_applet_frame_activating_free (frame_act);
}

/* Generic methods */

static GSList *no_reload_applets = NULL;

enum {
	LOADING_FAILED_RESPONSE_DONT_DELETE,
	LOADING_FAILED_RESPONSE_DELETE
};

static void
panel_applet_frame_activating_free (PanelAppletFrameActivating *frame_act)
{
	g_object_unref (frame_act->settings);
	g_free (frame_act->id);
	g_slice_free (PanelAppletFrameActivating, frame_act);
}

PanelOrientation
panel_applet_frame_activating_get_orientation (PanelAppletFrameActivating *frame_act)
{
	return panel_widget_get_applet_orientation (frame_act->panel);
}

gboolean
panel_applet_frame_activating_get_locked_down (PanelAppletFrameActivating *frame_act)
{
	return panel_lockdown_get_panels_locked_down_s ();
}

gchar *
panel_applet_frame_activating_get_settings_path (PanelAppletFrameActivating *frame_act)
{
        char *path;
        char *path_instance;

        g_object_get (frame_act->settings, "path", &path, NULL);
        path_instance = g_strdup_printf ("%s%s", path,
                                         PANEL_LAYOUT_OBJECT_CONFIG_SUFFIX);
        g_free (path);

	return path_instance;
}

gchar *
panel_applet_frame_activating_get_initial_settings_path (PanelAppletFrameActivating *frame_act)
{
        char *path;
        char *path_instance;

        g_object_get (frame_act->settings, "path", &path, NULL);
        path_instance = g_strdup_printf ("%sinitial-settings/", path);
        g_free (path);

        return path_instance;
}

static void
panel_applet_frame_loading_failed_response (GtkWidget *dialog,
					    guint      response,
					    char      *id)
{
	gtk_widget_destroy (dialog);

	if (response == LOADING_FAILED_RESPONSE_DELETE &&
	    !panel_lockdown_get_panels_locked_down_s () &&
	    panel_layout_is_writable ()) {
		GSList *item;

		item = g_slist_find_custom (no_reload_applets, id,
					    (GCompareFunc) strcmp);
		if (item) {
			g_free (item->data);
			no_reload_applets = g_slist_delete_link (no_reload_applets,
								 item);
		}

		panel_layout_delete_object (id);
	}

	g_free (id);
}

static void
panel_applet_frame_loading_failed (const char  *iid,
				   PanelWidget *panel,
				   const char  *id)
{
	GtkWidget *dialog;
	char      *problem_txt;
	gboolean   locked_down;

	no_reload_applets = g_slist_prepend (no_reload_applets,
					     g_strdup (id));

	locked_down = panel_lockdown_get_panels_locked_down_s ();

	problem_txt = g_strdup_printf (_("The panel encountered a problem "
					 "while loading \"%s\"."),
				       iid);

	dialog = gtk_message_dialog_new (NULL, 0,
					 locked_down ? GTK_MESSAGE_INFO : GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_NONE,
					 "%s", problem_txt);
	g_free (problem_txt);

	if (locked_down) {
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("OK"), LOADING_FAILED_RESPONSE_DONT_DELETE,
					NULL);
	} else {
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
					_("Do you want to delete the applet "
					  "from your configuration?"));
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("D_on't Delete"), LOADING_FAILED_RESPONSE_DONT_DELETE,
					_("_Delete"), LOADING_FAILED_RESPONSE_DELETE,
					NULL);
	}

	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 LOADING_FAILED_RESPONSE_DONT_DELETE);

	gtk_window_set_screen (GTK_WINDOW (dialog),
			       gtk_window_get_screen (GTK_WINDOW (panel->toplevel)));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (panel_applet_frame_loading_failed_response),
			  g_strdup (id));

	panel_widget_register_open_dialog (panel, dialog);
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);
	/* FIXME: http://bugzilla.gnome.org/show_bug.cgi?id=165132 */
	gtk_window_set_title (GTK_WINDOW (dialog), _("Error"));

	gtk_widget_show_all (dialog);

	/* Note: this call will free the memory for id, so the variable should
	 * not get accessed afterwards. */
	panel_object_loader_stop_loading (id);
}

static void
panel_applet_frame_load_helper (const gchar *iid,
				PanelWidget *panel,
				const char  *id,
				GSettings   *settings)
{
	PanelAppletFrameActivating *frame_act;

	if (g_slist_find_custom (no_reload_applets, id,
				 (GCompareFunc) strcmp)) {
		panel_object_loader_stop_loading (id);
		return;
	}

	if (panel_applets_manager_is_applet_disabled (iid, NULL)) {
		panel_object_loader_stop_loading (id);
		return;
	}

	frame_act = g_slice_new0 (PanelAppletFrameActivating);
	frame_act->panel    = panel;
	frame_act->id       = g_strdup (id);
	frame_act->settings = g_object_ref (settings);

	if (!panel_applets_manager_load_applet (iid, frame_act)) {
		panel_applet_frame_loading_failed (iid, panel, id);
		panel_applet_frame_activating_free (frame_act);
	}
}

void
panel_applet_frame_load (PanelWidget *panel_widget,
			 const char  *id,
			 GSettings   *settings)
{
	gchar *applet_iid;

	g_return_if_fail (panel_widget != NULL);
	g_return_if_fail (id != NULL);

	applet_iid = g_settings_get_string (settings, PANEL_OBJECT_IID_KEY);

	if (!panel_applets_manager_get_applet_info (applet_iid)) {
		gchar *new_iid;

		new_iid = panel_applets_manager_get_new_iid (applet_iid);

		if (new_iid != NULL) {
			g_settings_set_string (settings, PANEL_OBJECT_IID_KEY, new_iid);
			g_free (applet_iid);

			applet_iid = new_iid;
		}
	}

	panel_applet_frame_load_helper (applet_iid, panel_widget, id, settings);
	g_free (applet_iid);
}

void
panel_applet_frame_create (PanelToplevel       *toplevel,
			   PanelObjectPackType  pack_type,
			   int                  pack_index,
			   const char          *iid,
			   GVariant            *initial_settings)
{
	g_return_if_fail (iid != NULL);

	panel_layout_object_create (iid,
				    panel_toplevel_get_id (toplevel),
				    pack_type, pack_index,
				    initial_settings);
}
