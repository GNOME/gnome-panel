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

#include "gp-applet-manager.h"
#include "gp-error-applet.h"
#include "gp-handle.h"
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

typedef struct _PanelAppletFrameActivating PanelAppletFrameActivating;

struct _PanelAppletFrameActivating {
	PanelWidget *panel;
	char        *id;
	GSettings   *settings;
};

/* PanelAppletFrame implementation */

struct _PanelAppletFramePrivate {
	PanelWidget     *panel;
	AppletInfo      *applet_info;

	PanelOrientation orientation;

	GpApplet        *applet;

	char            *module_id;
	char            *applet_id;

	GtkWidget       *handle;

	gboolean         moving_focus_out;
};

enum {
	MOVE_FOCUS_OUT_OF_APPLET,
	LAST_SIGNAL
};

static guint panel_applet_frame_signals [LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (PanelAppletFrame, panel_applet_frame, GTK_TYPE_EVENT_BOX)

static void
panel_applet_frame_size_allocate (GtkWidget     *widget,
                                  GtkAllocation *allocation)
{
  PanelAppletFrame *self;
  GtkRequisition handle_size;
  GtkAllocation handle_allocation;
  GtkAllocation child_allocation;
  GtkWidget *child;

  self = PANEL_APPLET_FRAME (widget);

  if (self->priv->handle == NULL ||
      !gtk_widget_get_visible (self->priv->handle))
    {
      GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->size_allocate (widget,
                                                                         allocation);

      return;
    }

  gtk_widget_set_allocation (widget, allocation);
  gtk_widget_get_preferred_size (self->priv->handle, &handle_size, NULL);

  if (!gtk_widget_get_has_window (widget))
    {
      handle_allocation.x = allocation->x;
      handle_allocation.y = allocation->y;

      child_allocation.x = allocation->x;
      child_allocation.y = allocation->y;
    }
  else
    {
      if (gtk_widget_get_realized (widget))
        gdk_window_move_resize (gtk_widget_get_window (widget),
                                allocation->x,
                                allocation->y,
                                allocation->width,
                                allocation->height);

      handle_allocation.x = 0;
      handle_allocation.y = 0;

      child_allocation.x = 0;
      child_allocation.y = 0;
    }

  switch (self->priv->orientation)
    {
      case PANEL_ORIENTATION_TOP:
      case PANEL_ORIENTATION_BOTTOM:
        if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
          handle_allocation.x = allocation->width - handle_size.width;
        else
          child_allocation.x += handle_size.width;

        handle_allocation.width = handle_size.width;
        handle_allocation.height = allocation->height;

        child_allocation.width = allocation->width - handle_size.width;
        child_allocation.height = allocation->height;
        break;

      case PANEL_ORIENTATION_LEFT:
      case PANEL_ORIENTATION_RIGHT:
        handle_allocation.width = allocation->width;
        handle_allocation.height = handle_size.height;

        child_allocation.y += handle_size.height;
        child_allocation.width = allocation->width;
        child_allocation.height = allocation->height - handle_size.height;
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  gtk_widget_size_allocate (self->priv->handle, &handle_allocation);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child != NULL && gtk_widget_get_visible (child))
    gtk_widget_size_allocate (child, &child_allocation);
}

static void
panel_applet_frame_get_preferred_height (GtkWidget *widget,
                                         gint      *minimum_height,
                                         gint      *natural_height)
{
  PanelAppletFrame *self;
  int handle_minimum_height;
  int handle_natural_height;

  self = PANEL_APPLET_FRAME (widget);

  GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->get_preferred_height (widget,
                                                                            minimum_height,
                                                                            natural_height);

  if (self->priv->handle == NULL ||
      !gtk_widget_get_visible (self->priv->handle))
    return;

  gtk_widget_get_preferred_height (self->priv->handle,
                                   &handle_minimum_height,
                                   &handle_natural_height);

  if (self->priv->orientation & PANEL_VERTICAL_MASK)
    {
      *minimum_height += handle_minimum_height;
      *natural_height += handle_natural_height;
    }
  else
    {
      *minimum_height = MAX (*minimum_height, handle_minimum_height);
      *natural_height = MAX (*natural_height, handle_natural_height);
    }
}

static void
panel_applet_frame_get_preferred_width_for_height (GtkWidget *widget,
                                                   gint       height,
                                                   gint      *minimum_width,
                                                   gint      *natural_width)
{
  PanelAppletFrame *self;
  int handle_minimum_width;
  int handle_natural_width;

  self = PANEL_APPLET_FRAME (widget);

  GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->get_preferred_width_for_height (widget,
                                                                                      height,
                                                                                      minimum_width,
                                                                                      natural_width);

  if (self->priv->handle == NULL ||
      !gtk_widget_get_visible (self->priv->handle))
    return;

  gtk_widget_get_preferred_width_for_height (self->priv->handle,
                                             height,
                                             &handle_minimum_width,
                                             &handle_natural_width);

  if (self->priv->orientation & PANEL_HORIZONTAL_MASK)
    {
      *minimum_width += handle_minimum_width;
      *natural_width += handle_natural_width;
    }
  else
    {
      *minimum_width = MAX (*minimum_width, handle_minimum_width);
      *natural_width = MAX (*natural_width, handle_natural_width);
    }
}

static void
panel_applet_frame_get_preferred_width (GtkWidget *widget,
                                        gint      *minimum_width,
                                        gint      *natural_width)
{
  PanelAppletFrame *self;
  int handle_minimum_width;
  int handle_natural_width;

  self = PANEL_APPLET_FRAME (widget);

  GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->get_preferred_width (widget,
                                                                           minimum_width,
                                                                           natural_width);

  if (self->priv->handle == NULL ||
      !gtk_widget_get_visible (self->priv->handle))
    return;

  gtk_widget_get_preferred_width (self->priv->handle,
                                  &handle_minimum_width,
                                  &handle_natural_width);

  if (self->priv->orientation & PANEL_HORIZONTAL_MASK)
    {
      *minimum_width += handle_minimum_width;
      *natural_width += handle_natural_width;
    }
  else
    {
      *minimum_width = MAX (*minimum_width, handle_minimum_width);
      *natural_width = MAX (*natural_width, handle_natural_width);
    }
}

static void
panel_applet_frame_get_preferred_height_for_width (GtkWidget *widget,
                                                   gint       width,
                                                   gint      *minimum_height,
                                                   gint      *natural_height)
{
  PanelAppletFrame *self;
  int handle_minimum_height;
  int handle_natural_height;

  self = PANEL_APPLET_FRAME (widget);

  GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->get_preferred_height_for_width (widget,
                                                                                      width,
                                                                                      minimum_height,
                                                                                      natural_height);

  if (self->priv->handle == NULL ||
      !gtk_widget_get_visible (self->priv->handle))
    return;

  gtk_widget_get_preferred_height_for_width (self->priv->handle,
                                             width,
                                             &handle_minimum_height,
                                             &handle_natural_height);

  if (self->priv->orientation & PANEL_VERTICAL_MASK)
    {
      *minimum_height += handle_minimum_height;
      *natural_height += handle_natural_height;
    }
  else
    {
      *minimum_height = MAX (*minimum_height, handle_minimum_height);
      *natural_height = MAX (*natural_height, handle_natural_height);
    }
}

static inline gboolean
button_event_in_handle (PanelAppletFrame *self,
                        GdkEventButton   *event)
{
  GtkAllocation allocation;

  if (self->priv->handle == NULL)
    return FALSE;

  gtk_widget_get_allocation (self->priv->handle, &allocation);

  if (event->x >= allocation.x &&
      event->x <= (allocation.x + allocation.width) &&
      event->y >= allocation.y &&
      event->y <= (allocation.y + allocation.height))
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
  GpApplication *application;
  PanelLayout *layout;

  gp_applet_remove_from_panel (self->priv->applet);

  if (self->priv->applet_info == NULL)
    return;

  info = self->priv->applet_info;
  self->priv->applet_info = NULL;

  application = panel_toplevel_get_application (self->priv->panel->toplevel);
  layout = gp_application_get_layout (application);

  panel_layout_delete_object (layout, panel_applet_get_id (info));
}

static void
frame_popup_edit_menu (PanelAppletFrame *self,
                       guint             button,
                       guint32           timestamp)
{
  GtkWidget *menu;
  GtkWidget *menuitem;
  gboolean movable;
  GpApplication *application;
  PanelLayout *layout;
  gboolean removable;

  menu = gtk_menu_new ();

  movable = FALSE;
  if (self->priv->applet_info != NULL)
    movable = panel_applet_can_freely_move (self->priv->applet_info);

  application = panel_toplevel_get_application (self->priv->panel->toplevel);
  layout = gp_application_get_layout (application);
  removable = panel_layout_is_writable (layout);

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
		if (button_event_in_handle (frame, event)) {
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

	g_clear_pointer (&frame->priv->module_id, g_free);
	g_clear_pointer (&frame->priv->applet_id, g_free);

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
panel_applet_frame_forall (GtkContainer *container,
                           gboolean      include_internals,
                           GtkCallback   callback,
                           gpointer      callback_data)
{
  PanelAppletFrame *self;

  self = PANEL_APPLET_FRAME (container);

  GTK_CONTAINER_CLASS (panel_applet_frame_parent_class)->forall (container,
                                                                 include_internals,
                                                                 callback,
                                                                 callback_data);

  if (include_internals && self->priv->handle)
    (* callback) (self->priv->handle, callback_data);
}

static void
panel_applet_frame_remove (GtkContainer *container,
                           GtkWidget    *widget)
{
  PanelAppletFrame *self;

  self = PANEL_APPLET_FRAME (container);

  if (self->priv->handle != widget)
    {
      GTK_CONTAINER_CLASS (panel_applet_frame_parent_class)->remove (container,
                                                                     widget);

      return;
    }

  gtk_widget_unparent (widget);
  self->priv->handle = NULL;

  if (gtk_widget_get_visible (GTK_WIDGET (container)))
    gtk_widget_queue_resize (GTK_WIDGET (container));
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
	GtkContainerClass *container_class;
	GtkBindingSet *binding_set;

	container_class = GTK_CONTAINER_CLASS (klass);

	gobject_class->finalize = panel_applet_frame_finalize;

	widget_class->size_allocate = panel_applet_frame_size_allocate;
	widget_class->get_preferred_height = panel_applet_frame_get_preferred_height;
	widget_class->get_preferred_width_for_height = panel_applet_frame_get_preferred_width_for_height;
	widget_class->get_preferred_width = panel_applet_frame_get_preferred_width;
	widget_class->get_preferred_height_for_width = panel_applet_frame_get_preferred_height_for_width;
	widget_class->button_press_event   = panel_applet_frame_button_changed;
	widget_class->button_release_event = panel_applet_frame_button_changed;
	widget_class->focus                = panel_applet_frame_focus;

	container_class->forall = panel_applet_frame_forall;
	container_class->remove = panel_applet_frame_remove;

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
	frame->priv = panel_applet_frame_get_instance_private (frame);

	frame->priv->panel       = NULL;
	frame->priv->orientation = PANEL_ORIENTATION_TOP;
	frame->priv->applet_info = NULL;
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

  if ((flags & GP_APPLET_FLAGS_HAS_HANDLE) != 0)
    {
      self->priv->handle = gp_handle_new ();
      gtk_widget_set_parent (self->priv->handle, GTK_WIDGET (self));
      gtk_widget_show (self->priv->handle);
    }
  else
    {
      g_clear_pointer (&self->priv->handle, gtk_widget_destroy);
    }

  panel_widget_set_applet_expandable (self->priv->panel,
                                      GTK_WIDGET (self),
                                      major,
                                      minor);
}

static void
panel_applet_frame_init_properties (PanelAppletFrame *self)
{
  update_flags (self);
}

static void
panel_applet_frame_sync_menu_state (PanelLockdown *lockdown,
				    gpointer       user_data)
{
	PanelAppletFrame *frame = PANEL_APPLET_FRAME (user_data);
	gboolean          locked_down;
	GpLockdownFlags   lockdowns;

	locked_down = panel_lockdown_get_panels_locked_down (lockdown);
	lockdowns = panel_lockdown_get_flags (lockdown,
	                                      frame->priv->module_id,
	                                      frame->priv->applet_id);

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
	if (orientation == frame->priv->orientation)
		return;

	frame->priv->orientation = orientation;

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
_panel_applet_frame_set_applet (PanelAppletFrame *self,
                                GpApplet         *applet)
{
  self->priv->applet = applet;

  g_signal_connect (applet,
                    "flags-changed",
                    G_CALLBACK (flags_changed_cb),
                    self);

  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (applet));
  gtk_widget_show (GTK_WIDGET (applet));
}

static GpApplication *
get_application (PanelWidget *panel)
{
  return panel_toplevel_get_application (panel->toplevel);
}

static void
panel_applet_frame_activated (PanelAppletFrame           *frame,
                              PanelAppletFrameActivating *frame_act)
{
	AppletInfo *info;
	GpApplication *application;
	PanelLockdown *lockdown;

	frame->priv->panel = frame_act->panel;
	gtk_widget_show (GTK_WIDGET (frame));

	info = panel_applet_register (GTK_WIDGET (frame),
	                              frame->priv->panel,
	                              frame_act->id,
	                              frame_act->settings);
	frame->priv->applet_info = info;

	application = panel_toplevel_get_application (frame_act->panel->toplevel);
	lockdown = gp_application_get_lockdown (application);

	panel_lockdown_on_notify (lockdown,
				  NULL,
				  G_OBJECT (frame),
				  panel_applet_frame_sync_menu_state,
				  frame);
	panel_applet_frame_sync_menu_state (lockdown, frame);

	panel_applet_frame_init_properties (frame);

	panel_object_loader_stop_loading (get_application (frame_act->panel),
	                                  frame_act->id);
}

/* Generic methods */

static void
panel_applet_frame_activating_free (PanelAppletFrameActivating *frame_act)
{
	g_object_unref (frame_act->settings);
	g_free (frame_act->id);
	g_free (frame_act);
}

static PanelOrientation
panel_applet_frame_activating_get_orientation (PanelAppletFrameActivating *frame_act)
{
	return panel_widget_get_applet_orientation (frame_act->panel);
}

static gboolean
panel_applet_frame_activating_get_locked_down (PanelAppletFrameActivating *frame_act)
{
	GpApplication *application;
	PanelLockdown *lockdown;

	application = panel_toplevel_get_application (frame_act->panel->toplevel);
	lockdown = gp_application_get_lockdown (application);

	return panel_lockdown_get_panels_locked_down (lockdown);
}

static char *
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

static char *
panel_applet_frame_activating_get_initial_settings_path (PanelAppletFrameActivating *frame_act)
{
        char *path;
        char *path_instance;

        g_object_get (frame_act->settings, "path", &path, NULL);
        path_instance = g_strdup_printf ("%sinitial-settings/", path);
        g_free (path);

        return path_instance;
}

typedef struct
{
  PanelWidget *panel;
  char        *id;
} DeleteData;

static void
delete_data_free (gpointer data,
                  GClosure *closure)
{
  DeleteData *delete_data;

  delete_data = data;

  g_free (delete_data->id);
  g_free (delete_data);
}

static void
delete_cb (GpErrorApplet *applet,
           DeleteData    *data)
{
  GpApplication *application;
  PanelLockdown *lockdown;
  PanelLayout *layout;

  application = panel_toplevel_get_application (data->panel->toplevel);
  lockdown = gp_application_get_lockdown (application);
  layout = gp_application_get_layout (application);

  if (panel_lockdown_get_panels_locked_down (lockdown) ||
      !panel_layout_is_writable (layout))
    return;

  panel_layout_delete_object (layout, data->id);
}

static void
frame_activate (const char                 *module_id,
                const char                 *applet_id,
                PanelAppletFrameActivating *frame_act,
                GpApplet                   *applet);

static void
panel_applet_frame_loading_failed (const char                 *module_id,
                                   const char                 *applet_id,
                                   PanelAppletFrameActivating *frame_act,
                                   GError                     *error)
{
  GpApplication *application;
  GpErrorApplet *applet;
  DeleteData *data;

  application = panel_toplevel_get_application (frame_act->panel->toplevel);
  applet = gp_error_applet_new (module_id, applet_id, error, application);

  data = g_new0 (DeleteData, 1);
  data->panel = frame_act->panel;
  data->id = g_strdup (frame_act->id);

  g_signal_connect_data (applet,
                         "delete",
                         G_CALLBACK (delete_cb),
                         data,
                         delete_data_free,
                         0);

  frame_activate (module_id, applet_id, frame_act, GP_APPLET (applet));
}

static GVariant *
get_initial_settings (PanelAppletFrameActivating *frame_act)
{
  gchar *path;
  GSettings *settings;
  GVariant *initial_settings;

  path = panel_applet_frame_activating_get_initial_settings_path (frame_act);
  settings = g_settings_new_with_path ("org.gnome.gnome-panel.applet.initial-settings", path);
  g_free (path);

  initial_settings = g_settings_get_user_value (settings, "settings");
  g_object_unref (settings);

  return initial_settings;
}

static void
remove_initial_settings (PanelAppletFrameActivating *frame_act)
{
  gchar *path;
  GSettings *settings;

  path = panel_applet_frame_activating_get_initial_settings_path (frame_act);
  settings = g_settings_new_with_path ("org.gnome.gnome-panel.applet.initial-settings", path);
  g_free (path);

  g_settings_reset (settings, "settings");
  g_object_unref (settings);
}

static void
applet_setup (GpApplet                   *applet,
              PanelAppletFrameActivating *frame_act)
{
  gboolean locked_down;
  PanelOrientation panel_orientation;
  GtkOrientation orientation;
  GtkPositionType position;
  GpApplication *application;
  GSettings *general_settings;
  gboolean enable_tooltips;
  gboolean prefer_symbolic_icons;
  guint menu_icon_size;
  guint panel_icon_size;

  locked_down = panel_applet_frame_activating_get_locked_down (frame_act);
  panel_orientation = panel_applet_frame_activating_get_orientation (frame_act);

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
      default:
        orientation = GTK_ORIENTATION_HORIZONTAL;
        position = GTK_POS_TOP;
        break;
    }

  gp_applet_set_locked_down (applet, locked_down);
  gp_applet_set_orientation (applet, orientation);
  gp_applet_set_position (applet, position);

  application = panel_toplevel_get_application (frame_act->panel->toplevel);
  general_settings = gp_application_get_general_settings (application);

  enable_tooltips = g_settings_get_boolean (general_settings,
                                            "enable-tooltips");
  gp_applet_set_enable_tooltips (applet, enable_tooltips);

  prefer_symbolic_icons = g_settings_get_boolean (general_settings,
                                                  "prefer-symbolic-icons");
  gp_applet_set_prefer_symbolic_icons (applet, prefer_symbolic_icons);

  menu_icon_size = g_settings_get_enum (general_settings, "menu-icon-size");
  gp_applet_set_menu_icon_size (applet, menu_icon_size);

  panel_icon_size = panel_widget_get_icon_size (frame_act->panel);
  gp_applet_set_panel_icon_size (applet, panel_icon_size);
}

static void
frame_activate (const char                 *module_id,
                const char                 *applet_id,
                PanelAppletFrameActivating *frame_act,
                GpApplet                   *applet)
{
  PanelAppletFrame *frame;

  applet_setup (applet, frame_act);

  frame = g_object_new (PANEL_TYPE_APPLET_FRAME, NULL);

  _panel_applet_frame_set_applet (frame, applet);

  frame->priv->module_id = g_strdup (module_id);
  frame->priv->applet_id = g_strdup (applet_id);

  panel_applet_frame_activated (frame, frame_act);
}

static void
panel_applet_frame_load_helper (const char  *module_id,
                                const char  *applet_id,
                                PanelWidget *panel,
                                const char  *id,
                                GSettings   *settings)
{
	GpApplication *application;
	GpAppletManager *applet_manager;
	GError *error;
	PanelAppletFrameActivating *frame_act;
	char *settings_path;
	GVariant *initial_settings;
	GpApplet *applet;

	application = panel_toplevel_get_application (panel->toplevel);
	applet_manager = gp_application_get_applet_manager (application);

	frame_act = g_new0 (PanelAppletFrameActivating, 1);
	frame_act->panel = panel;
	frame_act->id = g_strdup (id);
	frame_act->settings = g_object_ref (settings);

	error = NULL;
	if (gp_applet_manager_get_applet_info (applet_manager,
	                                       module_id,
	                                       applet_id,
	                                       &error) == NULL) {
		panel_applet_frame_loading_failed (module_id, applet_id, frame_act, error);
		panel_applet_frame_activating_free (frame_act);
		g_error_free (error);
		return;
	}

	if (gp_applet_manager_is_applet_disabled (applet_manager,
	                                          module_id,
	                                          applet_id,
	                                          NULL)) {
		panel_object_loader_stop_loading (application, id);
		return;
	}

	settings_path = panel_applet_frame_activating_get_settings_path (frame_act);
	initial_settings = get_initial_settings (frame_act);

	error = NULL;
	applet = gp_applet_manager_load_applet (applet_manager,
	                                        module_id,
	                                        applet_id,
	                                        settings_path,
	                                        initial_settings,
	                                        &error);

	g_clear_pointer (&initial_settings, g_variant_unref);
	g_free (settings_path);

	if (applet == NULL) {
		panel_applet_frame_loading_failed (module_id, applet_id, frame_act, error);
		panel_applet_frame_activating_free (frame_act);
		g_error_free (error);
		return;
	}

	remove_initial_settings (frame_act);

	frame_activate (module_id, applet_id, frame_act, applet);
	panel_applet_frame_activating_free (frame_act);
}

static void
migrate_iid_to_ids (PanelWidget *panel,
                    GSettings   *settings)
{
  char *applet_iid;
  const char *applet_id;
  char *module_id;
  GpApplication *application;
  GpAppletManager *applet_manager;

  applet_iid = g_settings_get_string (settings, PANEL_OBJECT_IID_KEY);

  if (*applet_iid == '\0')
    {
      g_free (applet_iid);
      return;
    }

  applet_id = g_strrstr (applet_iid, "::");
  module_id = g_strndup (applet_iid, strlen (applet_iid) - strlen (applet_id));
  applet_id += 2;

  application = panel_toplevel_get_application (panel->toplevel);
  applet_manager = gp_application_get_applet_manager (application);

  if (!gp_applet_manager_get_applet_info (applet_manager,
                                          module_id,
                                          applet_id,
                                          NULL))
    {
      char *new_iid;

      new_iid = gp_applet_manager_get_new_iid (applet_manager, applet_iid);

      if (new_iid != NULL)
        {
          g_free (applet_iid);
          g_free (module_id);

          applet_iid = new_iid;

          applet_id = g_strrstr (applet_iid, "::");
          module_id = g_strndup (applet_iid,
                                 strlen (applet_iid) - strlen (applet_id));
          applet_id += 2;
        }
    }

  g_settings_set_string (settings, PANEL_OBJECT_MODULE_ID_KEY, module_id);
  g_settings_set_string (settings, PANEL_OBJECT_APPLET_ID_KEY, applet_id);
  g_settings_reset (settings, PANEL_OBJECT_IID_KEY);

  g_free (applet_iid);
  g_free (module_id);
}

void
panel_applet_frame_load (PanelWidget *panel_widget,
			 const char  *id,
			 GSettings   *settings)
{
	char *module_id;
	char *applet_id;

	g_return_if_fail (panel_widget != NULL);
	g_return_if_fail (id != NULL);

	migrate_iid_to_ids (panel_widget, settings);

	module_id = g_settings_get_string (settings, PANEL_OBJECT_MODULE_ID_KEY);
	applet_id = g_settings_get_string (settings, PANEL_OBJECT_APPLET_ID_KEY);

	panel_applet_frame_load_helper (module_id,
	                                applet_id,
	                                panel_widget,
	                                id,
	                                settings);

	g_free (module_id);
	g_free (applet_id);
}
