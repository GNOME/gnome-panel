/* -*- c-basic-offset: 8; indent-tabs-mode: t -*-
 * panel-addto.c:
 *
 * Copyright (C) 2004 Vincent Untz
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
 * Authors:
 *	Vincent Untz <vincent@vuntz.net>
 */

#include <config.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gdk/gdkx.h>

#include <libpanel-util/panel-glib.h>

#include "panel.h"
#include "panel-applets-manager.h"
#include "panel-applet-frame.h"
#include "panel-action-button.h"
#include "panel-toplevel.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-util.h"
#include "panel-addto-dialog.h"
#include "panel-icon-names.h"

struct _PanelAddtoDialog
{
	GtkWindow    parent;
	PanelWidget *panel_widget;

	GtkWidget    *dialog_vbox;
	GtkWidget    *label;
	GtkWidget    *search_entry;
	GtkWidget    *add_button;
	GtkWidget    *close_button;
	GtkWidget    *tree_view;

	GtkTreeModel *applet_model;
	GtkTreeModel *filter_applet_model;

	GSList       *applet_list;

	gchar        *search_text;
	gchar        *applet_search_text;

	PanelObjectPackType insert_pack_type;
};

G_DEFINE_TYPE (PanelAddtoDialog, panel_addto_dialog, GTK_TYPE_WINDOW)

static GQuark panel_addto_dialog_quark = 0;

typedef enum {
	PANEL_ADDTO_APPLET,
	PANEL_ADDTO_ACTION
} PanelAddtoItemType;

typedef struct {
	PanelAddtoItemType     type;
	char                  *name;
	char                  *description;
	GIcon                 *icon;
	PanelActionButtonType  action_type;
	char                  *iid;
} PanelAddtoItemInfo;

typedef struct {
	GSList             *children;
	PanelAddtoItemInfo  item_info;
} PanelAddtoAppList;

enum {
	COLUMN_ICON,
	COLUMN_TEXT,
	COLUMN_DATA,
	COLUMN_SEARCH,
	NUMBER_COLUMNS
};

static gboolean panel_addto_filter_func (GtkTreeModel *model,
					 GtkTreeIter  *iter,
					 gpointer      data);

static int
panel_addto_applet_info_sort_func (PanelAddtoItemInfo *a,
				   PanelAddtoItemInfo *b)
{
	return g_utf8_collate (a->name, b->name);
}

static GSList *
panel_addto_prepend_internal_applets (GSList *list)
{
	int i;

	for (i = 1; i < PANEL_ACTION_LAST; i++) {
		PanelAddtoItemInfo *info;

		if (i == PANEL_ACTION_REBOOT || i == PANEL_ACTION_HYBRID_SLEEP
		    || i == PANEL_ACTION_SUSPEND || i == PANEL_ACTION_HIBERNATE)
			continue;

		if (panel_action_get_is_disabled (i))
			continue;

		info              = g_new0 (PanelAddtoItemInfo, 1);
		info->type        = PANEL_ADDTO_ACTION;
		info->action_type = i;
		info->name        = g_strdup (panel_action_get_text (i));
		info->description = g_strdup (panel_action_get_tooltip (i));
		if (panel_action_get_icon_name (i) != NULL)
			info->icon = g_themed_icon_new (panel_action_get_icon_name (i));
		info->iid         = g_strdup (panel_action_get_drag_id (i));

		list = g_slist_prepend (list, info);
	}

        return list;
}

static char *
panel_addto_make_text (const char *name,
		       const char *desc)
{
	const char *real_name;
	char       *result; 

	real_name = name ? name : _("(empty)");

	if (!PANEL_GLIB_STR_EMPTY (desc)) {
		result = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>\n%s",
						  real_name, desc);
	} else {
		result = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>",
						  real_name);
	}

	return result;
}

static void  
panel_addto_drag_data_get_cb (GtkWidget        *widget,
			      GdkDragContext   *context,
			      GtkSelectionData *selection_data,
			      guint             info,
			      guint             time,
			      const char       *string)
{
	gtk_selection_data_set (selection_data,
				gtk_selection_data_get_target (selection_data), 8,
				(guchar *) string, strlen (string));
}

static void
panel_addto_drag_begin_cb (GtkWidget      *widget,
			   GdkDragContext *context,
			   gpointer        data)
{
	GtkTreeModel *filter_model;
	GtkTreeModel *child_model;
	GtkTreePath  *path;
	GtkTreeIter   iter;
	GtkTreeIter   filter_iter;
	GIcon        *gicon;

	filter_model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	   
	gtk_tree_view_get_cursor (GTK_TREE_VIEW (widget), &path, NULL);
	gtk_tree_model_get_iter (filter_model, &filter_iter, path);
	gtk_tree_path_free (path);
	gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (filter_model),
	                                                  &iter, &filter_iter);

	child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (filter_model));
	gtk_tree_model_get (child_model, &iter,
	                    COLUMN_ICON, &gicon,
	                    -1);

	gtk_drag_set_icon_gicon (context, gicon, 0, 0);
	g_object_unref (gicon);
}

static void
free_drag_text (gpointer  data,
                GClosure *closure)
{
	g_free (data);
}

static void
panel_addto_setup_drag (GtkTreeView          *tree_view,
			const GtkTargetEntry *target,
			const char           *text)
{
	if (!text || panel_lockdown_get_panels_locked_down_s ())
		return;
	
	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (tree_view),
						GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
						target, 1, GDK_ACTION_COPY);
	
	g_signal_connect_data (G_OBJECT (tree_view), "drag_data_get",
			       G_CALLBACK (panel_addto_drag_data_get_cb),
			       g_strdup (text),
			       free_drag_text,
			       0 /* connect_flags */);
	g_signal_connect_after (G_OBJECT (tree_view), "drag-begin",
	                        G_CALLBACK (panel_addto_drag_begin_cb),
	                        NULL);
}

static void
panel_addto_setup_applet_drag (GtkTreeView *tree_view,
			       const char  *iid)
{
	static const GtkTargetEntry target[] = {
		{ (gchar *) "application/x-panel-applet-iid", 0, 0 }
	};

	panel_addto_setup_drag (tree_view, target, iid);
}

static void
panel_addto_setup_internal_applet_drag (GtkTreeView *tree_view,
					const char  *applet_type)
{
	static const GtkTargetEntry target[] = {
		{ (gchar *) "application/x-panel-applet-internal", 0, 0 }
	};

	panel_addto_setup_drag (tree_view, target, applet_type);
}

static GSList *
panel_addto_query_applets (GSList *list)
{
	GList *applet_list, *l;

	applet_list = panel_applets_manager_get_applets ();

	for (l = applet_list; l; l = g_list_next (l)) {
		PanelAppletInfo *info;
		const char *iid, *name, *description, *icon;
		PanelAddtoItemInfo *applet;

		info = (PanelAppletInfo *)l->data;

		iid = panel_applet_info_get_iid (info);
		name = panel_applet_info_get_name (info);
		description = panel_applet_info_get_description (info);
		icon = panel_applet_info_get_icon (info);

		if (!name || panel_applets_manager_is_applet_disabled (iid, NULL)) {
			continue;
		}

		applet = g_new0 (PanelAddtoItemInfo, 1);
		applet->type = PANEL_ADDTO_APPLET;
		applet->name = g_strdup (name);
		applet->description = g_strdup (description);
		if (icon)
			applet->icon = g_themed_icon_new (icon);
		applet->iid = g_strdup (iid);

		list = g_slist_prepend (list, applet);
	}

	g_list_free (applet_list);

	return list;
}

static void
panel_addto_append_item (PanelAddtoDialog *dialog,
			 GtkListStore *model,
			 PanelAddtoItemInfo *applet)
{
	char *text;
	GtkTreeIter iter;

	if (applet == NULL) {
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    COLUMN_ICON, NULL,
				    COLUMN_TEXT, NULL,
				    COLUMN_DATA, NULL,
				    COLUMN_SEARCH, NULL,
				    -1);
	} else {
		gtk_list_store_append (model, &iter);

		text = panel_addto_make_text (applet->name,
					      applet->description);

		gtk_list_store_set (model, &iter,
				    COLUMN_ICON, applet->icon,
				    COLUMN_TEXT, text,
				    COLUMN_DATA, applet,
				    COLUMN_SEARCH, applet->name,
				    -1);

		g_free (text);
	}
}

static void
panel_addto_make_applet_model (PanelAddtoDialog *dialog)
{
	GtkListStore *model;
	GSList       *l;

	if (dialog->filter_applet_model != NULL)
		return;

	if (panel_layout_is_writable ()) {
		dialog->applet_list = panel_addto_query_applets (dialog->applet_list);
		dialog->applet_list = panel_addto_prepend_internal_applets (dialog->applet_list);
	}

	dialog->applet_list = g_slist_sort (dialog->applet_list,
					    (GCompareFunc) panel_addto_applet_info_sort_func);

	model = gtk_list_store_new (NUMBER_COLUMNS,
				    G_TYPE_ICON,
				    G_TYPE_STRING,
				    G_TYPE_POINTER,
				    G_TYPE_STRING);

	if (panel_layout_is_writable ()) {
		if (dialog->applet_list)
			panel_addto_append_item (dialog, model, NULL);
	}

	for (l = dialog->applet_list; l; l = l->next)
		panel_addto_append_item (dialog, model, l->data);

	dialog->applet_model = GTK_TREE_MODEL (model);
	dialog->filter_applet_model = gtk_tree_model_filter_new (GTK_TREE_MODEL (dialog->applet_model),
								 NULL);
	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (dialog->filter_applet_model),
						panel_addto_filter_func,
						dialog, NULL);
}

typedef struct
{
	PanelAddtoDialog *dialog;
	gchar            *iid;
} InitialSetupData;

static InitialSetupData *
initial_setup_data_new (PanelAddtoDialog *dialog,
                        const gchar      *iid)
{
	InitialSetupData *data;

	data = g_new0 (InitialSetupData, 1);

	data->dialog = g_object_ref (dialog);
	data->iid = g_strdup (iid);

	return data;
}

static void
initial_setup_data_free (gpointer user_data)
{
	InitialSetupData *data;

	data = (InitialSetupData *) user_data;

	g_object_unref (data->dialog);
	g_free (data->iid);
	g_free (data);
}

static void
initial_setup_dialog_cb (GpInitialSetupDialog *dialog,
                         gboolean              canceled,
                         gpointer              user_data)
{
	InitialSetupData *data;
	int pack_index;
	GVariant *initial_settings;

	data = (InitialSetupData *) user_data;

	if (canceled)
		return;

	pack_index = panel_widget_get_new_pack_index (data->dialog->panel_widget,
	                                              data->dialog->insert_pack_type);

	initial_settings = gp_initital_setup_dialog_get_settings (dialog);

	panel_applet_frame_create (data->dialog->panel_widget->toplevel,
	                           data->dialog->insert_pack_type, pack_index,
	                           data->iid, initial_settings);

	g_variant_unref (initial_settings);
}

static void
panel_addto_add_item (PanelAddtoDialog   *dialog,
	 	      PanelAddtoItemInfo *item_info)
{
	int pack_index;
	InitialSetupData *data;

	g_assert (item_info != NULL);

	pack_index = panel_widget_get_new_pack_index (dialog->panel_widget,
						      dialog->insert_pack_type);

	switch (item_info->type) {
	case PANEL_ADDTO_APPLET:
		data = initial_setup_data_new (dialog, item_info->iid);

		if (!panel_applets_manager_open_initial_setup_dialog (item_info->iid,
		                                                      NULL,
		                                                      GTK_WINDOW (dialog),
		                                                      initial_setup_dialog_cb,
		                                                      data, initial_setup_data_free)) {
			panel_applet_frame_create (dialog->panel_widget->toplevel,
			                           dialog->insert_pack_type, pack_index,
			                           item_info->iid, NULL);
		}
		break;
	case PANEL_ADDTO_ACTION:
		panel_action_button_create (dialog->panel_widget->toplevel,
					    dialog->insert_pack_type,
					    pack_index,
					    item_info->action_type);
		break;
	default:
		break;
	}
}

static void
panel_addto_dialog_add_button_cb (PanelAddtoDialog *dialog,
                                  GtkWidget        *widget)
{
	GtkTreeSelection *selection;
	GtkTreeModel *filter_model;
	GtkTreeModel *child_model;
	GtkTreeIter iter;
	GtkTreeIter filter_iter;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
	if (gtk_tree_selection_get_selected (selection, &filter_model,
	                                     &filter_iter))
	{
		PanelAddtoItemInfo *data;

		gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (filter_model),
		                                                  &iter,
		                                                  &filter_iter);
		child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (filter_model));
		gtk_tree_model_get (child_model, &iter,
		                    COLUMN_DATA, &data, -1);

		if (data != NULL)
			panel_addto_add_item (dialog, data);
	}
}

static void
panel_addto_dialog_close_button_cb (PanelAddtoDialog *dialog,
                                    GtkWidget        *widget)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
panel_addto_present_applets (PanelAddtoDialog *dialog)
{
	if (dialog->filter_applet_model == NULL)
		panel_addto_make_applet_model (dialog);
	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->tree_view),
				 dialog->filter_applet_model);
	gtk_window_set_focus (GTK_WINDOW (dialog),
			      dialog->search_entry);

	if (dialog->applet_search_text) {
		gtk_entry_set_text (GTK_ENTRY (dialog->search_entry),
				    dialog->applet_search_text);
		gtk_editable_set_position (GTK_EDITABLE (dialog->search_entry),
					   -1);

		g_free (dialog->applet_search_text);
		dialog->applet_search_text = NULL;
	}
}

static void
panel_addto_dialog_free_item_info (PanelAddtoItemInfo *item_info)
{
	if (item_info == NULL)
		return;

	/* the GIcon is never static */
	g_clear_object (&item_info->icon);

	g_free (item_info->name);
	item_info->name = NULL;

	g_free (item_info->description);
	item_info->description = NULL;

	g_free (item_info->iid);
	item_info->iid = NULL;
}

static void
panel_addto_name_change (PanelAddtoDialog *dialog)
{
	const char *name;
	char       *label;

	name = panel_toplevel_get_name (dialog->panel_widget->toplevel);
	label = NULL;

	if (!PANEL_GLIB_STR_EMPTY (name))
		label = g_strdup_printf (_("Find an _item to add to \"%s\":"),
					 name);

	if (label == NULL)
		label = g_strdup (_("Find an _item to add to the panel:"));

	gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->label), label);
	g_free (label);
}

static void
panel_addto_name_notify (GObject          *gobject,
			 GParamSpec       *pspec,
			 PanelAddtoDialog *dialog)
{
	panel_addto_name_change (dialog);
}

static gboolean
panel_addto_filter_func (GtkTreeModel *model,
			 GtkTreeIter  *iter,
			 gpointer      userdata)
{
	PanelAddtoDialog   *dialog;
	PanelAddtoItemInfo *data;

	dialog = (PanelAddtoDialog *) userdata;

	if (!dialog->search_text || !dialog->search_text[0])
		return TRUE;

	gtk_tree_model_get (model, iter, COLUMN_DATA, &data, -1);

	if (data == NULL)
		return FALSE;

	/* This is more a workaround than anything else: show all the root
	 * items in a tree store */
	if (GTK_IS_TREE_STORE (model) &&
	    gtk_tree_store_iter_depth (GTK_TREE_STORE (model), iter) == 0)
		return TRUE;

	return (panel_g_utf8_strstrcase (data->name,
					 dialog->search_text) != NULL ||
	        panel_g_utf8_strstrcase (data->description,
					 dialog->search_text) != NULL);
}

static void
panel_addto_search_entry_changed (PanelAddtoDialog *dialog,
                                  GtkWidget        *entry)
{
	GtkTreeModel *model;
	char         *new_text;
	GtkTreeIter   iter;
	GtkTreePath  *path;

	new_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->search_entry)));
	g_strchomp (new_text);
		
	if (dialog->search_text &&
	    g_utf8_collate (new_text, dialog->search_text) == 0) {
		g_free (new_text);
		return;
	}

	if (dialog->search_text)
		g_free (dialog->search_text);
	dialog->search_text = new_text;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->tree_view));
	gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (model));

	path = gtk_tree_path_new_first ();
	if (gtk_tree_model_get_iter (model, &iter, path)) {
		GtkTreeSelection *selection;

		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (dialog->tree_view),
					      path, NULL, FALSE, 0, 0);
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
		gtk_tree_selection_select_path (selection, path);
	}
	gtk_tree_path_free (path);
}

static void
panel_addto_search_entry_activated (PanelAddtoDialog *dialog,
                                    GtkWidget        *entry)
{
  panel_addto_dialog_add_button_cb (dialog, entry);
}

static void
panel_addto_selection_changed (PanelAddtoDialog *dialog,
                               GtkTreeSelection *selection)
{
	GtkTreeModel       *filter_model;
	GtkTreeModel       *child_model;
	GtkTreeIter         iter;
	GtkTreeIter         filter_iter;
	PanelAddtoItemInfo *data;

	if (!gtk_tree_selection_get_selected (selection,
					      &filter_model,
					      &filter_iter)) {
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->add_button),
					  FALSE);
		return;
	}

	gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (filter_model),
	                                                  &iter, &filter_iter);
	child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (filter_model));
	gtk_tree_model_get (child_model, &iter, COLUMN_DATA, &data, -1);

	if (!data) {
		gtk_tree_view_unset_rows_drag_source (GTK_TREE_VIEW (dialog->tree_view));
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->add_button),
					  FALSE);
		return;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (dialog->add_button), TRUE);

	/* only allow dragging applets if we can add applets */
	if (panel_layout_is_writable ()) {
		if (data->type == PANEL_ADDTO_APPLET) {
			panel_addto_setup_applet_drag (GTK_TREE_VIEW (dialog->tree_view),
						       data->iid);
		} else if (data->type == PANEL_ADDTO_ACTION) {
			panel_addto_setup_internal_applet_drag (GTK_TREE_VIEW (dialog->tree_view),
							        data->iid);
		}
	}
}

static void
panel_addto_selection_activated (PanelAddtoDialog  *dialog,
                                 GtkTreeView       *view,
                                 GtkTreePath       *path,
                                 GtkTreeViewColumn *column)
{
  panel_addto_dialog_add_button_cb (dialog, dialog->add_button);
}

static gboolean
panel_addto_separator_func (GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer data)
{
	int column = GPOINTER_TO_INT (data);
	char *text;
	
	gtk_tree_model_get (model, iter, column, &text, -1);
	
	if (!text)
		return TRUE;
	
	g_free(text);
	return FALSE;
}

static PanelAddtoDialog *
panel_addto_dialog_new (PanelWidget *panel_widget)
{
	PanelAddtoDialog *dialog;

	dialog = g_object_new (PANEL_TYPE_ADDTO_DIALOG, NULL);

	g_object_set_qdata (G_OBJECT (panel_widget->toplevel),
	                    panel_addto_dialog_quark,
	                    dialog);

	dialog->panel_widget = panel_widget;

	g_signal_connect_object (dialog->panel_widget->toplevel,
	                         "notify::name",
	                         G_CALLBACK (panel_addto_name_notify),
	                         dialog, 0);

	gtk_widget_show_all (dialog->dialog_vbox);

	panel_toplevel_push_autohide_disabler (dialog->panel_widget->toplevel);
	panel_widget_register_open_dialog (panel_widget,
	                                   GTK_WIDGET (dialog));

	panel_addto_name_change (dialog);

	return dialog;
}

static void
panel_addto_dialog_constructed (GObject *object)
{
	PanelAddtoDialog *dialog;

	dialog = PANEL_ADDTO_DIALOG (object);

	G_OBJECT_CLASS (panel_addto_dialog_parent_class)->constructed (object);

	//FIXME use the same search than the one for the search entry?
	gtk_tree_view_set_search_column (GTK_TREE_VIEW (dialog->tree_view),
	                                 COLUMN_SEARCH);

	gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (dialog->tree_view),
	                                      panel_addto_separator_func,
	                                      GINT_TO_POINTER (COLUMN_TEXT),
	                                      NULL);
}

static void
panel_addto_dialog_dispose (GObject *object)
{
	PanelAddtoDialog *dialog;

	dialog = PANEL_ADDTO_DIALOG (object);

	g_clear_object (&dialog->filter_applet_model);
	g_clear_object (&dialog->applet_model);

	G_OBJECT_CLASS (panel_addto_dialog_parent_class)->dispose (object);
}

static void
panel_addto_dialog_finalize (GObject *object)
{
	PanelAddtoDialog *dialog;
	GSList *item;

	dialog = PANEL_ADDTO_DIALOG (object);

	panel_toplevel_pop_autohide_disabler (PANEL_TOPLEVEL (dialog->panel_widget->toplevel));
	g_object_set_qdata (G_OBJECT (dialog->panel_widget->toplevel),
	                    panel_addto_dialog_quark,
	                    NULL);

	g_free (dialog->search_text);
	dialog->search_text = NULL;

	g_free (dialog->applet_search_text);
	dialog->applet_search_text = NULL;

	for (item = dialog->applet_list; item != NULL; item = item->next) {
		PanelAddtoItemInfo *applet;

		applet = (PanelAddtoItemInfo *) item->data;
		panel_addto_dialog_free_item_info (applet);
		g_free (applet);
	}
	g_slist_free (dialog->applet_list);

	G_OBJECT_CLASS (panel_addto_dialog_parent_class)->finalize (object);
}

static void
panel_addto_dialog_class_init (PanelAddtoDialogClass *dialog_class)
{
	GtkWidgetClass *widget_class;
	GObjectClass *object_class;

	widget_class = GTK_WIDGET_CLASS (dialog_class);
	object_class = G_OBJECT_CLASS (dialog_class);

	object_class->constructed = panel_addto_dialog_constructed;
	object_class->dispose = panel_addto_dialog_dispose;
	object_class->finalize = panel_addto_dialog_finalize;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/panel/panel-addto-dialog.ui");

	gtk_widget_class_bind_template_child (widget_class, PanelAddtoDialog, dialog_vbox);
	gtk_widget_class_bind_template_child (widget_class, PanelAddtoDialog, label);
	gtk_widget_class_bind_template_child (widget_class, PanelAddtoDialog, search_entry);
	gtk_widget_class_bind_template_child (widget_class, PanelAddtoDialog, add_button);
	gtk_widget_class_bind_template_child (widget_class, PanelAddtoDialog, close_button);
	gtk_widget_class_bind_template_child (widget_class, PanelAddtoDialog, tree_view);

	gtk_widget_class_bind_template_callback (widget_class, panel_addto_dialog_add_button_cb);
	gtk_widget_class_bind_template_callback (widget_class, panel_addto_dialog_close_button_cb);
	gtk_widget_class_bind_template_callback (widget_class, panel_addto_search_entry_changed);
	gtk_widget_class_bind_template_callback (widget_class, panel_addto_search_entry_activated);
	gtk_widget_class_bind_template_callback (widget_class, panel_addto_selection_changed);
	gtk_widget_class_bind_template_callback (widget_class, panel_addto_selection_activated);
}

static void
panel_addto_dialog_init (PanelAddtoDialog *dialog)
{
	gtk_widget_init_template (GTK_WIDGET (dialog));
}

#define MAX_ADDTOPANEL_HEIGHT 490

void
panel_addto_present (GtkMenuItem *item,
		     PanelWidget *panel_widget)
{
	PanelAddtoDialog *dialog;
	PanelToplevel *toplevel;
	PanelObjectPackType insert_pack_type;
	GdkEvent *current_event;
	GdkScreen *screen;
	GdkDisplay *display;
	GdkWindow *window;
	GdkMonitor *monitor;
	GdkRectangle workarea;
	gint height;

	toplevel = panel_widget->toplevel;

	if (!panel_addto_dialog_quark)
		panel_addto_dialog_quark =
			g_quark_from_static_string ("panel-addto-dialog");

	dialog = g_object_get_qdata (G_OBJECT (toplevel),
				     panel_addto_dialog_quark);

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));
	display = gdk_screen_get_display (screen);
	window = gtk_widget_get_window (GTK_WIDGET (panel_widget));
	monitor = gdk_display_get_monitor_at_window (display, window);
	gdk_monitor_get_workarea (monitor, &workarea);
	height = MIN (MAX_ADDTOPANEL_HEIGHT, 3 * (workarea.height / 4));

	if (!dialog) {
		dialog = panel_addto_dialog_new (panel_widget);
		panel_addto_present_applets (dialog);
	}

	insert_pack_type = PANEL_OBJECT_PACK_START;
	current_event = gtk_get_current_event ();

	if (current_event != NULL) {
		if (current_event->type == GDK_BUTTON_PRESS)
			insert_pack_type = panel_widget_get_insert_pack_type_at_cursor (panel_widget);

		gdk_event_free (current_event);
	}

	dialog->insert_pack_type = insert_pack_type;
	gtk_window_set_screen (GTK_WINDOW (dialog), screen);
	gtk_window_set_default_size (GTK_WINDOW (dialog),
				     height * 8 / 7, height);
	gtk_window_present (GTK_WINDOW (dialog));
}
