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

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomevfs/gnome-vfs.h>

#include "launcher.h"

#include "basep-widget.h"
#include "button-widget.h"
#include "drawer-widget.h"
#include "menu-fentry.h"
#include "menu.h"
#include "panel-util.h"
#include "panel-config-global.h"
#include "panel-gconf.h"
#include "session.h"

#include "gnome-desktop-item.h"
#include "gnome-ditem-edit.h"
#include "quick-desktop-reader.h"

#undef LAUNCHER_DEBUG

static void properties_apply (Launcher *launcher);
static void launcher_save    (Launcher *launcher);

extern GtkTooltips *panel_tooltips;

extern GSList *applets;

static char *default_app_pixmap = NULL;

extern GlobalConfig global_config;
extern gboolean commie_mode;

enum {
	REVERT_BUTTON
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
launch (Launcher *launcher)
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
		gnome_desktop_item_launch (item,
					   NULL /* file_list */,
					   0 /* flags */,
					   &error);
		if (error != NULL) {
			panel_error_dialog ("cannot_launch_icon",
					    _("<b>Cannot launch icon</b>\n\n"
					      "Details: %s"),
					    error->message);
			g_clear_error (&error);
		}
	}
	
	if(global_config.drawer_auto_close) {
		GtkWidget *parent =
			PANEL_WIDGET(launcher->button->parent)->panel_parent;
		g_return_if_fail(parent!=NULL);
		if(DRAWER_IS_WIDGET(parent)) {
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
	launch (data);
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
	GError *error = NULL;

	gnome_desktop_item_drop_uri_list (launcher->ditem,
					  (const char *)selection_data->data,
					  0 /* flags */,
					  &error);
	if (error != NULL) {
		panel_error_dialog ("cannot_launch_icon",
				    _("Cannot launch icon\n%s"),
				    error->message);
		g_clear_error (&error);
	}

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
is_this_drop_ok (GtkWidget      *widget,
		 GdkDragContext *context)
{
	static GdkAtom  text_uri_list = GDK_NONE;
	GList           *l;
	GtkWidget       *source;

	source = gtk_drag_get_source_widget (context);

	if (source == widget)
		return FALSE;

	if (!(context->actions & GDK_ACTION_COPY))
		return FALSE;

	if (!text_uri_list)
		text_uri_list = gdk_atom_intern ("text/uri-list", FALSE);

	for (l = context->targets; l; l = l->next) {
		if (GDK_POINTER_TO_ATOM (l->data) == text_uri_list)
			break;
	}

	return l ? TRUE : FALSE;
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

	if (!default_app_pixmap)
		default_app_pixmap = panel_pixmap_discovery ("gnome-unknown.png",
							     FALSE /* fallback */);
#ifdef LAUNCHER_DEBUG
	printf ("Creating Launcher %s\n", parameters);
#endif
	if (ditem == NULL) {
		if (parameters == NULL) {
			return NULL;
		}
#ifdef LAUNCHER_DEBUG
	printf ("Creating gnome_desktop_item_new_from_uri\n");
#endif
		ditem = gnome_desktop_item_new_from_uri (parameters,
							 0 /* flags */,
							 NULL /* error */);

		
		if (ditem == NULL) {
			gchar *entry;

			entry = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_DATADIR, 
							   parameters, TRUE, NULL);

			if (!entry)
				return NULL;

			ditem = gnome_desktop_item_new_from_file (entry, 0, NULL);

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
					      FALSE,
					      PANEL_ORIENT_UP,
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

	g_signal_connect (G_OBJECT(launcher->button), "drag_data_get",
			   G_CALLBACK(drag_data_get_cb),
			   launcher);
	g_signal_connect (G_OBJECT(launcher->button), "drag_data_received",
			   G_CALLBACK(drag_data_received_cb),
			   launcher);
	g_signal_connect (G_OBJECT(launcher->button), "drag_motion",
			   G_CALLBACK(drag_motion_cb),
			   launcher);
	g_signal_connect (G_OBJECT(launcher->button), "drag_drop",
			   G_CALLBACK(drag_drop_cb),
			   launcher);
	g_signal_connect (G_OBJECT(launcher->button), "drag_leave",
			   G_CALLBACK(drag_leave_cb),
			   launcher);


	g_signal_connect (G_OBJECT(launcher->button), "clicked",
			    (GtkSignalFunc) launch_cb,
			    launcher);
	
	g_signal_connect (G_OBJECT(launcher->button), "destroy",
			    G_CALLBACK(destroy_launcher),
			    launcher);

	launcher->ditem = ditem;

	return launcher;
}

static void
setup_button (Launcher *launcher)
{
	const char *comment;
	const char *name;
	const char *docpath;
	char *str;
	char *icon;
	
	g_return_if_fail (launcher != NULL);

	name = gnome_desktop_item_get_localestring (launcher->ditem,
						    GNOME_DESKTOP_ITEM_NAME);
	comment = gnome_desktop_item_get_localestring (launcher->ditem,
						       GNOME_DESKTOP_ITEM_COMMENT);

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
	g_free (icon);

	/* Setup help */
	docpath = gnome_desktop_item_get_string (launcher->ditem,
						 "DocPath");

	panel_applet_remove_callback (launcher->info, "help_on_app");

	if (docpath != NULL) {
		char *title;

		title = g_strdup_printf (_("Help on %s Application"), name);

		panel_applet_add_callback (launcher->info,
					   "help_on_app",
					   GTK_STOCK_HELP,
					   title);
		g_free (title);
	}
}


static void
properties_apply (Launcher *launcher)
{
	char *location;

	/* save location */
	location = g_strdup (gnome_desktop_item_get_location (launcher->ditem));

	gnome_desktop_item_unref (launcher->ditem);

	launcher->ditem =
		gnome_ditem_edit_get_ditem (GNOME_DITEM_EDIT (launcher->dedit));
	launcher->ditem = gnome_desktop_item_copy (launcher->ditem);

	/* restore location */
	gnome_desktop_item_set_location (launcher->ditem, location);
	g_free (location);

	launcher_save (launcher);

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
window_response (GtkWidget *w, int response, gpointer data)
{
	Launcher *launcher = data;

	if (response == GTK_RESPONSE_HELP) {
		panel_show_help ("launchers", NULL);
	} else if (response == REVERT_BUTTON) { /* revert */
		gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (launcher->dedit),
					    launcher->revert_ditem);
	} else {
		gtk_widget_destroy (w);
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

	/* watch the enum at the top of the file */
	dialog = gtk_dialog_new_with_buttons (_("Launcher properties"),
					      NULL /* parent */,
					      0 /* flags */,
					      GTK_STOCK_HELP,
					      GTK_RESPONSE_HELP,
					      GTK_STOCK_REVERT_TO_SAVED,
					      REVERT_BUTTON,
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_CLOSE,
					      NULL);

	launcher->dedit = gnome_ditem_edit_new ();
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    launcher->dedit, TRUE, TRUE, 0);

	gtk_window_set_wmclass (GTK_WINDOW (dialog),
			       "launcher_properties", "Panel");
	
	if (launcher->revert_ditem != NULL)
		gnome_desktop_item_unref (launcher->revert_ditem);
	launcher->revert_ditem = gnome_desktop_item_copy (launcher->ditem);

	gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (launcher->dedit),
				    launcher->ditem);

	g_signal_connect (G_OBJECT (launcher->dedit), "changed",
			    G_CALLBACK (launcher_changed),
			    launcher);

	g_signal_connect (G_OBJECT (dialog), "destroy",
			    G_CALLBACK (properties_close_callback),
			    launcher);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	g_signal_connect (G_OBJECT (dialog), "response",
			    G_CALLBACK (window_response),
			    launcher);

	gnome_ditem_edit_grab_focus (GNOME_DITEM_EDIT (launcher->dedit));

	return dialog;
}

void
launcher_properties (Launcher *launcher)
{
	if (launcher->prop_dialog != NULL) {
		gtk_window_present (GTK_WINDOW (launcher->prop_dialog));
		return;
	}

	launcher->prop_dialog = create_properties_dialog (launcher);
	gtk_widget_show_all (launcher->prop_dialog);
}

void
launcher_save_to_gconf (Launcher   *launcher,
			const char *gconf_key)
{
	const char *location;

	location = gnome_desktop_item_get_location (launcher->ditem);
	if (location) {
		GConfClient *client;
		const char  *profile;
		char        *temp_key;

		client  = panel_gconf_get_client ();
		profile = session_get_current_profile ();

		temp_key = panel_gconf_objects_profile_get_full_key (profile, gconf_key, "launcher-location");
		gconf_client_set_string (client, temp_key, location, NULL);
		g_free (temp_key);
	}
}

void
launcher_load_from_gconf (PanelWidget *panel_widget,
			  gint         position,
			  const char  *gconf_key,
			  gboolean     use_default)
{
        GConfClient *client;
        char        *temp_key;
        char        *launcher_location;

        g_return_if_fail (panel_widget);
        g_return_if_fail (gconf_key);

        client  = panel_gconf_get_client ();
	/* FIXME : We need to do checking of screen dimensions for default */
        temp_key = use_default ? panel_gconf_objects_default_profile_get_full_key ("medium", gconf_key, "launcher-location") :
		panel_gconf_objects_profile_get_full_key (session_get_current_profile (), gconf_key, "launcher-location");
        launcher_location = gconf_client_get_string (client, temp_key, NULL);
        g_free (temp_key);

	load_launcher_applet (launcher_location, panel_widget, position, TRUE, gconf_key);

        g_free (launcher_location);
}

Launcher *
load_launcher_applet_full (const char       *params,
			   GnomeDesktopItem *ditem,
			   PanelWidget      *panel,
			   int               pos,
			   gboolean          exactpos,
			   const char       *gconf_key)
{
	Launcher   *launcher;

	launcher = create_launcher (params, ditem);

	if (launcher == NULL)
		return NULL;

	launcher->info = panel_applet_register (launcher->button, launcher,
						free_launcher, panel, pos, 
						exactpos, APPLET_LAUNCHER,
						gconf_key);
	if (!launcher->info) {
		/* 
		 * Don't free launcher here, the button has 
		 * been destroyed above and the launcher 
		 * structure freed.
		 */
		return NULL;
	}

	if (!commie_mode)
		panel_applet_add_callback (launcher->info,
					   "properties",
					   GTK_STOCK_PROPERTIES,
					   _("Properties..."));

	panel_applet_add_callback (launcher->info, "help", GTK_STOCK_HELP, _("Help"));

	/* setup button according to ditem */
	setup_button (launcher);

	return launcher;
}

static void
really_add_launcher (GtkWidget *dialog, int response, gpointer data)
{
	GnomeDItemEdit *dedit = GNOME_DITEM_EDIT(data);
	int pos = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), "pos"));
	gboolean exactpos = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), "exactpos"));
	PanelWidget *panel = g_object_get_data (G_OBJECT (dialog), "panel");
	GnomeDesktopItem *ditem;
	
	if (response == GTK_RESPONSE_OK) {
		Launcher *launcher;

		ditem = gnome_ditem_edit_get_ditem (dedit);
		ditem = gnome_desktop_item_copy (ditem);

		launcher = load_launcher_applet_full (NULL, ditem, panel, pos, exactpos, NULL);
		if (launcher != NULL)
			launcher_hoard (launcher);

		panel_config_sync_schedule ();
	} else if (response == GTK_RESPONSE_HELP) {
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
	GnomeDesktopItem *ditem;

	dialog = gtk_dialog_new_with_buttons (_("Create launcher applet"),
					      NULL /* parent */,
					      0 /* flags */,
					      GTK_STOCK_HELP,
					      GTK_RESPONSE_HELP,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_OK,
					      NULL);

	gtk_window_set_wmclass (GTK_WINDOW (dialog),
				"create_launcher", "Panel");
	
	dee = GNOME_DITEM_EDIT (gnome_ditem_edit_new ());
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    GTK_WIDGET (dee),
			    TRUE, TRUE, GNOME_PAD_SMALL);

	ditem = gnome_desktop_item_new ();
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_EXEC, file);
	gnome_desktop_item_set_entry_type (ditem, GNOME_DESKTOP_ITEM_TYPE_APPLICATION);

	gnome_ditem_edit_set_ditem (dee, ditem);

	gnome_desktop_item_unref (ditem);
	
	g_object_set_data (G_OBJECT(dialog), "pos", GINT_TO_POINTER (pos));
	g_object_set_data (G_OBJECT(dialog), "exactpos",
			   GINT_TO_POINTER (exactpos));
	g_object_set_data (G_OBJECT (dialog), "panel", panel);

	g_signal_connect (G_OBJECT (dialog), "response",
			    G_CALLBACK (really_add_launcher),
			    dee);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_widget_show_all (dialog);

	gnome_ditem_edit_grab_focus (dee);
}

static void
ditem_set_icon (GnomeDesktopItem *ditem, const char *icon)
{
	if (icon != NULL &&
	    icon[0] != G_DIR_SEPARATOR) {
		char *full = gnome_desktop_item_find_icon (icon,
							   20 /* desired size */,
							   0 /* flags */);
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
	/* set current language and the "C" locale to this name,
	 * this is kind of evil... */
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_NAME, name);
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_COMMENT, comment);
	gnome_desktop_item_set_localestring (ditem, GNOME_DESKTOP_ITEM_NAME, name);
	gnome_desktop_item_set_localestring (ditem, GNOME_DESKTOP_ITEM_COMMENT, comment);

	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_EXEC, exec);
	ditem_set_icon (ditem, icon);
	gnome_desktop_item_set_entry_type (ditem,
					   GNOME_DESKTOP_ITEM_TYPE_APPLICATION);

	launcher = load_launcher_applet_full (NULL, ditem, panel, pos, exactpos, NULL);
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
	/* set current language and the "C" locale to this name,
	 * this is kind of evil... */
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_NAME, name);
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_COMMENT, comment);
	gnome_desktop_item_set_localestring (ditem, GNOME_DESKTOP_ITEM_NAME, name);
	gnome_desktop_item_set_localestring (ditem, GNOME_DESKTOP_ITEM_COMMENT, comment);

	ditem_set_icon (ditem, icon);
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_URL, url);
	gnome_desktop_item_set_entry_type (ditem,
					   GNOME_DESKTOP_ITEM_TYPE_LINK);

	launcher = load_launcher_applet_full (NULL, ditem, panel, pos, exactpos, NULL);
	if (launcher != NULL)
		launcher_save (launcher);

	panel_config_sync_schedule ();

	return launcher;
}

Launcher *
load_launcher_applet (const char  *params,
		      PanelWidget *panel,
		      int          pos,
		      gboolean     exactpos,
		      const char  *gconf_key)
{
	return load_launcher_applet_full (params, NULL, panel, pos, exactpos, gconf_key);
}

static char *
launcher_file_name (const char *location)
{
	char *tmp, *retval;

	g_return_val_if_fail (location!= NULL, NULL);

	tmp = gnome_util_home_file (PANEL_CONFIG_PATH "launchers");
	/* Make sure the launcher directory exists */
	if (!g_file_test (tmp, G_FILE_TEST_EXISTS)) {
		panel_ensure_dir (tmp);
	}

	retval = g_build_filename (tmp, location, NULL);

	g_free (tmp);
	return retval;
}

static char *
launcher_get_unique_uri (void)
{
	int rnd, word;
	char *uri;
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

		if ( ! g_file_test (full, G_FILE_TEST_EXISTS)) {
			uri = gnome_vfs_get_uri_from_local_path (full);
			g_free (full);
			return uri;
		}
	}

	g_assert_not_reached ();
	return NULL;
}

static void
launcher_save (Launcher *launcher)
{
	GError *error;

	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->ditem != NULL);

	if (gnome_desktop_item_get_location (launcher->ditem) == NULL)
		gnome_desktop_item_set_location (launcher->ditem,
						 launcher_get_unique_uri ());

	error = NULL;
	gnome_desktop_item_save (launcher->ditem,
				 NULL /* under */,
				 TRUE /* force */,
				 &error);
	if (error != NULL) {
		panel_error_dialog ("cannot_save_launcher" /* class */,
				    _("Cannot save launcher to disk, "
				      "the following error occured:\n\n"
				      "%s"),
				    error->message);
		g_clear_error (&error);
	}
}

void
launcher_hoard (Launcher *launcher)
{
	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->ditem != NULL);

	gnome_desktop_item_set_location (launcher->ditem, NULL);

	launcher_save (launcher);

	launcher_save_to_gconf (launcher, launcher->info->gconf_key);
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
