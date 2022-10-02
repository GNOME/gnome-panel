/*
 * Copyright (C) 2002 Red Hat, Inc.
 * Developed by Havoc Pennington
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
 */

#include "config.h"

#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libwnck/libwnck.h>
#include <string.h>

#include "wncklet.h"
#include "showdesktop.h"

#define TIMEOUT_ACTIVATE_SECONDS 1
#define SHOW_DESKTOP_ICON "user-desktop"

struct _ShowDesktopApplet
{
        GpApplet parent;

        WnckHandle *handle;

        GtkWidget *button;
        GtkWidget *image;

        GtkOrientation orient;
        int size;

        WnckScreen *wnck_screen;

        guint showing_desktop : 1;
        guint button_activate;

        GtkIconTheme *icon_theme;
};

G_DEFINE_TYPE (ShowDesktopApplet, show_desktop_applet, GP_TYPE_APPLET)

static void
update_icon (ShowDesktopApplet *sdd)
{
        GdkPixbuf *icon;
        int        icon_size;
	GError    *error;

	if (!sdd->icon_theme)
		return;

	icon_size = gp_applet_get_panel_icon_size (GP_APPLET (sdd));

	error = NULL;
	icon = gtk_icon_theme_load_icon (sdd->icon_theme,
					 SHOW_DESKTOP_ICON,
					 icon_size, 0, &error);

	if (icon == NULL) {
		g_printerr (_("Failed to load %s: %s\n"), SHOW_DESKTOP_ICON,
			    error ? error->message : _("Icon not found"));
		if (error) {
			g_error_free (error);
			error = NULL;
		}

		gtk_image_set_from_icon_name (GTK_IMAGE (sdd->image), "image-missing", GTK_ICON_SIZE_SMALL_TOOLBAR);
		return;
	}

        gtk_image_set_from_pixbuf (GTK_IMAGE (sdd->image), icon);
        g_object_unref (icon);
}

static void
panel_icon_size_cb (GpApplet          *applet,
                    GParamSpec        *pspec,
                    ShowDesktopApplet *sdd)
{
  update_icon (sdd);
}

/* This updates things that should be consistent with the button's appearance,
 * and update_button_state updates the button appearance itself
 */
static void
update_button_display (ShowDesktopApplet *sdd)
{
	const char *tip;

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sdd->button))) {
                tip = _("Click here to restore hidden windows.");
        } else {
                tip = _("Click here to hide all windows and show the desktop.");
        }

	gtk_widget_set_tooltip_text (sdd->button, tip);

	g_object_bind_property (sdd, "enable-tooltips",
	                        sdd->button, "has-tooltip",
	                        G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
}

static void
button_toggled_callback (GtkWidget         *button,
                         ShowDesktopApplet *sdd)
{
        if (!gdk_x11_screen_supports_net_wm_hint (gtk_widget_get_screen (button),
                                                  gdk_atom_intern ("_NET_SHOWING_DESKTOP", FALSE))) {
                static GtkWidget *dialog = NULL;

                if (dialog &&
                    gtk_widget_get_screen (dialog) != gtk_widget_get_screen (button))
                        gtk_widget_destroy (dialog);

                if (dialog) {
                        gtk_window_present (GTK_WINDOW (dialog));
                        return;
                }

                dialog = gtk_message_dialog_new (NULL,
                                                 GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_CLOSE,
                                                 _("Your window manager does not support the show desktop button, or you are not running a window manager."));

                g_object_add_weak_pointer (G_OBJECT (dialog),
                                           (gpointer) &dialog);

                g_signal_connect (G_OBJECT (dialog), "response",
                                  G_CALLBACK (gtk_widget_destroy),
                                  NULL);

                gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
                gtk_window_set_screen (GTK_WINDOW (dialog),
                                       gtk_widget_get_screen (button));
                gtk_widget_show (dialog);

                return;
        }

        if (sdd->wnck_screen != NULL)
                wnck_screen_toggle_showing_desktop (sdd->wnck_screen,
                                                    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));

        update_button_display (sdd);
}

static void
update_button_state (ShowDesktopApplet *sdd)
{
        if (sdd->showing_desktop) {
                g_signal_handlers_block_by_func (G_OBJECT (sdd->button),
                                                 G_CALLBACK (button_toggled_callback),
                                                 sdd);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sdd->button),
                                              TRUE);
                g_signal_handlers_unblock_by_func (G_OBJECT (sdd->button),
                                                   G_CALLBACK (button_toggled_callback),
                                                   sdd);
        } else {
                g_signal_handlers_block_by_func (G_OBJECT (sdd->button),
                                                 G_CALLBACK (button_toggled_callback),
                                                 sdd);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sdd->button),
                                              FALSE);
                g_signal_handlers_unblock_by_func (G_OBJECT (sdd->button),
                                                   G_CALLBACK (button_toggled_callback),
                                                   sdd);
        }

        update_button_display (sdd);
}

static void
show_desktop_changed_callback (WnckScreen        *screen,
                               ShowDesktopApplet *sdd)
{
        if (sdd->wnck_screen != NULL)
                sdd->showing_desktop =
                        wnck_screen_get_showing_desktop (sdd->wnck_screen);
        update_button_state (sdd);
}

static void
theme_changed_callback (GtkIconTheme      *icon_theme,
                        ShowDesktopApplet *sdd)
{
	update_icon (sdd);
}

static gboolean
do_not_eat_button_press (GtkWidget      *widget,
                         GdkEventButton *event)
{
        if (event->button != 1) {
                g_signal_stop_emission_by_name (widget, "button_press_event");
        }

        return FALSE;
}

static gboolean
button_motion_timeout (gpointer data)
{
	ShowDesktopApplet *sdd = (ShowDesktopApplet*) data;

	sdd->button_activate = 0;

	g_signal_emit_by_name (G_OBJECT (sdd->button), "clicked", sdd);

	return FALSE;
}

static void
button_drag_leave (GtkWidget         *widget,
                   GdkDragContext    *context,
                   guint              time,
                   ShowDesktopApplet *sdd)
{
	if (sdd->button_activate != 0) {
		g_source_remove (sdd->button_activate);
		sdd->button_activate = 0;
	}
}

static gboolean
button_drag_motion (GtkWidget         *widget,
                    GdkDragContext    *context,
                    gint               x,
                    gint               y,
                    guint              time,
                    ShowDesktopApplet *sdd)
{

	if (sdd->button_activate == 0)
		sdd->button_activate = g_timeout_add_seconds (TIMEOUT_ACTIVATE_SECONDS,
							      button_motion_timeout,
							      sdd);
	gdk_drag_status (context, 0, time);
	
	return TRUE;
}

static void 
show_desktop_applet_realized (GtkWidget         *widget,
                              ShowDesktopApplet *sdd)
{
	if (sdd->wnck_screen != NULL)
		g_signal_handlers_disconnect_by_func (sdd->wnck_screen,
						      show_desktop_changed_callback,
						      sdd);

	if (sdd->icon_theme != NULL)
		g_signal_handlers_disconnect_by_func (sdd->icon_theme,
						      theme_changed_callback,
						      sdd);

	sdd->wnck_screen = wnck_handle_get_default_screen (sdd->handle);

	if (sdd->wnck_screen != NULL)
		wncklet_connect_while_alive (sdd->wnck_screen,
					     "showing_desktop_changed",
					     G_CALLBACK (show_desktop_changed_callback),
					     sdd, sdd);
	else
		g_warning ("Could not get WnckScreen!");

        show_desktop_changed_callback (sdd->wnck_screen, sdd);

	sdd->icon_theme = gtk_icon_theme_get_default ();
	wncklet_connect_while_alive (sdd->icon_theme, "changed",
				     G_CALLBACK (theme_changed_callback),
				     sdd, sdd);

        update_icon (sdd);
}

static void
show_desktop_applet_fill (GpApplet *applet)
{
	ShowDesktopApplet *sdd;
	AtkObject       *atk_obj;

	sdd = SHOW_DESKTOP_APPLET (applet);

	sdd->handle = wnck_handle_new (WNCK_CLIENT_TYPE_PAGER);

	sdd->image = gtk_image_new ();
	sdd->orient = gp_applet_get_orientation (applet);

	g_signal_connect (sdd, "realize",
			  G_CALLBACK (show_desktop_applet_realized), sdd);

        sdd->button = gtk_toggle_button_new ();

	gtk_widget_set_name (sdd->button, "showdesktop-button");

	atk_obj = gtk_widget_get_accessible (sdd->button);
	atk_object_set_name (atk_obj, _("Show Desktop Button"));
        g_signal_connect (G_OBJECT (sdd->button), "button_press_event",
                          G_CALLBACK (do_not_eat_button_press), NULL);

        g_signal_connect (G_OBJECT (sdd->button), "toggled",
                          G_CALLBACK (button_toggled_callback), sdd);

        gtk_container_set_border_width (GTK_CONTAINER (sdd->button), 0);
        gtk_container_add (GTK_CONTAINER (sdd->button), sdd->image);
        gtk_container_add (GTK_CONTAINER (sdd), sdd->button);

        g_signal_connect (applet,
                          "notify::panel-icon-size",
                          G_CALLBACK (panel_icon_size_cb),
                          sdd);

	gtk_drag_dest_set (GTK_WIDGET(sdd->button), 0, NULL, 0, 0);

	g_signal_connect (G_OBJECT(sdd->button), "drag_motion",
			  G_CALLBACK (button_drag_motion),
			  sdd);
	g_signal_connect (G_OBJECT(sdd->button), "drag_leave",
			  G_CALLBACK (button_drag_leave),
			  sdd);

	gtk_widget_show_all (GTK_WIDGET (sdd));
}

static void
show_desktop_applet_constructed (GObject *object)
{
	G_OBJECT_CLASS (show_desktop_applet_parent_class)->constructed (object);

	show_desktop_applet_fill (GP_APPLET (object));
}

static void
show_desktop_applet_dispose (GObject *object)
{
	ShowDesktopApplet *sdd;

	sdd = SHOW_DESKTOP_APPLET (object);

	if (sdd->button_activate != 0) {
		g_source_remove (sdd->button_activate);
		sdd->button_activate = 0;
	}

	if (sdd->wnck_screen != NULL) {
		g_signal_handlers_disconnect_by_func (sdd->wnck_screen,
		                                      show_desktop_changed_callback,
		                                      sdd);
		sdd->wnck_screen = NULL;
	}

	if (sdd->icon_theme != NULL) {
		g_signal_handlers_disconnect_by_func (sdd->icon_theme,
		                                      theme_changed_callback,
		                                      sdd);
		sdd->icon_theme = NULL;
	}

	g_clear_object (&sdd->handle);

	G_OBJECT_CLASS (show_desktop_applet_parent_class)->dispose (object);
}

static void
show_desktop_applet_placement_changed (GpApplet        *applet,
                                       GtkOrientation   orientation,
                                       GtkPositionType  position)
{
	ShowDesktopApplet *sdd;

	sdd = SHOW_DESKTOP_APPLET (applet);

	if (orientation == sdd->orient)
		return;

	sdd->orient = orientation;

	update_icon (sdd);
}

static void
show_desktop_applet_class_init (ShowDesktopAppletClass *sdd_class)
{
	GObjectClass *object_class;
	GpAppletClass *applet_class;

	object_class = G_OBJECT_CLASS (sdd_class);
	applet_class = GP_APPLET_CLASS (sdd_class);

	object_class->constructed = show_desktop_applet_constructed;
	object_class->dispose = show_desktop_applet_dispose;

	applet_class->placement_changed = show_desktop_applet_placement_changed;
}

static void
show_desktop_applet_init (ShowDesktopApplet *sdd)
{
	gp_applet_set_flags (GP_APPLET (sdd), GP_APPLET_FLAGS_EXPAND_MINOR);
}
