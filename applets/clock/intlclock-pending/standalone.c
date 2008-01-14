#include <config.h>

#include <gtk/gtk.h>
#include <panel-applet.h>

#include "intlclock-applet.h"

static void
on_destroy (GtkWidget *widget, gpointer data)
{
        g_object_unref (G_OBJECT (data));
        gtk_main_quit ();
}

int main (int argc, char **argv)
{
        GnomeProgram *program;
        GOptionContext *context;
        GtkWidget *window;
        GtkWidget *applet;

        context = g_option_context_new ("");
        program = gnome_program_init (PACKAGE, VERSION,
                                      LIBGNOMEUI_MODULE,
                                      argc, argv, GNOME_PARAM_NONE);
        applet = panel_applet_new ();

        if (!intlclock_applet_init (PANEL_APPLET (applet), "OAFIID:IntlClockApplet", NULL)) {
                return 1;
        }

        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title (GTK_WINDOW (window), "Standalone International Clock");

        g_signal_connect (G_OBJECT (window), "destroy",
                          G_CALLBACK (on_destroy), applet);

        gtk_widget_reparent (applet, window);

        gtk_widget_show_all (window);

        gtk_main ();

        return 0;
}
