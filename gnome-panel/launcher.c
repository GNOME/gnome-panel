/*
 * GNOME panel launcher module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <config.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include "gnome.h"
#include "panel.h"


#define APPLET_ID "Launcher"


static PanelCmdFunc panel_cmd_func;

gpointer applet_cmd_func(AppletCommand *cmd);

static char *default_app_pixmap;

typedef struct {
	GtkWidget         *button;
	gint               signal_click_tag;
	GnomeDesktopEntry *dentry;
	char              *params;
} Launcher;

typedef struct {
	GtkWidget *dialog;

	GtkWidget *name_entry;
	GtkWidget *comment_entry;
	GtkWidget *execute_entry;
	GtkWidget *icon_entry;
	GtkWidget *documentation_entry;
	GtkWidget *terminal_toggle;

	/*information about this launcher*/
	Launcher *launcher;

	GnomeDesktopEntry *dentry;
} Properties;


static void
free_user_data(GtkWidget *widget, gpointer data)
{
	Launcher *launcher = gtk_object_get_user_data(GTK_OBJECT(widget));
	g_free(launcher->params);
	gnome_desktop_entry_free(launcher->dentry);
	g_free(launcher);
}

static void
launch (GtkWidget *widget, void *data)
{
	GnomeDesktopEntry *item = data;

	gnome_desktop_entry_launch (item);
}

static Launcher *
create_launcher (GtkWidget *window, char *parameters)
{
	GtkWidget *pixmap;
	GnomeDesktopEntry *dentry;
	Launcher *launcher;

	if (!default_app_pixmap)
		default_app_pixmap = gnome_pixmap_file ("gnome-unknown.xpm");

	if (*parameters == '/')
		dentry = gnome_desktop_entry_load (parameters);
	else {
		char *apps_par, *entry, *extension;

		if (strstr (parameters, ".desktop"))
			extension = NULL;
		else
			extension = ".desktop";
		
		apps_par = g_copy_strings ("apps/", parameters, extension, NULL);
		entry = gnome_datadir_file (apps_par);
		g_free (apps_par);
		
		if (!entry)
			return NULL;
		dentry = gnome_desktop_entry_load (entry);
		g_free (entry);
	}
	if (!dentry)
		return NULL; /*button is null*/

	launcher = g_new(Launcher,1);

	launcher->button = gtk_button_new ();
	pixmap = gnome_pixmap_new_from_file (dentry->opaque_icon);
	if (!pixmap)
		pixmap = gnome_pixmap_new_from_file (dentry->transparent_icon);
	if (!pixmap) {
		if (default_app_pixmap)
			pixmap = gnome_pixmap_new_from_file (default_app_pixmap);
		else
			pixmap = gtk_label_new ("App");
	}
	gtk_container_add (GTK_CONTAINER(launcher->button), pixmap);
	gtk_widget_show (pixmap);
	gtk_widget_set_usize (launcher->button, pixmap->requisition.width,
			      pixmap->requisition.height);
	gtk_widget_show (launcher->button);

	launcher->signal_click_tag = gtk_signal_connect (GTK_OBJECT(launcher->button), "clicked",
							 (GtkSignalFunc) launch,
							 dentry);

	gtk_object_set_user_data(GTK_OBJECT(launcher->button), launcher);

	/* The .desktop path is stored in the params field */
	launcher->params = g_strdup(parameters);

	launcher->dentry = dentry;

	gtk_signal_connect(GTK_OBJECT(launcher->button), "destroy",
			   (GtkSignalFunc) free_user_data,
			   NULL);

	return launcher;
}

static GtkWidget *
create_text_entry(GtkWidget *table, int row, char *label, char *text)
{
	GtkWidget *wlabel;
	GtkWidget *entry;

	wlabel = gtk_label_new(label);
	gtk_misc_set_alignment(GTK_MISC(wlabel), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), wlabel,
			 0, 1, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);
	gtk_widget_show(wlabel);

	entry = gtk_entry_new();
	if (text)
		gtk_entry_set_text(GTK_ENTRY(entry), text);
	gtk_table_attach(GTK_TABLE(table), entry,
			 1, 2, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);
	gtk_widget_show(entry);

	return entry;
}

static void
check_dentry_save(GnomeDesktopEntry *dentry)
{
	FILE *file;
	char *pruned;
	char *new_name;
	char *appsdir;

	file = fopen(dentry->location, "w");
	if (file) {
		fclose(file);
		return;
	}

	pruned = strrchr(dentry->location, '/');
	if (pruned)
		pruned++; /* skip over slash */
	else
		pruned = dentry->location;

	appsdir = gnome_util_home_file ("apps");
	mkdir (appsdir, 0755);

	new_name = g_concat_dir_and_file(appsdir, pruned);
	g_free(dentry->location);
	dentry->location = new_name;
}

#define free_and_nullify(x) { g_free(x); x = NULL; }

static void
properties_ok_callback(GtkWidget *widget, gpointer data)
{
	PanelCommand       cmd;
	Properties        *prop;
	GnomeDesktopEntry *dentry;
	GtkWidget         *pixmap;

	prop = data;
	dentry = prop->dentry;

	free_and_nullify(dentry->name);
	free_and_nullify(dentry->comment);
	free_and_nullify(dentry->exec);
	free_and_nullify(dentry->tryexec);
	free_and_nullify(dentry->icon_base);
	free_and_nullify(dentry->docpath);
	free_and_nullify(dentry->type);

	dentry->name      = g_strdup(gtk_entry_get_text(GTK_ENTRY(prop->name_entry)));
	dentry->comment   = g_strdup(gtk_entry_get_text(GTK_ENTRY(prop->comment_entry)));
	dentry->exec      = g_strdup(gtk_entry_get_text(GTK_ENTRY(prop->execute_entry)));
	dentry->icon_base = g_strdup(gtk_entry_get_text(GTK_ENTRY(prop->icon_entry)));
	dentry->docpath   = g_strdup(gtk_entry_get_text(GTK_ENTRY(prop->documentation_entry)));
	dentry->type      = g_strdup("Application"); /* FIXME: should handle more cases */
	dentry->terminal  = GTK_TOGGLE_BUTTON(prop->terminal_toggle)->active;

	check_dentry_save(dentry);
	gnome_desktop_entry_save(dentry);

	dentry=gnome_desktop_entry_load(dentry->location);
	gnome_desktop_entry_free(prop->dentry);
	prop->dentry = dentry;

	/* FIXME: should update the button and pixmap */

	g_free(prop->launcher->params);
	prop->launcher->params=g_strdup(dentry->location);

	cmd.cmd = PANEL_CMD_SET_TOOLTIP;
	cmd.params.set_tooltip.applet  = prop->launcher->button;
	cmd.params.set_tooltip.tooltip = dentry->comment;

	(*panel_cmd_func) (&cmd);
	
	pixmap=GTK_BUTTON(prop->launcher->button)->child;

	gtk_container_remove(GTK_CONTAINER(prop->launcher->button),pixmap);
	gtk_widget_destroy(pixmap);

	pixmap = gnome_pixmap_new_from_file (dentry->opaque_icon);
	if (!pixmap)
		pixmap = gnome_pixmap_new_from_file (dentry->transparent_icon);
	if (!pixmap) {
		if (default_app_pixmap)
			pixmap = gnome_pixmap_new_from_file (default_app_pixmap);
		else
			pixmap = gtk_label_new (_("App"));
	}
	gtk_container_add (GTK_CONTAINER(prop->launcher->button), pixmap);
	gtk_widget_show(pixmap);
	gtk_widget_set_usize (prop->launcher->button, pixmap->requisition.width,
			      pixmap->requisition.height);

	gtk_signal_disconnect(GTK_OBJECT(prop->launcher->button),
			      prop->launcher->signal_click_tag);

	prop->launcher->signal_click_tag = gtk_signal_connect (GTK_OBJECT(prop->launcher->button), "clicked",
							       (GtkSignalFunc) launch,
							       dentry);

	/*replace the dentry in launcher structure with the new one */
	gnome_desktop_entry_free(prop->launcher->dentry);
	prop->launcher->dentry=dentry;
}

#undef free_and_nullify

static void
properties_cancel_callback(GtkWidget *widget, gpointer data)
{
	Properties *prop;

	prop = data;

	gnome_desktop_entry_free(prop->dentry);
	gtk_widget_destroy(prop->dialog);
	g_free(prop);
}

static GtkWidget *
create_properties_dialog(GnomeDesktopEntry *dentry, Launcher *launcher)
{
	Properties *prop;
	GtkWidget  *dialog;
	GtkWidget  *table;
	GtkWidget  *button;
	GtkWidget  *toggle;

	prop = g_new(Properties, 1);
	prop->dentry = dentry;

	prop->launcher = launcher;

	prop->dialog = dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Launcher properties"));
	gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);

	table = gtk_table_new(6, 2, FALSE);
	gtk_container_border_width(GTK_CONTAINER(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, FALSE, 0);
	gtk_widget_show(table);

	prop->name_entry          = create_text_entry(table, 0, _("Name"), dentry->name);
	prop->comment_entry       = create_text_entry(table, 1, _("Comment"), dentry->comment);
	prop->execute_entry       = create_text_entry(table, 2, _("Execute"), dentry->exec);
	prop->icon_entry          = create_text_entry(table, 3, _("Icon"), dentry->icon_base);
	prop->documentation_entry = create_text_entry(table, 4, _("Documentation"), dentry->docpath);

	prop->terminal_toggle = toggle =
		gtk_check_button_new_with_label(_("Run inside terminal"));
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(toggle),
				    dentry->terminal ? TRUE : FALSE);
	gtk_table_attach(GTK_TABLE(table), toggle,
			 0, 2, 4, 5,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);
	gtk_widget_show(toggle);

	button = gtk_button_new_with_label(_("Close"));
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   (GtkSignalFunc) properties_cancel_callback,
			   prop);
	gtk_box_pack_end(GTK_BOX(GTK_DIALOG(dialog)->action_area), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 4);
	
	button = gtk_button_new_with_label(_("Apply"));
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   (GtkSignalFunc) properties_ok_callback,
			   prop);
	gtk_box_pack_end(GTK_BOX(GTK_DIALOG(dialog)->action_area), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	gtk_signal_connect(GTK_OBJECT(dialog), "delete_event",
			   (GtkSignalFunc) properties_cancel_callback,
			   prop);

	return dialog;
}

static void
properties(GtkWidget *widget)
{
	GnomeDesktopEntry *dentry;
	char              *path;
	GtkWidget         *dialog;
	Launcher          *launcher;

	launcher = gtk_object_get_user_data(GTK_OBJECT(widget));
	path = launcher->params;

	dentry = gnome_desktop_entry_load(path);
	if (!dentry) {
		fprintf(stderr,
			"launcher properties: oops, gnome_desktop_entry_load() returned NULL\n"
			"                     on \"%s\"\n", path);
		return;
	}

	dialog = create_properties_dialog(dentry,launcher);
	gtk_widget_show(dialog);
}

static void
create_instance (PanelWidget *panel, char *params, int pos)
{
	Launcher          *launcher;
	PanelCommand       cmd;
	GnomeDesktopEntry *dentry;

	launcher = create_launcher (GTK_WIDGET(panel), params);

	if (!launcher)
		return;

	cmd.cmd = PANEL_CMD_REGISTER_TOY;
	cmd.params.register_toy.applet = launcher->button;
	cmd.params.register_toy.id     = APPLET_ID;
	cmd.params.register_toy.pos    = pos;
	cmd.params.register_toy.flags  = APPLET_HAS_PROPERTIES;

	(*panel_cmd_func) (&cmd);

	dentry = gnome_desktop_entry_load(launcher->params);

	cmd.cmd = PANEL_CMD_SET_TOOLTIP;
	cmd.params.set_tooltip.applet  = launcher->button;
	cmd.params.set_tooltip.tooltip = dentry->comment;

	(*panel_cmd_func) (&cmd);
}

gpointer
applet_cmd_func(AppletCommand *cmd)
{
	Launcher *launcher;

	g_assert(cmd != NULL);

	switch (cmd->cmd) {
		case APPLET_CMD_QUERY:
			return APPLET_ID;

		case APPLET_CMD_INIT_MODULE:
			panel_cmd_func = cmd->params.init_module.cmd_func;
			break;

		case APPLET_CMD_DESTROY_MODULE:
			break;

		case APPLET_CMD_GET_DEFAULT_PARAMS:
			fprintf(stderr, "Launcher: APPLET_CMD_GET_DEFAULT_PARAMS not yet supported\n");
			return g_strdup(""); /* FIXME */

		case APPLET_CMD_CREATE_INSTANCE:
			create_instance(cmd->panel,
					cmd->params.create_instance.params,
					cmd->params.create_instance.pos);
			break;

		case APPLET_CMD_GET_INSTANCE_PARAMS:
			launcher = gtk_object_get_user_data(GTK_OBJECT(cmd->applet));
			if(!launcher) return NULL;
			return g_strdup(launcher->params);

		case APPLET_CMD_ORIENTATION_CHANGE_NOTIFY:
			break;

		case APPLET_CMD_PROPERTIES:
			properties(cmd->applet);
			break;

		default:
			fprintf(stderr,
				APPLET_ID " applet_cmd_func: Oops, unknown command type %d\n",
				(int) cmd->cmd);
			break;
	}

	return NULL;
}
