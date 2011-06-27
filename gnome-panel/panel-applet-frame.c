/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * panel-applet-frame.c: panel side container for applets
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
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
#include <string.h>

#include <glib/gi18n.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "panel-applets-manager.h"
#include "panel-bindings.h"
#include "panel.h"
#include "applet.h"
#include "panel-marshal.h"
#include "panel-background.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-object-loader.h"
#include "panel-schemas.h"
#include "panel-stock-icons.h"
#include "xstuff.h"
#include "panel-compatibility.h"

#include "panel-applet-frame.h"

static void panel_applet_frame_activating_free (PanelAppletFrameActivating *frame_act);

static void panel_applet_frame_loading_failed  (const char  *iid,
					        PanelWidget *panel,
					        const char  *id);

static void panel_applet_frame_load_helper     (const gchar *iid,
						PanelWidget *panel,
						const char  *id,
						GSettings   *settings);

struct _PanelAppletFrameActivating {
	PanelWidget *panel;
	char        *id;
	GSettings   *settings;
};

/* PanelAppletFrame implementation */

G_DEFINE_TYPE (PanelAppletFrame, panel_applet_frame, GTK_TYPE_EVENT_BOX)

#define PANEL_APPLET_FRAME_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_APPLET_FRAME, PanelAppletFramePrivate))

#define HANDLE_SIZE 10

struct _PanelAppletFramePrivate {
	PanelWidget     *panel;
	AppletInfo      *applet_info;

	PanelOrientation orientation;

	gchar           *iid;

	GtkAllocation    child_allocation;
	GdkRectangle     handle_rect;

	guint            has_handle : 1;
};

static gboolean
panel_applet_frame_draw (GtkWidget *widget,
                         cairo_t   *cr)
{
        PanelAppletFrame *frame = PANEL_APPLET_FRAME (widget);
	GtkStyleContext *context;

        if (GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->draw)
                GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->draw (widget, cr);

	if (!frame->priv->has_handle)
		return FALSE;

	context = gtk_widget_get_style_context (widget);
	gtk_style_context_save (context);
	gtk_style_context_set_state (context, gtk_widget_get_state_flags (widget));

	cairo_save (cr);
	cairo_rectangle (cr,
			 frame->priv->handle_rect.x,
			 frame->priv->handle_rect.y,
			 frame->priv->handle_rect.width,
			 frame->priv->handle_rect.height);
	cairo_clip (cr);
	gtk_render_handle (context, cr,
			   0, 0,
			   gtk_widget_get_allocated_width (widget),
			   gtk_widget_get_allocated_height (widget));
	cairo_restore (cr);

	gtk_style_context_restore (context);

        return FALSE;
}

static void
panel_applet_frame_update_background_size (PanelAppletFrame *frame,
					   GtkAllocation    *old_allocation,
					   GtkAllocation    *new_allocation)
{
	PanelBackground *background;

	if (old_allocation->x      == new_allocation->x &&
	    old_allocation->y      == new_allocation->y &&
	    old_allocation->width  == new_allocation->width &&
	    old_allocation->height == new_allocation->height)
		return;

	background = &frame->priv->panel->background;

	if (background->type == PANEL_BACK_NONE ||
	   (background->type == PANEL_BACK_COLOR && !background->has_alpha))
		return;

	panel_applet_frame_change_background (frame, background->type);
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
	GtkAllocation     old_allocation;
	GtkAllocation     widget_allocation;

	gtk_widget_get_allocation (widget, &widget_allocation);

	old_allocation.x      = widget_allocation.x;
	old_allocation.y      = widget_allocation.y;
	old_allocation.width  = widget_allocation.width;
	old_allocation.height = widget_allocation.height;

	frame = PANEL_APPLET_FRAME (widget);
	bin = GTK_BIN (widget);

	if (!frame->priv->has_handle) {
		GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->size_allocate (widget,
										   allocation);
		panel_applet_frame_update_background_size (frame,
							   &old_allocation,
							   allocation);
		return;
	}

	window = gtk_widget_get_window (widget);
	child = gtk_bin_get_child (bin);
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

	if (child && gtk_widget_get_visible (child))
		gtk_widget_size_allocate (child, &new_allocation);
  
	frame->priv->child_allocation = new_allocation;

	panel_applet_frame_update_background_size (frame,
						   &old_allocation,
						   allocation);
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

static gboolean
panel_applet_frame_button_changed (GtkWidget      *widget,
				   GdkEventButton *event)
{
	PanelAppletFrame *frame;
	gboolean          handled = FALSE;
	guint             modifiers;
	GdkDisplay       *display;
	GdkDevice        *pointer;
	GdkDeviceManager *device_manager;

	frame = PANEL_APPLET_FRAME (widget);

	if (!frame->priv->has_handle)
		return handled;

	if (event->window != gtk_widget_get_window (widget))
		return FALSE;

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
				handled = TRUE;
			} else if (event->type == GDK_BUTTON_RELEASE) {
				panel_widget_applet_drag_end (frame->priv->panel);
				handled = TRUE;
			}
		}
		break;
	case 3:
		if (event->type == GDK_BUTTON_PRESS ||
		    event->type == GDK_2BUTTON_PRESS) {
			display = gtk_widget_get_display (widget);
			device_manager = gdk_display_get_device_manager (display);
			pointer = gdk_device_manager_get_client_pointer (device_manager);
			gdk_device_ungrab (pointer, GDK_CURRENT_TIME);

			if (modifiers == panel_bindings_get_mouse_button_modifier_keymask ())
				PANEL_APPLET_FRAME_GET_CLASS (frame)->popup_edit_menu (frame,
										       event->button,
										       event->time);
			else
				PANEL_APPLET_FRAME_GET_CLASS (frame)->popup_menu (frame,
										  event->button,
										  event->time);

			handled = TRUE;
		} else if (event->type == GDK_BUTTON_RELEASE)
			handled = TRUE;
		break;
	default:
		break;
	}

	return handled;
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
panel_applet_frame_class_init (PanelAppletFrameClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

	gobject_class->finalize = panel_applet_frame_finalize;

	widget_class->draw                 = panel_applet_frame_draw;
	widget_class->get_preferred_width  = panel_applet_frame_get_preferred_width;
	widget_class->get_preferred_height = panel_applet_frame_get_preferred_height;
	widget_class->size_allocate        = panel_applet_frame_size_allocate;
	widget_class->button_press_event   = panel_applet_frame_button_changed;
	widget_class->button_release_event = panel_applet_frame_button_changed;

	g_type_class_add_private (klass, sizeof (PanelAppletFramePrivate));
}

static void
panel_applet_frame_init (PanelAppletFrame *frame)
{
	frame->priv = PANEL_APPLET_FRAME_GET_PRIVATE (frame);

	frame->priv->panel       = NULL;
	frame->priv->orientation = PANEL_ORIENTATION_TOP;
	frame->priv->applet_info = NULL;
	frame->priv->has_handle  = FALSE;
}

static void
panel_applet_frame_init_properties (PanelAppletFrame *frame)
{
	PANEL_APPLET_FRAME_GET_CLASS (frame)->init_properties (frame);
}

static void
panel_applet_frame_sync_menu_state (PanelLockdown *lockdown,
				    gpointer       user_data)
{
	PanelAppletFrame *frame = PANEL_APPLET_FRAME (user_data);
	gboolean          locked_down;
	gboolean          movable;
	gboolean          removable;

	movable = panel_applet_can_freely_move (frame->priv->applet_info);
	removable = panel_layout_is_writable ();
	locked_down = panel_lockdown_get_panels_locked_down_s ();

	PANEL_APPLET_FRAME_GET_CLASS (frame)->sync_menu_state (frame, movable, removable, locked_down);
}

void
panel_applet_frame_change_orientation (PanelAppletFrame *frame,
				       PanelOrientation  orientation)
{
	if (orientation == frame->priv->orientation)
		return;

	frame->priv->orientation = orientation;
	PANEL_APPLET_FRAME_GET_CLASS (frame)->change_orientation (frame, orientation);
}

void
panel_applet_frame_change_size (PanelAppletFrame *frame,
				guint             size)
{
	PANEL_APPLET_FRAME_GET_CLASS (frame)->change_size (frame, size);
}

void
panel_applet_frame_change_background (PanelAppletFrame    *frame,
				      PanelBackgroundType  type)
{
	GtkWidget *parent;

	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));

	parent = gtk_widget_get_parent (GTK_WIDGET (frame));

	g_return_if_fail (PANEL_IS_WIDGET (parent));

	if (frame->priv->has_handle) {
		PanelBackground *background;

		background = &PANEL_WIDGET (parent)->background;
		panel_background_change_background_on_widget (background,
							      GTK_WIDGET (frame));
	}

	PANEL_APPLET_FRAME_GET_CLASS (frame)->change_background (frame, type);
}

void
panel_applet_frame_set_panel (PanelAppletFrame *frame,
			      PanelWidget      *panel)
{
	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));
	g_return_if_fail (PANEL_IS_WIDGET (panel));

	frame->priv->panel = panel;
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
	gtk_widget_show_all (GTK_WIDGET (frame));

	info = panel_applet_register (GTK_WIDGET (frame), frame->priv->panel,
				      PANEL_OBJECT_APPLET, frame_act->id,
				      frame_act->settings,
				      GTK_WIDGET (frame), NULL);
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

void
_panel_applet_frame_update_flags (PanelAppletFrame *frame,
				  gboolean          major,
				  gboolean          minor,
				  gboolean          has_handle)
{
	gboolean old_has_handle;

	panel_widget_set_applet_expandable (
		frame->priv->panel, GTK_WIDGET (frame), major, minor);

	old_has_handle = frame->priv->has_handle;
	frame->priv->has_handle = has_handle;

	if (!old_has_handle && frame->priv->has_handle) {
		/* we've added an handle, so we need to get the background for
		 * it */
		PanelBackground *background;

		background = &frame->priv->panel->background;
		panel_applet_frame_change_background (frame, background->type);
	}
}

void
_panel_applet_frame_update_size_hints (PanelAppletFrame *frame,
				       gint             *size_hints,
				       guint             n_elements)
{
	if (frame->priv->has_handle) {
		gint extra_size = HANDLE_SIZE + 1;
		gint i;

		for (i = 0; i < n_elements; i++)
			size_hints[i] += extra_size;
	}

	/* It takes the ownership of size-hints array */
	panel_widget_set_applet_size_hints (frame->priv->panel,
					    GTK_WIDGET (frame),
					    size_hints,
					    n_elements);
}

char *
_panel_applet_frame_get_background_string (PanelAppletFrame    *frame,
					   PanelWidget         *panel,
					   PanelBackgroundType  type)
{
	GtkAllocation allocation;
	int x;
	int y;

	gtk_widget_get_allocation (GTK_WIDGET (frame), &allocation);

	x = allocation.x;
	y = allocation.y;

	if (frame->priv->has_handle) {
		switch (frame->priv->orientation) {
		case PANEL_ORIENTATION_TOP:
		case PANEL_ORIENTATION_BOTTOM:
			if (gtk_widget_get_direction (GTK_WIDGET (frame)) !=
			    GTK_TEXT_DIR_RTL)
				x += frame->priv->handle_rect.width;
			break;
		case PANEL_ORIENTATION_LEFT:
		case PANEL_ORIENTATION_RIGHT:
			y += frame->priv->handle_rect.height;
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}

	return panel_background_make_string (&panel->background, x, y);
}

static void
panel_applet_frame_reload_response (GtkWidget        *dialog,
				    int               response,
				    PanelAppletFrame *frame)
{
	AppletInfo *info;

	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));

	if (!frame->priv->iid || !frame->priv->panel) {
		g_object_unref (frame);
		gtk_widget_destroy (dialog);
		return;
	}

	info = frame->priv->applet_info;

	if (response == GTK_RESPONSE_YES) {
		PanelWidget *panel;
		char        *iid;
		char        *id = NULL;
		GSettings   *settings = NULL;

		panel = frame->priv->panel;
		iid   = g_strdup (frame->priv->iid);

		if (info) {
			id = g_strdup (info->id);
			settings = panel_applet_get_settings (info);
			g_object_ref (settings);
			panel_applet_clean (info);
		}

		panel_applet_frame_load_helper (iid, panel, id, settings);

		g_free (iid);
		g_free (id);
		if (settings)
			g_object_unref (settings);

	} else if (info) {
		/* if we can't write to applets list we can't really delete
		   it, so we'll just ignore this.  FIXME: handle this
		   more correctly I suppose. */
		if (panel_layout_is_writable ())
			panel_layout_delete_object (panel_applet_get_id (info));
	}

	g_object_unref (frame);
	gtk_widget_destroy (dialog);
}

void
_panel_applet_frame_applet_broken (PanelAppletFrame *frame)
{
	GtkWidget  *dialog;
	GdkScreen  *screen;
	const char *applet_name = NULL;
	char       *dialog_txt;

	screen = gtk_widget_get_screen (GTK_WIDGET (frame));

	if (xstuff_is_display_dead ())
		return;

	if (frame->priv->iid) {
		PanelAppletInfo *info;

		info = (PanelAppletInfo *)panel_applets_manager_get_applet_info (frame->priv->iid);
		applet_name = panel_applet_info_get_name (info);
	}

	if (applet_name)
		dialog_txt = g_strdup_printf (_("\"%s\" has quit unexpectedly"), applet_name);
	else
		dialog_txt = g_strdup (_("Panel object has quit unexpectedly"));

	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
					 dialog_txt, applet_name ? applet_name : NULL);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("If you reload a panel object, it will automatically "
						    "be added back to the panel."));

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("_Don't Reload"), GTK_RESPONSE_NO,
				_("_Reload"), GTK_RESPONSE_YES,
				NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_YES);

	gtk_window_set_screen (GTK_WINDOW (dialog), screen);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (panel_applet_frame_reload_response),
			  g_object_ref (frame));

	panel_widget_register_open_dialog (frame->priv->panel, dialog);
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);
	/* FIXME: http://bugzilla.gnome.org/show_bug.cgi?id=165132 */
	gtk_window_set_title (GTK_WINDOW (dialog), _("Error"));

	gtk_widget_show (dialog);
	g_free (dialog_txt);
}

void
_panel_applet_frame_applet_remove (PanelAppletFrame *frame)
{
	AppletInfo *info;

	if (!frame->priv->applet_info)
		return;

	info = frame->priv->applet_info;
	frame->priv->applet_info = NULL;

	panel_layout_delete_object (panel_applet_get_id (info));
}

void
_panel_applet_frame_applet_move (PanelAppletFrame *frame)
{
	GtkWidget *widget = GTK_WIDGET (frame);
	GtkWidget *parent = gtk_widget_get_parent (widget);

	if (!PANEL_IS_WIDGET (parent))
		return;

	panel_widget_applet_drag_start (PANEL_WIDGET (parent),
					widget,
					GDK_CURRENT_TIME);
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

GdkScreen *
panel_applet_frame_activating_get_screen (PanelAppletFrameActivating *frame_act)
{
	return gtk_widget_get_screen (GTK_WIDGET (frame_act->panel));
}

PanelOrientation
panel_applet_frame_activating_get_orientation (PanelAppletFrameActivating *frame_act)
{
	return panel_widget_get_applet_orientation (frame_act->panel);
}

guint32
panel_applet_frame_activating_get_size (PanelAppletFrameActivating *frame_act)
{
	return frame_act->panel->sz;
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
panel_applet_frame_activating_get_conf_path (PanelAppletFrameActivating *frame_act)
{
	return panel_layout_object_get_gconf_path (frame_act->id);
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
					GTK_STOCK_OK, LOADING_FAILED_RESPONSE_DONT_DELETE,
					NULL);
	} else {
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
					_("Do you want to delete the applet "
					  "from your configuration?"));
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					PANEL_STOCK_DONT_DELETE, LOADING_FAILED_RESPONSE_DONT_DELETE,
					GTK_STOCK_DELETE, LOADING_FAILED_RESPONSE_DELETE,
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

	g_return_if_fail (iid != NULL);
	g_return_if_fail (panel != NULL);
	g_return_if_fail (id != NULL);

	if (g_slist_find_custom (no_reload_applets, id,
				 (GCompareFunc) strcmp)) {
		panel_object_loader_stop_loading (id);
		return;
	}

	if (panel_lockdown_is_applet_disabled (panel_lockdown_get (), iid)) {
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

	applet_iid = panel_compatibility_get_applet_iid (settings, id);
	if (!applet_iid) {
		panel_object_loader_stop_loading (id);
		return;
	}

	panel_applet_frame_load_helper (applet_iid, panel_widget, id, settings);

	g_free (applet_iid);
}

void
panel_applet_frame_create (PanelToplevel       *toplevel,
			   PanelObjectPackType  pack_type,
			   int                  pack_index,
			   const char          *iid)
{
	g_return_if_fail (iid != NULL);

	panel_layout_object_create (PANEL_OBJECT_APPLET,
				    iid,
				    panel_toplevel_get_id (toplevel),
				    pack_type, pack_index);
}
