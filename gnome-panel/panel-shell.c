#include <config.h>

#include "panel-shell.h"

struct _PanelShellPrivate {
	int dummy;
};

static GObjectClass *panel_shell_parent_class = NULL;

static void
impl_runBox (PortableServer_Servant  servant,
	     const CORBA_char       *initial_string,
	     CORBA_Environment      *ev)
{
	PanelShell *shell;

	shell = PANEL_SHELL (bonobo_object (servant));

	g_message ("run: %s\n", initial_string);
}

static void
panel_shell_finalize (GObject *object)
{
	PanelShell *shell = PANEL_SHELL (object);

	if (shell->priv) {
		g_free (shell->priv);
		shell->priv = NULL;
	}

	panel_shell_parent_class->finalize (object);
}

static void
panel_shell_class_init (PanelShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	klass->epv.runBox = impl_runBox;

	object_class->finalize = panel_shell_finalize;

	panel_shell_parent_class = g_type_class_peek_parent (klass);
}

static void
panel_shell_init (PanelShell *shell)
{
	shell->priv = g_new0 (PanelShellPrivate, 1);
}

BONOBO_TYPE_FUNC_FULL (PanelShell,
		       GNOME_PanelShell,
		       BONOBO_OBJECT_TYPE,
		       panel_shell);

