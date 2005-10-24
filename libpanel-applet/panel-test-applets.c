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

static GtkWidget *applet_combo = NULL;
static GtkWidget *prefs_dir_entry = NULL;
static GtkWidget *orient_combo = NULL;
static GtkWidget *size_combo = NULL;

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

enum {
	COLUMN_TEXT,
	COLUMN_ITEM,
	NUMBER_COLUMNS
};

typedef struct {
	const char *name;
	const char *value;
} ComboItem;

static ComboItem orient_items [] = {
	{ N_("Top"),    "top"    },
	{ N_("Bottom"), "bottom" },
	{ N_("Left"),   "left"   },
	{ N_("Right"),  "right"  }
};


static ComboItem size_items [] = {
	{ N_("XX Small"), "xx-small" },
	{ N_("X Small"),  "x-small"  },
	{ N_("Small"),    "small"    },
	{ N_("Medium"),   "medium"   },
	{ N_("Large"),    "large"    },
	{ N_("X Large"),  "x-large"  },
	{ N_("XX Large"), "xx-large" }
};

static char *
get_combo_value (GtkWidget *combo_box)
{
	GtkTreeIter  iter;
	GtkTreeModel *model;
	char         *value;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo_box), &iter))
		return NULL;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	gtk_tree_model_get (model, &iter, COLUMN_ITEM, &value, -1);

	return value;
}

static char *
construct_moniker (void)
{
	const char *prefs_key;
	char       *iid;
	char       *size;
	char       *orient;
	char       *ret_value;

	iid = get_combo_value (applet_combo);
	g_assert (iid != NULL);
	size = get_combo_value (size_combo);
	g_assert (size != NULL);
	orient = get_combo_value (orient_combo);
	g_assert (orient != NULL);

	prefs_key = gtk_entry_get_text (GTK_ENTRY (prefs_dir_entry));

	ret_value= g_strdup_printf ("%s!prefs_key=%s;size=%s;orient=%s",
				    iid, prefs_key, size, orient);
	g_free (iid);
	g_free (size);
	g_free (orient);

	return ret_value;
}

static void
load_applet_into_window (const char *moniker,
			 const char *title)
{
	GtkWidget *applet_window;
	GtkWidget *applet;

	applet_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (G_OBJECT (applet_window), "destroy",
			  G_CALLBACK (gtk_main_quit), NULL);

	applet = bonobo_widget_new_control (moniker, NULL);

	gtk_widget_show (applet);

	gtk_container_add (GTK_CONTAINER (applet_window), applet);

	gtk_window_set_title (GTK_WINDOW (applet_window), title);
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

	load_applet_into_window (str->str, cli_iid);

	g_string_free (str, TRUE);
}

G_GNUC_UNUSED void
on_ok_button_clicked (GtkButton *button,
		      gpointer   dummy)
{
	char *moniker;
	char *title;

	moniker = construct_moniker ();
	title = get_combo_value (applet_combo);
	load_applet_into_window (moniker, title);
	g_free (moniker);
	g_free (title);
}

static void
setup_combo (GtkWidget *combo_box,
	     ComboItem *items,
	     int        nb_items,
	     gboolean   dynamic)
{
	GtkListStore          *model;
	GtkTreeIter            iter;
	GtkCellRenderer       *renderer;
	int                    i;

	model = gtk_list_store_new (NUMBER_COLUMNS,
				    G_TYPE_STRING,
				    G_TYPE_STRING);

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box),
				 GTK_TREE_MODEL (model));


	for (i = 0; i < nb_items; i++) {
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    COLUMN_TEXT, dynamic ? g_strdup (items [i].name) : _(items [i].name),
				    COLUMN_ITEM, dynamic ? g_strdup (items [i].value) : items [i].value,
				    -1);
	}

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box),
				    renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box),
					renderer, "text", COLUMN_TEXT, NULL);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);
}

static void
setup_options (void)
{
	Bonobo_ServerInfoList *applets;
	CORBA_Environment      env;
	int                    i;
	char                  *prefs_dir;
	char                  *unique_key;
	ComboItem             *applet_items;
	int                    applet_nb;

	CORBA_exception_init (&env);

	applets = bonobo_activation_query (
			"has (repo_ids, 'IDL:GNOME/Vertigo/PanelAppletShell:1.0')",
			NULL, &env);

	if (BONOBO_EX (&env))
		g_error (_("query returned exception %s\n"), BONOBO_EX_REPOID (&env));

	CORBA_exception_free (&env);

	applet_nb = applets->_length;
	applet_items = g_new0 (ComboItem, applet_nb);

	for (i = 0; i < applet_nb; i++) {
		Bonobo_ServerInfo *info;

		info = &applets->_buffer [i];

		applet_items[i].name = info->iid;
		applet_items[i].value = info->iid;
	}

	setup_combo (applet_combo, applet_items, applet_nb, TRUE);
	g_free (applet_items);
	CORBA_free (applets);

	setup_combo (size_combo, size_items, G_N_ELEMENTS (size_items), FALSE);
	setup_combo (orient_combo, orient_items,
		     G_N_ELEMENTS (orient_items), FALSE);

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
	applet_combo    = glade_xml_get_widget (gui, "applet-combo");
	prefs_dir_entry = glade_xml_get_widget (gui, "prefs-dir-entry");
	orient_combo    = glade_xml_get_widget (gui, "orient-combo");
	size_combo      = glade_xml_get_widget (gui, "size-combo");

	setup_options ();

	gtk_widget_show (win);

	gtk_main ();

	return 0;
}
