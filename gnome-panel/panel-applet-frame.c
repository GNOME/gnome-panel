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

#include <gconf/gconf.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "panel-applet-frame.h"
#include "panel-applets-manager.h"
#include "panel-profile.h"
#include "panel-util.h"
#include "panel.h"
#include "applet.h"
#include "panel-marshal.h"
#include "panel-background.h"
#include "panel-lockdown.h"
#include "panel-stock-icons.h"
#include "xstuff.h"
#include "panel-compatibility.h"

G_DEFINE_TYPE (PanelAppletFrame, panel_applet_frame, PANEL_TYPE_APPLET_CONTAINER)

#define PANEL_APPLET_FRAME_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_APPLET_FRAME, PanelAppletFramePrivate))

#define HANDLE_SIZE 10
#define PANEL_APPLET_PREFS_KEY "/apps/panel/applets/%s/prefs"

struct _PanelAppletFramePrivate {
	PanelWidget     *panel;
	AppletInfo      *applet_info;

	PanelOrientation orientation;

	gchar           *iid;

	GtkAllocation    child_allocation;
	GdkRectangle     handle_rect;

	guint            has_handle : 1;

	GCancellable    *bg_cancellable;
};

typedef struct {
	gboolean  locked;
	int       position;
	gboolean  exactpos;
	char     *id;
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

static char *panel_applet_frame_get_background_string (PanelAppletFrame    *frame,
						       PanelWidget         *panel,
						       PanelBackgroundType  type);

static void
panel_applet_frame_update_flags (PanelAppletFrame *frame,
				 guint             flags)
{
	gboolean major;
	gboolean minor;
	gboolean old_has_handle;

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
panel_applet_frame_update_size_hints (PanelAppletFrame *frame,
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

static void
panel_applet_frame_get_flags_cb (PanelAppletContainer *container,
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

	panel_applet_frame_update_flags (frame, g_variant_get_uint32 (value));
	g_variant_unref (value);
}

static void
panel_applet_frame_get_size_hints_cb (PanelAppletContainer *container,
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
	panel_applet_frame_update_size_hints (frame, size_hints, n_elements);
	g_variant_unref (value);
}

static void
panel_applet_frame_flags_changed (PanelAppletContainer *container,
				  const gchar          *prop_name,
				  GVariant             *value,
				  PanelAppletFrame     *frame)
{
	panel_applet_frame_update_flags (frame, g_variant_get_uint32 (value));
}

static void
panel_applet_frame_size_hints_changed (PanelAppletContainer *container,
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
	panel_applet_frame_update_size_hints (frame, size_hints, n_elements);
}

static void
panel_applet_frame_init_properties (PanelAppletFrame *frame)
{
	PanelAppletContainer *container = PANEL_APPLET_CONTAINER (frame);

	panel_applet_container_child_get (container, "flags", NULL,
					  (GAsyncReadyCallback)panel_applet_frame_get_flags_cb,
					  frame);
	panel_applet_container_child_get (container, "size-hints", NULL,
					  (GAsyncReadyCallback)panel_applet_frame_get_size_hints_cb,
					  frame);
	g_signal_connect (container, "child-property-changed::flags",
			  G_CALLBACK (panel_applet_frame_flags_changed),
			  frame);
	g_signal_connect (container, "child-property-changed::size-hints",
			  G_CALLBACK (panel_applet_frame_size_hints_changed),
			  frame);
}

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
	locked_down = panel_lockdown_get_locked_down ();

	panel_applet_container_child_set (PANEL_APPLET_CONTAINER (frame),
					  "locked", g_variant_new_boolean (lockable && locked),
					  NULL, NULL, NULL);
	panel_applet_container_child_set (PANEL_APPLET_CONTAINER (frame),
					  "locked-down", g_variant_new_boolean (locked_down),
					  NULL, NULL, NULL);
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

static guint
get_panel_applet_orient (PanelOrientation orientation)
{
	/* For some reason libpanel-applet and panel use
	 * a different logic for orientation, so we need
	 * to convert it. We should fix this.
	 */
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
panel_applet_frame_activated (PanelAppletContainer       *container,
			      GAsyncResult               *res,
			      PanelAppletFrameActivating *frame_act)
{
	PanelAppletFrame   *frame = PANEL_APPLET_FRAME (container);
	GtkWidget          *widget = NULL;
	AppletInfo         *info;
	GError             *error = NULL;

	if (!panel_applet_container_add_finish (container, res, &error)) {
		g_warning (G_STRLOC ": failed to load applet %s:\n%s",
			   frame->priv->iid, error->message);
		g_error_free (error);
		goto error_out;
	}

	gtk_widget_show_all (GTK_WIDGET (frame));

	info = panel_applet_register (GTK_WIDGET (frame), GTK_WIDGET (frame),
				      NULL, frame->priv->panel,
				      frame_act->locked, frame_act->position,
				      frame_act->exactpos, PANEL_OBJECT_APPLET,
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

static void
panel_applet_frame_load (const gchar *iid,
			 PanelWidget *panel,
			 gboolean     locked,
			 int          position,
			 gboolean     exactpos,
			 const char  *id)
{
	PanelAppletFrame           *frame;
	PanelAppletFrameActivating *frame_act;
	GVariantBuilder             builder;
	gchar                      *prefs_key;
	gchar                      *background;
	guint                       orient;

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

	if (!panel_applets_manager_factory_activate (iid)) {
		panel_applet_frame_loading_failed (frame, id);
		panel_applet_stop_loading (id);

		return;
	}

	frame_act = g_new (PanelAppletFrameActivating, 1);
	frame_act->locked   = locked;
	frame_act->position = position;
	frame_act->exactpos = exactpos;
	frame_act->id       = g_strdup (id);

	prefs_key = g_strdup_printf (PANEL_APPLET_PREFS_KEY, frame_act->id);
	background = panel_applet_frame_get_background_string (frame,
							       frame->priv->panel,
							       frame->priv->panel->background.type);
	orient = get_panel_applet_orient (panel_widget_get_applet_orientation (frame->priv->panel));

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&builder, "{sv}",
			       "prefs-key",
			       g_variant_new_string (prefs_key));
	g_variant_builder_add (&builder, "{sv}",
			       "orient",
			       g_variant_new_uint32 (orient));
	g_variant_builder_add (&builder, "{sv}",
			       "size",
			       g_variant_new_uint32 (frame->priv->panel->sz));
	g_variant_builder_add (&builder, "{sv}",
			       "locked",
			       g_variant_new_boolean (locked));
	g_variant_builder_add (&builder, "{sv}",
			       "locked-down",
			       g_variant_new_boolean (panel_lockdown_get_locked_down ()));
	if (background) {
		g_variant_builder_add (&builder, "{sv}",
				       "background",
				       g_variant_new_string (background));
	}

	panel_applet_container_add (PANEL_APPLET_CONTAINER (frame),
				    frame->priv->iid, NULL,
				    (GAsyncReadyCallback)panel_applet_frame_activated,
				    frame_act,
				    g_variant_builder_end (&builder));
	g_free (prefs_key);
	g_free (background);
}

void
panel_applet_frame_load_from_gconf (PanelWidget *panel_widget,
				    gboolean     locked,
				    int          position,
				    const char  *id)
{
	gchar *applet_iid;

	g_return_if_fail (panel_widget != NULL);
	g_return_if_fail (id != NULL);

	applet_iid = panel_compatiblity_get_applet_iid (id);
	if (!applet_iid)
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

	client =  panel_gconf_get_client ();

	id = panel_profile_prepare_object (PANEL_OBJECT_APPLET, toplevel, position, FALSE);

	key = panel_gconf_full_key (PANEL_GCONF_APPLETS, id, "applet_iid");
	gconf_client_set_string (client, key, iid, NULL);

	panel_profile_add_to_list (PANEL_GCONF_APPLETS, id);

	g_free (id);
}

static void
change_orientation_cb (PanelAppletContainer *container,
		       GAsyncResult         *res)
{
	GError   *error = NULL;

	if (!panel_applet_container_child_set_finish (container, res, &error)) {
		g_warning ("%s\n", error->message);
		g_error_free (error);

		return;
	}

	gtk_widget_queue_resize (GTK_WIDGET (container));
}

void
panel_applet_frame_change_orientation (PanelAppletFrame *frame,
				       PanelOrientation  orientation)
{
	if (orientation == frame->priv->orientation)
		return;

	frame->priv->orientation = orientation;
	panel_applet_container_child_set (PANEL_APPLET_CONTAINER (frame),
					  "orient",
					  g_variant_new_uint32 (get_panel_applet_orient (orientation)),
					  NULL,
					  (GAsyncReadyCallback)change_orientation_cb,
					  NULL);
}

void
panel_applet_frame_change_size (PanelAppletFrame *frame,
				guint             size)
{
	panel_applet_container_child_set (PANEL_APPLET_CONTAINER (frame),
					  "size", g_variant_new_uint32 (size),
					  NULL, NULL, NULL);
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

static void
container_child_background_set (GObject      *source_object,
				GAsyncResult *res,
				gpointer      user_data)
{
	PanelAppletContainer *container = PANEL_APPLET_CONTAINER (source_object);
	PanelAppletFrame     *frame = PANEL_APPLET_FRAME (source_object);

	panel_applet_container_child_set_finish (container, res, NULL);

	if (frame->priv->bg_cancellable)
		g_object_unref (frame->priv->bg_cancellable);
	frame->priv->bg_cancellable = NULL;
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
		if (frame->priv->bg_cancellable)
			g_cancellable_cancel (frame->priv->bg_cancellable);
		frame->priv->bg_cancellable = g_cancellable_new ();

		panel_applet_container_child_set (PANEL_APPLET_CONTAINER (frame),
						  "background",
						  g_variant_new_string (bg_str),
						  frame->priv->bg_cancellable,
						  container_child_background_set,
						  NULL);
		g_free (bg_str);
	}
}

static void
panel_applet_frame_finalize (GObject *object)
{
	PanelAppletFrame *frame = PANEL_APPLET_FRAME (object);

	panel_applets_manager_factory_deactivate (frame->priv->iid);

	panel_lockdown_notify_remove (G_CALLBACK (panel_applet_frame_sync_menu_state),
				      frame);

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
  
	if (GTK_WIDGET_DRAWABLE (widget)) {
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
	if (GTK_WIDGET_DRAWABLE (widget)) {
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
			PanelAppletContainer *container;

			container = PANEL_APPLET_CONTAINER (frame);
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			panel_applet_container_child_popup_menu (container,
								 event->button,
								 event->time,
								 NULL, NULL, NULL);
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

static void
panel_applet_frame_applet_broken (PanelAppletContainer *container)
{
	PanelAppletFrame *frame = PANEL_APPLET_FRAME (container);
	GtkWidget        *dialog;
	GdkScreen        *screen;
	const char       *applet_name = NULL;
	char             *dialog_txt;

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

static void
panel_applet_frame_applet_move (PanelAppletContainer *container)
{
	GtkWidget *widget = GTK_WIDGET (container);

	if (!PANEL_IS_WIDGET (widget->parent))
		return;

	panel_widget_applet_drag_start (PANEL_WIDGET (widget->parent),
					widget,
					PW_DRAG_OFF_CENTER,
					GDK_CURRENT_TIME);
}

static void
panel_applet_frame_applet_remove (PanelAppletContainer *container)
{
	PanelAppletFrame *frame = PANEL_APPLET_FRAME (container);
	AppletInfo       *info;

	if (!frame->priv->applet_info)
		return;

	info = frame->priv->applet_info;
	frame->priv->applet_info = NULL;

	panel_profile_delete_object (info);
}

static void
panel_applet_frame_applet_lock (PanelAppletContainer *container,
				gboolean              locked)
{
	PanelAppletFrame *frame = PANEL_APPLET_FRAME (container);
	PanelWidget      *panel_widget = PANEL_WIDGET (GTK_WIDGET (frame)->parent);

	if (panel_widget_get_applet_locked (panel_widget, GTK_WIDGET (frame)) == locked)
		return;

	panel_applet_toggle_locked (frame->priv->applet_info);
}

static void
panel_applet_frame_class_init (PanelAppletFrameClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
	PanelAppletContainerClass *container_class = (PanelAppletContainerClass *) klass;

	gobject_class->finalize = panel_applet_frame_finalize;

	widget_class->expose_event         = panel_applet_frame_expose;
	widget_class->size_request         = panel_applet_frame_size_request;
	widget_class->size_allocate        = panel_applet_frame_size_allocate;
	widget_class->button_press_event   = panel_applet_frame_button_changed;
	widget_class->button_release_event = panel_applet_frame_button_changed;

	container_class->applet_broken = panel_applet_frame_applet_broken;
	container_class->applet_move = panel_applet_frame_applet_move;
	container_class->applet_remove = panel_applet_frame_applet_remove;
	container_class->applet_lock = panel_applet_frame_applet_lock;

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

void
panel_applet_frame_set_panel (PanelAppletFrame *frame,
			      PanelWidget      *panel)
{
	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));
	g_return_if_fail (PANEL_IS_WIDGET (panel));

	frame->priv->panel = panel;
}
