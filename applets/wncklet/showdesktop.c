/* -*- mode: C; c-file-style: "linux" -*- */
/* "Show desktop" panel applet */

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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include "showdesktop.h"
#include <gtk/gtkaboutdialog.h>
#include <gtk/gtkicontheme.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkdnd.h>
#include <libgnomeui/gnome-help.h>
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/screen.h>
#include "wncklet.h"
#include <gdk/gdkx.h>

#include <string.h>

#define TIMEOUT_ACTIVATE 1000


typedef struct {
        /* widgets */
        GtkWidget *applet;
        GtkWidget *button;
        GtkWidget *image;
        GtkWidget *about_dialog;

        PanelAppletOrient orient;
        int size;

        WnckScreen *wnck_screen;

        guint showing_desktop : 1;
        guint button_activate;

	GtkIconTheme *icon_theme;
} ShowDesktopData;

static void display_help_dialog  (BonoboUIComponent *uic,
                                  ShowDesktopData   *sdd,
                                  const gchar       *verbname);
static void display_about_dialog (BonoboUIComponent *uic,
                                  ShowDesktopData   *sdd,
                                  const gchar       *verbname);

static void update_icon           (ShowDesktopData *sdd);
static void update_button_state   (ShowDesktopData *sdd);
static void update_button_display (ShowDesktopData *sdd);

static void theme_changed_callback        (GtkIconTheme    *icon_theme,
					   ShowDesktopData *sdd);

static void button_toggled_callback       (GtkWidget       *button,
                                           ShowDesktopData *sdd);
static void show_desktop_changed_callback (WnckScreen      *screen,
                                           ShowDesktopData *sdd);

static void
set_tooltip (GtkWidget  *widget,
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

/* this is when the panel orientation changes */

static void
applet_change_orient (PanelApplet       *applet,
                      PanelAppletOrient  orient,
                      ShowDesktopData   *sdd)
{
        GtkOrientation new_orient;

        switch (orient)
        {
        case PANEL_APPLET_ORIENT_LEFT:
        case PANEL_APPLET_ORIENT_RIGHT:
                new_orient = GTK_ORIENTATION_VERTICAL;
                break;
        case PANEL_APPLET_ORIENT_UP:
        case PANEL_APPLET_ORIENT_DOWN:
        default:
                new_orient = GTK_ORIENTATION_HORIZONTAL;
                break;
        }

        if (new_orient == sdd->orient)
                return;

        sdd->orient = new_orient;

        update_icon (sdd);
}

static void
applet_change_background (PanelApplet               *applet,
                          PanelAppletBackgroundType  type,
                          GdkColor                  *color,
                          GdkPixmap                 *pixmap,
                          ShowDesktopData           *sdd)
{
	GtkRcStyle *rc_style;
	GtkStyle   *style;

	/* reset style */
	gtk_widget_set_style (GTK_WIDGET (sdd->applet), NULL);
	rc_style = gtk_rc_style_new ();
	gtk_widget_modify_style (GTK_WIDGET (sdd->applet), rc_style);
	g_object_unref (rc_style);

	switch (type) {
	case PANEL_NO_BACKGROUND:
		break;
	case PANEL_COLOR_BACKGROUND:
		gtk_widget_modify_bg (GTK_WIDGET (sdd->applet),
				      GTK_STATE_NORMAL, color);
		break;
	case PANEL_PIXMAP_BACKGROUND:
		style = gtk_style_copy (GTK_WIDGET (sdd->applet)->style);
		if (style->bg_pixmap[GTK_STATE_NORMAL])
			g_object_unref (style->bg_pixmap[GTK_STATE_NORMAL]);
		style->bg_pixmap[GTK_STATE_NORMAL] = g_object_ref (pixmap);
		gtk_widget_set_style (GTK_WIDGET (sdd->applet), style);
		break;
	}
}

/* this is when the panel size changes */
static void
applet_size_allocated (PanelApplet       *applet,
		       GtkAllocation     *allocation,
                       ShowDesktopData   *sdd)
{
	if (((sdd->orient == GTK_ORIENTATION_HORIZONTAL)
		&& (sdd->size == allocation->height))
	    || ((sdd->orient == GTK_ORIENTATION_VERTICAL)
	    	&& (sdd->size == allocation->width)))
	     return;

	switch (sdd->orient) {
	case GTK_ORIENTATION_HORIZONTAL:
		sdd->size = allocation->height;
		break;
	case GTK_ORIENTATION_VERTICAL:
		sdd->size = allocation->width;
		break;
	}

        update_icon (sdd);
}

static void
update_icon (ShowDesktopData *sdd)
{
        int width, height;
        GdkPixbuf *icon;
        GdkPixbuf *scaled;
        int icon_size;
	GError *error;

#define SPACE_FOR_BUTTON_BORDER 4
	icon_size = sdd->size - SPACE_FOR_BUTTON_BORDER;

	error = NULL;
	icon = gtk_icon_theme_load_icon (sdd->icon_theme,
					 "gnome-fs-desktop",
					 icon_size, 0, &error);

	if (icon == NULL) {
		g_printerr (_("Failed to load %s: %s\n"), "gnome-fs-desktop",
			    error ? error->message : _("Icon not found"));
		if (error) {
			g_error_free (error);
			error = NULL;
		}

		gtk_image_set_from_stock (GTK_IMAGE (sdd->image),
					  GTK_STOCK_MISSING_IMAGE,
					  GTK_ICON_SIZE_SMALL_TOOLBAR);
		return;
	}

        width = gdk_pixbuf_get_width (icon);
        height = gdk_pixbuf_get_height (icon);

        scaled = NULL;

        /* Make it fit on the given panel */
        switch (sdd->orient) {
        case GTK_ORIENTATION_HORIZONTAL:
                width = (icon_size * width) / height;
                height = icon_size;
                break;
        case GTK_ORIENTATION_VERTICAL:
                height = (icon_size * height) / width;
                width = icon_size;
                break;
        }

        scaled = gdk_pixbuf_scale_simple (icon,
                                          width, height,
                                          GDK_INTERP_BILINEAR);

        if (scaled != NULL) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (sdd->image),
					   scaled);
		g_object_unref (scaled);
	} else
		gtk_image_set_from_pixbuf (GTK_IMAGE (sdd->image),
					   icon);

        g_object_unref (icon);
}

static const BonoboUIVerb show_desktop_menu_verbs [] = {
        BONOBO_UI_UNSAFE_VERB ("ShowDesktopHelp",        display_help_dialog),
        BONOBO_UI_UNSAFE_VERB ("ShowDesktopAbout",       display_about_dialog),
        BONOBO_UI_VERB_END
};

/* This updates things that should be consistent with the button's appearance,
 * and update_button_state updates the button appearance itself
 */
static void
update_button_display (ShowDesktopData *sdd)
{
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sdd->button))) {
                set_tooltip (sdd->button,
                             _("Click here to restore hidden windows."));
        } else {
                set_tooltip (sdd->button,
                             _("Click here to hide all windows and show the desktop."));
        }
}

static void
update_button_state (ShowDesktopData *sdd)
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
applet_destroyed (GtkWidget       *applet,
                  ShowDesktopData *sdd)
{
	if (sdd->about_dialog) {
		gtk_widget_destroy (sdd->about_dialog);
		sdd->about_dialog =  NULL;
	}

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

        g_free (sdd);
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
	ShowDesktopData *sdd = (ShowDesktopData*) data;

	sdd->button_activate = 0;

	g_signal_emit_by_name (G_OBJECT (sdd->button), "clicked", sdd);

	return FALSE;
}

static void
button_drag_leave (GtkWidget          *widget,
		   GdkDragContext     *context,
		   guint               time,
		   ShowDesktopData    *sdd)
{
	if (sdd->button_activate != 0) {
		g_source_remove (sdd->button_activate);
		sdd->button_activate = 0;
	}
}

static gboolean
button_drag_motion (GtkWidget          *widget,
		    GdkDragContext     *context,
		    gint                x,
		    gint                y,
		    guint               time,
		    ShowDesktopData    *sdd)
{

	if (sdd->button_activate == 0)
		sdd->button_activate = g_timeout_add (TIMEOUT_ACTIVATE,
						      button_motion_timeout,
						      sdd);
	gdk_drag_status (context, 0, time);
	
	return TRUE;
}

static void 
show_desktop_applet_realized (PanelApplet *applet, 
			      gpointer     data)
{
	ShowDesktopData *sdd;
	GdkScreen       *screen;
	
	sdd = (ShowDesktopData *) data;

	if (sdd->wnck_screen != NULL)
		g_signal_handlers_disconnect_by_func (sdd->wnck_screen,
						      show_desktop_changed_callback,
						      sdd);

	if (sdd->icon_theme != NULL)
		g_signal_handlers_disconnect_by_func (sdd->icon_theme,
						      theme_changed_callback,
						      sdd);

	screen = gtk_widget_get_screen (sdd->applet);
	sdd->wnck_screen = wnck_screen_get (gdk_screen_get_number (screen));

	if (sdd->wnck_screen != NULL)
		wncklet_connect_while_alive (sdd->wnck_screen,
					     "showing_desktop_changed",
					     G_CALLBACK (show_desktop_changed_callback),
					     sdd,
					     sdd->applet);
	else
		g_warning ("Could not get WnckScreen!");

        show_desktop_changed_callback (sdd->wnck_screen, sdd);

	sdd->icon_theme = gtk_icon_theme_get_for_screen (screen);
	wncklet_connect_while_alive (sdd->icon_theme, "changed",
				     G_CALLBACK (theme_changed_callback),
				     sdd,
				     sdd->applet);

        update_icon (sdd);
}

static void
theme_changed_callback (GtkIconTheme    *icon_theme,
			ShowDesktopData *sdd)
{
	update_icon (sdd);
}

gboolean
show_desktop_applet_fill (PanelApplet *applet)
{
        ShowDesktopData *sdd;
	AtkObject       *atk_obj;

	panel_applet_set_flags (applet, PANEL_APPLET_EXPAND_MINOR);

        sdd = g_new0 (ShowDesktopData, 1);

        sdd->applet = GTK_WIDGET (applet);

	sdd->image = gtk_image_new ();

        switch (panel_applet_get_orient (applet)) {
        case PANEL_APPLET_ORIENT_LEFT:
        case PANEL_APPLET_ORIENT_RIGHT:
                sdd->orient = GTK_ORIENTATION_VERTICAL;
                break;
        case PANEL_APPLET_ORIENT_UP:
        case PANEL_APPLET_ORIENT_DOWN:
        default:
                sdd->orient = GTK_ORIENTATION_HORIZONTAL;
                break;
        }

        sdd->size = panel_applet_get_size (PANEL_APPLET (sdd->applet));

	g_signal_connect (G_OBJECT (sdd->applet), "realize",
			  G_CALLBACK (show_desktop_applet_realized), sdd);

        sdd->button = gtk_toggle_button_new ();
	atk_obj = gtk_widget_get_accessible (sdd->button);
	atk_object_set_name (atk_obj, _("Show Desktop Button"));
        g_signal_connect (G_OBJECT (sdd->button), "button_press_event",
                          G_CALLBACK (do_not_eat_button_press), NULL);

        g_signal_connect (G_OBJECT (sdd->button), "toggled",
                          G_CALLBACK (button_toggled_callback), sdd);

        gtk_container_set_border_width (GTK_CONTAINER (sdd->button), 0);
        gtk_container_add (GTK_CONTAINER (sdd->button), sdd->image);
        gtk_container_add (GTK_CONTAINER (sdd->applet), sdd->button);

        /* FIXME: Update this comment. */
        /* we have to bind change_orient before we do applet_widget_add
           since we need to get an initial change_orient signal to set our
           initial oriantation, and we get that during the _add call */
        g_signal_connect (G_OBJECT (sdd->applet),
                          "change_orient",
                          G_CALLBACK (applet_change_orient),
                          sdd);

        /* similiar to the above in semantics*/
        g_signal_connect (G_OBJECT (sdd->applet),
                          "size_allocate",
                          G_CALLBACK (applet_size_allocated),
                          sdd);

        g_signal_connect (G_OBJECT (sdd->applet),
                          "change_background",
                          G_CALLBACK (applet_change_background),
                          sdd);

        panel_applet_setup_menu_from_file (PANEL_APPLET (sdd->applet),
                                           NULL,
                                           "GNOME_ShowDesktopApplet.xml",
                                           NULL,
                                           show_desktop_menu_verbs,
                                           sdd);

        g_signal_connect (G_OBJECT (sdd->applet),
                          "destroy",
                          G_CALLBACK (applet_destroyed),
                          sdd);

	gtk_drag_dest_set (GTK_WIDGET(sdd->button), 0, NULL, 0, 0);

	g_signal_connect (G_OBJECT(sdd->button), "drag_motion",
			  G_CALLBACK (button_drag_motion),
			  sdd);
	g_signal_connect (G_OBJECT(sdd->button), "drag_leave",
			  G_CALLBACK (button_drag_leave),
			  sdd);

  	gtk_widget_show_all (sdd->applet);

        return TRUE;
}

static void
display_help_dialog (BonoboUIComponent *uic,
                     ShowDesktopData         *sdd,
                     const gchar       *verbname)
{
        GError *error = NULL;

        gnome_help_display_desktop_on_screen (
                NULL, "user-guide", "user-guide.xml", "gospanel-564",
                gtk_widget_get_screen (sdd->applet),
		&error);

        if (error) {
                GtkWidget *dialog;
                dialog = gtk_message_dialog_new (NULL,
                                                 GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_OK,
                                                 _("There was an error displaying help: %s"),
                                                 error->message);

                g_signal_connect (G_OBJECT (dialog), "response",
                                  G_CALLBACK (gtk_widget_destroy),
                                  NULL);

                gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
                gtk_window_set_screen (GTK_WINDOW (dialog),
                                       gtk_widget_get_screen (sdd->applet));
                gtk_widget_show (dialog);
                g_error_free (error);
        }
}

static void
display_about_dialog (BonoboUIComponent *uic,
                      ShowDesktopData   *sdd,
                      const gchar       *verbname)
{
        static const gchar *authors[] = {
                "Havoc Pennington <hp@redhat.com>",
                NULL
        };
        static const char *documenters[] = {
                "Sun GNOME Documentation Team <gdocteam@sun.com>",
                NULL
        };

        /* Translator credits */
        const char *translator_credits = _("translator-credits");

        if (sdd->about_dialog) {
                gtk_window_set_screen (GTK_WINDOW (sdd->about_dialog),
                                       gtk_widget_get_screen (sdd->applet));
                gtk_window_present (GTK_WINDOW (sdd->about_dialog));
                return;
        }

        sdd->about_dialog = gtk_about_dialog_new ();
        g_object_set (sdd->about_dialog,
                      "name",  _("Show Desktop Button"),
                      "version", VERSION,
                      "copyright", "Copyright \xc2\xa9 2002 Red Hat, Inc.",
                      "comments", _("This button lets you hide all windows and show the desktop."),
                      "authors", authors,
                      "documenters", documenters,
                      "translator_credits", strcmp (translator_credits, "translator-credits") != 0 ? translator_credits : NULL,
                      "logo_icon_name", "gnome-fs-desktop",
                      NULL);

        gtk_window_set_wmclass (GTK_WINDOW (sdd->about_dialog), "show-desktop", "show-desktop");
        gtk_window_set_screen (GTK_WINDOW (sdd->about_dialog),
                               gtk_widget_get_screen (sdd->applet));

	gtk_window_set_icon_name (GTK_WINDOW (sdd->about_dialog),
				  "gnome-fs-desktop"); 

        g_signal_connect (G_OBJECT(sdd->about_dialog), "destroy",
                          (GCallback)gtk_widget_destroyed, &sdd->about_dialog);

        gtk_widget_show (sdd->about_dialog);
}

static void
button_toggled_callback (GtkWidget       *button,
                         ShowDesktopData *sdd)
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
                                                 GTK_BUTTONS_OK,
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
show_desktop_changed_callback (WnckScreen      *screen,
                               ShowDesktopData *sdd)
{
        if (sdd->wnck_screen != NULL)
                sdd->showing_desktop =
                        wnck_screen_get_showing_desktop (sdd->wnck_screen);
        update_button_state (sdd);
}
