/*
 * panel-test-applets.c:
 *
 * Authors:
 *    Mark McLoughlin <mark@skynet.ie>
 *
 * Copyright 2002 Sun Microsystems, Inc.
 */

#include <config.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-widget.h>
#include <gconf/gconf.h>

#include "panel-applet.h"

void on_ok_button_clicked (GtkButton *button, gpointer dummy);

static GtkWidget *applet_options = NULL;
static GtkWidget *prefs_dir_entry = NULL;
static GtkWidget *orient_options = NULL;
static GtkWidget *size_options = NULL;
static GPtrArray *applet_iids = NULL;

static char *cli_iid = NULL;
static char *cli_prefs_dir = NULL;
static char *cli_size = NULL;
static char *cli_orient = NULL;

static const struct poptOption options [] = {
	{ "iid", '\0', POPT_ARG_STRING, &cli_iid, 0, N_("Specify an applet IID to load"), NULL},
	{ "prefs-dir", '\0', POPT_ARG_STRING, &cli_prefs_dir, 0, N_("Specify a gconf location in which the applet preferences should be stored"), NULL},
	{ "size", '\0', POPT_ARG_STRING, &cli_size, 0, N_("Specify the initial size of the applet (xx-small, medium, large etc.)"), NULL},
	POPT_AUTOHELP
	{ "orient", '\0', POPT_ARG_STRING, &cli_orient, 0, N_("Specify the initial orientation of the applet (top, bottom, left or right)"), NULL},
	{NULL, '\0', 0, NULL, 0}
};

static char *sizes [] = {
	"xx-small", "x-small", "small", "medium", "large", "x-large", "xx-large"
};

static char *orients [] = {
	"top", "bottom", "left", "right" 
};

static char *
construct_moniker (void)
{
	const char *iid;
	const char *prefs_key;
	char       *size;
	char       *orient;
	int         idx;

	idx = gtk_option_menu_get_history (GTK_OPTION_MENU (applet_options));
	iid = g_ptr_array_index (applet_iids, idx);
	g_assert (iid != NULL);

	prefs_key = gtk_entry_get_text (GTK_ENTRY (prefs_dir_entry));

	idx = gtk_option_menu_get_history (GTK_OPTION_MENU (size_options));
	g_assert (idx > -1 && idx < G_N_ELEMENTS (sizes));
	size = sizes [idx];

	idx = gtk_option_menu_get_history (GTK_OPTION_MENU (orient_options));
	g_assert (idx > -1 && idx < G_N_ELEMENTS (orients));
	orient = orients [idx];

	return g_strdup_printf ("%s!prefs_key=%s;size=%s;orient=%s",
				iid, prefs_key, size, orient);
}

static void
load_applet_into_window (const char *moniker)
{
	GtkWidget *applet_window;
	GtkWidget *applet;

	applet_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (G_OBJECT (applet_window), "destroy",
			  G_CALLBACK (gtk_main_quit), NULL);

	applet = bonobo_widget_new_control (moniker, NULL);

	gtk_widget_show (applet);

	gtk_container_add (GTK_CONTAINER (applet_window), applet);

	gtk_widget_show (applet_window);
}

static void
load_applet_from_command_line (void)
{
	GString *str;

	g_assert (cli_iid != NULL);

	str = g_string_new (cli_iid);

	if (cli_prefs_dir || cli_size || cli_orient) {
		g_string_append_c (str, '!');

		if (cli_prefs_dir)
			g_string_append_printf (str, "prefs_key=%s", cli_prefs_dir);

		g_string_append_c (str, ';');

		if (cli_size)
			g_string_append_printf (str, "size=%s", cli_size);

		g_string_append_c (str, ';');

		if (cli_orient)
			g_string_append_printf (str, "orient=%s", cli_orient);
	}
	
	g_print ("Loading %s\n", str->str);

	load_applet_into_window (str->str);

	g_string_free (str, TRUE);
}

G_GNUC_UNUSED void
on_ok_button_clicked (GtkButton *button,
		      gpointer   dummy)
{
	char *moniker;

	moniker = construct_moniker ();
	load_applet_into_window (moniker);
	g_free (moniker);
}

static void
setup_options (void)
{
	Bonobo_ServerInfoList *applets;
	GtkWidget             *menu;
	CORBA_Environment      env;
	int                    i;
	char                  *prefs_dir;
	char                  *unique_key;

	CORBA_exception_init (&env);

	applets = bonobo_activation_query (
			"has (repo_ids, 'IDL:GNOME/Vertigo/PanelAppletShell:1.0')",
			NULL, &env);

	if (BONOBO_EX (&env))
		g_error (_("query returned exception %s\n"), BONOBO_EX_REPOID (&env));

	CORBA_exception_free (&env);

	menu = gtk_menu_new ();
	applet_iids = g_ptr_array_sized_new (applets->_length);

	for (i = 0; i < applets->_length; i++) {
		Bonobo_ServerInfo *info;
		GtkWidget         *menuitem;

		info = &applets->_buffer [i];

		g_ptr_array_add (applet_iids, g_strdup (info->iid));

		menuitem = gtk_menu_item_new_with_label (info->iid);
		gtk_widget_show (menuitem);

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (applet_options), menu);

	CORBA_free (applets);

	unique_key = gconf_unique_key ();
	prefs_dir = g_strdup_printf ("/tmp/%s", unique_key);
	g_free (unique_key);
	gtk_entry_set_text (GTK_ENTRY (prefs_dir_entry), prefs_dir);
	g_free (prefs_dir);
}

int
main (int argc, char **argv)
{
	GtkWidget *win;
	GladeXML  *gui;
	char      *gladefile;

	gnome_program_init (argv [0], "0.0.0.0", LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_POPT_TABLE, options,
			    GNOME_PARAM_NONE);

	if (cli_iid) {
		load_applet_from_command_line ();
		gtk_main ();
		return 0;
	}

	gladefile = PANEL_APPLET_GLADEDIR "/panel-test-applets.glade";
	gui = glade_xml_new (gladefile, "toplevel", NULL);
	if (!gui) {
		g_warning ("Error loading `%s'", gladefile);
		return 1;
	}

	glade_xml_signal_autoconnect (gui);

	win             = glade_xml_get_widget (gui, "toplevel");
	applet_options  = glade_xml_get_widget (gui, "applet-options");
	prefs_dir_entry = glade_xml_get_widget (gui, "prefs-dir-entry");
	orient_options  = glade_xml_get_widget (gui, "orient-options");
	size_options    = glade_xml_get_widget (gui, "size-options");

	setup_options ();

	gtk_widget_show (win);

	gtk_main ();

	return 0;
}
