/*
 * GNOME panel menu-properties module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <gnome.h>

#include "panel-include.h"

#include "icon-entry-hack.h"

/*#define PANEL_DEBUG 1*/

extern GlobalConfig global_config;

extern int config_sync_timeout;
extern int panels_to_sync;
extern int applets_to_sync;
extern int need_complete_save;


struct _MenuDialogInfo {
	GtkWidget *main_menu;
	GtkWidget *global_main;
	GtkWidget *system;
	GtkWidget *system_sub;
	GtkWidget *user;
	GtkWidget *user_sub;
	GtkWidget *applets;
	GtkWidget *applets_sub;
	GtkWidget *distribution;
	GtkWidget *distribution_sub;
 	GtkWidget *kde;
 	GtkWidget *kde_sub;
	GtkWidget *panel;
	GtkWidget *panel_sub;
	GtkWidget *desktop;
	GtkWidget *desktop_sub;
	GtkWidget *pathentry;
	GtkWidget *custom_icon;
	GtkWidget *custom_icon_entry;

	GtkWidget *main_frame;
	GtkWidget *normal_frame;
};

char *
get_real_menu_path(const char *arguments)
{
	char *this_menu;
	
	/*if null, let's put the main menu up*/
	if (string_empty (arguments))
		arguments = ".";

	if(strcmp(arguments, ".") == 0)
		this_menu = gnome_unconditional_datadir_file ("gnome/apps");
	else if (*arguments == '/')
		this_menu = g_strdup (arguments);
	else if (*arguments == '~')
		this_menu = g_concat_dir_and_file (g_get_home_dir(),
						   &arguments[1]);
	else
		this_menu = gnome_unconditional_datadir_file (arguments);

	if (!panel_file_exists (this_menu)) {
		g_warning("menu %s does not exist "
			  "(arguments are %s)",
			  this_menu, arguments);
		g_free (this_menu);
		return NULL;
	}
	return this_menu;
}

char *
get_pixmap(const char *menudir, gboolean main_menu)
{
	char *pixmap_name = NULL;
	if (main_menu) {
		pixmap_name = gnome_unconditional_pixmap_file("gnome-logo-icon-transparent.png");
	} else {
		char *dentry_name;
		GnomeDesktopEntry *item_info;

		dentry_name = g_concat_dir_and_file (menudir,
						     ".directory");
		item_info = gnome_desktop_entry_load (dentry_name);
		g_free (dentry_name);

		if(item_info && item_info->icon)
			pixmap_name = g_strdup(item_info->icon);
		else
			pixmap_name =
				gnome_unconditional_pixmap_file ("gnome-folder.png");

		if (item_info)
			gnome_desktop_entry_free(item_info);
	}
	return pixmap_name;
}

/* Cache this, we don't want to check in the path all the time */
static gboolean
got_gmenu (void)
{
	static gboolean checked = FALSE;
	static gboolean got_it = FALSE;
	char *tmp;

	if (checked)
		return got_it;

	tmp = panel_is_program_in_path ("gmenu");
	if (tmp != NULL)
		got_it = TRUE;
	else
		got_it = FALSE;

	g_free (tmp);

	checked = TRUE;

	return got_it;
}

static void
properties_apply_callback (Menu *menu)
{
	char *s;
	gboolean bool;
	gboolean change_icon = FALSE;
	gboolean need_reload = FALSE;
	char *old_path;
	gboolean old_global_main;
	int old_main_menu_flags;
	gboolean need_edit_menus, got_edit_menus;

	/* Store some old config */
	old_path = g_strdup (menu->path);
	old_global_main = menu->global_main;
	old_main_menu_flags = menu->main_menu_flags;


	/* Start with the icon stuff */
	bool = GTK_TOGGLE_BUTTON (menu->dialog_info->custom_icon)->active;
	if (( ! menu->custom_icon && bool) ||
	    (menu->custom_icon && ! bool)) {
		menu->custom_icon = bool;
		change_icon = TRUE;
	}

	s = hack_icon_entry_get_icon (GNOME_ICON_ENTRY(menu->dialog_info->custom_icon_entry));
	if (menu->custom_icon_file == NULL ||
	    s == NULL ||
	    strcmp (menu->custom_icon_file, s) != 0) {
		g_free (menu->custom_icon_file);
		menu->custom_icon_file = s;
		change_icon = TRUE;
	} else {
		g_free (s);
	}

	need_edit_menus = FALSE;
	if (GTK_TOGGLE_BUTTON (menu->dialog_info->main_menu)->active ||
	    GTK_TOGGLE_BUTTON (menu->dialog_info->global_main)->active) {
		g_free (menu->path);
		menu->path = g_strdup (".");

		if (got_gmenu ()) {
			need_edit_menus = TRUE;
		}
	} else {
		g_free (menu->path);
		s = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (menu->dialog_info->pathentry),
						    TRUE);
		if(s == NULL) {
			g_warning (_("Can't open directory, using main menu!"));
			menu->path = g_strdup (".");
		} else if (*s == '\0') {
			menu->path = g_strdup (".");
		} else {
			menu->path = g_strdup (s);
		}
	}


	/* Setup the edit_menus callback */
	if (applet_get_callback (menu->info->user_menu, "edit_menus") != NULL)
		got_edit_menus = TRUE;
	else
		got_edit_menus = FALSE;

	if (need_edit_menus && ! got_edit_menus)
		applet_add_callback (menu->info, "edit_menus",
				     NULL,
				     _("Edit menus..."));
	else if ( ! need_edit_menus && got_edit_menus)
		applet_remove_callback (menu->info, "edit_menus");



	if (GTK_TOGGLE_BUTTON(menu->dialog_info->global_main)->active)
		 menu->global_main = TRUE;
	else
		 menu->global_main = FALSE;

	menu->main_menu_flags = 0;
	if (GTK_TOGGLE_BUTTON(menu->dialog_info->system_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_SYSTEM_SUB;
	else if (GTK_TOGGLE_BUTTON(menu->dialog_info->system)->active)
		menu->main_menu_flags |= MAIN_MENU_SYSTEM;

	if(GTK_TOGGLE_BUTTON(menu->dialog_info->user_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_USER_SUB;
	else if (GTK_TOGGLE_BUTTON (menu->dialog_info->user)->active)
		menu->main_menu_flags |= MAIN_MENU_USER;

	if(GTK_TOGGLE_BUTTON(menu->dialog_info->applets_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_APPLETS_SUB;
	else if (GTK_TOGGLE_BUTTON (menu->dialog_info->applets)->active)
		menu->main_menu_flags |= MAIN_MENU_APPLETS;

	if(GTK_TOGGLE_BUTTON(menu->dialog_info->distribution_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_DISTRIBUTION_SUB;
	else if (GTK_TOGGLE_BUTTON (menu->dialog_info->distribution)->active)
		menu->main_menu_flags |= MAIN_MENU_DISTRIBUTION;

	if(GTK_TOGGLE_BUTTON(menu->dialog_info->kde_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_KDE_SUB;
	else if (GTK_TOGGLE_BUTTON (menu->dialog_info->kde)->active)
		menu->main_menu_flags |= MAIN_MENU_KDE;

	if(GTK_TOGGLE_BUTTON(menu->dialog_info->panel_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_PANEL_SUB;
	else if (GTK_TOGGLE_BUTTON(menu->dialog_info->panel)->active)
		menu->main_menu_flags |= MAIN_MENU_PANEL;

	if(GTK_TOGGLE_BUTTON(menu->dialog_info->desktop_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_DESKTOP_SUB;
	else if (GTK_TOGGLE_BUTTON(menu->dialog_info->desktop)->active)
		menu->main_menu_flags |= MAIN_MENU_DESKTOP;

	if (strcmp (old_path, menu->path) != 0) {
		need_reload = TRUE;
		change_icon = TRUE;
	}

	if (old_main_menu_flags != menu->main_menu_flags ||
	    ( ! menu->global_main && old_global_main) ||
	    (menu->global_main && ! old_global_main)) {
		need_reload = TRUE;
	}

	g_free (old_path);


	/* Apply menu changes */
	if (need_reload) {
		char *this_menu = get_real_menu_path (menu->path);
		GSList *list = g_slist_append (NULL, this_menu);
		gboolean main_menu = strcmp (menu->path, ".") == 0;
		
		add_menu_widget (menu, PANEL_WIDGET (menu->button->parent),
				 list, main_menu, TRUE);
		
		g_free (this_menu);

		g_slist_free (list);
	}

	/* Apply icon changes */
	if (change_icon) {
		char *this_menu = get_real_menu_path(menu->path);
		char *pixmap_name;

		if (menu->custom_icon &&
		    menu->custom_icon_file != NULL &&
		    panel_file_exists (menu->custom_icon_file))
			pixmap_name = g_strdup (menu->custom_icon_file);
		else
			pixmap_name = get_pixmap(this_menu, (strcmp (menu->path, ".") == 0));
		button_widget_set_pixmap(BUTTON_WIDGET(menu->button),
					 pixmap_name, -1);
	}
}

static void
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Menu *menu = data;

	menu->prop_dialog = NULL;

	g_free (menu->dialog_info);
	menu->dialog_info = NULL;
}

static void
toggle_prop(GtkWidget *widget, gpointer data)
{
	Menu *menu = data;

	if(GTK_TOGGLE_BUTTON(widget)->active)
		properties_apply_callback (menu);
}

static void
toggle_global_main(GtkWidget *widget, gpointer data)
{
	Menu *menu = data;

	if(GTK_TOGGLE_BUTTON(widget)->active) {
		gtk_widget_set_sensitive(menu->dialog_info->main_frame, FALSE);
		gtk_widget_set_sensitive(menu->dialog_info->normal_frame, FALSE);

		properties_apply_callback (menu);
	}
}

static void
toggle_main_menu(GtkWidget *widget, gpointer data)
{
	Menu *menu = data;

	if(GTK_TOGGLE_BUTTON(widget)->active) {
		gtk_widget_set_sensitive(menu->dialog_info->main_frame, TRUE);
		gtk_widget_set_sensitive(menu->dialog_info->normal_frame, FALSE);

		properties_apply_callback (menu);
	}
}

static void
toggle_custom_icon(GtkWidget *widget, gpointer data)
{
	Menu *menu = data;

	if(GTK_TOGGLE_BUTTON(widget)->active) {
		gtk_widget_set_sensitive(menu->dialog_info->custom_icon_entry, TRUE);
	} else {
		gtk_widget_set_sensitive(menu->dialog_info->custom_icon_entry, FALSE);
	}

	properties_apply_callback (menu);
}

static void
toggle_normal_menu(GtkWidget *widget, gpointer data)
{
	Menu *menu = data;

	if(GTK_TOGGLE_BUTTON(widget)->active) {
		gtk_widget_set_sensitive(menu->dialog_info->main_frame, FALSE);
		gtk_widget_set_sensitive(menu->dialog_info->normal_frame, TRUE);

		properties_apply_callback (menu);
	}
}

static void
textbox_changed (GtkWidget *widget, gpointer data)
{
	Menu *menu = data;

	properties_apply_callback (menu);
}

static void
add_menu_type_options(Menu *menu, GtkObject *dialog, GtkTable *table, int row,
		      char *title, GtkWidget **widget, GtkWidget **widget_sub,
		      gboolean on, gboolean sub)
{
	GtkWidget *w;
	GtkWidget *rb;

	w = gtk_label_new(title);
	gtk_table_attach_defaults(table,w,0,1,row,row+1);
	
	rb = w = gtk_radio_button_new_with_label (NULL, _("Off"));
	gtk_table_attach_defaults(table,w,3,4,row,row+1);

	if(!on && !sub)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    menu);
	
	w = gtk_radio_button_new_with_label (gtk_radio_button_group(GTK_RADIO_BUTTON(rb)),
					     _("In a submenu"));
	gtk_table_attach_defaults(table, w, 2, 3, row, row+1);

	*widget_sub = w;

	if (sub)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    menu);
	
	w = gtk_radio_button_new_with_label (gtk_radio_button_group(GTK_RADIO_BUTTON(rb)),
					     _("On the main menu"));
	*widget = w;
	gtk_table_attach_defaults(table, w, 1, 2, row, row+1);
	if (on)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    menu);
}

static void
dialog_clicked (GtkWidget *widget, int button, gpointer data)
{
	Menu *menu = data;

	if (button == 0 /* close */) {
		gnome_dialog_close (GNOME_DIALOG (widget));
	} else if (button == 1 /* help */) {
		if (GTK_TOGGLE_BUTTON (menu->dialog_info->main_menu)->active)
			panel_show_help ("mainmenu.html#MAINMENUCONFIG");
		else
			panel_show_help ("menus.html");
	}
}

static GtkWidget *
create_properties_dialog(Menu *menu)
{
	GtkWidget *dialog, *notebook;
	GtkWidget *vbox;
	GtkWidget *box;
	GtkWidget *table;
	GtkWidget *w, *w2;
	GtkWidget *f;
	GtkWidget *t;
	GtkWidget *main_menu, *global_main;

	dialog = gnome_dialog_new (_("Menu properties"),
				   GNOME_STOCK_BUTTON_CLOSE,
				   GNOME_STOCK_BUTTON_HELP,
				   NULL);
	gnome_dialog_set_close (GNOME_DIALOG (dialog),
				FALSE /* click_closes */);

	menu->prop_dialog = dialog;

	menu->dialog_info = g_new0 (MenuDialogInfo, 1);

	notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    notebook, TRUE, TRUE, 0);

	gtk_window_set_wmclass(GTK_WINDOW(dialog),
			       "menu_properties", "Panel");
	gtk_window_set_title(GTK_WINDOW(dialog), _("Menu properties"));
	/*gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	vbox = gtk_vbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(vbox),GNOME_PAD_SMALL);

	f = gtk_frame_new(_("Menu type"));
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(box),GNOME_PAD_SMALL);
	gtk_container_add(GTK_CONTAINER(f),box);
	
	w = gtk_radio_button_new_with_label (NULL, _("Global main menu"));
	global_main = w;
	menu->dialog_info->global_main = w;
	if((menu->path == NULL ||
	    strcmp(menu->path, ".") == 0) &&
	   menu->global_main)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_global_main), 
			    menu);
	gtk_box_pack_start(GTK_BOX(box), w, TRUE, TRUE, 0);

	w = gtk_radio_button_new_with_label (
		  gtk_radio_button_group (GTK_RADIO_BUTTON (global_main)),
		  _("Main menu"));
	main_menu = w;
	menu->dialog_info->main_menu = w;
	if((menu->path == NULL ||
	    strcmp(menu->path, ".") == 0) &&
	   ! menu->global_main)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_main_menu), 
			    menu);
	gtk_box_pack_start(GTK_BOX(box), w, TRUE, TRUE, 0);

	w2 = gtk_radio_button_new_with_label (
		  gtk_radio_button_group (GTK_RADIO_BUTTON (global_main)),
		  _("Normal menu"));
	if(menu->path && strcmp(menu->path, ".") != 0)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w2), TRUE);
	gtk_signal_connect (GTK_OBJECT (w2), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_normal_menu), 
			    menu);
	gtk_box_pack_start(GTK_BOX(box), w2, TRUE, TRUE, 0);

	f = gtk_frame_new(_("Main menu"));
	if((menu->path &&
	    strcmp(menu->path, ".") != 0) ||
	   menu->global_main)
		gtk_widget_set_sensitive(f, FALSE);
	menu->dialog_info->main_frame = f;
	gtk_box_pack_start(GTK_BOX(vbox), f, FALSE, FALSE, 0);
	
	table = gtk_table_new(7, 4, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), GNOME_PAD_SMALL);
	gtk_container_add(GTK_CONTAINER(f), table);

	add_menu_type_options(menu,
			      GTK_OBJECT(dialog), GTK_TABLE(table),0,
			      _("Programs: "),
			      &menu->dialog_info->system,
			      &menu->dialog_info->system_sub,
			      menu->main_menu_flags & MAIN_MENU_SYSTEM,
			      menu->main_menu_flags & MAIN_MENU_SYSTEM_SUB);
	add_menu_type_options(menu,
			      GTK_OBJECT(dialog), GTK_TABLE(table),1,
			      _("Favorites: "),
			      &menu->dialog_info->user,
			      &menu->dialog_info->user_sub,
			      menu->main_menu_flags & MAIN_MENU_USER,
			      menu->main_menu_flags & MAIN_MENU_USER_SUB);
	add_menu_type_options(menu,
			      GTK_OBJECT(dialog), GTK_TABLE(table),2,
			      _("Applets: "),
			      &menu->dialog_info->applets,
			      &menu->dialog_info->applets_sub,
			      menu->main_menu_flags & MAIN_MENU_APPLETS,
			      menu->main_menu_flags & MAIN_MENU_APPLETS_SUB);
	add_menu_type_options(menu,
			      GTK_OBJECT(dialog), GTK_TABLE(table),3,
			      _("Distribution menu (if found): "),
			      &menu->dialog_info->distribution,
			      &menu->dialog_info->distribution_sub,
			      menu->main_menu_flags & MAIN_MENU_DISTRIBUTION,
			      menu->main_menu_flags & MAIN_MENU_DISTRIBUTION_SUB);
 	add_menu_type_options(menu,
			      GTK_OBJECT(dialog), GTK_TABLE(table),4,
 			      _("KDE menu (if found): "),
			      &menu->dialog_info->kde,
			      &menu->dialog_info->kde_sub,
 			      menu->main_menu_flags & MAIN_MENU_KDE,
 			      menu->main_menu_flags & MAIN_MENU_KDE_SUB);
	add_menu_type_options(menu,
			      GTK_OBJECT(dialog), GTK_TABLE(table),6,
			      _("Panel menu: "),
			      &menu->dialog_info->panel,
			      &menu->dialog_info->panel_sub,
			      menu->main_menu_flags & MAIN_MENU_PANEL,
			      menu->main_menu_flags & MAIN_MENU_PANEL_SUB);
	add_menu_type_options(menu,
			      GTK_OBJECT(dialog), GTK_TABLE(table),7,
			      _("Desktop menu: "),
			      &menu->dialog_info->desktop,
			      &menu->dialog_info->desktop_sub,
			      menu->main_menu_flags & MAIN_MENU_DESKTOP,
			      menu->main_menu_flags & MAIN_MENU_DESKTOP_SUB);

	f = gtk_frame_new(_("Normal menu"));
	if(menu->path == NULL ||
	   strcmp(menu->path, ".") == 0)
		gtk_widget_set_sensitive(f, FALSE);
	menu->dialog_info->normal_frame = f;
	gtk_box_pack_start(GTK_BOX(vbox), f, FALSE, FALSE, 0);
	
	box = gtk_hbox_new(FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(box), GNOME_PAD_SMALL);
	gtk_container_add(GTK_CONTAINER(f), box);
	
	w = gtk_label_new(_("Menu path"));
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);

	w = gnome_file_entry_new("menu_path",_("Browse"));
	gnome_file_entry_set_directory(GNOME_FILE_ENTRY(w), TRUE);

	t = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w));
	menu->dialog_info->pathentry = w;
	if (menu->path) {
		char *s = get_real_menu_path(menu->path);
		gtk_entry_set_text(GTK_ENTRY(t), s);
		g_free(s);
	}
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);
	gtk_signal_connect (GTK_OBJECT (t), "changed",
			    GTK_SIGNAL_FUNC (textbox_changed),
			    menu);
	
	gtk_notebook_append_page (GTK_NOTEBOOK(notebook),
				  vbox, gtk_label_new (_("Menu")));

	vbox = gtk_vbox_new(FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), GNOME_PAD_SMALL);

	f = gtk_frame_new(_("Icon"));
	gtk_box_pack_start(GTK_BOX(vbox), f, FALSE, FALSE, 0);
	
	box = gtk_vbox_new(FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(box), GNOME_PAD_SMALL);
	gtk_container_add(GTK_CONTAINER(f), box);

	w = gtk_check_button_new_with_label (_("Use custom icon for panel button"));
	menu->dialog_info->custom_icon = w;
	if(menu->custom_icon &&
	   menu->custom_icon_file != NULL)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_custom_icon), 
			    menu);
	gtk_box_pack_start(GTK_BOX(box), w, TRUE, TRUE, 0);

	w = gnome_icon_entry_new("icon", _("Browse"));
	hack_icon_entry (GNOME_ICON_ENTRY (w));
	menu->dialog_info->custom_icon_entry = w;
	if (menu->custom_icon_file != NULL) {
		hack_icon_entry_set_icon(GNOME_ICON_ENTRY(w),
					 menu->custom_icon_file);
	}
	if ( ! menu->custom_icon) {
		gtk_widget_set_sensitive (w, FALSE);
	}
	gtk_box_pack_start(GTK_BOX(box), w, TRUE, TRUE, 0);
	t = gnome_icon_entry_gtk_entry (GNOME_ICON_ENTRY (w));
	gtk_signal_connect (GTK_OBJECT (t), "changed",
			    GTK_SIGNAL_FUNC (textbox_changed),
			    menu);

	gtk_widget_grab_focus (global_main);

	gtk_notebook_append_page (GTK_NOTEBOOK(notebook),
				  vbox, gtk_label_new (_("Icon")));
	
	gtk_signal_connect (GTK_OBJECT(dialog), "destroy",
			    GTK_SIGNAL_FUNC (properties_close_callback),
			    menu);

	gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
			    GTK_SIGNAL_FUNC (dialog_clicked),
			    menu);

	return dialog;
}

void
menu_properties (Menu *menu)
{
	GtkWidget *dialog;

	g_return_if_fail (menu != NULL);

	if (menu->prop_dialog != NULL) {
		gtk_widget_show_now (menu->prop_dialog);
		gdk_window_raise (menu->prop_dialog->window);
		return;
	}

	dialog = create_properties_dialog (menu);
	gtk_widget_show_all (dialog);
}
