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
#include <gnome.h>

#include "panel-include.h"

#include "icon-entry-hack.h"

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
	GnomeDesktopEntry *item;

	g_return_if_fail(launcher != NULL);
	g_return_if_fail(launcher->dentry != NULL);

	item = launcher->dentry;
	
	if(!item->exec) {
		GtkWidget *dlg;
		dlg = gnome_message_box_new(_("This launch icon does not "
					      "specify a program to run"),
					    GNOME_MESSAGE_BOX_ERROR,
					    GNOME_STOCK_BUTTON_CLOSE,
					    NULL);
		gtk_window_set_wmclass(GTK_WINDOW(dlg),
				       "no_exec_dialog","Panel");
		gtk_widget_show_all (dlg);
		panel_set_dialog_layer (dlg);
		return;
	}
	
	if (item->type && strcmp (item->type, "URL") == 0) {
		char *s;
		s = g_strjoinv (" ",item->exec);
		gnome_url_show (s);
		g_free (s);
	} else if (item->type && strcmp (item->type, "PanelApplet") == 0) {
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

	gnome_desktop_entry_free(launcher->dentry);
	launcher->dentry = NULL;

	if (launcher->revert_dentry != NULL)
		gnome_desktop_entry_free(launcher->revert_dentry);
	launcher->revert_dentry = NULL;

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
	gchar *uri_list;
	
	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->dentry != NULL);

	if (launcher->dentry->location == NULL)
		launcher_save (launcher);

	if (info == TARGET_URI_LIST) {
		uri_list = g_strconcat ("file:", launcher->dentry->location,
					"\r\n", NULL);

		gtk_selection_data_set (selection_data,
					selection_data->target, 8, (guchar *)uri_list,
					strlen (uri_list));
		g_free(uri_list);
	} else if (info == TARGET_ICON_INTERNAL) {
		gtk_selection_data_set (selection_data,
					selection_data->target, 8,
					launcher->dentry->location,
					strlen (launcher->dentry->location));
	}
}



static Launcher *
create_launcher (const char *parameters, GnomeDesktopEntry *dentry)
{
	char *icon;
	Launcher *launcher;
        static GtkTargetEntry dnd_targets[] = {
		{ "application/x-panel-icon-internal", 0, TARGET_ICON_INTERNAL },
		{ "text/uri-list", 0, TARGET_URI_LIST }
	};

	if (default_app_pixmap == NULL)
		default_app_pixmap = gnome_pixmap_file ("gnome-unknown.png");

	if(dentry == NULL) {
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

			dentry = gnome_desktop_entry_load_unconditional (entry);
			g_free (entry);
		}
	}
	if (dentry == NULL)
		return NULL; /*button is null*/

	launcher = g_new0 (Launcher, 1);

	launcher->info = NULL;
	launcher->button = NULL;
	launcher->dedit = NULL;
	launcher->prop_dialog = NULL;

	icon = dentry->icon;
	if (icon && *icon) {
		/* Sigh, now we need to make them local to the gnome install */
		if (*icon != '/') {
			dentry->icon = gnome_pixmap_file (icon);
			g_free (icon);
		}
		launcher->button = button_widget_new(dentry->icon,
						     -1,
						     LAUNCHER_TILE,
						     FALSE,
						     ORIENT_UP,
						     dentry->name);
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

	launcher->dentry = dentry;

	return launcher;
}

static void
properties_apply (Launcher *launcher)
{
	char *icon;
	char *location;
	char *docpath;

	/* save (steal) location */
	location = launcher->dentry->location;
	launcher->dentry->location = NULL;

	gnome_desktop_entry_free(launcher->dentry);

	launcher->dentry =
		gnome_dentry_get_dentry(GNOME_DENTRY_EDIT(launcher->dedit));

	/* restore location */
	launcher->dentry->location = location;

	if (string_empty (launcher->dentry->name)) {
		g_free (launcher->dentry->name);
		launcher->dentry->name = g_strdup ("???");
	}

	gtk_tooltips_set_tip (panel_tooltips,launcher->button,
			      launcher->dentry->comment,NULL);
	
	button_widget_set_text (BUTTON_WIDGET(launcher->button),
				launcher->dentry->name);
	icon = launcher->dentry->icon;
	if ( ! string_empty (icon)) {
		/* Sigh, now we need to make them local to the gnome
		   install */
		if (*icon != '/') {
			launcher->dentry->icon = gnome_pixmap_file (icon);
			g_free (icon);
		}
		if(!button_widget_set_pixmap (BUTTON_WIDGET(launcher->button),
					      launcher->dentry->icon,
					      -1))
			button_widget_set_pixmap (BUTTON_WIDGET(launcher->button),
						  default_app_pixmap,
						  -1);
	} else {
		button_widget_set_pixmap(BUTTON_WIDGET(launcher->button),
					 default_app_pixmap, -1);
	}

	applet_remove_callback (launcher->info, "help_on_app");
	docpath = panel_gnome_kde_help_path (launcher->dentry->docpath);
	if (docpath != NULL) {
		char *title;

		g_free (docpath);

		title = g_strdup_printf (_("Help on %s"),
					 launcher->dentry->name != NULL ?
					 launcher->dentry->name :
					 _("Application"));

		applet_add_callback (launcher->info, "help_on_app",
				     GNOME_STOCK_PIXMAP_HELP,
				     title);
		g_free (title);
	}
}

static void
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Launcher *launcher = data;

	launcher->prop_dialog = NULL;
	launcher->dedit = NULL;

	if (launcher->revert_dentry != NULL)
		gnome_desktop_entry_free (launcher->revert_dentry);
	launcher->revert_dentry = NULL;

	panel_config_sync_schedule ();
}

static void
window_clicked (GtkWidget *w, int button, gpointer data)
{
	Launcher *launcher = data;

	if (button == HELP_BUTTON) {
		panel_show_help ("launchers.html");
	} else if (button == REVERT_BUTTON) { /* revert */
		gnome_dentry_edit_set_dentry (GNOME_DENTRY_EDIT (launcher->dedit),
					      launcher->revert_dentry);
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
		gnome_dentry_edit_new_notebook(GTK_NOTEBOOK(notebook));
	hack_dentry_edit (GNOME_DENTRY_EDIT (launcher->dedit));
	
	types = NULL;
	types = g_list_append (types, "Application");
	types = g_list_append (types, "URL");
	types = g_list_append (types, "PanelApplet");
	gtk_combo_set_popdown_strings (GTK_COMBO (GNOME_DENTRY_EDIT (launcher->dedit)->type_combo), types);
	g_list_free (types);
	types = NULL;

	if (launcher->revert_dentry != NULL)
		gnome_desktop_entry_free (launcher->revert_dentry);
	launcher->revert_dentry = gnome_desktop_entry_copy (launcher->dentry);

	gnome_dentry_edit_set_dentry (GNOME_DENTRY_EDIT (launcher->dedit),
				      launcher->dentry);

	/* This sucks, but there is no other way to do this with the current
	   GnomeDEntry API.  */

#define SETUP_EDITABLE(entry_name)					\
	gnome_dialog_editable_enters					\
		(GNOME_DIALOG (dialog),					\
		 GTK_EDITABLE (gnome_dentry_get_##entry_name##_entry  	\
			       (GNOME_DENTRY_EDIT (launcher->dedit))));

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
		(gnome_dentry_get_name_entry
		 (GNOME_DENTRY_EDIT (launcher->dedit)));

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
load_launcher_applet_full (const char *params, GnomeDesktopEntry *dentry,
			   PanelWidget *panel, int pos, gboolean exactpos)
{
	Launcher *launcher;
	char *docpath;

	launcher = create_launcher (params, dentry);

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

	gtk_tooltips_set_tip (panel_tooltips,
			      launcher->button,
			      launcher->dentry->comment,
			      NULL);

	if ( ! commie_mode)
		applet_add_callback (applets_last->data,"properties",
				     GNOME_STOCK_MENU_PROP,
				     _("Properties..."));
	applet_add_callback (applets_last->data, "help",
			     GNOME_STOCK_PIXMAP_HELP,
			     _("Help"));

	docpath = panel_gnome_kde_help_path (launcher->dentry->docpath);
	if (docpath != NULL) {
		char *title;

		g_free (docpath);

		title = g_strdup_printf (_("Help on %s"),
					 launcher->dentry->name != NULL ?
					 launcher->dentry->name :
					 _("Application"));

		applet_add_callback (applets_last->data, "help_on_app",
				     GNOME_STOCK_PIXMAP_HELP,
				     title);
		g_free (title);
	}

	return launcher;
}

static void
really_add_launcher(GtkWidget *dialog, int button, gpointer data)
{
	GnomeDEntryEdit *dedit = GNOME_DENTRY_EDIT(data);
	int pos = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(dialog),"pos"));
	gboolean exactpos = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(dialog),"exactpos"));
	PanelWidget *panel = gtk_object_get_data(GTK_OBJECT(dialog),"panel");
	GnomeDesktopEntry *dentry;
	
	if(button == 0/*ok*/) {
		Launcher *launcher;

		dentry = gnome_dentry_get_dentry(dedit);

		if (string_empty (dentry->name)) {
			g_free (dentry->name);
			dentry->name = g_strdup ("???");
		}
		launcher = load_launcher_applet_full (NULL, dentry, panel, pos, exactpos);
		if (launcher != NULL)
			launcher_hoard (launcher);

		panel_config_sync_schedule ();
	} else if(button == 2/*help*/) {
		panel_show_help ("launchers.html#LAUNCHERS");
		/* just return as we don't want to close */
		return;
	}

	gtk_widget_destroy (dialog);
}

void
ask_about_launcher (const char *file, PanelWidget *panel, int pos, gboolean exactpos)
{
	GtkWidget *dialog;
	GtkWidget *notebook;
	GnomeDEntryEdit *dee;
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
	dee = GNOME_DENTRY_EDIT(gnome_dentry_edit_new_notebook(GTK_NOTEBOOK(notebook)));
	hack_dentry_edit (dee);

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
		 GTK_EDITABLE (gnome_dentry_get_##entry_name##_entry (dee)));

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

	gtk_widget_grab_focus (gnome_dentry_get_name_entry (dee));
}

Launcher *
load_launcher_applet_from_info (const char *name, const char *comment,
				char **exec, int execn, const char *icon,
				PanelWidget *panel, int pos,
				gboolean exactpos)
{
	GnomeDesktopEntry *dentry = g_new0 (GnomeDesktopEntry, 1);
	Launcher *launcher;

	dentry->name = g_strdup (name);
	dentry->comment = g_strdup (comment);
	dentry->exec_length = execn;
	dentry->exec = g_copy_vector (exec);

	if (icon != NULL &&
	    icon[0] != '/')
		dentry->icon = gnome_pixmap_file (icon);
	else
		dentry->icon = g_strdup (icon);
	
	dentry->type = g_strdup ("Application");

	launcher = load_launcher_applet_full (NULL, dentry, panel, pos, exactpos);
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
	char *exec[] = { NULL, NULL };
	GnomeDesktopEntry *dentry = g_new0 (GnomeDesktopEntry, 1);
	Launcher *launcher;

	dentry->name = g_strdup (name);
	dentry->comment = g_strdup (comment);
	dentry->exec_length = 1;
	exec[0] = (char *)url;
	dentry->exec = g_copy_vector (exec);

	if (icon != NULL &&
	    icon[0] != '/')
		dentry->icon = gnome_pixmap_file (icon);
	else
		dentry->icon = g_strdup (icon);
	dentry->type = g_strdup ("URL");

	launcher = load_launcher_applet_full (NULL, dentry, panel, pos, exactpos);
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

/* an imperfect conversion to gnome style, it's mostly the same but not
 * completely, this should work for 90% of cases */
static void
convert_dentry_to_gnome (GnomeDesktopEntry *dentry)
{
	int i;

	dentry->is_kde = FALSE;
	for (i = 0; i < dentry->exec_length; i++) {
		if (strcmp (dentry->exec[i], "\"%c\"") == 0 ||
		    strcmp (dentry->exec[i], "%c") == 0) {
			g_free (dentry->exec[i]);
			dentry->exec[i] = g_strdup_printf ("'%s'",
							   sure_string (dentry->name));
		} else if (dentry->exec[i][0] == '%' &&
			   strlen(dentry->exec[i]) == 2) {
			g_free (dentry->exec[i]);
			dentry->exec[i] = g_strdup ("");
		}
	}

	if (dentry->type != NULL &&
	    strcmp (dentry->type, "KonsoleApplication") == 0) {
		g_free (dentry->type);
		dentry->type = g_strdup ("Application");
		dentry->terminal = 1;
	}
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
	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->dentry != NULL);

	if (launcher->dentry->is_kde)
		convert_dentry_to_gnome (launcher->dentry);

	if (launcher->dentry->location == NULL)
		launcher->dentry->location = launcher_get_unique_file ();

	gnome_desktop_entry_save (launcher->dentry);
}

void
launcher_hoard (Launcher *launcher)
{
	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->dentry != NULL);

	if (launcher->dentry->location != NULL) {
		g_free (launcher->dentry->location);
		launcher->dentry->location = NULL;
	}

	launcher_save (launcher);
}

Launcher *
find_launcher (const char *path)
{
	GSList *li;

	g_return_val_if_fail (path != NULL, NULL);

	for (li = applets; li != NULL; li = li->next) {
		AppletInfo *info = li->data;
		if (info->type == APPLET_LAUNCHER) {
			Launcher *launcher = info->data;

			if (launcher->dentry != NULL &&
			    launcher->dentry->location != NULL &&
			    strcmp (launcher->dentry->location, path) == 0)
				return launcher;
		}
	}

	return NULL;
}
