/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * panel-action-button.c: panel "Action Button" module
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 * Copyright (C) 2004 Red Hat, Inc.
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
#include <stdlib.h>

#include "panel-action-button.h"

#include <glib/gi18n.h>

#include <libpanel-util/panel-error.h>
#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-launch.h>
#include <libpanel-util/panel-screensaver.h>
#include <libpanel-util/panel-session-manager.h>
#include <libpanel-util/panel-show.h>

#include "applet.h"
#include "panel-gconf.h"
#include "panel-typebuiltins.h"
#include "panel-force-quit.h"
#include "panel-util.h"
#include "panel-session.h"
#include "panel-globals.h"
#include "panel-run-dialog.h"
#include "panel-a11y.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-icon-names.h"

G_DEFINE_TYPE (PanelActionButton, panel_action_button, BUTTON_TYPE_WIDGET)

#define PANEL_ACTION_BUTTON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_ACTION_BUTTON, PanelActionButtonPrivate))

enum {
	PROP_0,
	PROP_ACTION_TYPE,
	PROP_DND_ENABLED
};

struct _PanelActionButtonPrivate {
	PanelActionButtonType  type;
	AppletInfo            *info;

	guint                  dnd_enabled : 1;
};

static GConfEnumStringPair panel_action_type_map [] = {
	{ PANEL_ACTION_NONE,           "none"           },
	{ PANEL_ACTION_LOCK,           "lock"           },
	{ PANEL_ACTION_LOGOUT,         "logout"         },
	{ PANEL_ACTION_RUN,            "run"            },
	{ PANEL_ACTION_SEARCH,         "search"         },
	{ PANEL_ACTION_FORCE_QUIT,     "force-quit"     },
	{ PANEL_ACTION_CONNECT_SERVER, "connect-server" },
	{ PANEL_ACTION_SHUTDOWN,       "shutdown"       },
	{ 0,                           NULL             },
};

/* Lock Screen
 */
static void
panel_action_lock_screen (GtkWidget *widget)
{
	panel_screensaver_lock (panel_screensaver_get ());
}

static gboolean
screensaver_properties_enabled (void)
{
	char *desktop;
	gboolean found;

	if (panel_lockdown_get_panels_locked_down_s () ||
	    panel_lockdown_get_disable_lock_screen_s ())
		return FALSE;

	desktop = panel_g_lookup_in_applications_dirs ("gnome-screen-panel.desktop");
	found = (desktop != NULL);
	g_free (desktop);

	return found;
}

static gboolean
panel_action_lock_is_enabled (void)
{
	return !panel_lockdown_get_disable_lock_screen_s ();
}

static gboolean
panel_action_lock_is_disabled (void)
{
	return !panel_action_lock_is_enabled ();
}

static void
panel_action_lock_setup_menu (PanelActionButton *button)
{
	panel_applet_add_callback (button->priv->info,
				   "lock",
				   NULL,
				   _("_Lock Screen"),
				   panel_action_lock_is_enabled);

	panel_applet_add_callback (button->priv->info,
				   "activate",
				   NULL,
				   _("_Activate Screensaver"),
				   NULL);

	panel_applet_add_callback (button->priv->info,
				   "prefs",
				   GTK_STOCK_PROPERTIES,
				   _("_Properties"),
				   screensaver_properties_enabled);
}

static void
panel_action_lock_invoke_menu (PanelActionButton *button,
			       const char *callback_name)
{
	g_return_if_fail (PANEL_IS_ACTION_BUTTON (button));
	g_return_if_fail (callback_name != NULL);

	if (g_strcmp0 (callback_name, "lock") == 0)
		panel_screensaver_lock (panel_screensaver_get ());
	else if (g_strcmp0 (callback_name, "activate") == 0)
		panel_screensaver_activate (panel_screensaver_get ());
	else if (g_strcmp0 (callback_name, "prefs") == 0)
		panel_launch_desktop_file ("gnome-screen-panel.desktop",
					   gtk_widget_get_screen (GTK_WIDGET (button)),
					   NULL);
	else
		g_assert_not_reached ();
}

/* Log Out
 */
static void
panel_action_logout (GtkWidget *widget)
{
	/* FIXME: we need to use widget to get the screen for the
	 * confirmation dialog, see
	 * http://bugzilla.gnome.org/show_bug.cgi?id=536914 */
	panel_session_manager_request_logout (panel_session_manager_get (),
					      PANEL_SESSION_MANAGER_LOGOUT_MODE_NORMAL);
}

static void
panel_action_shutdown (GtkWidget *widget)
{
	PanelSessionManager *manager;

	manager = panel_session_manager_get ();
	panel_session_manager_request_shutdown (manager);
}

static gboolean
panel_action_shutdown_reboot_is_disabled (void)
{
	PanelSessionManager *manager;

	if (panel_lockdown_get_disable_log_out_s ())
		return TRUE;

	manager = panel_session_manager_get ();

	return (!panel_session_manager_is_shutdown_available (manager));
}

/* Run Application
 */
static void
panel_action_run_program (GtkWidget *widget)
{
	panel_run_dialog_present (gtk_widget_get_screen (widget),
				  gtk_get_current_event_time ());
}

/* Search For Files
 */
static void
panel_action_search (GtkWidget *widget)
{
	GdkScreen *screen;

	screen = gtk_widget_get_screen (widget);
	panel_launch_desktop_file_with_fallback ("gnome-search-tool.desktop",
						 "gnome-search-tool",
						 screen, NULL);
}

/* Force Quit
 */
static void
panel_action_force_quit (GtkWidget *widget)
{
	panel_force_quit (gtk_widget_get_screen (widget),
			  gtk_get_current_event_time ());
}

/* Connect Server
 */
static void
panel_action_connect_server (GtkWidget *widget)
{
	GdkScreen *screen;
	GAppInfo  *app_info;
	GError    *error;

	screen = gtk_widget_get_screen (GTK_WIDGET (widget));
	error = NULL;
	app_info = g_app_info_create_from_commandline ("nautilus-connect-server",
						       _("Connect to server"),
						       G_APP_INFO_CREATE_NONE,
						       &error);

	if (!error) {
		GdkAppLaunchContext *launch_context;
		GdkDisplay          *display;

		display = gdk_screen_get_display (screen);
		launch_context = gdk_display_get_app_launch_context (display);
		gdk_app_launch_context_set_screen (launch_context, screen);

		g_app_info_launch (app_info, NULL,
				   G_APP_LAUNCH_CONTEXT (launch_context),
				   &error);

		g_object_unref (launch_context);
		g_object_unref (app_info);
	}

	if (error) {
		panel_error_dialog (NULL, screen,
				    "cannot_connect_server",
				    TRUE,
				    _("Could not connect to server"),
				    error->message);
		g_clear_error (&error);
	}
}

typedef struct {
	PanelActionButtonType   type;
	char                   *icon_name;
	char                   *text;
	char                   *tooltip;
	char                   *drag_id;
	void                  (*invoke)      (GtkWidget         *widget);
	void                  (*setup_menu)  (PanelActionButton *button);
	void                  (*invoke_menu) (PanelActionButton *button,
					      const char        *callback_name);
	gboolean              (*is_disabled) (void);
} PanelAction;

/* Keep order in sync with PanelActionButtonType
 */
static PanelAction actions [] = {
	{
		PANEL_ACTION_NONE,
		NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL
	},
	{
		PANEL_ACTION_LOCK,
		PANEL_ICON_LOCKSCREEN,
		N_("Lock Screen"),
		N_("Protect your computer from unauthorized use"),
		"ACTION:lock:NEW",
		panel_action_lock_screen,
		panel_action_lock_setup_menu,
		panel_action_lock_invoke_menu,
		panel_action_lock_is_disabled
	},
	{
		PANEL_ACTION_LOGOUT,
		PANEL_ICON_LOGOUT,
		/* when changing one of those two strings, don't forget to
		 * update the ones in panel-menu-items.c (look for
		 * "1" (msgctxt: "panel:showusername")) */
		N_("Log Out..."),
		N_("Log out of this session to log in as a different user"),
		"ACTION:logout:NEW",
		panel_action_logout, NULL, NULL,
		panel_lockdown_get_disable_log_out_s
	},
	{
		PANEL_ACTION_RUN,
		PANEL_ICON_RUN,
		N_("Run Application..."),
		N_("Run an application by typing a command or choosing from a list"),
		"ACTION:run:NEW",
		panel_action_run_program, NULL, NULL,
		panel_lockdown_get_disable_command_line_s
	},
	{
		PANEL_ACTION_SEARCH,
		PANEL_ICON_SEARCHTOOL,
		N_("Search for Files..."),
		N_("Locate documents and folders on this computer by name or content"),
		"ACTION:search:NEW",
		panel_action_search, NULL, NULL, NULL
	},
	{
		PANEL_ACTION_FORCE_QUIT,
		PANEL_ICON_FORCE_QUIT,
		N_("Force Quit"),
		N_("Force a misbehaving application to quit"),
		"ACTION:force-quit:NEW",
		panel_action_force_quit, NULL, NULL,
		panel_lockdown_get_disable_force_quit_s
	},
	{
		PANEL_ACTION_CONNECT_SERVER,
		PANEL_ICON_REMOTE, //FIXME icon
		N_("Connect to Server..."),
		N_("Connect to a remote computer or shared disk"),
		"ACTION:connect-server:NEW",
		panel_action_connect_server, NULL, NULL, NULL
	},
	{
		PANEL_ACTION_SHUTDOWN,
		PANEL_ICON_SHUTDOWN,
		N_("Shut Down..."),
		N_("Shut down the computer"),
		"ACTION:shutdown:NEW",
		panel_action_shutdown, NULL, NULL,
		panel_action_shutdown_reboot_is_disabled
	}
};

static gboolean
panel_action_get_is_deprecated (PanelActionButtonType type)
{
	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, FALSE);

	return (type > PANEL_ACTION_LAST_NON_DEPRECATED);
}

gboolean
panel_action_get_is_disabled (PanelActionButtonType type)
{
	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, FALSE);

	if (panel_action_get_is_deprecated (type))
		return TRUE;

	if (actions [type].is_disabled)
		return actions [type].is_disabled ();

	return FALSE;
}

GCallback
panel_action_get_invoke (PanelActionButtonType type)
{
	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, NULL);

	g_assert (actions[type].invoke != NULL);

	return G_CALLBACK (actions[type].invoke);
}

const char*
panel_action_get_icon_name (PanelActionButtonType type)
{
	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, NULL);

	return actions[type].icon_name;
}

const char*
panel_action_get_text (PanelActionButtonType type)
{
	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, NULL);

	return _(actions[type].text);
}

const char*
panel_action_get_tooltip (PanelActionButtonType type)
{
	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, NULL);

	return _(actions[type].tooltip);
}

const char*
panel_action_get_drag_id (PanelActionButtonType type)
{
	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, NULL);

	return actions[type].drag_id;
}

static void
panel_action_button_update_sensitivity (PanelLockdown *lockdown,
					gpointer       user_data)
{
	PanelActionButton *button = user_data;

	if (actions [button->priv->type].is_disabled)
		button_widget_set_activatable (BUTTON_WIDGET (button),
					       !actions [button->priv->type].is_disabled ());
}

static void
panel_action_button_finalize (GObject *object)
{
	PanelActionButton *button = PANEL_ACTION_BUTTON (object);

	button->priv->info = NULL;
	button->priv->type = PANEL_ACTION_NONE;

	G_OBJECT_CLASS (panel_action_button_parent_class)->finalize (object);
}

static void
panel_action_button_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
	PanelActionButton *button;

	g_return_if_fail (PANEL_IS_ACTION_BUTTON (object));

	button = PANEL_ACTION_BUTTON (object);

	switch (prop_id) {
	case PROP_ACTION_TYPE:
		g_value_set_enum (value, button->priv->type);
		break;
	case PROP_DND_ENABLED:
		g_value_set_boolean (value, button->priv->dnd_enabled);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_action_button_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
	PanelActionButton *button;

	g_return_if_fail (PANEL_IS_ACTION_BUTTON (object));

	button = PANEL_ACTION_BUTTON (object);

	switch (prop_id) {
	case PROP_ACTION_TYPE:
		panel_action_button_set_type (button,
					      g_value_get_enum (value));
		break;
	case PROP_DND_ENABLED:
		panel_action_button_set_dnd_enabled (button,
						     g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_action_button_drag_data_get (GtkWidget          *widget,
				   GdkDragContext     *context,
				   GtkSelectionData   *selection_data,
				   guint               info,
				   guint               time)
{
	PanelActionButton *button;
	char              *drag_data;

	g_return_if_fail (PANEL_IS_ACTION_BUTTON (widget));

	button = PANEL_ACTION_BUTTON (widget);

	drag_data = g_strdup_printf ("ACTION:%s:%d", 
				     gconf_enum_to_string (panel_action_type_map, button->priv->type),
				     panel_find_applet_index (widget));

	gtk_selection_data_set (
		selection_data, gtk_selection_data_get_target (selection_data),
		8, (guchar *) drag_data, strlen (drag_data));

	g_free (drag_data);
}

static void
panel_action_button_clicked (GtkButton *gtk_button)
{
	PanelActionButton *button;

	g_return_if_fail (PANEL_IS_ACTION_BUTTON (gtk_button));

	button = PANEL_ACTION_BUTTON (gtk_button);

	g_return_if_fail (button->priv->type > PANEL_ACTION_NONE);
	g_return_if_fail (button->priv->type < PANEL_ACTION_LAST);

	if (actions [button->priv->type].invoke)
		actions [button->priv->type].invoke (GTK_WIDGET (button));
}

static void
panel_action_button_class_init (PanelActionButtonClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkWidgetClass *widget_class  = (GtkWidgetClass *) klass;
	GtkButtonClass *button_class  = (GtkButtonClass *) klass;

	gobject_class->finalize     = panel_action_button_finalize;
	gobject_class->get_property = panel_action_button_get_property;
	gobject_class->set_property = panel_action_button_set_property;

	widget_class->drag_data_get = panel_action_button_drag_data_get;

	button_class->clicked       = panel_action_button_clicked;

	g_type_class_add_private (klass, sizeof (PanelActionButtonPrivate));

	g_object_class_install_property (
			gobject_class,
			PROP_ACTION_TYPE,
			g_param_spec_enum ("action-type",
					   "Action Type",
					   "The type of action this button implements",
					   PANEL_TYPE_ACTION_BUTTON_TYPE,
					   PANEL_ORIENTATION_TOP,
					   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
			gobject_class,
			PROP_DND_ENABLED,
			g_param_spec_boolean ("dnd-enabled",
					      "Drag and drop enabled",
					      "Whether or not drag and drop is enabled on the widget",
					      TRUE,
					      G_PARAM_READWRITE));
}

static void
panel_action_button_init (PanelActionButton *button)
{
	button->priv = PANEL_ACTION_BUTTON_GET_PRIVATE (button);

	button->priv->type = PANEL_ACTION_NONE;
	button->priv->info = NULL;

	button->priv->dnd_enabled  = FALSE;
}

void
panel_action_button_set_type (PanelActionButton     *button,
			      PanelActionButtonType  type)
{
	g_return_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST);

	if (panel_action_get_is_deprecated (type))
		return;

	if (type == button->priv->type)
		return;

	button->priv->type = type;

	if (actions [type].icon_name != NULL)
		button_widget_set_icon_name (BUTTON_WIDGET (button), actions [type].icon_name);

	panel_util_set_tooltip_text (GTK_WIDGET (button),
				     _(actions [type].tooltip));
	panel_a11y_set_atk_name_desc (GTK_WIDGET (button), _(actions [type].tooltip), NULL);

	panel_action_button_update_sensitivity (panel_lockdown_get (), button);
}

static void
panel_action_button_style_updated (PanelActionButton *button)
{
	if (actions [button->priv->type].icon_name != NULL)
		button_widget_set_icon_name (BUTTON_WIDGET (button), actions [button->priv->type].icon_name);
}

static void
panel_action_button_load_helper (PanelWidget           *panel,
				 const char            *id,
				 GSettings             *settings,
				 PanelActionButtonType  type)
{
	PanelActionButton *button;

	g_return_if_fail (panel != NULL);

	button = g_object_new (PANEL_TYPE_ACTION_BUTTON, "action-type", type, NULL);

	button->priv->info = panel_applet_register (GTK_WIDGET (button), panel,
						    PANEL_OBJECT_ACTION, id,
						    settings,
						    NULL, NULL);
	if (!button->priv->info) {
		gtk_widget_destroy (GTK_WIDGET (button));
		return;
	}

	panel_widget_set_applet_expandable (panel, GTK_WIDGET (button), FALSE, TRUE);
	panel_widget_set_applet_size_constrained (panel, GTK_WIDGET (button), TRUE);

	if (actions [button->priv->type].setup_menu)
		actions [button->priv->type].setup_menu (button);

	panel_lockdown_on_notify (panel_lockdown_get (),
				  NULL,
				  G_OBJECT (button),
				  panel_action_button_update_sensitivity,
				  button);

	g_signal_connect (button, "style-updated",
			  G_CALLBACK (panel_action_button_style_updated), NULL);
}

void
panel_action_button_create (PanelToplevel         *toplevel,
			    PanelObjectPackType    pack_type,
			    int                    pack_index,
			    PanelActionButtonType  type)
{
	const char *detail;

	detail = gconf_enum_to_string (panel_action_type_map, type);

	panel_layout_object_create (PANEL_OBJECT_ACTION,
				    detail,
				    panel_toplevel_get_id (toplevel),
				    pack_type, pack_index);
}

void
panel_action_button_load (PanelWidget *panel,
			  const char  *id,
			  GSettings *settings,
			  const char  *detail_for_type)
{
	int type = PANEL_ACTION_NONE;
	int i;

	for (i = 0; panel_action_type_map[i].str != NULL; i++) {
		if (g_strcmp0 (detail_for_type,
			       panel_action_type_map[i].str) == 0) {
			type = panel_action_type_map[i].enum_value;
			break;
		}
	}

	if (type == PANEL_ACTION_NONE) {
		g_warning ("Unkown action type '%s", detail_for_type);
		return;
	}

	panel_action_button_load_helper (panel, id, settings, type);
}

void
panel_action_button_invoke_menu (PanelActionButton *button,
				 const char        *callback_name)
{
	g_return_if_fail (PANEL_IS_ACTION_BUTTON (button));
	g_return_if_fail (callback_name != NULL);
	g_return_if_fail (button->priv->type > PANEL_ACTION_NONE &&
			  button->priv->type < PANEL_ACTION_LAST);

	if (actions [button->priv->type].invoke_menu)
		actions [button->priv->type].invoke_menu (button, callback_name);
}

gboolean
panel_action_button_load_from_drag (PanelToplevel       *toplevel,
				    PanelObjectPackType  pack_type,
				    int                  pack_index,
				    const char          *drag_string,
				    int                 *old_applet_idx)
{
	PanelActionButtonType   type = PANEL_ACTION_NONE;
	gboolean                retval = FALSE;
	char                  **elements;

	if (strncmp (drag_string, "ACTION:", strlen ("ACTION:")))
		return retval;

	elements = g_strsplit (drag_string, ":", 0);

	g_assert (elements != NULL);

	if (!elements [1] || !elements [2]) {
		g_strfreev (elements);
		return retval;
	}

	if (!gconf_string_to_enum (panel_action_type_map, elements [1], (gpointer) &type)) {
		g_strfreev (elements);
		return retval;
	}

	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, FALSE);

	if (panel_action_get_is_deprecated (type))
		return retval;

	if (strcmp (elements [2], "NEW")) {
		*old_applet_idx = strtol (elements [2], NULL, 10);
		retval = TRUE; /* Remove the old applet */
	}

	g_strfreev (elements);

	panel_action_button_create (toplevel, pack_type, pack_index, type);

	return retval;
}

void
panel_action_button_set_dnd_enabled (PanelActionButton *button,
				     gboolean           enabled)
{
	g_return_if_fail (PANEL_IS_ACTION_BUTTON (button));

	if (!button->priv->type)
		return; /* wait until we know what type it is */

	enabled = enabled != FALSE;

	if (button->priv->dnd_enabled == enabled)
		return;

	if (enabled) {
		static GtkTargetEntry dnd_targets [] = {
			{ "application/x-panel-applet-internal", 0, 0 }
		};

		gtk_widget_set_has_window (GTK_WIDGET (button), TRUE);
		gtk_drag_source_set (GTK_WIDGET (button), GDK_BUTTON1_MASK,
				     dnd_targets, 1,
				     GDK_ACTION_COPY | GDK_ACTION_MOVE);
		if (actions [button->priv->type].icon_name != NULL)
			gtk_drag_source_set_icon_name (GTK_WIDGET (button),
						       actions [button->priv->type].icon_name);
		gtk_widget_set_has_window (GTK_WIDGET (button), FALSE);
	} else
		gtk_drag_source_unset (GTK_WIDGET (button));

	button->priv->dnd_enabled = enabled;

	g_object_notify (G_OBJECT (button), "dnd-enabled");
}
