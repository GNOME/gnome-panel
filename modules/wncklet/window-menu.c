/*
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 *      George Lebl <jirka@5z.com>
 *      Jacob Berkman <jacob@helixcode.com>
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <libwnck/libwnck.h>
#include <string.h>

#include "wncklet.h"
#include "window-menu.h"

#define WINDOW_MENU_ICON "gnome-panel-window-menu"

struct _WindowMenuApplet
{
	GpApplet parent;

	WnckHandle   *handle;
	GtkWidget    *selector;
	int	      size;
	GtkOrientation orient;
};

G_DEFINE_TYPE (WindowMenuApplet, window_menu_applet, GP_TYPE_APPLET)

static gboolean
window_menu_on_draw (GtkWidget *widget,
                     cairo_t   *cr,
                     gpointer   data)
{
        GtkStyleContext *context;
        GtkStateFlags    state;
        WindowMenuApplet *window_menu = data;

	if (!gtk_widget_has_focus (GTK_WIDGET (window_menu)))
                return FALSE;

        state = gtk_widget_get_state_flags (widget);
        context = gtk_widget_get_style_context (widget);
        gtk_style_context_save (context);
        gtk_style_context_set_state (context, state);

        cairo_save (cr);
        gtk_render_focus (context, cr,
                          0., 0.,
                          gtk_widget_get_allocated_width (widget),
                          gtk_widget_get_allocated_height (widget));
        cairo_restore (cr);

        gtk_style_context_restore (context);

	return FALSE;
}

static void
window_menu_size_allocate (GtkWidget        *widget,
                           GtkAllocation    *allocation,
                           WindowMenuApplet *window_menu)
{
	GtkOrientation orient;
	GList             *children;
	GtkWidget         *child;

	orient = gp_applet_get_orientation (GP_APPLET (widget));

	children = gtk_container_get_children (GTK_CONTAINER (window_menu->selector));
	child = GTK_WIDGET (children->data);
	g_list_free (children);

	if (orient == GTK_ORIENTATION_VERTICAL) {
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
window_menu_key_press_event (GtkWidget        *widget,
                             GdkEventKey      *event,
                             WindowMenuApplet *window_menu)
{
	GtkMenuShell *menu_shell;
	WnckSelector *selector;

	switch (event->keyval) {
	case GDK_KEY_KP_Enter:
	case GDK_KEY_ISO_Enter:
	case GDK_KEY_3270_Enter:
	case GDK_KEY_Return:
	case GDK_KEY_space:
	case GDK_KEY_KP_Space:
		selector = WNCK_SELECTOR(window_menu->selector);
		/* 
		 * We need to call _gtk_menu_shell_activate() here as is done in 
		 * window_key_press_handler in gtkmenubar.c which pops up menu
		 * when F10 is pressed.
		 *
		 * As that function is private its code is replicated here.
		 */
		menu_shell = GTK_MENU_SHELL (selector);
		/* FIXMEgpoo: We need either accessors or a workaround
		   to grab the focus */
#if 0
		if (!menu_shell->GSEAL(active)) {
			gtk_grab_add (GTK_WIDGET (menu_shell));
			menu_shell->GSEAL(have_grab) = TRUE;
			menu_shell->GSEAL(active) = TRUE;
		}
#endif
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

static void
window_menu_applet_fill (GpApplet *applet)
{
	WindowMenuApplet *window_menu;

	window_menu = WINDOW_MENU_APPLET (applet);

	gtk_widget_set_name (GTK_WIDGET (window_menu), "window-menu-applet-button");
	gtk_widget_set_tooltip_text (GTK_WIDGET (window_menu), _("Window Selector"));

	g_object_bind_property (applet, "enable-tooltips",
	                        window_menu, "has-tooltip",
	                        G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	window_menu->orient = gp_applet_get_orientation (applet);

	window_menu->handle = wnck_handle_new (WNCK_CLIENT_TYPE_PAGER);
	window_menu->selector = wnck_selector_new_with_handle (window_menu->handle);
	gtk_container_add (GTK_CONTAINER (window_menu),
			   window_menu->selector);

	g_signal_connect (window_menu, "key_press_event",
			  G_CALLBACK (window_menu_key_press_event), window_menu);
	g_signal_connect (window_menu, "size-allocate",
			  G_CALLBACK (window_menu_size_allocate), window_menu);

	g_signal_connect_after (window_menu, "focus-in-event",
				G_CALLBACK (gtk_widget_queue_draw), window_menu);
	g_signal_connect_after (window_menu, "focus-out-event",
				G_CALLBACK (gtk_widget_queue_draw), window_menu);
	g_signal_connect_after (G_OBJECT (window_menu->selector), "draw",
				G_CALLBACK (window_menu_on_draw), window_menu);

	g_signal_connect (G_OBJECT (window_menu->selector), "button_press_event",
			  G_CALLBACK (filter_button_press), window_menu);

	gtk_widget_show_all (GTK_WIDGET (window_menu));
}

static void
window_menu_applet_constructed (GObject *object)
{
	G_OBJECT_CLASS (window_menu_applet_parent_class)->constructed (object);

	window_menu_applet_fill (GP_APPLET (object));
}

static void
window_menu_applet_dispose (GObject *object)
{
	WindowMenuApplet *window_menu;

	window_menu = WINDOW_MENU_APPLET (object);

	g_clear_object (&window_menu->handle);

	G_OBJECT_CLASS (window_menu_applet_parent_class)->dispose (object);
}

static void
window_menu_applet_class_init (WindowMenuAppletClass *window_menu_class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (window_menu_class);

	object_class->constructed = window_menu_applet_constructed;
	object_class->dispose = window_menu_applet_dispose;
}

static void
window_menu_applet_init (WindowMenuApplet *window_menu)
{
	gp_applet_set_flags (GP_APPLET (window_menu), GP_APPLET_FLAGS_EXPAND_MINOR);
}
