/*
 * panel-applet-shell.c:
 *
 * Authors:
 *   Mark McLoughlin <mark@skynet.ie>
 *
 * Copyright 2001 Sun Microsystems, Inc.
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
impl_PanelAppletShell_saveYourself (PortableServer_Servant  servant,
				    const CORBA_char       *global_key,
				    const CORBA_char       *private_key,
				    CORBA_Environment      *ev)
{
	PanelAppletShell *shell;

	shell = PANEL_APPLET_SHELL (bonobo_object (servant));

	panel_applet_save_yourself (shell->priv->applet, global_key, private_key);
}

static void
impl_PanelAppletShell_getExpandFlags (PortableServer_Servant  servant,
				      CORBA_boolean          *expand_major,
				      CORBA_boolean          *expand_minor,
				      CORBA_Environment      *ev)
{
	PanelAppletShell *shell;
	gboolean major, minor;

	shell = PANEL_APPLET_SHELL (bonobo_object (servant));

	panel_applet_get_expand_flags (shell->priv->applet,
				       &major, &minor);

	*expand_major = major;
	*expand_minor = minor;
}


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

	klass->epv.saveYourself = impl_PanelAppletShell_saveYourself;
	klass->epv.getExpandFlags = impl_PanelAppletShell_getExpandFlags;

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
		       GNOME_PanelAppletShell,
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
