/* wncklet.c: A collection of window navigation applets
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <panel-applet.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libwnck/screen.h>
#include <libwnck/util.h>

#include "wncklet.h"
#include "window-menu.h"
#include "workspace-switcher.h"
#include "window-list.h"
#include "showdesktop.h"

void
wncklet_display_about (GtkWidget   *applet,
		       GtkWidget  **dialog,
		       const char  *name,
		       const char  *copyright,
		       const char  *comments,
		       const char **authors,
		       const char **documenters,
		       const char  *translator_credits,
		       const char  *icon_name,
		       const char  *wmclass_name,
		       const char  *wmclass_class)
{
	if (*dialog) {
		gtk_window_set_screen (GTK_WINDOW (*dialog),
				       gtk_widget_get_screen (applet));
		gtk_window_present (GTK_WINDOW (*dialog));
		return;
	}

	*dialog = gtk_about_dialog_new ();
	g_object_set (*dialog,
		      "program-name",  name,
		      "version", VERSION,
		      "copyright", copyright,
		      "comments", comments,
		      "authors", authors,
		      "documenters", documenters,
		      "translator-credits", translator_credits,
		      "logo-icon-name", icon_name,
		      NULL);

	gtk_window_set_wmclass (GTK_WINDOW (*dialog),
				wmclass_name, wmclass_class);
	gtk_window_set_screen (GTK_WINDOW (*dialog),
			       gtk_widget_get_screen (applet));

	gtk_window_set_icon_name (GTK_WINDOW (*dialog), icon_name); 
	g_signal_connect (*dialog, "destroy",
			  (GCallback) gtk_widget_destroyed, dialog);
	
	g_signal_connect (*dialog, "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	gtk_widget_show (*dialog);
}

void
wncklet_display_help (GtkWidget  *widget,
		      const char *doc_id,
		      const char *link_id,
		      const char *icon_name)
{
	GError *error = NULL;
	char   *uri;

	if (link_id)
		uri = g_strdup_printf ("ghelp:%s?%s", doc_id, link_id);
	else
		uri = g_strdup_printf ("ghelp:%s", doc_id);

	gtk_show_uri (gtk_widget_get_screen (widget), uri,
		      gtk_get_current_event_time (), &error);

	g_free (uri);

	if (error &&
	    g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		g_error_free (error);
	else if (error) {
		GtkWidget *parent;
		GtkWidget *dialog;
		char      *primary;

		if (GTK_IS_WINDOW (widget))
			parent = widget;
		else
			parent = NULL;

		primary = g_markup_printf_escaped (
				_("Could not display help document '%s'"),
				doc_id);
		dialog = gtk_message_dialog_new (
				parent ? GTK_WINDOW (parent) : NULL,
				GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				"%s", primary);

		gtk_message_dialog_format_secondary_text (
					GTK_MESSAGE_DIALOG (dialog),
					"%s", error->message);

		g_error_free (error);
		g_free (primary);

		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_widget_get_screen (widget));

		if (parent == NULL) {
			/* we have no parent window */
			gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog),
							  FALSE);
			gtk_window_set_title (GTK_WINDOW (dialog),
					      _("Error displaying help document"));
		}

		gtk_widget_show (dialog);
	}
}

WnckScreen *
wncklet_get_screen (GtkWidget *applet)
{
	int screen_num;

	if (!gtk_widget_has_screen (applet))
		return wnck_screen_get_default ();

	screen_num = gdk_screen_get_number (gtk_widget_get_screen (applet));

	return wnck_screen_get (screen_num);
}

void
wncklet_connect_while_alive (gpointer    object,
			     const char *signal,
			     GCallback   func,
			     gpointer    func_data,
			     gpointer    alive_object)
{
	GClosure *closure;

	closure = g_cclosure_new (func, func_data, NULL);
	g_object_watch_closure (G_OBJECT (alive_object), closure);
	g_signal_connect_closure_by_id (
			object	,
			g_signal_lookup (signal, G_OBJECT_TYPE (object)), 0,
			closure,
			FALSE);
}

static gboolean 
wncklet_factory (PanelApplet *applet,
		 const char  *iid,
		 gpointer     data)
{
	gboolean retval = FALSE;
	static gboolean type_registered = FALSE;

	if (!type_registered) {
		wnck_set_client_type (WNCK_CLIENT_TYPE_PAGER);
		type_registered = TRUE;
	}

	if (!strcmp (iid, "WindowMenuApplet"))
		retval = window_menu_applet_fill (applet);

	else if (!strcmp (iid, "WorkspaceSwitcherApplet")||
	         !strcmp (iid, "PagerApplet"))
		retval = workspace_switcher_applet_fill (applet);

	else if (!strcmp (iid, "WindowListApplet") ||
	         !strcmp (iid, "TasklistApplet"))
		retval = window_list_applet_fill (applet);

	else if (!strcmp (iid, "ShowDesktopApplet"))
		retval = show_desktop_applet_fill (applet);

	return retval;
}


#ifdef WNCKLET_INPROCESS
PANEL_APPLET_IN_PROCESS_FACTORY ("WnckletFactory",
				 PANEL_TYPE_APPLET,
				 "WindowNavigationApplets",
				 wncklet_factory,
				 NULL)
#else
PANEL_APPLET_OUT_PROCESS_FACTORY ("WnckletFactory",
				  PANEL_TYPE_APPLET,
				  "WindowNavigationApplets",
				  wncklet_factory,
				  NULL)
#endif
