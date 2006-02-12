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
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-widget.h>
#include <gconf/gconf.h>

#include "panel-applet.h"

void on_execute_button_clicked (GtkButton *button, gpointer dummy);

static GtkWidget *win = NULL;
static GtkWidget *applet_combo = NULL;
static GtkWidget *prefs_dir_entry = NULL;
static GtkWidget *orient_combo = NULL;
static GtkWidget *size_combo = NULL;

static char *cli_iid = NULL;
static char *cli_prefs_dir = NULL;
static char *cli_size = NULL;
static char *cli_orient = NULL;

static const GOptionEntry options [] = {
	{ "iid", 0, 0, G_OPTION_ARG_STRING, &cli_iid, N_("Specify an applet IID to load"), NULL},
	{ "prefs-dir", 0, 0, G_OPTION_ARG_STRING, &cli_prefs_dir, N_("Specify a gconf location in which the applet preferences should be stored"), NULL},
	{ "size", 0, 0, G_OPTION_ARG_STRING, &cli_size, N_("Specify the initial size of the applet (xx-small, medium, large etc.)"), NULL},
	{ "orient", 0, 0, G_OPTION_ARG_STRING, &cli_orient, N_("Specify the initial orientation of the applet (top, bottom, left or right)"), NULL},
	{ NULL}
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
	{ N_("Orientation|Top"),    "top"    },
	{ N_("Orientation|Bottom"), "bottom" },
	{ N_("Orientation|Left"),   "left"   },
	{ N_("Orientation|Right"),  "right"  }
};


static ComboItem size_items [] = {
	{ N_("Size|XX Small"), "xx-small" },
	{ N_("Size|X Small"),  "x-small"  },
	{ N_("Size|Small"),    "small"    },
	{ N_("Size|Medium"),   "medium"   },
	{ N_("Size|Large"),    "large"    },
	{ N_("Size|X Large"),  "x-large"  },
	{ N_("Size|XX Large"), "xx-large" }
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

	applet = bonobo_widget_new_control (moniker, NULL);

	if (!applet) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (win ? GTK_WINDOW (win) : NULL,
						 GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("Failed to load applet %s"),
						 title);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}

	applet_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_widget_show (applet);

	gtk_container_add (GTK_CONTAINER (applet_window), applet);

	//FIXME: we could set the window icon with the applet icon
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
on_execute_button_clicked (GtkButton *button,
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
				    COLUMN_TEXT, dynamic ? g_strdup (items [i].name) : Q_(items [i].name),
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
	GOptionContext *context;
	GladeXML       *gui;
	char           *gladefile;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new ("");

	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	gnome_program_init (argv [0], "0.0.0.0", LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_GOPTION_CONTEXT, context,
			    GNOME_PARAM_NONE);

	g_option_context_free (context);

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
