/*
 * libwnck based pager applet.
 * (C) 2001 Alexander Larsson 
 * (C) 2001 Red Hat, Inc
 *
 * Authors: Alexander Larsson
 *
 */

#define WNCK_I_KNOW_THIS_IS_UNSTABLE 1

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
	GtkWidget *num_rows_spin;
	GtkWidget *num_workspaces_spin;
	GtkWidget *workspaces_tree;

	GtkListStore *workspaces_store;
	
	GtkOrientation orientation;
	int n_rows;
	WnckPagerDisplayMode display_mode;
	gboolean display_all;
	int size;
  
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
destroy_pager(GtkWidget * widget, gpointer data)
{
}

static const BonoboUIVerb pager_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("PagerProperties", display_properties_dialog),
	BONOBO_UI_UNSAFE_VERB ("PagerHelp",       display_help_dialog),
	BONOBO_UI_UNSAFE_VERB ("PagerAbout",      display_about_dialog),
        BONOBO_UI_VERB_END
};

static const char pager_menu_xml [] =
	"<popup name=\"button3\">\n"
	"   <menuitem name=\"Pager Properties Item\" verb=\"PagerProperties\" _label=\"Properties ...\"\n"
	"             pixtype=\"stock\" pixname=\"gtk-properties\"/>\n"
	"   <menuitem name=\"Pager Help Item\" verb=\"PagerHelp\" _label=\"Help\"\n"
	"             pixtype=\"stock\" pixname=\"gtk-help\"/>\n"
	"   <menuitem name=\"Pager About Item\" verb=\"PagerAbout\" _label=\"About ...\"\n"
	"             pixtype=\"stock\" pixname=\"gnome-stock-about\"/>\n"
	"</popup>\n";


static void
num_rows_changed (GConfClient *client,
		  guint        cnxn_id,
		  GConfEntry  *entry,
		  PagerData   *pager)
{
	int n_rows = 2; /* Default value */
	
	if (entry->value != NULL &&
	    entry->value->type == GCONF_VALUE_INT) {
		n_rows = gconf_value_get_int (entry->value);
	}
	
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
	gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)num_rows_changed,
				pager,
				NULL, NULL);
		
	g_free (key);


	key = panel_applet_gconf_get_full_key (PANEL_APPLET (pager->applet),
					       "display_workspace_names");
	gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)display_workspace_names_changed,
				pager,
				NULL, NULL);
		
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (pager->applet),
					       "display_all_workspaces");
	gconf_client_notify_add(client, key,
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
	
	panel_applet_add_preferences (applet, "/schemas/apps/pager-applet/prefs", NULL);
	
	pager = g_new0 (PagerData, 1);

	pager->applet = GTK_WIDGET (applet);

	error = NULL;
	pager->n_rows = panel_applet_gconf_get_int (applet, "num_rows", &error);
	if (error) {
		g_error_free (error);
		pager->n_rows = 2; /* Default value */
	}

	error = NULL;
	display_names = panel_applet_gconf_get_bool (applet, "display_workspace_names", &error);
	if (error) {
		g_error_free (error);
		display_names = FALSE; /* Default value */
	}

	if (display_names) {
		pager->display_mode = WNCK_PAGER_DISPLAY_NAME;
	} else {
		pager->display_mode = WNCK_PAGER_DISPLAY_CONTENT;
	}

	error = NULL;
	pager->display_all = panel_applet_gconf_get_bool (applet, "display_all_workspaces", &error);
	if (error) {
		g_error_free (error);
		pager->display_all = TRUE; /* Default value */
	}
	
	pager->size = 48;
	pager->orientation = GTK_ORIENTATION_HORIZONTAL;

	/* TODO: Needs to get the screen number from DISPLAY or the panel. */
	pager->screen = wnck_screen_get (0);

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

	gtk_widget_show (pager->applet);

	g_signal_connect (G_OBJECT (pager->applet),
			  "change_orient",
			  G_CALLBACK (applet_change_orient),
			  pager);
	g_signal_connect (G_OBJECT (pager->applet),
			  "change_size",
			  G_CALLBACK (applet_change_pixel_size),
			  pager);
	
	panel_applet_setup_menu (PANEL_APPLET (pager->applet), pager_menu_xml, pager_menu_verbs, pager);

	setup_gconf (pager);
	
	return TRUE;
}


static void
display_help_dialog (BonoboUIComponent *uic,
		     PagerData         *pager,
		     const gchar       *verbname)
{
	/* TODO: Implement this */
}

static void
display_about_dialog (BonoboUIComponent *uic,
		      PagerData         *pager,
		      const gchar       *verbname)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf;
	gchar *file;
	
	static const gchar *authors[] =
	{
		"Alexander Larsson <alla@lysator.liu.se>",
		NULL
	};

	if (about != NULL) {
		gtk_widget_show (about);
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	pixbuf = NULL;

	/* TODO: This should not use gnome-clock! */
	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, "gnome-clock.png", TRUE, NULL);
	if (!file) {
		g_warning (G_STRLOC ": gnome-clock.png cannot be found");
		pixbuf = gdk_pixbuf_new_from_file (file, NULL);
	}

	about = gnome_about_new (_("Pager Applet"), "1.0",
				 _("(c) 2001 Red Hat, Inc."),
				 _("The pager applet shows you a small version of your workspaces that lets you manage the windows."),
				 authors,
				 NULL, /* documenters */
				 NULL, /* translator_credits */
				 pixbuf);
	
	gtk_window_set_wmclass (GTK_WINDOW (about), "pager", "Pager");

	if (pixbuf) {
		gtk_window_set_icon (GTK_WINDOW (about), pixbuf);
		g_object_unref (pixbuf);
	}
	
	g_signal_connect (G_OBJECT(about), "destroy",
			  (GCallback)gtk_widget_destroyed, &about);
	
	gtk_widget_show (about);
}


#define WID(s) glade_xml_get_widget (xml, s)

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
		workspace = wnck_workspace_get (i);
		gtk_list_store_append (pager->workspaces_store, &iter);
		gtk_list_store_set (pager->workspaces_store,
				    &iter,
				    0, wnck_workspace_get_name (workspace),
				    -1);
	}
}

static void
workspace_created_or_destroyed (WnckScreen *screen,
				WnckWorkspace *space,
				PagerData *pager)
{
	update_workspaces_model (pager);
}

static void
num_workspaces_value_changed (GtkSpinButton *button,
			      PagerData       *pager)
{
	GConfClient* client;
	
	client = gconf_client_get_default ();

	gconf_client_set_int (client, "/desktop/gnome/applications/window_manager/number_of_workspaces",
			      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pager->num_workspaces_spin)), NULL);
}

static void 
workspace_name_edited (GtkCellRendererText *cell_renderer_text,
		       const gchar         *path,
		       const gchar         *new_text,
		       PagerData           *pager)
{
	GConfClient* client;
	GtkTreeIter iter;
	char *str;
	GSList *list;

	if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (pager->workspaces_store),
						 &iter, path)) {
		gtk_list_store_set (pager->workspaces_store,
				    &iter,
				    0, new_text,
				    -1);
	}

	
	list = NULL;
	gtk_tree_model_get_iter_root (GTK_TREE_MODEL (pager->workspaces_store), &iter);
	do {
		gtk_tree_model_get (GTK_TREE_MODEL (pager->workspaces_store),
				    &iter,
				    0, &str,
				    -1);
		list = g_slist_prepend (list, str);
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (pager->workspaces_store), &iter));
	
	list = g_slist_reverse (list);

	client = gconf_client_get_default ();
	gconf_client_set_list (client,  "/desktop/gnome/applications/window_manager/workspace_names",
			       GCONF_VALUE_STRING,
			       list,
			       NULL);

	g_slist_foreach (list, (GFunc)g_free, NULL);
	g_slist_free (list);
}

static void
setup_dialog (GladeXML  *xml,
	      PagerData *pager)
{
	gboolean value;
	GConfClient *client;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	
	client = gconf_client_get_default ();

	pager->display_workspaces_toggle = WID ("workspace_name_toggle");
	pager->all_workspaces_radio = WID ("all_workspaces_radio");
	pager->current_only_radio = WID ("current_only_radio");
	pager->num_rows_spin = WID ("num_rows_spin");
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

	
	g_signal_connect_swapped (WID ("done_button"), "pressed",
				  (GCallback) gtk_widget_hide, pager->properties_dialog);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pager->num_workspaces_spin),
				   wnck_screen_get_workspace_count (pager->screen));
	g_signal_connect (G_OBJECT (pager->num_workspaces_spin), "value_changed",
			  (GCallback) num_workspaces_value_changed, pager);
	
	g_signal_connect_swapped (pager->screen, "workspace_created",
				  (GCallback) workspace_created_or_destroyed, pager);
	g_signal_connect_swapped (pager->screen, "workspace_destroyed",
				  (GCallback) workspace_created_or_destroyed, pager);

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

		setup_dialog (xml, pager);
		
		g_object_unref (G_OBJECT (xml));
	}

	gtk_window_present (GTK_WINDOW (pager->properties_dialog));
}
