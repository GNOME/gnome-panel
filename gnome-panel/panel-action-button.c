/*
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

#include <libgnome/gnome-i18n.h>

#include "applet.h"
#include "menu.h"
#include "panel-config-global.h"
#include "panel-gconf.h"
#include "panel-profile.h"
#include "panel-stock-icons.h"
#include "panel-typebuiltins.h"
#include "panel-force-quit.h"
#include "panel-util.h"
#include "panel-session.h"
#include "panel-globals.h"
#include "panel-run-dialog.h"
#include "panel-a11y.h"
#include "panel-lockdown.h"

#define PANEL_ACTION_BUTTON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_ACTION_BUTTON, PanelActionButtonPrivate))

enum {
	PROP_0,
	PROP_ACTION_TYPE,
	PROP_DND_ENABLED
};

struct _PanelActionButtonPrivate {
	PanelActionButtonType  type;
	AppletInfo            *info;

	guint                  gconf_notify;
	
	guint                  dnd_enabled : 1;
};

static GObjectClass *parent_class;

static GConfEnumStringPair panel_action_type_map [] = {
	{ PANEL_ACTION_NONE,         "none"         },
	{ PANEL_ACTION_LOCK,         "lock"         },
	{ PANEL_ACTION_LOGOUT,       "logout"       },
	{ PANEL_ACTION_RUN,          "run"          },
	{ PANEL_ACTION_SEARCH,       "search"       },
	{ PANEL_ACTION_SCREENSHOT,   "screenshot"   },
	{ PANEL_ACTION_FORCE_QUIT,   "force-quit"   },
};

/* Lock Screen
 */
void
panel_action_lock_screen (GtkWidget *widget)
{
	panel_lock_screen (gtk_widget_get_screen (widget));
}

static gboolean
screensaver_properties_enabled (void)
{
	if (panel_lockdown_get_locked_down () ||
	    panel_lockdown_get_disable_lock_screen ())
		return FALSE;

	return TRUE;
}

static gboolean
screensaver_enabled (void)
{
  return !panel_lockdown_get_disable_lock_screen ();
}

static void
panel_action_lock_setup_menu (PanelActionButton *button)
{
	panel_applet_add_callback (button->priv->info,
				   "activate",
				   NULL,
				   _("_Activate Screensaver"),
				   screensaver_enabled);

	panel_applet_add_callback (button->priv->info,
				   "lock",
				   NULL,
				   _("_Lock Screen"),
				   screensaver_enabled);

	panel_applet_add_callback (button->priv->info,
				   "exit",
				   NULL,
				   _("_Kill Screensaver Daemon"),
				   screensaver_properties_enabled);

	panel_applet_add_callback (button->priv->info,
				   "restart",
				   NULL,
				   _("Restart _Screensaver Daemon"),
				   screensaver_properties_enabled);

	panel_applet_add_callback (button->priv->info,
				   "prefs",
				   NULL,
				   _("_Properties"),
				   screensaver_properties_enabled);
}

static void
panel_action_lock_invoke_menu (PanelActionButton *button,
			       const char *callback_name)
{
	char *command = NULL;

	if (!strcmp (callback_name, "prefs"))
		command = g_strdup ("xscreensaver-demo");

	else if (!strcmp (callback_name, "activate") ||
		 !strcmp (callback_name, "lock") ||
		 !strcmp (callback_name, "exit") ||
		 !strcmp (callback_name, "restart"))
		command = g_strdup_printf ("xscreensaver-command -%s", callback_name);

	if (command)
		gdk_spawn_command_line_on_screen (
			gtk_widget_get_screen (GTK_WIDGET (button)),
			command,
			NULL);

	g_free (command);
}

/* Log Out
 */
void
panel_action_logout (GtkWidget *widget)
{
	static int recursion_guard = 0;

	if (recursion_guard)
		return;

	recursion_guard++;

	panel_session_request_logout ();

	recursion_guard--;
}

/* Run Application
 */
void
panel_action_run_program (GtkWidget *widget)
{
	panel_run_dialog_present (gtk_widget_get_screen (widget), gtk_get_current_event_time ());
}

/* Search For Files
 */
void
panel_action_search (GtkWidget *widget)
{
	GdkScreen *screen;
	GError    *error = NULL;
	char      *argv[2] = {"gnome-search-tool", NULL};

	screen = gtk_widget_get_screen (widget);

	if (!gdk_spawn_on_screen (screen, NULL, argv, NULL,
				  G_SPAWN_SEARCH_PATH,
				  NULL, NULL, NULL, &error)) {
		panel_error_dialog (screen,
				    "cannot_exec_gnome-search-tool",
				    _("Cannot execute '%s'"),
				    "%s",
				    "gnome-search-tool",
				    error->message);
		g_error_free (error);
	}
}

/* Take Screenshot
 */
void
panel_action_screenshot (GtkWidget *widget)
{
	GdkScreen *screen;
	GError    *error = NULL;
	char      *argv [2] = {"gnome-panel-screenshot", NULL};

	screen = gtk_widget_get_screen (widget);

	if (!gdk_spawn_on_screen (screen, NULL, argv, NULL,
				  G_SPAWN_SEARCH_PATH,
				  NULL, NULL, NULL, &error)) {
		panel_error_dialog (screen,
				    "cannot_exec_gnome-panel-screenshot",
				    _("Cannot execute '%s'"),
				    "%s",
				    "gnome-panel-screenshot",
				    error->message);
		g_error_free (error);
	}
}

/* Force Quit
 */
static void
panel_action_force_quit (GtkWidget *widget)
{
	panel_force_quit (gtk_widget_get_screen (widget));
}

typedef struct {
	PanelActionButtonType   type;
	char                   *stock_icon;
	char                   *text;
	char                   *tooltip;
	char                   *help_index;
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
		PANEL_STOCK_LOCKSCREEN,
		N_("Lock screen"),
		N_("Protect your computer from unauthorized use"),
		"gospanel-21",
		"ACTION:lock:NEW",
		panel_action_lock_screen,
		panel_action_lock_setup_menu,
		panel_action_lock_invoke_menu,
		panel_lockdown_get_disable_lock_screen
	},
	{
		PANEL_ACTION_LOGOUT,
		PANEL_STOCK_LOGOUT,
		N_("Log Out"),
		N_("Log out of this session to log in as a different user or to shut down the computer"),
		"gospanel-20",
		"ACTION:logout:NEW",
		panel_action_logout, NULL, NULL,
		panel_lockdown_get_disable_log_out
	},
	{
		PANEL_ACTION_RUN,
		PANEL_STOCK_RUN,
		N_("Run Application..."),
		N_("Run an Application by entering a command"),
		"gospanel-555",
		"ACTION:run:NEW",
		panel_action_run_program, NULL, NULL,
		panel_lockdown_get_disable_command_line
	},
	{
		PANEL_ACTION_SEARCH,
		PANEL_STOCK_SEARCHTOOL,
		N_("Search for Files..."),
		N_("Find files, folders, and documents on your computer"),
		"gospanel-554",
		"ACTION:search:NEW",
		panel_action_search, NULL, NULL, NULL
	},
	{
		PANEL_ACTION_SCREENSHOT,
		PANEL_STOCK_SCREENSHOT,
		N_("Take Screenshot..."),
		N_("Take a screenshot of your desktop"),
		"gospanel-553",
		"ACTION:screenshot:NEW",
		panel_action_screenshot, NULL, NULL, NULL
	},
	{
		PANEL_ACTION_FORCE_QUIT,
		PANEL_STOCK_FORCE_QUIT,
		N_("Force Quit"),
		N_("Force a misbehaving application to quit"),
		"gospanel-563",
		"ACTION:force-quit:NEW",
		panel_action_force_quit, NULL, NULL,
		panel_lockdown_get_disable_force_quit
	}
};

gboolean
panel_action_get_is_disabled (PanelActionButtonType type)
{
	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, FALSE);

	if (actions [type].is_disabled)
		return actions [type].is_disabled ();

	return FALSE;
}

G_CONST_RETURN char*
panel_action_get_stock_icon (PanelActionButtonType type)
{
	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, NULL);

	return actions[type].stock_icon;
}

G_CONST_RETURN char*
panel_action_get_text (PanelActionButtonType type)
{
	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, NULL);

	return _(actions[type].text);
}

G_CONST_RETURN char*
panel_action_get_tooltip (PanelActionButtonType type)
{
	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, NULL);

	return _(actions[type].tooltip);
}

G_CONST_RETURN char*
panel_action_get_drag_id (PanelActionButtonType type)
{
	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, NULL);

	return actions[type].drag_id;
}

static void
panel_action_button_update_sensitivity (PanelActionButton *button)
{
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

	panel_lockdown_notify_remove (G_CALLBACK (panel_action_button_update_sensitivity),
				      button);

	gconf_client_notify_remove (panel_gconf_get_client (),
				    button->priv->gconf_notify);
	button->priv->gconf_notify = 0;

	parent_class->finalize (object);
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
		selection_data, selection_data->target,
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

	if (panel_global_config_get_drawer_auto_close ()) {
		PanelToplevel *toplevel;

		toplevel = PANEL_WIDGET (GTK_WIDGET (button)->parent)->toplevel;

		if (panel_toplevel_get_is_attached (toplevel))
			panel_toplevel_hide (toplevel, FALSE, -1);
	}

	if (actions [button->priv->type].invoke)
		actions [button->priv->type].invoke (GTK_WIDGET (button));
}

static void
panel_action_button_class_init (PanelActionButtonClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkWidgetClass *widget_class  = (GtkWidgetClass *) klass;
	GtkButtonClass *button_class  = (GtkButtonClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

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
					   G_PARAM_READWRITE));

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
panel_action_button_instance_init (PanelActionButton      *button,
				   PanelActionButtonClass *klass)
{
	button->priv = PANEL_ACTION_BUTTON_GET_PRIVATE (button);

	button->priv->type = PANEL_ACTION_NONE;
	button->priv->info = NULL;

	button->priv->gconf_notify = 0;
	button->priv->dnd_enabled  = FALSE;
}

GType
panel_action_button_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelActionButtonClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_action_button_class_init,
			NULL,
			NULL,
			sizeof (PanelActionButton),
			0,
			(GInstanceInitFunc) panel_action_button_instance_init,
			NULL
		};

		type = g_type_register_static (
				BUTTON_TYPE_WIDGET, "PanelActionButton", &info, 0);
	}

	return type;
}

void
panel_action_button_set_type (PanelActionButton     *button,
			      PanelActionButtonType  type)
{
	g_return_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST);

	if (type == button->priv->type)
		return;

	button->priv->type = type;

	button_widget_set_stock_id (BUTTON_WIDGET (button), actions [type].stock_icon);

	gtk_tooltips_set_tip (panel_tooltips, GTK_WIDGET (button),
			      _(actions [type].tooltip), NULL);
	panel_a11y_set_atk_name_desc (GTK_WIDGET (button), _(actions [type].tooltip), NULL);

	panel_action_button_update_sensitivity (button);
}

static void
panel_action_button_type_changed (GConfClient       *client,
				  guint              cnxn_id,
				  GConfEntry        *entry,
				  PanelActionButton *button)
{
	int         type;
	const char *action_type;

	g_return_if_fail (PANEL_IS_ACTION_BUTTON (button));

	if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
		return;

	action_type = gconf_value_get_string (entry->value);

	if (!gconf_string_to_enum (panel_action_type_map, action_type, &type))
		return;

	panel_action_button_set_type (button, type);
}

static void
panel_action_button_connect_to_gconf (PanelActionButton *button)
{
	const char  *key;
	const char  *profile;

	profile = panel_profile_get_name ();

	key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, button->priv->info->id, "action_type");

	button->priv->gconf_notify =
		gconf_client_notify_add (panel_gconf_get_client (), key, 
					 (GConfClientNotifyFunc) panel_action_button_type_changed,
					 button, NULL, NULL);

	panel_lockdown_notify_add (G_CALLBACK (panel_action_button_update_sensitivity),
				   button);
}

static void
panel_action_button_style_set (PanelActionButton *button)
{
	button_widget_set_stock_id (BUTTON_WIDGET (button), actions [button->priv->type].stock_icon);
}

static void
panel_action_button_load (PanelActionButtonType  type,
			  PanelWidget           *panel,
			  gboolean               locked,
			  int                    position,
			  gboolean               exactpos,
			  const char            *id,
			  gboolean               compatibility)
{
	PanelActionButton *button;
	PanelObjectType    object_type;

	g_return_if_fail (panel != NULL);

	button = g_object_new (PANEL_TYPE_ACTION_BUTTON, "action-type", type, NULL);

	object_type = PANEL_OBJECT_ACTION;
	if (compatibility) { /* Backward compatibility with GNOME 2.0.x */
		if (type == PANEL_ACTION_LOCK)
			object_type = PANEL_OBJECT_LOCK;
		else if (type == PANEL_ACTION_LOGOUT)
			object_type = PANEL_OBJECT_LOGOUT;
	}

	button->priv->info = panel_applet_register (GTK_WIDGET (button),
						    NULL, NULL,
						    panel, locked, position,
						    exactpos, object_type, id);
	if (!button->priv->info) {
		gtk_widget_destroy (GTK_WIDGET (button));
		return;
	}

	panel_applet_add_callback (button->priv->info,
				   "help",
				   GTK_STOCK_HELP,
				   _("_Help"),
				   NULL);

	panel_widget_set_applet_expandable (panel, GTK_WIDGET (button), FALSE, TRUE);
	panel_widget_set_applet_size_constrained (panel, GTK_WIDGET (button), TRUE);

	if (actions [button->priv->type].setup_menu)
		actions [button->priv->type].setup_menu (button);

	panel_action_button_connect_to_gconf (button);

	g_signal_connect (button, "style-set",
			  G_CALLBACK (panel_action_button_style_set), NULL);
}

void
panel_action_button_create (PanelToplevel         *toplevel,
			    int                    position,
			    PanelActionButtonType  type)
{
	GConfClient *client;
	const char  *profile;
	const char  *key;
	char        *id;

	client  = panel_gconf_get_client ();
	profile = panel_profile_get_name ();

	id = panel_profile_prepare_object (PANEL_OBJECT_ACTION, toplevel, position, FALSE);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "action_type");
	gconf_client_set_string (client,
				 key,
				 gconf_enum_to_string (panel_action_type_map, type),
				 NULL);

	panel_profile_add_to_list (PANEL_GCONF_OBJECTS, id);

	g_free (id);
}

/* This is only for backwards compatibility with 2.0.x
 * We load an old-style lock/logout button as an action
 * button but make sure to retain the lock/logout configuration
 * so logging back into 2.0.x still works.
 */
void
panel_action_button_load_compatible (PanelObjectType  object_type,
				     PanelWidget     *panel,
				     gboolean         locked,
				     int              position,
				     gboolean         exactpos,
				     const char      *id)
{
	PanelActionButtonType action_type;

	g_assert (object_type == PANEL_OBJECT_LOGOUT || object_type == PANEL_OBJECT_LOCK);

	action_type = object_type == PANEL_OBJECT_LOGOUT ? PANEL_ACTION_LOGOUT : PANEL_ACTION_LOCK;

	panel_action_button_load (action_type, panel, locked, position, exactpos, id, TRUE);
}

void
panel_action_button_load_from_gconf (PanelWidget *panel,
				     gboolean     locked,
				     int          position,
				     gboolean     exactpos,
				     const char  *id)
{
	int          type;
	const char  *key;
	const char  *profile;
	char        *action_type;

	profile = panel_profile_get_name ();

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "action_type");
	action_type = gconf_client_get_string (panel_gconf_get_client (), key, NULL);

	if (!gconf_string_to_enum (panel_action_type_map, action_type, &type)) {
		g_warning ("Unkown action type '%s' from %s", action_type, key);
		g_free (action_type);
		return;
	}

	g_free (action_type);

	panel_action_button_load (type, panel, locked, position, exactpos, id, FALSE);
}

void
panel_action_button_invoke_menu (PanelActionButton *button,
				 const char        *callback_name)
{
	g_return_if_fail (PANEL_IS_ACTION_BUTTON (button));
	g_return_if_fail (callback_name != NULL);
	g_return_if_fail (button->priv->type > PANEL_ACTION_NONE &&
			  button->priv->type < PANEL_ACTION_LAST);

	if (!strcmp (callback_name, "help")) {
		GdkScreen *screen;

		if (!actions [button->priv->type].help_index)
			return;

		screen = gtk_widget_get_screen (GTK_WIDGET (button));

		panel_show_help (screen, "user-guide.xml",
				 actions [button->priv->type].help_index);

		return;
	}

	if (actions [button->priv->type].invoke_menu)
		actions [button->priv->type].invoke_menu (button, callback_name);
}

gboolean
panel_action_button_load_from_drag (PanelToplevel *toplevel,
				    int            position,
				    const char    *drag_string,
				    int           *old_applet_idx)
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

	if (strcmp (elements [2], "NEW")) {
		*old_applet_idx = strtol (elements [2], NULL, 10);
		retval = TRUE; /* Remove the old applet */
	}

	g_strfreev (elements);

	panel_action_button_create (toplevel, position, type);

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

		GTK_WIDGET_UNSET_FLAGS (button, GTK_NO_WINDOW);
		gtk_drag_source_set (GTK_WIDGET (button), GDK_BUTTON1_MASK,
				     dnd_targets, 1,
				     GDK_ACTION_COPY | GDK_ACTION_MOVE);
		gtk_drag_source_set_icon_stock (GTK_WIDGET (button),
						actions [button->priv->type].stock_icon);
		GTK_WIDGET_SET_FLAGS (button, GTK_NO_WINDOW);
	} else
		gtk_drag_source_unset (GTK_WIDGET (button));

	button->priv->dnd_enabled = enabled;

	g_object_notify (G_OBJECT (button), "dnd-enabled");
}
