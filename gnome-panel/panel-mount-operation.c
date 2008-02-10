/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Based on eel-mount-operation.c - Gtk+ implementation for GMountOperation

   Copyright (C) 2007 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include <glib/gi18n.h>
#include "panel-mount-operation.h"
#include <libgnomeui/gnome-password-dialog.h>
#include <gtk/gtkmessagedialog.h>

G_DEFINE_TYPE (PanelMountOperation, panel_mount_operation, G_TYPE_MOUNT_OPERATION);

enum {
	ACTIVE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct PanelMountOperationPrivate {
	GtkWindow *parent_window;
	gboolean is_active;
};
	
static void
panel_mount_operation_finalize (GObject *object)
{
	PanelMountOperation *operation;
	PanelMountOperationPrivate *priv;

	operation = PANEL_MOUNT_OPERATION (object);

	priv = operation->priv;

	if (priv->parent_window) {
		g_object_unref (priv->parent_window);
	}
  
	(*G_OBJECT_CLASS (panel_mount_operation_parent_class)->finalize) (object);
}

static void
set_active (PanelMountOperation *operation,
	    gboolean is_active)
{
	if (operation->priv->is_active != is_active) {
		operation->priv->is_active = is_active;
		g_signal_emit (operation, signals[ACTIVE_CHANGED], 0, is_active);
	}
}

static void
password_dialog_button_clicked (GtkDialog *dialog, 
				gint button_number, 
				GMountOperation *op)
{
	char *username, *domain, *password;
	gboolean anon;
	GnomePasswordDialog *gpd;

	gpd = GNOME_PASSWORD_DIALOG (dialog);

	if (button_number == GTK_RESPONSE_OK) {
		username = gnome_password_dialog_get_username (gpd);
		if (username) {
			g_mount_operation_set_username (op, username);
			g_free (username);
		}

		domain = gnome_password_dialog_get_domain (gpd);
		if (domain) {
			g_mount_operation_set_domain (op, domain);
			g_free (domain);
		}
		
		password = gnome_password_dialog_get_password (gpd);
		if (password) {
			g_mount_operation_set_password (op, password);
			g_free (password);
		}

		anon = gnome_password_dialog_anon_selected (gpd);
		g_mount_operation_set_anonymous (op, anon);

		g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
	} else {
		g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
	set_active (PANEL_MOUNT_OPERATION (op), FALSE);
	g_object_unref (op);
}

static void
ask_password (GMountOperation *op,
	      const char      *message,
	      const char      *default_user,
	      const char      *default_domain,
	      GAskPasswordFlags flags)
{
	GtkWidget *dialog;

	dialog = gnome_password_dialog_new (_("Enter Password"),
					    message,
					    default_user,
					    "",
					    FALSE);

	gnome_password_dialog_set_show_password (GNOME_PASSWORD_DIALOG (dialog),
						 flags & G_ASK_PASSWORD_NEED_PASSWORD);
	
	gnome_password_dialog_set_show_username (GNOME_PASSWORD_DIALOG (dialog),
						 flags & G_ASK_PASSWORD_NEED_USERNAME);
	gnome_password_dialog_set_show_domain (GNOME_PASSWORD_DIALOG (dialog),
					       flags & G_ASK_PASSWORD_NEED_DOMAIN);
	gnome_password_dialog_set_show_userpass_buttons	(GNOME_PASSWORD_DIALOG (dialog),
							 flags & G_ASK_PASSWORD_ANONYMOUS_SUPPORTED);
	if (default_domain) {
		gnome_password_dialog_set_domain (GNOME_PASSWORD_DIALOG (dialog),
						  default_domain);
	}
		
	if (PANEL_MOUNT_OPERATION (op)->priv->parent_window != NULL) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog),
					      PANEL_MOUNT_OPERATION (op)->priv->parent_window);
	}

	g_signal_connect (dialog, "response", 
			  G_CALLBACK (password_dialog_button_clicked), op);

	set_active (PANEL_MOUNT_OPERATION (op), TRUE);
	gtk_widget_show (GTK_WIDGET (dialog));
	g_object_ref (op);
}


static void
question_dialog_button_clicked (GtkDialog *dialog, 
				gint button_number, 
				GMountOperation *op)
{
	if (button_number >= 0) {
		g_mount_operation_set_choice (op, button_number);
		g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
	} else {
		g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);
	}
	
	gtk_widget_destroy (GTK_WIDGET (dialog));
	set_active (PANEL_MOUNT_OPERATION (op), FALSE);
	g_object_unref (op);
}

  
static void
ask_question (GMountOperation *op,
	      const char      *message,
	      const char      *choices[])
{
	GtkWidget *dialog;
	int cnt, len;
	
	dialog = gtk_message_dialog_new (PANEL_MOUNT_OPERATION (op)->priv->parent_window,
					 0, GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE, "%s", message);
	
	if (choices) {
		/* First count the items in the list then 
		 * add the buttons in reverse order */
		for (len = 0; choices[len] != NULL; len++) {
			;
		}
		
		for (cnt = len - 1; cnt >= 0; cnt--) {
			gtk_dialog_add_button (GTK_DIALOG (dialog), choices[cnt], cnt);
		}
	}


	g_signal_connect (GTK_OBJECT(dialog), "response", 
			  G_CALLBACK (question_dialog_button_clicked), op);

	set_active (PANEL_MOUNT_OPERATION (op), TRUE);
	
	gtk_widget_show (GTK_WIDGET (dialog));

	g_object_ref (op);
}

static void
panel_mount_operation_class_init (PanelMountOperationClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GMountOperationClass *gmountoperation_class = G_MOUNT_OPERATION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (PanelMountOperationPrivate));
	
	gobject_class->finalize = panel_mount_operation_finalize;
	
	gmountoperation_class->ask_password = ask_password;
	gmountoperation_class->ask_question = ask_question;


	signals[ACTIVE_CHANGED] =
		g_signal_new ("active_changed",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (PanelMountOperationClass, active_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1,
			      G_TYPE_BOOLEAN);
}

static void
panel_mount_operation_init (PanelMountOperation *operation)
{
	operation->priv = G_TYPE_INSTANCE_GET_PRIVATE (operation,
						       PANEL_TYPE_MOUNT_OPERATION,
						       PanelMountOperationPrivate);
}

GMountOperation *
panel_mount_operation_new (GtkWindow *parent)
{
	PanelMountOperation *mount_operation;

	mount_operation = g_object_new (panel_mount_operation_get_type (), NULL);

	if (parent) {
		mount_operation->priv->parent_window = g_object_ref (parent);
	}

	return G_MOUNT_OPERATION (mount_operation);
}
