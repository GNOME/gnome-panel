/*
 * panel-applet-frame.c:
 *
 * Copyright (C) 2001 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
#include "panel-gconf.h"
#include "panel-util.h"
#include "panel.h"
#include "session.h"
#include "applet.h"
#include "panel-marshal.h"
#include "panel-background.h"

#define HANDLE_SIZE 10

#define PANEL_STOCK_DONT_DELETE "panel-dont-delete"

#undef PANEL_APPLET_FRAME_DEBUG

struct _PanelAppletFramePrivate {
	GNOME_Vertigo_PanelAppletShell  applet_shell;
	Bonobo_PropertyBag              property_bag;
	BonoboUIComponent              *ui_component;

	PanelWidget                    *panel;
	AppletInfo                     *applet_info;
	PanelOrient                     orient;

	gchar                          *iid;
	gboolean			moving_focus_out;

	GtkAllocation                   child_allocation;
	GdkRectangle                    handle_rect;
};

static GObjectClass *parent_class;

/* Keep in sync with panel-applet.h. Uggh.
 */	
typedef enum {
	APPLET_FLAGS_NONE   = 0,
	APPLET_EXPAND_MAJOR = 1 << 0,
	APPLET_EXPAND_MINOR = 1 << 1,
	APPLET_HAS_HANDLE   = 1 << 2,
} PanelAppletFlags;

void
panel_applet_frame_save_to_gconf (PanelAppletFrame *frame,
				  const char       *gconf_key)
{
	GConfClient *client;
	const char  *profile;
	const char  *temp_key;

	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	temp_key = panel_gconf_full_key (PANEL_GCONF_APPLETS, profile, gconf_key, "bonobo_iid");
	gconf_client_set_string (client, temp_key, frame->priv->iid, NULL);
}

void
panel_applet_frame_load_from_gconf (PanelWidget *panel_widget,
				    gint         position,
				    const char  *gconf_key)
{
	GConfClient *client;
	const char  *profile;
	const char  *temp_key;
	char        *applet_iid;

	g_return_if_fail (panel_widget != NULL);
	g_return_if_fail (gconf_key != NULL);

	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	temp_key = panel_gconf_full_key (PANEL_GCONF_APPLETS, profile, gconf_key, "bonobo_iid");
	applet_iid = gconf_client_get_string (client, temp_key, NULL);

	panel_applet_frame_load (applet_iid, panel_widget, position, TRUE, gconf_key);

	g_free (applet_iid);
}

static void
popup_handle_remove (BonoboUIComponent *uic,
		     PanelAppletFrame  *frame,
		     const gchar       *verbname)
{
	AppletInfo *info;

	info = frame->priv->applet_info;
	frame->priv->applet_info = NULL;

	panel_applet_clean (info, TRUE);
}

static void
popup_handle_move (BonoboUIComponent *uic,
		   PanelAppletFrame  *frame,
		   const gchar       *verbname)
{
	GtkWidget *widget;

	g_return_if_fail (GTK_IS_WIDGET (frame));
	g_return_if_fail (PANEL_IS_WIDGET (frame->priv->panel));

	widget = GTK_WIDGET (frame);

	g_return_if_fail (PANEL_IS_WIDGET (widget->parent));

	panel_widget_applet_drag_start (
		frame->priv->panel, widget, PW_DRAG_OFF_CENTER);
}

static BonoboUIVerb popup_verbs [] = {
        BONOBO_UI_UNSAFE_VERB ("RemoveAppletFromPanel", popup_handle_remove),
        BONOBO_UI_UNSAFE_VERB ("MoveApplet",            popup_handle_move),

        BONOBO_UI_VERB_END
};

void
panel_applet_frame_load (const gchar *iid,
			 PanelWidget *panel,
			 int          position,
			 gboolean     exactpos,
			 const char  *gconf_key)
{
	GtkWidget  *frame = NULL;
	AppletInfo *info;
	char       *real_key;

	g_return_if_fail (iid != NULL);
	g_return_if_fail (panel != NULL);

	if (gconf_key)
		real_key = g_strdup (gconf_key);
	else
		real_key = gconf_unique_key ();

	frame = panel_applet_frame_new (panel, iid, real_key);

	if (!frame) {
		g_free (real_key);
		return;
	}
	
	gtk_widget_show_all (frame);

	info = panel_applet_register (frame, frame, NULL, panel, position,
				      exactpos, APPLET_BONOBO, real_key);

	if (!info)
		g_warning (_("Cannot register control widget\n"));

	g_free (real_key);

	panel_applet_frame_set_info (PANEL_APPLET_FRAME (frame), info);
}

static int
panel_applet_frame_get_flags (PanelAppletFrame *frame)
{
	return bonobo_pbclient_get_short (
			frame->priv->property_bag, "panel-applet-flags", NULL);
}

void
panel_applet_frame_get_expand_flags (PanelAppletFrame *frame,
				     gboolean         *expand_major,
				     gboolean         *expand_minor)
{
	int flags;

	flags = panel_applet_frame_get_flags (frame);

	*expand_major = flags & APPLET_EXPAND_MAJOR;
	*expand_minor = flags & APPLET_EXPAND_MINOR;
}

int 
panel_applet_frame_get_size_hints (PanelAppletFrame  *frame,
				   int              **size_hints)
{
	CORBA_sequence_CORBA_long *seq;
	CORBA_any                 *value;
	int                        retval = 0;
	int                        extra_size = 0;
	int                        i;


	g_return_val_if_fail (PANEL_IS_APPLET_FRAME (frame), 0);
	g_return_val_if_fail (size_hints != NULL, 0);

	*size_hints = NULL;

	value = bonobo_pbclient_get_value (frame->priv->property_bag,
					   "panel-applet-size-hints",
					   TC_CORBA_sequence_CORBA_long,
					   NULL);
	
	if (value == NULL)
		return retval;

	seq = value->_value;

	retval = seq->_length;
	*size_hints = g_new (int, seq->_length);

	extra_size = 0;
	if (panel_applet_frame_get_flags (frame) & APPLET_HAS_HANDLE)
		extra_size = HANDLE_SIZE + 1;
	
	for (i = 0; i < seq->_length; i++)
		(*size_hints) [i] = seq->_buffer [i] + extra_size;
	
	CORBA_free (value);
	
	return retval;
}

void
panel_applet_frame_change_orient (PanelAppletFrame *frame,
				  PanelOrient       orient)
{
	if (orient == frame->priv->orient)
		return;

	frame->priv->orient = orient;
	bonobo_pbclient_set_short (frame->priv->property_bag, 
				   "panel-applet-orient",
				   orient,
				   NULL);

	gtk_widget_queue_resize (GTK_WIDGET (frame));
}

void
panel_applet_frame_change_size (PanelAppletFrame *frame,
				PanelSize         size)
{
	bonobo_pbclient_set_short (frame->priv->property_bag, 
				   "panel-applet-size",
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

	bonobo_pbclient_set_string (frame->priv->property_bag,
				    "panel-applet-background",
				    bg_str, NULL);

	g_free (bg_str);
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

        g_free (frame->priv);
        frame->priv = NULL;

        parent_class->finalize (object);
}

static void
panel_applet_frame_paint (GtkWidget    *widget,
			  GdkRectangle *area)
{
	PanelAppletFrame *frame;

	frame = PANEL_APPLET_FRAME (widget);

	if (!(panel_applet_frame_get_flags (frame) & APPLET_HAS_HANDLE))
		return;
  
	if (GTK_WIDGET_DRAWABLE (widget)) {
		GtkOrientation orient = 0;

		switch (frame->priv->orient) {
		case PANEL_ORIENT_UP:
		case PANEL_ORIENT_DOWN:
			orient = GTK_ORIENTATION_HORIZONTAL;
			break;
		case PANEL_ORIENT_LEFT:
		case PANEL_ORIENT_RIGHT:
			orient = GTK_ORIENTATION_VERTICAL;
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
                        orient);
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
panel_applet_frame_constrain_size (PanelAppletFrame *frame,
				   GtkRequisition   *requisition)
{
	PanelWidget *panel;

	g_return_if_fail (PANEL_IS_WIDGET (frame->priv->panel));

	panel = frame->priv->panel;

	switch (frame->priv->orient) {
	case PANEL_ORIENT_UP:
	case PANEL_ORIENT_DOWN:
		if (requisition->height > panel->sz)
			requisition->height = panel->sz;
		break;
	case PANEL_ORIENT_LEFT:
	case PANEL_ORIENT_RIGHT:
		if (requisition->width > panel->sz)
			requisition->width = panel->sz;
		break;
	default:
		g_assert_not_reached ();
		break;
	}
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

	if (!(panel_applet_frame_get_flags (frame) & APPLET_HAS_HANDLE)) {
		GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);
		panel_applet_frame_constrain_size (frame, requisition);
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

	switch (frame->priv->orient) {
	case PANEL_ORIENT_UP:
	case PANEL_ORIENT_DOWN:
		requisition->width += HANDLE_SIZE;
		break;
	case PANEL_ORIENT_LEFT:
	case PANEL_ORIENT_RIGHT:
		requisition->height += HANDLE_SIZE;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	panel_applet_frame_constrain_size (frame, requisition);
}

static void
panel_applet_frame_size_allocate (GtkWidget     *widget,
				  GtkAllocation *allocation)
{
	PanelAppletFrame *frame;
	GtkBin           *bin;
	GtkAllocation     new_allocation;

	frame = PANEL_APPLET_FRAME (widget);
	bin = GTK_BIN (widget);

	if (!(panel_applet_frame_get_flags (frame) & APPLET_HAS_HANDLE)) {
		GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
		return;
	}

	widget->allocation = *allocation;

	frame->priv->handle_rect.x = 0;
	frame->priv->handle_rect.y = 0;

	switch (frame->priv->orient) {
	case PANEL_ORIENT_UP:
	case PANEL_ORIENT_DOWN:
		frame->priv->handle_rect.width  = HANDLE_SIZE;
		frame->priv->handle_rect.height = allocation->height;

		new_allocation.x      = HANDLE_SIZE;
		new_allocation.y      = 0;
		new_allocation.width  = allocation->width - HANDLE_SIZE;
		new_allocation.height = allocation->height;
		break;
	case PANEL_ORIENT_LEFT:
	case PANEL_ORIENT_RIGHT:
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

	if (!(panel_applet_frame_get_flags (frame) & APPLET_HAS_HANDLE))
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
					frame->priv->panel, GTK_WIDGET (frame), PW_DRAG_OFF_CURSOR);
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
		char        *gconf_key = NULL;
		int          position = -1;

		panel = frame->priv->panel;
		iid   = g_strdup (frame->priv->iid);

		if (info) {
			gconf_key = g_strdup (info->gconf_key);
			position  = panel_applet_get_position (info);
			panel_applet_clean (info, FALSE);
		}

		panel_applet_frame_load (iid, panel, position, TRUE, gconf_key);

		g_free (iid);
		g_free (gconf_key);

	} else if (info)
		panel_applet_clean (info, TRUE);

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
panel_applet_frame_cnx_broken (PanelAppletFrame *frame)
{
	GtkWidget *dialog;
	GdkScreen *screen;
	char      *applet_name = NULL;
	char      *txt;

	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));

	screen = gtk_widget_get_screen (GTK_WIDGET (frame));

	if (frame->priv->iid)
		applet_name = panel_applet_frame_get_name (frame->priv->iid);

	txt = g_strdup_printf (
			_("The %s applet appears to have died "
			  "unexpectedly\n\n"
			  "Reload this applet?\n\n"
			  "(If you choose not to reload it at this time"
			  " you can always add it by right clicking on "
			  "the panel and clicking on the \"Add to Panel\""
			  " submenu)"), applet_name ? applet_name : "");

	dialog = gtk_message_dialog_new (
				NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_YES_NO,
				txt);
	gtk_window_set_screen (GTK_WINDOW (dialog), screen);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (panel_applet_frame_reload_response),
			  g_object_ref (frame));

	gtk_widget_show (dialog);
	g_free (applet_name);
	g_free (txt);
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

static void
panel_applet_frame_loading_failed (PanelAppletFrame  *frame,
				   CORBA_Environment *ev,
				   const char        *iid,
				   const char        *gconf_key,
				   GtkWindow         *panel)
{
	GtkWidget *dialog;
	char      *error;
	int        response;

	error = bonobo_exception_get_text (ev);

	dialog = gtk_message_dialog_new (
				NULL, 0,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_NONE,
				_("The panel encountered a problem while loading \"%s\"\n"
				  "Details: %s\n\n"
				  "Do you want to delete the applet from your configuration?"),
				iid, error);

	g_free (error);

	register_stock_item ();

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				PANEL_STOCK_DONT_DELETE, GTK_RESPONSE_CANCEL,
				GTK_STOCK_DELETE, GTK_RESPONSE_OK,
				NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_window_set_screen (GTK_WINDOW (dialog),
			       gtk_window_get_screen (panel));

        response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_OK)
		panel_applet_clean_gconf (APPLET_BONOBO, gconf_key, TRUE);
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
}

static void
panel_applet_frame_instance_init (PanelAppletFrame      *frame,
				  PanelAppletFrameClass *klass)
{
	frame->priv = g_new0 (PanelAppletFramePrivate, 1);

	frame->priv->applet_shell     = CORBA_OBJECT_NIL;
	frame->priv->property_bag     = CORBA_OBJECT_NIL;
	frame->priv->ui_component     = NULL;
	frame->priv->panel            = NULL;
	frame->priv->orient           = PANEL_ORIENT_UP;
	frame->priv->applet_info      = NULL;
	frame->priv->moving_focus_out = FALSE;
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
	PanelOrient  orient;
	const char  *retval = NULL;

	orient = panel_widget_get_applet_orient (panel);

	switch (orient) {
	case PANEL_ORIENT_UP:
		retval = "up";
		break;
	case PANEL_ORIENT_DOWN:
		retval = "down";
		break;
	case PANEL_ORIENT_LEFT:
		retval = "left";
		break;
	case PANEL_ORIENT_RIGHT:
		retval = "right";
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

	switch (panel->sz) {
	case PANEL_SIZE_XX_SMALL:
		retval = "xx-small";
		break;
	case PANEL_SIZE_X_SMALL:
		retval = "x-small";
		break;
	case PANEL_SIZE_SMALL:
		retval = "small";
		break;
	case PANEL_SIZE_MEDIUM:
		retval = "medium";
		break;
	case PANEL_SIZE_LARGE:
		retval = "large";
		break;
	case PANEL_SIZE_X_LARGE:
		retval = "x-large";
		break;
	case PANEL_SIZE_XX_LARGE:
		retval = "xx-large";
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return retval;
}

static char *
panel_applet_frame_construct_moniker (PanelAppletFrame *frame,
				      PanelWidget      *panel,
				      const char       *iid,
				      const char       *gconf_key)
{
	char *retval;
	char *bg_str;

	bg_str = panel_applet_frame_get_background_string (
				frame, panel, panel->background.type);

	retval = g_strdup_printf (
			"%s!prefs_key=/apps/panel/profiles/%s/applets/%s/prefs;"
			"background=%s;orient=%s;size=%s",
			iid, panel_gconf_get_profile (), gconf_key, bg_str,
			panel_applet_frame_get_orient_string (frame, panel),
			panel_applet_frame_get_size_string (frame, panel));

	g_free (bg_str);

	return retval;
}

GtkWidget *
panel_applet_frame_construct (PanelAppletFrame *frame,
			      PanelWidget      *panel,
			      const char       *iid,
			      const char       *gconf_key)
{
	BonoboControlFrame    *control_frame;
	Bonobo_Control         control;
	CORBA_Environment      ev;
	ORBitConnectionStatus  cnx_status;
	GtkWidget             *widget;
	char                  *moniker;

	frame->priv->panel = panel;

	moniker = panel_applet_frame_construct_moniker (frame, panel, iid, gconf_key);

	/* FIXME: this should really use bonobo_get_object_async */
	CORBA_exception_init (&ev);

	control = bonobo_get_object (
		moniker, "IDL:Bonobo/Control:1.0", &ev);

	g_free (moniker);

	if (BONOBO_EX (&ev)) {
		panel_applet_frame_loading_failed (
			frame, &ev, iid, gconf_key, GTK_WINDOW (panel->panel_parent));
		CORBA_exception_free (&ev);
		return NULL;
	}

	frame->priv->iid = g_strdup (iid);

	cnx_status = ORBit_small_get_connection_status (control);
	if (cnx_status != ORBIT_CONNECTION_IN_PROC)
		g_signal_connect_object (
			ORBit_small_get_connection (control),
			"broken",
			G_CALLBACK (panel_applet_frame_cnx_broken),
			frame,
			G_CONNECT_SWAPPED);
	
	CORBA_exception_free (&ev);

	widget = bonobo_widget_new_control_from_objref (control, CORBA_OBJECT_NIL);

	bonobo_object_release_unref (control, NULL);

	if (!widget) {
		g_warning (G_STRLOC ": failed to load applet %s", iid);
		return NULL;
	}

	control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (widget));

	frame->priv->property_bag = 
		bonobo_control_frame_get_control_property_bag (control_frame, NULL);

	frame->priv->ui_component =
		bonobo_control_frame_get_popup_component (control_frame, NULL);
	if (!frame->priv->ui_component)
		return NULL;

	bonobo_ui_util_set_ui (frame->priv->ui_component, DATADIR,
			       "GNOME_Panel_Popup.xml", "panel", NULL);

	bonobo_ui_component_add_verb_list_with_data (
		frame->priv->ui_component, popup_verbs, frame);

	control = bonobo_control_frame_get_control (control_frame);
	if (!control)
		return NULL;

	frame->priv->applet_shell = panel_applet_frame_get_applet_shell (control);

	gtk_container_add (GTK_CONTAINER (frame), widget);

	return widget;
}

GtkWidget *
panel_applet_frame_new (PanelWidget *panel,
			const char  *iid,
			const char  *gconf_key)
{
	PanelAppletFrame *frame;

	g_return_val_if_fail (iid != NULL && gconf_key != NULL, NULL);

	frame = g_object_new (PANEL_TYPE_APPLET_FRAME, NULL);

	if (!panel_applet_frame_construct (frame, panel, iid, gconf_key)) {
		gtk_object_sink (GTK_OBJECT (frame));
		return NULL;
	}

	return GTK_WIDGET (frame);
}
