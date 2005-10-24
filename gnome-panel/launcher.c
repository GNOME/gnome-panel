/*
 * GNOME panel launcher module.
 * (C) 1997,1998,1999,2000 The Free Software Foundation
 * (C) 2000 Eazel, Inc.
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 * CORBAized by George Lebl
 * de-CORBAized by George Lebl
 *
 */

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include <glib/gi18n.h>
#include <libgnomeui/gnome-ditem-edit.h>
#include <libgnomeui/gnome-url.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gdk/gdkx.h>

#include "launcher.h"

#include "button-widget.h"
#include "menu.h"
#include "panel-util.h"
#include "panel-config-global.h"
#include "panel-gconf.h"
#include "panel-profile.h"
#include "xstuff.h"
#include "panel-toplevel.h"
#include "panel-a11y.h"
#include "panel-globals.h"
#include "panel-multiscreen.h"
#include "panel-lockdown.h"
#include "panel-compatibility.h"

#undef LAUNCHER_DEBUG

static void properties_apply (Launcher *launcher);
static void launcher_save    (Launcher *launcher);

enum {
	REVERT_BUTTON
};

static GdkScreen *
launcher_get_screen (Launcher *launcher)
{
	PanelWidget *panel_widget;

	g_return_val_if_fail (launcher != NULL, NULL);
	g_return_val_if_fail (launcher->info != NULL, NULL);
	g_return_val_if_fail (launcher->info->widget != NULL, NULL);

	panel_widget = PANEL_WIDGET (launcher->info->widget->parent);

	return gtk_window_get_screen (GTK_WINDOW (panel_widget->toplevel));
}

static void
launcher_widget_open_dialog_destroyed (GtkWidget *dialog,
				       Launcher *launcher)
{
	g_return_if_fail (launcher->error_dialogs != NULL);

	launcher->error_dialogs = g_slist_remove (launcher->error_dialogs, dialog);
}
                                                                                                                            
static void
launcher_widget_destroy_open_dialogs (Launcher *launcher)
{
	GSList *l, *list;

	list = launcher->error_dialogs;
	launcher->error_dialogs = NULL;

	for (l = list; l; l = l->next) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (l->data),
						      G_CALLBACK (launcher_widget_open_dialog_destroyed),
						      launcher);
		gtk_widget_destroy (l->data);
	}
	g_slist_free (list);
}

static void
launcher_register_error_dialog (Launcher *launcher, 
				GtkWidget *dialog)
{
	launcher->error_dialogs = g_slist_append (launcher->error_dialogs,
						  dialog);
	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (launcher_widget_open_dialog_destroyed),
			  launcher);
}

static const char *
panel_launcher_get_filename (const char *path)
{
	char *p;

	if ((p = strstr (path, PANEL_LAUNCHERS_PATH)))
		p += sizeof (PANEL_LAUNCHERS_PATH);

	return p;
}

static void
panel_launcher_save_ditem (GnomeDesktopItem *ditem,
			   gboolean          report_errors,
			   GdkScreen        *screen)
{
	GError     *error = NULL;
	const char  *location;

	g_return_if_fail (ditem != NULL);

	location = gnome_desktop_item_get_location (ditem);
	if (!location) {
		char *uri;

		uri = panel_make_unique_uri (NULL, ".desktop");
		gnome_desktop_item_set_location (ditem, uri);
		g_free (uri);
	} else if (!strchr (location, G_DIR_SEPARATOR)) {
		char *path;

		path = panel_make_full_path (NULL, location);
		gnome_desktop_item_set_location (ditem, path);
		g_free (path);
	}

	gnome_desktop_item_save (ditem, NULL, TRUE, &error);
	if (error) {
		if (report_errors)
			panel_error_dialog (screen,
					    "cannot_save_launcher", TRUE,
					    _("Cannot save launcher to disk"),
					    "%s",
					    error->message);

		g_error_free (error);
	}
}

static void
launch_url (Launcher *launcher)
{
	GnomeDesktopItem *item;
	const char *url;
	GError *error = NULL;
	GdkScreen *screen;

	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->ditem != NULL);

	item = launcher->ditem;
	url = gnome_desktop_item_get_string (item,
					     GNOME_DESKTOP_ITEM_URL);

	screen = launcher_get_screen (launcher);

	if (!url) {
		GtkWidget *error_dialog;

		error_dialog = panel_error_dialog (screen,
						   "no_url_dialog", TRUE,
						   _("Cannot launch icon"),
						   _("This launch icon does not specify a url to show."));
		launcher_register_error_dialog (launcher, error_dialog);
		return;
	}

	gnome_url_show_on_screen (url, screen, &error);
	if (error) {
		GtkWidget *error_dialog;
	
		error_dialog = panel_error_dialog (screen,
						   "cannot_show_url_dialog",
						   TRUE,
						   _("Cannot show %s"),
						   "%s",
						   url,
						   error->message);
		launcher_register_error_dialog (launcher, error_dialog);
		g_clear_error (&error);
	}
}

void
launcher_launch (Launcher  *launcher,
		 GtkWidget *widget)
{
	GnomeDesktopItem *item;

	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->ditem != NULL);

	item = launcher->ditem;

	if (panel_global_config_get_enable_animations ())
		xstuff_zoom_animate (widget, NULL);
	
	if (gnome_desktop_item_get_entry_type (item) == GNOME_DESKTOP_ITEM_TYPE_LINK)
		launch_url (launcher);

	else {
		GError *error = NULL;

		panel_ditem_launch (
			item, NULL, 0, launcher_get_screen (launcher), &error);
		if (error) {
			GtkWidget *error_dialog;

			error_dialog = panel_error_dialog (launcher_get_screen (launcher),
							   "cannot_launch_icon",
							   TRUE,
							   _("Cannot launch icon"),
							   "%s",
							   error->message);
			launcher_register_error_dialog (launcher, error_dialog);
			g_clear_error (&error);
		}
	}
	
	if (panel_global_config_get_drawer_auto_close ()) {
		PanelToplevel *toplevel;
		PanelToplevel *parent;

		toplevel = PANEL_WIDGET (launcher->button->parent)->toplevel;

		if (panel_toplevel_get_is_attached (toplevel)) {
			parent = panel_toplevel_get_attach_toplevel (toplevel);

			while (panel_toplevel_get_is_attached (parent)) {
				toplevel = parent;
				parent = panel_toplevel_get_attach_toplevel (toplevel);
			}

			panel_toplevel_hide (toplevel, FALSE, -1);
		}
	}
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
	GdkScreen  *screen = launcher_get_screen (launcher);
	GError     *error = NULL;
	char      **envp = NULL;

	envp = panel_make_environment_for_screen (screen, NULL);

	if (panel_global_config_get_enable_animations ())
		xstuff_zoom_animate (widget, NULL);
	
	gnome_desktop_item_drop_uri_list_with_env (launcher->ditem,
						   (const char *)selection_data->data,
						   0 /* flags */,
						   envp,
						   &error);
	g_strfreev (envp);

	if (error) {
		GtkWidget *error_dialog;
		error_dialog = panel_error_dialog (launcher_get_screen (launcher),
						   "cannot_launch_icon", TRUE,
						   _("Cannot launch icon"),
						   "%s",
						   error->message);
		launcher_register_error_dialog (launcher, error_dialog);
		g_clear_error (&error);
	}

	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
destroy_launcher (GtkWidget *widget,
		  Launcher  *launcher)
{
	launcher_properties_destroy (launcher);
	launcher_widget_destroy_open_dialogs (launcher);
}

void
launcher_properties_destroy (Launcher *launcher)
{
	GtkWidget *dialog;

	dialog = launcher->prop_dialog;
	launcher->prop_dialog = NULL;

	if (dialog)
		gtk_widget_destroy (dialog);
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

void
panel_launcher_delete (Launcher *launcher)
{
	const char *location;

	if (!(location = gnome_desktop_item_get_location (launcher->ditem)))
		return;

	if (panel_launcher_get_filename (location)) {
		GnomeVFSResult result;

		result = gnome_vfs_unlink (location);
		if (result != GNOME_VFS_OK)
			g_warning ("Error unlinking '%s': %s\n", location,
				   gnome_vfs_result_to_string (result));
	}
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
	static GdkAtom text_uri_list = NULL;

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
	const char *location;
	
	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->ditem != NULL);

	location = gnome_desktop_item_get_location (launcher->ditem);

	if (!location) {
		launcher_save (launcher);
		location = gnome_desktop_item_get_location (launcher->ditem);
	}

	if (info == TARGET_URI_LIST) {
		if (location [0] != '/')
			gtk_selection_data_set (
				selection_data, selection_data->target, 8,
				(guchar *)location, strlen (location));
		else {
			char *uri;

			uri = g_strconcat ("file://", location, "\r\n", NULL);

			gtk_selection_data_set (
				selection_data, selection_data->target, 8,
				(guchar *)uri, strlen (uri));

			g_free (uri);
		}
	} else if (info == TARGET_ICON_INTERNAL)
		gtk_selection_data_set (selection_data,
					selection_data->target, 8,
					(unsigned char *) location,
					strlen (location));
}

static Launcher *
create_launcher (const char *location)
{
	GnomeDesktopItem *ditem;
	Launcher         *launcher;
	GError           *error = NULL;

	if (!location) {
		g_printerr (_("No URI provided for panel launcher desktop file\n"));
		return NULL;
	}

	if (!strchr (location, G_DIR_SEPARATOR)) {
		char *path;

		path = panel_make_full_path (NULL, location);
		ditem = gnome_desktop_item_new_from_file (path, 0, &error);
		g_free (path);
	} else
		ditem = gnome_desktop_item_new_from_uri (location, 0, &error);

	if (!ditem) {
		g_printerr (_("Unable to open desktop file %s for panel launcher%s%s\n"),
			    location,
			    error ? ": " : "",
			    error ? error->message : "");
		if (error)
			g_error_free (error);

		return NULL; /*button is null*/
	}

	launcher = g_new0 (Launcher, 1);

	launcher->info = NULL;
	launcher->button = NULL;
	launcher->dedit = NULL;
	launcher->prop_dialog = NULL;
	launcher->destroy_handler = 0;
	launcher->non_writable = FALSE;

	/* Icon will be setup later */
	launcher->button = button_widget_new (NULL /* icon */,
					      FALSE,
					      PANEL_ORIENTATION_TOP);

	gtk_widget_show (launcher->button);

	/*gtk_drag_dest_set (GTK_WIDGET (launcher->button),
			   GTK_DEST_DEFAULT_ALL,
			   dnd_targets, 2,
			   GDK_ACTION_COPY);*/
	gtk_drag_dest_set (GTK_WIDGET (launcher->button),
			   0, NULL, 0, 0);

	g_signal_connect (launcher->button, "drag_data_get",
			   G_CALLBACK (drag_data_get_cb), launcher);
	g_signal_connect (launcher->button, "drag_data_received",
			   G_CALLBACK (drag_data_received_cb), launcher);
	g_signal_connect (launcher->button, "drag_motion",
			   G_CALLBACK (drag_motion_cb), launcher);
	g_signal_connect (launcher->button, "drag_drop",
			   G_CALLBACK (drag_drop_cb), launcher);
	g_signal_connect (launcher->button, "drag_leave",
			   G_CALLBACK (drag_leave_cb), launcher);
	g_signal_connect_swapped (launcher->button, "clicked",
				  G_CALLBACK (launcher_launch), launcher);

	launcher->destroy_handler =
			g_signal_connect (launcher->button, "destroy",
					  G_CALLBACK (destroy_launcher),
					  launcher);
	launcher->ditem = ditem;

	return launcher;
}

static char *
guess_icon_from_exec (GtkIconTheme     *icon_theme,
		      GnomeDesktopItem *ditem)
{
	const char *exec;
	char       *icon_name;
	char       *retval;

	exec = gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_EXEC);
	if (!exec || !exec [0])
		return NULL;

	icon_name = g_path_get_basename (exec);
	retval = panel_find_icon (icon_theme, icon_name, 48);
	g_free (icon_name);

	return retval;
}

static void
setup_button (Launcher *launcher)
{
	const char *comment;
	const char *name;
	char *str;
	const char *icon;
	char *unescaped_str;
	char *freeme = NULL;
	
	g_return_if_fail (launcher != NULL);

	name = gnome_desktop_item_get_localestring (launcher->ditem,
						    GNOME_DESKTOP_ITEM_NAME);
	comment = gnome_desktop_item_get_localestring (launcher->ditem,
						       GNOME_DESKTOP_ITEM_COMMENT);

	/* Setup tooltip */
	if ( ! string_empty (comment))
		str = g_strdup_printf ("%s\n%s", name, comment);
	else
		str = g_strdup (name);

	/* If we can unescape the string, then we probably have an escaped
	 * string (a location e.g.). If we can't, then it most probably means
	 * we have a % that is not here to encode a character, and we don't
	 * want to unescape in this case. See bug #170516 for details. */
	unescaped_str = gnome_vfs_unescape_string (str, NULL);
	if (unescaped_str) {
		g_free (str);
		str = unescaped_str;
	}

	gtk_tooltips_set_tip (panel_tooltips, launcher->button, str, NULL);
	/* Setup accessible name */
	panel_a11y_set_atk_name_desc (launcher->button, str, NULL);

	g_free (str);

	/* Setup icon */
	icon = gnome_desktop_item_get_string (launcher->ditem,
					      GNOME_DESKTOP_ITEM_ICON);
	if (!icon)
		icon = freeme = guess_icon_from_exec (BUTTON_WIDGET (launcher->button)->icon_theme,
						      launcher->ditem);

	button_widget_set_icon_name (BUTTON_WIDGET (launcher->button), sure_string (icon));

	g_free (freeme);
}

static char *
panel_launcher_hoard (const char *launcher_location)
{
	GnomeDesktopItem *ditem;
	const char       *path;
	char             *retval;

	retval = NULL;
	if (!launcher_location ||
	    !strchr (launcher_location, G_DIR_SEPARATOR) ||
	    (retval = g_strdup (panel_launcher_get_filename (launcher_location))))
		return retval;

	ditem = gnome_desktop_item_new_from_uri (launcher_location, 0, NULL);
	if (!ditem)
		return NULL;

	gnome_desktop_item_set_location (ditem, NULL);
	panel_launcher_save_ditem (ditem, FALSE, NULL);
	
	retval = NULL;
	if ((path = gnome_desktop_item_get_location (ditem)))
		retval = g_strdup (panel_launcher_get_filename (path));

	gnome_desktop_item_unref (ditem);

	return retval;
}

static void
properties_apply (Launcher *launcher)
{
	char *location;
	char *new_location;

	/* save location */
	location = g_strdup (gnome_desktop_item_get_location (launcher->ditem));

	gnome_desktop_item_unref (launcher->ditem);

	launcher->ditem =
		gnome_ditem_edit_get_ditem (GNOME_DITEM_EDIT (launcher->dedit));
	launcher->ditem = gnome_desktop_item_copy (launcher->ditem);

	if ((new_location = panel_launcher_hoard (location))) {
		GConfClient *client;
		const char  *key;

		client = panel_gconf_get_client ();

		key = panel_gconf_full_key (PANEL_GCONF_OBJECTS,
					    launcher->info->id,
					    "launcher_location");

		gconf_client_set_string (client, key, new_location, NULL);

		gnome_desktop_item_set_location (launcher->ditem, new_location);

		g_free (new_location);
	} else {
		gnome_desktop_item_set_location (launcher->ditem, location);
	}

	g_free (location);

	launcher_save (launcher);

	/* Setup the button look */
	setup_button (launcher);

	/* FIXME_FOR_NEW_CONFIG: need to save changed settings here */
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
}

static void
launcher_changed (GObject *dedit, gpointer data)
{
	Launcher         *launcher;
	GnomeDesktopItem *new_ditem;
	const char       *old_exec;
	const char       *new_exec;

	launcher = (Launcher *) data;
	new_ditem = gnome_ditem_edit_get_ditem (GNOME_DITEM_EDIT (dedit));

	old_exec = gnome_desktop_item_get_string (launcher->ditem,
						  GNOME_DESKTOP_ITEM_EXEC);
	new_exec = gnome_desktop_item_get_string (new_ditem,
						  GNOME_DESKTOP_ITEM_EXEC);

	if (!old_exec || !new_exec || strcmp (old_exec, new_exec))
		gnome_desktop_item_clear_attr (new_ditem, "StartupNotify");

	properties_apply (launcher);
}

static void
set_revert_insensitive (GtkWidget *button,	
			gpointer   dummy)
{
     gtk_widget_set_sensitive (button, FALSE);
}

static void
set_revert_sensitive (GtkWidget *button,
		      gpointer   dummy)
{
     gtk_widget_set_sensitive (button, TRUE);
}

static void
window_response (GtkWidget *w, int response, gpointer data)
{
	Launcher *launcher = data;

	if (response == GTK_RESPONSE_HELP) {
		panel_show_help (
			gtk_window_get_screen (GTK_WINDOW (w)),
			"user-guide.xml", "gospanel-52");
	} else if (response == REVERT_BUTTON) { /* revert */
		char *new_location;

		if (launcher->ditem != NULL)
			gnome_desktop_item_unref (launcher->ditem);
		launcher->ditem = gnome_desktop_item_copy (launcher->revert_ditem);

		/* We want to ignore the "changed" signal first */ 
		g_signal_handlers_disconnect_by_func (
				   G_OBJECT (GNOME_DITEM_EDIT(launcher->dedit)),
				   G_CALLBACK(launcher_changed),
				   launcher);

		gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (launcher->dedit),
					    launcher->revert_ditem);

		/* connect the launcher_changed () callback for the "changed"
		 * signal again
		 */
		g_signal_connect (G_OBJECT (launcher->dedit), "changed",
				  G_CALLBACK (launcher_changed),
				  launcher);

		if ((new_location = panel_launcher_hoard (gnome_desktop_item_get_location (launcher->ditem)))) {
			GConfClient *client;
			const char  *key;

			client = panel_gconf_get_client ();

			key = panel_gconf_full_key (PANEL_GCONF_OBJECTS,
						    launcher->info->id,
						    "launcher_location");

			gconf_client_set_string (client, key, new_location, NULL);

			gnome_desktop_item_set_location (launcher->ditem, new_location);

			g_free (new_location);
		}

		/* resave launcher */
		launcher_save (launcher);

		/* Setup the button look */
		setup_button (launcher);

	} else {
		gtk_widget_destroy (w);
	}
}

static GtkWidget *
create_properties_dialog (Launcher  *launcher,
			  GdkScreen *screen)
{
	GtkWidget *dialog;
        GtkWidget *help;
        GtkWidget *close;
        GtkWidget *revert;

	dialog = gtk_dialog_new ();

	gtk_window_set_title (GTK_WINDOW (dialog), _("Launcher Properties"));
	gtk_window_set_screen (GTK_WINDOW (dialog), screen);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	help = gtk_dialog_add_button (
			GTK_DIALOG (dialog), GTK_STOCK_HELP, GTK_RESPONSE_HELP);

	revert = gtk_dialog_add_button (
			GTK_DIALOG (dialog), GTK_STOCK_REVERT_TO_SAVED, REVERT_BUTTON);
        gtk_widget_set_sensitive (revert, FALSE);

	close = gtk_dialog_add_button (
			GTK_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

	launcher->dedit = gnome_ditem_edit_new ();
	gtk_container_set_border_width (GTK_CONTAINER (launcher->dedit), 5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    launcher->dedit, TRUE, TRUE, 0);

	gtk_window_set_wmclass (GTK_WINDOW (dialog),
			       "launcher_properties", "Panel");
	
	if (launcher->revert_ditem != NULL)
		gnome_desktop_item_unref (launcher->revert_ditem);
	launcher->revert_ditem = gnome_desktop_item_copy (launcher->ditem);

	gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (launcher->dedit),
				    launcher->ditem);

	g_signal_connect (launcher->dedit, "changed",
			  G_CALLBACK (launcher_changed),
			  launcher);
	
	panel_widget_register_open_dialog (PANEL_WIDGET 
					   (launcher->info->widget->parent),
					   dialog);

        g_signal_connect_swapped (launcher->dedit, "changed",
				  G_CALLBACK (set_revert_sensitive),
				  revert);

	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (properties_close_callback),
			  launcher);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (window_response),
			  launcher);

	g_signal_connect (revert, "clicked",
			  G_CALLBACK (set_revert_insensitive),
			  NULL);

	gnome_ditem_edit_grab_focus (GNOME_DITEM_EDIT (launcher->dedit));

	return dialog;
}

void
launcher_properties (Launcher  *launcher,
		     GdkScreen *screen)
{
	if (launcher->prop_dialog != NULL) {
		gtk_window_set_screen (
			GTK_WINDOW (launcher->prop_dialog), screen);
		gtk_window_present (GTK_WINDOW (launcher->prop_dialog));
		return;
	}

	launcher->prop_dialog = create_properties_dialog (launcher, screen);
	gtk_widget_show_all (launcher->prop_dialog);
}

static gboolean
lancher_properties_enabled (void)
{
	if (panel_lockdown_get_locked_down () ||
	    panel_lockdown_get_disable_command_line ())
		return FALSE;

	return TRUE;
}

static Launcher *
load_launcher_applet (const char       *location,
		      PanelWidget      *panel,
		      gboolean          locked,
		      int               pos,
		      gboolean          exactpos,
		      const char       *id)
{
	Launcher *launcher;

	launcher = create_launcher (location);

	if (!launcher)
		return NULL;

	launcher->info = panel_applet_register (launcher->button, launcher,
						free_launcher,
						panel, locked, pos, exactpos,
						PANEL_OBJECT_LAUNCHER, id);
	if (!launcher->info)
		return NULL;

	panel_applet_add_callback (launcher->info,
				   "launch",
				   GTK_STOCK_EXECUTE,
				   _("_Launch"),
				   NULL);

	panel_applet_add_callback (launcher->info,
				   "properties",
				   GTK_STOCK_PROPERTIES,
				   _("_Properties"),
				   lancher_properties_enabled);

	panel_widget_set_applet_expandable (panel, GTK_WIDGET (launcher->button), FALSE, TRUE);
	panel_widget_set_applet_size_constrained (panel, GTK_WIDGET (launcher->button), TRUE);

	/* setup button according to ditem */
	setup_button (launcher);

	return launcher;
}

void
launcher_load_from_gconf (PanelWidget *panel_widget,
			  gboolean     locked,
			  int          position,
			  const char  *id)
{
	GConfClient *client;
	Launcher    *launcher;
	const char  *key;
	char        *launcher_location;

	g_return_if_fail (panel_widget != NULL);
	g_return_if_fail (id != NULL);

	client  = panel_gconf_get_client ();

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, id, "launcher_location");
	panel_compatibility_migrate_applications_scheme (client, key);
	launcher_location = gconf_client_get_string (client, key, NULL);

	if (!launcher_location) {
		g_printerr (_("Key %s is not set, can't load launcher\n"), key);
		return;
	}
        
	launcher = load_launcher_applet (launcher_location,
					 panel_widget,
					 locked,
					 position,
					 TRUE,
					 id);

	if (launcher) {
		key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, id, "launcher_location");
		if (!gconf_client_key_is_writable (client, key, NULL)) {
			AppletUserMenu *menu;

			menu = panel_applet_get_callback (launcher->info->user_menu,
							  "properties");
			if (menu != NULL)
				menu->sensitive = FALSE;
		}
	}

	g_free (launcher_location);
}

static void
ensure_item_localefiled (GnomeDesktopItem *ditem, const char *field)
{
	if (gnome_desktop_item_get_string (ditem, field) == NULL) {
		const char *localized = gnome_desktop_item_get_localestring (ditem, field);
		if (localized != NULL)
			gnome_desktop_item_set_string (ditem, field, localized);
	}
}

static void
really_add_launcher (GtkWidget *dialog, int response, gpointer data)
{
	GtkWidget *err_dialog;
	GnomeDItemEdit *dedit = GNOME_DITEM_EDIT(data);
	int pos = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), "pos"));
	PanelWidget *panel = g_object_get_data (G_OBJECT (dialog), "panel");
	GnomeDesktopItem *ditem;
	
	if (response == GTK_RESPONSE_OK) {
		const char *location;

		ditem = gnome_ditem_edit_get_ditem (dedit);

		/* check for valid name */
		if (string_empty (gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_NAME))) {
			err_dialog = panel_error_dialog (gtk_window_get_screen (GTK_WINDOW (dialog)),
						         "cannot_create_launcher",
							 TRUE,
						         _("Cannot create launcher"),
						         _("You have to specify a name."));
			g_signal_connect_object (G_OBJECT (dialog),
						 "destroy",
						 G_CALLBACK (gtk_widget_destroy),
						 G_OBJECT (err_dialog),
						 G_CONNECT_SWAPPED);
			return;
		}
		

		/* check for valid URL or command */
		if ((gnome_desktop_item_get_entry_type (ditem) == GNOME_DESKTOP_ITEM_TYPE_APPLICATION && 
		     string_empty (gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_EXEC))) ||
		    (gnome_desktop_item_get_entry_type (ditem) == GNOME_DESKTOP_ITEM_TYPE_LINK &&
		     string_empty (gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_URL)))) {
			err_dialog = panel_error_dialog (gtk_window_get_screen (GTK_WINDOW (dialog)),
							 "cannot_create_launcher",
							 TRUE,
							 _("Cannot create launcher"),
							 _("You have to specify a valid URL or command."));
			g_signal_connect_object (G_OBJECT (dialog),
						 "destroy",
						 G_CALLBACK (gtk_widget_destroy),
						 G_OBJECT (err_dialog),
						 G_CONNECT_SWAPPED);
			return;
		}

		/* Make sure we set the "C" locale strings to the terms we set
		   here.  This is so that if the user logs into another locale
		   they get their own description there rather then empty.  It
		   is not the C locale however, but the user created this entry
		   herself so it's OK */
		ensure_item_localefiled (ditem, GNOME_DESKTOP_ITEM_NAME);
		ensure_item_localefiled (ditem, GNOME_DESKTOP_ITEM_GENERIC_NAME);
		ensure_item_localefiled (ditem, GNOME_DESKTOP_ITEM_COMMENT);

		panel_launcher_save_ditem (ditem, TRUE, gtk_window_get_screen (GTK_WINDOW (dialog)));
		location = gnome_desktop_item_get_location (ditem);

		panel_launcher_create (panel->toplevel, pos, location);
	} else if (response == GTK_RESPONSE_HELP) {
		panel_show_help (
			gtk_window_get_screen (GTK_WINDOW (dialog)),
			"user-guide.xml", "gospanel-52");
		/* just return as we don't want to close */
		return;
	}

	gtk_widget_destroy (dialog);
}

void
ask_about_launcher (const char  *file,
		    PanelWidget *panel,
		    int          pos,
		    gboolean     exactpos)
{
	GtkWidget *dialog;
	GnomeDItemEdit *dee;
	GnomeDesktopItem *ditem;

	dialog = gtk_dialog_new_with_buttons (
				_("Create Launcher"),
				NULL,
				0 /* flags */,
				GTK_STOCK_HELP, GTK_RESPONSE_HELP,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);

	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	gtk_window_set_wmclass (GTK_WINDOW (dialog),
				"create_launcher", "Panel");
	
	dee = GNOME_DITEM_EDIT (gnome_ditem_edit_new ());
	gtk_container_set_border_width (GTK_CONTAINER (dee), 5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    GTK_WIDGET (dee), TRUE, TRUE, 0);

	panel_widget_register_open_dialog (panel, dialog);

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

	gtk_window_set_screen (GTK_WINDOW (dialog), gtk_widget_get_screen (GTK_WIDGET (panel)));

	gtk_widget_show_all (dialog);

	gnome_ditem_edit_grab_focus (dee);
}

void
panel_launcher_create_from_info (PanelToplevel *toplevel,
				 int            position,
				 gboolean       exec_info,
				 const char    *exec_or_uri,
				 const char    *name,
				 const char    *comment,
				 const char    *icon)
{
	GnomeDesktopItem *ditem;
	const char       *location;

	ditem = gnome_desktop_item_new ();

	/* set current language and the "C" locale to this name,
	 * this is kind of evil... */
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_NAME, name);
	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_COMMENT, comment);
	gnome_desktop_item_set_localestring (ditem, GNOME_DESKTOP_ITEM_NAME, name);
	gnome_desktop_item_set_localestring (ditem, GNOME_DESKTOP_ITEM_COMMENT, comment);

	gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_ICON, icon);

	if (exec_info) {
		gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_EXEC, exec_or_uri);
		gnome_desktop_item_set_entry_type (ditem, GNOME_DESKTOP_ITEM_TYPE_APPLICATION);
	} else {
		gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_URL, exec_or_uri);
		gnome_desktop_item_set_entry_type (ditem, GNOME_DESKTOP_ITEM_TYPE_LINK);
	}

	panel_launcher_save_ditem (ditem, TRUE, gtk_window_get_screen (GTK_WINDOW (toplevel)));
	location = gnome_desktop_item_get_location (ditem);

	panel_launcher_create (toplevel, position, location);

	gnome_desktop_item_unref (ditem);
}

void
panel_launcher_create_with_id (const char    *toplevel_id,
			       int            position,
			       const char    *location)
{
	GConfClient *client;
	const char  *key;
	char        *id;
	char        *new_location;

	g_return_if_fail (location != NULL);

	client = panel_gconf_get_client ();

	id = panel_profile_prepare_object_with_id (PANEL_OBJECT_LAUNCHER, toplevel_id, position, FALSE);

	new_location = panel_launcher_hoard (location);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS,
				    id,
				    "launcher_location");
	if (new_location)
		gconf_client_set_string (client, key, new_location, NULL);
	else
		gconf_client_set_string (client, key, location, NULL);

	panel_profile_add_to_list (PANEL_GCONF_OBJECTS, id);

	g_free (new_location);
	g_free (id);
}

void
panel_launcher_create (PanelToplevel *toplevel,
		       int            position,
		       const char    *location)
{
	panel_launcher_create_with_id (panel_profile_get_toplevel_id (toplevel),
				       position,
				       location);
}

void
panel_launcher_create_copy (PanelToplevel *toplevel,
			    int            position,
			    const char    *location)
{
	GnomeVFSURI *source_uri;
	GnomeVFSURI *dest_uri;
	char        *new_location;
	const char  *filename;

	new_location = panel_make_unique_uri (NULL, ".desktop");
	
	source_uri = gnome_vfs_uri_new (location);
	dest_uri   = gnome_vfs_uri_new (new_location);

	gnome_vfs_xfer_uri (source_uri,
			    dest_uri,
			    GNOME_VFS_XFER_FOLLOW_LINKS,
			    GNOME_VFS_XFER_ERROR_MODE_ABORT,
			    GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
			    NULL, NULL);

	gnome_vfs_uri_unref (source_uri);
	gnome_vfs_uri_unref (dest_uri);

	filename = panel_launcher_get_filename (new_location);
	panel_launcher_create (toplevel, position, filename);
	g_free (new_location);
}

static void
launcher_save (Launcher *launcher)
{
	g_return_if_fail (launcher != NULL);
	g_return_if_fail (launcher->ditem != NULL);

	panel_launcher_save_ditem (launcher->ditem, TRUE, launcher_get_screen (launcher));
}

Launcher *
find_launcher (const char *path)
{
	GSList *l;

	g_return_val_if_fail (path != NULL, NULL);

	for (l = panel_applet_list_applets (); l; l = l->next) {
		AppletInfo *info = l->data;
		Launcher *launcher;
		const char *location;

		if (info->type != PANEL_OBJECT_LAUNCHER)
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

void
panel_launcher_set_dnd_enabled (Launcher *launcher,
				gboolean  dnd_enabled)
{
	if (dnd_enabled) {
		static GtkTargetEntry dnd_targets[] = {
			{ "application/x-panel-icon-internal", 0, TARGET_ICON_INTERNAL },
			{ "text/uri-list", 0, TARGET_URI_LIST }
		};

		GTK_WIDGET_UNSET_FLAGS (launcher->button, GTK_NO_WINDOW);
		gtk_drag_source_set (launcher->button,
				     GDK_BUTTON1_MASK,
				     dnd_targets, 2,
				     GDK_ACTION_COPY | GDK_ACTION_MOVE);
		//FIXME: this doesn't work since the pixbuf isn't loaded yet
		if (BUTTON_WIDGET (launcher->button)->pixbuf)
			gtk_drag_source_set_icon_pixbuf (launcher->button,
							 BUTTON_WIDGET (launcher->button)->pixbuf);
		GTK_WIDGET_SET_FLAGS (launcher->button, GTK_NO_WINDOW);
	

	} else
		gtk_drag_source_unset (launcher->button);
}
