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
#include <libgnomeui.h>

#include <gnome-desktop/gnome-ditem.h>

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
launch (Launcher *launcher, int argc, char *argv[])
{
#ifdef FIXME
	GnomeDesktopItem *item;
	GNOME_DesktopEntryType type;
	gchar *command;

	g_return_if_fail(launcher != NULL);
	g_return_if_fail(launcher->ditem != NULL);

	item = launcher->ditem;
	type = gnome_desktop_item_get_type(item);
	command = gnome_desktop_item_get_command(item);
	
	if(!command) {
		GtkWidget *dlg;
		dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_MESSAGE_ERROR,
					     GTK_BUTTONS_CLOSE,
					     _("This launch icon does not "
					       "specify a program to run"));
		gtk_window_set_wmclass(GTK_WINDOW(dlg),
				       "no_exec_dialog","Panel");
		gtk_widget_show_all(dlg);
		panel_set_dialog_layer(dlg);
		return;
	}
	
	if (type == GNOME_DESKTOP_ENTRY_TYPE_URL) {
		gnome_url_show (command);
	} else if (type == GNOME_DESKTOP_ENTRY_TYPE_PANEL_APPLET) {
		char *goad_id;

		goad_id = get_applet_goad_id_from_dentry (item);

		if (goad_id != NULL) {
			load_extern_applet (goad_id, NULL,
					    NULL, -1,
					    FALSE, FALSE);
		} else {
			g_warning (_("Can't get goad_id from desktop entry!"));
		}

		g_free (goad_id);
	} else {
		char *curdir = g_get_current_dir ();
		chdir (g_get_home_dir ());

		gnome_desktop_entry_launch_with_args (item, argc, argv);

		chdir (curdir);
		g_free (curdir);
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

	g_free (command);
#endif
}

static void
launch_cb (GtkWidget *widget, gpointer data)
{
	launch(data,0,NULL);
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
#ifdef FIXME
	GList *li, *files;
	int argc;
	char **argv;
	int i;

	g_return_if_fail(launcher!=NULL);

	files = gnome_uri_list_extract_filenames((char *)selection_data->data);
	argc = g_list_length(files);
	argv = g_new(char *,argc+1);
	argv[argc] = NULL;

	for(i=0,li = files; li; i++,li = g_list_next(li))
		argv[i]=li->data;
	
	launch(launcher,argc,argv);
	gnome_uri_list_free_strings (files);
	g_free(argv);
#endif

	gtk_drag_finish(context,TRUE,FALSE,time);
}

static void
destroy_launcher(GtkWidget *widget, gpointer data)
{
	Launcher *launcher = data;
	GtkWidget *prop_dialog = launcher->prop_dialog;

	launcher->prop_dialog = NULL;

	if (prop_dialog != NULL)
		gtk_widget_destroy (prop_dialog);
}

static void
free_launcher(gpointer data)
{
	Launcher *launcher = data;

#ifdef FIXME
	gnome_desktop_item_free(launcher->ditem);
#endif
	launcher->ditem = NULL;

#ifdef FIXME
	if (launcher->revert_ditem != NULL)
		gnome_desktop_item_free(launcher->revert_ditem);
#endif
	launcher->revert_ditem = NULL;

	g_free(launcher);
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
	gchar *uri_list, *location;
	
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

	g_free (location);
}



static Launcher *
create_launcher (const char *parameters, GnomeDesktopItem *ditem)
{
	char *icon;
	Launcher *launcher;
        static GtkTargetEntry dnd_targets[] = {
		{ "application/x-panel-icon-internal", 0, TARGET_ICON_INTERNAL },
		{ "text/uri-list", 0, TARGET_URI_LIST }
	};

	if (default_app_pixmap == NULL)
		default_app_pixmap = gnome_pixmap_file ("gnome-unknown.png");

	if(ditem == NULL) {
#ifdef FIXME
		if (parameters == NULL) {
			return NULL;
		} else if (*parameters == '/') {
			dentry = gnome_desktop_entry_load_unconditional (parameters);
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
				if ( ! panel_file_exists (entry)) {
					g_free (entry);
					entry = NULL;
				}
			}

			/* eek, not found */
			if (entry == NULL) {
				return NULL;
			}

			ditem = gnome_desktop_entry_load_unconditional (entry);
			g_free (entry);
		}
#endif
	}
	if (ditem == NULL)
		return NULL; /*button is null*/

	launcher = g_new0 (Launcher, 1);

	launcher->info = NULL;
	launcher->button = NULL;
	launcher->dedit = NULL;
	launcher->prop_dialog = NULL;

	icon = gnome_desktop_item_get_icon_path(ditem);
	if (icon) {
		gchar *name;

		name = gnome_desktop_item_get_name(ditem, NULL);
		launcher->button = button_widget_new(icon,
						     -1,
						     LAUNCHER_TILE,
						     FALSE,
						     ORIENT_UP,
						     name);
		g_free(name);
		g_free(icon);
	}
	if (!launcher->button) {
		launcher->button =
			button_widget_new(default_app_pixmap,-1,
					  LAUNCHER_TILE,
					  FALSE,ORIENT_UP,
					  _("App"));
	}
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
properties_apply (Launcher *launcher)
{
#ifdef FIXME
	char *icon;
	char *location;
	char *docpath;

	/* save (steal) location */
	location = gnome_desktop_item_get_location(launcher->ditem);

	gnome_desktop_entry_free(launcher->ditem);

	launcher->ditem =
		gnome_ditem_get_ditem(GNOME_DITEM_EDIT(launcher->dedit));

	/* restore location */
	launcher->ditem->location = location;

	if (string_empty (launcher->ditem->name)) {
		g_free (launcher->ditem->name);
		launcher->ditem->name = g_strdup ("???");
	}

	gtk_tooltips_set_tip (panel_tooltips,launcher->button,
			      launcher->ditem->comment,NULL);
	
	button_widget_set_text (BUTTON_WIDGET(launcher->button),
				launcher->ditem->name);
	icon = launcher->ditem->icon;
	if ( ! string_empty (icon)) {
		/* Sigh, now we need to make them local to the gnome
		   install */
		if (*icon != '/') {
			launcher->ditem->icon = gnome_pixmap_file (icon);
			g_free (icon);
		}
		if(!button_widget_set_pixmap (BUTTON_WIDGET(launcher->button),
					      launcher->ditem->icon,
					      -1))
			button_widget_set_pixmap (BUTTON_WIDGET(launcher->button),
						  default_app_pixmap,
						  -1);
	} else {
		button_widget_set_pixmap(BUTTON_WIDGET(launcher->button),
					 default_app_pixmap, -1);
	}

	applet_remove_callback (launcher->info, "help_on_app");
	docpath = panel_gnome_kde_help_path (launcher->ditem->docpath);
	if (docpath != NULL) {
		char *title;

		g_free (docpath);

		title = g_strdup_printf (_("Help on %s"),
					 launcher->ditem->name != NULL ?
					 launcher->ditem->name :
					 _("Application"));

		applet_add_callback (launcher->info, "help_on_app",
				     GNOME_STOCK_PIXMAP_HELP,
				     title);
		g_free (title);
	}
#endif
}

static void
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Launcher *launcher = data;

	launcher->prop_dialog = NULL;
	launcher->dedit = NULL;

#ifdef FIXME
	if (launcher->revert_ditem != NULL)
		gnome_desktop_item_free (launcher->revert_ditem);
#endif
	launcher->revert_ditem = NULL;

	panel_config_sync_schedule ();
}

static void
window_clicked (GtkWidget *w, int button, gpointer data)
{
#ifdef FIXME
	Launcher *launcher = data;

	if (button == HELP_BUTTON) {
		panel_show_help ("launchers.html");
	} else if (button == REVERT_BUTTON) { /* revert */
		gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (launcher->dedit),
					      launcher->revert_ditem);
	} else {
		gnome_dialog_close (GNOME_DIALOG (w));
	}
#endif
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
#ifdef FIXME
	GtkWidget *dialog;
	GtkWidget *notebook;
	GList *types;

	/* watch the enum at the top of the file */
	dialog = gnome_dialog_new (_("Launcher properties"),
				   GNOME_STOCK_BUTTON_HELP,
				   _("Revert"),
				   GNOME_STOCK_BUTTON_CLOSE,
				   NULL);
	gnome_dialog_set_close (GNOME_DIALOG (dialog),
				FALSE /* click_closes */);

	notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    notebook, TRUE, TRUE, 0);

	gtk_window_set_wmclass(GTK_WINDOW(dialog),
			       "launcher_properties", "Panel");
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	launcher->dedit =
		gnome_ditem_edit_new_notebook(GTK_NOTEBOOK(notebook));
	hack_ditem_edit (GNOME_DITEM_EDIT (launcher->dedit));
	
	types = NULL;
	types = g_list_append (types, "Application");
	types = g_list_append (types, "URL");
	types = g_list_append (types, "PanelApplet");
	gtk_combo_set_popdown_strings (GTK_COMBO (GNOME_DITEM_EDIT (launcher->dedit)->type_combo), types);
	g_list_free (types);
	types = NULL;

	if (launcher->revert_ditem != NULL)
		gnome_desktop_entry_free (launcher->revert_ditem);
	launcher->revert_ditem = gnome_desktop_entry_copy (launcher->ditem);

	gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (launcher->dedit),
				      launcher->ditem);

	/* This sucks, but there is no other way to do this with the current
	   GnomeDitem API.  */

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
	
	gtk_signal_connect (GTK_OBJECT (launcher->dedit), "changed",
			    GTK_SIGNAL_FUNC (launcher_changed),
			    launcher);

	gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
			    GTK_SIGNAL_FUNC (properties_close_callback),
			    launcher);

	/* YAIKES, the problem here is that the notebook will attempt
	 * to destroy the dedit, so if we unref it in the close handler,
	 * it will be finalized by the time the notebook will destroy it,
	 * dedit is just a horrible thing */
	gtk_signal_connect (GTK_OBJECT (launcher->dedit), "destroy",
			    GTK_SIGNAL_FUNC (gtk_object_unref),
			    NULL);


	gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
			    GTK_SIGNAL_FUNC (window_clicked),
			    launcher);

	gtk_widget_grab_focus
		(gnome_ditem_get_name_entry
		 (GNOME_DITEM_EDIT (launcher->dedit)));

	return dialog;
#else
	return NULL;
#endif
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

	if(!launcher)
		return NULL;

	if(!register_toy(launcher->button,
			 launcher, free_launcher,
			 panel, pos, exactpos,
			 APPLET_LAUNCHER)) {
		/* Don't free launcher here, the button has been destroyed
		   above and the launcher structure freed */
		return NULL;
	}

	launcher->info = applets_last->data;

#ifdef FIXME
	gtk_tooltips_set_tip (panel_tooltips,
			      launcher->button,
			      launcher->ditem->comment,
			      NULL);
#endif

	if ( ! commie_mode)
		applet_add_callback (applets_last->data,"properties",
				     GTK_STOCK_PROPERTIES,
				     _("Properties..."));
	applet_add_callback (applets_last->data, "help",
			     GTK_STOCK_HELP,
			     _("Help"));

#ifdef FIXME
	docpath = panel_gnome_kde_help_path (launcher->ditem->docpath);
	if (docpath != NULL) {
		char *title;

		g_free (docpath);

		title = g_strdup_printf (_("Help on %s"),
					 launcher->ditem->name != NULL ?
					 launcher->ditem->name :
					 _("Application"));

		applet_add_callback (applets_last->data, "help_on_app",
				     GTK_STOCK_HELP,
				     title);
		g_free (title);
	}
#endif

	return launcher;
}

static void
really_add_launcher(GtkWidget *dialog, int button, gpointer data)
{
#ifdef FIXME
	GnomeDitemEdit *dedit = GNOME_DITEM_EDIT(data);
	int pos = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(dialog),"pos"));
	gboolean exactpos = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(dialog),"exactpos"));
	PanelWidget *panel = gtk_object_get_data(GTK_OBJECT(dialog),"panel");
	GnomeDesktopItem *ditem;
	
	if(button == 0/*ok*/) {
		Launcher *launcher;

		ditem = gnome_ditem_get_ditem(dedit);

		if (string_empty (ditem->name)) {
			g_free (ditem->name);
			ditem->name = g_strdup ("???");
		}
		launcher = load_launcher_applet_full (NULL, ditem, panel, pos, exactpos);
		if (launcher != NULL)
			launcher_hoard (launcher);

		panel_config_sync_schedule ();
	} else if(button == 2/*help*/) {
		panel_show_help ("launchers.html#LAUNCHERS");
		/* just return as we don't want to close */
		return;
	}

	gtk_widget_destroy (dialog);
#endif
}

void
ask_about_launcher (const char *file, PanelWidget *panel, int pos, gboolean exactpos)
{
#ifdef FIXME
	GtkWidget *dialog;
	GtkWidget *notebook;
	GnomeDitemEdit *dee;
	GList *types;

	dialog = gnome_dialog_new (_("Create launcher applet"),
				   GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL,
				   GNOME_STOCK_BUTTON_HELP,
				   NULL);
	gtk_window_set_wmclass (GTK_WINDOW (dialog),
				"create_launcher", "Panel");
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, TRUE);
	
	notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX(GNOME_DIALOG(dialog)->vbox), notebook,
			    TRUE, TRUE, GNOME_PAD_SMALL);
	dee = GNOME_DITEM_EDIT(gnome_ditem_edit_new_notebook(GTK_NOTEBOOK(notebook)));
	hack_ditem_edit (dee);

	types = NULL;
	types = g_list_append(types, "Application");
	types = g_list_append(types, "URL");
	types = g_list_append(types, "PanelApplet");
	gtk_combo_set_popdown_strings(GTK_COMBO(dee->type_combo), types);
	g_list_free(types);
	types = NULL;

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
	
	if (file != NULL)
		gtk_entry_set_text(GTK_ENTRY(dee->exec_entry), file);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(dee->type_combo)->entry),
			   "Application");
	
	
	gtk_object_set_data (GTK_OBJECT(dialog), "pos", GINT_TO_POINTER (pos));
	gtk_object_set_data (GTK_OBJECT(dialog), "exactpos",
			     GINT_TO_POINTER (exactpos));
	gtk_object_set_data (GTK_OBJECT (dialog), "panel", panel);

	gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
			    GTK_SIGNAL_FUNC (really_add_launcher),
			    dee);
	/* YAIKES, the problem here is that the notebook will attempt
	 * to destroy the dedit, so if we unref it in the close handler,
	 * it will be finalized by the time the notebook will destroy it,
	 * dedit is just a horrible thing */
	gtk_signal_connect (GTK_OBJECT (dee), "destroy",
			    GTK_SIGNAL_FUNC (gtk_object_unref), NULL);

	gnome_dialog_close_hides(GNOME_DIALOG(dialog),FALSE);

	gnome_dialog_set_default(GNOME_DIALOG(dialog),0);

	gtk_widget_show_all (dialog);
	panel_set_dialog_layer (dialog);

	gtk_widget_grab_focus (gnome_ditem_get_name_entry (dee));
#endif
}

Launcher *
load_launcher_applet_from_info (const char *name, const char *comment,
				char **exec, int execn, const char *icon,
				PanelWidget *panel, int pos,
				gboolean exactpos)
{
#ifdef FIXME
	GnomeDesktopItem *ditem = g_new0 (GnomeDesktopItem, 1);
	Launcher *launcher;

	ditem->name = g_strdup (name);
	ditem->comment = g_strdup (comment);
	ditem->exec_length = execn;
	ditem->exec = g_copy_vector (exec);

	if (icon != NULL &&
	    icon[0] != '/')
		ditem->icon = gnome_pixmap_file (icon);
	else
		ditem->icon = g_strdup (icon);
	
	ditem->type = g_strdup ("Application");

	launcher = load_launcher_applet_full (NULL, ditem, panel, pos, exactpos);
	if (launcher != NULL)
		launcher_save (launcher);

	panel_config_sync_schedule ();

	return launcher;
#else
	return NULL;
#endif
}

Launcher *
load_launcher_applet_from_info_url (const char *name, const char *comment,
				    const char *url, const char *icon,
				    PanelWidget *panel, int pos,
				    gboolean exactpos)
{
#ifdef FIXME
	char *exec[] = { NULL, NULL };
	GnomeDesktopItem *ditem = g_new0 (GnomeDesktopItem, 1);
	Launcher *launcher;

	ditem->name = g_strdup (name);
	ditem->comment = g_strdup (comment);
	ditem->exec_length = 1;
	exec[0] = (char *)url;
	ditem->exec = g_copy_vector (exec);

	if (icon != NULL &&
	    icon[0] != '/')
		ditem->icon = gnome_pixmap_file (icon);
	else
		ditem->icon = g_strdup (icon);
	ditem->type = g_strdup ("URL");

	launcher = load_launcher_applet_full (NULL, ditem, panel, pos, exactpos);
	if (launcher != NULL)
		launcher_save (launcher);

	panel_config_sync_schedule ();

	return launcher;
#else
	return NULL;
#endif
}

Launcher *
load_launcher_applet (const char *params, PanelWidget *panel, int pos,
		      gboolean exactpos)
{
	return load_launcher_applet_full (params, NULL, panel, pos, exactpos);
}

/* an imperfect conversion to gnome style, it's mostly the same but not
 * completely, this should work for 90% of cases */
static void
convert_ditem_to_gnome (GnomeDesktopItem *ditem)
{
#ifdef FIXME
	int i;

	ditem->is_kde = FALSE;
	for (i = 0; i < ditem->exec_length; i++) {
		if (strcmp (ditem->exec[i], "\"%c\"") == 0 ||
		    strcmp (ditem->exec[i], "%c") == 0) {
			g_free (ditem->exec[i]);
			ditem->exec[i] = g_strdup_printf ("'%s'",
							   sure_string (ditem->name));
		} else if (ditem->exec[i][0] == '%' &&
			   strlen(ditem->exec[i]) == 2) {
			g_free (ditem->exec[i]);
			ditem->exec[i] = g_strdup ("");
		}
	}

	if (ditem->type != NULL &&
	    strcmp (ditem->type, "KonsoleApplication") == 0) {
		g_free (ditem->type);
		ditem->type = g_strdup ("Application");
		ditem->terminal = 1;
	}
#endif
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

		if ( ! panel_file_exists (full))
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
#ifdef FIXME
	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->ditem != NULL);

	if (launcher->ditem->is_kde)
		convert_ditem_to_gnome (launcher->ditem);

	if (launcher->ditem->location == NULL)
		launcher->ditem->location = launcher_get_unique_file ();

	gnome_desktop_entry_save (launcher->ditem);
#endif
}

void
launcher_hoard (Launcher *launcher)
{
#ifdef FIXME
	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->ditem != NULL);

	if (launcher->ditem->location != NULL) {
		g_free (launcher->ditem->location);
		launcher->ditem->location = NULL;
	}

	launcher_save (launcher);
#endif
}

Launcher *
find_launcher (const char *path)
{
#ifdef FIXME
	GSList *li;

	g_return_val_if_fail (path != NULL, NULL);

	for (li = applets; li != NULL; li = li->next) {
		AppletInfo *info = li->data;
		if (info->type == APPLET_LAUNCHER) {
			Launcher *launcher = info->data;

			if (launcher->ditem != NULL &&
			    launcher->ditem->location != NULL &&
			    strcmp (launcher->ditem->location, path) == 0)
				return launcher;
		}
	}

#endif
	return NULL;
}
