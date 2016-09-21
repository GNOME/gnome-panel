/*
 * panel-test-applets.c:
 *
 * Authors:
 *    Mark McLoughlin <mark@skynet.ie>
 *
 * Copyright 2002 Sun Microsystems, Inc.
 */

#include <config.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libpanel-util/panel-cleanup.h>

#include <libpanel-applet-private/panel-applet-container.h>
#include <libpanel-applet-private/panel-applets-manager-dbus.h>

#include "panel-modules.h"

G_GNUC_UNUSED void on_execute_button_clicked (GtkButton *button, gpointer dummy);

static GtkWidget *win = NULL;
static GtkWidget *applet_combo = NULL;
static GtkWidget *orient_combo = NULL;

static char *cli_iid = NULL;
static char *cli_orient = NULL;

static const GOptionEntry options [] = {
	{ "iid", 0, 0, G_OPTION_ARG_STRING, &cli_iid, N_("Specify an applet IID to load"), NULL},
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
	guint       value;
} ComboItem;

static ComboItem orient_items [] = {
	{ NC_("Orientation", "Top"),    PANEL_ORIENTATION_TOP    },
	{ NC_("Orientation", "Bottom"), PANEL_ORIENTATION_BOTTOM },
	{ NC_("Orientation", "Left"),   PANEL_ORIENTATION_LEFT   },
	{ NC_("Orientation", "Right"),  PANEL_ORIENTATION_RIGHT  }
};

static guint
get_combo_value (GtkWidget *combo_box)
{
	GtkTreeIter  iter;
	GtkTreeModel *model;
	guint         value;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo_box), &iter))
		return 0;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	gtk_tree_model_get (model, &iter, COLUMN_ITEM, &value, -1);

	return value;
}

static gchar *
get_combo_applet_id (GtkWidget *combo_box)
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

static void
applet_activated_cb (GObject      *source_object,
		     GAsyncResult *res,
		     GtkWidget    *applet_window)
{
	GError *error = NULL;

	if (!panel_applet_container_add_finish (PANEL_APPLET_CONTAINER (source_object),
						res, &error)) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (GTK_WINDOW (applet_window),
						 GTK_DIALOG_MODAL|
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("Failed to load applet %s"),
						 error->message); // FIXME
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}

	gtk_widget_show (applet_window);
}

static void
load_applet_into_window (const char *title,
			 guint       orientation)
{
	GtkWidget       *box;
	GtkWidget       *container;
	GtkWidget       *applet_window;
	GVariantBuilder  builder;

	applet_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	//FIXME: we could set the window icon with the applet icon
	gtk_window_set_title (GTK_WINDOW (applet_window), title);
	gtk_window_set_transient_for (GTK_WINDOW (applet_window), GTK_WINDOW (win));
	gtk_window_set_default_size (GTK_WINDOW (applet_window),
	                             orientation & PANEL_HORIZONTAL_MASK ? 200 : 24,
	                             orientation & PANEL_HORIZONTAL_MASK ? 24 : 200);

	box = gtk_box_new (orientation & PANEL_HORIZONTAL_MASK ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (applet_window), box);
	gtk_widget_show (box);

	container = panel_applet_container_new ();
	gtk_box_set_center_widget (GTK_BOX (box), container);
	gtk_widget_show (container);

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (&builder, "{sv}",
			       "orient", g_variant_new_uint32 (orientation));
	panel_applet_container_add (PANEL_APPLET_CONTAINER (container),
				    gtk_widget_get_screen (applet_window),
				    title, NULL,
				    (GAsyncReadyCallback)applet_activated_cb,
				    applet_window,
				    g_variant_builder_end (&builder));
}

static void
load_applet_from_command_line (void)
{
	guint orient = PANEL_ORIENTATION_TOP;
	guint i;

	g_assert (cli_iid != NULL);

	if (cli_orient) {
		for (i = 0; i < G_N_ELEMENTS (orient_items); i++) {
			if (strcmp (g_dpgettext2 (NULL, "Orientation", orient_items[i].name), cli_orient) == 0) {
				orient = orient_items[i].value;
				break;
			}
		}
	}

	g_print ("Loading %s\n", cli_iid);

	load_applet_into_window (cli_iid, orient);
}

G_GNUC_UNUSED void
on_execute_button_clicked (GtkButton *button,
			   gpointer   dummy)
{
	char *title;

	title = get_combo_applet_id (applet_combo);

	load_applet_into_window (title, get_combo_value (orient_combo));
	g_free (title);
}

static void
setup_combo (GtkWidget  *combo_box,
	     ComboItem  *items,
	     const char *context,
	     int         nb_items)
{
	GtkListStore          *model;
	GtkTreeIter            iter;
	GtkCellRenderer       *renderer;
	int                    i;

	model = gtk_list_store_new (NUMBER_COLUMNS,
				    G_TYPE_STRING,
				    G_TYPE_INT);

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box),
				 GTK_TREE_MODEL (model));


	for (i = 0; i < nb_items; i++) {
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    COLUMN_TEXT, g_dpgettext2 (NULL, context, items [i].name),
				    COLUMN_ITEM, items [i].value,
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
	PanelAppletsManager *manager;
	GList               *applet_list, *l;
	int                  i;
	GtkListStore        *model;
	GtkTreeIter          iter;
	GtkCellRenderer     *renderer;

	model = gtk_list_store_new (NUMBER_COLUMNS,
				    G_TYPE_STRING,
				    G_TYPE_STRING);

	gtk_combo_box_set_model (GTK_COMBO_BOX (applet_combo),
				 GTK_TREE_MODEL (model));

	manager = g_object_new (PANEL_TYPE_APPLETS_MANAGER_DBUS, NULL);
	applet_list = PANEL_APPLETS_MANAGER_GET_CLASS (manager)->get_applets (manager);
	for (l = applet_list, i = 1; l; l = g_list_next (l), i++) {
		PanelAppletInfo *info = (PanelAppletInfo *)l->data;

		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    COLUMN_TEXT, g_strdup (panel_applet_info_get_name (info)),
				    COLUMN_ITEM, g_strdup (panel_applet_info_get_iid (info)),
				    -1);
	}
	g_list_free (applet_list);
	g_object_unref (manager);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (applet_combo),
				    renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (applet_combo),
					renderer, "text", COLUMN_TEXT, NULL);

	gtk_combo_box_set_active (GTK_COMBO_BOX (applet_combo), 0);

	setup_combo (orient_combo, orient_items, "Orientation",
		     G_N_ELEMENTS (orient_items));
}

int
main (int argc, char **argv)
{
	GtkBuilder *builder;
	char       *applets_dir;
	GError     *error;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	error = NULL;
	if (!gtk_init_with_args (&argc, &argv,
				 "", (GOptionEntry *) options, GETTEXT_PACKAGE,
				 &error)) {
		if (error) {
			g_printerr ("%s\n", error->message);
			g_error_free (error);
		} else
			g_printerr ("Cannot initiliaze GTK+.\n");

		return 1;
	}

	panel_modules_ensure_loaded ();

	if (g_file_test ("../libpanel-applet", G_FILE_TEST_IS_DIR)) {
		applets_dir = g_strdup_printf ("%s:../libpanel-applet", PANEL_APPLETS_DIR);
		g_setenv ("GNOME_PANEL_APPLETS_DIR", applets_dir, FALSE);
		g_free (applets_dir);
	}

	if (cli_iid) {
		load_applet_from_command_line ();
		gtk_main ();
		panel_cleanup_do ();

		return 0;
	}

	builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	gtk_builder_add_from_resource (builder, "/org/gnome/panel/test/panel-test-applets.ui", NULL);

	gtk_builder_connect_signals (builder, NULL);

	win             = GTK_WIDGET (gtk_builder_get_object (builder,
							      "toplevel"));
	applet_combo    = GTK_WIDGET (gtk_builder_get_object (builder,
							      "applet-combo"));
	orient_combo    = GTK_WIDGET (gtk_builder_get_object (builder,
							      "orient-combo"));
	g_object_unref (builder);

	setup_options ();

	gtk_widget_show (win);

	gtk_main ();

	panel_cleanup_do ();

	return 0;
}
