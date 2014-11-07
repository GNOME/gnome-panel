/* Gnome panel: Initialization routines
 * (C) 1997,1998,1999,2000 the Free Software Foundation
 * (C) 2000 Eazel, Inc.
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 *          George Lebl
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#include <glib/gi18n.h>
#include <glib-unix.h>

#include <libpanel-util/panel-cleanup.h>
#include <libpanel-util/panel-glib.h>

#include "panel-toplevel.h"
#include "panel-multiscreen.h"
#include "panel-session.h"
#include "panel-stock-icons.h"
#include "panel-action-protocol.h"
#include "panel-icon-names.h"
#include "panel-layout.h"
#include "xstuff.h"

#include "nothing.cP"

/* globals */
GSList *panels = NULL;
GSList *panel_list = NULL;

static gboolean  replace = FALSE;
static GtkCssProvider *provider = NULL;

static const GOptionEntry options[] = {
  { "replace", 0, 0, G_OPTION_ARG_NONE, &replace, N_("Replace a currently running panel"), NULL },
  { NULL }
};

static void
theme_changed (GtkSettings *settings)
{
	GdkScreen *screen;
	gchar     *theme;

	screen = gdk_screen_get_default ();
	g_object_get (settings, "gtk-theme-name", &theme, NULL);

	if (g_str_equal (theme, "Adwaita") || g_str_equal (theme, "HighContrast")) {
		if (provider == NULL) {
			GFile *file;

			file = g_file_new_for_uri ("resource:///org/gnome/panel/gnome-panel.css");
			provider = gtk_css_provider_new ();

			gtk_css_provider_load_from_file (provider, file, NULL);

			g_object_unref (file);
		}

		gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider),
		                                           GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	} else if (provider != NULL) {
		gtk_style_context_remove_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider));
		g_object_unref (provider);
		provider = NULL;
	}

	g_free (theme);
}

static gboolean
on_term_signal (gpointer user_data)
{
	gtk_main_quit ();
	return FALSE;
}

static gboolean
on_int_signal (gpointer user_data)
{
	gtk_main_quit ();
	return FALSE;
}

int
main (int argc, char **argv)
{
	GOptionContext *context;
	GError         *error;
	GtkSettings    *settings;
	PanelSession   *session;
	GSList         *toplevels_to_destroy;
	GSList         *l;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_prgname ("gnome-panel");

	context = g_option_context_new ("");
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	g_unix_signal_add (SIGTERM, on_term_signal, NULL);
	g_unix_signal_add (SIGINT, on_int_signal, NULL);

	error = NULL;
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		g_option_context_free (context);

		return 1;
	}

	g_option_context_free (context);

	session = panel_session_new (replace);
	if (session == NULL)
		return 1;

	g_set_application_name (_("Panel"));
	gtk_window_set_default_icon_name (PANEL_ICON_PANEL);

	panel_action_protocol_init ();
	panel_multiscreen_init ();

	if (!panel_layout_load ()) {
		panel_cleanup_do ();
		return 1;
	}

	xstuff_init ();

	/* Flush to make sure our struts are seen by everyone starting
	 * immediate after (eg, the nautilus desktop). */
	gdk_flush ();

	/* Do this at the end, to be sure that we're really ready when
	 * connecting to the session manager */
	panel_session_register_client (session);

	settings = gtk_settings_get_default ();
	g_signal_connect (settings, "notify::gtk-theme-name", G_CALLBACK (theme_changed), NULL);
	theme_changed (settings);

	gtk_main ();

	g_object_unref (session);

	toplevels_to_destroy = g_slist_copy (panel_toplevel_list_toplevels ());
	for (l = toplevels_to_destroy; l; l = l->next)
	gtk_widget_destroy (l->data);
	g_slist_free (toplevels_to_destroy);

	panel_cleanup_do ();

	return 0;
}
