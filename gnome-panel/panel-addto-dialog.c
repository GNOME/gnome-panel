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

#include <gmenu-tree.h>

#include <libpanel-util/panel-glib.h>

#include "launcher.h"
#include "menu.h"
#include "panel.h"
#include "panel-applets-manager.h"
#include "panel-applet-frame.h"
#include "panel-action-button.h"
#include "panel-menu-bar.h"
#include "panel-toplevel.h"
#include "panel-menu-button.h"
#include "panel-globals.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-util.h"
#include "panel-addto-dialog.h"
#include "panel-icon-names.h"
#include "panel-user-menu.h"

struct _PanelAddtoDialog
{
	GtkWindow    parent;
	PanelWidget *panel_widget;

	GtkWidget    *dialog_vbox;
	GtkWidget    *label;
	GtkWidget    *search_entry;
	GtkWidget    *back_button;
	GtkWidget    *add_button;
	GtkWidget    *close_button;
	GtkWidget    *tree_view;
	GtkWidget    *tree_view_selection;

	GtkTreeModel *applet_model;
	GtkTreeModel *filter_applet_model;
	GtkTreeModel *application_model;
	GtkTreeModel *filter_application_model;

	GMenuTree    *menu_tree;

	GSList       *applet_list;
	GSList       *application_list;
	GSList       *settings_list;

	gchar        *search_text;
	gchar        *applet_search_text;

	PanelObjectPackType insert_pack_type;
};

G_DEFINE_TYPE (PanelAddtoDialog, panel_addto_dialog, GTK_TYPE_WINDOW)

static GQuark panel_addto_dialog_quark = 0;

typedef enum {
	PANEL_ADDTO_APPLET,
	PANEL_ADDTO_ACTION,
	PANEL_ADDTO_LAUNCHER_MENU,
	PANEL_ADDTO_LAUNCHER,
	PANEL_ADDTO_LAUNCHER_NEW,
	PANEL_ADDTO_MENU,
	PANEL_ADDTO_MENUBAR,
	PANEL_ADDTO_USER_MENU
} PanelAddtoItemType;

typedef struct {
	PanelAddtoItemType     type;
	char                  *name;
	char                  *description;
	GIcon                 *icon;
	PanelActionButtonType  action_type;
	char                  *launcher_path;
	char                  *menu_filename;
	char                  *menu_path;
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

static void panel_addto_present_applications (PanelAddtoDialog *dialog);
static void panel_addto_present_applets      (PanelAddtoDialog *dialog);
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
prepend_internal_applets (GSList *list)
{
	PanelAddtoItemInfo *internal;

	internal = g_new0 (PanelAddtoItemInfo, 1);
	internal->type = PANEL_ADDTO_MENU;
	internal->name = g_strdup (_("Main Menu"));
	internal->description = g_strdup (_("The main GNOME menu"));
	internal->icon = g_themed_icon_new (PANEL_ICON_MAIN_MENU);
	internal->action_type = PANEL_ACTION_NONE;
	internal->iid = g_strdup ("MENU:MAIN");
	list = g_slist_prepend (list, internal);
	
	internal = g_new0 (PanelAddtoItemInfo, 1);
	internal->type = PANEL_ADDTO_MENUBAR;
	internal->name = g_strdup (_("Menu Bar"));
	internal->description = g_strdup (_("A custom menu bar"));
	internal->icon = g_themed_icon_new (PANEL_ICON_MAIN_MENU);
	internal->action_type = PANEL_ACTION_NONE;
	internal->iid = g_strdup ("MENUBAR:NEW");
	list = g_slist_prepend (list, internal);

	internal = g_new0 (PanelAddtoItemInfo, 1);
	internal->type = PANEL_ADDTO_USER_MENU;
	internal->name = g_strdup (_("User menu"));
	internal->description = g_strdup (_("Menu to change your settings and log out"));
	internal->icon = g_themed_icon_new (PANEL_ICON_COMPUTER);
	internal->action_type = PANEL_ACTION_NONE;
	internal->iid = g_strdup ("USERMENU:NEW");
	list = g_slist_prepend (list, internal);

	return list;
}

static GSList *
panel_addto_prepend_internal_applets (GSList *list)
{
	int i;

	list = prepend_internal_applets (list);

	for (i = PANEL_ACTION_LOCK; i < PANEL_ACTION_LAST; i++) {
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
			       (GClosureNotify) g_free,
			       0 /* connect_flags */);
	g_signal_connect_after (G_OBJECT (tree_view), "drag-begin",
	                        G_CALLBACK (panel_addto_drag_begin_cb),
	                        NULL);
}

static void
panel_addto_setup_launcher_drag (GtkTreeView *tree_view,
				 const char  *path)
{
	static const GtkTargetEntry target[] = {
		{ (gchar *) "text/uri-list", 0, 0 }
	};
	char *uri;
	char *uri_list;

	uri = g_filename_to_uri (path, NULL, NULL);
	uri_list = g_strconcat (uri, "\r\n", NULL);
	panel_addto_setup_drag (tree_view, target, uri_list);
	g_free (uri_list);
	g_free (uri);
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

		if (!name ||
		    panel_lockdown_is_applet_disabled (panel_lockdown_get (),
						       iid)) {
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
panel_addto_append_special_applets (PanelAddtoDialog *dialog,
				    GtkListStore *model)
{
	PanelAddtoItemInfo *special;

	if (!panel_lockdown_get_disable_command_line_s ()) {
		special = g_new0 (PanelAddtoItemInfo, 1);
		special->type = PANEL_ADDTO_LAUNCHER_NEW;
		special->name = g_strdup (_("Custom Application Launcher"));
		special->description = g_strdup (_("Create a new launcher"));
		special->icon = g_themed_icon_new (PANEL_ICON_LAUNCHER);
		special->action_type = PANEL_ACTION_NONE;
		special->iid = g_strdup ("LAUNCHER:ASK");
		panel_addto_append_item (dialog, model, special);
	}

	special = g_new0 (PanelAddtoItemInfo, 1);
	special->type = PANEL_ADDTO_LAUNCHER_MENU;
	special->name = g_strdup (_("Application Launcher..."));
	special->description = g_strdup (_("Copy a launcher from the applications menu"));
	special->icon = g_themed_icon_new (PANEL_ICON_LAUNCHER);
	special->action_type = PANEL_ACTION_NONE;
	special->iid = g_strdup ("LAUNCHER:MENU");
	panel_addto_append_item (dialog, model, special);
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
		panel_addto_append_special_applets (dialog, model);
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

typedef enum {
	PANEL_ADDTO_MENU_SHOW_DIRECTORIES = 1 << 0,
	PANEL_ADDTO_MENU_SHOW_ENTRIES     = 1 << 1,
	PANEL_ADDTO_MENU_SHOW_ALIAS       = 1 << 2
#define PANEL_ADDTO_MENU_SHOW_ALL (PANEL_ADDTO_MENU_SHOW_DIRECTORIES | PANEL_ADDTO_MENU_SHOW_ENTRIES | PANEL_ADDTO_MENU_SHOW_ALIAS)
} PanelAddtoMenuShowFlags;

static void panel_addto_make_application_list (GSList                  **parent_list,
					       GMenuTreeDirectory       *directory,
					       const char               *filename,
					       PanelAddtoMenuShowFlags   show_flags);

static void
panel_addto_prepend_directory (GSList             **parent_list,
			       GMenuTreeDirectory  *directory,
			       const char          *filename)
{
	PanelAddtoAppList *data;
	GIcon *icon;

	data = g_new0 (PanelAddtoAppList, 1);

	icon = gmenu_tree_directory_get_icon (directory);
	if (icon)
		g_object_ref (icon);

	data->item_info.type          = PANEL_ADDTO_MENU;
	data->item_info.name          = g_strdup (gmenu_tree_directory_get_name (directory));
	data->item_info.description   = g_strdup (gmenu_tree_directory_get_comment (directory));
	data->item_info.icon          = icon;
	data->item_info.menu_filename = g_strdup (filename);
	data->item_info.menu_path     = gmenu_tree_directory_make_path (directory, NULL);

	/* We should set the iid here to something and do
	 * iid = g_strdup_printf ("MENU:%s", tfr->name)
	 * but this means we'd have to free the iid later
	 * and this would complexify too much the free
	 * function.
	 * So the iid is built when we select the row.
	 */

	*parent_list = g_slist_prepend (*parent_list, data);
			
	/* We always want to show everything in non-root directories */
	panel_addto_make_application_list (&data->children, directory,
					   filename, PANEL_ADDTO_MENU_SHOW_ALL);
}

static void
panel_addto_prepend_entry (GSList         **parent_list,
			   GMenuTreeEntry  *entry,
			   const char      *filename)
{
	PanelAddtoAppList *data;
	GAppInfo *app_info;
	GIcon *icon;

	data = g_new0 (PanelAddtoAppList, 1);

	app_info = G_APP_INFO (gmenu_tree_entry_get_app_info (entry));

	icon = g_app_info_get_icon (app_info);
	if (icon)
		g_object_ref (icon);

	data->item_info.type          = PANEL_ADDTO_LAUNCHER;
	data->item_info.name          = g_strdup (g_app_info_get_display_name (app_info));
	data->item_info.description   = g_strdup (g_app_info_get_description (app_info));
	data->item_info.icon          = icon;
	data->item_info.launcher_path = g_strdup (gmenu_tree_entry_get_desktop_file_path (entry));

	*parent_list = g_slist_prepend (*parent_list, data);
}

static void
panel_addto_prepend_alias (GSList         **parent_list,
			   GMenuTreeAlias  *alias,
			   const char      *filename)
{
	GMenuTreeItemType type;

	type = gmenu_tree_alias_get_aliased_item_type (alias);

	if (type == GMENU_TREE_ITEM_DIRECTORY) {
		GMenuTreeDirectory *directory = gmenu_tree_alias_get_aliased_directory (alias);
		panel_addto_prepend_directory (parent_list,
					       directory,
					       filename);
		gmenu_tree_item_unref (directory);
	} else if (type == GMENU_TREE_ITEM_ENTRY) {
		GMenuTreeEntry *entry = gmenu_tree_alias_get_aliased_entry (alias);
		panel_addto_prepend_entry (parent_list,
					   entry,
					   filename);
		gmenu_tree_item_unref (entry);
	}
}

static void
panel_addto_make_application_list (GSList                  **parent_list,
				   GMenuTreeDirectory       *directory,
				   const char               *filename,
				   PanelAddtoMenuShowFlags   show_flags)
{
	GMenuTreeIter *iter;
	GMenuTreeItemType next_type;

	iter = gmenu_tree_directory_iter (directory);

	while ((next_type = gmenu_tree_iter_next (iter)) != GMENU_TREE_ITEM_INVALID) {
		if (next_type == GMENU_TREE_ITEM_DIRECTORY) {
			if (show_flags & PANEL_ADDTO_MENU_SHOW_DIRECTORIES) {
				GMenuTreeDirectory *dir = gmenu_tree_iter_get_directory (iter);
				panel_addto_prepend_directory (parent_list, dir, filename);
				gmenu_tree_item_unref (dir);
			}
		} else if (next_type == GMENU_TREE_ITEM_ENTRY) {
			if (show_flags & PANEL_ADDTO_MENU_SHOW_ENTRIES) {
				GMenuTreeEntry *entry = gmenu_tree_iter_get_entry (iter);
				panel_addto_prepend_entry (parent_list, entry, filename);
				gmenu_tree_item_unref (entry);
			}
		} else if (next_type == GMENU_TREE_ITEM_ALIAS) {
			if (show_flags & PANEL_ADDTO_MENU_SHOW_ALIAS) {
				GMenuTreeAlias *alias = gmenu_tree_iter_get_alias (iter);
				panel_addto_prepend_alias (parent_list, alias, filename);
				gmenu_tree_item_unref (alias);
			}
		}
	}
	gmenu_tree_iter_unref (iter);

	*parent_list = g_slist_reverse (*parent_list);
}

static void
panel_addto_populate_application_model (GtkTreeStore *store,
					GtkTreeIter  *parent,
					GSList       *app_list)
{
	PanelAddtoAppList *data;
	GtkTreeIter        iter;
	char              *text;
	GSList            *app;

	for (app = app_list; app != NULL; app = app->next) {
		data = app->data;
		gtk_tree_store_append (store, &iter, parent);

		text = panel_addto_make_text (data->item_info.name,
					      data->item_info.description);
		gtk_tree_store_set (store, &iter,
				    COLUMN_ICON, data->item_info.icon,
				    COLUMN_TEXT, text,
				    COLUMN_DATA, &(data->item_info),
				    COLUMN_SEARCH, data->item_info.name,
				    -1);

		g_free (text);

		if (data->children != NULL) 
			panel_addto_populate_application_model (store,
								&iter,
								data->children);
	}
}

static void
panel_addto_make_application_model (PanelAddtoDialog *dialog)
{
	GtkTreeStore      *store;
	GMenuTree          *tree;
	GMenuTreeDirectory *root;
	gchar              *applications_menu;

	if (dialog->filter_application_model != NULL)
		return;

	store = gtk_tree_store_new (NUMBER_COLUMNS,
				    G_TYPE_ICON,
				    G_TYPE_STRING,
				    G_TYPE_POINTER,
				    G_TYPE_STRING);

	applications_menu = get_applications_menu ();
	tree = gmenu_tree_new (applications_menu, GMENU_TREE_FLAGS_SORT_DISPLAY_NAME);

	if (!gmenu_tree_load_sync (tree, NULL)) {
		g_object_unref (tree);
		tree = NULL;
	}

	if (tree != NULL && (root = gmenu_tree_get_root_directory (tree))) {
		panel_addto_make_application_list (&dialog->application_list,
						   root, applications_menu,
						   PANEL_ADDTO_MENU_SHOW_ALL);
		panel_addto_populate_application_model (store, NULL, dialog->application_list);

		gmenu_tree_item_unref (root);
	}

	g_free (applications_menu);

	if (tree != NULL)
		g_object_unref (tree);

	tree = gmenu_tree_new ("gnomecc.menu", GMENU_TREE_FLAGS_SORT_DISPLAY_NAME);

	if (!gmenu_tree_load_sync (tree, NULL)) {
		g_object_unref (tree);
		tree = NULL;
	}

	if (tree != NULL && (root = gmenu_tree_get_root_directory (tree))) {
		GtkTreeIter iter;

		gtk_tree_store_append (store, &iter, NULL);
		gtk_tree_store_set (store, &iter,
				    COLUMN_ICON, NULL,
				    COLUMN_TEXT, NULL,
				    COLUMN_DATA, NULL,
				    COLUMN_SEARCH, NULL,
				    -1);

		/* The gnome-control-center shell does not display toplevel
		 * entries that are not directories, so do the same. */
		panel_addto_make_application_list (&dialog->settings_list,
						   root, "gnomecc.menu",
						   PANEL_ADDTO_MENU_SHOW_DIRECTORIES);
		panel_addto_populate_application_model (store, NULL,
							dialog->settings_list);

		gmenu_tree_item_unref (root);
	}

	if (tree != NULL)
		g_object_unref (tree);

	dialog->application_model = GTK_TREE_MODEL (store);
	dialog->filter_application_model = gtk_tree_model_filter_new (GTK_TREE_MODEL (dialog->application_model),
								      NULL);
	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (dialog->filter_application_model),
						panel_addto_filter_func,
						dialog, NULL);
}

static void
panel_addto_add_item (PanelAddtoDialog   *dialog,
	 	      PanelAddtoItemInfo *item_info)
{
	int pack_index;

	g_assert (item_info != NULL);

	pack_index = panel_widget_get_new_pack_index (dialog->panel_widget,
						      dialog->insert_pack_type);

	switch (item_info->type) {
	case PANEL_ADDTO_APPLET:
		panel_applet_frame_create (dialog->panel_widget->toplevel,
					   dialog->insert_pack_type,
					   pack_index,
					   item_info->iid);
		break;
	case PANEL_ADDTO_ACTION:
		panel_action_button_create (dialog->panel_widget->toplevel,
					    dialog->insert_pack_type,
					    pack_index,
					    item_info->action_type);
		break;
	case PANEL_ADDTO_LAUNCHER_MENU:
		panel_addto_present_applications (dialog);
		break;
	case PANEL_ADDTO_LAUNCHER:
		panel_launcher_create (dialog->panel_widget->toplevel,
				       dialog->insert_pack_type,
				       pack_index,
				       item_info->launcher_path);
		break;
	case PANEL_ADDTO_LAUNCHER_NEW:
		ask_about_launcher (NULL, dialog->panel_widget,
				    dialog->insert_pack_type);
		break;
	case PANEL_ADDTO_MENU:
		panel_menu_button_create (dialog->panel_widget->toplevel,
					  dialog->insert_pack_type,
					  pack_index,
					  item_info->menu_filename,
					  item_info->menu_path,
					  item_info->name);
		break;
	case PANEL_ADDTO_MENUBAR:
		panel_menu_bar_create (dialog->panel_widget->toplevel,
				       dialog->insert_pack_type,
				       pack_index);
		break;
	case PANEL_ADDTO_USER_MENU:
		panel_user_menu_create (dialog->panel_widget->toplevel,
					dialog->insert_pack_type,
					pack_index);
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
panel_addto_dialog_back_button_cb (PanelAddtoDialog *dialog,
                                   GtkWidget        *widget)
{
	panel_addto_present_applets (dialog);
}

static void
panel_addto_dialog_close_button_cb (PanelAddtoDialog *dialog,
                                    GtkWidget        *widget)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
panel_addto_present_applications (PanelAddtoDialog *dialog)
{
	if (dialog->filter_application_model == NULL)
		panel_addto_make_application_model (dialog);
	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->tree_view),
				 dialog->filter_application_model);
	gtk_window_set_focus (GTK_WINDOW (dialog),
			      dialog->search_entry);
	gtk_widget_set_sensitive (dialog->back_button, TRUE);

	if (dialog->applet_search_text)
		g_free (dialog->applet_search_text);

	dialog->applet_search_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->search_entry)));
	/* show everything */
	gtk_entry_set_text (GTK_ENTRY (dialog->search_entry), "");
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
	gtk_widget_set_sensitive (dialog->back_button, FALSE);

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

	g_free (item_info->launcher_path);
	item_info->launcher_path = NULL;

	g_free (item_info->menu_filename);
	item_info->menu_filename = NULL;

	g_free (item_info->menu_path);
	item_info->menu_path = NULL;
}

static void
panel_addto_dialog_free_application_list (GSList *application_list)
{
	PanelAddtoAppList *data;
	GSList            *app;

	if (application_list == NULL)
		return;

	for (app = application_list; app != NULL; app = app->next) {
		data = app->data;
		if (data->children) {
			panel_addto_dialog_free_application_list (data->children);
		}
		panel_addto_dialog_free_item_info (&data->item_info);
		g_free (data);
	}
	g_slist_free (application_list);
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
	char               *iid;

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
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->add_button),
					  FALSE);
		return;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (dialog->add_button), TRUE);

	if (data->type == PANEL_ADDTO_LAUNCHER_MENU) {
		gtk_button_set_label (GTK_BUTTON (dialog->add_button),
				      _("_Forward"));
	} else {
		gtk_button_set_label (GTK_BUTTON (dialog->add_button),
				      _("_Add"));
	}

	/* only allow dragging applets if we can add applets */
	if (panel_layout_is_writable ()) {
		if (data->type == PANEL_ADDTO_LAUNCHER) {
			panel_addto_setup_launcher_drag (GTK_TREE_VIEW (dialog->tree_view),
							 data->launcher_path);
		} else if (data->type == PANEL_ADDTO_APPLET) {
			panel_addto_setup_applet_drag (GTK_TREE_VIEW (dialog->tree_view),
						       data->iid);
		} else if (data->type == PANEL_ADDTO_LAUNCHER_MENU) {
			gtk_tree_view_unset_rows_drag_source (GTK_TREE_VIEW (dialog->tree_view));
		} else if (data->type == PANEL_ADDTO_MENU) {
			/* build the iid for menus other than the main menu */
			if (data->iid == NULL) {
				iid = g_strdup_printf ("MENU:%s/%s",
						       data->menu_filename,
						       data->menu_path);
			} else {
				iid = g_strdup (data->iid);
			}
			panel_addto_setup_internal_applet_drag (GTK_TREE_VIEW (dialog->tree_view),
							        iid);
			g_free (iid);
		} else {
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
	g_clear_object (&dialog->filter_application_model);
	g_clear_object (&dialog->application_model);
	g_clear_object (&dialog->menu_tree);

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

	panel_addto_dialog_free_application_list (dialog->application_list);
	panel_addto_dialog_free_application_list (dialog->settings_list);

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
	gtk_widget_class_bind_template_child (widget_class, PanelAddtoDialog, back_button);
	gtk_widget_class_bind_template_child (widget_class, PanelAddtoDialog, close_button);
	gtk_widget_class_bind_template_child (widget_class, PanelAddtoDialog, tree_view);
	gtk_widget_class_bind_template_child (widget_class, PanelAddtoDialog, tree_view_selection);

	gtk_widget_class_bind_template_callback (widget_class, panel_addto_dialog_add_button_cb);
	gtk_widget_class_bind_template_callback (widget_class, panel_addto_dialog_back_button_cb);
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
	PanelData     *pd;
	GdkScreen *screen;
	gint screen_height;
	gint height;

	toplevel = panel_widget->toplevel;
	pd = g_object_get_data (G_OBJECT (toplevel), "PanelData");

	if (!panel_addto_dialog_quark)
		panel_addto_dialog_quark =
			g_quark_from_static_string ("panel-addto-dialog");

	dialog = g_object_get_qdata (G_OBJECT (toplevel),
				     panel_addto_dialog_quark);

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));
	screen_height = gdk_screen_get_height (screen);
	height = MIN (MAX_ADDTOPANEL_HEIGHT, 3 * (screen_height / 4));

	if (!dialog) {
		dialog = panel_addto_dialog_new (panel_widget);
		panel_addto_present_applets (dialog);
	}

	dialog->insert_pack_type = pd ? pd->insert_pack_type : PANEL_OBJECT_PACK_START;
	gtk_window_set_screen (GTK_WINDOW (dialog), screen);
	gtk_window_set_default_size (GTK_WINDOW (dialog),
				     height * 8 / 7, height);
	gtk_window_present (GTK_WINDOW (dialog));
}
