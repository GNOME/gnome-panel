#include <config.h>
#include <libgnome/gnome-i18n.h>

#include <gtk/gtk.h>

#include "panel-shell.h"
#include "panel-util.h"

struct _PanelShellPrivate {
	int dummy;
};

static GObjectClass *panel_shell_parent_class = NULL;

/*
 * PanelShell is a singleton.
 */
static PanelShell *panel_shell = NULL;

gboolean
panel_shell_register (void)
{
        if (!panel_shell) {
		Bonobo_RegistrationResult  reg_res;
		char                      *message = NULL;
		char                      *iid;

		panel_shell = g_object_new (PANEL_SHELL_TYPE, NULL);
		bonobo_object_set_immortal (BONOBO_OBJECT (panel_shell), TRUE);

		iid = bonobo_activation_make_registration_id (
				"OAFIID:GNOME_PanelShell", g_getenv ("DISPLAY"));

		reg_res = bonobo_activation_active_server_register (
				iid, BONOBO_OBJREF (panel_shell));

		g_free (iid);

		switch (reg_res) {
		case Bonobo_ACTIVATION_REG_SUCCESS:
			break;
		case Bonobo_ACTIVATION_REG_ALREADY_ACTIVE:
			message = _("I've detected a panel already running,\n"
				    "and will now exit.");
			break;
		default:
			message = g_strdup_printf (_("There was a problem registering the panel "
						     "with the bonobo-activation server.\n"
						     "The error code is: %d\n"
						     "The panel will now exit."), reg_res);
			break;
		}

		if (message) {
			GtkWidget *dlg = panel_error_dialog (
						gdk_screen_get_default (),
						"panel_shell_register_error",
						"%s", message);

			/* FIXME: quick hack */
			g_signal_handlers_disconnect_by_func
				(dlg, G_CALLBACK (gtk_widget_destroy), dlg);
			gtk_dialog_run (GTK_DIALOG (dlg));
			gtk_widget_destroy (dlg);
			return FALSE;
		}
	}

	return TRUE;
}

void
panel_shell_unregister (void)
{
	bonobo_activation_active_server_unregister ("OAFIID:GNOME_PanelShell",
						    BONOBO_OBJREF (panel_shell));
}

static void
impl_displayRunDialog (PortableServer_Servant  servant,
		       const CORBA_char       *initial_string,
		       CORBA_Environment      *ev)
{
	PanelShell *shell;

	shell = PANEL_SHELL (bonobo_object (servant));

	g_message ("displayRunDialog: %s\n", initial_string);
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

	klass->epv.displayRunDialog = impl_displayRunDialog;

	object_class->finalize = panel_shell_finalize;

	panel_shell_parent_class = g_type_class_peek_parent (klass);
}

static void
panel_shell_init (PanelShell *shell)
{
	shell->priv = g_new0 (PanelShellPrivate, 1);
}

BONOBO_TYPE_FUNC_FULL (PanelShell,
		       GNOME_Vertigo_PanelShell,
		       BONOBO_OBJECT_TYPE,
		       panel_shell);

