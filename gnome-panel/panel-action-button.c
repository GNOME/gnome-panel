/*
 * panel-action-button.c:
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
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
#include <stdlib.h>

#include "panel-action-button.h"

#include <libgnome/gnome-i18n.h>

#include "applet.h"
#include "drawer-widget.h"
#include "egg-screen-exec.h"
#include "gnome-run.h"
#include "menu.h"
#include "panel-config-global.h"
#include "panel-gconf.h"
#include "panel-stock-icons.h"
#include "panel-typebuiltins.h"
#include "panel-util.h"
#include "session.h"

extern GtkTooltips  *panel_tooltips;
extern GlobalConfig  global_config;
extern gboolean      commie_mode;

enum {
	PROP_0,
	PROP_ACTION_TYPE,
};

struct _PanelActionButtonPrivate {
	PanelActionButtonType  type;
	AppletInfo            *info;

	guint                  gconf_notify;
};

static GObjectClass *parent_class;

static GConfEnumStringPair panel_action_type_map [] = {
	{ PANEL_ACTION_NONE,         "none"         },
	{ PANEL_ACTION_LOCK,         "lock"         },
	{ PANEL_ACTION_LOGOUT,       "logout"       },
	{ PANEL_ACTION_RUN,          "run"          },
	{ PANEL_ACTION_SEARCH,       "search"       },
	{ PANEL_ACTION_SCREENSHOT,   "screenshot"   },
};

/* Lock Screen
 */
void
panel_action_lock_screen (GtkWidget *widget)
{
	panel_lock_screen (gtk_widget_get_screen (widget));
}

static void
panel_action_lock_setup_menu (PanelActionButton *button)
{
	panel_applet_add_callback (button->priv->info, "activate", NULL, _("_Activate Screensaver"));
	panel_applet_add_callback (button->priv->info, "lock",     NULL, _("_Lock Screen"));
	panel_applet_add_callback (button->priv->info, "exit",     NULL, _("_Kill Screensaver Daemon"));
	panel_applet_add_callback (button->priv->info, "restart",  NULL, _("Restart _Screensaver Daemon"));

	if (!commie_mode)
		panel_applet_add_callback (button->priv->info, "prefs", NULL, _("_Properties"));
}

static void
panel_action_lock_invoke_menu (PanelActionButton *button,
			       const char *callback_name)
{
	char *command = NULL;

	if (!strcmp (callback_name, "restart"))
		command = g_strdup ("xscreensaver-command -exit ; xscreensaver &");

	else if (!strcmp (callback_name, "prefs"))
		command = g_strdup ("xscreensaver-demo");

	else if (!strcmp (callback_name, "activate") ||
		 !strcmp (callback_name, "lock") ||
		 !strcmp (callback_name, "exit"))
		command = g_strdup_printf ("xscreensaver-command -%s", callback_name);

	if (command)
		egg_screen_execute_shell (
			gtk_widget_get_screen (GTK_WIDGET (button)),
					       g_get_home_dir (), command);

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

	panel_quit ();

	recursion_guard--;
}

/* Run Program
 */
void
panel_action_run_program (GtkWidget *widget)
{
	show_run_dialog (gtk_widget_get_screen (widget));
}

/* Search For Files
 */
void
panel_action_search (GtkWidget *widget)
{
	GdkScreen *screen;
	char      *argv[2] = {"gnome-search-tool", NULL};

	screen = gtk_widget_get_screen (widget);

	if (egg_screen_execute_async (screen, g_get_home_dir (), 1, argv) < 0)
		panel_error_dialog (screen,
				    "cannot_exec_gnome-search-tool",
				    _("Cannot execute gnome-search-tool"));
}

/* Take Screenshot
 */
void
panel_action_screenshot (GtkWidget *widget)
{
	GdkScreen *screen;
	char      *argv [2] = {"gnome-panel-screenshot", NULL};

	screen = gtk_widget_get_screen (widget);

	if (egg_screen_execute_async (screen, g_get_home_dir (), 1, argv) < 0)
		panel_error_dialog (screen,
				    "cannot_exec_gnome-panel-screenshot",
				    _("Cannot execute gnome-panel-screenshot"));
}

typedef struct {
	PanelActionButtonType   type;
	char                   *stock_icon;
	char                   *tooltip;
	char                   *help_index;
	void                  (*invoke)      (GtkWidget         *widget);
	void                  (*setup_menu)  (PanelActionButton *button);
	void                  (*invoke_menu) (PanelActionButton *button,
					      const char        *callback_name);
} PanelAction;

/* Keep order in sync with PanelActionButtonType
 */
static PanelAction actions [] = {
	{ PANEL_ACTION_NONE, NULL, NULL },
	{
		PANEL_ACTION_LOCK, PANEL_STOCK_LOCKSCREEN,
		N_("Lock screen"), "gospanel-21",
		panel_action_lock_screen,
		panel_action_lock_setup_menu,
		panel_action_lock_invoke_menu
	},
	{
		PANEL_ACTION_LOGOUT, PANEL_STOCK_LOGOUT,
		N_("Log out of GNOME"), "gospanel-20",
		panel_action_logout, NULL, NULL
	},
	{
		PANEL_ACTION_RUN, PANEL_STOCK_RUN,
		N_("Run Program"), NULL,
		panel_action_run_program, NULL, NULL
	},
	{
		PANEL_ACTION_SEARCH, PANEL_STOCK_SEARCHTOOL,
		N_("Search for Files"), NULL,
		panel_action_search, NULL, NULL
	},
	{
		PANEL_ACTION_SCREENSHOT, PANEL_STOCK_SCREENSHOT,
		N_("Screenshot"), NULL,
		panel_action_screenshot, NULL, NULL
	}
};

static void
panel_action_button_finalize (GObject *object)
{
	PanelActionButton *button = PANEL_ACTION_BUTTON (object);

	button->priv->info = NULL;
	button->priv->type = PANEL_ACTION_NONE;

	gconf_client_notify_remove (panel_gconf_get_client (), button->priv->gconf_notify);
	button->priv->gconf_notify = 0;

	g_free (button->priv);
	button->priv = NULL;

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
				     panel_find_applet (widget));

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

	if (global_config.drawer_auto_close) {
		GtkWidget *parent;

		g_return_if_fail (PANEL_WIDGET (GTK_WIDGET (button)->parent)->panel_parent != NULL);

		parent = PANEL_WIDGET (GTK_WIDGET (button)->parent)->panel_parent;

		if (DRAWER_IS_WIDGET (parent)) {
			GtkWidget *grandparent;

			grandparent = PANEL_WIDGET (
						PANEL_WIDGET (
							BASEP_WIDGET (parent)->panel
						)->master_widget->parent
					)->panel_parent;

			drawer_widget_close_drawer (
				DRAWER_WIDGET (parent), grandparent);
		}
	}

	if (actions [button->priv->type].invoke)
		actions [button->priv->type].invoke (GTK_WIDGET (button));
}

static void
panel_action_button_class_init (PanelActionButtonClass *klass,
				gpointer               dummy)
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

	g_object_class_install_property (
			gobject_class,
			PROP_ACTION_TYPE,
			g_param_spec_enum ("action-type",
					   _("Action Type"),
					   _("The type of action this button implements"),
					   PANEL_TYPE_ACTION_BUTTON_TYPE,
					   PANEL_ORIENT_UP,
					   G_PARAM_READWRITE));
}

static void
panel_action_button_instance_init (PanelActionButton      *button,
				   PanelActionButtonClass *klass)
{
	static GtkTargetEntry dnd_targets [] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};

	button->priv = g_new0 (PanelActionButtonPrivate, 1);

	button->priv->type = PANEL_ACTION_NONE;
	button->priv->info = NULL;

	button->priv->gconf_notify = 0;

	GTK_WIDGET_UNSET_FLAGS (button, GTK_NO_WINDOW);
	gtk_drag_source_set (GTK_WIDGET (button), GDK_BUTTON1_MASK,
			     dnd_targets, 1,
			     GDK_ACTION_COPY | GDK_ACTION_MOVE);
	GTK_WIDGET_SET_FLAGS (button, GTK_NO_WINDOW);

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
}

static void
panel_action_button_type_changed (GConfClient       *client,
				  guint              cnxn_id,
				  GConfEntry        *entry,
				  PanelActionButton *button)
{
	PanelActionButtonType  type;
	const char            *action_type;

	g_return_if_fail (PANEL_IS_ACTION_BUTTON (button));

	if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
		return;

	action_type = gconf_value_get_string (entry->value);

	if (!gconf_string_to_enum (panel_action_type_map, action_type, (int *) &type))
		return;

	panel_action_button_set_type (button, type);
}

static void
panel_action_button_connect_to_gconf (PanelActionButton *button)
{
	GConfClient *client;
	const char  *key;
	const char  *profile;

	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, button->priv->info->gconf_key, "action_type");

	button->priv->gconf_notify =
		gconf_client_notify_add (client, key, 
					 (GConfClientNotifyFunc) panel_action_button_type_changed,
					 button, NULL, NULL);
}

GtkWidget *
panel_action_button_load (PanelActionButtonType  type,
			  PanelWidget           *panel,
			  int                    position,
			  gboolean               exactpos,
			  const char            *gconf_key,
			  gboolean               compatibility)
{
	PanelActionButton *button;
	AppletType         applet_type;

	g_return_val_if_fail (panel != NULL, NULL);

	button = g_object_new (PANEL_TYPE_ACTION_BUTTON, "action-type", type, NULL);

	applet_type = APPLET_ACTION;
	if (compatibility) { /* Backward compatibility with GNOME 2.0.x */
		if (type == PANEL_ACTION_LOCK)
			applet_type = APPLET_LOCK;
		else if (type == PANEL_ACTION_LOGOUT)
			applet_type = APPLET_LOGOUT;
	}

	button->priv->info = panel_applet_register (
				GTK_WIDGET (button), NULL, NULL, panel,
				position, exactpos, applet_type, gconf_key);
	if (!button->priv->info) {
		gtk_widget_destroy (GTK_WIDGET (button));
		return NULL;
	}

	panel_applet_add_callback (
		button->priv->info, "help", GTK_STOCK_HELP, _("_Help"));

	if (actions [button->priv->type].setup_menu)
		actions [button->priv->type].setup_menu (button);

	panel_action_button_connect_to_gconf (button);

	return GTK_WIDGET (button);
}

GtkWidget *
panel_action_button_load_from_gconf (PanelWidget *panel,
				     int          position,
				     gboolean     exactpos,
				     const char  *gconf_key)
{
	PanelActionButtonType  type = PANEL_ACTION_NONE;
	GConfClient           *client;
	const char            *key;
	const char            *profile;
	char                  *action_type;

	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "action_type");
	action_type = gconf_client_get_string (client, key, NULL);

	if (!gconf_string_to_enum (panel_action_type_map, action_type, (int *) &type)) {
		g_warning ("Unkown action type '%s' from %s", action_type, key);
		type = PANEL_ACTION_NONE;
	}

	g_free (action_type);

	if (type == PANEL_ACTION_NONE)
		return NULL;

	return panel_action_button_load (
			type, panel, position, exactpos, gconf_key, FALSE);
}

void
panel_action_button_save_to_gconf (PanelActionButton *button,
				   const char        *gconf_key)
{
	GConfClient *client;
	const char  *key;
	const char  *profile;
	const char  *action_type;

	g_return_if_fail (PANEL_IS_ACTION_BUTTON (button));

	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	key = panel_gconf_sprintf (
			"/apps/panel/profiles/%s/objects/%s", profile, gconf_key);
	panel_gconf_add_dir (key);

	key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "action_type");

	action_type = gconf_enum_to_string (panel_action_type_map, button->priv->type);

	g_assert (action_type != NULL);

	gconf_client_set_string (client, key, action_type, NULL);
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

		panel_show_help (screen, "wgospanel.xml",
				 actions [button->priv->type].help_index);

		return;
	}

	if (actions [button->priv->type].invoke_menu)
		actions [button->priv->type].invoke_menu (button, callback_name);
}

gboolean
panel_action_button_load_from_drag (const char  *drag_string,
				    PanelWidget *panel,
				    int          position,
				    gboolean     exactpos,
				    const char  *gconf_key,
				    int         *old_applet)
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

	if (!gconf_string_to_enum (panel_action_type_map, elements [1], (int *) &type)) {
		g_strfreev (elements);
		return retval;
	}

	g_return_val_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST, FALSE);

	if (strcmp (elements [2], "NEW")) {
		*old_applet = strtol (elements [2], NULL, 10);
		retval = TRUE; /* Remove the old applet */
	}

	g_strfreev (elements);

	panel_action_button_load (
			type, panel, position, exactpos, gconf_key, FALSE);

	return retval;
}
