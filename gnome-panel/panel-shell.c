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

#include <libpanel-util/panel-error.h>

#include "panel-shell.h"
#include "panel-session.h"
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

static void
panel_shell_register_error_dialog (int reg_res)
{
	GtkWidget *dlg;
	GtkWidget *checkbox;
	char      *secondary;

	secondary = g_strdup_printf (_("The panel could not register with the "
				       "bonobo-activation server (error code: "
				       "%d) and will exit.\n"
				       "It may be automatically restarted."),
				       reg_res);

	dlg = panel_error_dialog (NULL,
				  gdk_screen_get_default (),
				  "panel_shell_register_error",
				  FALSE,
				  _("The panel has encountered a fatal error"),
				  secondary);

	g_free (secondary);

	//FIXME: the checkbox is not correctly aligned in the dialog...
	checkbox = gtk_check_button_new_with_mnemonic (_("Force the panel to "
							 "not be automatically "
							 "restarted"));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox),
			    checkbox, FALSE, FALSE, 0);
	gtk_widget_show (checkbox);

	gtk_dialog_run (GTK_DIALOG (dlg));

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox)))
		panel_session_do_not_restart ();

	gtk_widget_destroy (dlg);
}

gboolean
panel_shell_register (void)
{
	gboolean retval;

	retval = TRUE;

        if (!panel_shell) {
		Bonobo_RegistrationResult  reg_res;

		panel_shell = g_object_new (PANEL_SHELL_TYPE, NULL);
		bonobo_object_set_immortal (BONOBO_OBJECT (panel_shell), TRUE);

		reg_res = panel_shell_bonobo_activation_register_for_display
				("OAFIID:GNOME_PanelShell",
				 BONOBO_OBJREF (panel_shell));

		switch (reg_res) {
		case Bonobo_ACTIVATION_REG_SUCCESS:
			break;
		case Bonobo_ACTIVATION_REG_ALREADY_ACTIVE:
			retval = FALSE;
			g_printerr ("A panel is already running.\n");
			panel_session_do_not_restart ();
			break;
		default:
			retval = FALSE;
			panel_shell_register_error_dialog (reg_res);
			break;
		}
	}

	return retval;
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

