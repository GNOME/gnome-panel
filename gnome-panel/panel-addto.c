/*
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Vincent Untz <vincent@vuntz.net>
 */

#include <config.h>
#include <string.h>

#include <libgnome/libgnome.h>
#include <libbonobo.h>

#include "menu.h"

#include "launcher.h"
#include "menu-fentry.h"
#include "menu-ditem.h"
#include "panel.h"
#include "drawer.h"
#include "panel-applet-frame.h"
#include "panel-stock-icons.h"
#include "panel-action-button.h"
#include "panel-menu-bar.h"
#include "panel-toplevel.h"
#include "panel-menu-button.h"
#include "panel-globals.h"
#include "panel-lockdown.h"
#include "panel-util.h"
#include "panel-profile.h"
#include "panel-addto.h"

typedef struct {
	PanelWidget *panel_widget;

	GtkWidget    *addto_dialog;
	GtkWidget    *label;
	GtkWidget    *back_button;
	GtkWidget    *add_button;
	GtkWidget    *tree_view;
	GtkTreeModel *applet_model;
	GtkTreeModel *application_model;

	GSList       *applet_list;
	GSList       *application_list;

	guint         name_notify;

	int           insertion_position;
} PanelAddtoDialog;

static GQuark panel_addto_dialog_quark = 0;

typedef enum {
	PANEL_ADDTO_APPLET,
	PANEL_ADDTO_ACTION,
	PANEL_ADDTO_LAUNCHER_MENU,
	PANEL_ADDTO_LAUNCHER,
	PANEL_ADDTO_LAUNCHER_NEW,
	PANEL_ADDTO_MENU,
	PANEL_ADDTO_MENUBAR,
	PANEL_ADDTO_DRAWER
} PanelAddtoItemType;

typedef struct {
	PanelAddtoItemType     type;
	const char            *name;
	const char            *description;
	const char            *icon;
	const char            *stock_icon;
	PanelActionButtonType  action_type;
	const char            *launcher_name;
	const char            *menu_path;
	const char            *iid;
	gboolean               static_data;
} PanelAddtoItemInfo;

typedef struct {
	GSList             *children;
	PanelAddtoItemInfo  item_info;
} PanelAddtoAppList;

static PanelAddtoItemInfo internal_addto_items [] = {

	{ PANEL_ADDTO_LAUNCHER_NEW,
	  N_("Custom Application Launcher"),
	  N_("Create a new launcher"),
	  NULL,
	  PANEL_STOCK_LAUNCHER,
	  PANEL_ACTION_NONE,
	  NULL,
	  NULL,
	  "LAUNCHER:ASK",
	  TRUE },

	{ PANEL_ADDTO_LAUNCHER_MENU,
	  N_("Application Launcher..."),
	  N_("Launch a program that is already in the GNOME menu"),
	  NULL,
	  PANEL_STOCK_LAUNCHER,
	  PANEL_ACTION_NONE,
	  NULL,
	  NULL,
	  "LAUNCHER:MENU",
	  TRUE },

	{ PANEL_ADDTO_MENU,
	  N_("Main Menu"),
	  N_("The main GNOME menu"),
	  NULL,
	  PANEL_STOCK_MAIN_MENU,
	  PANEL_ACTION_NONE,
	  NULL,
	  NULL,
	  "MENU:MAIN",
	  TRUE },

	{ PANEL_ADDTO_MENUBAR,
	  N_("Menu Bar"),
	  N_("A custom menu bar"),
	  NULL,
	  PANEL_STOCK_GNOME_LOGO,
	  PANEL_ACTION_NONE,
	  NULL,
	  NULL,
	  "MENUBAR:NEW",
	  TRUE },

	{ PANEL_ADDTO_DRAWER,
	  N_("Drawer"),
	  N_("A pop out drawer to store other items in"),
	  NULL,
	  PANEL_STOCK_DRAWER,
	  PANEL_ACTION_NONE,
	  NULL,
	  NULL,
	  "DRAWER:NEW",
	  TRUE }
};

static const char applet_requirements [] =
	"has_all (repo_ids, ['IDL:Bonobo/Control:1.0',"
	"		     'IDL:GNOME/Vertigo/PanelAppletShell:1.0']) && "
	"defined (panel:icon)";

static char *applet_sort_criteria [] = {
	"name",
	NULL
	};

enum {
	COLUMN_ICON,
	COLUMN_TEXT,
	COLUMN_DATA,
	COLUMN_SEARCH,
	NUMBER_COLUMNS
};

enum {
	PANEL_ADDTO_RESPONSE_BACK,
	PANEL_ADDTO_RESPONSE_ADD
};

static void panel_addto_present_applications (PanelAddtoDialog *dialog);
static void panel_addto_present_applets      (PanelAddtoDialog *dialog);

static int
panel_addto_applet_info_sort_func (PanelAddtoItemInfo *a,
				   PanelAddtoItemInfo *b)
{
	return g_utf8_collate (a->name, b->name);
}

static GSList *
panel_addto_append_internal_applets (GSList *list)
{
	static gboolean translated = FALSE;
	int             i;

	for (i = 0; i < G_N_ELEMENTS (internal_addto_items); i++) {
		internal_addto_items [i].name        = _(internal_addto_items [i].name);
		internal_addto_items [i].description = _(internal_addto_items [i].description);

                list = g_slist_append (list, &internal_addto_items [i]);
        }

	translated = TRUE;

	for (i = PANEL_ACTION_LOCK; i < PANEL_ACTION_LAST; i++) {
		PanelAddtoItemInfo *info;

		if (panel_action_get_is_disabled (i))
			continue;

		info              = g_new0 (PanelAddtoItemInfo, 1);
		info->type        = PANEL_ADDTO_ACTION;
		info->action_type = i;
		info->name        = (char *) panel_action_get_text (i);
		info->description = (char *) panel_action_get_tooltip (i);
		info->stock_icon  = (char *) panel_action_get_stock_icon (i);
		info->iid         = (char *) panel_action_get_drag_id (i);
		info->static_data = FALSE;

                list = g_slist_append (list, info);
	}

        return list;
}

static char *
panel_addto_make_text (const char *name,
		       const char *desc)
{
	char *esc_name;
	char *esc_desc;
	char *result; 

	esc_name = name ? g_markup_escape_text (name, -1) :
			  g_strdup(_("(empty)"));
	if (desc != NULL) {
		esc_desc = g_markup_escape_text (desc, -1);
		result = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n%s",
					  esc_name, esc_desc);
		g_free (esc_desc);
	} else {
		result = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>",
					  esc_name);
	}
	g_free (esc_name);
	return result;
}

static GdkPixbuf *
panel_addto_make_pixbuf (const char *filename,
			 int         size)
{
	char *file;
	GdkPixbuf *pb;

	file = gnome_desktop_item_find_icon (panel_icon_theme,
					     filename, size, 0);

	if (file == NULL)
		return NULL;

	pb = gdk_pixbuf_new_from_file (file, NULL);

	g_free (file);
	return pb;
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
				selection_data->target, 8, (guchar *) string,
				strlen (string));
}

static void
panel_addto_setup_drag (GtkTreeView          *tree_view,
			const GtkTargetEntry *target,
			const char           *text)
{
	if (!text || panel_lockdown_get_locked_down ())
		return;
	
	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (tree_view),
						GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
						target, 1, GDK_ACTION_COPY);
	
	g_signal_connect_data (G_OBJECT (tree_view), "drag_data_get",
			       G_CALLBACK (panel_addto_drag_data_get_cb),
			       g_strdup (text),
			       (GClosureNotify) g_free,
			       0 /* connect_flags */);
}

static void
panel_addto_setup_launcher_drag (GtkTreeView *tree_view,
				 const char  *uri)
{
        static GtkTargetEntry target[] = {
		{ "text/uri-list", 0, 0 }
	};
	char *uri_list;

	uri_list = g_strconcat (uri, "\r\n", NULL);
	panel_addto_setup_drag (tree_view, target, uri_list);
	g_free (uri_list);
}

static void
panel_addto_setup_applet_drag (GtkTreeView *tree_view,
			       const char  *iid)
{
	static GtkTargetEntry target[] = {
		{ "application/x-panel-applet-iid", 0, 0 }
	};

	panel_addto_setup_drag (tree_view, target, iid);
}

static void
panel_addto_setup_internal_applet_drag (GtkTreeView *tree_view,
					const char  *applet_type)
{
	static GtkTargetEntry target[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};

	panel_addto_setup_drag (tree_view, target, applet_type);
}

static GSList *
panel_addto_query_applets (GSList *list)
{
	Bonobo_ServerInfoList *applet_list;
	CORBA_Environment  env;
	const GList *langs_glist;
	GSList *langs_gslist;
	int i;

	CORBA_exception_init (&env);

	applet_list = bonobo_activation_query (applet_requirements,
					       applet_sort_criteria,
					       &env);
	if (BONOBO_EX (&env)) {
		g_warning (_("query returned exception %s\n"),
			   BONOBO_EX_REPOID (&env));

		CORBA_exception_free (&env);

		return NULL;
	}

	CORBA_exception_free (&env);

	/* Evil evil evil evil, we need to convery from a
	   GList to a GSList */
	langs_gslist = NULL;
	for (langs_glist = gnome_i18n_get_language_list ("LC_MESSAGES");
	     langs_glist != NULL; langs_glist = langs_glist->next) {
		langs_gslist = g_slist_append (langs_gslist,
					       langs_glist->data);
	}

	for (i = 0; i < applet_list->_length; i++) {
		Bonobo_ServerInfo *info;
		const char *name, *description, *icon;
		PanelAddtoItemInfo *applet;

		info = &applet_list->_buffer[i];

		name = bonobo_server_info_prop_lookup (info,
						       "name",
						       langs_gslist);
		description = bonobo_server_info_prop_lookup (info,
							      "description",
							      langs_gslist);
		icon = bonobo_server_info_prop_lookup (info,
						       "panel:icon",
						       NULL);

		if (!name) {
			continue;
		}

		applet = g_new0 (PanelAddtoItemInfo, 1);
		applet->type = PANEL_ADDTO_APPLET;
		applet->name = name;
		applet->description = description;
		applet->icon = icon;
		applet->iid = info->iid;
		applet->static_data = FALSE;

		list = g_slist_append (list, applet);
	}

	g_slist_free (langs_gslist);

	return list;
}

static GtkTreeModel *
panel_addto_make_applet_model (PanelAddtoDialog *dialog)
{
	GtkListStore *model;
	GtkTreeIter iter;
	GSList *item = NULL;

	if (panel_profile_list_is_writable (PANEL_GCONF_APPLETS)) {
		dialog->applet_list = panel_addto_query_applets (dialog->applet_list);
	}

	if (panel_profile_list_is_writable (PANEL_GCONF_OBJECTS)) {
		dialog->applet_list = panel_addto_append_internal_applets (dialog->applet_list);
	}

	dialog->applet_list = g_slist_sort (dialog->applet_list,
					    (GCompareFunc) panel_addto_applet_info_sort_func);

	model = gtk_list_store_new (NUMBER_COLUMNS,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,
				    G_TYPE_POINTER,
				    G_TYPE_STRING);

	for (item = dialog->applet_list; item != NULL; item = item->next) {
		PanelAddtoItemInfo *applet;
		char *text;
		GdkPixbuf *pixbuf;

		applet = (PanelAddtoItemInfo *) item->data;

		if (applet->icon != NULL) {
			pixbuf = panel_addto_make_pixbuf (applet->icon,
							  PANEL_DEFAULT_MENU_ICON_SIZE);
		} else {
			pixbuf = gtk_widget_render_icon (GTK_WIDGET (dialog->panel_widget),
							 applet->stock_icon,
							 GTK_ICON_SIZE_DIALOG,
							 NULL);
		}

		gtk_list_store_append (model, &iter);

		text = panel_addto_make_text (applet->name,
					      applet->description);

		gtk_list_store_set (model, &iter,
				    COLUMN_ICON, pixbuf,
				    COLUMN_TEXT, text,
				    COLUMN_DATA, applet,
				    COLUMN_SEARCH, applet->name,
				    -1);
		g_free (text);
	}

	return (GtkTreeModel *) model;
}

static void
panel_addto_make_application_list_r (GSList **parent,
				     DirRec *dr)
{
	PanelAddtoAppList *data;
	FileRec *tfr;
	GSList *li;

	if (dr == NULL)
		return;

	for (li = dr->recs; li != NULL; li = li->next) {
		tfr = li->data;

		switch (tfr->type) {
		case FILE_REC_DIR:
			data = g_new0 (PanelAddtoAppList, 1);
			data->item_info.type = PANEL_ADDTO_MENU;
			data->item_info.name = tfr->fullname;
			data->item_info.description = tfr->comment;
			data->item_info.icon = tfr->icon;
			data->item_info.menu_path = tfr->name;
			/* We should set the iid here to something and do
			 * iid = g_strdup_printf ("MENU:%s", tfr->name)
			 * but this means we'd have to free the iid later
			 * and this would complexify too much the free
			 * function.
			 * So the iid is built when we select the row. */
			tfr = fr_check_and_reread (tfr);
			panel_addto_make_application_list_r (&(data->children),
							     (DirRec *) tfr);
			*parent = g_slist_append (*parent, data);
			break;
		case FILE_REC_FILE:
			data = g_new0 (PanelAddtoAppList, 1);
			data->item_info.type = PANEL_ADDTO_LAUNCHER;
			data->item_info.name = tfr->fullname;
			data->item_info.description = tfr->comment;
			data->item_info.icon = tfr->icon;
			data->item_info.launcher_name = tfr->name;
			*parent = g_slist_append (*parent, data);
			break;
		default:
			break;
		}
	}
}

static void
panel_addto_make_application_model_r (GtkTreeStore *store,
				      GtkTreeIter  *parent,
				      GSList       *app_list)
{
	PanelAddtoAppList *data;
	GtkTreeIter        iter;
	char              *text;
	GdkPixbuf         *pixbuf;
	GSList            *app;

	for (app = app_list; app != NULL; app = app->next) {
		data = app->data;
		gtk_tree_store_append (store, &iter, parent);

		text = panel_addto_make_text (data->item_info.name,
					      data->item_info.description);
		pixbuf = panel_addto_make_pixbuf (data->item_info.icon,
						  PANEL_DEFAULT_MENU_ICON_SIZE);
		gtk_tree_store_set (store, &iter,
				    COLUMN_ICON, pixbuf,
				    COLUMN_TEXT, text,
				    COLUMN_DATA, &(data->item_info),
				    COLUMN_SEARCH, data->item_info.name,
				    -1);

		if (pixbuf) {
			g_object_unref (pixbuf);
		}
		g_free (text);

		if (data->children != NULL) 
			panel_addto_make_application_model_r (store, &iter,
							      data->children);
	}
}

static GtkTreeModel *
panel_addto_make_application_model (PanelAddtoDialog *dialog)
{
	GtkTreeStore *store;
	DirRec *root;

	store = gtk_tree_store_new (NUMBER_COLUMNS,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,
				    G_TYPE_POINTER,
				    G_TYPE_STRING);

	root = (DirRec *) fr_get_dir ("applications:/");
	panel_addto_make_application_list_r (&(dialog->application_list),
					     root);
	panel_addto_make_application_model_r (store, NULL,
					      dialog->application_list);

	return GTK_TREE_MODEL (store);
}

static gboolean
panel_addto_add_item (PanelAddtoDialog   *dialog,
	 	      PanelAddtoItemInfo *item_info)
{
	gboolean destroy;
	
	g_assert (item_info != NULL);

	destroy = TRUE;

	switch (item_info->type) {
	case PANEL_ADDTO_APPLET:
		panel_applet_frame_create (dialog->panel_widget->toplevel,
					   dialog->insertion_position,
					   item_info->iid);
		break;
	case PANEL_ADDTO_ACTION:
		panel_action_button_create (dialog->panel_widget->toplevel,
					    dialog->insertion_position,
					    item_info->action_type);
		break;
	case PANEL_ADDTO_LAUNCHER_MENU:
		panel_addto_present_applications (dialog);
		destroy = FALSE;
		break;
	case PANEL_ADDTO_LAUNCHER:
		panel_launcher_create (dialog->panel_widget->toplevel,
				       dialog->insertion_position,
				       item_info->launcher_name);
		break;
	case PANEL_ADDTO_LAUNCHER_NEW:
		ask_about_launcher (NULL, dialog->panel_widget,
				    dialog->insertion_position, FALSE);
		break;
	case PANEL_ADDTO_MENU:
		panel_menu_button_create (dialog->panel_widget->toplevel,
					  dialog->insertion_position,
					  item_info->menu_path,
					  item_info->menu_path != NULL,
					  item_info->name);
		break;
	case PANEL_ADDTO_MENUBAR:
		panel_menu_bar_create (dialog->panel_widget->toplevel,
				       dialog->insertion_position);
		break;
	case PANEL_ADDTO_DRAWER:
		panel_drawer_create (dialog->panel_widget->toplevel,
				     dialog->insertion_position,
				     NULL, FALSE, NULL);
		break;
	}

	return destroy;
}

static void
panel_addto_dialog_response (GtkWidget *widget_dialog,
			     guint response_id,
			     PanelAddtoDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	switch (response_id) {
	case GTK_RESPONSE_HELP:
		panel_show_help (gtk_window_get_screen (GTK_WINDOW (dialog->addto_dialog)),
				 "user-guide.xml", "gospanel-28"); //FIXME
		break;

	case PANEL_ADDTO_RESPONSE_ADD:
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
		if (gtk_tree_selection_get_selected (selection,
						     &model, &iter)) {
			PanelAddtoItemInfo *data;

			gtk_tree_model_get (model, &iter,
					    COLUMN_DATA, &data, -1);

			if (data != NULL) {
				if (panel_addto_add_item (dialog, data))
					gtk_widget_destroy (widget_dialog);
			}
		}
		break;

	case PANEL_ADDTO_RESPONSE_BACK:
		/* This response only happens when we're showing the
		 * application list and the user wants to go back to the
		 * applet list. */
		panel_addto_present_applets (dialog);
		break;

	case GTK_RESPONSE_CANCEL:
		gtk_widget_destroy (widget_dialog);
		break;

	default:
		break;
	}
}

static void
panel_addto_dialog_destroy (GtkWidget *widget_dialog,
			    PanelAddtoDialog *dialog)
{
	panel_toplevel_unblock_auto_hide (PANEL_TOPLEVEL (dialog->panel_widget->toplevel));
	g_object_set_qdata (G_OBJECT (dialog->panel_widget->toplevel),
			    panel_addto_dialog_quark,
			    NULL);
}

static void
panel_addto_present_applications (PanelAddtoDialog *dialog)
{
	if (dialog->application_model == NULL)
		dialog->application_model = panel_addto_make_application_model (dialog);
	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->tree_view),
				 dialog->application_model);
	gtk_window_set_focus (GTK_WINDOW (dialog->addto_dialog),
			      dialog->tree_view);
	gtk_widget_set_sensitive (dialog->back_button, TRUE);
}

static void
panel_addto_present_applets (PanelAddtoDialog *dialog)
{
	if (dialog->applet_model == NULL)
		dialog->applet_model = panel_addto_make_applet_model (dialog);
	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->tree_view),
				 dialog->applet_model);
	gtk_window_set_focus (GTK_WINDOW (dialog->addto_dialog),
			      dialog->tree_view);
	gtk_widget_set_sensitive (dialog->back_button, FALSE);
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
		g_free (data);
	}
	g_slist_free (application_list);
}

static void
panel_addto_dialog_free (PanelAddtoDialog *dialog)
{
	GConfClient *client;
	GSList      *item;

	client = panel_gconf_get_client ();

	if (dialog->name_notify)
		gconf_client_notify_remove (client, dialog->name_notify);
	dialog->name_notify = 0;

	if (dialog->addto_dialog)
		gtk_widget_destroy (dialog->addto_dialog);
	dialog->addto_dialog = NULL;

	for (item = dialog->applet_list; item != NULL; item = item->next) {
		PanelAddtoItemInfo *applet;

		applet = (PanelAddtoItemInfo *) item->data;
		if (!applet->static_data) {
			g_free (applet);
		}
	}
	g_slist_free (dialog->applet_list);

	panel_addto_dialog_free_application_list (dialog->application_list);

	g_free (dialog);
}

static void
panel_addto_name_change (PanelAddtoDialog *dialog,
			 const char       *name)
{
	char *title;
	char *markup_title;

	if (name) {
		title = g_strdup_printf (_("Add to %s"), name);
	} else {
		title = g_strdup_printf (_("Add to the panel"));
	}
	gtk_window_set_title (GTK_WINDOW (dialog->addto_dialog), title);
	g_free (title);

	if (name) {
		title = g_strdup_printf (_("Select an _item to add to %s:"),
					 name);
	} else {
		title = g_strdup (_("Select an _item to add to the panel:"));
	}
	markup_title = g_strdup_printf ("<span weight=\"bold\">%s</span>",
					title);
	g_free (title);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (dialog->label),
					    markup_title);
	g_free (markup_title);
}

static void
panel_addto_name_notify (GConfClient      *client,
			 guint             cnxn_id,
			 GConfEntry       *entry,
			 PanelAddtoDialog *dialog)
{
	GConfValue *value;
	const char *key;
	const char *text = NULL;

	key = panel_gconf_basename (gconf_entry_get_key (entry));

	if (strcmp (key, "name"))
		return;

	value = gconf_entry_get_value (entry);

	if (value && value->type == GCONF_VALUE_STRING)
		text = gconf_value_get_string (value);

	if (text)
		panel_addto_name_change (dialog, text);
}

static void
panel_addto_selection_changed (GtkTreeSelection *selection,
			       PanelAddtoDialog *dialog)
{
	GtkTreeModel       *model;
	GtkTreeIter         iter;
	PanelAddtoItemInfo *data;
	char               *iid;

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, COLUMN_DATA, &data, -1);

	if (!data)
		return;

	if (data->type == PANEL_ADDTO_LAUNCHER_MENU) {
		gtk_button_set_label (GTK_BUTTON (dialog->add_button),
				      GTK_STOCK_GO_FORWARD);
		gtk_button_set_use_stock (GTK_BUTTON (dialog->add_button),
					  TRUE);
	} else {
		//FIXME
		gtk_button_set_label (GTK_BUTTON (dialog->add_button),
				      GTK_STOCK_ADD);
		gtk_button_set_use_stock (GTK_BUTTON (dialog->add_button),
					  TRUE);
	}

	/* only allow dragging applets if we can add applets */
	if (panel_profile_list_is_writable (PANEL_GCONF_APPLETS)) {
		switch (data->type) {
		case PANEL_ADDTO_LAUNCHER:
			panel_addto_setup_launcher_drag (GTK_TREE_VIEW (dialog->tree_view),
							 data->launcher_name);
			break;
		case PANEL_ADDTO_APPLET:
			panel_addto_setup_applet_drag (GTK_TREE_VIEW (dialog->tree_view),
						       data->iid);
			break;
		case PANEL_ADDTO_LAUNCHER_MENU:
			gtk_tree_view_unset_rows_drag_source (GTK_TREE_VIEW (dialog->tree_view));
			break;
		case PANEL_ADDTO_MENU:
			/* build the iid for menus other than the main menu */
			if (data->iid == NULL) {
				iid = g_strdup_printf ("MENU:%s",
						       data->menu_path);
			} else {
				iid = g_strdup (data->iid);
			}
			panel_addto_setup_internal_applet_drag (GTK_TREE_VIEW (dialog->tree_view),
							        iid);
			g_free (iid);
			break;
		default:
			panel_addto_setup_internal_applet_drag (GTK_TREE_VIEW (dialog->tree_view),
							        data->iid);
			break;
		}
	}
}

static void
panel_addto_selection_activated (GtkTreeView       *view,
				 GtkTreePath       *path,
				 GtkTreeViewColumn *column,
				 PanelAddtoDialog  *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog->addto_dialog),
			     PANEL_ADDTO_RESPONSE_ADD);
}

static PanelAddtoDialog *
panel_addto_dialog_new (PanelWidget *panel_widget)
{
	PanelAddtoDialog *dialog;
	GtkWidget *vbox;
	GtkWidget *inner_vbox;
	GtkWidget *sw;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;

	dialog = g_new0 (PanelAddtoDialog, 1);

	g_object_set_qdata_full (G_OBJECT (panel_widget->toplevel),
				 panel_addto_dialog_quark,
				 dialog,
				 (GDestroyNotify) panel_addto_dialog_free);

	dialog->panel_widget = panel_widget;
	dialog->name_notify =
		panel_profile_toplevel_notify_add (
			dialog->panel_widget->toplevel,
			"name",
			(GConfClientNotifyFunc) panel_addto_name_notify,
			dialog);


	dialog->addto_dialog = gtk_dialog_new ();
	gtk_dialog_add_button (GTK_DIALOG (dialog->addto_dialog),
			       GTK_STOCK_HELP, GTK_RESPONSE_HELP);
	gtk_dialog_add_button (GTK_DIALOG (dialog->addto_dialog),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);
	dialog->back_button = gtk_dialog_add_button (GTK_DIALOG (dialog->addto_dialog),
						     GTK_STOCK_GO_BACK,
						     PANEL_ADDTO_RESPONSE_BACK);
	dialog->add_button = gtk_dialog_add_button (GTK_DIALOG (dialog->addto_dialog),
						     GTK_STOCK_ADD,
						     PANEL_ADDTO_RESPONSE_ADD);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog->addto_dialog),
				      FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog->addto_dialog),
					 PANEL_ADDTO_RESPONSE_ADD);

	gtk_container_set_border_width (GTK_CONTAINER (dialog->addto_dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog->addto_dialog)->vbox), 2);
	g_signal_connect (G_OBJECT (dialog->addto_dialog), "response",
			  G_CALLBACK (panel_addto_dialog_response), dialog);
	g_signal_connect (dialog->addto_dialog, "destroy",
			  G_CALLBACK (panel_addto_dialog_destroy), dialog);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog->addto_dialog)->vbox),
			   vbox);

	inner_vbox = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), inner_vbox, TRUE, TRUE, 0);

	dialog->label = gtk_label_new_with_mnemonic ("");
	gtk_misc_set_alignment (GTK_MISC (dialog->label), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (dialog->label), TRUE);

	gtk_box_pack_start (GTK_BOX (inner_vbox), dialog->label,
			    FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
					     GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (inner_vbox), sw, TRUE, TRUE, 0);

	dialog->tree_view = gtk_tree_view_new ();
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dialog->tree_view),
					   FALSE);
	gtk_tree_view_expand_all (GTK_TREE_VIEW (dialog->tree_view));

	renderer = g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF,
				 "xpad", 4,
				 "ypad", 4,
				 NULL);

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (dialog->tree_view),
						     -1, NULL,
						     renderer,
						     "pixbuf", COLUMN_ICON,
						     NULL);
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (dialog->tree_view),
						     -1, NULL,
						     renderer,
						     "markup", COLUMN_TEXT,
						     NULL);

	gtk_tree_view_set_search_column (GTK_TREE_VIEW (dialog->tree_view),
					 COLUMN_SEARCH);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	column = gtk_tree_view_get_column (GTK_TREE_VIEW (dialog->tree_view),
					   COLUMN_TEXT);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (panel_addto_selection_changed),
			  dialog);

	g_signal_connect (dialog->tree_view, "row-activated",
			  G_CALLBACK (panel_addto_selection_activated),
			  dialog);

	gtk_container_add (GTK_CONTAINER (sw), dialog->tree_view);

	gtk_widget_show_all (vbox);

	gtk_label_set_mnemonic_widget (GTK_LABEL (dialog->label),
				       dialog->tree_view);

	panel_toplevel_block_auto_hide (dialog->panel_widget->toplevel);
	panel_addto_name_change (dialog,
				 panel_toplevel_get_name (dialog->panel_widget->toplevel));

	return dialog;
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

	dialog->insertion_position = pd ? pd->insertion_pos : -1;
	gtk_window_set_screen (GTK_WINDOW (dialog->addto_dialog), screen);
	gtk_window_set_default_size (GTK_WINDOW (dialog->addto_dialog),
				     height * 8 / 7, height);
	gtk_window_present (GTK_WINDOW (dialog->addto_dialog));
}
