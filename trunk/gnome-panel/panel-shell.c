/*
 * panel-shell.c: panel shell interface implementation
 *
 * Copyright (C) 2001 Ximian, Inc.
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
 *      Jacob Berkman <jacob@ximian.com>
 */

#include <config.h>
#include <glib/gi18n.h>

#include <string.h>
#include <gtk/gtk.h>

#include "panel-shell.h"
#include "panel-util.h"

/*
 * PanelShell is a singleton.
 */
static PanelShell *panel_shell = NULL;

static Bonobo_RegistrationResult
panel_shell_bonobo_activation_register_for_display (const char    *iid,
						    Bonobo_Unknown ref)
{
	const char *display_name;
	GSList     *reg_env ;
	Bonobo_RegistrationResult result;
	
	display_name = gdk_display_get_name (gdk_display_get_default ());
	reg_env = bonobo_activation_registration_env_set (NULL,
							  "DISPLAY",
							  display_name);
	result = bonobo_activation_register_active_server (iid, ref, reg_env);
	bonobo_activation_registration_env_free (reg_env);
	return result;
}

gboolean
panel_shell_register (void)
{
        if (!panel_shell) {
		Bonobo_RegistrationResult  reg_res;
		char                      *message = NULL;

		panel_shell = g_object_new (PANEL_SHELL_TYPE, NULL);
		bonobo_object_set_immortal (BONOBO_OBJECT (panel_shell), TRUE);

		reg_res = panel_shell_bonobo_activation_register_for_display
				("OAFIID:GNOME_PanelShell",
				 BONOBO_OBJREF (panel_shell));

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
						NULL, gdk_screen_get_default (),
						"panel_shell_register_error",
						FALSE, message, NULL);

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
	bonobo_activation_unregister_active_server ("OAFIID:GNOME_PanelShell",
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
panel_shell_class_init (PanelShellClass *klass)
{
	klass->epv.displayRunDialog = impl_displayRunDialog;
}

static void
panel_shell_init (PanelShell *shell)
{
}

BONOBO_TYPE_FUNC_FULL (PanelShell,
		       GNOME_Vertigo_PanelShell,
		       BONOBO_OBJECT_TYPE,
		       panel_shell)

