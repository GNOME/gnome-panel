/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * panel-applet-frame-bonobo.c: panel side container for applets
 *
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
 * Copyright (C) 2010 Vincent Untz <vuntz@gnome.org>
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

#include <panel-applet-frame.h>
#include <panel-applets-manager.h>

#include "GNOME_Panel.h"

#include "panel-applet-frame-bonobo.h"

G_DEFINE_TYPE (PanelAppletFrameBonobo,
	       panel_applet_frame_bonobo,
	       PANEL_TYPE_APPLET_FRAME)

struct _PanelAppletFrameBonoboPrivate
{
	GNOME_Vertigo_PanelAppletShell  applet_shell;
	CORBA_Object                    control;
	Bonobo_PropertyBag              property_bag;
	BonoboUIComponent              *ui_component;
};

#define PROPERTY_ORIENT      "panel-applet-orient"
#define PROPERTY_SIZE        "panel-applet-size"
#define PROPERTY_BACKGROUND  "panel-applet-background"
#define PROPERTY_FLAGS       "panel-applet-flags"
#define PROPERTY_SIZE_HINTS  "panel-applet-size-hints"
#define PROPERTY_LOCKED_DOWN "panel-applet-locked-down"

typedef enum {
       PANEL_SIZE_XX_SMALL = GNOME_Vertigo_PANEL_XX_SMALL,
       PANEL_SIZE_X_SMALL  = GNOME_Vertigo_PANEL_X_SMALL,
       PANEL_SIZE_SMALL    = GNOME_Vertigo_PANEL_SMALL,
       PANEL_SIZE_MEDIUM   = GNOME_Vertigo_PANEL_MEDIUM,
       PANEL_SIZE_LARGE    = GNOME_Vertigo_PANEL_LARGE,
       PANEL_SIZE_X_LARGE  = GNOME_Vertigo_PANEL_X_LARGE,
       PANEL_SIZE_XX_LARGE = GNOME_Vertigo_PANEL_XX_LARGE 
} PanelSize;

/* Keep in sync with panel-applet.h. Uggh. */
typedef enum {
	APPLET_FLAGS_NONE   = 0,
	APPLET_EXPAND_MAJOR = 1 << 0,
	APPLET_EXPAND_MINOR = 1 << 1,
	APPLET_HAS_HANDLE   = 1 << 2
} PanelAppletFlags;

GQuark
panel_applet_frame_bonobo_error_quark (void)
{
        static GQuark ret = 0;

        if (ret == 0) {
                ret = g_quark_from_static_string ("panel_applet_frame_bonobo_error");
        }

        return ret;
}

static guint
get_panel_applet_orient (PanelOrientation orientation)
{
	/* For some reason libpanel-applet and panel use a different logic for
	 * orientation, so we need to convert it. We should fix this. */
	switch (orientation) {
	case PANEL_ORIENTATION_TOP:
		return 1;
	case PANEL_ORIENTATION_BOTTOM:
		return 0;
	case PANEL_ORIENTATION_LEFT:
		return 3;
	case PANEL_ORIENTATION_RIGHT:
		return 2;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
panel_applet_frame_bonobo_update_flags (PanelAppletFrame *frame,
					const CORBA_any  *any)
{
	int      flags;
	gboolean major;
	gboolean minor;
	gboolean has_handle;
	
	flags = BONOBO_ARG_GET_SHORT (any);

	major = (flags & APPLET_EXPAND_MAJOR) != 0;
	minor = (flags & APPLET_EXPAND_MINOR) != 0;
	has_handle = (flags & APPLET_HAS_HANDLE) != 0;

	_panel_applet_frame_update_flags (frame, major, minor, has_handle);
}

static void
panel_applet_frame_bonobo_update_size_hints (PanelAppletFrame *frame,
					     const CORBA_any  *any)
{
	CORBA_sequence_CORBA_long *seq;

	seq = any->_value;

	_panel_applet_frame_update_size_hints (frame, seq->_buffer, seq->_length);
}

static void
panel_applet_frame_bonobo_init_properties (PanelAppletFrame *frame)
{
	PanelAppletFrameBonobo *bonobo_frame = PANEL_APPLET_FRAME_BONOBO (frame);
	CORBA_any *any;

	any = bonobo_pbclient_get_value (bonobo_frame->priv->property_bag,
					 PROPERTY_FLAGS,
					 BONOBO_ARG_SHORT,
					 NULL);
	if (any) {
		panel_applet_frame_bonobo_update_flags (frame, any);
		CORBA_free (any);
	}
	
	any = bonobo_pbclient_get_value (bonobo_frame->priv->property_bag,
					 PROPERTY_SIZE_HINTS,
					 TC_CORBA_sequence_CORBA_long,
					 NULL);
	if (any) {
		panel_applet_frame_bonobo_update_size_hints (frame, any);
		CORBA_free (any);
	}
}

static void
panel_applet_frame_bonobo_sync_menu_state (PanelAppletFrame *frame,
					   gboolean          movable,
					   gboolean          removable,
					   gboolean          lockable,
					   gboolean          locked,
					   gboolean          locked_down)
{
	PanelAppletFrameBonobo *bonobo_frame = PANEL_APPLET_FRAME_BONOBO (frame);

	bonobo_ui_component_set_prop (bonobo_frame->priv->ui_component,
				      "/commands/LockAppletToPanel",
				      "state",
				      locked ? "1" : "0",
				      NULL);

	/* First sensitivity */
	bonobo_ui_component_set_prop (bonobo_frame->priv->ui_component,
				      "/commands/LockAppletToPanel",
				      "sensitive",
				      lockable ? "1" : "0",
				      NULL);

	bonobo_ui_component_set_prop (bonobo_frame->priv->ui_component,
				      "/commands/RemoveAppletFromPanel",
				      "sensitive",
				      (locked && !lockable) ? "0" : (removable ? "1" : "0"),
				      NULL);

	bonobo_ui_component_set_prop (bonobo_frame->priv->ui_component,
				      "/commands/MoveApplet",
				      "sensitive",
				      locked ? "0" : (movable ? "1" : "0"),
				      NULL);

	bonobo_ui_component_set_prop (bonobo_frame->priv->ui_component,
				      "/commands/LockAppletToPanel",
				      "hidden",
				      locked_down ? "1" : "0",
				      NULL);

	bonobo_ui_component_set_prop (bonobo_frame->priv->ui_component,
				      "/commands/LockSeparator",
				      "hidden",
				      locked_down ? "1" : "0",
				      NULL);

	bonobo_ui_component_set_prop (bonobo_frame->priv->ui_component,
				      "/commands/RemoveAppletFromPanel",
				      "hidden",
				      locked_down ? "1" : "0",
				      NULL);

	bonobo_ui_component_set_prop (bonobo_frame->priv->ui_component,
				      "/commands/MoveApplet",
				      "hidden",
				      locked_down ? "1" : "0",
				      NULL);
}

static void
panel_applet_frame_bonobo_popup_menu (PanelAppletFrame *frame,
				      guint             button,
				      guint32           timestamp)
{
	PanelAppletFrameBonobo *bonobo_frame = PANEL_APPLET_FRAME_BONOBO (frame);
	CORBA_Environment env;

	CORBA_exception_init (&env);

	GNOME_Vertigo_PanelAppletShell_popup_menu (bonobo_frame->priv->applet_shell,
						   button, timestamp, &env);
	if (BONOBO_EX (&env))
		g_warning ("Exception from popup_menu '%s'\n", env._id);

	CORBA_exception_free (&env);
}

static void
panel_applet_frame_bonobo_change_orientation (PanelAppletFrame *frame,
					      PanelOrientation  orientation)
{
	PanelAppletFrameBonobo *bonobo_frame = PANEL_APPLET_FRAME_BONOBO (frame);
	CORBA_unsigned_short orient = 0;

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

	bonobo_pbclient_set_short (bonobo_frame->priv->property_bag, 
				   PROPERTY_ORIENT,
				   orient,
				   NULL);

	gtk_widget_queue_resize (GTK_WIDGET (frame));
}

static void
panel_applet_frame_bonobo_change_size (PanelAppletFrame *frame,
				       guint             size)
{
	PanelAppletFrameBonobo *bonobo_frame = PANEL_APPLET_FRAME_BONOBO (frame);

	/* Normalise the size to the constants defined in the IDL. */
	size = size <= PANEL_SIZE_XX_SMALL ? PANEL_SIZE_XX_SMALL :
	       size <= PANEL_SIZE_X_SMALL  ? PANEL_SIZE_X_SMALL  :
	       size <= PANEL_SIZE_SMALL    ? PANEL_SIZE_SMALL    :
	       size <= PANEL_SIZE_MEDIUM   ? PANEL_SIZE_MEDIUM   :
	       size <= PANEL_SIZE_LARGE    ? PANEL_SIZE_LARGE    :
	       size <= PANEL_SIZE_X_LARGE  ? PANEL_SIZE_X_LARGE  : PANEL_SIZE_XX_LARGE;
		 
	bonobo_pbclient_set_short (bonobo_frame->priv->property_bag, 
				   PROPERTY_SIZE,
				   size,
				   NULL);
}

static void
panel_applet_frame_bonobo_change_background (PanelAppletFrame    *frame,
					     PanelBackgroundType  type)
{
	PanelAppletFrameBonobo *bonobo_frame = PANEL_APPLET_FRAME_BONOBO (frame);
	char *bg_str;

	bg_str = _panel_applet_frame_get_background_string (
			frame, PANEL_WIDGET (GTK_WIDGET (frame)->parent), type);

	if (bg_str != NULL) {
		bonobo_pbclient_set_string (bonobo_frame->priv->property_bag,
					    PROPERTY_BACKGROUND,
					    bg_str, NULL);

		g_free (bg_str);
	}
}

static void
panel_applet_frame_bonobo_applet_broken (ORBitConnection  *cnx,
					 PanelAppletFrame *frame)
{
	_panel_applet_frame_applet_broken (frame);
}

static void
popup_handle_remove (BonoboUIComponent *uic,
		     PanelAppletFrame  *frame,
		     const gchar       *verbname)
{
	_panel_applet_frame_applet_remove (frame);
}

static void
popup_handle_move (BonoboUIComponent *uic,
		   PanelAppletFrame  *frame,
		   const gchar       *verbname)
{
	_panel_applet_frame_applet_move (frame);
}

static void
listener_popup_handle_lock (BonoboUIComponent            *uic,
			    const char                   *path,
			    Bonobo_UIComponent_EventType  type,
			    const char                   *state,
			    gpointer                      data)
{
	PanelAppletFrame *frame;
	gboolean          locked;

	g_assert (!strcmp (path, "LockAppletToPanel"));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state)
		return;

	frame = (PanelAppletFrame *) data;
	locked = (strcmp (state, "1") == 0);

	_panel_applet_frame_applet_lock (frame, locked);

	panel_applet_frame_sync_menu_state (frame);
}

static BonoboUIVerb popup_verbs [] = {
        BONOBO_UI_UNSAFE_VERB ("RemoveAppletFromPanel", popup_handle_remove),
        BONOBO_UI_UNSAFE_VERB ("MoveApplet",            popup_handle_move),

        BONOBO_UI_VERB_END
};


static void
panel_applet_frame_bonobo_finalize (GObject *object)
{
	PanelAppletFrameBonobo *frame = PANEL_APPLET_FRAME_BONOBO (object);

	if (frame->priv->control) {
		/* do this before unref'ing every bonobo stuff since it looks
		 * like we can receive some events when unref'ing them */
		ORBit_small_unlisten_for_broken (frame->priv->control,
						 G_CALLBACK (panel_applet_frame_bonobo_applet_broken));
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

	G_OBJECT_CLASS (panel_applet_frame_bonobo_parent_class)->finalize (object);
}

static void
panel_applet_frame_bonobo_init (PanelAppletFrameBonobo *frame)
{
	GtkWidget *container;

	frame->priv = G_TYPE_INSTANCE_GET_PRIVATE (frame,
						   PANEL_TYPE_APPLET_FRAME_BONOBO,
						   PanelAppletFrameBonoboPrivate);

	frame->priv->applet_shell = CORBA_OBJECT_NIL;
	frame->priv->control      = CORBA_OBJECT_NIL;
	frame->priv->property_bag = CORBA_OBJECT_NIL;
	frame->priv->ui_component = NULL;
}

static void
panel_applet_frame_bonobo_class_init (PanelAppletFrameBonoboClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);
	PanelAppletFrameClass *frame_class = PANEL_APPLET_FRAME_CLASS (class);

	gobject_class->finalize = panel_applet_frame_bonobo_finalize;

	frame_class->init_properties = panel_applet_frame_bonobo_init_properties;
	frame_class->sync_menu_state = panel_applet_frame_bonobo_sync_menu_state;
	frame_class->popup_menu = panel_applet_frame_bonobo_popup_menu;
	frame_class->change_orientation = panel_applet_frame_bonobo_change_orientation;
	frame_class->change_size = panel_applet_frame_bonobo_change_size;
	frame_class->change_background = panel_applet_frame_bonobo_change_background;

	g_type_class_add_private (class, sizeof (PanelAppletFrameBonoboPrivate));
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
		g_warning ("Unable to obtain AppletShell interface from control\n");

		retval = CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&env);

	return retval;
}

static G_CONST_RETURN char *
panel_applet_frame_get_orient_string (PanelAppletFrame           *frame,
				      PanelAppletFrameActivating *frame_act)
{
	PanelOrientation  orientation;
	const char       *retval = NULL;

	orientation = panel_applet_frame_activating_get_orientation (frame_act);

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
panel_applet_frame_get_size_string (PanelAppletFrame           *frame,
				    PanelAppletFrameActivating *frame_act)
{
	const char *retval = NULL;
	guint32 size;

	size = panel_applet_frame_activating_get_size (frame_act);

	if (size <= PANEL_SIZE_XX_SMALL)
		retval = "xx-small";
	else if (size <= PANEL_SIZE_X_SMALL)
		retval = "x-small";
	else if (size <= PANEL_SIZE_SMALL)
		retval = "small";
	else if (size <= PANEL_SIZE_MEDIUM)
		retval = "medium";
	else if (size <= PANEL_SIZE_LARGE)
		retval = "large";
	else if (size <= PANEL_SIZE_X_LARGE)
		retval = "x-large";
	else
		retval = "xx-large";

	return retval;
}

static char *
panel_applet_frame_construct_item (PanelAppletFrame           *frame,
				   PanelAppletFrameActivating *frame_act)
{
	char *retval;
	char *conf_path = NULL;
	char *bg_str = NULL;
	gboolean locked_down;

	conf_path = panel_applet_frame_activating_get_conf_path (frame_act);
	//FIXME vuntz
#if 0
	bg_str = _panel_applet_frame_get_background_string (
				frame, panel, panel->background.type);
#endif

	if (bg_str == NULL)
		bg_str = g_strdup ("");

	locked_down = panel_applet_frame_activating_get_locked_down (frame_act);

	retval = g_strdup_printf (
			"prefs_key=%s;"
			"background=%s;orient=%s;size=%s;locked_down=%s",
			conf_path, bg_str,
			panel_applet_frame_get_orient_string (frame, frame_act),
			panel_applet_frame_get_size_string (frame, frame_act),
			locked_down ? "true" : "false");

	g_free (conf_path);
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
		panel_applet_frame_bonobo_update_flags (frame, any);

	else if (!strcmp (event, "Bonobo/Property:change:" PROPERTY_SIZE_HINTS))
		panel_applet_frame_bonobo_update_size_hints (frame, any);
}

static void
panel_applet_frame_bonobo_activated (CORBA_Object  object,
				     const char   *error_reason,
				     gpointer      data)
{
	PanelAppletFrameActivating *frame_act;
	PanelAppletFrameBonobo *bonobo_frame;
	PanelAppletFrame   *frame;
	GtkWidget          *widget;
	BonoboControlFrame *control_frame;
	Bonobo_Control      control;
	Bonobo_ItemContainer container;
	CORBA_Environment   corba_ev;
	AppletInfo         *info;
	char               *error;
	char               *item_name;
	GError             *gerror = NULL;

	widget = NULL;
	bonobo_frame = PANEL_APPLET_FRAME_BONOBO (data);
	frame = PANEL_APPLET_FRAME (data);
	frame_act = g_object_get_data (G_OBJECT (frame), "panel-applet-frame-activating");
	g_object_set_data (G_OBJECT (frame), "panel-applet-frame-activating", NULL);

	/* according to the source of bonobo control == NULL && no
	   exception can happen, so handle it */
	if (error_reason != NULL || object == CORBA_OBJECT_NIL) {
		gerror = g_error_new_literal (panel_applet_frame_bonobo_error_quark (), 0, error_reason);
		goto error_out;
	}

	CORBA_exception_init (&corba_ev);

	item_name = panel_applet_frame_construct_item (frame,
						       frame_act);

	bonobo_frame->priv->control = CORBA_OBJECT_NIL;
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
			bonobo_frame->priv->control =
				Bonobo_Unknown_queryInterface (containee,
							       "IDL:Bonobo/Control:1.0",
							       &corba_ev);

			bonobo_object_release_unref (containee, NULL);
		}
	}
	g_free (item_name);

	if (bonobo_frame->priv->control == CORBA_OBJECT_NIL) {
		error = bonobo_exception_get_text (&corba_ev);
		gerror = g_error_new (panel_applet_frame_bonobo_error_quark (), 0, "failed to get Bonobo/Control interface:\n", error);
		CORBA_exception_free (&corba_ev);
		bonobo_object_release_unref (object, NULL);
		g_free (error);
		goto error_out;
	}

	widget = bonobo_widget_new_control_from_objref (bonobo_frame->priv->control,
							CORBA_OBJECT_NIL);

	CORBA_exception_free (&corba_ev);
	bonobo_object_release_unref (object, NULL);

	if (!widget) {
		gerror = g_error_new_literal (panel_applet_frame_bonobo_error_quark (), 0, "no widget created");
		goto error_out;
	}

	control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (widget));
	if (control_frame == NULL) {
		gerror = g_error_new_literal (panel_applet_frame_bonobo_error_quark (), 0, "cannot get control frame");
		goto error_out;
	}

	bonobo_frame->priv->property_bag = 
		bonobo_control_frame_get_control_property_bag (control_frame,
							       &corba_ev);
	if (bonobo_frame->priv->property_bag == NULL || BONOBO_EX (&corba_ev)) {
		error = bonobo_exception_get_text (&corba_ev);
		CORBA_exception_free (&corba_ev);
		gerror = g_error_new (panel_applet_frame_bonobo_error_quark (), 0, "cannot get property bag frame:\n%s", error);
		g_free (error);
		goto error_out;
	}

	bonobo_event_source_client_add_listener (bonobo_frame->priv->property_bag,
						 (BonoboListenerCallbackFn) panel_applet_frame_event_listener,
						 "Bonobo/Property:change:panel-applet",
						 NULL,
						 frame);
	
	bonobo_frame->priv->ui_component =
		bonobo_control_frame_get_popup_component (control_frame,
							  &corba_ev);
	if (bonobo_frame->priv->ui_component == NULL || BONOBO_EX (&corba_ev)) {
		error = bonobo_exception_get_text (&corba_ev);
		CORBA_exception_free (&corba_ev);
		gerror = g_error_new (panel_applet_frame_bonobo_error_quark (), 0, "cannot get popup component:\n%s", error);
		g_free (error);
		goto error_out;
	}

	bonobo_ui_util_set_ui (bonobo_frame->priv->ui_component, DATADIR,
			       "GNOME_Panel_Popup.xml", "panel", NULL);

	bonobo_ui_component_add_listener (bonobo_frame->priv->ui_component,
					  "LockAppletToPanel",
					  listener_popup_handle_lock,
					  frame);

	bonobo_ui_component_add_verb_list_with_data (
		bonobo_frame->priv->ui_component, popup_verbs, frame);

	control = bonobo_control_frame_get_control (control_frame);
	if (!control) {
		CORBA_exception_free (&corba_ev);
		gerror = g_error_new_literal (panel_applet_frame_bonobo_error_quark (), 0, "cannot get control");
		goto error_out;
	}

	bonobo_frame->priv->applet_shell = panel_applet_frame_get_applet_shell (control);
	if (bonobo_frame->priv->applet_shell == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&corba_ev);
		gerror = g_error_new_literal (panel_applet_frame_bonobo_error_quark (), 0, "cannot get applet shell");
		goto error_out;
	}

	CORBA_exception_free (&corba_ev);

	ORBit_small_listen_for_broken (object,
				       G_CALLBACK (panel_applet_frame_bonobo_applet_broken),
				       frame);

	gtk_container_add (GTK_CONTAINER (frame), widget);

	goto out;

error_out:
	if (widget)
		g_object_unref (widget);
	if (!gerror)
		gerror = g_error_new_literal (panel_applet_frame_bonobo_error_quark (), 0, "unknown error");

out:
	_panel_applet_frame_activated (frame, frame_act, gerror);
}

gboolean
panel_applet_frame_bonobo_load (const gchar                 *iid,
			        PanelAppletFrameActivating  *frame_act)
{
	PanelAppletFrameBonobo *bonobo_frame;
	PanelAppletFrame       *frame;
	CORBA_Environment       ev;

	g_return_val_if_fail (iid != NULL, FALSE);
	g_return_val_if_fail (frame_act != NULL, FALSE);

	if (!panel_applets_manager_factory_activate (iid))
		return FALSE;

	bonobo_frame = g_object_new (PANEL_TYPE_APPLET_FRAME_BONOBO, NULL);
	frame = PANEL_APPLET_FRAME (bonobo_frame);
	_panel_applet_frame_set_iid (frame, iid);

	g_object_set_data (G_OBJECT (frame), "panel-applet-frame-activating", frame_act);

	CORBA_exception_init (&ev);

	bonobo_activation_activate_from_id_async ((gchar *) iid, 0,
						  (BonoboActivationCallback) panel_applet_frame_bonobo_activated,
						  frame, &ev);

	CORBA_exception_free (&ev);

	return TRUE;
}
