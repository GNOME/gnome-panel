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
#include <libgnomeui/gnome-help.h>
#include <gtk/gtkaboutdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtktooltips.h>
#include <libwnck/screen.h>
#include <libwnck/util.h>

#include "wncklet.h"
#include "window-menu.h"
#include "workspace-switcher.h"
#include "window-list.h"
#include "showdesktop.h"

void
wncklet_set_tooltip (GtkWidget  *widget,
		     const char *tip)
{
        GtkTooltips *tooltips;

        tooltips = g_object_get_data (G_OBJECT (widget), "tooltips");
        if (!tooltips) {
                tooltips = gtk_tooltips_new ();
                g_object_ref (tooltips);
                gtk_object_sink (GTK_OBJECT (tooltips));
                g_object_set_data_full (G_OBJECT (widget), "tooltips", tooltips,
                                        (GDestroyNotify) g_object_unref);
        }

        gtk_tooltips_set_tip (tooltips, widget, tip, NULL);
}

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
		      "name",  name,
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
		      const char *filename,
		      const char *link_id)
{
	GError *error = NULL;

	gnome_help_display_desktop_on_screen (NULL, doc_id, filename, link_id,
					      gtk_widget_get_screen (widget),
					      &error);

	if (error) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL,
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("There was an error displaying help: %s"),
						 error->message);

		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_widget_get_screen (widget));
		gtk_widget_show (dialog);
		g_error_free (error);
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

	if (!strcmp (iid, "OAFIID:GNOME_WindowMenuApplet"))
		retval = window_menu_applet_fill (applet);

	else if (!strcmp (iid, "OAFIID:GNOME_WorkspaceSwitcherApplet")||
	         !strcmp (iid, "OAFIID:GNOME_PagerApplet"))
		retval = workspace_switcher_applet_fill (applet);

	else if (!strcmp (iid, "OAFIID:GNOME_WindowListApplet") ||
	         !strcmp (iid, "OAFIID:GNOME_TasklistApplet"))
		retval = window_list_applet_fill (applet);

	else if (!strcmp (iid, "OAFIID:GNOME_ShowDesktopApplet"))
		retval = show_desktop_applet_fill (applet);

	return retval;
}


#ifdef WNCKLET_INPROCESS
PANEL_APPLET_BONOBO_SHLIB_FACTORY ("OAFIID:GNOME_Wncklet_Factory",
				   PANEL_TYPE_APPLET,
				   "WindowNavigationApplets",
				   wncklet_factory,
				   NULL);
#else
PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_Wncklet_Factory",
                             PANEL_TYPE_APPLET,
                             "WindowNavigationApplets",
                             "0",
                             wncklet_factory,
                             NULL);
#endif
