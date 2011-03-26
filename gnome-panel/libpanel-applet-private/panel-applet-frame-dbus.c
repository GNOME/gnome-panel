/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * panel-applet-frame-dbus.c: panel side container for applets
 *
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include <panel-applet-frame.h>
#include <panel-applets-manager.h>

#include "panel-applet-container.h"

#include "panel-applet-frame-dbus.h"

G_DEFINE_TYPE (PanelAppletFrameDBus,
	       panel_applet_frame_dbus,
	       PANEL_TYPE_APPLET_FRAME)

struct _PanelAppletFrameDBusPrivate
{
	PanelAppletContainer *container;
	GCancellable         *bg_cancellable;
};

/* Keep in sync with panel-applet.h. Uggh. */
typedef enum {
	APPLET_FLAGS_NONE   = 0,
	APPLET_EXPAND_MAJOR = 1 << 0,
	APPLET_EXPAND_MINOR = 1 << 1,
	APPLET_HAS_HANDLE   = 1 << 2
} PanelAppletFlags;


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
panel_applet_frame_dbus_update_flags (PanelAppletFrame *frame,
				      GVariant         *value)
{
	guint32  flags;
	gboolean major;
	gboolean minor;
	gboolean has_handle;

	flags = g_variant_get_uint32 (value);

	major = (flags & APPLET_EXPAND_MAJOR) != 0;
	minor = (flags & APPLET_EXPAND_MINOR) != 0;
	has_handle = (flags & APPLET_HAS_HANDLE) != 0;

	_panel_applet_frame_update_flags (frame, major, minor, has_handle);
}


static void
panel_applet_frame_dbus_get_flags_cb (PanelAppletContainer *container,
				      GAsyncResult         *res,
				      PanelAppletFrame     *frame)
{
	GVariant *value;
	GError   *error = NULL;

	value = panel_applet_container_child_get_finish (container, res, &error);
	if (!value) {
		g_warning ("%s\n", error->message);
		g_error_free (error);
		return;
	}

	panel_applet_frame_dbus_update_flags (frame, value);
	g_variant_unref (value);
}

static void
panel_applet_frame_dbus_get_size_hints_cb (PanelAppletContainer *container,
					   GAsyncResult         *res,
					   PanelAppletFrame     *frame)
{
	GVariant   *value;
	const gint *sz;
	gint       *size_hints = NULL;
	gsize       n_elements;
	GError     *error = NULL;

	value = panel_applet_container_child_get_finish (container, res, &error);
	if (!value) {
		g_warning ("%s\n", error->message);
		g_error_free (error);
		return;
	}

	sz = g_variant_get_fixed_array (value, &n_elements, sizeof (gint32));
	if (n_elements > 0) {
		size_hints = g_new (gint32, n_elements);
		memcpy (size_hints, sz, n_elements * sizeof (gint32));
	}

	_panel_applet_frame_update_size_hints (frame, size_hints, n_elements);
	g_variant_unref (value);
}

static void
panel_applet_frame_dbus_init_properties (PanelAppletFrame *frame)
{
	PanelAppletFrameDBus *dbus_frame = PANEL_APPLET_FRAME_DBUS (frame);

	panel_applet_container_child_get (dbus_frame->priv->container, "flags", NULL,
					  (GAsyncReadyCallback) panel_applet_frame_dbus_get_flags_cb,
					  frame);
	panel_applet_container_child_get (dbus_frame->priv->container, "size-hints", NULL,
					  (GAsyncReadyCallback) panel_applet_frame_dbus_get_size_hints_cb,
					  frame);
}

static void
panel_applet_frame_dbus_sync_menu_state (PanelAppletFrame *frame,
					 gboolean          movable,
					 gboolean          removable,
					 gboolean          locked_down)
{
	PanelAppletFrameDBus *dbus_frame = PANEL_APPLET_FRAME_DBUS (frame);

	panel_applet_container_child_set (dbus_frame->priv->container,
					  "locked-down", g_variant_new_boolean (locked_down),
					  NULL, NULL, NULL);
}

static void
panel_applet_frame_dbus_popup_menu (PanelAppletFrame *frame,
				    guint             button,
				    guint32           timestamp)
{
	PanelAppletFrameDBus *dbus_frame = PANEL_APPLET_FRAME_DBUS (frame);

	panel_applet_container_child_popup_menu (dbus_frame->priv->container,
						 button, timestamp,
						 NULL, NULL, NULL);
}

static void
panel_applet_frame_dbus_popup_edit_menu (PanelAppletFrame *frame,
					 guint             button,
					 guint32           timestamp)
{
	PanelAppletFrameDBus *dbus_frame = PANEL_APPLET_FRAME_DBUS (frame);

	panel_applet_container_child_popup_edit_menu (dbus_frame->priv->container,
						      button, timestamp,
						      NULL, NULL, NULL);
}

static void
change_orientation_cb (PanelAppletContainer *container,
		       GAsyncResult         *res,
		       PanelAppletFrame     *frame)
{
	GError *error = NULL;

	if (!panel_applet_container_child_set_finish (container, res, &error)) {
		g_warning ("%s\n", error->message);
		g_error_free (error);

		return;
	}

	gtk_widget_queue_resize (GTK_WIDGET (frame));
}

static void
panel_applet_frame_dbus_change_orientation (PanelAppletFrame *frame,
					    PanelOrientation  orientation)
{
	PanelAppletFrameDBus *dbus_frame = PANEL_APPLET_FRAME_DBUS (frame);

	panel_applet_container_child_set (dbus_frame->priv->container,
					  "orient",
					  g_variant_new_uint32 (get_panel_applet_orient (orientation)),
					  NULL,
					  (GAsyncReadyCallback)change_orientation_cb,
					  frame);
}

static void
panel_applet_frame_dbus_change_size (PanelAppletFrame *frame,
				     guint             size)
{
	PanelAppletFrameDBus *dbus_frame = PANEL_APPLET_FRAME_DBUS (frame);

	panel_applet_container_child_set (dbus_frame->priv->container,
					  "size", g_variant_new_uint32 (size),
					  NULL, NULL, NULL);
}

static void
container_child_background_set (GObject      *source_object,
				GAsyncResult *res,
				gpointer      user_data)
{
	PanelAppletContainer *container = PANEL_APPLET_CONTAINER (source_object);
	PanelAppletFrameDBus *frame = PANEL_APPLET_FRAME_DBUS (user_data);

	panel_applet_container_child_set_finish (container, res, NULL);

	if (frame->priv->bg_cancellable)
		g_object_unref (frame->priv->bg_cancellable);
	frame->priv->bg_cancellable = NULL;
}

static void
panel_applet_frame_dbus_change_background (PanelAppletFrame    *frame,
					   PanelBackgroundType  type)
{
	PanelAppletFrameDBus *dbus_frame = PANEL_APPLET_FRAME_DBUS (frame);
	char *bg_str;

	bg_str = _panel_applet_frame_get_background_string (
			frame, PANEL_WIDGET (gtk_widget_get_parent (GTK_WIDGET (frame))), type);

	if (bg_str != NULL) {
		if (dbus_frame->priv->bg_cancellable)
			g_cancellable_cancel (dbus_frame->priv->bg_cancellable);
		dbus_frame->priv->bg_cancellable = g_cancellable_new ();

		panel_applet_container_child_set (dbus_frame->priv->container,
						  "background",
						  g_variant_new_string (bg_str),
						  dbus_frame->priv->bg_cancellable,
						  container_child_background_set,
						  dbus_frame);
		g_free (bg_str);
	}
}

static void
panel_applet_frame_dbus_flags_changed (PanelAppletContainer *container,
				       const gchar          *prop_name,
				       GVariant             *value,
				       PanelAppletFrame     *frame)
{
	panel_applet_frame_dbus_update_flags (frame, value);
}

static void
panel_applet_frame_dbus_size_hints_changed (PanelAppletContainer *container,
					    const gchar          *prop_name,
					    GVariant             *value,
					    PanelAppletFrame     *frame)
{
	const gint *sz;
	gint       *size_hints = NULL;
	gsize       n_elements;

	sz = g_variant_get_fixed_array (value, &n_elements, sizeof (gint32));
	if (n_elements > 0) {
		size_hints = g_new (gint32, n_elements);
		memcpy (size_hints, sz, n_elements * sizeof (gint32));
	}

	_panel_applet_frame_update_size_hints (frame, size_hints, n_elements);
}

static void
panel_applet_frame_dbus_applet_broken (PanelAppletContainer *container,
				       PanelAppletFrame     *frame)
{
	_panel_applet_frame_applet_broken (frame);
}

static void
panel_applet_frame_dbus_applet_remove (PanelAppletContainer *container,
				       PanelAppletFrame     *frame)
{
	_panel_applet_frame_applet_remove (frame);
}

static void
panel_applet_frame_dbus_applet_move (PanelAppletContainer *container,
				     PanelAppletFrame     *frame)
{
	_panel_applet_frame_applet_move (frame);
}

static void
panel_applet_frame_dbus_finalize (GObject *object)
{
	PanelAppletFrameDBus *frame = PANEL_APPLET_FRAME_DBUS (object);

	if (frame->priv->bg_cancellable)
		g_object_unref (frame->priv->bg_cancellable);
	frame->priv->bg_cancellable = NULL;

	G_OBJECT_CLASS (panel_applet_frame_dbus_parent_class)->finalize (object);
}

static void
panel_applet_frame_dbus_init (PanelAppletFrameDBus *frame)
{
	GtkWidget *container;

	frame->priv = G_TYPE_INSTANCE_GET_PRIVATE (frame,
						   PANEL_TYPE_APPLET_FRAME_DBUS,
						   PanelAppletFrameDBusPrivate);

	container = panel_applet_container_new ();
	gtk_widget_show (container);
	gtk_container_add (GTK_CONTAINER (frame), container);
	frame->priv->container = PANEL_APPLET_CONTAINER (container);

	g_signal_connect (container, "child-property-changed::flags",
			  G_CALLBACK (panel_applet_frame_dbus_flags_changed),
			  frame);
	g_signal_connect (container, "child-property-changed::size-hints",
			  G_CALLBACK (panel_applet_frame_dbus_size_hints_changed),
			  frame);
	g_signal_connect (container, "applet-broken",
			  G_CALLBACK (panel_applet_frame_dbus_applet_broken),
			  frame);
	g_signal_connect (container, "applet-remove",
			  G_CALLBACK (panel_applet_frame_dbus_applet_remove),
			  frame);
	g_signal_connect (container, "applet-move",
			  G_CALLBACK (panel_applet_frame_dbus_applet_move),
			  frame);
}

static void
panel_applet_frame_dbus_class_init (PanelAppletFrameDBusClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);
	PanelAppletFrameClass *frame_class = PANEL_APPLET_FRAME_CLASS (class);

	gobject_class->finalize = panel_applet_frame_dbus_finalize;

	frame_class->init_properties = panel_applet_frame_dbus_init_properties;
	frame_class->sync_menu_state = panel_applet_frame_dbus_sync_menu_state;
	frame_class->popup_menu = panel_applet_frame_dbus_popup_menu;
	frame_class->popup_edit_menu = panel_applet_frame_dbus_popup_edit_menu;
	frame_class->change_orientation = panel_applet_frame_dbus_change_orientation;
	frame_class->change_size = panel_applet_frame_dbus_change_size;
	frame_class->change_background = panel_applet_frame_dbus_change_background;

	g_type_class_add_private (class, sizeof (PanelAppletFrameDBusPrivate));
}

static void
panel_applet_frame_dbus_activated (PanelAppletContainer *container,
				   GAsyncResult         *res,
				   PanelAppletFrame     *frame)
{
	PanelAppletFrameActivating *frame_act;
	GError *error = NULL;

	if (!panel_applet_container_add_finish (container, res, &error))
		g_assert (error != NULL);

	frame_act = g_object_get_data (G_OBJECT (frame), "panel-applet-frame-activating");
	g_object_set_data (G_OBJECT (frame), "panel-applet-frame-activating", NULL);

	_panel_applet_frame_activated (frame, frame_act, error);
}

gboolean
panel_applet_frame_dbus_load (const gchar                 *iid,
			      PanelAppletFrameActivating  *frame_act)
{
	PanelAppletFrameDBus *dbus_frame;
	PanelAppletFrame     *frame;
	GVariantBuilder       builder;
	GdkScreen            *screen;
	gchar                *settings_path;
	gchar                *conf_path;
	gchar                *background;
	guint                 orient;

	g_return_val_if_fail (iid != NULL, FALSE);
	g_return_val_if_fail (frame_act != NULL, FALSE);

	if (!panel_applets_manager_factory_activate (iid))
		return FALSE;

	dbus_frame = g_object_new (PANEL_TYPE_APPLET_FRAME_DBUS, NULL);
	frame = PANEL_APPLET_FRAME (dbus_frame);
	_panel_applet_frame_set_iid (frame, iid);

	screen = panel_applet_frame_activating_get_screen (frame_act);
	orient = get_panel_applet_orient (panel_applet_frame_activating_get_orientation (frame_act));
	settings_path = panel_applet_frame_activating_get_settings_path (frame_act);
	conf_path = panel_applet_frame_activating_get_conf_path (frame_act);
	/* we can't really get a background string at this point since we don't
	 * know the position of the applet */
	background = NULL;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&builder, "{sv}",
			       "settings-path",
			       g_variant_new_string (settings_path));
	g_variant_builder_add (&builder, "{sv}",
			       "prefs-key",
			       g_variant_new_string (conf_path));
	g_variant_builder_add (&builder, "{sv}",
			       "orient",
			       g_variant_new_uint32 (orient));
	g_variant_builder_add (&builder, "{sv}",
			       "size",
			       g_variant_new_uint32 (panel_applet_frame_activating_get_size (frame_act)));
	g_variant_builder_add (&builder, "{sv}",
			       "locked-down",
			       g_variant_new_boolean (panel_applet_frame_activating_get_locked_down (frame_act)));
	if (background) {
		g_variant_builder_add (&builder, "{sv}",
				       "background",
				       g_variant_new_string (background));
	}

	g_object_set_data (G_OBJECT (frame), "panel-applet-frame-activating", frame_act);

	panel_applet_container_add (dbus_frame->priv->container,
				    screen, iid, NULL,
				    (GAsyncReadyCallback) panel_applet_frame_dbus_activated,
				    frame,
				    g_variant_builder_end (&builder));

	g_free (conf_path);
	g_free (settings_path);
	g_free (background);

	return TRUE;
}
