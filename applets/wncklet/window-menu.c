/* window-menu.c: Window Selector applet
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2001 Free Software Foundation, Inc.
 * Copyright (C) 2000 Helix Code, Inc.
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
 *      George Lebl <jirka@5z.com>
 *      Jacob Berkman <jacob@helixcode.com>
 */

#define WNCK_I_KNOW_THIS_IS_UNSTABLE 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <panel-applet.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <libwnck/selector.h>

#include "wncklet.h"
#include "window-menu.h"

#define WINDOW_MENU_ICON "gnome-panel-window-menu"

typedef struct {
	GtkWidget    *applet;
	GtkWidget    *selector;
	GtkWidget    *about_dialog;
	int	      size;
	PanelAppletOrient orient;
} WindowMenu;

static void
window_menu_help (GtkAction  *action,
                  WindowMenu *window_menu)
{
	wncklet_display_help (window_menu->applet, "user-guide",
			      "panel-windowselector", WINDOW_MENU_ICON);
}

static void
window_menu_about (GtkAction  *action,
                   WindowMenu *window_menu)
{
	static const char *authors[] = {
		"Mark McLoughlin <mark@skynet.ie>",
		"George Lebl <jirka@5z.com>",
		"Jacob Berkman <jacob@helixcode.com>",
		NULL
	};
	const char *documenters [] = { 
                "Sun GNOME Documentation Team <gdocteam@sun.com>",
                NULL 
        };
	const char *translator_credits = _("translator-credits");

	wncklet_display_about (window_menu->applet, &window_menu->about_dialog,
			       _("Window Selector"),
			       "Copyright \xc2\xa9 2003 Sun Microsystems, Inc.\n"
			       "Copyright \xc2\xa9 2001 Free Software Foundation, Inc.\n"
			       "Copyright \xc2\xa9 2000 Helix Code, Inc.",
			       _("The Window Selector shows a list of all windows in a menu and lets you browse them."),
			       authors,
			       documenters,
			       translator_credits,
			       WINDOW_MENU_ICON,
			       "window-menu",
			       "WindowMenu");
}

static const GtkActionEntry window_menu_actions [] =
{
	{ "WindowMenuHelp", GTK_STOCK_HELP, N_("_Help"),
	  NULL, NULL,
	  G_CALLBACK (window_menu_help) },
	{ "WindowMenuAbout", GTK_STOCK_ABOUT, N_("_About"),
	  NULL, NULL,
	  G_CALLBACK (window_menu_about) }
};

static void
window_menu_destroy (GtkWidget  *widget,
                     WindowMenu *window_menu)
{
	if (window_menu->about_dialog) {
		gtk_widget_destroy (window_menu->about_dialog);
		window_menu->about_dialog = NULL;
	}

	g_free (window_menu);
}

static gboolean
window_menu_on_expose (GtkWidget *widget,
                       GdkEventExpose *event,
                       gpointer data)
{
	WindowMenu *window_menu = data;

	if (gtk_widget_has_focus (window_menu->applet))
		gtk_paint_focus (gtk_widget_get_style (widget),
				 gtk_widget_get_window (widget), 
				 gtk_widget_get_state (widget),
				 NULL,
				 widget,
				 "menu-applet",
				 0, 0, -1, -1);
	return FALSE;
}

static inline void
force_no_focus_padding (GtkWidget *widget)
{
        gboolean first_time = TRUE;

        if (first_time) {
                gtk_rc_parse_string ("\n"
                                     "   style \"window-menu-applet-button-style\"\n"
                                     "   {\n"
                                     "      GtkWidget::focus-line-width=0\n"
                                     "      GtkWidget::focus-padding=0\n"
                                     "   }\n"
                                     "\n"
                                     "    widget \"*.window-menu-applet-button\" style \"window-menu-applet-button-style\"\n"
                                     "\n");
                first_time = FALSE;
        }

        gtk_widget_set_name (widget, "window-menu-applet-button");
}

static void
window_menu_size_allocate (PanelApplet	 *applet, 
			   GtkAllocation *allocation,
			   WindowMenu	 *window_menu)
{
	PanelAppletOrient  orient;
	GList             *children;
	GtkWidget         *child;

	orient = panel_applet_get_orient (applet);

	children = gtk_container_get_children (GTK_CONTAINER (window_menu->selector));
	child = GTK_WIDGET (children->data);
	g_list_free (children);

	if (orient == PANEL_APPLET_ORIENT_LEFT || 
	    orient == PANEL_APPLET_ORIENT_RIGHT) {
		if (window_menu->size == allocation->width &&
		    orient == window_menu->orient)
			return;
		window_menu->size = allocation->width;
		gtk_widget_set_size_request (child, window_menu->size, -1);
	} else {
		if (window_menu->size == allocation->height &&
		    orient == window_menu->orient)
			return;
		window_menu->size = allocation->height;
		gtk_widget_set_size_request (child, -1, window_menu->size);
	}

	window_menu->orient = orient;
}

static gboolean
window_menu_key_press_event (GtkWidget   *widget,
                             GdkEventKey *event,
                             WindowMenu  *window_menu)
{
	GtkMenuShell *menu_shell;
	WnckSelector *selector;

	switch (event->keyval) {
	case GDK_KP_Enter:
	case GDK_ISO_Enter:
	case GDK_3270_Enter:
	case GDK_Return:
	case GDK_space:
	case GDK_KP_Space:
		selector = WNCK_SELECTOR(window_menu->selector);
		/* 
		 * We need to call _gtk_menu_shell_activate() here as is done in 
		 * window_key_press_handler in gtkmenubar.c which pops up menu
		 * when F10 is pressed.
		 *
		 * As that function is private its code is replicated here.
		 */
		menu_shell = GTK_MENU_SHELL (selector);
		if (!menu_shell->active) {
			gtk_grab_add (GTK_WIDGET (menu_shell));
			menu_shell->have_grab = TRUE;
			menu_shell->active = TRUE;
		}
		gtk_menu_shell_select_first (menu_shell, FALSE);
		return TRUE;
	default:
		break;
	}
	
	return FALSE;
}

static gboolean
filter_button_press (GtkWidget *widget,
                     GdkEventButton *event,
                     gpointer data)
{
	if (event->button != 1)
		g_signal_stop_emission_by_name (widget, "button_press_event");
	
	return FALSE;
}

gboolean
window_menu_applet_fill (PanelApplet *applet)
{
	WindowMenu *window_menu;
	GtkActionGroup *action_group;
	gchar *ui_path;

	window_menu = g_new0 (WindowMenu, 1);

	window_menu->applet = GTK_WIDGET (applet);
	force_no_focus_padding (window_menu->applet);
	gtk_widget_set_tooltip_text (window_menu->applet, _("Window Selector"));
 
	panel_applet_set_flags (applet, PANEL_APPLET_EXPAND_MINOR);
	window_menu->size = panel_applet_get_size (applet);
	window_menu->orient = panel_applet_get_orient (applet);

	g_signal_connect (window_menu->applet, "destroy",
			  G_CALLBACK (window_menu_destroy), window_menu);

	action_group = gtk_action_group_new ("WindowMenu Applet Actions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
				      window_menu_actions,
				      G_N_ELEMENTS (window_menu_actions),
				      window_menu);
	ui_path = g_build_filename (WNCK_MENU_UI_DIR, "window-menu-menu.xml", NULL);
	panel_applet_setup_menu_from_file (applet, ui_path, action_group);
	g_free (ui_path);
	g_object_unref (action_group);

	window_menu->selector = wnck_selector_new ();
	gtk_container_add (GTK_CONTAINER (window_menu->applet), 
			   window_menu->selector);

	panel_applet_set_background_widget (PANEL_APPLET (window_menu->applet),
					    GTK_WIDGET (window_menu->selector));

	g_signal_connect (window_menu->applet, "key_press_event",
			  G_CALLBACK (window_menu_key_press_event), window_menu);
	g_signal_connect (window_menu->applet, "size-allocate",
			  G_CALLBACK (window_menu_size_allocate), window_menu);

	g_signal_connect_after (G_OBJECT (window_menu->applet), "focus-in-event",
				G_CALLBACK (gtk_widget_queue_draw), window_menu);
	g_signal_connect_after (G_OBJECT (window_menu->applet), "focus-out-event",
				G_CALLBACK (gtk_widget_queue_draw), window_menu);
	g_signal_connect_after (G_OBJECT (window_menu->selector), "expose-event",
				G_CALLBACK (window_menu_on_expose), window_menu);

	g_signal_connect (G_OBJECT (window_menu->selector), "button_press_event",
			  G_CALLBACK (filter_button_press), window_menu);

	gtk_widget_show_all (GTK_WIDGET (window_menu->applet));

	return TRUE;
}
