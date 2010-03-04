/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * panel-applet-frame.c: panel side container for applets
 *
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

#include <libbonoboui.h>
#include <gconf/gconf.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "panel-applet-frame.h"
#include "panel-profile.h"
#include "panel-util.h"
#include "panel.h"
#include "applet.h"
#include "panel-marshal.h"
#include "panel-background.h"
#include "panel-lockdown.h"
#include "panel-stock-icons.h"
#include "xstuff.h"

G_DEFINE_TYPE (PanelAppletFrame, panel_applet_frame, GTK_TYPE_EVENT_BOX)

#define PANEL_APPLET_FRAME_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_APPLET_FRAME, PanelAppletFramePrivate))

#define HANDLE_SIZE 10

#define PROPERTY_ORIENT     "panel-applet-orient"
#define PROPERTY_SIZE       "panel-applet-size"
#define PROPERTY_BACKGROUND "panel-applet-background"
#define PROPERTY_FLAGS      "panel-applet-flags"
#define PROPERTY_SIZE_HINTS "panel-applet-size-hints"
#define PROPERTY_LOCKED_DOWN "panel-applet-locked-down"


struct _PanelAppletFramePrivate {
	GNOME_Vertigo_PanelAppletShell  applet_shell;
	CORBA_Object                    control;
	Bonobo_PropertyBag              property_bag;
	BonoboUIComponent              *ui_component;

	PanelWidget                    *panel;
	AppletInfo                     *applet_info;
	PanelOrientation                orientation;

	gchar                          *iid;

	GtkAllocation                   child_allocation;
	GdkRectangle                    handle_rect;

	guint                           has_handle : 1;
};

typedef struct {
	PanelAppletFrame *frame;
	gboolean          locked;
	int               position;
	gboolean          exactpos;
	char             *id;
} PanelAppletFrameActivating;

/* Keep in sync with panel-applet.h. Uggh.
 */	
typedef enum {
	APPLET_FLAGS_NONE   = 0,
	APPLET_EXPAND_MAJOR = 1 << 0,
	APPLET_EXPAND_MINOR = 1 << 1,
	APPLET_HAS_HANDLE   = 1 << 2
} PanelAppletFlags;

static GSList *no_reload_applets = NULL;

static void panel_applet_frame_cnx_broken (ORBitConnection  *cnx,
					   PanelAppletFrame *frame);

static void panel_applet_frame_activated (CORBA_Object object,
					  const char   *error_reason,
					  gpointer     data);

void
panel_applet_frame_sync_menu_state (PanelAppletFrame *frame)
{
	PanelWidget *panel_widget;
	gboolean     locked_down;
	gboolean     locked;
	gboolean     lockable;
	gboolean     movable;
	gboolean     removable;

	panel_widget = PANEL_WIDGET (GTK_WIDGET (frame)->parent);

	lockable = panel_applet_lockable (frame->priv->applet_info);
	movable = panel_applet_can_freely_move (frame->priv->applet_info);
	removable = panel_profile_id_lists_are_writable ();

	locked = panel_widget_get_applet_locked (panel_widget, GTK_WIDGET (frame));

	bonobo_ui_component_set_prop (frame->priv->ui_component,
				      "/commands/LockAppletToPanel",
				      "state",
				      locked ? "1" : "0",
				      NULL);

	/* First sensitivity */
	bonobo_ui_component_set_prop (frame->priv->ui_component,
				      "/commands/LockAppletToPanel",
				      "sensitive",
				      lockable ? "1" : "0",
				      NULL);

	bonobo_ui_component_set_prop (frame->priv->ui_component,
				      "/commands/RemoveAppletFromPanel",
				      "sensitive",
				      (locked && !lockable) ? "0" : (removable ? "1" : "0"),
				      NULL);

	bonobo_ui_component_set_prop (frame->priv->ui_component,
				      "/commands/MoveApplet",
				      "sensitive",
				      locked ? "0" : (movable ? "1" : "0"),
				      NULL);

	/* Second visibility */
	locked_down = panel_lockdown_get_locked_down ();

	bonobo_ui_component_set_prop (frame->priv->ui_component,
				      "/commands/LockAppletToPanel",
				      "hidden",
				      locked_down ? "1" : "0",
				      NULL);

	bonobo_ui_component_set_prop (frame->priv->ui_component,
				      "/commands/LockSeparator",
				      "hidden",
				      locked_down ? "1" : "0",
				      NULL);

	bonobo_ui_component_set_prop (frame->priv->ui_component,
				      "/commands/RemoveAppletFromPanel",
				      "hidden",
				      locked_down ? "1" : "0",
				      NULL);

	bonobo_ui_component_set_prop (frame->priv->ui_component,
				      "/commands/MoveApplet",
				      "hidden",
				      locked_down ? "1" : "0",
				      NULL);
}

static void
panel_applet_frame_set_flags_from_any (PanelAppletFrame *frame,
				       const CORBA_any  *any)
{
	gboolean major;
	gboolean minor;
	gboolean old_has_handle;
	int      flags;
	
	flags = BONOBO_ARG_GET_SHORT (any);

	major = flags & APPLET_EXPAND_MAJOR;
	minor = flags & APPLET_EXPAND_MINOR;

	panel_widget_set_applet_expandable (
		frame->priv->panel, GTK_WIDGET (frame), major, minor);

	old_has_handle = frame->priv->has_handle;
	frame->priv->has_handle = (flags & APPLET_HAS_HANDLE) != 0;

	if (!old_has_handle && frame->priv->has_handle) {
		/* we've added an handle, so we need to get the background for
		 * it */
		PanelBackground *background;

		background = &frame->priv->panel->background;
		panel_applet_frame_change_background (frame, background->type);
	}
}

static void
panel_applet_frame_set_size_hints_from_any (PanelAppletFrame *frame,
					    const CORBA_any  *any)
{
	CORBA_sequence_CORBA_long *seq;
	int                       *size_hints;
	int                        extra_size;
	int                        i;

	seq = any->_value;

	size_hints = g_new0 (int, seq->_length);

	if (frame->priv->has_handle) {
		extra_size = HANDLE_SIZE + 1;
	
		for (i = 0; i < seq->_length; i++)
			size_hints [i] = seq->_buffer [i] + extra_size;
	}
	
	panel_widget_set_applet_size_hints (frame->priv->panel,
					    GTK_WIDGET (frame),
					    size_hints,
					    seq->_length);
}

static void
panel_applet_frame_init_properties (PanelAppletFrame *frame)
{
	CORBA_any *any;

	any = bonobo_pbclient_get_value (frame->priv->property_bag,
					 PROPERTY_FLAGS,
					 BONOBO_ARG_SHORT,
					 NULL);
	if (any) {
		panel_applet_frame_set_flags_from_any (frame, any);
		CORBA_free (any);
	}
	
	any = bonobo_pbclient_get_value (frame->priv->property_bag,
					 PROPERTY_SIZE_HINTS,
					 TC_CORBA_sequence_CORBA_long,
					 NULL);
	if (any) {
		panel_applet_frame_set_size_hints_from_any (frame, any);
		CORBA_free (any);
	}
}

static void
popup_handle_remove (BonoboUIComponent *uic,
		     PanelAppletFrame  *frame,
		     const gchar       *verbname)
{
	AppletInfo *info;

	info = frame->priv->applet_info;
	frame->priv->applet_info = NULL;

	panel_profile_delete_object (info);
}

static void
listener_popup_handle_lock (BonoboUIComponent            *uic,
			    const char                   *path,
			    Bonobo_UIComponent_EventType  type,
			    const char                   *state,
			    gpointer                      data)
{
	PanelAppletFrame *frame;
	AppletInfo       *info;
	gboolean          s;

	g_assert (!strcmp (path, "LockAppletToPanel"));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state)
		return;

	frame = (PanelAppletFrame *) data;
	info = frame->priv->applet_info;
	s = (strcmp (state, "1") == 0);

	if (panel_widget_get_applet_locked (PANEL_WIDGET (info->widget->parent),
					    info->widget) == s)
		return;

	panel_applet_toggle_locked (frame->priv->applet_info);

	panel_applet_frame_sync_menu_state (frame);
}

static void
popup_handle_move (BonoboUIComponent *uic,
		   PanelAppletFrame  *frame,
		   const gchar       *verbname)
{
	GtkWidget *widget;

	g_return_if_fail (GTK_IS_WIDGET (frame));

	widget = GTK_WIDGET (frame);
	
	g_return_if_fail (PANEL_IS_WIDGET (widget->parent));

	panel_widget_applet_drag_start (
		PANEL_WIDGET (widget->parent), widget, PW_DRAG_OFF_CENTER, GDK_CURRENT_TIME);
}

static BonoboUIVerb popup_verbs [] = {
        BONOBO_UI_UNSAFE_VERB ("RemoveAppletFromPanel", popup_handle_remove),
        BONOBO_UI_UNSAFE_VERB ("MoveApplet",            popup_handle_move),

        BONOBO_UI_VERB_END
};

static void
panel_applet_frame_load (const gchar *iid,
			 PanelWidget *panel,
			 gboolean     locked,
			 int          position,
			 gboolean     exactpos,
			 const char  *id)
{
	PanelAppletFrame           *frame;
	CORBA_Environment           ev;
	PanelAppletFrameActivating *frame_act;

	g_return_if_fail (iid != NULL);
	g_return_if_fail (panel != NULL);
	g_return_if_fail (id != NULL);

	if (g_slist_find_custom (no_reload_applets, id,
				 (GCompareFunc) strcmp))
		return;

	if (panel_lockdown_is_applet_disabled (iid))
		return;

	frame = g_object_new (PANEL_TYPE_APPLET_FRAME, NULL);
	frame->priv->panel = panel;
	frame->priv->iid   = g_strdup (iid);

	frame_act = g_new (PanelAppletFrameActivating, 1);
	frame_act->frame    = frame;
	frame_act->locked   = locked;
	frame_act->position = position;
	frame_act->exactpos = exactpos;
	frame_act->id       = g_strdup (id);

	CORBA_exception_init (&ev);

	bonobo_activation_activate_from_id_async (frame->priv->iid, 0,
						  (BonoboActivationCallback) panel_applet_frame_activated,
						  frame_act, &ev);

	CORBA_exception_free (&ev);
}

void
panel_applet_frame_load_from_gconf (PanelWidget *panel_widget,
				    gboolean     locked,
				    int          position,
				    const char  *id)
{
	const char  *key;
	char        *applet_iid;

	g_return_if_fail (panel_widget != NULL);
	g_return_if_fail (id != NULL);

	key = panel_gconf_full_key (PANEL_GCONF_APPLETS, id, "bonobo_iid");
	applet_iid = gconf_client_get_string (panel_gconf_get_client (),
					      key, NULL);

	if (!applet_iid || !applet_iid[0])
		return;

	panel_applet_frame_load (applet_iid, panel_widget,
				 locked, position, TRUE, id);

	g_free (applet_iid);
}

void
panel_applet_frame_create (PanelToplevel *toplevel,
			   int            position,
			   const char    *iid)
{
	GConfClient *client;
	const char  *key;
	char        *id;

	g_return_if_fail (iid != NULL);

	client  = panel_gconf_get_client ();

	id = panel_profile_prepare_object (PANEL_OBJECT_BONOBO, toplevel, position, FALSE);

	key = panel_gconf_full_key (PANEL_GCONF_APPLETS, id, "bonobo_iid");
	gconf_client_set_string (client, key, iid, NULL);

	panel_profile_add_to_list (PANEL_GCONF_APPLETS, id);

	g_free (id);
}

void
panel_applet_frame_change_orientation (PanelAppletFrame *frame,
				       PanelOrientation  orientation)
{
	CORBA_unsigned_short orient = 0;

	if (orientation == frame->priv->orientation)
		return;

	frame->priv->orientation = orientation;

	switch (orientation) {
	case PANEL_ORIENTATION_TOP:
		orient = GNOME_Vertigo_PANEL_ORIENT_DOWN;
		break;
	case PANEL_ORIENTATION_BOTTOM:
		orient = GNOME_Vertigo_PANEL_ORIENT_UP;
		break;
	case PANEL_ORIENTATION_LEFT:
		orient = GNOME_Vertigo_PANEL_ORIENT_RIGHT;
		break;
	case PANEL_ORIENTATION_RIGHT:
		orient = GNOME_Vertigo_PANEL_ORIENT_LEFT;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	bonobo_pbclient_set_short (frame->priv->property_bag, 
				   PROPERTY_ORIENT,
				   orient,
				   NULL);

	gtk_widget_queue_resize (GTK_WIDGET (frame));
}

void
panel_applet_frame_change_size (PanelAppletFrame *frame,
				PanelSize         size)
{
	/* Normalise the size to the constants defined in
	 * the IDL.
	 */
	size = size <= PANEL_SIZE_XX_SMALL ? PANEL_SIZE_XX_SMALL :
	       size <= PANEL_SIZE_X_SMALL  ? PANEL_SIZE_X_SMALL  :
	       size <= PANEL_SIZE_SMALL    ? PANEL_SIZE_SMALL    :
	       size <= PANEL_SIZE_MEDIUM   ? PANEL_SIZE_MEDIUM   :
	       size <= PANEL_SIZE_LARGE    ? PANEL_SIZE_LARGE    :
	       size <= PANEL_SIZE_X_LARGE  ? PANEL_SIZE_X_LARGE  : PANEL_SIZE_XX_LARGE;
		 
	bonobo_pbclient_set_short (frame->priv->property_bag, 
				   PROPERTY_SIZE,
				   size,
				   NULL);
}

static char *
panel_applet_frame_get_background_string (PanelAppletFrame    *frame,
					  PanelWidget         *panel,
					  PanelBackgroundType  type)
{
	int x;
	int y;

	x = GTK_WIDGET (frame)->allocation.x;
	y = GTK_WIDGET (frame)->allocation.y;

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

void
panel_applet_frame_change_background (PanelAppletFrame    *frame,
				      PanelBackgroundType  type)
{
	char *bg_str;

	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));
	g_return_if_fail (PANEL_IS_WIDGET (GTK_WIDGET (frame)->parent));

	if (frame->priv->has_handle) {
		PanelBackground *background;

		background = &PANEL_WIDGET (GTK_WIDGET (frame)->parent)->background;
		panel_background_change_background_on_widget (background,
							      GTK_WIDGET (frame));
	}

	bg_str = panel_applet_frame_get_background_string (
			frame, PANEL_WIDGET (GTK_WIDGET (frame)->parent), type);

	if (bg_str != NULL) {
		bonobo_pbclient_set_string (frame->priv->property_bag,
					    PROPERTY_BACKGROUND,
					    bg_str, NULL);

		g_free (bg_str);
	}
}

static void
panel_applet_frame_finalize (GObject *object)
{
	PanelAppletFrame *frame = PANEL_APPLET_FRAME (object);

	panel_lockdown_notify_remove (G_CALLBACK (panel_applet_frame_sync_menu_state),
				      frame);

	if (frame->priv->control) {
		/* do this before unref'ing every bonobo stuff since it looks
		 * like we can receive some events when unref'ing them */
		ORBit_small_unlisten_for_broken (frame->priv->control,
						 G_CALLBACK (panel_applet_frame_cnx_broken));
		bonobo_object_release_unref (frame->priv->control, NULL);
		frame->priv->control = CORBA_OBJECT_NIL;
	}

	if (frame->priv->property_bag)
		bonobo_object_release_unref (
			frame->priv->property_bag, NULL);

	if (frame->priv->applet_shell)
		bonobo_object_release_unref (
			frame->priv->applet_shell, NULL);

	if (frame->priv->ui_component)
		bonobo_object_unref (
			BONOBO_OBJECT (frame->priv->ui_component));

	g_free (frame->priv->iid);
	frame->priv->iid = NULL;

        G_OBJECT_CLASS (panel_applet_frame_parent_class)->finalize (object);
}

static void
panel_applet_frame_paint (GtkWidget    *widget,
			  GdkRectangle *area)
{
	PanelAppletFrame *frame;

	frame = PANEL_APPLET_FRAME (widget);

	if (!frame->priv->has_handle)
		return;
  
	if (gtk_widget_is_drawable (widget)) {
		GtkOrientation orientation = GTK_ORIENTATION_HORIZONTAL;

		switch (frame->priv->orientation) {
		case PANEL_ORIENTATION_TOP:
		case PANEL_ORIENTATION_BOTTOM:
			orientation = GTK_ORIENTATION_VERTICAL;
			break;
		case PANEL_ORIENTATION_LEFT:
		case PANEL_ORIENTATION_RIGHT:
			orientation = GTK_ORIENTATION_HORIZONTAL;
			break;
		default:
			g_assert_not_reached ();
			break;
		}

		gtk_paint_handle (
			widget->style, widget->window,
			GTK_WIDGET_STATE (widget),
			GTK_SHADOW_OUT,
			area, widget, "handlebox",
			frame->priv->handle_rect.x,
                        frame->priv->handle_rect.y,
                        frame->priv->handle_rect.width,
                        frame->priv->handle_rect.height,
                        orientation);
	}
}

static gboolean
panel_applet_frame_expose (GtkWidget      *widget,
			   GdkEventExpose *event)
{
	if (gtk_widget_is_drawable (widget)) {
		GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->expose_event (widget, event);

		panel_applet_frame_paint (widget, &event->area);

	}

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
panel_applet_frame_size_request (GtkWidget      *widget,
				 GtkRequisition *requisition)
{
	PanelAppletFrame *frame;
	GtkBin           *bin;
	GtkRequisition    child_requisition;  

	frame = PANEL_APPLET_FRAME (widget);
	bin = GTK_BIN (widget);

	if (!frame->priv->has_handle) {
		GTK_WIDGET_CLASS (panel_applet_frame_parent_class)->size_request (widget, requisition);
		return;
	}
  
	requisition->width = 0;
	requisition->height = 0;
  
	if (bin->child && gtk_widget_get_visible (bin->child)) {
		gtk_widget_size_request (bin->child, &child_requisition);

		requisition->width  = child_requisition.width;
		requisition->height = child_requisition.height;
	}

	requisition->width += GTK_CONTAINER (widget)->border_width;
	requisition->height += GTK_CONTAINER (widget)->border_width;

	switch (frame->priv->orientation) {
	case PANEL_ORIENTATION_TOP:
	case PANEL_ORIENTATION_BOTTOM:
		requisition->width += HANDLE_SIZE;
		break;
	case PANEL_ORIENTATION_LEFT:
	case PANEL_ORIENTATION_RIGHT:
		requisition->height += HANDLE_SIZE;
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
	GtkAllocation     new_allocation;
	GtkAllocation     old_allocation;

	old_allocation.x      = widget->allocation.x;
	old_allocation.y      = widget->allocation.y;
	old_allocation.width  = widget->allocation.width;
	old_allocation.height = widget->allocation.height;

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

	widget->allocation = *allocation;

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
	 	gdk_window_invalidate_rect (widget->window, &widget->allocation, FALSE);

	if (gtk_widget_get_realized (widget)) {
		gdk_window_move_resize (widget->window,
			allocation->x + GTK_CONTAINER (widget)->border_width,
			allocation->y + GTK_CONTAINER (widget)->border_width,
			MAX (allocation->width - GTK_CONTAINER (widget)->border_width * 2, 0),
			MAX (allocation->height - GTK_CONTAINER (widget)->border_width * 2, 0));
	}

	if (bin->child && gtk_widget_get_visible (bin->child))
		gtk_widget_size_allocate (bin->child, &new_allocation);
  
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

	frame = PANEL_APPLET_FRAME (widget);

	if (!frame->priv->has_handle)
		return handled;

	if (event->window != widget->window)
		return FALSE;

	switch (event->button) {
	case 1:
	case 2:
		if (button_event_in_rect (event, &frame->priv->handle_rect)) {
			if (event->type == GDK_BUTTON_PRESS ||
			    event->type == GDK_2BUTTON_PRESS) {
				panel_widget_applet_drag_start (
					frame->priv->panel, GTK_WIDGET (frame),
					PW_DRAG_OFF_CURSOR, event->time);
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
			CORBA_Environment env;

			CORBA_exception_init (&env);

			gdk_pointer_ungrab (GDK_CURRENT_TIME);

			GNOME_Vertigo_PanelAppletShell_popup_menu (
					frame->priv->applet_shell,
					event->button,
					event->time, &env);
			if (BONOBO_EX (&env))
				g_warning (_("Exception from popup_menu '%s'\n"), env._id);

			CORBA_exception_free (&env);

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
		int          position = -1;
		gboolean     locked = FALSE;

		panel = frame->priv->panel;
		iid   = g_strdup (frame->priv->iid);

		if (info) {
			id = g_strdup (info->id);
			position  = panel_applet_get_position (info);
			locked = panel_widget_get_applet_locked (panel, info->widget);
			panel_applet_clean (info);
		}

		panel_applet_frame_load (iid, panel, locked,
					 position, TRUE, id);

		g_free (iid);
		g_free (id);

	} else if (info) {
		/* if we can't write to applets list we can't really delete
		   it, so we'll just ignore this.  FIXME: handle this
		   more correctly I suppose. */
		if (panel_profile_id_lists_are_writable ())
			panel_profile_delete_object (info);
	}

	g_object_unref (frame);
	gtk_widget_destroy (dialog);
}

static char *
panel_applet_frame_get_name (char *iid)
{
	Bonobo_ServerInfoList *list;
	char                  *query;
	char                  *retval = NULL;
	
	query = g_strdup_printf ("iid == '%s'", iid);

	list = bonobo_activation_query (query, NULL, NULL);
	if (list && list->_length > 0 && list->_buffer) {
		Bonobo_ServerInfo  *info = &list->_buffer [0];
		const char * const *langs;
		GSList             *langs_gslist;
		int                 i;

		langs = g_get_language_names ();

		langs_gslist = NULL;
		for (i = 0; langs[i]; i++)
			langs_gslist = g_slist_prepend (langs_gslist,
							(char *) langs[i]);

		langs_gslist = g_slist_reverse (langs_gslist);

		retval = g_strdup (bonobo_server_info_prop_lookup (
						info, "name", langs_gslist));

		g_slist_free (langs_gslist);
	}

	g_free (query);
	CORBA_free (list);

	return retval;
}

static void
panel_applet_frame_cnx_broken (ORBitConnection  *cnx,
			       PanelAppletFrame *frame)
{
	GtkWidget *dialog;
	GdkScreen *screen;
	char      *applet_name = NULL;
	char      *dialog_txt;

	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));

	screen = gtk_widget_get_screen (GTK_WIDGET (frame));

	if (xstuff_is_display_dead ())
		return;

	if (frame->priv->iid)
		applet_name = panel_applet_frame_get_name (frame->priv->iid);

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
	g_free (applet_name);
	g_free (dialog_txt);
}

enum {
	LOADING_FAILED_RESPONSE_DONT_DELETE,
	LOADING_FAILED_RESPONSE_DELETE
};
 
static void
panel_applet_frame_loading_failed_response (GtkWidget *dialog,
					    guint      response,
					    char      *id)
{
	gtk_widget_destroy (dialog);

	if (response == LOADING_FAILED_RESPONSE_DELETE &&
	    !panel_lockdown_get_locked_down () &&
	    panel_profile_id_lists_are_writable ()) {
		GSList *item;

		item = g_slist_find_custom (no_reload_applets, id,
					    (GCompareFunc) strcmp);
		if (item) {
			g_free (item->data);
			no_reload_applets = g_slist_delete_link (no_reload_applets,
								 item);
		}

		panel_profile_remove_from_list (PANEL_GCONF_APPLETS, id);
	}

	g_free (id);
}

static void
panel_applet_frame_loading_failed (PanelAppletFrame  *frame,
				   const char        *id)
{
	GtkWidget *dialog;
	char      *problem_txt;
	gboolean   locked_down;

	no_reload_applets = g_slist_prepend (no_reload_applets,
					     g_strdup (id));

	locked_down = panel_lockdown_get_locked_down ();

	problem_txt = g_strdup_printf (_("The panel encountered a problem "
					 "while loading \"%s\"."),
				       frame->priv->iid);

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
			       gtk_window_get_screen (GTK_WINDOW (frame->priv->panel->toplevel)));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (panel_applet_frame_loading_failed_response),
			  g_strdup (id));

	panel_widget_register_open_dialog (frame->priv->panel, dialog);
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);
	/* FIXME: http://bugzilla.gnome.org/show_bug.cgi?id=165132 */
	gtk_window_set_title (GTK_WINDOW (dialog), _("Error"));

	gtk_widget_show_all (dialog);

	g_free (frame->priv->iid);
	frame->priv->iid = NULL;

	gtk_widget_destroy (GTK_WIDGET (frame));
}

static void
panel_applet_frame_class_init (PanelAppletFrameClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

	gobject_class->finalize = panel_applet_frame_finalize;

	widget_class->expose_event         = panel_applet_frame_expose;
	widget_class->size_request         = panel_applet_frame_size_request;
	widget_class->size_allocate        = panel_applet_frame_size_allocate;
	widget_class->button_press_event   = panel_applet_frame_button_changed;
	widget_class->button_release_event = panel_applet_frame_button_changed;

	g_type_class_add_private (klass, sizeof (PanelAppletFramePrivate));
}

static void
panel_applet_frame_init (PanelAppletFrame *frame)
{
	frame->priv = PANEL_APPLET_FRAME_GET_PRIVATE (frame);

	frame->priv->applet_shell     = CORBA_OBJECT_NIL;
	frame->priv->property_bag     = CORBA_OBJECT_NIL;
	frame->priv->ui_component     = NULL;
	frame->priv->panel            = NULL;
	frame->priv->orientation      = PANEL_ORIENTATION_TOP;
	frame->priv->applet_info      = NULL;
	frame->priv->has_handle       = FALSE;
}

static GNOME_Vertigo_PanelAppletShell
panel_applet_frame_get_applet_shell (Bonobo_Control control)
{
	CORBA_Environment              env;
	GNOME_Vertigo_PanelAppletShell retval;

	CORBA_exception_init (&env);

	retval = Bonobo_Unknown_queryInterface (control, 
						"IDL:GNOME/Vertigo/PanelAppletShell:1.0",
						&env);
	if (BONOBO_EX (&env)) {
		g_warning (_("Unable to obtain AppletShell interface from control\n"));

		retval = CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&env);

	return retval;
}

static G_CONST_RETURN char *
panel_applet_frame_get_orient_string (PanelAppletFrame *frame,
				      PanelWidget      *panel)
{
	PanelOrientation  orientation;
	const char       *retval = NULL;

	orientation = panel_widget_get_applet_orientation (panel);

	switch (orientation) {
	case PANEL_ORIENTATION_TOP:
		retval = "down";
		break;
	case PANEL_ORIENTATION_BOTTOM:
		retval = "up";
		break;
	case PANEL_ORIENTATION_LEFT:
		retval = "right";
		break;
	case PANEL_ORIENTATION_RIGHT:
		retval = "left";
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return retval;
}

static G_CONST_RETURN char *
panel_applet_frame_get_size_string (PanelAppletFrame *frame,
				    PanelWidget      *panel)
{
	const char *retval = NULL;

	if (panel->sz <= PANEL_SIZE_XX_SMALL)
		retval = "xx-small";
	else if (panel->sz <= PANEL_SIZE_X_SMALL)
		retval = "x-small";
	else if (panel->sz <= PANEL_SIZE_SMALL)
		retval = "small";
	else if (panel->sz <= PANEL_SIZE_MEDIUM)
		retval = "medium";
	else if (panel->sz <= PANEL_SIZE_LARGE)
		retval = "large";
	else if (panel->sz <= PANEL_SIZE_X_LARGE)
		retval = "x-large";
	else
		retval = "xx-large";

	return retval;
}

static char *
panel_applet_frame_construct_item (PanelAppletFrame *frame,
				   PanelWidget      *panel,
				   const char       *id)
{
	char *retval;
	char *bg_str;
	gboolean locked_down;

	bg_str = panel_applet_frame_get_background_string (
				frame, panel, panel->background.type);

	if (bg_str == NULL)
		bg_str = g_strdup ("");

	locked_down = panel_lockdown_get_locked_down ();

	retval = g_strdup_printf (
			"prefs_key=/apps/panel/applets/%s/prefs;"
			"background=%s;orient=%s;size=%s;locked_down=%s",
			id, bg_str,
			panel_applet_frame_get_orient_string (frame, panel),
			panel_applet_frame_get_size_string (frame, panel),
			locked_down ? "true" : "false");

	g_free (bg_str);

	return retval;
}

static void
panel_applet_frame_event_listener (BonoboListener    *listener,
				   const char        *event,
				   const CORBA_any   *any,
				   CORBA_Environment *ev,
				   PanelAppletFrame  *frame)
{
	if (!strcmp (event, "Bonobo/Property:change:" PROPERTY_FLAGS))
		panel_applet_frame_set_flags_from_any (frame, any);

	else if (!strcmp (event, "Bonobo/Property:change:" PROPERTY_SIZE_HINTS))
		panel_applet_frame_set_size_hints_from_any (frame, any);
}

static void
panel_applet_frame_activated (CORBA_Object  object,
			      const char   *error_reason,
			      gpointer      data)
{
	PanelAppletFrameActivating *frame_act;
	PanelAppletFrame   *frame;
	GtkWidget          *widget;
	BonoboControlFrame *control_frame;
	Bonobo_Control      control;
	Bonobo_ItemContainer container;
	CORBA_Environment   corba_ev;
	AppletInfo         *info;
	char               *error;
	char               *item_name;

	widget = NULL;
	frame_act = (PanelAppletFrameActivating *) data;
	frame = frame_act->frame;

	/* according to the source of bonobo control == NULL && no
	   exception can happen, so handle it */
	if (error_reason != NULL || object == CORBA_OBJECT_NIL) {
		g_warning (G_STRLOC ": failed to load applet %s:\n%s",
			   frame->priv->iid, error_reason);
		goto error_out;
	}

	CORBA_exception_init (&corba_ev);

	item_name = panel_applet_frame_construct_item (frame,
						       frame->priv->panel,
						       frame_act->id);

	frame->priv->control = CORBA_OBJECT_NIL;
	container = Bonobo_Unknown_queryInterface (object,
						   "IDL:Bonobo/ItemContainer:1.0",
						   &corba_ev);
	if (!BONOBO_EX (&corba_ev) && container != CORBA_OBJECT_NIL) {
		Bonobo_Unknown containee;

		containee = Bonobo_ItemContainer_getObjectByName (container,
								  item_name,
								  TRUE,
								  &corba_ev);
		bonobo_object_release_unref (container, NULL);

		if (!BONOBO_EX (&corba_ev) && containee != CORBA_OBJECT_NIL) {
			frame->priv->control =
				Bonobo_Unknown_queryInterface (containee,
							       "IDL:Bonobo/Control:1.0",
							       &corba_ev);

			bonobo_object_release_unref (containee, NULL);
		}
	}
	g_free (item_name);

	if (frame->priv->control == CORBA_OBJECT_NIL) {
		error = bonobo_exception_get_text (&corba_ev);
		g_warning (G_STRLOC ": failed to get Bonobo/Control interface on applet %s:\n%s",
                           frame->priv->iid, error);
		CORBA_exception_free (&corba_ev);
		bonobo_object_release_unref (object, NULL);
		g_free (error);
		goto error_out;
	}

	widget = bonobo_widget_new_control_from_objref (frame->priv->control,
							CORBA_OBJECT_NIL);

	CORBA_exception_free (&corba_ev);
	bonobo_object_release_unref (object, NULL);

	if (!widget) {
		g_warning (G_STRLOC ": failed to load applet %s",
			   frame->priv->iid);
		goto error_out;
	}

	control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (widget));
	if (control_frame == NULL) {
		g_warning (G_STRLOC ": failed to load applet %s "
			   "(cannot get control frame)", frame->priv->iid);
		goto error_out;
	}

	frame->priv->property_bag = 
		bonobo_control_frame_get_control_property_bag (control_frame,
							       &corba_ev);
	if (frame->priv->property_bag == NULL || BONOBO_EX (&corba_ev)) {
		error = bonobo_exception_get_text (&corba_ev);
		CORBA_exception_free (&corba_ev);
		g_warning (G_STRLOC ": failed to load applet %s "
			   "(cannot get property bag):\n%s",
			   frame->priv->iid, error);
		g_free (error);
		goto error_out;
	}

	bonobo_event_source_client_add_listener (frame->priv->property_bag,
						 (BonoboListenerCallbackFn) panel_applet_frame_event_listener,
						 "Bonobo/Property:change:panel-applet",
						 NULL,
						 frame);
	
	frame->priv->ui_component =
		bonobo_control_frame_get_popup_component (control_frame,
							  &corba_ev);
	if (frame->priv->ui_component == NULL || BONOBO_EX (&corba_ev)) {
		error = bonobo_exception_get_text (&corba_ev);
		CORBA_exception_free (&corba_ev);
		g_warning (G_STRLOC ": failed to load applet %s "
			   "(cannot get popup component):\n%s",
			   frame->priv->iid, error);
		g_free (error);
		goto error_out;
	}

	bonobo_ui_util_set_ui (frame->priv->ui_component, DATADIR,
			       "GNOME_Panel_Popup.xml", "panel", NULL);

	bonobo_ui_component_add_listener (frame->priv->ui_component,
					  "LockAppletToPanel",
					  listener_popup_handle_lock,
					  frame);

	bonobo_ui_component_add_verb_list_with_data (
		frame->priv->ui_component, popup_verbs, frame);

	control = bonobo_control_frame_get_control (control_frame);
	if (!control) {
		CORBA_exception_free (&corba_ev);
		g_warning (G_STRLOC ": failed to load applet %s "
			   "(cannot get control)", frame->priv->iid);
		goto error_out;
	}

	frame->priv->applet_shell = panel_applet_frame_get_applet_shell (control);
	if (frame->priv->applet_shell == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&corba_ev);
		g_warning (G_STRLOC ": failed to load applet %s "
			   "(cannot get applet shell)", frame->priv->iid);
		goto error_out;
	}

	CORBA_exception_free (&corba_ev);

	ORBit_small_listen_for_broken (object,
				       G_CALLBACK (panel_applet_frame_cnx_broken),
				       frame);

	gtk_container_add (GTK_CONTAINER (frame), widget);

	gtk_widget_show_all (GTK_WIDGET (frame));

	info = panel_applet_register (GTK_WIDGET (frame), GTK_WIDGET (frame),
				      NULL, frame->priv->panel,
				      frame_act->locked, frame_act->position,
				      frame_act->exactpos, PANEL_OBJECT_BONOBO,
				      frame_act->id);
	frame->priv->applet_info = info;

	panel_widget_set_applet_size_constrained (frame->priv->panel,
						  GTK_WIDGET (frame), TRUE);

	panel_applet_frame_sync_menu_state (frame);
	panel_applet_frame_init_properties (frame);

	panel_lockdown_notify_add (G_CALLBACK (panel_applet_frame_sync_menu_state),
				   frame);

	panel_applet_stop_loading (frame_act->id);
	g_free (frame_act->id);
	g_free (frame_act);

	return;

error_out:
	panel_applet_frame_loading_failed (frame, frame_act->id);
	if (widget)
		g_object_unref (widget);
	panel_applet_stop_loading (frame_act->id);
	g_free (frame_act->id);
	g_free (frame_act);
}

void
panel_applet_frame_set_panel (PanelAppletFrame *frame,
			      PanelWidget      *panel)
{
	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));
	g_return_if_fail (PANEL_IS_WIDGET (panel));

	frame->priv->panel = panel;
}
