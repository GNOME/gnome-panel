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

#include <config.h>
#include <string.h>
#include <panel-applet.h>
#include <gdk/gdkkeysyms.h>
#include <libgnomeui/gnome-help.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/selector.h>

#include "window-menu.h"

typedef struct {
	GtkWidget    *applet;
	GtkWidget    *selector;
	GtkWidget    *about_dialog;
	int	      size;
	PanelAppletOrient orient;
} WindowMenu;

static void
window_menu_help (BonoboUIComponent *uic,
                  WindowMenu        *window_menu,
                  const char        *verb) 
{
	GError *error = NULL;

	gnome_help_display_desktop_on_screen (
		NULL, "user-guide", "user-guide.xml", "gosmetacity-27",
		gtk_widget_get_screen (window_menu->applet),
		&error);

	if (error) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (
				NULL,
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
				       gtk_widget_get_screen (window_menu->applet));
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

static void
window_menu_about (BonoboUIComponent *uic,
                   WindowMenu        *window_menu,
                   const char        *verb) 
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

	if (window_menu->about_dialog) {
		gtk_window_set_screen (GTK_WINDOW (window_menu->about_dialog),
				       gtk_widget_get_screen (window_menu->applet));
		gtk_window_present (GTK_WINDOW (window_menu->about_dialog));
		return;
	}

	window_menu->about_dialog = gtk_about_dialog_new ();
	g_object_set (window_menu->about_dialog,
		      "name",  _("Window Selector"),
		      "version", VERSION,
		      "copyright", "Copyright \xc2\xa9 2003 Sun Microsystems, Inc.\n"
				   "Copyright \xc2\xa9 2001 Free Software Foundation, Inc.\n"
				   "Copyright \xc2\xa9 2000 Helix Code, Inc.",
		      "comments", _("The Window Selector shows a list of all windows and lets you browse them."),
		      "authors", authors,
		      "documenters", documenters,
		      "translator_credits", strcmp (translator_credits, "translator-credits") != 0 ? translator_credits : NULL,
		      "logo_icon_name", "panel-window-menu",
		      NULL);

	gtk_window_set_wmclass (GTK_WINDOW (window_menu->about_dialog), "window-menu", "WindowMenu");
	gtk_window_set_screen (GTK_WINDOW (window_menu->about_dialog),
			       gtk_widget_get_screen (window_menu->applet));

	gtk_window_set_icon_name (GTK_WINDOW (window_menu->about_dialog),
				  "panel-window-menu"); 

	g_signal_connect (window_menu->about_dialog, "destroy",
			  (GCallback) gtk_widget_destroyed, &window_menu->about_dialog);
	gtk_widget_show (window_menu->about_dialog);
}

static const BonoboUIVerb window_menu_verbs [] =
{   
	BONOBO_UI_UNSAFE_VERB ("WindowMenuHelp",  window_menu_help),
	BONOBO_UI_UNSAFE_VERB ("WindowMenuAbout", window_menu_about),
	BONOBO_UI_VERB_END
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

static void
window_menu_change_background (PanelApplet               *applet,
                               PanelAppletBackgroundType  type,
                               GdkColor                  *color,
                               GdkPixmap                 *pixmap,
                               WindowMenu                *window_menu)
{
	GtkRcStyle *rc_style;
	GtkStyle   *style;

	/* reset style */
	gtk_widget_set_style (GTK_WIDGET (window_menu->selector), NULL);
	rc_style = gtk_rc_style_new ();
	gtk_widget_modify_style (GTK_WIDGET (window_menu->selector), rc_style);
	g_object_unref (rc_style);

	switch (type) {
	case PANEL_NO_BACKGROUND:
		break;
	case PANEL_COLOR_BACKGROUND:
		gtk_widget_modify_bg (GTK_WIDGET (window_menu->selector),
				      GTK_STATE_NORMAL, color);
		break;
	case PANEL_PIXMAP_BACKGROUND:
		style = gtk_style_copy (GTK_WIDGET (window_menu->selector)->style);
		if (style->bg_pixmap[GTK_STATE_NORMAL])
			g_object_unref (style->bg_pixmap[GTK_STATE_NORMAL]);
		style->bg_pixmap[GTK_STATE_NORMAL] = g_object_ref (pixmap);
		gtk_widget_set_style (GTK_WIDGET (window_menu->selector), style);
		break;
	}
}


static void
set_tooltip (GtkWidget  *widget,
             const char *tip)
{
	GtkTooltips *tooltips;

	tooltips = g_object_get_data (G_OBJECT (widget), "window-menu-tooltips");
	if (!tooltips) {
		tooltips = gtk_tooltips_new ();
		g_object_ref (tooltips);
		gtk_object_sink (GTK_OBJECT (tooltips));
		g_object_set_data_full (G_OBJECT (widget), "window-menu-tooltips", tooltips,
					(GDestroyNotify) g_object_unref);
	}

	gtk_tooltips_set_tip (tooltips, widget, tip, NULL);
}

static gboolean
window_menu_on_expose (GtkWidget *widget,
                       GdkEventExpose *event,
                       gpointer data)
{
	WindowMenu *window_menu = data;

	if (GTK_WIDGET_HAS_FOCUS (window_menu->applet))
		gtk_paint_focus (widget->style,
				 widget->window, 
				 GTK_WIDGET_STATE (widget),
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
	PanelAppletOrient          orient;
	GList                     *children;
	GtkWidget                 *child;
	PanelAppletBackgroundType  bg_type;
	GdkColor                   bg_color;
	GdkPixmap                 *bg_pixmap;

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

	bg_type = panel_applet_get_background (PANEL_APPLET (window_menu->applet),
					       &bg_color, &bg_pixmap);
	window_menu_change_background (PANEL_APPLET (window_menu->applet),
				       bg_type, &bg_color, bg_pixmap,
				       window_menu);
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

static WnckScreen *
applet_get_screen (GtkWidget *applet)
{
	int screen_num;

	if (!gtk_widget_has_screen (applet))
		return wnck_screen_get_default ();

	screen_num = gdk_screen_get_number (gtk_widget_get_screen (applet));

	return wnck_screen_get (screen_num);
}

gboolean
window_menu_applet_fill (PanelApplet *applet)
{
	WindowMenu *window_menu;
	WnckScreen *screen;

	window_menu = g_new0 (WindowMenu, 1);

	window_menu->applet = GTK_WIDGET (applet);
	force_no_focus_padding (window_menu->applet);
	set_tooltip (window_menu->applet, _("Window Selector"));
 
	panel_applet_set_flags (applet, PANEL_APPLET_EXPAND_MINOR);
	window_menu->size = panel_applet_get_size (applet);
	window_menu->orient = panel_applet_get_orient (applet);

	g_signal_connect (window_menu->applet, "destroy",
			  G_CALLBACK (window_menu_destroy), window_menu);

	panel_applet_setup_menu_from_file (applet, NULL, "GNOME_WindowMenuApplet.xml",
					   NULL, window_menu_verbs, window_menu);

	screen = applet_get_screen (window_menu->applet);
	window_menu->selector = wnck_selector_new (screen);
	gtk_container_add (GTK_CONTAINER (window_menu->applet), 
			   window_menu->selector);

	g_signal_connect (G_OBJECT (window_menu->applet), "change_background",
			  G_CALLBACK (window_menu_change_background), window_menu);
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
