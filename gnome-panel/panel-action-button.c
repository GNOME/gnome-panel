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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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
#include <libpanel-util/panel-show.h>

#include "applet.h"
#include "panel-typebuiltins.h"
#include "panel-util.h"
#include "panel-run-dialog.h"
#include "panel-a11y.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-icon-names.h"

enum {
	PROP_0,
	PROP_ACTION_TYPE
};

struct _PanelActionButtonPrivate {
	PanelActionButtonType  type;
	AppletInfo            *info;
};

G_DEFINE_TYPE_WITH_PRIVATE (PanelActionButton, panel_action_button, BUTTON_TYPE_WIDGET)

typedef struct {
	gint enum_value;
	const gchar* str;
} PanelEnumStringPair;

static PanelEnumStringPair panel_action_type_map [] = {
	{ PANEL_ACTION_NONE,           "none"           },
	{ PANEL_ACTION_RUN,            "run"            },
	{ 0,                           NULL             },
};

static gboolean
panel_string_to_enum (const gchar *str, gint *enum_value_retloc)
{
	int i = 0;
	while (panel_action_type_map[i].str != NULL)
	{
		if (g_ascii_strcasecmp (panel_action_type_map[i].str, str) == 0)
		{
			*enum_value_retloc = panel_action_type_map[i].enum_value;
			return TRUE;
		}
		++i;
	}
	return FALSE;
}

static const gchar*
panel_enum_to_string (gint enum_value)
{
	int i = 0;
	while (panel_action_type_map[i].str != NULL)
	{
		if (panel_action_type_map[i].enum_value == enum_value)
			return panel_action_type_map[i].str;
		++i;
	}
	return NULL;
}

/* Run Application
 */
static void
panel_action_run_program (GtkWidget *widget)
{
	panel_run_dialog_present (gtk_widget_get_screen (widget),
				  gtk_get_current_event_time ());
}

typedef struct {
	PanelActionButtonType   type;
	const gchar            *icon_name;
	const gchar            *text;
	const gchar            *tooltip;
	const gchar            *drag_id;
	void                  (*invoke)      (GtkWidget         *widget);
	gboolean              (*is_disabled) (void);
} PanelAction;

/* Keep order in sync with PanelActionButtonType
 */
static PanelAction actions [] = {
	{
		PANEL_ACTION_NONE,
		NULL, NULL, NULL, NULL,
		NULL, NULL
	},
	{
		PANEL_ACTION_RUN,
		PANEL_ICON_RUN,
		N_("Run Application..."),
		N_("Run an application by typing a command or choosing from a list"),
		"ACTION:run:NEW",
		panel_action_run_program,
		panel_lockdown_get_disable_command_line_s
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
	GtkButtonClass *button_class  = (GtkButtonClass *) klass;

	gobject_class->finalize     = panel_action_button_finalize;
	gobject_class->get_property = panel_action_button_get_property;
	gobject_class->set_property = panel_action_button_set_property;

	button_class->clicked       = panel_action_button_clicked;

	g_object_class_install_property (
			gobject_class,
			PROP_ACTION_TYPE,
			g_param_spec_enum ("action-type",
					   "Action Type",
					   "The type of action this button implements",
					   PANEL_TYPE_ACTION_BUTTON_TYPE,
					   PANEL_ORIENTATION_TOP,
					   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
panel_action_button_init (PanelActionButton *button)
{
	button->priv = panel_action_button_get_instance_private (button);

	button->priv->type = PANEL_ACTION_NONE;
	button->priv->info = NULL;
}

void
panel_action_button_set_type (PanelActionButton     *button,
			      PanelActionButtonType  type)
{
	g_return_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST);

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
	panel_layout_object_create (PANEL_OBJECT_ACTION,
				    panel_enum_to_string (type),
				    panel_toplevel_get_id (toplevel),
				    pack_type, pack_index,
				    NULL);
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
		g_debug ("Unknown action type '%s'", detail_for_type);
		panel_layout_delete_object (id);
		return;
	}

	panel_action_button_load_helper (panel, id, settings, type);
}

void
panel_action_button_load_from_drag (PanelToplevel       *toplevel,
				    PanelObjectPackType  pack_type,
				    int                  pack_index,
				    const char          *drag_string)
{
	PanelActionButtonType   type = PANEL_ACTION_NONE;
	char                  **elements;

	if (strncmp (drag_string, "ACTION:", strlen ("ACTION:")))
		return;

	elements = g_strsplit (drag_string, ":", 0);

	g_assert (elements != NULL);

	if (!elements [1] || !elements [2]) {
		g_strfreev (elements);
		return;
	}

	if (!panel_string_to_enum (elements [1], (gpointer) &type)) {
		g_strfreev (elements);
		return;
	}

	g_strfreev (elements);

	g_return_if_fail (type > PANEL_ACTION_NONE && type < PANEL_ACTION_LAST);

	panel_action_button_create (toplevel, pack_type, pack_index, type);
}
