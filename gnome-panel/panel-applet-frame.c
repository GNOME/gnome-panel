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

#include <libbonoboui.h>
#include <gconf/gconf.h>
#include <libgnome/libgnome.h>
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

#define PANEL_APPLET_FRAME_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_APPLET_FRAME, PanelAppletFramePrivate))

#undef PANEL_APPLET_FRAME_DEBUG

#define HANDLE_SIZE 10

#define PANEL_STOCK_DONT_DELETE "panel-dont-delete"

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
	gboolean			moving_focus_out;

	GtkAllocation                   child_allocation;
	GdkRectangle                    handle_rect;

	guint                           has_handle : 1;
};

static GObjectClass *parent_class;

/* Keep in sync with panel-applet.h. Uggh.
 */	
typedef enum {
	APPLET_FLAGS_NONE   = 0,
	APPLET_EXPAND_MAJOR = 1 << 0,
	APPLET_EXPAND_MINOR = 1 << 1,
	APPLET_HAS_HANDLE   = 1 << 2
} PanelAppletFlags;

static void panel_applet_frame_cnx_broken (ORBitConnection  *cnx,
					   PanelAppletFrame *frame);

static void
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
	removable = panel_profile_list_is_writable (PANEL_GCONF_APPLETS);

	locked = panel_widget_get_applet_locked (panel_widget, GTK_WIDGET (frame));

	bonobo_ui_component_set_prop (frame->priv->ui_component,
				      "/popups/button3/placeholder/lock",
				      "label",
				      locked ? _("Un_lock") : _("_Lock"),
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
				      removable ? "1" : "0",
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
	int      flags;
	
	flags = BONOBO_ARG_GET_SHORT (any);

	major = flags & APPLET_EXPAND_MAJOR;
	minor = flags & APPLET_EXPAND_MINOR;

	panel_widget_set_applet_expandable (
		frame->priv->panel, GTK_WIDGET (frame), major, minor);

	frame->priv->has_handle = (flags & APPLET_HAS_HANDLE) != 0;
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

	extra_size = 0;
	if (frame->priv->has_handle)
		extra_size = HANDLE_SIZE + 1;
	
	for (i = 0; i < seq->_length; i++)
		size_hints [i] = seq->_buffer [i] + extra_size;
	
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
popup_handle_lock (BonoboUIComponent *uic,
		   PanelAppletFrame  *frame,
		   const char        *verb)
{
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
	BONOBO_UI_UNSAFE_VERB ("LockAppletToPanel",     popup_handle_lock),
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
	PanelAppletFrame *frame;
	GtkWidget        *widget;
	AppletInfo       *info;

	g_return_if_fail (iid != NULL);
	g_return_if_fail (panel != NULL);
	g_return_if_fail (id != NULL);

	widget = panel_applet_frame_new (panel, iid, id);
	if (!widget)
		return;
	
	gtk_widget_show_all (widget);

	info = panel_applet_register (widget, widget, NULL,
				      panel, locked, position,
				      exactpos, PANEL_OBJECT_BONOBO, id);

	panel_widget_set_applet_size_constrained (panel, widget, TRUE);

	frame = PANEL_APPLET_FRAME (widget);

	panel_applet_frame_set_info (frame, info);
	panel_applet_frame_sync_menu_state (frame);
	panel_applet_frame_init_properties (frame);

	panel_lockdown_notify_add (G_CALLBACK (panel_applet_frame_sync_menu_state),
				   frame);
}

void
panel_applet_frame_load_from_gconf (PanelWidget *panel_widget,
				    gboolean     locked,
				    int          position,
				    const char  *id)
{
	const char  *profile;
	const char  *key;
	char        *applet_iid;

	g_return_if_fail (panel_widget != NULL);
	g_return_if_fail (id != NULL);

	profile = panel_profile_get_name ();

	key = panel_gconf_full_key (PANEL_GCONF_APPLETS, profile, id, "bonobo_iid");
	applet_iid = gconf_client_get_string (panel_gconf_get_client (), key, NULL);

	panel_applet_frame_load (applet_iid, panel_widget, locked, position, TRUE, id);

	g_free (applet_iid);
}

void
panel_applet_frame_create (PanelToplevel *toplevel,
			   int            position,
			   const char    *iid)
{
	GConfClient *client;
	const char  *profile;
	const char  *key;
	char        *id;

	g_return_if_fail (iid != NULL);

	client  = panel_gconf_get_client ();
	profile = panel_profile_get_name ();

	id = panel_profile_prepare_object (PANEL_OBJECT_BONOBO, toplevel, position, FALSE);

	key = panel_gconf_full_key (PANEL_GCONF_APPLETS, profile, id, "bonobo_iid");
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
	return panel_background_make_string (
			&panel->background,
			GTK_WIDGET (frame)->allocation.x,
			GTK_WIDGET (frame)->allocation.y);
}

void
panel_applet_frame_change_background (PanelAppletFrame    *frame,
				      PanelBackgroundType  type)
{
	char *bg_str;

	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));
	g_return_if_fail (PANEL_IS_WIDGET (GTK_WIDGET (frame)->parent));

	bg_str = panel_applet_frame_get_background_string (
			frame, PANEL_WIDGET (GTK_WIDGET (frame)->parent), type);

	if (bg_str != NULL) {
		bonobo_pbclient_set_string (frame->priv->property_bag,
					    PROPERTY_BACKGROUND,
					    bg_str, NULL);

		g_free (bg_str);
	}
}

void
panel_applet_frame_set_info (PanelAppletFrame *frame,
			     AppletInfo       *info)
{
	frame->priv->applet_info = info;
}

static void
panel_applet_frame_finalize (GObject *object)
{
	PanelAppletFrame *frame = PANEL_APPLET_FRAME (object);

	panel_lockdown_notify_remove (G_CALLBACK (panel_applet_frame_sync_menu_state),
				      frame);

	if (frame->priv->property_bag)
		bonobo_object_release_unref (
			frame->priv->property_bag, NULL);

	if (frame->priv->applet_shell)
		bonobo_object_release_unref (
			frame->priv->applet_shell, NULL);

	if (frame->priv->ui_component)
		bonobo_object_unref (
			BONOBO_OBJECT (frame->priv->ui_component));

	if (frame->priv->control) {
		ORBit_small_unlisten_for_broken (frame->priv->control,
						 G_CALLBACK (panel_applet_frame_cnx_broken));
		CORBA_Object_release (frame->priv->control, NULL);
		frame->priv->control = CORBA_OBJECT_NIL;
	}

	g_free (frame->priv->iid);
	frame->priv->iid = NULL;

        parent_class->finalize (object);
}

static void
panel_applet_frame_paint (GtkWidget    *widget,
			  GdkRectangle *area)
{
	PanelAppletFrame *frame;

	frame = PANEL_APPLET_FRAME (widget);

	if (!frame->priv->has_handle)
		return;
  
	if (GTK_WIDGET_DRAWABLE (widget)) {
		GtkOrientation orientation = GTK_ORIENTATION_HORIZONTAL;

		switch (frame->priv->orientation) {
		case PANEL_ORIENTATION_TOP:
		case PANEL_ORIENTATION_BOTTOM:
			orientation = GTK_ORIENTATION_HORIZONTAL;
			break;
		case PANEL_ORIENTATION_LEFT:
		case PANEL_ORIENTATION_RIGHT:
			orientation = GTK_ORIENTATION_VERTICAL;
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
	if (GTK_WIDGET_DRAWABLE (widget)) {
		GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);

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
		GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);
		return;
	}
  
	requisition->width = 0;
	requisition->height = 0;
  
	if (bin->child && GTK_WIDGET_VISIBLE (bin->child)) {
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
		GTK_WIDGET_CLASS (parent_class)->size_allocate (widget,
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

		new_allocation.x      = HANDLE_SIZE;
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
	if (GTK_WIDGET_MAPPED (widget) &&
	    (new_allocation.x != frame->priv->child_allocation.x ||
	     new_allocation.y != frame->priv->child_allocation.y ||
	     new_allocation.width != frame->priv->child_allocation.width ||
	     new_allocation.height != frame->priv->child_allocation.height))
	 	gdk_window_invalidate_rect (widget->window, &widget->allocation, FALSE);

	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_move_resize (widget->window,
			allocation->x + GTK_CONTAINER (widget)->border_width,
			allocation->y + GTK_CONTAINER (widget)->border_width,
			MAX (allocation->width - GTK_CONTAINER (widget)->border_width * 2, 0),
			MAX (allocation->height - GTK_CONTAINER (widget)->border_width * 2, 0));
	}

	if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
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

	if (!frame->priv->iid || !frame->priv->panel)
		return;

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
		if (panel_profile_list_is_writable (PANEL_GCONF_APPLETS))
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
		Bonobo_ServerInfo *info = &list->_buffer [0];
		const GList       *langs_glist;
		GSList            *langs_gslist;

		/* Evil evil evil evil, we need to convert to
		 * a GSList from a GList */
		langs_glist = gnome_i18n_get_language_list ("LC_MESSAGES");
		langs_gslist = NULL;
		while (langs_glist) {
			langs_gslist = g_slist_append (langs_gslist, langs_glist->data);
			langs_glist = langs_glist->next;
		}

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
	char      *applet_txt;
	char      *dialog_txt;
	gboolean   locked_down;

	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));

	screen = gtk_widget_get_screen (GTK_WIDGET (frame));

	if (frame->priv->iid)
		applet_name = panel_applet_frame_get_name (frame->priv->iid);

	locked_down = panel_lockdown_get_locked_down ();

	applet_txt = g_strdup_printf (
			_("The \"%s\" applet appears to have died "
			  "unexpectedly.\n\n"
			  "Do you want to reload this applet?"),
			applet_name ? applet_name : "");
	if (!locked_down) {
		dialog_txt = g_strconcat(applet_txt,
				_("\n\n"
				  "(If you choose not to reload it at this time"
				  " you can always add it by right clicking on "
				  "the panel and clicking on the "
				  "\"Add to Panel\" submenu)"),
				NULL);
		g_free (applet_txt);
	} else {
		dialog_txt = applet_txt;
	}

	dialog = gtk_message_dialog_new (
				NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_YES_NO,
				dialog_txt);
	gtk_window_set_screen (GTK_WINDOW (dialog), screen);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (panel_applet_frame_reload_response),
			  g_object_ref (frame));

	panel_widget_register_open_dialog (frame->priv->panel, dialog);

	gtk_widget_show (dialog);
	g_free (applet_name);
	g_free (dialog_txt);
}

static inline void
register_stock_item (void)
{
        static gboolean registered = FALSE;

        if (!registered) {
                GtkIconFactory      *factory;
                GtkIconSet          *cancel_icons;

                static GtkStockItem  dont_delete_item [] = {
                        { PANEL_STOCK_DONT_DELETE, N_("D_on't Delete"), 0, 0, GETTEXT_PACKAGE },
                };

                cancel_icons = gtk_icon_factory_lookup_default (GTK_STOCK_CANCEL);

                factory = gtk_icon_factory_new ();

                gtk_icon_factory_add (factory, PANEL_STOCK_DONT_DELETE, cancel_icons);

                gtk_icon_factory_add_default (factory);

                gtk_stock_add_static (dont_delete_item, 1);

                registered = TRUE;
        }
}

enum {
	LOADING_FAILED_RESPONSE_DONT_DELETE,
	LOADING_FAILED_RESPONSE_DELETE
};

static void
panel_applet_frame_loading_failed (PanelAppletFrame  *frame,
				   CORBA_Environment *ev,
				   const char        *iid,
				   const char        *id,
				   GtkWindow         *panel)
{
	GtkWidget *dialog;
	int        response;
	char      *problem_txt;
	gboolean   locked_down;

	locked_down = panel_lockdown_get_locked_down ();

	problem_txt = g_strdup_printf (_("The panel encountered a problem "
					 "while loading \"%s\"."), iid);

	if (ev != NULL) {
		char *error = bonobo_exception_get_text (ev);
		char *detail_txt = g_strdup_printf (_("Details: %s"), error);

		dialog = gtk_message_dialog_new (
					NULL, 0,
					locked_down ? GTK_MESSAGE_INFO : GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_NONE,
					"%s\n%s%s",
					problem_txt,
					detail_txt,
					locked_down ? "" :
					_("\n\n"
					  "Do you want to delete the applet "
					  "from your configuration?"));
		g_free (error);
		g_free (detail_txt);
	} else {
		dialog = gtk_message_dialog_new (
					NULL, 0,
					locked_down ? GTK_MESSAGE_INFO : GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_NONE,
					"%s%s",
					problem_txt,
					locked_down ? "" :
					_("\n\n"
					  "Do you want to delete the applet "
					  "from your configuration?"));
	}

	register_stock_item ();

	if (locked_down) {
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					GTK_STOCK_OK, LOADING_FAILED_RESPONSE_DONT_DELETE,
					NULL);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), LOADING_FAILED_RESPONSE_DONT_DELETE);
	} else {
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					PANEL_STOCK_DONT_DELETE, LOADING_FAILED_RESPONSE_DONT_DELETE,
					GTK_STOCK_DELETE, LOADING_FAILED_RESPONSE_DELETE,
					NULL);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), LOADING_FAILED_RESPONSE_DELETE);
	}

	gtk_window_set_screen (GTK_WINDOW (dialog),
			       gtk_window_get_screen (panel));

        response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	if (response == LOADING_FAILED_RESPONSE_DELETE &&
	    !locked_down &&
	    panel_profile_list_is_writable (PANEL_GCONF_APPLETS))
		panel_profile_remove_from_list (PANEL_GCONF_APPLETS, id);
}

static void
panel_applet_frame_class_init (PanelAppletFrameClass *klass,
			       gpointer               dummy)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = panel_applet_frame_finalize;

	widget_class->expose_event         = panel_applet_frame_expose;
	widget_class->size_request         = panel_applet_frame_size_request;
	widget_class->size_allocate        = panel_applet_frame_size_allocate;
	widget_class->button_press_event   = panel_applet_frame_button_changed;
	widget_class->button_release_event = panel_applet_frame_button_changed;

	g_type_class_add_private (klass, sizeof (PanelAppletFramePrivate));
}

static void
panel_applet_frame_instance_init (PanelAppletFrame      *frame,
				  PanelAppletFrameClass *klass)
{
	frame->priv = PANEL_APPLET_FRAME_GET_PRIVATE (frame);

	frame->priv->applet_shell     = CORBA_OBJECT_NIL;
	frame->priv->property_bag     = CORBA_OBJECT_NIL;
	frame->priv->ui_component     = NULL;
	frame->priv->panel            = NULL;
	frame->priv->orientation      = PANEL_ORIENTATION_TOP;
	frame->priv->applet_info      = NULL;
	frame->priv->moving_focus_out = FALSE;
	frame->priv->has_handle       = FALSE;
}

GType
panel_applet_frame_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelAppletFrameClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_applet_frame_class_init,
			NULL,
			NULL,
			sizeof (PanelAppletFrame),
			0,
			(GInstanceInitFunc) panel_applet_frame_instance_init,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_EVENT_BOX,
					       "PanelAppletFrame",
					       &info, 0);
	}

	return type;
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
panel_applet_frame_construct_moniker (PanelAppletFrame *frame,
				      PanelWidget      *panel,
				      const char       *iid,
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
			"%s!prefs_key=/apps/panel/profiles/%s/applets/%s/prefs;"
			"background=%s;orient=%s;size=%s;locked_down=%s",
			iid, panel_profile_get_name (), id, bg_str,
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

GtkWidget *
panel_applet_frame_construct (PanelAppletFrame *frame,
			      PanelWidget      *panel,
			      const char       *iid,
			      const char       *id)
{
	BonoboControlFrame *control_frame;
	Bonobo_Control      control;
	CORBA_Environment   ev;
	GtkWidget          *widget;
	char               *moniker;

	frame->priv->panel = panel;

	if (panel_lockdown_is_applet_disabled (iid))
		return NULL;

	moniker = panel_applet_frame_construct_moniker (frame, panel, iid, id);

	/* FIXME: this should really use bonobo_get_object_async */
	CORBA_exception_init (&ev);

	control = bonobo_get_object (
		moniker, "IDL:Bonobo/Control:1.0", &ev);

	g_free (moniker);

	/* according to the source of bonobo control == NULL && no
	   exception can happen, so handle it */
	if (BONOBO_EX (&ev) || control == CORBA_OBJECT_NIL) {
		panel_applet_frame_loading_failed (
			frame, &ev, iid, id, GTK_WINDOW (panel->toplevel));
		CORBA_exception_free (&ev);
		return NULL;
	}

	frame->priv->iid = g_strdup (iid);

	frame->priv->control = CORBA_Object_duplicate (control, NULL);
	ORBit_small_listen_for_broken (control,
				       G_CALLBACK (panel_applet_frame_cnx_broken),
				       frame);

	widget = bonobo_widget_new_control_from_objref (control, CORBA_OBJECT_NIL);

	bonobo_object_release_unref (control, NULL);

	if (!widget) {
		CORBA_exception_free (&ev);
		panel_applet_frame_loading_failed (
			frame, NULL, iid, id, GTK_WINDOW (panel->toplevel));
		g_warning (G_STRLOC ": failed to load applet %s", iid);
		return NULL;
	}

	control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (widget));
	if (control_frame == NULL) {
		CORBA_exception_free (&ev);
		panel_applet_frame_loading_failed (
			frame, NULL, iid, id, GTK_WINDOW (panel->toplevel));
		g_warning (G_STRLOC ": failed to load applet %s (can't get control frame)", iid);
		gtk_object_sink (GTK_OBJECT (widget));
		return NULL;
	}

	frame->priv->property_bag = 
		bonobo_control_frame_get_control_property_bag (control_frame, &ev);
	if (frame->priv->property_bag == NULL || BONOBO_EX (&ev)) {
		panel_applet_frame_loading_failed (
			frame, &ev, iid, id, GTK_WINDOW (panel->toplevel));
		CORBA_exception_free (&ev);
		g_warning (G_STRLOC ": failed to load applet %s (can't get property bag)", iid);
		gtk_object_sink (GTK_OBJECT (widget));
		return NULL;
	}

	bonobo_event_source_client_add_listener (frame->priv->property_bag,
						 (BonoboListenerCallbackFn) panel_applet_frame_event_listener,
						 "Bonobo/Property:change:panel-applet",
						 NULL,
						 frame);
	
	frame->priv->ui_component =
		bonobo_control_frame_get_popup_component (control_frame, &ev);
	if (frame->priv->ui_component == NULL || BONOBO_EX (&ev)) {
		panel_applet_frame_loading_failed (
			frame, &ev, iid, id, GTK_WINDOW (panel->toplevel));
		CORBA_exception_free (&ev);
		g_warning (G_STRLOC ": failed to load applet %s (can't get popup component)", iid);
		gtk_object_sink (GTK_OBJECT (widget));
		return NULL;
	}

	bonobo_ui_util_set_ui (frame->priv->ui_component, DATADIR,
			       "GNOME_Panel_Popup.xml", "panel", NULL);

	bonobo_ui_component_add_verb_list_with_data (
		frame->priv->ui_component, popup_verbs, frame);

	control = bonobo_control_frame_get_control (control_frame);
	if (!control) {
		CORBA_exception_free (&ev);
		panel_applet_frame_loading_failed (
			frame, NULL, iid, id, GTK_WINDOW (panel->toplevel));
		g_warning (G_STRLOC ": failed to load applet %s (can't get control)", iid);
		gtk_object_sink (GTK_OBJECT (widget));
		return NULL;
	}

	frame->priv->applet_shell = panel_applet_frame_get_applet_shell (control);
	if (frame->priv->applet_shell == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&ev);
		panel_applet_frame_loading_failed (
			frame, NULL, iid, id, GTK_WINDOW (panel->toplevel));
		g_warning (G_STRLOC ": failed to load applet %s (can't get applet shell)", iid);
		gtk_object_sink (GTK_OBJECT (widget));
		return NULL;
	}

	CORBA_exception_free (&ev);

	gtk_container_add (GTK_CONTAINER (frame), widget);

	return widget;
}

GtkWidget *
panel_applet_frame_new (PanelWidget *panel,
			const char  *iid,
			const char  *id)
{
	PanelAppletFrame *frame;

	g_return_val_if_fail (iid != NULL && id != NULL, NULL);

	frame = g_object_new (PANEL_TYPE_APPLET_FRAME, NULL);

	if (!panel_applet_frame_construct (frame, panel, iid, id))
		return NULL;

	return GTK_WIDGET (frame);
}

PanelWidget *
panel_applet_frame_get_panel (PanelAppletFrame *frame)
{
	g_return_val_if_fail (PANEL_IS_APPLET_FRAME (frame), NULL);

	return frame->priv->panel;
}

void
panel_applet_frame_set_panel (PanelAppletFrame *frame,
			      PanelWidget      *panel)
{
	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));
	g_return_if_fail (PANEL_IS_WIDGET (panel));

	frame->priv->panel = panel;
}
