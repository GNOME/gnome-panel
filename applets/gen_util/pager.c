/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * libwnck based pager applet.
 * (C) 2001 Alexander Larsson 
 * (C) 2001 Red Hat, Inc
 *
 * Authors: Alexander Larsson
 *
 */

#define WNCK_I_KNOW_THIS_IS_UNSTABLE 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <panel-applet.h>
#include <panel-applet-gconf.h>

#include <stdlib.h>

#include <gtk/gtk.h>
#include <libbonobo.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <glade/glade-xml.h>
#include <libwnck/libwnck.h>
#include <gconf/gconf-client.h>

#include "pager.h"

#include "egg-screen-help.h"

/* even 16 is pretty darn dubious. */
#define MAX_REASONABLE_ROWS 16
#define DEFAULT_ROWS 1

typedef struct {
	GtkWidget *applet;

	GtkWidget *frame;
	GtkWidget *pager;
	
	WnckScreen *screen;

	/* Properties: */
	GtkWidget *properties_dialog;
	GtkWidget *display_workspaces_toggle;
	GtkWidget *all_workspaces_radio;
	GtkWidget *current_only_radio;
	GtkWidget *num_rows_spin;	       /* for vertical layout this is cols */
	GtkWidget *label_row_col;
	GtkWidget *num_workspaces_spin;
	GtkWidget *workspaces_tree;

	GtkListStore *workspaces_store;
	
	GtkOrientation orientation;
	int n_rows;				/* for vertical layout this is cols */
	WnckPagerDisplayMode display_mode;
	gboolean display_all;
	int size;

	/* gconf listeners id */
	guint listeners [3];
} PagerData;

static void display_properties_dialog (BonoboUIComponent *uic,
				       PagerData         *pager,
				       const gchar       *verbname);
static void display_help_dialog       (BonoboUIComponent *uic,
				       PagerData         *pager,
				       const gchar       *verbname);
static void display_about_dialog      (BonoboUIComponent *uic,
				       PagerData         *pager,
				       const gchar       *verbname);
static void set_tooltip               (GtkWidget         *applet);


static void
pager_update (PagerData *pager)
{
	int internal_size;

	/* 2 pixels of frame on each side */
	internal_size = pager->size - 2 * 2;
	
	if (pager->orientation == GTK_ORIENTATION_HORIZONTAL) {
		gtk_widget_set_size_request (GTK_WIDGET (pager->pager),
					     -1, internal_size);
	} else {
		gtk_widget_set_size_request (GTK_WIDGET (pager->pager),
					     internal_size, -1);
	}

	wnck_pager_set_orientation (WNCK_PAGER (pager->pager),
				    pager->orientation);
	wnck_pager_set_n_rows (WNCK_PAGER (pager->pager),
			       pager->n_rows);
	wnck_pager_set_display_mode (WNCK_PAGER (pager->pager),
				     pager->display_mode);
	wnck_pager_set_show_all (WNCK_PAGER (pager->pager),
				 pager->display_all);
}

static WnckScreen *
applet_get_screen (GtkWidget *applet)
{
	int screen_num;

	if (!gtk_widget_has_screen (applet))
		return wnck_screen_get_default ();

	screen_num = gdk_screen_get_number (gtk_widget_get_screen (applet));

	return wnck_screen_get (screen_num);
}

static void
applet_realized (PanelApplet *applet,
		 PagerData   *pager)
{
	WnckScreen *screen;

	screen = applet_get_screen (GTK_WIDGET (applet));

	wnck_pager_set_screen (WNCK_PAGER (pager->pager), screen);
}

static void
applet_change_orient (PanelApplet       *applet,
		      PanelAppletOrient  orient,
		      PagerData         *pager)
{
	GtkOrientation new_orient;
  
	switch (orient)	{
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

	if (new_orient == pager->orientation)
		return;
  
	pager->orientation = new_orient;
	pager_update (pager);
	if (pager->label_row_col) 
		gtk_label_set_text(GTK_LABEL(pager->label_row_col), pager->orientation == GTK_ORIENTATION_HORIZONTAL ? _("Rows") : _("Columns"));	
}

static void 
response_cb(GtkWidget * widget,int id, PagerData *pager)
{
	if(id == GTK_RESPONSE_HELP) {
		
		GError *error = NULL;
		static GnomeProgram *applet_program = NULL;

		if (!applet_program) {
			int argc = 1;
			char *argv[2] = { "workspace-switcher" };
			applet_program = gnome_program_init ("workspace-switcher", VERSION,
							     LIBGNOME_MODULE, argc, argv,
							     GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);
		}

		egg_help_display_desktop_on_screen (
			applet_program, "workspace-switcher",
			"workspace-switcher", "workspacelist-prefs",
			gtk_widget_get_screen (pager->applet),
			&error);
		if (error) {
			GtkWidget *dialog;
			dialog = gtk_message_dialog_new (GTK_WINDOW(widget),
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_CLOSE,
							  _("There was an error displaying help: %s"),
							 error->message);

			g_signal_connect (G_OBJECT (dialog), "response",
					  G_CALLBACK (gtk_widget_destroy),
					  NULL);

			gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
			gtk_window_set_screen (GTK_WINDOW (dialog),
					       gtk_widget_get_screen (pager->applet));
			gtk_widget_show (dialog);
			g_error_free (error);

		}

	}
	else
		gtk_widget_hide (widget);
}

static void
applet_change_pixel_size (PanelApplet *applet,
			  gint         size,
			  PagerData   *pager)
{
	if (pager->size == size)
		return;

	pager->size = size;
	pager_update (pager);
}

static void
destroy_pager(GtkWidget * widget, PagerData *pager)
{
	GConfClient *client = gconf_client_get_default ();

	gconf_client_notify_remove (client, pager->listeners[0]);
	gconf_client_notify_remove (client, pager->listeners[1]);
	gconf_client_notify_remove (client, pager->listeners[2]);

	pager->listeners[0] = 0;
	pager->listeners[1] = 0;
	pager->listeners[2] = 0;

	/* FIXME: does this not leak PagerData ? */

}

static const BonoboUIVerb pager_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("PagerPreferences", display_properties_dialog),
	BONOBO_UI_UNSAFE_VERB ("PagerHelp",        display_help_dialog),
	BONOBO_UI_UNSAFE_VERB ("PagerAbout",       display_about_dialog),
        BONOBO_UI_VERB_END
};

static void
num_rows_changed (GConfClient *client,
		  guint        cnxn_id,
		  GConfEntry  *entry,
		  PagerData   *pager)
{
	int n_rows = DEFAULT_ROWS;
	
	if (entry->value != NULL &&
	    entry->value->type == GCONF_VALUE_INT) {
		n_rows = gconf_value_get_int (entry->value);
	}

        n_rows = CLAMP (n_rows, 1, MAX_REASONABLE_ROWS);
        
	pager->n_rows = n_rows;
	pager_update (pager);

	if (pager->num_rows_spin &&
	    gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pager->num_rows_spin)) != n_rows)
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (pager->num_rows_spin), pager->n_rows);
}

static void
display_workspace_names_changed (GConfClient *client,
				 guint        cnxn_id,
				 GConfEntry  *entry,
				 PagerData   *pager)
{
	gboolean value = FALSE; /* Default value */
	
	if (entry->value != NULL &&
	    entry->value->type == GCONF_VALUE_BOOL) {
		value = gconf_value_get_bool (entry->value);
	}

	if (value) {
		pager->display_mode = WNCK_PAGER_DISPLAY_NAME;
	} else {
		pager->display_mode = WNCK_PAGER_DISPLAY_CONTENT;
	}
	pager_update (pager);
	
	if (pager->display_workspaces_toggle &&
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pager->display_workspaces_toggle)) != value) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->display_workspaces_toggle),
					      value);
	}
}


static void
all_workspaces_changed (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			PagerData   *pager)
{
	gboolean value = TRUE; /* Default value */
	
	if (entry->value != NULL &&
	    entry->value->type == GCONF_VALUE_BOOL) {
		value = gconf_value_get_bool (entry->value);
	}

	pager->display_all = value;
	pager_update (pager);

	if (pager->all_workspaces_radio){
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pager->all_workspaces_radio)) != value) {
			if (value) {
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->all_workspaces_radio), TRUE);
			} else {
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->current_only_radio), TRUE);
			}
		}
		gtk_widget_set_sensitive (pager->num_rows_spin, value);
	}
}

static void
setup_gconf (PagerData *pager)
{
	GConfClient *client;
	char *key;

	client = gconf_client_get_default ();

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (pager->applet),
					       "num_rows");
	pager->listeners[0] = gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)num_rows_changed,
				pager,
				NULL, NULL);
		
	g_free (key);


	key = panel_applet_gconf_get_full_key (PANEL_APPLET (pager->applet),
					       "display_workspace_names");
	pager->listeners[1] = gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)display_workspace_names_changed,
				pager,
				NULL, NULL);
		
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (pager->applet),
					       "display_all_workspaces");
	pager->listeners[2] = gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)all_workspaces_changed,
				pager,
				NULL, NULL);
		
	g_free (key);

}

gboolean
fill_pager_applet(PanelApplet *applet)
{
	PagerData *pager;
	GError *error;
	gboolean display_names;
	
	panel_applet_add_preferences (applet, "/schemas/apps/pager_applet/prefs", NULL);
	
	pager = g_new0 (PagerData, 1);

	pager->applet = GTK_WIDGET (applet);

	setup_gconf (pager);
	
	error = NULL;
	pager->n_rows = panel_applet_gconf_get_int (applet, "num_rows", &error);
	if (error) {
                g_printerr (_("Error loading num_rows value for Workspace Switcher: %s\n"),
                            error->message);
		g_error_free (error);
                /* leave current value */
	}

        pager->n_rows = CLAMP (pager->n_rows, 1, MAX_REASONABLE_ROWS);

	error = NULL;
	display_names = panel_applet_gconf_get_bool (applet, "display_workspace_names", &error);
	if (error) {
                g_printerr (_("Error loading display_workspace_names value for Workspace Switcher: %s\n"),
                            error->message);
		g_error_free (error);
                /* leave current value */
	}

	if (display_names) {
		pager->display_mode = WNCK_PAGER_DISPLAY_NAME;
	} else {
		pager->display_mode = WNCK_PAGER_DISPLAY_CONTENT;
	}

	error = NULL;
	pager->display_all = panel_applet_gconf_get_bool (applet, "display_all_workspaces", &error);
	if (error) {
                g_printerr (_("Error loading display_all_workspaces value for Workspace Switcher: %s\n"),
                            error->message);
		g_error_free (error);
                /* leave current value */
	}
	
	pager->size = panel_applet_get_size (applet);
	switch (panel_applet_get_orient (applet)) {
	case PANEL_APPLET_ORIENT_LEFT:
	case PANEL_APPLET_ORIENT_RIGHT:
		pager->orientation = GTK_ORIENTATION_VERTICAL;
		break;
	case PANEL_APPLET_ORIENT_UP:
	case PANEL_APPLET_ORIENT_DOWN:
	default:
		pager->orientation = GTK_ORIENTATION_HORIZONTAL;
		break;
	}

	pager->screen = applet_get_screen (pager->applet);

	/* because the pager doesn't respond to signals at the moment */
	wnck_screen_force_update (pager->screen);

	pager->frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (pager->frame), GTK_SHADOW_IN);
	
	pager->pager = wnck_pager_new (pager->screen);

	g_signal_connect (G_OBJECT (pager->pager), "destroy",
			  G_CALLBACK (destroy_pager),
			  pager);

	pager_update (pager);
	
	gtk_widget_show (pager->pager);
	gtk_container_add (GTK_CONTAINER (pager->frame), pager->pager);

	gtk_widget_show (pager->frame);
	
	gtk_container_add (GTK_CONTAINER (pager->applet), pager->frame);

	set_tooltip (GTK_WIDGET (pager->applet));

	gtk_widget_show (pager->applet);

	g_signal_connect (G_OBJECT (pager->applet),
			  "realize",
			  G_CALLBACK (applet_realized),
			  pager);
	g_signal_connect (G_OBJECT (pager->applet),
			  "change_orient",
			  G_CALLBACK (applet_change_orient),
			  pager);
	g_signal_connect (G_OBJECT (pager->applet),
			  "change_size",
			  G_CALLBACK (applet_change_pixel_size),
			  pager);
	
	panel_applet_setup_menu_from_file (PANEL_APPLET (pager->applet),
					   NULL,
					   "GNOME_PagerApplet.xml",
					   NULL,
					   pager_menu_verbs,
					   pager);

	return TRUE;
}


static void
display_help_dialog (BonoboUIComponent *uic,
		     PagerData         *pager,
		     const gchar       *verbname)
{
	GError *error = NULL;
	static GnomeProgram *applet_program = NULL;

	if (!applet_program) {
		int argc = 1;
		char *argv[2] = { "workspace-switcher" };
		applet_program = gnome_program_init ("workspace-switcher", VERSION,
						     LIBGNOME_MODULE, argc, argv,
						     GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);
	}

	egg_help_display_desktop_on_screen (
		applet_program, "workspace-switcher",
		"workspace-switcher", NULL,
		gtk_widget_get_screen (pager->applet),
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
				       gtk_widget_get_screen (pager->applet));
		gtk_widget_show (dialog);
		g_error_free (error);

	}
}

static void
display_about_dialog (BonoboUIComponent *uic,
		      PagerData         *pager,
		      const gchar       *verbname)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf = NULL;
	gchar *file;
	
	static const gchar *authors[] =
	{
		"Alexander Larsson <alla@lysator.liu.se>",
		NULL
	};
	const char *documenters [] = {
	  NULL
	};
	const char *translator_credits = _("translator_credits");

	if (about) {
		gtk_window_set_screen (GTK_WINDOW (about),
				       gtk_widget_get_screen (pager->applet));
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, "gnome-workspace.png", TRUE, NULL);
	pixbuf = gdk_pixbuf_new_from_file (file, NULL);
	g_free(file);

	about = gnome_about_new (_("Workspace Switcher"), VERSION,
				 "Copyright \xc2\xa9 2001-2002 Red Hat, Inc.",
				 _("The Workspace Switcher shows you a small version of your workspaces that lets you manage your windows."),
				 authors,
				 documenters,
				 strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
				 pixbuf);
	
	gtk_window_set_wmclass (GTK_WINDOW (about), "pager", "Pager");
	gtk_window_set_screen (GTK_WINDOW (about),
			       gtk_widget_get_screen (pager->applet));

	if (pixbuf) {
		gtk_window_set_icon (GTK_WINDOW (about), pixbuf);
		g_object_unref (pixbuf);
	}
	
	g_signal_connect (G_OBJECT(about), "destroy",
			  (GCallback)gtk_widget_destroyed, &about);
	
	gtk_widget_show (about);
}


static void
display_workspace_names_toggled (GtkToggleButton *button,
				 PagerData       *pager)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (pager->applet),
				     "display_workspace_names",
				     gtk_toggle_button_get_active (button),
				     NULL);
}

static void
all_workspaces_toggled (GtkToggleButton *button,
			PagerData       *pager)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (pager->applet),
				     "display_all_workspaces",
				     gtk_toggle_button_get_active (button),
				     NULL);
}

static void
num_rows_value_changed (GtkSpinButton *button,
			PagerData       *pager)
{
	panel_applet_gconf_set_int (PANEL_APPLET (pager->applet),
				    "num_rows",
				    gtk_spin_button_get_value_as_int (button),
				    NULL);
}

static void
update_workspaces_model (PagerData *pager)
{
	int nr_ws, i;
	WnckWorkspace *workspace;
	GtkTreeIter iter;

	nr_ws = wnck_screen_get_workspace_count (pager->screen);
        
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pager->num_workspaces_spin), nr_ws);

	gtk_list_store_clear (pager->workspaces_store);
	for (i = 0; i < nr_ws; i++) {
		workspace = wnck_screen_get_workspace (pager->screen, i);
		gtk_list_store_append (pager->workspaces_store, &iter);
		gtk_list_store_set (pager->workspaces_store,
				    &iter,
				    0, wnck_workspace_get_name (workspace),
				    -1);
	}
}

static void
workspace_renamed (WnckWorkspace *space,
		   PagerData     *pager)
{
	update_workspaces_model (pager);
}

static void
workspace_created (WnckScreen    *screen,
		   WnckWorkspace *space,
		   PagerData     *pager)
{
        g_return_if_fail (WNCK_IS_SCREEN (screen));
        
	update_workspaces_model (pager);
	g_signal_connect (G_OBJECT (space), "name_changed",
			  (GCallback) workspace_renamed, pager);
}

static void
workspace_destroyed (WnckScreen    *screen,
		     WnckWorkspace *space,
		     PagerData     *pager)
{
        g_return_if_fail (WNCK_IS_SCREEN (screen));
	update_workspaces_model (pager);
}

static void
num_workspaces_value_changed (GtkSpinButton *button,
			      PagerData       *pager)
{
        wnck_screen_change_workspace_count (pager->screen,
                                            gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pager->num_workspaces_spin)));
}

static void 
workspace_name_edited (GtkCellRendererText *cell_renderer_text,
		       const gchar         *path,
		       const gchar         *new_text,
		       PagerData           *pager)
{
        const gint *indices;
        WnckWorkspace *workspace;
        GtkTreePath *p;

        p = gtk_tree_path_new_from_string (path);
        indices = gtk_tree_path_get_indices (p);
        workspace = wnck_screen_get_workspace (pager->screen,
                                               indices[0]);
        if (workspace != NULL)
                wnck_workspace_change_name (workspace,
                                            new_text);
        else
                g_warning ("Edited name of workspace %d which no longer exists",
                           indices[0]);

        gtk_tree_path_free (p);
}

static gboolean
delete_event (GtkWidget *widget, gpointer data)
{
	gtk_widget_hide (widget);
	return TRUE;
}


#define WID(s) glade_xml_get_widget (xml, s)

static void
close_dialog (GtkWidget *button,
              gpointer data)
{
	PagerData *pager = data;
	GtkTreeViewColumn *col;

	/* This is a hack. The "editable" signal for GtkCellRenderer is emitted
	only on button press or focus cycle. Hence when the user changes the
	name and closes the preferences dialog without a button-press he would
	lose the name changes. So, we call the gtk_cell_editable_editing_done
	to stop the editing. Thanks to Paolo for a better crack than the one I had.
	*/

	col = gtk_tree_view_get_column(GTK_TREE_VIEW (pager->workspaces_tree),0);
	if (col->editable_widget != NULL && GTK_IS_CELL_EDITABLE (col->editable_widget))
	    gtk_cell_editable_editing_done(col->editable_widget);

	gtk_widget_hide (pager->properties_dialog);
}

static void
setup_dialog (GladeXML  *xml,
	      PagerData *pager)
{
	gboolean value;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	int nr_ws, i;
	
	pager->display_workspaces_toggle = WID ("workspace_name_toggle");
	pager->all_workspaces_radio = WID ("all_workspaces_radio");
	pager->current_only_radio = WID ("current_only_radio");
	pager->num_rows_spin = WID ("num_rows_spin");
	pager->label_row_col = WID("label_row_col");
	pager->num_workspaces_spin = WID ("num_workspaces_spin");
	pager->workspaces_tree = WID ("workspaces_tree_view");

	/* Display workspace names: */
	
	g_signal_connect (G_OBJECT (pager->display_workspaces_toggle), "toggled",
			  (GCallback) display_workspace_names_toggled, pager);

	if (pager->display_mode == WNCK_PAGER_DISPLAY_NAME) {
		value = TRUE;
	} else {
		value = FALSE;
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->display_workspaces_toggle),
				      value);

	/* Display all workspaces: */
	g_signal_connect (G_OBJECT (pager->all_workspaces_radio), "toggled",
			  (GCallback) all_workspaces_toggled, pager);

	if (pager->display_all) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->all_workspaces_radio), TRUE);
		gtk_widget_set_sensitive (pager->num_rows_spin, TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->current_only_radio), TRUE);
		gtk_widget_set_sensitive (pager->num_rows_spin, FALSE);
	}
		
	/* Num rows: */
	g_signal_connect (G_OBJECT (pager->num_rows_spin), "value_changed",
			  (GCallback) num_rows_value_changed, pager);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pager->num_rows_spin), pager->n_rows);
	gtk_label_set_text(GTK_LABEL(pager->label_row_col), pager->orientation == GTK_ORIENTATION_HORIZONTAL ? _("Rows") : _("Columns"));

	g_signal_connect (pager->properties_dialog, "delete_event",
			  G_CALLBACK (delete_event),
			  pager);
	g_signal_connect (pager->properties_dialog, "response",
			  G_CALLBACK (response_cb),
			  pager);
	
	g_signal_connect (WID ("done_button"), "clicked",
			  (GCallback) close_dialog, pager);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pager->num_workspaces_spin),
				   wnck_screen_get_workspace_count (pager->screen));
	g_signal_connect (G_OBJECT (pager->num_workspaces_spin), "value_changed",
			  (GCallback) num_workspaces_value_changed, pager);
	
	g_signal_connect (pager->screen, "workspace_created",
                          (GCallback) workspace_created, pager);
	g_signal_connect (pager->screen, "workspace_destroyed",
                          (GCallback) workspace_destroyed, pager);

	pager->workspaces_store = gtk_list_store_new (1, G_TYPE_STRING, NULL);
	update_workspaces_model (pager);
	gtk_tree_view_set_model (GTK_TREE_VIEW (pager->workspaces_tree), GTK_TREE_MODEL (pager->workspaces_store));
	cell = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT, "editable", TRUE, NULL);
	column = gtk_tree_view_column_new_with_attributes ("workspace",
							   cell,
							   "text", 0,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pager->workspaces_tree), column);
	g_signal_connect (cell, "edited",
			  (GCallback) workspace_name_edited, pager);
	
	nr_ws = wnck_screen_get_workspace_count (pager->screen);
	for (i = 0; i < nr_ws; i++) {
		g_signal_connect (G_OBJECT (wnck_screen_get_workspace (pager->screen, i)), "name_changed",
				  (GCallback) workspace_renamed, pager);
	}
}

static void 
display_properties_dialog (BonoboUIComponent *uic,
			   PagerData         *pager,
			   const gchar       *verbname)
{
	if (pager->properties_dialog == NULL) {
		GladeXML  *xml;

		xml = glade_xml_new (PAGER_GLADEDIR "/pager.glade", NULL, NULL);
		pager->properties_dialog = glade_xml_get_widget (xml, "pager_properties_dialog");

		g_object_add_weak_pointer (G_OBJECT (pager->properties_dialog), 
					   (gpointer *) &pager->properties_dialog);

		setup_dialog (xml, pager);
		
		g_object_unref (G_OBJECT (xml));
	}

	gtk_window_set_screen (GTK_WINDOW (pager->properties_dialog),
			       gtk_widget_get_screen (pager->applet));
	gtk_window_present (GTK_WINDOW (pager->properties_dialog));
}

/*
 *Function to set tooltip
 */
static void
set_tooltip (GtkWidget *applet)
{
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new ();
	g_object_ref (tooltips);
	gtk_object_sink (GTK_OBJECT (tooltips));
	g_object_set_data_full (G_OBJECT (applet), "tooltips", tooltips,
                                (GDestroyNotify) g_object_unref);
        gtk_tooltips_set_tip (tooltips, applet,
                              _("Workspace Switcher"), NULL);
}
