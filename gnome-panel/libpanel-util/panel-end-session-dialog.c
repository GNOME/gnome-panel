/*
 * panel-session.c:
 *
 * Copyright (C) 2008 Novell, Inc.
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
 *	Vincent Untz <vuntz@gnome.org>
 */

#include <gio/gio.h>

#include "dbus-login1-manager.h"
#include "panel-cleanup.h"
#include "panel-end-session-dialog.h"
#include "panel-util-types.h"

#define END_SESSION_DIALOG_NAME      "org.gnome.Shell"
#define END_SESSION_DIALOG_PATH      "/org/gnome/SessionManager/EndSessionDialog"
#define END_SESSION_DIALOG_INTERFACE "org.gnome.SessionManager.EndSessionDialog"

#define AUTOMATIC_ACTION_TIMEOUT 60

struct _PanelEndSessionDialogPrivate {
	GDBusProxy    *dialog_proxy;
	Login1Manager *login1_proxy;
};

/* Should match enum values with FlashbackLogoutAction in
 * flashback-inhibit-dialog.h in gnome-flashback module */
typedef enum {
	REQUEST_TYPE_HIBERNATE    = 3,
	REQUEST_TYPE_SUSPEND      = 4,
	REQUEST_TYPE_HYBRID_SLEEP = 5
} RequestType;

G_DEFINE_TYPE_WITH_PRIVATE (PanelEndSessionDialog, panel_end_session_dialog, G_TYPE_OBJECT);

static void
open_ready_callback (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
	PanelEndSessionDialog *dialog;
	GError *error;
	GVariant *ret;

	dialog = PANEL_END_SESSION_DIALOG (user_data);
	error = NULL;
	ret = g_dbus_proxy_call_finish (dialog->priv->dialog_proxy, res, &error);

	if (error) {
		g_warning ("Unable to make Open call: %s", error->message);
		g_error_free (error);
		return;
	}

	g_variant_unref (ret);
}

static void
panel_end_session_dialog_do_request (PanelEndSessionDialog *dialog,
                                     RequestType            type)
{
	const gchar *inhibitors[] = { NULL };
	GVariant *parameters;

	g_return_if_fail (PANEL_IS_END_SESSION_DIALOG (dialog));

	if (!dialog->priv->dialog_proxy) {
		g_warning ("End session dialog is not available");
		return;
	}

	parameters = g_variant_new ("(uuu^ao)",
	                            (guint) type,
	                            0,
	                            AUTOMATIC_ACTION_TIMEOUT,
	                            inhibitors);
	g_dbus_proxy_call (dialog->priv->dialog_proxy,
	                   "Open",
	                   parameters,
	                   G_DBUS_CALL_FLAGS_NONE,
	                   -1,
	                   NULL,
	                   (GAsyncReadyCallback) open_ready_callback,
	                   dialog);
}

static void
panel_end_session_dialog_on_signal (GDBusProxy *proxy,
                                    gchar      *sender_name,
                                    gchar      *signal_name,
                                    GVariant   *parameters,
                                    gpointer    user_data)
{
	PanelEndSessionDialog *dialog = PANEL_END_SESSION_DIALOG (user_data);

	if (!dialog->priv->login1_proxy)
		return;

	if (g_str_equal ("ConfirmedHibernate", signal_name)) {
		login1_manager_call_hibernate_sync (dialog->priv->login1_proxy,
		                                    FALSE,
		                                    NULL,
		                                    NULL);
	} else if (g_str_equal ("ConfirmedSuspend", signal_name)) {
		login1_manager_call_suspend_sync (dialog->priv->login1_proxy,
		                                  FALSE,
		                                  NULL,
		                                  NULL);
	} else if (g_str_equal ("ConfirmedHybridSleep", signal_name)) {
		login1_manager_call_hybrid_sleep_sync (dialog->priv->login1_proxy,
		                                       FALSE,
		                                       NULL,
		                                       NULL);
	}
}

static void
panel_end_session_dialog_finalize (GObject *object)
{
	PanelEndSessionDialog *dialog = PANEL_END_SESSION_DIALOG (object);

	g_clear_object (&dialog->priv->login1_proxy);
	g_clear_object (&dialog->priv->dialog_proxy);

	G_OBJECT_CLASS (panel_end_session_dialog_parent_class)->finalize (object);
}

static void
panel_end_session_dialog_class_init (PanelEndSessionDialogClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);

	gobject_class->finalize = panel_end_session_dialog_finalize;
}

static void
panel_end_session_dialog_init (PanelEndSessionDialog *dialog)
{
	GError *error;

	dialog->priv = panel_end_session_dialog_get_instance_private (dialog);

	error = NULL;

	dialog->priv->dialog_proxy =
		g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
		                               G_DBUS_PROXY_FLAGS_NONE,
		                               NULL,
		                               END_SESSION_DIALOG_NAME,
		                               END_SESSION_DIALOG_PATH,
		                               END_SESSION_DIALOG_INTERFACE,
		                               NULL,
		                               &error);

	if (error) {
		g_warning ("Could not connect to end session dialog: %s", error->message);
		g_error_free (error);
		return;
	}

	g_signal_connect (dialog->priv->dialog_proxy, "g-signal",
	                  G_CALLBACK (panel_end_session_dialog_on_signal), dialog);

	dialog->priv->login1_proxy =
		login1_manager_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
		                                       G_DBUS_PROXY_FLAGS_NONE,
		                                       "org.freedesktop.login1",
		                                       "/org/freedesktop/login1",
		                                       NULL,
		                                       NULL);
}

PanelEndSessionDialog *
panel_end_session_dialog_get (void)
{
	static PanelEndSessionDialog *dialog = NULL;

	if (dialog == NULL) {
		dialog = g_object_new (PANEL_TYPE_END_SESSION_DIALOG, NULL);
		panel_cleanup_register (panel_cleanup_unref_and_nullify, &dialog);
	}

	return dialog;
}

gboolean
panel_end_session_dialog_is_hibernate_available (PanelEndSessionDialog *dialog)
{
	gchar *result;
	gboolean ret;

	g_return_val_if_fail (PANEL_IS_END_SESSION_DIALOG (dialog), FALSE);

	if (!dialog->priv->login1_proxy)
		return FALSE;

	login1_manager_call_can_hibernate_sync (dialog->priv->login1_proxy,
	                                        &result,
	                                        NULL,
	                                        NULL);

	ret = FALSE;
	if (g_str_equal ("yes", result))
		ret = TRUE;

	g_free (result);

	return ret;
}

void
panel_end_session_dialog_request_hibernate (PanelEndSessionDialog *dialog)
{
	panel_end_session_dialog_do_request (dialog, REQUEST_TYPE_HIBERNATE);
}

gboolean
panel_end_session_dialog_is_suspend_available (PanelEndSessionDialog *dialog)
{
	gchar *result;
	gboolean ret;

	g_return_val_if_fail (PANEL_IS_END_SESSION_DIALOG (dialog), FALSE);

	if (!dialog->priv->login1_proxy)
		return FALSE;

	login1_manager_call_can_suspend_sync (dialog->priv->login1_proxy,
	                                      &result,
	                                      NULL,
	                                      NULL);

	ret = FALSE;
	if (g_str_equal ("yes", result))
		ret = TRUE;

	g_free (result);

	return ret;
}

void
panel_end_session_dialog_request_suspend (PanelEndSessionDialog *dialog)
{
	panel_end_session_dialog_do_request (dialog, REQUEST_TYPE_SUSPEND);
}

gboolean
panel_end_session_dialog_is_hybrid_sleep_available (PanelEndSessionDialog *dialog)
{
	gchar *result;
	gboolean ret;

	g_return_val_if_fail (PANEL_IS_END_SESSION_DIALOG (dialog), FALSE);

	if (!dialog->priv->login1_proxy)
		return FALSE;

	login1_manager_call_can_hybrid_sleep_sync (dialog->priv->login1_proxy,
	                                           &result,
	                                           NULL,
	                                           NULL);

	ret = FALSE;
	if (g_str_equal ("yes", result))
		ret = TRUE;

	g_free (result);

	return ret;
}

void
panel_end_session_dialog_request_hybrid_sleep (PanelEndSessionDialog *dialog)
{
	panel_end_session_dialog_do_request (dialog, REQUEST_TYPE_HYBRID_SLEEP);
}
