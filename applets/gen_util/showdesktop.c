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
#include <gtk/gtktogglebutton.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/gnome-about.h>
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/screen.h>
#include "egg-screen-help.h"

#include <string.h>

typedef struct {
        /* widgets */
        GtkWidget *applet;
        GtkWidget *button;
        GtkWidget *image;
        GdkPixbuf *icon;

        PanelAppletOrient orient;
        int size;

        WnckScreen *wnck_screen;

        guint showing_desktop : 1;
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
                      ShowDesktopData         *sdd)
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
                          const gchar               *pixmap,
                          ShowDesktopData                 *sdd)
{
        if (type == PANEL_NO_BACKGROUND) {
                GtkRcStyle *rc_style;

                rc_style = gtk_rc_style_new ();

                gtk_widget_modify_style (sdd->applet, rc_style);

                g_object_unref (rc_style);

        } else if (type == PANEL_COLOR_BACKGROUND)
                gtk_widget_modify_bg (sdd->applet, GTK_STATE_NORMAL, color);

        /* else if (type == PANEL_PIXMAP_BACKGROUND)
         * FIXME: Handle this when the panel support works again
         */
}


/* this is when the panel size changes */
static void
applet_change_pixel_size (PanelApplet *applet,
                          gint         size,
                          ShowDesktopData   *sdd)
{
        if (sdd->size == size)
                return;

        sdd->size = size;

        update_icon (sdd);
}

static void
update_icon (ShowDesktopData *sdd)
{
        int width, height;
        GdkPixbuf *scaled;
        double aspect;
        int icon_size;

        if (sdd->icon == NULL)
                return;

        width = gdk_pixbuf_get_width (sdd->icon);
        height = gdk_pixbuf_get_height (sdd->icon);

        aspect = (double) width / (double) height;

#define SPACE_FOR_BUTTON_BORDER 3
        icon_size = sdd->size - SPACE_FOR_BUTTON_BORDER;

        scaled = NULL;

        /* Make it fit on the given panel */
        switch (sdd->orient) {
        case GTK_ORIENTATION_HORIZONTAL:
                width = icon_size * aspect;
                height = icon_size;
                break;
        case GTK_ORIENTATION_VERTICAL:
                height = icon_size / aspect;
                width = icon_size;
                break;
        }

        scaled = gdk_pixbuf_scale_simple (sdd->icon,
                                          width, height,
                                          GDK_INTERP_BILINEAR);

        if (scaled == NULL)
                return;

        gtk_image_set_from_pixbuf (GTK_IMAGE (sdd->image),
                                   scaled);

        g_object_unref (G_OBJECT (scaled));
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
        g_object_unref (G_OBJECT (sdd->icon));

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

gboolean
fill_show_desktop_applet (PanelApplet *applet)
{
        ShowDesktopData *sdd;
        char *file;
        GError *error;

        sdd = g_new0 (ShowDesktopData, 1);

        sdd->applet = GTK_WIDGET (applet);

        file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
                                          "gnome-show-desktop.png", TRUE, NULL);
        error = NULL;
        if (file) {
                sdd->icon = gdk_pixbuf_new_from_file (file, &error);
                g_free (file);
        }

        if (sdd->icon == NULL) {
                g_printerr (_("Failed to load %s: %s\n"),
                            file, error ? error->message : _("File not found"));
                if (error)
                        g_error_free (error);
        }

        if (sdd->icon) {
                sdd->image = gtk_image_new_from_pixbuf (sdd->icon);
        } else {
                sdd->image = gtk_image_new_from_stock (GTK_STOCK_MISSING_IMAGE,
                                                       GTK_ICON_SIZE_SMALL_TOOLBAR);
        }

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

        update_icon (sdd);

        sdd->button = gtk_toggle_button_new ();

        g_signal_connect (G_OBJECT (sdd->button), "button_press_event",
                          G_CALLBACK (do_not_eat_button_press), NULL);

        g_signal_connect (G_OBJECT (sdd->button), "toggled",
                          G_CALLBACK (button_toggled_callback), sdd);

        gtk_container_set_border_width (GTK_CONTAINER (sdd->button), 1);
        gtk_container_add (GTK_CONTAINER (sdd->button), sdd->image);
        gtk_container_add (GTK_CONTAINER (sdd->applet), sdd->button);

        update_button_state (sdd);

        sdd->wnck_screen =
                wnck_screen_get (gdk_screen_get_number (gtk_widget_get_screen (sdd->applet)));

        if (sdd->wnck_screen != NULL)
                g_signal_connect (G_OBJECT (sdd->wnck_screen),
                                  "showing_desktop_changed",
                                  G_CALLBACK (show_desktop_changed_callback),
                                  sdd);
        else
                g_warning ("Could not get WnckScreen!");

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
                          "change_size",
                          G_CALLBACK (applet_change_pixel_size),
                          sdd);

        /* FIXME: initial background, this needs some panel-applet voodoo */
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

        gtk_widget_show_all (sdd->applet);

        return TRUE;
}

static void
display_help_dialog (BonoboUIComponent *uic,
                     ShowDesktopData         *sdd,
                     const gchar       *verbname)
{
        GError *error = NULL;
        static GnomeProgram *applet_program = NULL;

        if (!applet_program) {
                /* FIXME there is no way this crack is right; it's some junk
                 * copied from clock.c
                 */
                int argc = 1;
                char *argv[2] = { "show-desktop" };
                applet_program = gnome_program_init ("show-desktop", VERSION,
                                                     LIBGNOME_MODULE, argc, argv,
                                                     GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);
        }

        egg_help_display_desktop_on_screen (
                applet_program, "show-desktop", "show-desktop", NULL,
                gtk_widget_get_screen (sdd->applet),
		&error);

        if (error) {
                GtkWidget *dialog;
                dialog = gtk_message_dialog_new (NULL,
                                                 GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_CLOSE,
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
        static GtkWidget *about = NULL;

        static const gchar *authors[] = {
                "Havoc Pennington <hp@redhat.com>",
                NULL
        };
        static const char *documenters[] = {
                NULL
        };

        /* Translator credits */
        const char *translator_credits = _("translator_credits");

        if (about) {
                gtk_window_set_screen (GTK_WINDOW (about),
                                       gtk_widget_get_screen (sdd->applet));
                gtk_window_present (GTK_WINDOW (about));
                return;
        }

        about = gnome_about_new (_("Show Desktop Button"), VERSION,
                                 "Copyright \xc2\xa9 2002 Red Hat, Inc.",
                                 _("This button lets you hide all windows and show the desktop"),
                                 authors,
                                 documenters,
                                 strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
                                 sdd->icon);

        gtk_window_set_wmclass (GTK_WINDOW (about), "show-desktop", "show-desktop");
        gtk_window_set_screen (GTK_WINDOW (about),
                               gtk_widget_get_screen (sdd->applet));

        if (sdd->icon)
                gtk_window_set_icon (GTK_WINDOW (about), sdd->icon);

        g_signal_connect (G_OBJECT(about), "destroy",
                          (GCallback)gtk_widget_destroyed, &about);

        gtk_widget_show (about);
}

static void
button_toggled_callback (GtkWidget       *button,
                         ShowDesktopData *sdd)
{
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
