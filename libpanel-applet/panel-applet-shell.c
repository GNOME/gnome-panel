/*
 * panel-applet-shell.c: the panel's interface to the applet.
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
 *     Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-applet-shell.h"
#include "panel-applet.h"
#include "panel-applet-private.h"

struct _PanelAppletShellPrivate {
	PanelApplet *applet;
};

static GObjectClass *parent_class = NULL;

static void
panel_applet_shell_finalize (GObject *object)
{
	PanelAppletShell *shell = PANEL_APPLET_SHELL (object);

	if (shell->priv) {
		g_free (shell->priv);
		shell->priv = NULL;
	}

	parent_class->finalize (object);
}

static void
panel_applet_shell_class_init (PanelAppletShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = panel_applet_shell_finalize;

	parent_class = g_type_class_peek_parent (klass);
}

static void
panel_applet_shell_init (PanelAppletShell *shell)
{
	shell->priv = g_new0 (PanelAppletShellPrivate, 1);

	shell->priv->applet = NULL;
}

BONOBO_TYPE_FUNC_FULL (PanelAppletShell,
		       GNOME_Vertigo_PanelAppletShell,
		       BONOBO_OBJECT_TYPE,
		       panel_applet_shell);

void
panel_applet_shell_construct (PanelAppletShell *shell,
			      PanelApplet      *applet)
{
	shell->priv->applet = applet;
}

PanelAppletShell *
panel_applet_shell_new (PanelApplet *applet)
{
	PanelAppletShell *shell;

	shell = g_object_new (PANEL_APPLET_SHELL_TYPE, NULL);

	panel_applet_shell_construct (shell, applet);

	return shell;
}
