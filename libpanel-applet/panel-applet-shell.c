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
impl_GNOME_PanelAppletShell_changeBackground (PortableServer_Servant       servant,
					      const GNOME_PanelBackground *background,
					      CORBA_Environment           *ev)
{
	PanelAppletShell *shell;

        shell = PANEL_APPLET_SHELL (bonobo_object (servant));

	switch (background->_d) {
	case GNOME_NONE:
		panel_applet_clear_background (shell->priv->applet);
		break;
	case GNOME_COLOUR: {
		GdkColor colour;

		colour.red   = background->_u.colour.red;
                colour.green = background->_u.colour.green;
                colour.blue  = background->_u.colour.blue;

		panel_applet_set_background_colour (shell->priv->applet, &colour);
		}
		break;
	case GNOME_PIXMAP:
		panel_applet_set_background_pixmap (shell->priv->applet,
						    background->_u.pixmap);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
impl_GNOME_PanelAppletShell_changeOrientation (PortableServer_Servant  servant,
					       GNOME_PanelOrient       orient,
					       CORBA_Environment      *ev)
{
	PanelAppletShell *shell;

	shell = PANEL_APPLET_SHELL (bonobo_object (servant));
	
	panel_applet_change_orient (shell->priv->applet, orient);
}

static void
impl_GNOME_PanelAppletShell_changeSize (PortableServer_Servant  servant,
					const GNOME_PanelSize   size,
					CORBA_Environment      *ev)
{
	PanelAppletShell *shell;

	shell = PANEL_APPLET_SHELL (bonobo_object (servant));
	
	panel_applet_change_size (shell->priv->applet, size);
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

	klass->epv.changeOrientation = impl_GNOME_PanelAppletShell_changeOrientation;
	klass->epv.changeSize        = impl_GNOME_PanelAppletShell_changeSize;
	klass->epv.changeBackground  = impl_GNOME_PanelAppletShell_changeBackground;

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
