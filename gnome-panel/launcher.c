/*
 * GNOME panel launcher module.
 * (C) 1997,1998,1999,2000 The Free Software Foundation
 * (C) 2000 Eazel, Inc.
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 * CORBAized by George Lebl
 * de-CORBAized by George Lebl
 */

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs.h>

#include "gnome-desktop-item.h"
#include "gnome-ditem-edit.h"

#include "panel-include.h"

static void properties_apply (Launcher *launcher);

extern GtkTooltips *panel_tooltips;

extern GSList *applets;
extern GSList *applets_last;
extern int applet_count;

static char *default_app_pixmap = NULL;

extern GlobalConfig global_config;
extern gboolean commie_mode;

extern char *merge_merge_dir;

extern GSList *panels;

enum {
	HELP_BUTTON = 0,
	REVERT_BUTTON,
	CLOSE_BUTTON
};

static void
launch_url (Launcher *launcher)
{
	GnomeDesktopItem *item;
	const char *url;
	GError *error = NULL;

	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->ditem != NULL);

	item = launcher->ditem;
	url = gnome_desktop_item_get_string (item,
					     GNOME_DESKTOP_ITEM_URL);

	if (url == NULL) {
		panel_error_dialog ("no_url_dialog",
				    _("This launch icon does not "
				      "specify a url to show"));
		return;
	}

	gnome_url_show (url, &error);
	if (error != NULL) {
		panel_error_dialog ("cant_show_url_dialog",
				    _("Cannot show %s\n%s"),
				    url, error->message);
		g_clear_error (&error);
	}
}


static void
launch (Launcher *launcher, int argc, char *argv[])
{
	GnomeDesktopItem *item;
	GnomeDesktopItemType type;

	g_return_if_fail(launcher != NULL);
	g_return_if_fail(launcher->ditem != NULL);

	item = launcher->ditem;
	type = gnome_desktop_item_get_entry_type (item);

	if (type == GNOME_DESKTOP_ITEM_TYPE_LINK) {
		launch_url (launcher);
	} else {
		GError *error = NULL;
		gnome_desktop_item_launch (item, argc, argv, &error);
		if (error != NULL) {
			panel_error_dialog ("cannot_launch_icon",
					    _("Cannot launch icon\n%s"),
					    error->message);
			g_clear_error (&error);
		}
	}
	
	if(global_config.drawer_auto_close) {
		GtkWidget *parent =
			PANEL_WIDGET(launcher->button->parent)->panel_parent;
		g_return_if_fail(parent!=NULL);
		if(IS_DRAWER_WIDGET(parent)) {
			BasePWidget *basep = BASEP_WIDGET(parent);
			GtkWidget *grandparent = PANEL_WIDGET(basep->panel)->master_widget->parent;
			GtkWidget *grandparentw =
				PANEL_WIDGET(grandparent)->panel_parent;
			drawer_widget_close_drawer (DRAWER_WIDGET (parent),
						    grandparentw);
		}
	}
}

static void
launch_cb (GtkWidget *widget, gpointer data)
{
	launch (data, 0, NULL);
}

static void
drag_data_received_cb (GtkWidget        *widget,
		       GdkDragContext   *context,
		       gint              x,
		       gint              y,
		       GtkSelectionData *selection_data,
		       guint             info,
		       guint             time,
		       Launcher         *launcher)
{
	GList *list;
	GError *error = NULL;

	list = gnome_vfs_uri_list_parse ((char *)selection_data->data);

	gnome_desktop_item_drop_uri_list (launcher->ditem, list, &error);
	if (error != NULL) {
		panel_error_dialog ("cannot_launch_icon",
				    _("Cannot launch icon\n%s"),
				    error->message);
		g_clear_error (&error);
	}

	gnome_vfs_uri_list_free (list);

	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
destroy_launcher (GtkWidget *widget, gpointer data)
{
	Launcher *launcher = data;
	GtkWidget *prop_dialog = launcher->prop_dialog;

	launcher->prop_dialog = NULL;

	if (prop_dialog != NULL)
		gtk_widget_destroy (prop_dialog);
}

static void
free_launcher (gpointer data)
{
	Launcher *launcher = data;

	gnome_desktop_item_unref (launcher->ditem);
	launcher->ditem = NULL;

	if (launcher->revert_ditem != NULL)
		gnome_desktop_item_unref (launcher->revert_ditem);
	launcher->revert_ditem = NULL;

	g_free (launcher);
}

static gboolean
is_this_drop_ok (GtkWidget *widget, GdkDragContext *context)
{
	static GdkAtom text_uri_list = 0;
	GList *li;
	GtkWidget *wid;

	wid = gtk_drag_get_source_widget(context);

	if(wid == widget)
		return FALSE;

	if(!(context->actions & GDK_ACTION_COPY))
		return FALSE;

	if (text_uri_list == 0)
		text_uri_list = gdk_atom_intern ("text/uri-list", FALSE);

	for (li = context->targets; li; li = li->next) {
		if (GPOINTER_TO_INT (li->data) == text_uri_list)
			break;
	}
	/* if we haven't found it */
	if (li == NULL)
		return FALSE;

	return TRUE;
}

static void  
drag_leave_cb(GtkWidget	       *widget,
	      GdkDragContext   *context,
	      guint             time,
	      Launcher *launcher)
{
	button_widget_set_dnd_highlight(BUTTON_WIDGET(widget), FALSE);
}


static gboolean
drag_motion_cb(GtkWidget *widget,
	       GdkDragContext *context,
	       gint x,
	       gint y,
	       guint time,
	       Launcher *launcher)
{
	if ( ! is_this_drop_ok (widget, context))
		return FALSE;

	gdk_drag_status (context, GDK_ACTION_COPY, time);

	button_widget_set_dnd_highlight(BUTTON_WIDGET(widget), TRUE);

	return TRUE;
}

static gboolean
drag_drop_cb (GtkWidget	        *widget,
	      GdkDragContext    *context,
	      gint               x,
	      gint               y,
	      guint              time,
	      Launcher          *launcher)
{
	static GdkAtom text_uri_list = 0;

	if ( ! is_this_drop_ok (widget, context))
		return FALSE;

	if (text_uri_list == 0)
		text_uri_list = gdk_atom_intern ("text/uri-list", FALSE);

	gtk_drag_get_data (widget, context, text_uri_list, time);

	return TRUE;
}

enum {
	TARGET_ICON_INTERNAL,
	TARGET_URI_LIST
};


static void  
drag_data_get_cb (GtkWidget          *widget,
		  GdkDragContext     *context,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint               time,
		  Launcher           *launcher)
{
	char *uri_list;
	const char *location;
	
	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->ditem != NULL);

	location = gnome_desktop_item_get_location (launcher->ditem);

	if (location == NULL)
		launcher_save (launcher);

	if (info == TARGET_URI_LIST) {
		uri_list = g_strconcat ("file:", location, "\r\n", NULL);

		gtk_selection_data_set (selection_data,
					selection_data->target, 8, (guchar *)uri_list,
					strlen (uri_list));
		g_free(uri_list);
	} else if (info == TARGET_ICON_INTERNAL) {
		gtk_selection_data_set (selection_data,
					selection_data->target, 8,
					location, strlen (location));
	}
}



static Launcher *
create_launcher (const char *parameters, GnomeDesktopItem *ditem)
{
	Launcher *launcher;
        static GtkTargetEntry dnd_targets[] = {
		{ "application/x-panel-icon-internal", 0, TARGET_ICON_INTERNAL },
		{ "text/uri-list", 0, TARGET_URI_LIST }
	};

	if (default_app_pixmap == NULL)
		default_app_pixmap = gnome_pixmap_file ("gnome-unknown.png");

	if (ditem == NULL) {
		if (parameters == NULL) {
			return NULL;
		} else if (*parameters == '/') {
			ditem = gnome_desktop_item_new_from_file (parameters,
								  0 /* flags */,
								  NULL /* error */);
		} else {
			char *apps_par, *entry, *extension;

			if (strstr (parameters, ".desktop"))
				extension = NULL;
			else
				extension = ".desktop";

			apps_par = g_strconcat ("gnome/apps/", parameters,
						extension, NULL);
			entry = gnome_datadir_file (apps_par);
			g_free (apps_par);

			if (entry == NULL) {
				/* perhaps just datadir? */
				entry = gnome_datadir_file (parameters);
			}

			if (entry == NULL && merge_merge_dir != NULL) {
				/* the merge dir? */
				entry = g_strconcat (merge_merge_dir, "/",
						     parameters,
						     extension, NULL);
				if ( ! g_file_test (entry, G_FILE_TEST_EXISTS)) {
					g_free (entry);
					entry = NULL;
				}
			}

			/* eek, not found */
			if (entry == NULL) {
				return NULL;
			}

			ditem = gnome_desktop_item_new_from_file (entry,
								  0 /* flags */,
								  NULL /* error */);
			g_free (entry);
		}
	}
	if (ditem == NULL)
		return NULL; /*button is null*/

	launcher = g_new0 (Launcher, 1);

	launcher->info = NULL;
	launcher->button = NULL;
	launcher->dedit = NULL;
	launcher->prop_dialog = NULL;

	/* Icon/Text will be setup later */
	launcher->button = button_widget_new (NULL /* icon */,
					      -1,
					      LAUNCHER_TILE,
					      FALSE,
					      ORIENT_UP,
					      NULL /* text */);

	gtk_widget_show (launcher->button);

	/*A hack since this function only pretends to work on window
	  widgets (which we actually kind of are) this will select
	  some (already selected) events on the panel instead of
	  the launcher window (where they are also selected) but
	  we don't mind*/
	GTK_WIDGET_UNSET_FLAGS (launcher->button, GTK_NO_WINDOW);
	gtk_drag_source_set(launcher->button,
			    GDK_BUTTON1_MASK,
			    dnd_targets, 2,
			    GDK_ACTION_COPY | GDK_ACTION_MOVE);
	GTK_WIDGET_SET_FLAGS (launcher->button, GTK_NO_WINDOW);
	
	/*gtk_drag_dest_set (GTK_WIDGET (launcher->button),
			   GTK_DEST_DEFAULT_ALL,
			   dnd_targets, 2,
			   GDK_ACTION_COPY);*/
	gtk_drag_dest_set (GTK_WIDGET (launcher->button),
			   0, NULL, 0, 0);

	gtk_signal_connect(GTK_OBJECT(launcher->button), "drag_data_get",
			   GTK_SIGNAL_FUNC(drag_data_get_cb),
			   launcher);
	gtk_signal_connect(GTK_OBJECT(launcher->button), "drag_data_received",
			   GTK_SIGNAL_FUNC(drag_data_received_cb),
			   launcher);
	gtk_signal_connect(GTK_OBJECT(launcher->button), "drag_motion",
			   GTK_SIGNAL_FUNC(drag_motion_cb),
			   launcher);
	gtk_signal_connect(GTK_OBJECT(launcher->button), "drag_drop",
			   GTK_SIGNAL_FUNC(drag_drop_cb),
			   launcher);
	gtk_signal_connect(GTK_OBJECT(launcher->button), "drag_leave",
			   GTK_SIGNAL_FUNC(drag_leave_cb),
			   launcher);


	gtk_signal_connect (GTK_OBJECT(launcher->button), "clicked",
			    (GtkSignalFunc) launch_cb,
			    launcher);
	
	gtk_signal_connect (GTK_OBJECT(launcher->button), "destroy",
			    GTK_SIGNAL_FUNC(destroy_launcher),
			    launcher);

	gtk_object_set_user_data(GTK_OBJECT(launcher->button), launcher);

	launcher->ditem = ditem;

	return launcher;
}

static void
setup_button (Launcher *launcher)
{
	const char *comment;
	const char *name;
	const char *docpath;
	char *real_docpath;
	char *str;
	char *icon;

	name = gnome_desktop_item_get_string (launcher->ditem,
					      GNOME_DESKTOP_ITEM_NAME);
	comment = gnome_desktop_item_get_string (launcher->ditem,
						 GNOME_DESKTOP_ITEM_COMMENT);

	/* GnomeDesktopItem makes this guarantee */
	g_assert (name != NULL);

	/* Setup tooltip */
	if (comment != NULL)
		str = g_strdup_printf ("%s\n%s", name, comment);
	else
		str = g_strdup (name);
	gtk_tooltips_set_tip (panel_tooltips, launcher->button,
			      str, NULL);
	g_free (str);

	/* Setup text */
	button_widget_set_text (BUTTON_WIDGET (launcher->button), name);

	/* Setup icon */
	icon = gnome_desktop_item_get_icon (launcher->ditem);
	if (icon == NULL ||
	    ! button_widget_set_pixmap (BUTTON_WIDGET (launcher->button),
					icon, -1))
		button_widget_set_pixmap (BUTTON_WIDGET (launcher->button),
					  default_app_pixmap, -1);

	/* Setup help */
	docpath = gnome_desktop_item_get_string (launcher->ditem,
						 "DocPath");
	applet_remove_callback (launcher->info, "help_on_app");
	real_docpath = panel_gnome_kde_help_path (docpath);
	if (real_docpath != NULL) {
		char *title;

		g_free (real_docpath);

		title = g_strdup_printf (_("Help on %s"),
					 name,
					 _("Application"));

		applet_add_callback (launcher->info, "help_on_app",
				     GTK_STOCK_HELP,
				     title);
		g_free (title);
	}
}


static void
properties_apply (Launcher *launcher)
{
	char *location;

	/* save (steal) location */
	location = g_strdup (gnome_desktop_item_get_location (launcher->ditem));

	gnome_desktop_item_unref (launcher->ditem);

	launcher->ditem =
		gnome_ditem_edit_get_ditem (GNOME_DITEM_EDIT (launcher->dedit));
	launcher->ditem = gnome_desktop_item_copy (launcher->ditem);

	/* restore location */
	gnome_desktop_item_set_location (launcher->ditem, location);
	g_free (location);

	/* Setup the button look */
	setup_button (launcher);
}

static void
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Launcher *launcher = data;

	launcher->prop_dialog = NULL;
	launcher->dedit = NULL;

	if (launcher->revert_ditem != NULL)
		gnome_desktop_item_unref (launcher->revert_ditem);
	launcher->revert_ditem = NULL;

	panel_config_sync_schedule ();
}

static void
window_clicked (GtkWidget *w, int button, gpointer data)
{
	Launcher *launcher = data;

	if (button == HELP_BUTTON) {
		panel_show_help ("launchers", NULL);
	} else if (button == REVERT_BUTTON) { /* revert */
		gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (launcher->dedit),
					    launcher->revert_ditem);
	} else {
		gnome_dialog_close (GNOME_DIALOG (w));
	}
}

static void
launcher_changed (GtkObject *dedit, gpointer data)
{
	Launcher *launcher = data;

	properties_apply (launcher);
}

static GtkWidget *
create_properties_dialog (Launcher *launcher)
{
	GtkWidget *dialog;
	GList *types;

	/* watch the enum at the top of the file */
	dialog = gnome_dialog_new (_("Launcher properties"),
				   GNOME_STOCK_BUTTON_HELP,
				   _("Revert"),
				   GNOME_STOCK_BUTTON_CLOSE,
				   NULL);
	gnome_dialog_set_close (GNOME_DIALOG (dialog),
				FALSE /* click_closes */);

	launcher->dedit = gnome_ditem_edit_new ();
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    launcher->dedit, TRUE, TRUE, 0);

	gtk_window_set_wmclass (GTK_WINDOW (dialog),
			       "launcher_properties", "Panel");
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, TRUE);
	
	if (launcher->revert_ditem != NULL)
		gnome_desktop_item_unref (launcher->revert_ditem);
	launcher->revert_ditem = gnome_desktop_item_copy (launcher->ditem);

	gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (launcher->dedit),
				    launcher->ditem);

	/* FIXME: this is 1) ugly, 2) needs to be done in a better way,
	 * FIXME: read the dialog proposal */

#if 0
#define SETUP_EDITABLE(entry_name)					\
	gnome_dialog_editable_enters					\
		(GNOME_DIALOG (dialog),					\
		 GTK_EDITABLE (gnome_ditem_get_##entry_name##_entry  	\
			       (GNOME_DITEM_EDIT (launcher->dedit))));

	SETUP_EDITABLE (name);
	SETUP_EDITABLE (comment);
	SETUP_EDITABLE (exec);
	SETUP_EDITABLE (tryexec);
	SETUP_EDITABLE (doc);

#undef SETUP_EDITABLE
#endif
	
	gtk_signal_connect (GTK_OBJECT (launcher->dedit), "changed",
			    GTK_SIGNAL_FUNC (launcher_changed),
			    launcher);

	gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
			    GTK_SIGNAL_FUNC (properties_close_callback),
			    launcher);


	gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
			    GTK_SIGNAL_FUNC (window_clicked),
			    launcher);

	gnome_ditem_edit_grab_focus (GNOME_DITEM_EDIT (launcher->dedit));

	return dialog;
}

void
launcher_properties (Launcher *launcher)
{
	if (launcher->prop_dialog != NULL) {
		gtk_widget_show_now (launcher->prop_dialog);
		gdk_window_raise (launcher->prop_dialog->window);
		return;
	}

	launcher->prop_dialog = create_properties_dialog (launcher);
	gtk_widget_show_all (launcher->prop_dialog);
	panel_set_dialog_layer (launcher->prop_dialog);
}

Launcher *
load_launcher_applet_full (const char *params, GnomeDesktopItem *ditem,
			   PanelWidget *panel, int pos, gboolean exactpos)
{
	Launcher *launcher;
	char *docpath;

	launcher = create_launcher (params, ditem);

	if (launcher == NULL)
		return NULL;

	if ( ! register_toy (launcher->button,
			     launcher, free_launcher,
			     panel, pos, exactpos,
			     APPLET_LAUNCHER)) {
		/* Don't free launcher here, the button has been destroyed
		   above and the launcher structure freed */
		return NULL;
	}

	launcher->info = applets_last->data;

	if ( ! commie_mode)
		applet_add_callback (applets_last->data,"properties",
				     GTK_STOCK_PROPERTIES,
				     _("Properties..."));
	applet_add_callback (applets_last->data, "help",
			     GTK_STOCK_HELP,
			     _("Help"));

	/* setup button according to ditem */
	setup_button (launcher);

	return launcher;
}

static void
really_add_launcher(GtkWidget *dialog, int button, gpointer data)
{
	GnomeDItemEdit *dedit = GNOME_DITEM_EDIT(data);
	int pos = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(dialog),"pos"));
	gboolean exactpos = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(dialog),"exactpos"));
	PanelWidget *panel = gtk_object_get_data(GTK_OBJECT(dialog),"panel");
	GnomeDesktopItem *ditem;
	
	if (button == 0/*ok*/) {
		Launcher *launcher;

		ditem = gnome_ditem_edit_get_ditem (dedit);
		ditem = gnome_desktop_item_copy (ditem);

		launcher = load_launcher_applet_full (NULL, ditem, panel, pos, exactpos);
		if (launcher != NULL)
			launcher_hoard (launcher);

		panel_config_sync_schedule ();
	} else if (button == 2/*help*/) {
		panel_show_help ("launchers", "LAUNCHERS");
		/* just return as we don't want to close */
		return;
	}

	gtk_widget_destroy (dialog);
}

void
ask_about_launcher (const char *file, PanelWidget *panel, int pos, gboolean exactpos)
{
	GtkWidget *dialog;
	GnomeDItemEdit *dee;
	GList *types;
	GnomeDesktopItem *ditem;

	dialog = gnome_dialog_new (_("Create launcher applet"),
				   GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL,
				   GNOME_STOCK_BUTTON_HELP,
				   NULL);
	gtk_window_set_wmclass (GTK_WINDOW (dialog),
				"create_launcher", "Panel");
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, TRUE);
	
	dee = GNOME_DITEM_EDIT (gnome_ditem_edit_new ());
	gtk_box_pack_start (GTK_BOX(GNOME_DIALOG(dialog)->vbox),
			    GTK_WIDGET (dee),
			    TRUE, TRUE, GNOME_PAD_SMALL);

	/* FIXME: this is 1) ugly, 2) needs to be done in a better way,
	 * FIXME: read the dialog proposal */

#if 0
#define SETUP_EDITABLE(entry_name)					\
	gnome_dialog_editable_enters					\
		(GNOME_DIALOG (dialog),					\
		 GTK_EDITABLE (gnome_ditem_get_##entry_name##_entry (dee)));

	SETUP_EDITABLE (name);
	SETUP_EDITABLE (comment);
	SETUP_EDITABLE (exec);
	SETUP_EDITABLE (tryexec);
	SETUP_EDITABLE (doc);

#undef SETUP_EDITABLE
#endif

	ditem = gnome_desktop_item_new ();
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_EXEC, file);
	gnome_desktop_item_set_entry_type (ditem, GNOME_DESKTOP_ITEM_TYPE_APPLICATION);

	gnome_ditem_edit_set_ditem (dee, ditem);

	gnome_desktop_item_unref (ditem);
	
	gtk_object_set_data (GTK_OBJECT(dialog), "pos", GINT_TO_POINTER (pos));
	gtk_object_set_data (GTK_OBJECT(dialog), "exactpos",
			     GINT_TO_POINTER (exactpos));
	gtk_object_set_data (GTK_OBJECT (dialog), "panel", panel);

	gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
			    GTK_SIGNAL_FUNC (really_add_launcher),
			    dee);

	gnome_dialog_close_hides(GNOME_DIALOG(dialog),FALSE);

	gnome_dialog_set_default(GNOME_DIALOG(dialog),0);

	gtk_widget_show_all (dialog);
	panel_set_dialog_layer (dialog);

	gnome_ditem_edit_grab_focus (dee);
}

static void
ditem_set_icon (GnomeDesktopItem *ditem, const char *icon)
{
	if (icon != NULL &&
	    icon[0] != G_DIR_SEPARATOR) {
		char *full = gnome_program_locate_file
			(NULL,
			 GNOME_FILE_DOMAIN_PIXMAP,
			 icon,
			 TRUE /* only_if_exists */,
			 NULL /* ret_locations */);
		if (full == NULL) {
			full = gnome_program_locate_file
				(NULL,
				 GNOME_FILE_DOMAIN_APP_PIXMAP,
				 icon,
				 TRUE /* only_if_exists */,
				 NULL /* ret_locations */);
		}
		if (full != NULL) {
			gnome_desktop_item_set_string (ditem,
						       GNOME_DESKTOP_ITEM_ICON,
						       full);
			g_free (full);
		} else {
			gnome_desktop_item_set_string (ditem,
						       GNOME_DESKTOP_ITEM_ICON,
						       icon);
		}
	} else {
		gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_ICON,
					       icon);
	}
}


Launcher *
load_launcher_applet_from_info (const char *name, const char *comment,
				const char *exec, const char *icon,
				PanelWidget *panel, int pos,
				gboolean exactpos)
{
	GnomeDesktopItem *ditem;
	Launcher *launcher;

	ditem = gnome_desktop_item_new ();
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_NAME, name);
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_COMMENT, comment);
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_EXEC, exec);
	ditem_set_icon (ditem, icon);
	gnome_desktop_item_set_entry_type (ditem,
					   GNOME_DESKTOP_ITEM_TYPE_APPLICATION);

	launcher = load_launcher_applet_full (NULL, ditem, panel, pos, exactpos);
	if (launcher != NULL)
		launcher_save (launcher);

	panel_config_sync_schedule ();

	return launcher;
}

Launcher *
load_launcher_applet_from_info_url (const char *name, const char *comment,
				    const char *url, const char *icon,
				    PanelWidget *panel, int pos,
				    gboolean exactpos)
{
	GnomeDesktopItem *ditem;
	Launcher *launcher;

	ditem = gnome_desktop_item_new ();
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_NAME, name);
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_COMMENT,
				       comment);
	ditem_set_icon (ditem, icon);
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_URL, url);
	gnome_desktop_item_set_entry_type (ditem,
					   GNOME_DESKTOP_ITEM_TYPE_LINK);

	launcher = load_launcher_applet_full (NULL, ditem, panel, pos, exactpos);
	if (launcher != NULL)
		launcher_save (launcher);

	panel_config_sync_schedule ();

	return launcher;
}

Launcher *
load_launcher_applet (const char *params, PanelWidget *panel, int pos,
		      gboolean exactpos)
{
	return load_launcher_applet_full (params, NULL, panel, pos, exactpos);
}

static char *
launcher_get_unique_file (void)
{
	int rnd, word;
#define NUM_OF_WORDS 12
	char *words[] = {
		"foo",
		"bar",
		"blah",
		"gegl",
		"frobate",
		"hadjaha",
		"greasy",
		"hammer",
		"eek",
		"larry",
		"curly",
		"moe",
		NULL};
	char *fname, *full;

	for (;;) {
		rnd = rand ();
		word = rand () % NUM_OF_WORDS;
		fname = g_strdup_printf ("%s-%010x.desktop",
					 words[word],
					 (guint)rnd);
		full = launcher_file_name (fname);
		g_free (fname);

		if ( ! g_file_test (full, G_FILE_TEST_EXISTS))
			return full;
	}

	g_assert_not_reached ();
	return NULL;
}

char *
launcher_file_name (const char *base)
{
#ifdef PER_SESSION_CONFIGURATION
	/* FIXME: this needs to do stuff, or perhaps just ignore per session
	 * configs */
#ifdef __GNUC__
#warning FIXME: per session config must be done for launchers
#endif
#else
	g_return_val_if_fail (base != NULL, NULL);
	return g_strdup_printf ("%s/.gnome/panel.d/default/launchers/%s",
				g_get_home_dir (),
				base);
#endif
}


void
launcher_save (Launcher *launcher)
{
	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->ditem != NULL);

	if (gnome_desktop_item_get_location (launcher->ditem) == NULL)
		gnome_desktop_item_set_location (launcher->ditem,
						 launcher_get_unique_file ());

	gnome_desktop_item_save (launcher->ditem,
				 NULL /* under */,
				 TRUE /* force */,
				 NULL /* FIXME: error */);
	/* FIXME: handle errors */
}

void
launcher_hoard (Launcher *launcher)
{
	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->ditem != NULL);

	gnome_desktop_item_set_location (launcher->ditem, NULL);

	launcher_save (launcher);
}

Launcher *
find_launcher (const char *path)
{
	GSList *li;

	g_return_val_if_fail (path != NULL, NULL);

	for (li = applets; li != NULL; li = li->next) {
		AppletInfo *info = li->data;
		Launcher *launcher;
		const char *location;

		if (info->type != APPLET_LAUNCHER)
			continue;

		launcher = info->data;

		if (launcher->ditem == NULL)
			continue;

		location = gnome_desktop_item_get_location (launcher->ditem);

		if (location != NULL &&
		    strcmp (location, path) == 0)
			return launcher;
	}

	return NULL;
}
