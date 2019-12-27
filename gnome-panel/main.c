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

#include <glib/gi18n.h>
#include <glib-unix.h>
#include <gtk/gtkx.h>

#include <libpanel-util/panel-cleanup.h>
#include <libpanel-util/panel-glib.h>

#include "panel-toplevel.h"
#include "panel-multiscreen.h"
#include "panel-session.h"
#include "panel-action-protocol.h"
#include "panel-icon-names.h"
#include "panel-layout.h"
#include "panel-schemas.h"

static gboolean  replace = FALSE;
static gboolean  version = FALSE;

static GSettings *g_settings = NULL;
static GtkSettings *gtk_settings = NULL;
static GtkStyleProvider *provider = NULL;

static const GOptionEntry options[] = {
  { "replace", 0, 0, G_OPTION_ARG_NONE, &replace, N_("Replace a currently running panel"), NULL },
  { "version", 0, 0, G_OPTION_ARG_NONE, &version, N_("Print version"), NULL},
  { NULL }
};

typedef struct
{
  const char *name;
  const char *dir;
  const char *variant;
  gboolean    has_dark_variant;
} GpSupportedTheme;

static GpSupportedTheme supported_themes[] =
{
  { "Adwaita", "Adwaita", NULL, TRUE },
  { "Adwaita-dark", "Adwaita", "dark", FALSE },
  { "HighContrast", "HighContrast", NULL, FALSE },
  { "HighContrastInverse", "HighContrast", "inverse", FALSE },
  { NULL, NULL, FALSE, FALSE }
};

static char *
get_theme_resource (GpSupportedTheme *theme,
                    gboolean          prefer_dark)
{
  char *filename;
  const char *resource_base;
  char *resource;

  if (theme->variant != NULL)
    filename = g_strdup_printf ("gnome-panel-%s.css", theme->variant);
  else if (theme->has_dark_variant && prefer_dark)
    filename = g_strdup ("gnome-panel-dark.css");
  else
    filename = g_strdup ("gnome-panel.css");

  resource_base = "/org/gnome/gnome-panel/theme";
  resource = g_strdup_printf ("%s/%s/%s", resource_base, theme->dir, filename);
  g_free (filename);

  return resource;
}

static gboolean
is_theme_supported (const char        *theme_name,
                    GpSupportedTheme **theme)
{
  int i;

  for (i = 0; supported_themes[i].name != NULL; i++)
    {
      if (g_strcmp0 (supported_themes[i].name, theme_name) == 0)
        {
          *theme = &supported_themes[i];
          return TRUE;
        }
    }

  return FALSE;
}

static void
theme_changed (GtkSettings *settings,
               GParamSpec  *pspec,
               gpointer     user_data)
{
  GdkScreen *screen;
  gchar *theme_name;
  gboolean dark_theme;
  GpSupportedTheme *theme;
  guint priority;
  gchar *resource;
  GtkCssProvider *css;

  screen = gdk_screen_get_default ();

  if (provider != NULL)
    {
      gtk_style_context_remove_provider_for_screen (screen, provider);
      g_clear_object (&provider);
    }

  g_object_get (settings,
                "gtk-theme-name", &theme_name,
                "gtk-application-prefer-dark-theme", &dark_theme,
                NULL);

  if (is_theme_supported (theme_name, &theme))
    {
      resource = get_theme_resource (theme, dark_theme);
      priority = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION;
    }
  else
    {
      resource = g_strdup ("/org/gnome/gnome-panel/theme/fallback.css");
      priority = GTK_STYLE_PROVIDER_PRIORITY_FALLBACK;
    }

  css = gtk_css_provider_new ();
  provider = GTK_STYLE_PROVIDER (css);

  gtk_css_provider_load_from_resource (css, resource);
  gtk_style_context_add_provider_for_screen (screen, provider, priority);

  g_free (theme_name);
  g_free (resource);
}

static void
theme_variant_changed_cb (GSettings   *settings,
                          const gchar *key,
                          gpointer     user_data)
{
  PanelThemeVariant variant;

  variant = g_settings_get_enum (settings, key);

  if (variant == PANEL_THEME_VARIANT_SYSTEM)
    {
      gtk_settings_reset_property (gtk_settings,
                                   "gtk-application-prefer-dark-theme");
    }
  else
    {
      g_object_set (gtk_settings, "gtk-application-prefer-dark-theme",
                    variant == PANEL_THEME_VARIANT_DARK, NULL);
    }

  theme_changed (gtk_settings, NULL, NULL);
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
	GdkDisplay     *display;
	GError         *error;
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

	/* FIXME: High dpi scaling does not work... */
	display = gdk_display_get_default ();
	gdk_x11_display_set_window_scale (display, 1);

	g_unix_signal_add (SIGTERM, on_term_signal, NULL);
	g_unix_signal_add (SIGINT, on_int_signal, NULL);

	error = NULL;
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		g_option_context_free (context);

		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	if (version) {
		printf("%s\n", PACKAGE " " VERSION);
		return EXIT_SUCCESS;
	}

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

	/* Flush to make sure our struts are seen by everyone starting
	 * immediate after (eg, the nautilus desktop). */
	gdk_display_flush (display);

	/* Do this at the end, to be sure that we're really ready when
	 * connecting to the session manager */
	panel_session_register_client (session);

	g_settings = g_settings_new (PANEL_GENERAL_SCHEMA);
	g_signal_connect (g_settings, "changed::" PANEL_GENERAL_THEME_VARIANT_KEY,
	                  G_CALLBACK (theme_variant_changed_cb), NULL);

	gtk_settings = gtk_settings_get_default ();
	g_signal_connect (gtk_settings, "notify::gtk-theme-name",
	                  G_CALLBACK (theme_changed), NULL);
	g_signal_connect (gtk_settings, "notify::gtk-application-prefer-dark-theme",
	                  G_CALLBACK (theme_changed), NULL);

	theme_variant_changed_cb (g_settings, PANEL_GENERAL_THEME_VARIANT_KEY, NULL);

	gtk_main ();

	g_object_unref (g_settings);
	g_object_unref (session);

	toplevels_to_destroy = g_slist_copy (panel_toplevel_list_toplevels ());
	for (l = toplevels_to_destroy; l; l = l->next)
	gtk_widget_destroy (l->data);
	g_slist_free (toplevels_to_destroy);

	panel_cleanup_do ();

	return 0;
}
