/*
 * Copyright (C) 2001 Red Hat, Inc
 * Copyright (C) 2001 Alexander Larsson
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Alexander Larsson
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libwnck/libwnck.h>
#include <stdlib.h>
#include <string.h>

#include "workspace-switcher.h"
#include "wncklet.h"

#define NEVER_SENSITIVE "never_sensitive"
#define WORKSPACE_SWITCHER_ICON "gnome-panel-workspace-switcher"

typedef enum {
	PAGER_WM_METACITY,
	PAGER_WM_COMPIZ,
	PAGER_WM_UNKNOWN
} PagerWM;

struct _WorkspaceSwitcherApplet
{
	GpApplet parent;

	WnckHandle *handle;
	GtkWidget *pager;

	WnckScreen *screen;
	PagerWM     wm;

	/* Properties: */
	GtkWidget *properties_dialog;
	GtkWidget *workspaces_frame;
	GtkWidget *workspace_names_label;
	GtkWidget *workspace_names_scroll;
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

	GSettings *settings;
};

G_DEFINE_TYPE (WorkspaceSwitcherApplet, workspace_switcher_applet, GP_TYPE_APPLET)

static void
pager_update (WorkspaceSwitcherApplet *pager)
{
	wnck_pager_set_orientation (WNCK_PAGER (pager->pager),
				    pager->orientation);
	wnck_pager_set_n_rows (WNCK_PAGER (pager->pager),
			       pager->n_rows);
	wnck_pager_set_show_all (WNCK_PAGER (pager->pager),
				 pager->display_all);

	if (pager->wm == PAGER_WM_METACITY)
		wnck_pager_set_display_mode (WNCK_PAGER (pager->pager),
					     pager->display_mode);
	else
		wnck_pager_set_display_mode (WNCK_PAGER (pager->pager),
					     WNCK_PAGER_DISPLAY_CONTENT);
}

static void
update_properties_for_wm (WorkspaceSwitcherApplet *pager)
{
	switch (pager->wm) {
	case PAGER_WM_METACITY:
		if (pager->workspaces_frame)
			gtk_widget_show (pager->workspaces_frame);
		if (pager->workspace_names_label)
			gtk_widget_show (pager->workspace_names_label);
		if (pager->workspace_names_scroll)
			gtk_widget_show (pager->workspace_names_scroll);
		if (pager->display_workspaces_toggle)
			gtk_widget_show (pager->display_workspaces_toggle);
		break;
	case PAGER_WM_COMPIZ:
		if (pager->workspaces_frame)
			gtk_widget_show (pager->workspaces_frame);
		if (pager->workspace_names_label)
			gtk_widget_hide (pager->workspace_names_label);
		if (pager->workspace_names_scroll)
			gtk_widget_hide (pager->workspace_names_scroll);
		if (pager->display_workspaces_toggle)
			gtk_widget_hide (pager->display_workspaces_toggle);
		break;
	case PAGER_WM_UNKNOWN:
		if (pager->workspaces_frame)
			gtk_widget_hide (pager->workspaces_frame);
		break;
	default:
		g_assert_not_reached ();
	}

	if (pager->properties_dialog) {
	        gtk_widget_hide (pager->properties_dialog);
	        gtk_widget_unrealize (pager->properties_dialog);
	        gtk_widget_show (pager->properties_dialog);
	}
}

static void
window_manager_changed (WnckScreen              *screen,
                        WorkspaceSwitcherApplet *pager)
{
	const char *wm_name;

	wm_name = wnck_screen_get_window_manager_name (screen);

	if (!wm_name)
		pager->wm = PAGER_WM_UNKNOWN;
	else if (strcmp (wm_name, "Metacity") == 0)
		pager->wm = PAGER_WM_METACITY;
	else if (strcmp (wm_name, "Compiz") == 0)
		pager->wm = PAGER_WM_COMPIZ;
	else
		pager->wm = PAGER_WM_UNKNOWN;

	update_properties_for_wm (pager);
	pager_update (pager);
}

static void
applet_realized (GtkWidget               *widget,
                 WorkspaceSwitcherApplet *pager)
{
	pager->screen = wnck_handle_get_default_screen (pager->handle);

	window_manager_changed (pager->screen, pager);
	wncklet_connect_while_alive (pager->screen, "window_manager_changed",
				     G_CALLBACK (window_manager_changed),
				     pager, pager);
}

static void
applet_unrealized (GtkWidget               *widget,
                   WorkspaceSwitcherApplet *pager)
{
	pager->screen = NULL;
	pager->wm = PAGER_WM_UNKNOWN;
}

static void
destroy_pager (GtkWidget               *widget,
               WorkspaceSwitcherApplet *pager)
{
	g_object_unref (G_OBJECT (pager->settings));

	if (pager->properties_dialog)
		gtk_widget_destroy (pager->properties_dialog);
}

static void
num_rows_changed (GSettings               *settings,
                  const gchar             *key,
                  WorkspaceSwitcherApplet *pager)
{
	int n_rows;

        n_rows = g_settings_get_int (settings, key);
        
	pager->n_rows = n_rows;
	pager_update (pager);

	if (pager->num_rows_spin &&
	    gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pager->num_rows_spin)) != n_rows)
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (pager->num_rows_spin), pager->n_rows);
}

static void
display_workspace_names_changed (GSettings               *settings,
                                 const gchar             *key,
                                 WorkspaceSwitcherApplet *pager)
{
	gboolean value;
       
	value = g_settings_get_boolean (settings, key);

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
all_workspaces_changed (GSettings               *settings,
                        const gchar             *key,
                        WorkspaceSwitcherApplet *pager)
{
	gboolean value;

	value = g_settings_get_boolean (settings, key);

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
		if ( ! g_object_get_data (G_OBJECT (pager->num_rows_spin), NEVER_SENSITIVE))
			gtk_widget_set_sensitive (pager->num_rows_spin, value);
	}
}

static void
setup_gsettings (WorkspaceSwitcherApplet *pager)
{
	pager->settings =
	  gp_applet_settings_new (GP_APPLET (pager),
				     "org.gnome.gnome-panel.applet.workspace-switcher");

	g_signal_connect (pager->settings, "changed::num-rows",
			  G_CALLBACK (num_rows_changed), pager);
	g_signal_connect (pager->settings, "changed::display-workspace-names",
			  G_CALLBACK (display_workspace_names_changed), pager);
	g_signal_connect (pager->settings, "changed::display-all-workspaces",
			  G_CALLBACK (all_workspaces_changed), pager);
}

static void
display_workspace_names_toggled (GtkToggleButton         *button,
                                 WorkspaceSwitcherApplet *pager)
{
	g_settings_set_boolean (pager->settings,
				"display-workspace-names",
				gtk_toggle_button_get_active (button));
}

static void
all_workspaces_toggled (GtkToggleButton         *button,
                        WorkspaceSwitcherApplet *pager)
{
  	g_settings_set_boolean (pager->settings,
				"display-all-workspaces",
				gtk_toggle_button_get_active (button));
}

static void
num_rows_value_changed (GtkSpinButton           *button,
                        WorkspaceSwitcherApplet *pager)
{
	g_settings_set_int (pager->settings,
			    "num-rows",
			    gtk_spin_button_get_value_as_int (button));
}

static void
update_workspaces_model (WorkspaceSwitcherApplet *pager)
{
	int nr_ws, i;
	WnckWorkspace *workspace;
	GtkTreeIter iter;

	nr_ws = wnck_screen_get_workspace_count (pager->screen);
        
	if (pager->properties_dialog) {
		if (nr_ws != gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pager->num_workspaces_spin)))
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
}

static void
workspace_renamed (WnckWorkspace           *space,
                   WorkspaceSwitcherApplet *pager)
{
	int         i;
	GtkTreeIter iter;

	i = wnck_workspace_get_number (space);
	if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (pager->workspaces_store),
					   &iter, NULL, i))
		gtk_list_store_set (pager->workspaces_store,
				    &iter,
				    0, wnck_workspace_get_name (space),
				    -1);
}

static void
workspace_created (WnckScreen              *screen,
                   WnckWorkspace           *space,
                   WorkspaceSwitcherApplet *pager)
{
        g_return_if_fail (WNCK_IS_SCREEN (screen));
        
	update_workspaces_model (pager);

	wncklet_connect_while_alive (space, "name_changed",
				     G_CALLBACK(workspace_renamed),
				     pager,
				     pager->properties_dialog);

}

static void
workspace_destroyed (WnckScreen              *screen,
                     WnckWorkspace           *space,
                     WorkspaceSwitcherApplet *pager)
{
        g_return_if_fail (WNCK_IS_SCREEN (screen));
	update_workspaces_model (pager);
}

static void
num_workspaces_value_changed (GtkSpinButton           *button,
                              WorkspaceSwitcherApplet *pager)
{
#if 0
	/* Slow down a bit after the first change, since it's moving really to
	 * fast. See bug #336731 for background.
	 * FIXME: remove this if bug 410520 gets fixed. */
	button->timer_step = 0.2;
#endif

        wnck_screen_change_workspace_count (pager->screen,
                                            gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pager->num_workspaces_spin)));
}

static gboolean
workspaces_tree_focused_out (GtkTreeView             *treeview,
                             GdkEventFocus           *event,
                             WorkspaceSwitcherApplet *pager)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_unselect_all (selection);
	return TRUE;
}

static void 
workspace_name_edited (GtkCellRendererText     *cell_renderer_text,
                       const gchar             *path,
                       const gchar             *new_text,
                       WorkspaceSwitcherApplet *pager)
{
        const gint *indices;
        WnckWorkspace *workspace;
        GtkTreePath *p;

        p = gtk_tree_path_new_from_string (path);
        indices = gtk_tree_path_get_indices (p);
        workspace = wnck_screen_get_workspace (pager->screen,
                                               indices[0]);
        if (workspace != NULL) {
                gchar* temp_name = g_strdup(new_text);

                wnck_workspace_change_name (workspace,
                                            g_strstrip(temp_name));
                g_free (temp_name);
        }
        else
                g_warning ("Edited name of workspace %d which no longer exists",
                           indices[0]);

        gtk_tree_path_free (p);
}

static void
properties_dialog_destroyed (GtkWidget               *widget,
                             WorkspaceSwitcherApplet *pager)
{
	pager->properties_dialog = NULL;
	pager->workspaces_frame = NULL;
	pager->workspace_names_label = NULL;
	pager->workspace_names_scroll = NULL;
	pager->display_workspaces_toggle = NULL;
	pager->all_workspaces_radio = NULL;
	pager->current_only_radio = NULL;
	pager->num_rows_spin = NULL;
	pager->label_row_col = NULL;
	pager->num_workspaces_spin = NULL;
	pager->workspaces_tree = NULL;
	pager->workspaces_store = NULL;
}

static gboolean
delete_event (GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy (widget);
	return TRUE;
}

static void 
response_cb (GtkWidget               *widget,
             gint                     id,
             WorkspaceSwitcherApplet *pager)
{
	gtk_widget_destroy (widget);
}

static void
close_dialog (GtkWidget *button,
              gpointer data)
{
	WorkspaceSwitcherApplet *pager = data;
	GtkTreeViewColumn *col;
	GtkCellArea *area;
	GtkCellEditable *edit_widget;

	/* This is a hack. The "editable" signal for GtkCellRenderer is emitted
	only on button press or focus cycle. Hence when the user changes the
	name and closes the preferences dialog without a button-press he would
	lose the name changes. So, we call the gtk_cell_editable_editing_done
	to stop the editing. Thanks to Paolo for a better crack than the one I had.
	*/

	col = gtk_tree_view_get_column (GTK_TREE_VIEW (pager->workspaces_tree), 0);
	area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (col));
	edit_widget = gtk_cell_area_get_edit_widget (area);

	if (edit_widget)
		gtk_cell_editable_editing_done (edit_widget);

	gtk_widget_destroy (pager->properties_dialog);
}

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

static void
setup_sensitivity (WorkspaceSwitcherApplet *pager,
                   GtkBuilder              *builder,
                   const gchar             *wid1,
                   const gchar             *wid2,
                   const gchar             *wid3,
                   const gchar             *key)
{
	GtkWidget *w;

	if (g_settings_is_writable (pager->settings, key)) {
		return;
	}

	w = WID (wid1);
	g_assert (w != NULL);
	g_object_set_data (G_OBJECT (w), NEVER_SENSITIVE,
			   GINT_TO_POINTER (1));
	gtk_widget_set_sensitive (w, FALSE);

	if (wid2 != NULL) {
		w = WID (wid2);
		g_assert (w != NULL);
		g_object_set_data (G_OBJECT (w), NEVER_SENSITIVE,
				   GINT_TO_POINTER (1));
		gtk_widget_set_sensitive (w, FALSE);
	}
	if (wid3 != NULL) {
		w = WID (wid3);
		g_assert (w != NULL);
		g_object_set_data (G_OBJECT (w), NEVER_SENSITIVE,
				   GINT_TO_POINTER (1));
		gtk_widget_set_sensitive (w, FALSE);
	}

}

static void
setup_dialog (GtkBuilder              *builder,
              WorkspaceSwitcherApplet *pager)
{
	gboolean value;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	int nr_ws, i;
	
	pager->workspaces_frame = WID ("workspaces_frame");
	pager->workspace_names_label = WID ("workspace_names_label");
	pager->workspace_names_scroll = WID ("workspace_names_scroll");

	pager->display_workspaces_toggle = WID ("workspace_name_toggle");
	setup_sensitivity (pager, builder,
			   "workspace_name_toggle",
			   NULL,
			   NULL,
			   "display-workspace-names" /* key */);

	pager->all_workspaces_radio = WID ("all_workspaces_radio");
	pager->current_only_radio = WID ("current_only_radio");
	setup_sensitivity (pager, builder,
			   "all_workspaces_radio",
			   "current_only_radio",
			   "label_row_col",
			   "display-all-workspaces" /* key */);

	pager->num_rows_spin = WID ("num_rows_spin");
	pager->label_row_col = WID("label_row_col");
	setup_sensitivity (pager, builder,
			   "num_rows_spin",
			   NULL,
			   NULL,
			   "num-rows" /* key */);

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
		if ( ! g_object_get_data (G_OBJECT (pager->num_rows_spin), NEVER_SENSITIVE))
			gtk_widget_set_sensitive (pager->num_rows_spin, TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pager->current_only_radio), TRUE);
		gtk_widget_set_sensitive (pager->num_rows_spin, FALSE);
	}
		
	/* Num rows: */
	g_signal_connect (G_OBJECT (pager->num_rows_spin), "value_changed",
			  (GCallback) num_rows_value_changed, pager);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pager->num_rows_spin), pager->n_rows);
	gtk_label_set_text (GTK_LABEL (pager->label_row_col), pager->orientation == GTK_ORIENTATION_HORIZONTAL ? _("rows") : _("columns"));

	g_signal_connect (pager->properties_dialog, "destroy",
			  G_CALLBACK (properties_dialog_destroyed),
			  pager);
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
	
	wncklet_connect_while_alive (pager->screen, "workspace_created",
				     G_CALLBACK(workspace_created),
				     pager,
				     pager->properties_dialog);

	wncklet_connect_while_alive (pager->screen, "workspace_destroyed",
				     G_CALLBACK(workspace_destroyed),
				     pager,
				     pager->properties_dialog);

	g_signal_connect (G_OBJECT (pager->workspaces_tree), "focus_out_event",
			  (GCallback) workspaces_tree_focused_out, pager);

	pager->workspaces_store = gtk_list_store_new (1, G_TYPE_STRING, NULL);
	update_workspaces_model (pager);
	gtk_tree_view_set_model (GTK_TREE_VIEW (pager->workspaces_tree), GTK_TREE_MODEL (pager->workspaces_store));

	g_object_unref (pager->workspaces_store);

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
		wncklet_connect_while_alive (
				G_OBJECT (wnck_screen_get_workspace (pager->screen, i)),
			   	"name_changed",
				G_CALLBACK(workspace_renamed),
				pager,
				pager->properties_dialog);
	}

	update_properties_for_wm (pager);
}

static void 
display_properties_dialog (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
	WorkspaceSwitcherApplet *pager = (WorkspaceSwitcherApplet *) user_data;

	if (pager->properties_dialog == NULL) {
		GtkBuilder *builder;

		builder = gtk_builder_new ();
		gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
		gtk_builder_add_from_resource (builder, WNCKLET_RESOURCE_PATH "workspace-switcher.ui", NULL);

		pager->properties_dialog = WID ("pager_properties_dialog");

		g_object_add_weak_pointer (G_OBJECT (pager->properties_dialog), 
					   (gpointer *) &pager->properties_dialog);

		setup_dialog (builder, pager);
		
		g_object_unref (builder);
	}

	gtk_window_set_icon_name (GTK_WINDOW (pager->properties_dialog),
	                          WORKSPACE_SWITCHER_ICON);
	gtk_window_present (GTK_WINDOW (pager->properties_dialog));
}

static const GActionEntry pager_menu_actions [] = {
        { "preferences", display_properties_dialog, NULL, NULL, NULL },
        { NULL }
};

static void
workspace_switcher_applet_fill (GpApplet *applet)
{
	WorkspaceSwitcherApplet *pager;
	GAction *action;
	gboolean display_names;

	pager = WORKSPACE_SWITCHER_APPLET (applet);

	setup_gsettings (pager);

	pager->n_rows = g_settings_get_int (pager->settings, "num-rows");

	display_names = g_settings_get_boolean (pager->settings, "display-workspace-names");

	if (display_names) {
		pager->display_mode = WNCK_PAGER_DISPLAY_NAME;
	} else {
		pager->display_mode = WNCK_PAGER_DISPLAY_CONTENT;
	}

	pager->display_all = g_settings_get_boolean (pager->settings, "display-all-workspaces");

	pager->orientation = gp_applet_get_orientation (applet);

	pager->handle = wnck_handle_new (WNCK_CLIENT_TYPE_PAGER);
	pager->pager = wnck_pager_new_with_handle (pager->handle);
	pager->screen = NULL;
	pager->wm = PAGER_WM_UNKNOWN;
	wnck_pager_set_shadow_type (WNCK_PAGER (pager->pager), GTK_SHADOW_IN);

	g_signal_connect (G_OBJECT (pager->pager), "destroy",
			  G_CALLBACK (destroy_pager),
			  pager);

	gtk_container_add (GTK_CONTAINER (pager), pager->pager);
	gtk_widget_show (pager->pager);

	g_signal_connect (G_OBJECT (pager),
			  "realize",
			  G_CALLBACK (applet_realized),
			  pager);
	g_signal_connect (G_OBJECT (pager),
			  "unrealize",
			  G_CALLBACK (applet_unrealized),
			  pager);

	gp_applet_setup_menu_from_resource (applet,
	                                    WNCKLET_RESOURCE_PATH "workspace-switcher-menu.ui",
	                                    pager_menu_actions);

	action = gp_applet_menu_lookup_action (applet, "preferences");
	g_object_bind_property (pager, "locked-down", action, "enabled",
				G_BINDING_DEFAULT|G_BINDING_INVERT_BOOLEAN|G_BINDING_SYNC_CREATE);

	gtk_widget_show (GTK_WIDGET (pager));
}

static void
workspace_switcher_applet_constructed (GObject *object)
{
	G_OBJECT_CLASS (workspace_switcher_applet_parent_class)->constructed (object);

	workspace_switcher_applet_fill (GP_APPLET (object));
}

static void
workspace_switcher_applet_dispose (GObject *object)
{
	WorkspaceSwitcherApplet *pager;

	pager = WORKSPACE_SWITCHER_APPLET (object);

	g_clear_object (&pager->handle);

	G_OBJECT_CLASS (workspace_switcher_applet_parent_class)->dispose (object);
}

static void
workspace_switcher_applet_placement_changed (GpApplet        *applet,
                                             GtkOrientation   orientation,
                                             GtkPositionType  position)
{
	WorkspaceSwitcherApplet *pager;

	pager = WORKSPACE_SWITCHER_APPLET (applet);

	if (orientation == pager->orientation)
		return;

	pager->orientation = orientation;
	pager_update (pager);

	if (pager->label_row_col)
		gtk_label_set_text (GTK_LABEL (pager->label_row_col), orientation == GTK_ORIENTATION_HORIZONTAL ? _("rows") : _("columns"));
}

static void
workspace_switcher_applet_class_init (WorkspaceSwitcherAppletClass *pager_class)
{
	GObjectClass *object_class;
	GpAppletClass *applet_class;

	object_class = G_OBJECT_CLASS (pager_class);
	applet_class = GP_APPLET_CLASS (pager_class);

	object_class->constructed = workspace_switcher_applet_constructed;
	object_class->dispose = workspace_switcher_applet_dispose;

	applet_class->placement_changed = workspace_switcher_applet_placement_changed;
}

static void
workspace_switcher_applet_init (WorkspaceSwitcherApplet *pager)
{
	gp_applet_set_flags (GP_APPLET (pager), GP_APPLET_FLAGS_EXPAND_MINOR);
}
