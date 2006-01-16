/*
 * panel-logout.c:
 *
 * Copyright (C) 2006 Vincent Untz
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
 *	Vincent Untz <vuntz@gnome.org>
 */


#include <config.h>

#include <glib/gi18n.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>

#include "panel-logout.h"
#include "panel-gdm.h"
#include "panel-session.h"

#define PANEL_LOGOUT_DIALOG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_LOGOUT_DIALOG, PanelLogoutDialogPrivate))

#define AUTOMATIC_ACTION_TIMEOUT 60

enum {
	PANEL_LOGOUT_RESPONSE_LOGOUT,
	PANEL_LOGOUT_RESPONSE_SWITCH_USER,
	PANEL_LOGOUT_RESPONSE_SHUTDOWN,
	PANEL_LOGOUT_RESPONSE_REBOOT,
	PANEL_LOGOUT_RESPONSE_STD,
	PANEL_LOGOUT_RESPONSE_STR
};

struct _PanelLogoutDialogPrivate {
	PanelLogoutDialogType type;

	int                   timeout;
	unsigned int          timeout_id;

	unsigned int          default_response;
	
};

static void panel_logout_destroy (PanelLogoutDialog *logout_dialog,
				  gpointer           data);
static void panel_logout_response (GtkWidget *logout_dialog,
				   guint      response_id,
				   gpointer   data);

G_DEFINE_TYPE (PanelLogoutDialog, panel_logout, GTK_TYPE_MESSAGE_DIALOG);

static void
panel_logout_class_init (PanelLogoutDialogClass *klass)
{
	g_type_class_add_private (klass, sizeof (PanelLogoutDialogPrivate));
}

static void
panel_logout_init (PanelLogoutDialog *logout_dialog)
{
	logout_dialog->priv = PANEL_LOGOUT_DIALOG_GET_PRIVATE (logout_dialog);

	logout_dialog->priv->timeout_id = 0;
	logout_dialog->priv->timeout    = AUTOMATIC_ACTION_TIMEOUT;
	logout_dialog->priv->default_response = GTK_RESPONSE_CANCEL;

	//FIXME gtk_window_set_transient_for (GTK_WINDOW (logout_dialog), );
	//above all, always visible
	gtk_window_set_skip_pager_hint (GTK_WINDOW (logout_dialog), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (logout_dialog), TRUE);

	g_signal_connect (logout_dialog, "destroy",
			  G_CALLBACK (panel_logout_destroy), NULL);
	g_signal_connect (logout_dialog, "response",
			  G_CALLBACK (panel_logout_response), NULL);
}

static void
panel_logout_destroy (PanelLogoutDialog *logout_dialog,
		      gpointer           data)
{
	if (logout_dialog->priv->timeout_id != 0)
		g_source_remove (logout_dialog->priv->timeout_id);
	logout_dialog->priv->timeout_id = 0;
}

static void
panel_logout_response (GtkWidget *logout_dialog,
		       guint      response_id,
		       gpointer   data)
{
	gtk_widget_destroy (logout_dialog);

	switch (response_id) {
	case GTK_RESPONSE_CANCEL:
		break;
	case PANEL_LOGOUT_RESPONSE_LOGOUT:
		gdm_set_logout_action (GDM_LOGOUT_ACTION_NONE);
		panel_session_request_logout ();
		break;
	case PANEL_LOGOUT_RESPONSE_SWITCH_USER:
		gdm_new_login ();
		break;
	case PANEL_LOGOUT_RESPONSE_SHUTDOWN:
		gdm_set_logout_action (GDM_LOGOUT_ACTION_SHUTDOWN);
		panel_session_request_logout ();
		break;
	case PANEL_LOGOUT_RESPONSE_REBOOT:
		gdm_set_logout_action (GDM_LOGOUT_ACTION_REBOOT);
		panel_session_request_logout ();
		break;
	case PANEL_LOGOUT_RESPONSE_STD:
		gdm_set_logout_action (GDM_LOGOUT_ACTION_SUSPEND);
		panel_session_request_logout ();
		break;
	case PANEL_LOGOUT_RESPONSE_STR:
		//FIXME
		break;
	default:
		g_assert_not_reached ();
	}
}

static gboolean
panel_logout_timeout (gpointer data)
{
	PanelLogoutDialog *logout_dialog;
	char              *secondary_text;

	logout_dialog = (PanelLogoutDialog *) data;

	if (!logout_dialog->priv->timeout) {
		gtk_dialog_response (GTK_DIALOG (logout_dialog),
				     logout_dialog->priv->default_response);

		return FALSE;
	}

	switch (logout_dialog->priv->type) {
	case PANEL_LOGOUT_DIALOG_LOGOUT:
		secondary_text = ngettext ("You will be automatically logged "
					   "out in %d second.",
					   "You will be automatically logged "
					   "out in %d seconds.",
					   logout_dialog->priv->timeout);
		break;
	case PANEL_LOGOUT_DIALOG_SHUTDOWN:
		secondary_text = ngettext ("This system will be automatically "
					   "shut down in %d second.",
					   "This system will be automatically "
					   "shut down in %d seconds.",
					   logout_dialog->priv->timeout);
		break;
	default:
		g_assert_not_reached ();
	}

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (logout_dialog),
						  secondary_text,
						  NULL);

	logout_dialog->priv->timeout--;

	return TRUE;
}

//FIXME should take a GdkScreen as argument
//FIXME we shouldn't be able to have two such dialogs
void
panel_logout_new (PanelLogoutDialogType type)
{
	PanelLogoutDialog *logout_dialog;
	char              *icon_name;
	char              *primary_text;

	logout_dialog = g_object_new (PANEL_TYPE_LOGOUT_DIALOG, NULL);
	gtk_window_set_title (GTK_WINDOW (logout_dialog), "");

	logout_dialog->priv->type = type;

	icon_name    = NULL;
	primary_text = NULL;

	switch (type) {
	case PANEL_LOGOUT_DIALOG_LOGOUT:
		icon_name      = "gnome-logout";
		primary_text   = _("Are you sure you want to log out now?");
		// FIXME need to verify that this response can be used
		// FIXME set default button
		logout_dialog->priv->default_response = PANEL_LOGOUT_DIALOG_LOGOUT;

		//FIXME is gdm running?
		gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
				       _("_Switch User"),
				       PANEL_LOGOUT_RESPONSE_SWITCH_USER);
		gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
				       GTK_STOCK_CANCEL,
				       GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
				       _("_Log Out"),
				       PANEL_LOGOUT_RESPONSE_LOGOUT);
		break;
	case PANEL_LOGOUT_DIALOG_SHUTDOWN:
		icon_name      = "gnome-shutdown";
		primary_text   = _("Are you sure you want to shut down this "
				   "system now?");

		logout_dialog->priv->default_response = PANEL_LOGOUT_DIALOG_SHUTDOWN;
		//FIXME see previous FIXME

		if (gdm_supports_logout_action (GDM_LOGOUT_ACTION_SUSPEND))
			gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
					       _("_Suspend"),
					       PANEL_LOGOUT_RESPONSE_STD);

		gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
				       _("_Reboot"),
				       PANEL_LOGOUT_RESPONSE_REBOOT);
		gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
				       GTK_STOCK_CANCEL,
				       GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
				       _("_Shut Down"),
				       PANEL_LOGOUT_RESPONSE_SHUTDOWN);
		break;
	default:
		g_assert_not_reached ();
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (GTK_MESSAGE_DIALOG (logout_dialog)->image),
				      icon_name, GTK_ICON_SIZE_DIALOG);

	gtk_label_set_text (GTK_LABEL (GTK_MESSAGE_DIALOG (logout_dialog)->label),
			    primary_text);

	gtk_dialog_set_default_response (GTK_DIALOG (logout_dialog),
					 logout_dialog->priv->default_response);

	/* Sets the secondary text */
	panel_logout_timeout (logout_dialog);

	logout_dialog->priv->timeout_id = g_timeout_add (1000,
							 panel_logout_timeout,
							 logout_dialog);

	gtk_widget_show (GTK_WIDGET (logout_dialog));
}
