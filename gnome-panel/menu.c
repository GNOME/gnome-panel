/*
 * GNOME panel menu module.
 * (C) 1997, 1998, 1999, 2000 The Free Software Foundation
 * Copyright 2000 Helix Code, Inc.
 * Copyright 2000 Eazel, Inc.
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
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <math.h>

#include <gdk/gdkkeysyms.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <gnome-desktop-item.h>
#include <gnome-ditem-edit.h>
#include <gconf/gconf-client.h>
#include <libbonobo.h>

#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "aligned-widget.h"
#include "button-widget.h"
#include "distribution.h"
#include "drawer-widget.h"
#include "edge-widget.h"
#include "floating-widget.h"
#include "foobar-widget.h"
#include "gnome-run.h"
#include "launcher.h"
#include "logout.h"
#include "nothing.h"
#include "menu-fentry.h"
#include "menu-properties.h"
#include "menu-util.h"
#include "multiscreen-stuff.h"
#include "panel-util.h"
#include "panel-gconf.h"
#include "panel-main.h"
#include "panel.h"
#include "panel-config-global.h"
#include "session.h"
#include "sliding-widget.h"
#include "status.h"
#include "swallow.h"
#include "tearoffitem.h"
#include "panel-applet-frame.h"
#include "quick-desktop-reader.h"
#include "xstuff.h"

#undef MENU_DEBUG

#define ICON_SIZE 20

static char *gnome_folder = NULL;

extern GSList *applets;

/*list of all toplevel panel widgets (basep) created*/
extern GSList *panel_list;
/*list of all PanelWidgets created*/
extern GSList *panels;

extern gboolean commie_mode;
extern gboolean no_run_box;
extern GlobalConfig global_config;

extern int panels_to_sync;
extern int need_complete_save;

extern int base_panels;

extern char *kde_menudir;
extern char *kde_icondir;


extern GtkTooltips *panel_tooltips;

enum {
	REVERT_BUTTON
};

typedef struct _TearoffMenu TearoffMenu;
struct _TearoffMenu {
	GtkWidget *menu;
	GSList *mfl;
	char *special;
	char *title;
	char *wmclass;
};

/* list of TearoffMenu s */
static GSList *tearoffs = NULL;

typedef struct _ShowItemMenu ShowItemMenu;
struct _ShowItemMenu {
	int type;
	const char *item_loc;
	MenuFinfo *mf;
	GtkWidget *menu;
	GtkWidget *menuitem;
	int applet;
};

typedef struct {
	GtkWidget *pixmap;
	char *image;
	char *fallback_image;
	gboolean force_image;
} IconToLoad;

static guint load_icons_id = 0;
static GHashTable *loaded_icons = NULL;
static GList *icons_to_load = NULL;

static GtkWidget * create_menu_at_fr (GtkWidget *menu,
				      FileRec *fr,
				      gboolean applets,
				      gboolean launcher_add,
				      const char *dir_name,
				      const char *pixmap_name,
				      gboolean fake_submenus,
				      gboolean force);
static GtkWidget * create_panel_submenu (GtkWidget *m,
					 gboolean fake_sub,
					 gboolean tearoff,
					 gboolean is_base);
static GtkWidget * create_desktop_menu (GtkWidget *m,
					gboolean fake_sub,
					gboolean tearoff);

static void add_kde_submenu (GtkWidget *root_menu,
			     gboolean fake_submenus,
			     gboolean launcher_add);
static void add_distribution_submenu (GtkWidget *root_menu,
				      gboolean fake_submenus,
				      gboolean launcher_add);

static GtkWidget * create_add_launcher_menu (GtkWidget *menu,
					     gboolean fake_submenus);

static void setup_menuitem_try_pixmap (GtkWidget *menuitem,
				       const char *try_file,
				       const char *title);

static gboolean panel_menus_have_icons   = TRUE;
static gboolean panel_menus_have_tearoff = TRUE;

static void
panel_menu_have_icons_notify (GConfClient *client,
			      guint        cnxn_id,
			      GConfEntry  *entry,
			      gpointer     user_data)
{
	GConfValue *value = gconf_entry_get_value (entry);
	if (value->type == GCONF_VALUE_BOOL)
		panel_menus_have_icons = gconf_value_get_bool (value);
	else
		/* default to true */
		panel_menus_have_icons = TRUE;
}

gboolean
panel_menu_have_icons (void)
{
	static guint notify_id = 0;

	if (notify_id == 0) {
		GConfValue  *value;
		GConfClient *client = panel_gconf_get_client ();
		gchar       *key    = PANEL_MENU_HAVE_ICONS_KEY;
		GError      *error  = NULL;

		notify_id = gconf_client_notify_add (client, key, 
						     panel_menu_have_icons_notify,
						     NULL, NULL, &error);
		if (error) {
			g_warning (G_STRLOC ": failed to add notification for '%s' : '%s'",
				   key, error->message);
			g_error_free (error);
		}

		value = gconf_client_get (client, key, NULL);
		if (value != NULL &&
		    value->type == GCONF_VALUE_BOOL)
			panel_menus_have_icons = gconf_value_get_bool (value);
		else
			/* default to true */
			panel_menus_have_icons = TRUE;

		if (value != NULL)
			gconf_value_free (value);
	}

	return panel_menus_have_icons;
}

static void
panel_menu_have_tearoff_notify (GConfClient *client,
				guint        cnxn_id,
				GConfEntry  *entry,
				gpointer     user_data)
{
	GConfValue *value = gconf_entry_get_value (entry);
	if (value->type == GCONF_VALUE_BOOL)
		panel_menus_have_tearoff = gconf_value_get_bool (value);
	else
		/* default to true */
		panel_menus_have_tearoff = TRUE;
}

gboolean
panel_menu_have_tearoff (void)
{
	static guint notify_id = 0;

	if (notify_id == 0) {
		GConfValue  *value;
		GConfClient *client = panel_gconf_get_client ();
		gchar       *key    = PANEL_MENU_HAVE_TEAROFF_KEY;
		GError      *error  = NULL;

		notify_id = gconf_client_notify_add (client, key, 
						     panel_menu_have_tearoff_notify,
						     NULL, NULL, &error);
		if (error) {
			g_warning (G_STRLOC ": failed to add notification for '%s' : '%s'",
				   key, error->message);
			g_error_free (error);
		}
		
		panel_menus_have_tearoff = gconf_client_get_bool (client, key, NULL);

		value = gconf_client_get (client, key, NULL);
		if (value != NULL &&
		    value->type == GCONF_VALUE_BOOL)
			panel_menus_have_tearoff = gconf_value_get_bool (value);
		else
			/* default to true */
			panel_menus_have_tearoff = TRUE;

		if (value != NULL)
			gconf_value_free (value);
	}

	return panel_menus_have_tearoff;
}
/*to be called on startup to load in some of the directories,
  this makes the startup a little bit slower, and take up slightly
  more ram, but it also speeds up later operation*/
void
init_menus (void)
{
	const DistributionInfo *distribution_info = get_distribution_info ();
	char *menu;

	/*just load the menus from disk, don't make the widgets
	  this just reads the .desktops of the top most directory
	  and a level down*/
	fr_read_dir (NULL, "applications:/", 0, 2);

	menu = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_DATADIR, 
					  "applets", TRUE, NULL);
	if (menu != NULL) {
		char *uri = gnome_vfs_get_uri_from_local_path (menu);
		fr_read_dir (NULL, uri, 0, 2);
		g_free (uri);
	}
	g_free (menu);

	if (distribution_info != NULL &&
	    distribution_info->menu_init_func != NULL)
		distribution_info->menu_init_func ();
}

static gboolean
check_for_screen (GtkWidget *w, GdkEvent *ev, gpointer data)
{
	static int times = 0;
	if (ev->type != GDK_KEY_PRESS)
		return FALSE;
	if (ev->key.keyval == GDK_f ||
	    ev->key.keyval == GDK_F) {
		times++;
		if (times == 3) {
			times = 0;
			start_screen_check ();
		}
	}
	return FALSE;
}

/*the most important dialog in the whole application*/
static void
about_cb (GtkWidget *widget, gpointer data)
{
	static GtkWidget *about;
	GtkWidget *hbox, *l;
	/* FIXME: fill in all the wankers who did stuff */
	char *authors[] = {
	  "George Lebl (jirka@5z.com)",
	  "Jacob Berkman (jberkman@andrew.cmu.edu)",
	  "Miguel de Icaza (miguel@kernel.org)",
	  "Federico Mena (quartic@gimp.org)",
	  "Tom Tromey (tromey@cygnus.com)",
	  "Ian Main (imain@gtk.org)",
	  "Elliot Lee (sopwith@redhat.com)",
	  "Owen Taylor (otaylor@redhat.com)",
	N_("Many many others ..."),
	/* ... from the Monty Pythons show...  */
	N_("and finally, The Knights Who Say ... NI!"),
	  NULL
	  };
	char *documenters[] = {
		/* FIXME: */
		"Joe Documentor",
		NULL
	  };

	if (about) {
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

#ifdef ENABLE_NLS
	{
		int i=0;
		while (authors[i] != NULL) {
		       	authors[i]=_(authors[i]);
			i++;
		}
	}
#endif
	
	about = gnome_about_new ( _("The GNOME Panel"), VERSION,
			_("(C) 1997-2000 the Free Software Foundation"),
			_("This program is responsible for launching "
			"other applications, embedding small applets "
			"within itself, world peace, and random X crashes."),
			(const char **)authors,
			(const char **)documenters,
			"foo" /* FIXME: ??? translator_credits */,
			NULL /* FIXME: logo "gnome-gegl2.png" */);
	gtk_window_set_wmclass (GTK_WINDOW (about), "about_dialog", "Panel");
	g_signal_connect (G_OBJECT (about), "destroy",
			    G_CALLBACK (gtk_widget_destroyed),
			    &about);
	g_signal_connect (G_OBJECT (about), "event",
			  G_CALLBACK (check_for_screen), NULL);

	hbox = gtk_hbox_new (TRUE, 0);
	l = gnome_href_new ("http://www.wfp.org/",
			    _("End world hunger"));
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (about)->vbox),
			    hbox, TRUE, FALSE, 0);
	gtk_widget_show_all (hbox);

	if (commie_mode) {
		l = gtk_label_new (_("Running in \"Lockdown\" mode.  This "
				     "means your system administrator has "
				     "prohibited any changes to the panel's "
				     "configuration to take place."));
		gtk_widget_show (l);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (about)->vbox),
				    l, FALSE, FALSE, 0);
	}

	gtk_widget_show (about);
}

static void
about_gnome_cb(GtkObject *object, char *program_path)
{
	if (gnome_execute_async (g_get_home_dir (), 1, &program_path)<0)
		panel_error_dialog ("cannot_exec_about_gnome",
				    _("Can't execute 'About GNOME'"));
}

static void
activate_app_def (GtkWidget *widget, const char *item_loc)
{
	GnomeDesktopItem *item = gnome_desktop_item_new_from_uri
		(item_loc,
		 GNOME_DESKTOP_ITEM_LOAD_NO_TRANSLATIONS,
		 NULL /* error */);
	if (item != NULL) {
		char *curdir = g_get_current_dir ();
		chdir (g_get_home_dir ());

		gnome_desktop_item_launch (item,
					   NULL /* file_list */,
					   0 /* flags */,
					   NULL /* error */);
		gnome_desktop_item_unref (item);

		chdir (curdir);
		g_free (curdir);
	} else {
		panel_error_dialog ("cannot_load_entry",
				    _("Can't load entry"));
	}
}

static PanelWidget *
get_panel_from_menu_data(GtkWidget *menu, gboolean must_have)
{
	g_return_val_if_fail (menu != NULL, NULL);
	g_return_val_if_fail (GTK_IS_MENU(menu) || GTK_IS_MENU_ITEM(menu),
			      NULL);

	if(GTK_IS_MENU_ITEM(menu))
		menu = menu->parent;

	while(menu) {
		PanelWidget *panel = g_object_get_data (G_OBJECT (menu),
							"menu_panel");

		if (panel != NULL) {
			if(PANEL_IS_WIDGET(panel))
				return panel;
			else
				g_warning("Menu is on crack");
		}
		menu = gtk_menu_get_attach_widget (GTK_MENU (menu))->parent;
	}
	if (must_have) {
		g_warning("Something went quite terribly wrong and we can't "
			  "find where this menu belongs");
		return panels->data;
	} else {
		return NULL;
	}
}

static void
setup_menu_panel (GtkWidget *menu)
{
	PanelWidget *menu_panel = g_object_get_data (G_OBJECT (menu),
						     "menu_panel");
	if (menu_panel != NULL) {
		menu_panel = get_panel_from_menu_data (menu, TRUE);
		g_object_set_data (G_OBJECT (menu), "menu_panel", menu_panel);
	}
}

static void
pixmaps_changed (GConfClient* client,
		 guint cnxn_id,
		 GConfEntry *entry,
		 gpointer user_data)
{
	GtkWidget *menu = user_data;
	GConfValue *value = gconf_entry_get_value (entry);
	GList *list, *li;
	gboolean pixmaps;

	if (value->type == GCONF_VALUE_BOOL)
		pixmaps = gconf_value_get_bool (value);
	else
		/* default to true */
		pixmaps = TRUE;

	list = g_list_copy (GTK_MENU_SHELL (menu)->children);
	for (li = list;
	     li != NULL;
	     li = li->next) {
		GtkWidget *item = li->data;
		GtkWidget *cur_image;
		GtkWidget *image;

		if ( ! GTK_IS_IMAGE_MENU_ITEM (item))
			continue;

		image = g_object_get_data (G_OBJECT (item), "Panel:Image");

		if (image == NULL)
			continue;

		/* A forced image is always on */
		if (g_object_get_data (G_OBJECT (item),
				       "Panel:ForceImage"))
			continue;

		cur_image = gtk_image_menu_item_get_image
			(GTK_IMAGE_MENU_ITEM (item));

		if (pixmaps) {
			if (cur_image != image) {
				gtk_image_menu_item_set_image
					(GTK_IMAGE_MENU_ITEM (item), image);
				gtk_widget_show (image);
			}
		} else {
			gtk_image_menu_item_set_image
				(GTK_IMAGE_MENU_ITEM (item), NULL);
		}
	}
	g_list_free (list);
}

static void
tearoff_changed (GConfClient* client,
		 guint cnxn_id,
		 GConfEntry *entry,
		 gpointer user_data)
{
	GtkWidget *menu = user_data;
	GConfValue *value = gconf_entry_get_value (entry);
	gboolean tearoff;

	if (value->type == GCONF_VALUE_BOOL)
		tearoff = gconf_value_get_bool (value);
	else
		/* default to true */
		tearoff = TRUE;

	if (GTK_MENU_SHELL (menu)->children != NULL) {
		GtkWidget *item = GTK_MENU_SHELL (menu)->children->data;

		if (TEAROFF_IS_ITEM (item) ||
		    GTK_IS_TEAROFF_MENU_ITEM (item)) {
			if (tearoff)
				gtk_widget_show (item);
			else
				gtk_widget_hide (item);
		}
	}
}

GtkWidget *
panel_menu_new (void)
{
	GtkWidget *menu;
	menu = gtk_menu_new ();
	panel_gconf_notify_add_while_alive (PANEL_MENU_HAVE_ICONS_KEY,
					    pixmaps_changed,
					    G_OBJECT (menu));
	panel_gconf_notify_add_while_alive (PANEL_MENU_HAVE_TEAROFF_KEY,
					    tearoff_changed,
					    G_OBJECT (menu));
	return menu;
}

static GtkWidget *
menu_new (void)
{
	GtkWidget *ret;
	ret = panel_menu_new ();
	g_signal_connect (G_OBJECT (ret), "show",
			  G_CALLBACK (setup_menu_panel), NULL);

	return ret;
}

static void
icon_to_load_free (IconToLoad *icon)
{
	if (icon != NULL) {
	        g_object_unref (G_OBJECT (icon->pixmap));
	        icon->pixmap = NULL;
	        g_free (icon->image);
	        icon->image = NULL;
		g_free (icon->fallback_image);
		icon->fallback_image = NULL;
		g_free (icon);
	}
}

static IconToLoad *
icon_to_load_copy (IconToLoad *icon)
{
	IconToLoad *new_icon = NULL;
	if (icon != NULL) {
		new_icon = g_new0 (IconToLoad, 1);
	        new_icon->pixmap = g_object_ref (G_OBJECT (icon->pixmap));
	        new_icon->image = g_strdup (icon->image);
		new_icon->fallback_image = g_strdup (icon->fallback_image);
		new_icon->force_image = icon->force_image;
	}
	return new_icon;
}

static void
remove_pixmap_from_loaded (GtkWidget *pixmap, gpointer data)
{
	if (loaded_icons != NULL) 
		g_hash_table_remove (loaded_icons, data);
}

static gboolean
load_icons_handler (gpointer data)
{
	GtkImage *pixmap;
	GdkPixbuf *pb;
	IconToLoad *icon;
	char *file;
	gboolean loaded;

load_icons_handler_again:

	if (icons_to_load == NULL) {
		load_icons_id = 0;
		return FALSE;
	}

	icon = icons_to_load->data;
	icons_to_load = g_list_remove (icons_to_load, icon);

	/* if not visible anymore, just ignore */
	if ( ! GTK_WIDGET_VISIBLE (icon->pixmap)) {
		icon_to_load_free (icon);
		/* we didn't do anything long/hard, so just do this again,
		 * this is fun, don't go back to main loop */
		goto load_icons_handler_again;
	}

	file = gnome_desktop_item_find_icon (icon->image,
					     20 /* desired size */,
					     0 /* flags */);
	if (file == NULL)
		file = gnome_desktop_item_find_icon (icon->fallback_image,
						     20 /* desired size */,
						     0 /* flags */);

	if (file == NULL) {
		icon_to_load_free (icon);
		/* we didn't do anything long/hard, so just do this again,
		 * this is fun, don't go back to main loop */
		goto load_icons_handler_again;
	}

	pb = NULL;

	loaded = FALSE;
	pixmap = NULL;

	if (loaded_icons != NULL &&
	    (pixmap = g_hash_table_lookup (loaded_icons, file)) != NULL) {
		pb = gtk_image_get_pixbuf (GTK_IMAGE (pixmap));
		if (pb != NULL)
			g_object_ref (G_OBJECT (pb));
	}

	if (pb == NULL) {
		pb = gdk_pixbuf_new_from_file (file, NULL);
		loaded = TRUE;
	}
	if (pb == NULL) {
		g_free (file);
		icon_to_load_free (icon);
		/* this may have been a long operation so jump back to
		 * the main loop for a while */
		return TRUE;
	}

	if (gdk_pixbuf_get_width (pb) != ICON_SIZE ||
	    gdk_pixbuf_get_height (pb) != ICON_SIZE) {
		GdkPixbuf *pb2;
		pb2 = gdk_pixbuf_scale_simple (pb, ICON_SIZE, ICON_SIZE,
					       GDK_INTERP_BILINEAR);
		g_object_unref (G_OBJECT (pb));
		pb = pb2;
	}

	gtk_image_set_from_pixbuf (GTK_IMAGE (icon->pixmap), pb);
	g_object_unref (G_OBJECT (pb));

	if (loaded) {
		if (loaded_icons == NULL)
			loaded_icons = g_hash_table_new_full
				(g_str_hash, g_str_equal,
				 (GDestroyNotify) g_free,
				 (GDestroyNotify) g_object_unref);
		g_hash_table_replace (loaded_icons,
				      g_strdup (file),
				      g_object_ref (G_OBJECT (icon->pixmap)));
		g_signal_connect_data (G_OBJECT (icon->pixmap), "destroy",
				       G_CALLBACK (remove_pixmap_from_loaded),
				       g_strdup (file),
				       (GClosureNotify) g_free,
				       0 /* connect_flags */);
	} else {
		g_free (file);
		/* we didn't load from disk, so just do this again,
		 * this is fun, don't go back to main loop */
		goto load_icons_handler_again;
	}

	g_free (file);

	/* if still more we'll come back */
	return TRUE;
}

/* replaces '/' with returns _'s, originally from gmenu */
static void
validate_for_filename (char *file)
{
	char *ptr;

	g_return_if_fail (file != NULL);
	
	ptr = file;
	while (*ptr != '\0') {
		if (*ptr == '/')
			*ptr = '_';
		ptr++;
	}
}

static void
really_add_new_menu_item (GtkWidget *d, int response, gpointer data)
{
	GnomeDItemEdit *dedit = GNOME_DITEM_EDIT(data);
	char *dir = g_object_get_data (G_OBJECT (d), "dir");
	GnomeDesktopItem *ditem;
	GError *error = NULL;
	int i;
	char *name, *loc;

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (d);
		return;
	}

	g_return_if_fail (dir != NULL);

	panel_push_window_busy (d);

	ditem = gnome_ditem_edit_get_ditem (dedit);

	if ((gnome_desktop_item_get_entry_type (ditem) == GNOME_DESKTOP_ITEM_TYPE_APPLICATION &&
	     string_empty (gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_EXEC))) ||
	    (gnome_desktop_item_get_entry_type (ditem) == GNOME_DESKTOP_ITEM_TYPE_LINK &&
	     string_empty (gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_URL)))) {
		gnome_desktop_item_unref (ditem);
		panel_error_dialog ("cannot_create_launcher",
				    _("Cannot create the launcher.\n\n"
				      "No command or url specified."));
		return;
	}

	/* assume we are making a new file */
	name = g_strdup (gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_NAME));

	validate_for_filename (name);

	loc = g_strdup_printf ("%s/%s.desktop", dir, name);

	i = 2;
	while (g_file_test (loc, G_FILE_TEST_EXISTS)) {
		g_free (loc);
		loc = g_strdup_printf ("%s/%s%d.desktop",
				       dir, name,
				       i ++);
	}
	gnome_desktop_item_set_location_file (ditem, loc);
	g_free (name);

	error = NULL;
	gnome_desktop_item_save (ditem,
				 NULL /* under */,
				 TRUE /* force */,
				 &error);
	if (error != NULL) {
		panel_error_dialog ("cannot_save_menu_item" /* class */,
				    _("Cannot save menu item to disk, "
				      "the following error occured:\n\n"
				      "%s"),
				    error->message);
		g_clear_error (&error);
	}

	gnome_desktop_item_unref (ditem);

	panel_pop_window_busy (d);

	gtk_widget_destroy (d);
	g_free (loc);
}

static void
add_new_app_to_menu (GtkWidget *widget, const char *item_loc)
{
	GtkWidget *dialog;
	GtkWidget *dee;

	dialog = gtk_dialog_new_with_buttons (_("Create menu item"),
					      NULL /* parent */,
					      0 /* flags */,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_OK,
					      NULL);

	gtk_window_set_wmclass (GTK_WINDOW (dialog),
			       "create_menu_item", "Panel");
	
	dee = gnome_ditem_edit_new ();
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), dee,
			    TRUE, TRUE, GNOME_PAD_SMALL);

	gnome_ditem_edit_set_entry_type (GNOME_DITEM_EDIT (dee), 
					 "Application");

	g_object_set_data_full (G_OBJECT (dialog), "dir",
				g_strdup (item_loc),
				(GDestroyNotify)g_free);
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (really_add_new_menu_item),
			  dee);

	gtk_dialog_set_default_response (GTK_DIALOG(dialog),
					 GTK_RESPONSE_OK);

	gtk_widget_show_all (dialog);

	gnome_ditem_edit_grab_focus (GNOME_DITEM_EDIT (dee));
}

static void
remove_menuitem (GtkWidget *widget, ShowItemMenu *sim)
{
	char *file;
	char *dir, *directory_file;
	GnomeDesktopItem *ditem;

	g_return_if_fail (sim->item_loc != NULL);
	g_return_if_fail (sim->menuitem != NULL);

	gtk_widget_hide (sim->menuitem);

	if (unlink (sim->item_loc) < 0) {
		panel_error_dialog("cant_remove_menu_item",
				   _("Could not remove the menu item %s: %s\n"), 
				    sim->item_loc, g_strerror (errno));
		return;
	}

	file = g_path_get_basename (sim->item_loc);
	if (file == NULL) {
		g_warning (_("Could not get file name from path: %s"),
			  sim->item_loc);
		return;
	}

	dir = g_path_get_dirname (sim->item_loc);
	if (dir == NULL) {
		g_warning (_("Could not get directory name from path: %s"),
			  sim->item_loc);
		g_free (file);
		return;
	}

	directory_file = g_build_path ("/", dir, ".directory", NULL);
	ditem = gnome_desktop_item_new_from_uri (directory_file,
						 0 /* flags */,
						 NULL /* error */);
	g_free (directory_file);

	if (ditem != NULL) {
		char **sort_order = gnome_desktop_item_get_strings (ditem,
								    GNOME_DESKTOP_ITEM_SORT_ORDER);
		if (sort_order != NULL) {
			int i, j;
			j = 0;
			for (i = 0; sort_order[i] != NULL; i++) {
				if (strcmp (file, sort_order[i]) != 0) {
					sort_order[j++] = sort_order[i];
				} else {
					g_free (sort_order[i]);
					sort_order[i] = NULL;
				}
			}
			sort_order[j++] = NULL;
			gnome_desktop_item_set_strings (ditem,
							GNOME_DESKTOP_ITEM_SORT_ORDER,
							sort_order);
			g_strfreev (sort_order);

			gnome_desktop_item_save (ditem,
						 NULL /* under */,
						 TRUE /* force */,
						 NULL /* error */);

			/* ignore errors, it's not at all important if we failed,
			 * no point bothering the user with it. */

		}
		gnome_desktop_item_unref (ditem);
	}
}

static void
add_to_run_dialog (GtkWidget *widget, const char *item_loc)
{
	GnomeDesktopItem *item =
		gnome_desktop_item_new_from_uri (item_loc,
						 GNOME_DESKTOP_ITEM_LOAD_NO_TRANSLATIONS,
						 NULL /* error */);
	if (item != NULL) {
		const char *exec;

		exec = gnome_desktop_item_get_string
			(item, GNOME_DESKTOP_ITEM_EXEC);
		if (exec == NULL)
			exec = gnome_desktop_item_get_string
				(item, GNOME_DESKTOP_ITEM_URL);

		if (exec != NULL) {
			show_run_dialog_with_text (exec);
		} else {
			panel_error_dialog ("no_exec_or_url_field",
					    _("No 'Exec' or 'URL' field in entry"));
		}
		gnome_desktop_item_unref (item);
	} else {
		panel_error_dialog ("cant_load_entry",
				    _("Can't load entry"));
	}
}

static void
show_help_on (GtkWidget *widget, const char *item_loc)
{
	GnomeDesktopItem *item =
		gnome_desktop_item_new_from_uri (item_loc,
						 GNOME_DESKTOP_ITEM_LOAD_NO_TRANSLATIONS,
						 NULL /* error */);
	if (item != NULL) {
		const char *docpath = gnome_desktop_item_get_string
			(item, "DocPath");
		char *path = panel_gnome_kde_help_path (docpath);
		if (path != NULL) {
			GError *error = NULL;
			gnome_url_show (path, &error);
			if (error != NULL) {
				const char *name = gnome_desktop_item_get_localestring
					(item, GNOME_DESKTOP_ITEM_NAME);
				panel_error_dialog ("cant_load_help_on",
						    _("Cannot load help on %s.\n\n%s"),
						    name, error->message);
				g_clear_error (&error);
			}

			/* FIXME: this should prolly use gnome_help */
			g_free (path);
		}
		gnome_desktop_item_unref (item);
	} else {
		panel_error_dialog ("cant_load_entry",
				    _("Can't load entry"));
	}
}

static void
add_app_to_panel (GtkWidget *widget, const char *item_loc)
{
	PanelWidget *panel = get_panel_from_menu_data (widget, TRUE);
	Launcher *launcher;

	launcher = load_launcher_applet (item_loc, panel, 0, FALSE, NULL);

	if (launcher != NULL)
		launcher_hoard (launcher);
}


static void
add_drawers_from_dir (const char *dirname, const char *name,
		      int pos, PanelWidget *panel)
{
	Drawer *drawer;
	PanelWidget *newpanel;
	QuickDesktopItem *item_info;
	char *dentry_name;
	const char *subdir_name;
	char *pixmap_name;
	char *filename = NULL;
	GSList *list, *li;

	dentry_name = g_build_path ("/",
				    dirname,
				    ".directory",
				    NULL);
	item_info = quick_desktop_item_load_uri (dentry_name,
						 NULL /* expected_type */,
						 FALSE /* run_tryexec */);
	g_free (dentry_name);

	if (name == NULL)
		subdir_name = item_info != NULL ? item_info->name : NULL;
	else
		subdir_name = name;
	pixmap_name = item_info != NULL ? item_info->icon : NULL;

	drawer = load_drawer_applet (NULL, pixmap_name, subdir_name, panel, pos, FALSE, NULL);
	if (!drawer) {
		g_warning("Can't load a drawer");
		return;
	}

	newpanel = PANEL_WIDGET(BASEP_WIDGET(drawer->drawer)->panel);

	list = get_mfiles_from_menudir (dirname);
	for(li = list; li!= NULL; li = li->next) {
		MFile *mfile = li->data;
		GnomeDesktopItem *dentry;

		g_free (filename);
		filename = g_build_filename (dirname, mfile->name, NULL);

		if ( ! mfile->verified) {
			continue;
		}

		if (mfile->is_dir) {
			add_drawers_from_dir (filename, NULL, G_MAXINT/2,
					      newpanel);
			continue;
		}
			
		if (is_ext2 (mfile->name, ".desktop", ".kdelnk")) {
			/*we load the applet at the right
			  side, that is end of the drawer*/
			dentry = gnome_desktop_item_new_from_uri (filename,
								  GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS, NULL);
			if (dentry) {
				Launcher *launcher;

				launcher =
					load_launcher_applet_full (filename,
								   dentry,
								   newpanel,
								   G_MAXINT/2,
								   FALSE,
								   NULL);

				if (launcher != NULL)
					launcher_hoard (launcher);
			}
		}
	}
	g_free (filename);

	free_mfile_list (list);
}

/*add a drawer with the contents of a menu to the panel*/
static void
add_menudrawer_to_panel(GtkWidget *w, gpointer data)
{
	MenuFinfo *mf = data;
	PanelWidget *panel = get_panel_from_menu_data (w, TRUE);
	g_return_if_fail(mf);
	
	add_drawers_from_dir (mf->menudir, mf->dir_name, 0, panel);
}

static void
add_menu_to_panel (GtkWidget *widget, gpointer data)
{
	const char *menudir = data;
	gboolean main_menu;
	PanelWidget *panel;
	DistributionType distribution = get_distribution_type ();
	int flags = MAIN_MENU_SYSTEM_SUB | MAIN_MENU_APPLETS_SUB |
		MAIN_MENU_PANEL_SUB | MAIN_MENU_DESKTOP_SUB;
	
	/*guess distribution menus*/
	if (distribution != DISTRIBUTION_UNKNOWN)
		flags |= MAIN_MENU_DISTRIBUTION_SUB;

	/*guess KDE menus*/
	if (g_file_test (kde_menudir, G_FILE_TEST_IS_DIR))
		flags |= MAIN_MENU_KDE_SUB;

	panel = get_panel_from_menu_data (widget, TRUE);

	if (menudir == NULL) {
		main_menu = TRUE;
		menudir = "applications:/";
	} else {
		main_menu = FALSE;
	}

	load_menu_applet (menudir, main_menu, flags,
			  TRUE /* global_main */,
			  FALSE /* custom_icon */,
			  NULL /* custom_icon_file */,
			  panel,
			  0 /* pos */,
			  FALSE /* exactpos */,
			  NULL);
}

/*most of this function stolen from the real gtk_menu_popup*/
static void
restore_grabs(GtkWidget *w, gpointer data)
{
	GtkWidget *menu_item = data;
	GtkMenu *menu = GTK_MENU(menu_item->parent); 
	GtkWidget *xgrab_shell;
	GtkWidget *parent;
	/* Find the last viewable ancestor, and make an X grab on it
	 */
	parent = GTK_WIDGET (menu);
	xgrab_shell = NULL;
	while (parent) {
		gboolean viewable = TRUE;
		GtkWidget *tmp = parent;

		while (tmp) {
			if (!GTK_WIDGET_MAPPED (tmp)) {
				viewable = FALSE;
				break;
			}
			tmp = tmp->parent;
		}

		if (viewable)
			xgrab_shell = parent;

		parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
	}

	/*only grab if this HAD a grab before*/
	if (xgrab_shell && (GTK_MENU_SHELL (xgrab_shell)->have_xgrab)) {
		GdkCursor *cursor = gdk_cursor_new (GDK_ARROW);

		GTK_MENU_SHELL (xgrab_shell)->have_xgrab = 
			(gdk_pointer_grab (xgrab_shell->window, TRUE,
					   GDK_BUTTON_PRESS_MASK |
					   GDK_BUTTON_RELEASE_MASK |
					   GDK_ENTER_NOTIFY_MASK |
					   GDK_LEAVE_NOTIFY_MASK,
					   NULL, cursor, 0) == 0);
		gdk_cursor_unref (cursor);
	}
	
	gtk_grab_add (GTK_WIDGET (menu));
}

static void
ditem_properties_clicked (GtkWidget *w, int response, gpointer data)
{
	GnomeDItemEdit *dee = g_object_get_data (G_OBJECT (w), "GnomeDItemEdit");
	GnomeDesktopItem *ditem = data;

	if (response == GTK_RESPONSE_HELP) {
		panel_show_help ("launchers", NULL);
	} else if (response == REVERT_BUTTON) {
		if (ditem != NULL)
			gnome_ditem_edit_set_ditem (dee, ditem);
		else
			gnome_ditem_edit_clear (dee);
	} else {
		gtk_widget_destroy (w);
	}
}

static gboolean
ditem_properties_apply_timeout (gpointer data)
{
	GtkWidget *dedit = data;
	GnomeDesktopItem *ditem;
	const char *loc;

	g_object_set_data (G_OBJECT (dedit), "apply_timeout", NULL);

	ditem = gnome_ditem_edit_get_ditem (GNOME_DITEM_EDIT (dedit));
	loc = g_object_get_data (G_OBJECT (dedit), "location");
	gnome_desktop_item_save (ditem,
				 loc /* under */,
				 TRUE /* force */,
				 NULL /* error */);
	/* FIXME: we don't want to really handle errors here though,
	 * only on OK, but make sure that we know by the time we
	 * hit OK that something went wrong here */
	gnome_desktop_item_unref (ditem);

	return FALSE;
}

/* 
 * Will save after 5 seconds of no changes.  If something is changed, the save
 * is postponed to another 5 seconds.  This seems to be a saner behaviour,
 * then just saving every N seconds.
 */
static void
ditem_properties_changed (GtkWidget *dedit, gpointer data)
{
	gpointer timeout_data = g_object_get_data (G_OBJECT (dedit),
						   "apply_timeout");
	guint timeout = GPOINTER_TO_UINT (timeout_data);

	g_object_set_data (G_OBJECT (dedit), "apply_timeout", NULL);

	if (timeout != 0)
		gtk_timeout_remove (timeout);

	/* Will save after 5 seconds */
	timeout = gtk_timeout_add (5 * 1000,
				   ditem_properties_apply_timeout,
				   dedit);

	g_object_set_data (G_OBJECT (dedit), "apply_timeout",
			   GUINT_TO_POINTER (timeout));
}


static void
ditem_properties_close (GtkWidget *widget, gpointer data)
{
	GtkWidget *dedit = data;
	gpointer timeout_data = g_object_get_data (G_OBJECT (dedit),
						   "apply_timeout");
	guint timeout = GPOINTER_TO_UINT (timeout_data);

	g_object_set_data (G_OBJECT (dedit), "apply_timeout", NULL);

	if (timeout != 0) {
		gtk_timeout_remove (timeout);

		ditem_properties_apply_timeout (dedit);
	}
}

static gboolean
is_item_writable (const ShowItemMenu *sim)
{
	errno = 0;
	if (sim->item_loc != NULL &&
	    /*A HACK: but it works, don't have it edittable if it's redhat
	      menus as they are auto generated!*/
	    strstr (sim->item_loc,"/" GNOME_DOT_GNOME "/apps-redhat/") == NULL &&
	    /*if it's a kdelnk file, don't let it be editted*/
	    ! is_ext (sim->item_loc, ".kdelnk") &&
	    access (sim->item_loc, W_OK) == 0) {
#ifdef MENU_DEBUG
		puts (sim->item_loc);
#endif
		/*file exists and is writable, we're in bussines*/
		return TRUE;
	} else if ((sim->item_loc == NULL ||
		    errno == ENOENT) &&
		   sim->mf != NULL) {
		/*the dentry isn't there, check if we can write the
		  directory*/
		if (access (sim->mf->menudir, W_OK) == 0 &&
		   /*A HACK: but it works, don't have it edittable if it's redhat
		     menus as they are auto generated!*/
		   strstr (sim->mf->menudir, GNOME_DOT_GNOME "apps-redhat/") == NULL)
			return TRUE;
	}
	return FALSE;
}

static void
set_ditem_sensitive (GtkDialog *dialog,
		     GnomeDItemEdit *dedit,
		     ShowItemMenu *sim)
{
	gboolean sensitive;

	sensitive = is_item_writable (sim);

	gnome_ditem_edit_set_editable (dedit, sensitive);

	gtk_dialog_set_response_sensitive (dialog, REVERT_BUTTON, sensitive);
}

static void
edit_dentry (GtkWidget *widget, ShowItemMenu *sim)
{
	GtkWidget *dialog;
	GtkWidget *dedit;
	GnomeDesktopItem *ditem;
	
	g_return_if_fail (sim != NULL);
	g_return_if_fail (sim->item_loc != NULL);

	ditem = gnome_desktop_item_new_from_uri (sim->item_loc,
						 0 /* flags */,
						 NULL /* error */);

	/* watch the enum at the top of the file */
	dialog = gtk_dialog_new_with_buttons (_("Desktop entry properties"),
					      NULL /* parent */,
					      0 /* flags */,
					      GTK_STOCK_HELP,
					      GTK_RESPONSE_HELP,
					      GTK_STOCK_REVERT_TO_SAVED,
					      REVERT_BUTTON,
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_CLOSE,
					      NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	dedit = gnome_ditem_edit_new ();

	gtk_widget_show (dedit);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    dedit, TRUE, TRUE, 0);

	gtk_window_set_wmclass(GTK_WINDOW(dialog),
			       "desktop_entry_properties","Panel");
	
	g_object_set_data_full (G_OBJECT (dedit), "location",
				g_strdup (sim->item_loc),
				(GDestroyNotify)g_free);

	if (ditem != NULL)
		gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (dedit), ditem);

	set_ditem_sensitive (GTK_DIALOG (dialog),
			     GNOME_DITEM_EDIT (dedit), sim);

	g_signal_connect (G_OBJECT (dedit), "changed",
			    G_CALLBACK (ditem_properties_changed),
			    NULL);

	g_signal_connect (G_OBJECT (dialog), "destroy",
			    G_CALLBACK (ditem_properties_close),
			    dedit);

	g_object_set_data (G_OBJECT (dialog), "GnomeDItemEdit", dedit);

	if (ditem != NULL) {
		/* pass the ditem as the data to clicked */
		g_signal_connect_data (G_OBJECT (dialog), "response",
				       G_CALLBACK (ditem_properties_clicked),
				       ditem,
				       (GClosureNotify) gnome_desktop_item_unref,
				       0 /* connect_flags */);
	} else {
		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (ditem_properties_clicked),
				  NULL);
	}

	gtk_widget_show (dialog);

	gnome_ditem_edit_grab_focus (GNOME_DITEM_EDIT (dedit));
}

static void
edit_direntry (GtkWidget *widget, ShowItemMenu *sim)
{
	GtkWidget *dialog;
	GtkWidget *dedit;
	char *dirfile = g_build_path ("/", sim->mf->menudir, ".directory", NULL);
	GnomeDesktopItem *ditem;

	ditem = gnome_desktop_item_new_from_uri (dirfile,
						 0 /* flags */,
						 NULL /* error */);
	if (ditem == NULL) {
		return;
	}

	/* watch the enum at the top of the file */
	dialog = gtk_dialog_new_with_buttons (_("Desktop entry properties"),
					      NULL /* parent */,
					      0 /* flags */,
					      GTK_STOCK_HELP,
					      GTK_RESPONSE_HELP,
					      GTK_STOCK_REVERT_TO_SAVED,
					      REVERT_BUTTON,
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_CLOSE,
					      NULL);

	gtk_window_set_wmclass (GTK_WINDOW (dialog),
				"desktop_entry_properties", "Panel");
	
	dedit = gnome_ditem_edit_new ();
	gtk_widget_show (dedit);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    dedit, TRUE, TRUE, 0);

	if (ditem != NULL) {
		gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (dedit), ditem);
		g_object_set_data_full (G_OBJECT (dedit), "location",
					g_strdup (gnome_desktop_item_get_location (ditem)),
					(GDestroyNotify)g_free);
		g_free (dirfile);
		dirfile = NULL;
	} else {
		ditem = gnome_desktop_item_new ();
		if (sim->mf->dir_name == NULL) {
			gnome_desktop_item_set_string (ditem,
						       GNOME_DESKTOP_ITEM_NAME,
						       "Menu");
			gnome_desktop_item_set_localestring
				(ditem,
				 GNOME_DESKTOP_ITEM_NAME,
				 _("Menu"));
		} else {
			gnome_desktop_item_set_string (ditem,
						       GNOME_DESKTOP_ITEM_NAME,
						       sim->mf->dir_name);
		}
		gnome_desktop_item_set_string (ditem,
					       GNOME_DESKTOP_ITEM_TYPE,
					       "Directory");
		/*we don't have to free dirfile here it will be freed as if
		  we had strduped it here*/
		g_object_set_data_full (G_OBJECT (dedit),
					"location", dirfile,
					(GDestroyNotify)g_free);
		dirfile = NULL;
		gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (dedit), ditem);
	}

	gnome_ditem_edit_set_directory_only (GNOME_DITEM_EDIT (dedit),
					     TRUE /* directory_only */);

	set_ditem_sensitive (GTK_DIALOG (dialog),
			     GNOME_DITEM_EDIT (dedit), sim);

	g_signal_connect (G_OBJECT (dedit), "changed",
			    G_CALLBACK (ditem_properties_changed),
			    NULL);

	g_signal_connect (G_OBJECT (dialog), "destroy",
			    G_CALLBACK (ditem_properties_close),
			    dedit);

	/* YAIKES, the problem here is that the notebook will attempt
	 * to destroy the dedit, so if we unref it in the close handler,
	 * it will be finalized by the time the notebook will destroy it,
	 * dedit is just a horrible thing */
	g_signal_connect (G_OBJECT (dedit), "destroy",
			  G_CALLBACK (g_object_unref),
			  NULL);

	g_object_set_data (G_OBJECT (dialog), "GnomeDItemEdit", dedit);

	if (ditem != NULL) {
		/* pass the dentry as the data to clicked */
		g_signal_connect_data (G_OBJECT (dialog), "response",
				       G_CALLBACK (ditem_properties_clicked),
				       ditem,
				       (GClosureNotify) gnome_desktop_item_unref,
				       0 /* connect_flags */);
	} else {
		g_signal_connect (G_OBJECT (dialog), "response",
				    G_CALLBACK (ditem_properties_clicked),
				    NULL);
	}

	gtk_widget_show (dialog);

	gnome_ditem_edit_grab_focus (GNOME_DITEM_EDIT (dedit));
}

static void
show_item_menu (GtkWidget *item, GdkEventButton *bevent, ShowItemMenu *sim)
{
	GtkWidget *menuitem;
	char *tmp;
	GnomeDesktopItem *ii;

	if (sim->menu == NULL) {
		sim->menu = menu_new ();

		g_object_set_data (G_OBJECT (sim->menu), "menu_panel",
				   get_panel_from_menu_data (sim->menuitem,
							     TRUE));
		
		g_signal_connect (G_OBJECT (sim->menu), "deactivate",
				  G_CALLBACK (restore_grabs),
				  item);

		if (sim->type == 1) {
			ii = gnome_desktop_item_new_from_uri (sim->item_loc, 0, NULL);

			menuitem = gtk_image_menu_item_new ();
			if ( ! sim->applet)
				setup_menuitem (menuitem, NULL,
						_("Add this launcher to panel"));
			else
				setup_menuitem (menuitem, NULL,
						_("Add this applet as a launcher to panel"));
			gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu), menuitem);
			g_signal_connect (G_OBJECT(menuitem), "activate",
					    G_CALLBACK(add_app_to_panel),
					    (gpointer)sim->item_loc);

			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, NULL,
					_("Remove this item"));
			gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu), menuitem);
			g_signal_connect (G_OBJECT(menuitem), "activate",
					    G_CALLBACK (remove_menuitem),
					    sim);
			tmp = g_path_get_dirname(sim->item_loc);
			if (access (tmp, W_OK) != 0)
				gtk_widget_set_sensitive(menuitem,FALSE);
			g_free (tmp);
			g_signal_connect_swapped (G_OBJECT (menuitem),
						   "activate",
						   G_CALLBACK (gtk_menu_shell_deactivate),
						   G_OBJECT (item->parent));

			if ( ! sim->applet) {
				menuitem = gtk_image_menu_item_new ();
				setup_menuitem (menuitem, NULL,
						_("Put into run dialog"));
				gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu),
						 menuitem);
				g_signal_connect (G_OBJECT(menuitem), "activate",
					          G_CALLBACK(add_to_run_dialog),
					          (gpointer)sim->item_loc);
				g_signal_connect_swapped
					(G_OBJECT(menuitem),
					 "activate",
					 G_CALLBACK(gtk_menu_shell_deactivate),
					 G_OBJECT(item->parent));
			}

			if (ii != NULL) {
				const char *doc_path = gnome_desktop_item_get_string (ii, "DocPath");
				tmp = panel_gnome_kde_help_path (doc_path);
			} else {
				tmp = NULL;
			}

			if (tmp != NULL) {
				char *title;
				const char *name;

				g_free (tmp);

				menuitem = gtk_image_menu_item_new ();
				name = gnome_desktop_item_get_localestring (ii, GNOME_DESKTOP_ITEM_NAME);
				title = g_strdup_printf (_("Help on %s"),
							 (name != NULL) ? name : _("Application"));
				setup_menuitem (menuitem, NULL, title);
				g_free (title);
				gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu),
						 menuitem);
				g_signal_connect (G_OBJECT(menuitem), "activate",
					          G_CALLBACK(show_help_on),
					          (gpointer)sim->item_loc);
				g_signal_connect_swapped (G_OBJECT(menuitem),
					 		  "activate",
					 		   G_CALLBACK(gtk_menu_shell_deactivate),
					 		   G_OBJECT(item->parent));
			}

			menuitem = gtk_image_menu_item_new ();
			/*when activated we must pop down the first menu*/
			g_signal_connect_swapped (G_OBJECT (menuitem),
						   "activate",
						   G_CALLBACK (gtk_menu_shell_deactivate),
						   G_OBJECT (item->parent));

			g_signal_connect (G_OBJECT(menuitem),
					   "activate",
					   G_CALLBACK(edit_dentry),
					   sim);
			setup_menuitem (menuitem, NULL, _("Properties..."));
			gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu), menuitem);

			gnome_desktop_item_unref (ii);
		}
		
		if (sim->mf != NULL) {
			GtkWidget *submenu;

			if (sim->type == 0) {
				submenu = sim->menu;
			} else {
				submenu = menu_new ();

				g_object_set_data
					(G_OBJECT(submenu), "menu_panel",
					 get_panel_from_menu_data (sim->menuitem,
								   TRUE));

				menuitem = gtk_image_menu_item_new ();
				setup_menuitem (menuitem, NULL,
						_("Entire menu"));
				gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu), menuitem);
				gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
							   submenu);
			}


			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, NULL,
					_("Add this as drawer to panel"));
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
			g_signal_connect (G_OBJECT(menuitem), "activate",
				   G_CALLBACK(add_menudrawer_to_panel),
				   sim->mf);

			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, NULL,
					_("Add this as menu to panel"));
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
			g_signal_connect (G_OBJECT(menuitem), "activate",
					   G_CALLBACK(add_menu_to_panel),
					   sim->mf->menudir);

			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, NULL,
					_("Add new item to this menu"));
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
			/*when activated we must pop down the first menu*/
			g_signal_connect_swapped (G_OBJECT (menuitem),
						 "activate",
						 G_CALLBACK (gtk_menu_shell_deactivate),
						 G_OBJECT (item->parent));

			g_signal_connect (G_OBJECT(menuitem), "activate",
					    G_CALLBACK(add_new_app_to_menu),
					    sim->mf->menudir);
			if (access (sim->mf->menudir, W_OK) != 0)
				gtk_widget_set_sensitive (menuitem, FALSE);


			menuitem = gtk_image_menu_item_new ();
			/*when activated we must pop down the first menu*/
			g_signal_connect_swapped (G_OBJECT (menuitem),
						 "activate",
						 G_CALLBACK (gtk_menu_shell_deactivate),
						 G_OBJECT (item->parent));

			g_signal_connect (G_OBJECT (menuitem),
					    "activate",
					    G_CALLBACK (edit_direntry),
					    sim);
			setup_menuitem (menuitem, NULL, _("Properties..."));
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
		}
	}
	
	gtk_menu_popup (GTK_MENU (sim->menu),
			NULL,
			NULL,
			NULL,
			NULL,
			bevent->button,
			bevent->time);
}

static gboolean
show_item_menu_mi_cb (GtkWidget *w, GdkEvent *event, ShowItemMenu *sim)
{
	GdkEventButton *bevent = (GdkEventButton *)event;

	/* no item menu in commie mode */
	if (commie_mode)
		return FALSE;
	
	if (event->type == GDK_BUTTON_PRESS &&
	    bevent->button == 3)
		show_item_menu (w, bevent, sim);
	
	return FALSE;
}

static void
destroy_item_menu(GtkWidget *w, ShowItemMenu *sim)
{
	/*NOTE: don't free item_loc or mf.. it's not ours and will be free'd
	  elsewhere*/
	if (sim->menu != NULL)
		g_object_unref (G_OBJECT (sim->menu));
	sim->menu = NULL;
	g_free (sim);
}

/* This is a _horrible_ hack to have this here. This needs to be added to the
 * GTK+ menuing code in some manner.
 */
static void  
drag_end_menu_cb (GtkWidget *widget, GdkDragContext     *context)
{
  GtkWidget *xgrab_shell;
  GtkWidget *parent;

  /* Find the last viewable ancestor, and make an X grab on it
   */
  parent = widget->parent;
  xgrab_shell = NULL;
  while (parent)
    {
      gboolean viewable = TRUE;
      GtkWidget *tmp = parent;
      
      while (tmp)
	{
	  if (!GTK_WIDGET_MAPPED (tmp))
	    {
	      viewable = FALSE;
	      break;
	    }
	  tmp = tmp->parent;
	}
      
      if (viewable)
	xgrab_shell = parent;
      
      parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
    }
  
  if (xgrab_shell && !GTK_MENU(xgrab_shell)->torn_off)
    {
      GdkCursor *cursor = gdk_cursor_new (GDK_ARROW);

      if ((gdk_pointer_grab (xgrab_shell->window, TRUE,
			     GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			     GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
			     GDK_POINTER_MOTION_MASK,
			     NULL, cursor, GDK_CURRENT_TIME) == 0))
	{
	  if (gdk_keyboard_grab (xgrab_shell->window, TRUE,
				 GDK_CURRENT_TIME) == 0)
	    GTK_MENU_SHELL (xgrab_shell)->have_xgrab = TRUE;
	  else
	    {
	      gdk_pointer_ungrab (GDK_CURRENT_TIME);
	    }
	}

      gdk_cursor_unref (cursor);
    }
}

static void  
drag_data_get_menu_cb (GtkWidget *widget, GdkDragContext     *context,
		       GtkSelectionData   *selection_data, guint info,
		       guint time, char *item_loc)
{
	gchar *uri_list = g_strconcat (item_loc, "\r\n", NULL);
	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)uri_list,
				strlen (uri_list));
	g_free (uri_list);
}

static void  
drag_data_get_string_cb (GtkWidget *widget, GdkDragContext     *context,
			 GtkSelectionData   *selection_data, guint info,
			 guint time, const char *string)
{
	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)string,
				strlen(string));
}

static void
setup_full_menuitem (GtkWidget *menuitem, GtkWidget *pixmap,
		     const char *title, const char *item_loc,
		     gboolean applet, MenuFinfo *mf)
			       
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "text/uri-list", 0, 0 }
	};

	GtkWidget *label;

	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);
	
	gtk_container_add (GTK_CONTAINER (menuitem), label);

	if (pixmap != NULL) {
		g_object_set_data_full (G_OBJECT (menuitem),
					"Panel:Image",
					g_object_ref (G_OBJECT (pixmap)),
					(GDestroyNotify) g_free);
		gtk_widget_show (pixmap);
		if (panel_menu_have_icons ())
			gtk_image_menu_item_set_image
				(GTK_IMAGE_MENU_ITEM (menuitem), pixmap);
	}

	if (item_loc != NULL) {
		ShowItemMenu *sim = g_new0 (ShowItemMenu, 1);
		sim->type = 1;
		sim->item_loc = item_loc; /*make sure you don't free this,
					    it's not ours!*/
		sim->applet = applet;
		sim->mf = mf;
		sim->menuitem = menuitem;
		g_signal_connect (G_OBJECT (menuitem), "event",
				  G_CALLBACK (show_item_menu_mi_cb),
				  sim);
		g_signal_connect (G_OBJECT (menuitem), "destroy",
				  G_CALLBACK (destroy_item_menu),
				  sim);

		/*applets have their own drag'n'drop*/
		if ( ! applet && ! commie_mode) {
			gtk_drag_source_set(menuitem,
					    GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
					    menu_item_targets, 1,
					    GDK_ACTION_COPY);

			g_signal_connect (G_OBJECT(menuitem), "drag_data_get",
					   G_CALLBACK(drag_data_get_menu_cb),
					   (gpointer)item_loc);
			g_signal_connect (G_OBJECT(menuitem), "drag_end",
					   G_CALLBACK(drag_end_menu_cb), NULL);
		}
	}

	gtk_widget_show (menuitem);
}

void
setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, const char *title)
{
	setup_full_menuitem (menuitem, pixmap, title, NULL, FALSE, NULL);
}

void
setup_internal_applet_drag (GtkWidget *menuitem, const char *applet_type)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};
	
	if (applet_type == NULL ||
	    commie_mode)
		return;
	
	gtk_drag_source_set (menuitem,
			     GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			     menu_item_targets, 1,
			     GDK_ACTION_COPY);
	
	g_signal_connect_data (G_OBJECT (menuitem), "drag_data_get",
			       G_CALLBACK (drag_data_get_string_cb),
			       g_strdup (applet_type),
			       (GClosureNotify)g_free,
			       0 /* connect_flags */);
	g_signal_connect (G_OBJECT (menuitem), "drag_end",
			  G_CALLBACK (drag_end_menu_cb), NULL);

}

static void
setup_applet_drag (GtkWidget *menuitem, const char *goad_id)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet", 0, 0 }
	};
	
	if (goad_id == NULL)
		return;
	
	gtk_drag_source_set (menuitem,
			     GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			     menu_item_targets, 1,
			     GDK_ACTION_COPY);

	/*note: goad_id should be alive long enough!!*/
	g_signal_connect (G_OBJECT (menuitem), "drag_data_get",
			    G_CALLBACK (drag_data_get_string_cb),
			    (gpointer)goad_id);
	g_signal_connect (G_OBJECT (menuitem), "drag_end",
			    G_CALLBACK (drag_end_menu_cb), NULL);

}

static void
add_drawer_to_panel (GtkWidget *widget, gpointer data)
{
	load_drawer_applet (NULL, NULL, NULL,
			    get_panel_from_menu_data(widget, TRUE), 0, FALSE, NULL);
}

static void
add_logout_to_panel (GtkWidget *widget, gpointer data)
{
	load_logout_applet (get_panel_from_menu_data(widget, TRUE), 0, FALSE, FALSE);
}

static void
add_lock_to_panel (GtkWidget *widget, gpointer data)
{
	load_lock_applet (get_panel_from_menu_data(widget, TRUE), 0, FALSE, FALSE);
}

static void
try_add_status_to_panel (GtkWidget *widget, gpointer data)
{
	if(!load_status_applet(get_panel_from_menu_data(widget, TRUE),
			       0, FALSE, FALSE)) {
		GtkWidget *mbox;
		mbox = gtk_message_dialog_new (NULL, 0,
					       GTK_MESSAGE_INFO,
					       GTK_BUTTONS_CLOSE,
					       _("You already have a status "
						 "dock on the panel. You can "
						 "only have one"));
		gtk_window_set_wmclass(GTK_WINDOW(mbox),
				       "no_more_status_dialog","Panel");
		gtk_widget_show_all (mbox);
	}
}

static void
add_applet (GtkWidget *w, const char *item_loc)
{
	/*
	 * FIXME: remove all old applet's menu code
	 */
}

static void
add_launcher (GtkWidget *w, const char *item_loc)
{
	Launcher *launcher;

	launcher = load_launcher_applet
		(item_loc, get_panel_from_menu_data (w, TRUE), 0, FALSE, NULL);

	if (launcher != NULL)
		launcher_hoard (launcher);
}

static void
destroy_mf(MenuFinfo *mf)
{
	if (mf->fr != NULL) {
		DirRec *dr = (DirRec *)mf->fr;
		mf->fr = NULL;
		dr->mfl = g_slist_remove(dr->mfl, mf);
	}
	g_free(mf->menudir);
	mf->menudir = NULL;
	g_free(mf->dir_name);
	mf->dir_name = NULL;
	g_free(mf->pixmap_name);
	mf->pixmap_name = NULL;
	g_free(mf);
}


static void
menu_destroy(GtkWidget *menu, gpointer data)
{
	GSList *mfl = g_object_get_data (G_OBJECT (menu), "mf");
	GSList *li;
	for (li = mfl; li != NULL; li = li->next) {
		MenuFinfo *mf = li->data;
		destroy_mf (mf);
	}
	g_slist_free (mfl);
	g_object_set_data (G_OBJECT (menu), "mf", NULL);
}

/*reread the applet menu, not a submenu*/
static void
check_and_reread_applet (Menu *menu)
{
	GSList *mfl, *list;

	if (menu_need_reread (menu->menu)) {
		mfl = g_object_get_data (G_OBJECT (menu->menu), "mf");

		/*that will be destroyed in add_menu_widget*/
		if (menu->main_menu) {
			add_menu_widget (menu, NULL, NULL, TRUE);
		} else {
			GSList *dirlist = NULL;
			for(list = mfl; list != NULL;
			    list = list->next) {
				MenuFinfo *mf = list->data;
				dirlist = g_slist_append (dirlist,
							  g_strdup (mf->menudir));
			}
			add_menu_widget (menu, NULL, dirlist, TRUE);

			g_slist_foreach (dirlist, (GFunc)g_free, NULL);
			g_slist_free (dirlist);
		}
	}
}

/* XXX: hmmm the gtk_menu_reposition only calls
   gtk_menu_position if the widget is drawable, but that's not the
   case when we want to do it*/
void
our_gtk_menu_position (GtkMenu *menu)
{
	/* HACK! */
	gboolean old_visible = GTK_WIDGET_VISIBLE (menu);
	gboolean old_mapped = GTK_WIDGET_MAPPED (menu);
	GTK_WIDGET_SET_FLAGS (menu, GTK_VISIBLE | GTK_MAPPED);
	gtk_menu_reposition (menu);
	if ( ! old_visible)
		GTK_WIDGET_UNSET_FLAGS (menu, GTK_VISIBLE);
	if ( ! old_mapped)
		GTK_WIDGET_UNSET_FLAGS (menu, GTK_MAPPED);
}

/* Stolen from GTK+
 * Reparent the menu, taking care of the refcounting
 */
static void 
my_gtk_menu_reparent (GtkMenu      *menu, 
		      GtkWidget    *new_parent, 
		      gboolean      unrealize)
{
  GObject *object = G_OBJECT (menu);
  GtkWidget *widget = GTK_WIDGET (menu);
  gboolean was_floating = GTK_OBJECT_FLOATING (object);

  g_object_ref (object);
  gtk_object_sink (GTK_OBJECT (menu));

  if (unrealize)
    {
      g_object_ref (object);
      gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
      gtk_container_add (GTK_CONTAINER (new_parent), widget);
      g_object_unref (object);
    }
  else
    gtk_widget_reparent (GTK_WIDGET (menu), new_parent);
  gtk_widget_set_size_request (new_parent, -1, -1);
  
  if (was_floating)
    GTK_OBJECT_SET_FLAGS (object, GTK_FLOATING);
  else
    g_object_unref (object);
}

static gboolean
move_window_handler (gpointer data)
{
	int x, y, sx, sy, wx, wy, foox, fooy;
	GtkWidget *win = data;

	data = g_object_get_data (G_OBJECT (win), "move_speed_x");
	sx = GPOINTER_TO_INT (data);
	data = g_object_get_data (G_OBJECT (win), "move_speed_y");
	sy = GPOINTER_TO_INT (data);

	gdk_window_get_pointer (NULL, &x, &y, NULL);
	wx = win->allocation.x;
	wy = win->allocation.y;

	foox = wx + (win->allocation.width / 2);
	fooy = wy + (win->allocation.height / 2);

	if (sqrt ((foox - x)*(foox - x) + (fooy - y)*(fooy - y)) <
	    MAX (win->allocation.width, win->allocation.height)) {
		if (foox < x) sx -= 5;
		else sx += 5;
		if (fooy < y) sy -= 5;
		else sy += 5;
	} else {
		sx /= 2;
		sy /= 2;
	}
	
	if (sx > 50) sx = 50;
	else if (sx < -50) sx = -50;
	if (sy > 50) sy = 50;
	else if (sy < -50) sy = -50;

	wx += sx;
	wy += sy;

	if (wx < 0) wx = 0;
	if (wy < 0) wy = 0;
	if (wx + win->allocation.width > gdk_screen_width ())
		wx = gdk_screen_width () - win->allocation.width;
	if (wy + win->allocation.height > gdk_screen_height ())
		wy = gdk_screen_height () - win->allocation.height;

	gtk_window_move (GTK_WINDOW (win), wx, wy);
	win->allocation.x = wx;
	win->allocation.y = wy;

	data = GINT_TO_POINTER (sx);
	g_object_set_data (G_OBJECT (win), "move_speed_x", data);
	data = GINT_TO_POINTER (sy);
	g_object_set_data (G_OBJECT (win), "move_speed_y", data);

	return TRUE;
}

static void
move_window_destroyed (GtkWidget *win)
{
	gpointer data = g_object_get_data (G_OBJECT (win), "move_window_handler");
	int handler = GPOINTER_TO_INT (data);

	if (handler != 0)
		gtk_timeout_remove (handler);
	g_object_set_data (G_OBJECT (win), "move_window_handler", NULL);
}

static void
doblah (GtkWidget *window)
{
	gpointer data = g_object_get_data (G_OBJECT (window), "move_window_handler");
	int handler = GPOINTER_TO_INT (data);

	if (handler == 0) {
		handler = gtk_timeout_add (30, move_window_handler, window);
		data = GINT_TO_POINTER (handler);
		g_object_set_data (G_OBJECT (window), "move_window_handler", data);
		g_signal_connect (G_OBJECT (window), "destroy",
				    G_CALLBACK (move_window_destroyed),
				    NULL);
	}
}

/*mostly stolen from GTK+ */
static gboolean
my_gtk_menu_window_event (GtkWidget *window,
			  GdkEvent  *event,
			  GtkWidget *menu)
{
	gboolean handled = FALSE;

	g_object_ref (G_OBJECT (window));
	g_object_ref (G_OBJECT (menu));

	switch (event->type) {
		static int foo = 0;
	case GDK_KEY_PRESS:
		if((event->key.state & GDK_CONTROL_MASK) && foo < 4) {
			switch (event->key.keyval) {
			case GDK_r:
			case GDK_R:
				if(foo == 3) { doblah (window); } foo = 0; break;
			case GDK_a:
			case GDK_A:
				if(foo == 2) { foo++; } else { foo = 0; } break;
			case GDK_e:
			case GDK_E:
				if(foo == 1) { foo++; } else { foo = 0; } break;
			case GDK_f:
			case GDK_F:
				if(foo == 0) { foo++; } else { foo = 0; } break;
			default:
				foo = 0;
			}
		}
		/* fall thru */
	case GDK_KEY_RELEASE:
		gtk_widget_event (menu, event);
		handled = TRUE;
		break;
	default:
		break;
	}

	g_object_unref (G_OBJECT (window));
	g_object_unref (G_OBJECT (menu));

	return handled;
}

static gulong wmclass_number = 0;

static char *
get_unique_tearoff_wmclass (void)
{
	static char buf[256];

	g_snprintf (buf, sizeof (buf), "panel_tearoff_%lu", wmclass_number++);

	return buf;
}

static void
show_tearoff_menu (GtkWidget *menu, const char *title, gboolean cursor_position,
		   int x, int y, const char *wmclass)
{
	GtkWidget *win;
	win = GTK_MENU(menu)->tearoff_window =
		gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass (GTK_WINDOW (win),
				wmclass, "Panel");
	gtk_widget_set_app_paintable (win, TRUE);
	g_signal_connect (G_OBJECT (win), "event",
			  G_CALLBACK (my_gtk_menu_window_event), 
			  menu);
	gtk_widget_realize (win);
	      
	gdk_window_set_title (win->window, title);
	
	gdk_window_set_decorations (win->window, 
				    GDK_DECOR_ALL |
				    GDK_DECOR_RESIZEH |
				    GDK_DECOR_MINIMIZE |
				    GDK_DECOR_MAXIMIZE);
	my_gtk_menu_reparent (GTK_MENU (menu), win, FALSE);

	/* set sticky so that we mask the fact that we have no clue
	   how to restore non sticky windows */
	gtk_window_stick (GTK_WINDOW (win));
	
	GTK_MENU (menu)->torn_off = TRUE;

	if (cursor_position)
		our_gtk_menu_position (GTK_MENU (menu));
	else
		gtk_window_move (GTK_WINDOW (win), x, y);

	gtk_widget_show (GTK_WIDGET (menu));
	gtk_widget_show (win);
}

static void
tearoff_destroyed (GtkWidget *tearoff, TearoffMenu *tm)
{
	tearoffs = g_slist_remove(tearoffs, tm);
	g_free(tm->title);
	tm->title = NULL;
	g_free(tm->wmclass);
	tm->wmclass = NULL;
	g_free(tm->special);
	tm->special = NULL;
	g_free(tm);
}

static void
tearoff_new_menu(GtkWidget *item, GtkWidget *menuw)
{
	GSList *mfl = g_object_get_data (G_OBJECT (menuw), "mf");
	GSList *list;
	GtkWidget *menu;
	GString *title;
	TearoffMenu *tm;
	char *wmclass;
	PanelWidget *menu_panel;
	
	if (mfl == NULL)
		return;

	menu = menu_new();

	menu_panel = get_panel_from_menu_data(menuw, TRUE);

	/*set the panel to use as the data*/
	g_object_set_data (G_OBJECT (menu), "menu_panel", menu_panel);

	panel_signal_connect_object_while_alive (G_OBJECT (menu_panel),
						 "destroy",
						 G_CALLBACK (g_object_unref),
						 G_OBJECT(menu));
	
	title = g_string_new(NULL);

	for(list = mfl; list != NULL; list = list->next) {
		MenuFinfo *mf = list->data;

		menu = create_menu_at_fr (menu,
					  mf->fr,
					  mf->applets,
					  mf->launcher_add,
					  mf->dir_name,
					  mf->pixmap_name,
					  TRUE /*fake_submenus*/,
					  FALSE /*force*/);
		
		if (list != mfl)
			g_string_append_c (title, ' ');
		g_string_append (title, mf->dir_name);
	}

	wmclass = get_unique_tearoff_wmclass ();
	show_tearoff_menu(menu, title->str, TRUE, 0, 0, wmclass);

	tm = g_new0 (TearoffMenu, 1);
	tm->menu = menu;
	tm->mfl = g_object_get_data (G_OBJECT (menu), "mf");
	tm->special = NULL;
	tm->title = title->str;
	tm->wmclass = g_strdup (wmclass);
	g_signal_connect (G_OBJECT (menu), "destroy",
			    G_CALLBACK (tearoff_destroyed), tm);

	tearoffs = g_slist_prepend (tearoffs, tm);

	g_string_free (title, FALSE);

	need_complete_save = TRUE;
}

/*
 * menu_add_tearoff:
 * @menu:
 * @func:
 * @user_data:
 *
 * Add a tear-off menuitem to @menu and connect @func with @user_data
 * to the menuitem's "activate" signal.
 *
 * Return value: #FALSE if the user global preferences don't allow 
 *               tear-off menuitems.
 */
static gboolean
menu_add_tearoff (GtkMenu       *menu, 
		  GtkSignalFunc  func,
		  gpointer       user_data)
{
	GtkWidget *menuitem;

        if (!panel_menu_have_tearoff ())
		return FALSE;

	menuitem = tearoff_item_new ();

	gtk_widget_show (menuitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT (menuitem), "activate", func, user_data);

	return TRUE;
}

/*BTW, this also updates the fr entires */
gboolean
menu_need_reread(GtkWidget *menuw)
{
	GSList *mfl = g_object_get_data (G_OBJECT (menuw), "mf");
	GSList *list;
	gboolean need_reread = FALSE;

	/*if(!mfl)
	  g_warning("Weird menu doesn't have mf entry");*/

	/* FIXME: figure out how to update tornoff menus */
	if (GTK_MENU(menuw)->torn_off)
		return FALSE;

	if (g_object_get_data (G_OBJECT (menuw), "need_reread")) {
		need_reread = TRUE;
		g_object_set_data (G_OBJECT(menuw), "need_reread", NULL);
	}
	
	/*check if we need to reread this*/
	for(list = mfl; list != NULL; list = list->next) {
		MenuFinfo *mf = list->data;
		if(mf->fake_menu ||
		   mf->fr == NULL) {
			if(mf->fr != NULL)
				mf->fr = fr_replace(mf->fr);
			else
				mf->fr = fr_get_dir(mf->menudir);
			need_reread = TRUE;
		} else {
			FileRec *fr;
			fr = fr_check_and_reread(mf->fr);
			if(fr != mf->fr ||
			   fr == NULL) {
				need_reread = TRUE;
				mf->fr = fr;
			}
		}
	}

	return need_reread;
}

void
submenu_to_display (GtkWidget *menuw, gpointer data)
{
	GSList *mfl, *list;
	gboolean add_launcher_hack;

	if (GTK_MENU(menuw)->torn_off)
		return;

	/*this no longer constitutes a bad hack, now it's purely cool :)*/
	if( ! menu_need_reread(menuw))
		return;

	/* EEEEEK! hacks */
	add_launcher_hack = 
		GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuw),
						    "_add_launcher_menu_hack_"));

	/* Note this MUST be destroy and not unref, unref would fuck
	 * up here, we don't hold a reference to them, so we must
	 * destroy them, menu shell will unref these */
	while(GTK_MENU_SHELL(menuw)->children)
		gtk_widget_destroy(GTK_MENU_SHELL(menuw)->children->data);

	if (add_launcher_hack) {
		create_add_launcher_menu (menuw, TRUE /* fake_submenus */);
	} else {

		menu_add_tearoff (GTK_MENU (menuw), G_CALLBACK (tearoff_new_menu), menuw);

		mfl = g_object_get_data (G_OBJECT (menuw), "mf");

		g_object_set_data (G_OBJECT (menuw), "mf", NULL);
		for(list = mfl;
		    list != NULL;
		    list = list->next) {
			MenuFinfo *mf = list->data;

			menuw = create_menu_at_fr (menuw,
						   mf->fr,
						   mf->applets,
						   mf->launcher_add,
						   mf->dir_name,
						   mf->pixmap_name,
						   TRUE /*fake_submenus*/,
						   FALSE /*force*/);
			destroy_mf(mf);
		}
		g_slist_free(mfl);
	}

	our_gtk_menu_position(GTK_MENU(menuw));
}

GtkWidget *
create_fake_menu_at (const char *menudir,
		     gboolean applets,
		     gboolean launcher_add,
		     const char *dir_name,
		     const char *pixmap_name)
{	
	MenuFinfo *mf;
	GtkWidget *menu;
	GSList *list;
	
	menu = menu_new ();

	mf = g_new0 (MenuFinfo, 1);
	mf->menudir = g_strdup (menudir);
	mf->applets = applets;
	mf->launcher_add = launcher_add;
	mf->dir_name = g_strdup (dir_name);
	mf->pixmap_name = g_strdup (pixmap_name);
	mf->fake_menu = TRUE;
	mf->fr = NULL;
	
	list = g_slist_prepend(NULL, mf);
	g_object_set_data (G_OBJECT (menu), "mf", list);
	
	g_signal_connect (G_OBJECT(menu), "destroy",
			   G_CALLBACK(menu_destroy), NULL);
	
	return menu;
}

static gboolean
create_menuitem (GtkWidget *menu,
		 FileRec *fr,
		 gboolean applets,
		 gboolean launcher_add,
		 gboolean fake_submenus,
		 gboolean *add_separator,
		 int *first_item,
		 MenuFinfo *mf)
{
	GtkWidget *menuitem, *sub;
	char *itemname;
	const char *icon, *fallback;
	
	g_return_val_if_fail (fr != NULL, FALSE);

	if(fr->type == FILE_REC_EXTRA)
		return FALSE;


	if(fr->type == FILE_REC_FILE && applets &&
	   !fr->goad_id) {
		g_warning(_("Can't get goad_id for applet, ignoring it"));
		return FALSE;
	}

	sub = NULL;
	if(fr->fullname) {
		itemname = g_strdup(fr->fullname);
	} else {
		char *p;
		itemname = g_path_get_basename (fr->name);
		p = strrchr(itemname, '.');
		if(p) *p = '\0';
	}

	icon = fr->icon;
	fallback = NULL;

	if(fr->type == FILE_REC_DIR) {

		if (icon == NULL)
			icon = gnome_folder;
		else
			fallback = gnome_folder;

		if(fake_submenus)
			sub = create_fake_menu_at (fr->name,
						   applets,
						   launcher_add,
						   itemname,
						   fr->icon);
		else
			sub = create_menu_at_fr (NULL, fr,
						 applets,
						 launcher_add,
						 itemname,
						 fr->icon,
						 fake_submenus,
						 FALSE /*force*/);

		if (!sub) {
			g_free(itemname);
			return FALSE;
		}
	}

	menuitem = gtk_image_menu_item_new ();
	if (sub) {
		gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), sub);
		g_signal_connect (G_OBJECT(sub), "show",
				  G_CALLBACK(submenu_to_display), NULL);
	}

	if (icon != NULL) {
		panel_load_menu_image_deferred (menuitem, icon, fallback,
						FALSE /* force_image */);
	}

	if (sub)
	        setup_full_menuitem (menuitem, NULL, itemname,
				     NULL, FALSE, mf);

	else if (strstr (fr->name,"/applets/") && fr->goad_id) {
		setup_applet_drag (menuitem, fr->goad_id);
		setup_full_menuitem (menuitem, NULL, itemname,
				     fr->name, TRUE, mf);

	} else
	        setup_full_menuitem (menuitem, NULL, itemname,
				     fr->name, FALSE, mf);


	if(*add_separator) {
		add_menu_separator(menu);
		(*first_item)++;
		*add_separator = FALSE;
	}
	
	if(fr->comment)
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      fr->comment, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	if(!sub) {
		if (launcher_add)
			g_signal_connect (G_OBJECT (menuitem), "activate",
					    G_CALLBACK (add_launcher),
					    fr->name);
		else if (applets)
			g_signal_connect (G_OBJECT (menuitem), "activate",
					    G_CALLBACK (add_applet),
					    fr->name);
		else
			g_signal_connect (G_OBJECT (menuitem), "activate",
					    G_CALLBACK (activate_app_def),
					    fr->name);
	}

	g_free (itemname);

	return TRUE;
}

GtkWidget *
create_menu_at (GtkWidget *menu,
		const char *menudir,
		gboolean applets,
		gboolean launcher_add,
		const char *dir_name,
		const char *pixmap_name,
		gboolean fake_submenus,
		gboolean force)
{
	return create_menu_at_fr (menu,
				  fr_get_dir (menudir),
				  applets,
				  launcher_add,
				  dir_name,
				  pixmap_name,
				  fake_submenus,
				  force);
}

static GtkWidget *
create_menu_at_fr (GtkWidget *menu,
		   FileRec *fr,
		   gboolean applets,
		   gboolean launcher_add,
		   const char *dir_name,
		   const char *pixmap_name,
		   gboolean fake_submenus,
		   gboolean force)
{	
	GSList *li;
	GSList *mfl = NULL;
	gboolean add_separator = FALSE;
	int first_item = 0;
	GtkWidget *menuitem;
	MenuFinfo *mf = NULL;
	DirRec *dr = (DirRec *)fr;

	g_return_val_if_fail(!(fr&&fr->type!=FILE_REC_DIR),menu);
	
	if( ! force &&
	   fr == NULL)
		return menu;

	/* unfilled out, but the pointer will be correct */
	mf = g_new0 (MenuFinfo, 1);
	
	/*get this info ONLY if we haven't gotten it already*/
	if (dir_name == NULL)
		dir_name = (fr != NULL && fr->fullname != NULL)
			? fr->fullname
			: _("Menu");
	if (pixmap_name == NULL)
		pixmap_name = (fr != NULL && fr->icon != NULL)
			?  fr->icon
			: gnome_folder;
	
	if (menu == NULL) {
		menu = menu_new ();

		if (menu_add_tearoff (GTK_MENU (menu), 
				      G_CALLBACK (tearoff_new_menu),
				      menu))
			first_item++;

		g_signal_connect (G_OBJECT(menu), "destroy",
				    G_CALLBACK (menu_destroy), NULL);
	} else {
		first_item = g_list_length(GTK_MENU_SHELL(menu)->children);
		mfl = g_object_get_data (G_OBJECT (menu), "mf");
		if(GTK_MENU_SHELL(menu)->children &&
		   !(GTK_MENU_SHELL(menu)->children->next == NULL &&
		     TEAROFF_IS_ITEM(GTK_MENU_SHELL(menu)->children->data)))
			add_separator = TRUE;
	}

	if (fr != NULL) {
		/* Last added points to the last fr list item that was successfuly
		 * added as a menu item */
		GSList *last_added = NULL;
		for(li = dr->recs; li != NULL; li = li->next) {
			FileRec *tfr = li->data;

#ifdef MENU_DEBUG
			g_print ("FileRec: '%s'\n", tfr->name);
#endif /* MENU_DEBUG */

			if (tfr->type == FILE_REC_SEP)
				add_menu_separator (menu);				
			else if (create_menuitem (menu, tfr,
						  applets,
						  launcher_add,
						  fake_submenus,
						  &add_separator,
						  &first_item,
						  mf))
				last_added = li;
		}
	}

	mf->applets        = applets;
	mf->launcher_add   = launcher_add;
	mf->dir_name       = g_strdup (dir_name);
	mf->pixmap_name    = g_strdup (pixmap_name);
	mf->fake_menu      = FALSE;
	mf->fr             = fr;

	if (fr) {
		dr->mfl = g_slist_prepend (dr->mfl, mf);

		mf->menudir = g_strdup (fr->name);
	}

	/*add separator*/
	if (add_separator) {
		menuitem = gtk_image_menu_item_new();	
		gtk_menu_shell_insert(GTK_MENU_SHELL(menu), menuitem, first_item);
		gtk_widget_show(menuitem);
		gtk_widget_set_sensitive(menuitem, FALSE);
		add_separator = FALSE;
	}

	mfl = g_slist_append (mfl, mf);

	g_object_set_data (G_OBJECT (menu), "mf", mfl);

	return menu;
}

static void
destroy_menu (GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	GtkWidget *prop_dialog = menu->prop_dialog;

	menu->prop_dialog = NULL;

	if (prop_dialog != NULL)
		g_object_unref (G_OBJECT (prop_dialog));

	menu->button = NULL;

	if (menu->menu != NULL)
		g_object_unref (G_OBJECT (menu->menu));
	menu->menu = NULL;
}

static void
free_menu (gpointer data)
{
	Menu *menu = data;

	g_free (menu->path);
	menu->path = NULL;

	g_free (menu->custom_icon_file);
	menu->custom_icon_file = NULL;

	g_free(menu);
}

static void
menu_deactivate (GtkWidget *w, gpointer data)
{
	Menu *menu = data;
	GtkWidget *panel = get_panel_parent (menu->button);

	/* allow the panel to hide again */
	if (BASEP_IS_WIDGET (panel))
		BASEP_WIDGET (panel)->autohide_inhibit = FALSE;
	BUTTON_WIDGET (menu->button)->in_button = FALSE;
	BUTTON_WIDGET (menu->button)->ignore_leave = FALSE;
	button_widget_up (BUTTON_WIDGET (menu->button));
	menu->age = 0;
}

/*
 * FIXME: figure out how to handle internationalised strings.
 * FIXME: lazily create a hashtable
 */
static const gchar *
applet_menu_get_category_icon (const gchar *category)
{
	if (!strcmp (category, "Amusements")) {
		return "gnome-amusements.png";
	} else if (!strcmp (category, "Clocks")) {
		return "gnome-clock.png";
	} else if (!strcmp (category, "Monitors")) {
		return "gnome-monitor.png";
	} else if (!strcmp (category, "Multimedia")) {
		return "gnome-multimedia.png";
	} else if (!strcmp (category, "Network")) {
		return "gnome-networktool.png";
	} else if (!strcmp (category, "Utility")) {
		return "gnome-util.png";
	}

	return NULL;
}

static GtkWidget *
applet_menu_append (GtkWidget   *menu,
		    const gchar *name,
		    const gchar *icon)
{
	GtkWidget *menuitem;

	menuitem = gtk_image_menu_item_new ();

	if (icon != NULL)
		panel_load_menu_image_deferred (menuitem, icon, NULL,
						FALSE /* force_image */);

	setup_full_menuitem (menuitem, NULL, name, NULL, FALSE, NULL);

	gtk_widget_show_all (menuitem);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	return menuitem;
}

static void
add_bonobo_applet (GtkWidget  *widget,
		   const char *iid)
{
	PanelWidget *panel;

	panel = get_panel_from_menu_data (widget, TRUE);

	panel_applet_frame_load (iid, panel, -1, NULL);

	/*g_free (iid);*/
}

static const gchar applet_requirements [] = 
	"has_all (repo_ids, ['IDL:Bonobo/Control:1.0', 'IDL:GNOME/PanelAppletShell:1.0']) && "
	"defined (panel:icon) && defined (panel:category)";

static gchar *applet_sort_criteria [] = {
	"panel:category",
	"name",
	NULL
	};

static GtkWidget *
create_applets_menu (void)
{
	CORBA_Environment      env;
	Bonobo_ServerInfoList *list;
	GtkWidget             *menu;
	GtkWidget             *prev_menu = NULL;
	const gchar           *prev_category = NULL;
	gint                   i;

	CORBA_exception_init (&env);

	list = bonobo_activation_query (applet_requirements,
					applet_sort_criteria,
					&env);
	if (BONOBO_EX (&env)) {
		g_warning (_("query returned exception %s\n"), BONOBO_EX_REPOID (&env));

		CORBA_exception_free (&env);

		return NULL;
	}

	menu = menu_new ();

	menu_add_tearoff (GTK_MENU (menu), G_CALLBACK (tearoff_new_menu), menu);

	g_signal_connect (G_OBJECT (menu), "destroy", G_CALLBACK (menu_destroy), NULL);

	for (i = 0; i < list->_length; i++) {
		Bonobo_ServerInfo *info;
		GtkWidget         *menuitem;
		const gchar       *name;
		const gchar       *icon;
		const gchar       *category;
		const gchar       *iid;

		info = &list->_buffer [i];

		iid = info->iid;

		name     = bonobo_server_info_prop_lookup (info, "name", NULL);
		icon     = bonobo_server_info_prop_lookup (info, "panel:icon", NULL);
		category = bonobo_server_info_prop_lookup (info, "panel:category", NULL);

		if (string_empty (name)) {
			continue;
		}

		if (string_empty (category)) {
			applet_menu_append (menu, name, icon);
			continue;
		}

		if (prev_category == NULL ||
		    strcmp (prev_category, category) != 0) {
			const gchar *cat_icon;

			prev_category = category;
			prev_menu = menu_new ();
			/* FIXME: EEK! WHY DO WE NEED THIS! THIS SHOULD NOT BE NEEDED! */
			g_signal_connect (G_OBJECT (prev_menu), "show",
					  G_CALLBACK (our_gtk_menu_position), NULL);

			cat_icon = applet_menu_get_category_icon (category);

			menuitem = applet_menu_append (menu, category, cat_icon);

			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), prev_menu);
		}

		menuitem = applet_menu_append (prev_menu, name, icon);

		g_signal_connect (G_OBJECT (menuitem),
				  "activate",
				  G_CALLBACK (add_bonobo_applet),
				  g_strdup (iid));
	}

	CORBA_free (list);

	CORBA_exception_free (&env);

	return menu;
}

static void
find_empty_pos_array (int screen, int posscore[3][3])
{
	GSList *li;
	int i,j;
	PanelData *pd;
	BasePWidget *basep;
	
	int tx, ty;
	int w, h;
	gfloat sw, sw2, sh, sh2;

	sw2 = 2 * (sw = multiscreen_width (screen) / 3);
	sh2 = 2 * (sh = multiscreen_height (screen) / 3);
	
	for (li = panel_list; li != NULL; li = li->next) {
		pd = li->data;

		if (DRAWER_IS_WIDGET(pd->panel) ||
		    FOOBAR_IS_WIDGET (pd->panel))
			continue;

		basep = BASEP_WIDGET (pd->panel);
		
		if (basep->screen != screen)
			continue;

		basep_widget_get_pos (basep, &tx, &ty);
		tx -= multiscreen_x (screen);
		ty -= multiscreen_y (screen);
		basep_widget_get_size (basep, &w, &h);

		if (PANEL_WIDGET (basep->panel)->orient == GTK_ORIENTATION_HORIZONTAL) {
			j = MIN (ty / sh, 2);
			ty = tx + w;
			if (tx < sw) posscore[0][j]++;
			if (tx < sw2 && ty > sw) posscore[1][j]++;
			if (ty > sw2) posscore[2][j]++;
		} else {
			i = MIN (tx / sw, 2);
			tx = ty + h;
			if (ty < sh) posscore[i][0]++;
			if (ty < sh2 && tx > sh) posscore[i][1]++;
			if (tx > sh2) posscore[i][2]++;
		}
	}
}

static void
find_empty_pos (int screen, gint16 *x, gint16 *y)
{
	int posscore[3][3] = { {0,0,0}, {0,512,0}, {0,0,0}};
	int i, j, lowi= 0, lowj = 0;

	find_empty_pos_array (screen, posscore);

	for (j = 2; j >= 0; j--) {
		for (i = 0; i < 3; i++) {
			if (posscore[i][j] < posscore[lowi][lowj]) {
				lowi = i;
				lowj = j;
			}
		}
	}

	*x = ((float)lowi * multiscreen_width (screen)) / 2.0;
	*y = ((float)lowj * multiscreen_height (screen)) / 2.0;

	*x += multiscreen_x (screen);
	*y += multiscreen_y (screen);
}

static BorderEdge
find_empty_edge (int screen)
{
	int posscore[3][3] = { {0,0,0}, {0,512,0}, {0,0,0}};
	int escore [4] = { 0, 0, 0, 0};
	BorderEdge edge = BORDER_BOTTOM;
	int low=512, i;

	find_empty_pos_array (screen, posscore);

	escore[BORDER_TOP] = posscore[0][0] + posscore[1][0] + posscore[2][0];
	escore[BORDER_RIGHT] = posscore[2][0] + posscore[2][1] + posscore[2][2];
	escore[BORDER_BOTTOM] = posscore[0][2] + posscore[1][2] + posscore[2][2];
	escore[BORDER_LEFT] = posscore[0][0] + posscore[0][1] + posscore[0][2];
	
	for (i = 0; i < 4; i++) {
		if (escore[i] < low) {
			edge = i;
			low = escore[i];
		}
	}
	return edge;
}

static void
create_new_panel (GtkWidget *w, gpointer data)
{
	PanelType type = GPOINTER_TO_INT (data);
	GdkColor bcolor = {0, 0, 0, 1};
	gint16 x, y;
	GtkWidget *panel = NULL;
	PanelWidget *menu_panel;
	int screen;

	g_return_if_fail (type != DRAWER_PANEL);

	menu_panel = get_panel_from_menu_data (w, TRUE);
	if (menu_panel != NULL)
		screen = multiscreen_screen_from_panel
			(menu_panel->panel_parent);
	else
		screen = 0;

	switch (type) {
	case ALIGNED_PANEL: 
		find_empty_pos (screen, &x, &y);
		panel = aligned_widget_new (NULL,
					    screen,
					    ALIGNED_LEFT,
					    BORDER_TOP,
					    BASEP_EXPLICIT_HIDE,
					    BASEP_SHOWN,
					    PANEL_SIZE_MEDIUM,
					    TRUE,
					    TRUE,
					    PANEL_BACK_NONE,
					    NULL,
					    TRUE, FALSE, TRUE,
					    &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);
		break;
	case EDGE_PANEL: 
		panel = edge_widget_new (NULL,
					 screen,
					 find_empty_edge (screen),
					 BASEP_EXPLICIT_HIDE,
					 BASEP_SHOWN,
					 PANEL_SIZE_MEDIUM,
					 TRUE,
					 TRUE,
					 PANEL_BACK_NONE,
					 NULL,
					 TRUE, FALSE, TRUE,
					 &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);	
		break;
	case SLIDING_PANEL:
		find_empty_pos (screen, &x, &y);
		panel = sliding_widget_new (NULL,
					    screen,
					    SLIDING_ANCHOR_LEFT, 0,
					    BORDER_TOP,
					    BASEP_EXPLICIT_HIDE,
					    BASEP_SHOWN,
					    PANEL_SIZE_MEDIUM,
					    TRUE, TRUE,
					    PANEL_BACK_NONE,
					    NULL, TRUE, FALSE, TRUE,
					    &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);	
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);
		break;
	case FLOATING_PANEL:
		find_empty_pos (screen, &x, &y);
		panel = floating_widget_new (NULL,
					     screen,
					     x, y,
					     GTK_ORIENTATION_VERTICAL,
					     BASEP_EXPLICIT_HIDE,
					     BASEP_SHOWN,
					     PANEL_SIZE_MEDIUM,
					     TRUE, TRUE,
					     PANEL_BACK_NONE,
					     NULL, TRUE, FALSE, TRUE,
					     &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);
		break;
	case FOOBAR_PANEL: {
		GtkWidget *dialog;
		gchar *s;
		if (!foobar_widget_exists (screen)) {
			const char *panel_id;
			
			panel = foobar_widget_new (NULL, screen);
			panel_id = PANEL_WIDGET (FOOBAR_WIDGET (panel)->panel)->unique_id;

			foobar_widget_set_clock_format (FOOBAR_WIDGET (panel), _("%I: %M: %S: %p"));
			
			panel_setup (panel);
			gtk_widget_show (panel);
			break;
		}
		s = _("You can only have one menu panel at a time.");
		
		dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 s);
		gtk_window_set_wmclass (GTK_WINDOW (dialog),
					"only_one_foobar", "Panel");
		gtk_widget_show_all (dialog);
		break;
	}
	default: break;
	}

	if (panel == NULL)
		return;
		
	panels_to_sync = TRUE;
}

static GtkWidget * create_add_panel_submenu (gboolean tearoff);

static void
add_panel_tearoff_new_menu(GtkWidget *w, gpointer data)
{
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass ();
	GtkWidget *menu = create_add_panel_submenu (FALSE);
	PanelWidget *menu_panel;

	menu_panel = get_panel_from_menu_data (w, TRUE);

	/*set the panel to use as the data*/
	g_object_set_data (G_OBJECT (menu), "menu_panel", menu_panel);
	panel_signal_connect_object_while_alive
		(G_OBJECT (menu_panel), "destroy",
		 G_CALLBACK (g_object_unref),
		 G_OBJECT(menu));

	show_tearoff_menu (menu, _("Create panel"), TRUE, 0, 0, wmclass);

	tm = g_new0 (TearoffMenu, 1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup (_("Create panel"));
	tm->special = g_strdup ("ADD_PANEL");
	tm->wmclass = g_strdup (wmclass);
	g_signal_connect (G_OBJECT(menu), "destroy",
			   G_CALLBACK(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend (tearoffs, tm);
}

static GtkWidget *
create_add_panel_submenu (gboolean tearoff)
{
	GtkWidget *menu, *menuitem;

	menu = menu_new ();
	
	if (tearoff)
		menu_add_tearoff (GTK_MENU (menu), 
				  G_CALLBACK (add_panel_tearoff_new_menu),
				  NULL);

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem, NULL, _("Menu panel"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(create_new_panel),
			   GINT_TO_POINTER(FOOBAR_PANEL));
 	
	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem, NULL, _("Edge panel"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(create_new_panel),
			   GINT_TO_POINTER(EDGE_PANEL));

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem, NULL, _("Aligned panel"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(create_new_panel),
			   GINT_TO_POINTER(ALIGNED_PANEL));

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem, NULL, _("Sliding panel"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(create_new_panel),
			   GINT_TO_POINTER(SLIDING_PANEL));
	
	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem, NULL, _("Floating panel"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(create_new_panel),
			   GINT_TO_POINTER(FLOATING_PANEL));

	return menu;
}

static void
setup_menuitem_try_pixmap (GtkWidget *menuitem, const char *try_file,
			   const char *title)
{
	panel_load_menu_image_deferred (menuitem, try_file, NULL,
					FALSE /* force_image */);

	setup_menuitem (menuitem, NULL /* pixmap */, title);
}
	  

static GtkWidget *
create_system_menu (GtkWidget *menu, gboolean fake_submenus,
		    gboolean fake, gboolean launcher_add)
{
	if ( ! fake ||
	     menu != NULL) {
		menu = create_menu_at (menu, "applications:/",
				       FALSE /* applets */,
				       launcher_add,
				       _("Applications"),
				       "gnome-logo-icon-transparent.png",
				       fake_submenus, FALSE);
	} else {
		menu = create_fake_menu_at ("applications:/",
					    FALSE /* applets */,
					    launcher_add,
					    _("Applications"),
					    "gnome-logo-icon-transparent.png");
	}

	return menu;
}

static GtkWidget *
create_distribution_menu (GtkWidget *menu,
			  gboolean fake_submenus,
			  gboolean fake,
			  gboolean launcher_add)
{
	const DistributionInfo *info = get_distribution_info ();
	gchar *pixmap_file;

	if (!info)
		return NULL;

	if (info->menu_icon != NULL)
		pixmap_file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
							 info->menu_icon, TRUE, NULL);
	else
		pixmap_file = NULL;

	if (!fake || menu) {
		menu = create_menu_at (menu, info->menu_path,
				       FALSE /* applets */,
				       launcher_add,
				       info->menu_name, pixmap_file,
				       fake_submenus, FALSE);
	} else {
		menu = create_fake_menu_at (info->menu_path,
					    FALSE /* applets */,
					    launcher_add,
					    info->menu_name, pixmap_file);
	}

	g_free (pixmap_file);

	return menu;
}

static GtkWidget *
create_kde_menu (GtkWidget *menu, gboolean fake_submenus,
		 gboolean force, gboolean fake,
		 gboolean launcher_add)
{
	char *pixmap_name;
	char *uri;

	pixmap_name = gnome_desktop_item_find_icon ("go.png",
						    20 /* desired_size */,
						    0 /* flags */);
	if (pixmap_name == NULL) {
		pixmap_name = g_build_filename (kde_icondir, "exec.xpm", NULL);
	}

	uri = gnome_vfs_get_uri_from_local_path (kde_menudir);

	if ( ! fake ||
	    menu != NULL) {
		menu = create_menu_at (menu, 
				       uri,
				       FALSE /* applets */,
				       launcher_add,
				       _("KDE menus"), 
				       pixmap_name,
				       fake_submenus,
				       force);
	} else {
		menu = create_fake_menu_at (uri,
					    FALSE /* applets */,
					    launcher_add,
					    _("KDE menus"),
					    pixmap_name);
	}
	g_free (pixmap_name);
	g_free (uri);

	return menu;
}

static GtkWidget *
create_add_launcher_menu (GtkWidget *menu, gboolean fake_submenus)
{
	if (menu == NULL)
		menu = menu_new ();

	/* Eeeek, a hack, if this is set then the reloading
	 * function will use create_add_launcher_menu, rather then
	 * the nomral way of reloading, as that would dump the
	 * submenus */
	g_object_set_data (G_OBJECT (menu),
			   "_add_launcher_menu_hack_",
			   GINT_TO_POINTER (TRUE));

	create_system_menu (menu,
			    fake_submenus,
			    FALSE /* fake */,
			    TRUE /* launcher_add */);

	add_menu_separator (menu);

	add_distribution_submenu (menu, fake_submenus,
				  TRUE /*launcher_add */);
	if (g_file_test (kde_menudir, G_FILE_TEST_IS_DIR)) {
		add_kde_submenu (menu, fake_submenus,
				 TRUE /*launcher_add */);
	}

	return menu;
}

static void
remove_panel (GtkWidget *widget)
{
	status_unparent (widget);
	gtk_widget_destroy (widget);
}

static void
remove_panel_accept (GtkWidget *w, int response, GtkWidget *panelw)
{
	if (response == GTK_RESPONSE_YES) {
		panel_push_window_busy (w);
		remove_panel (panelw);
		panel_pop_window_busy (w);
	}
	gtk_widget_destroy (w);
}

static void
remove_panel_query (GtkWidget *w, gpointer data)
{
	GtkWidget *dialog;
	GtkWidget *panelw;
	PanelWidget *panel;

	g_print ("-------\n");
	panel = get_panel_from_menu_data(w, TRUE);
	panelw = panel->panel_parent;

	g_print ("woo, in the remove panel query: %p!\n", panel);
	
	if (!DRAWER_IS_WIDGET (panelw) && base_panels == 1) {
		panel_error_dialog ("cannot_remove_last_panel",
				    _("You cannot remove your last panel."));
		return;
	}

	if(!global_config.confirm_panel_remove) {
		remove_panel (panelw);
		return;
	}
	dialog = gtk_message_dialog_new (NULL /* parent */,
					 0 /* flags */,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_YES_NO,
					 _("When a panel is removed, the panel "
					   "and its\napplet settings are lost. "
					   "Remove this panel?"));
	gtk_window_set_wmclass (GTK_WINDOW (dialog),
				"panel_remove_query", "Panel");

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (remove_panel_accept),
			  panelw);
	panel_signal_connect_object_while_alive (G_OBJECT (panelw), "destroy",
						 G_CALLBACK (gtk_widget_destroy),
						 G_OBJECT (dialog));
	gtk_widget_show_all (dialog);
}

static void
panel_tearoff_new_menu(GtkWidget *w, gpointer data)
{
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass();
	GtkWidget *menu = NULL;
	PanelWidget *menu_panel;

	int flags = GPOINTER_TO_INT (data);

	menu_panel = get_panel_from_menu_data (w, TRUE);

	menu = create_root_menu (NULL, TRUE, flags, FALSE,
				 BASEP_IS_WIDGET (menu_panel->panel_parent),
				 TRUE /* run_item */);

	g_object_set_data (G_OBJECT (menu), "menu_panel", menu_panel);
	panel_signal_connect_object_while_alive (G_OBJECT (menu_panel),
						 "destroy", G_CALLBACK (g_object_unref),
						 G_OBJECT (menu));

	show_tearoff_menu(menu, _("Main Menu"),TRUE,0,0,wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Main Menu"));
	tm->special = g_strdup_printf("PANEL:%d", flags);
	tm->wmclass = g_strdup(wmclass);
	g_signal_connect (G_OBJECT(menu), "destroy",
			   G_CALLBACK(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

GtkWidget *
create_panel_root_menu(PanelWidget *panel, gboolean tearoff)
{
	GtkWidget *menu;

	menu = create_root_menu (NULL, TRUE, global_config.menu_flags, tearoff,
				 BASEP_IS_WIDGET (panel->panel_parent),
				 TRUE /* run_item */);

	g_object_set_data (G_OBJECT (menu), "menu_panel", panel);

	return menu;
}

static void
current_panel_config(GtkWidget *w, gpointer data)
{
	PanelWidget *panel = get_panel_from_menu_data(w, TRUE);
	GtkWidget *parent = panel->panel_parent;
	panel_config(parent);
}

static void
ask_about_launcher_cb(GtkWidget *w, gpointer data)
{
	ask_about_launcher(NULL, get_panel_from_menu_data(w, TRUE), 0, FALSE);
}

static void
ask_about_swallowing_cb(GtkWidget *w, gpointer data)
{
	ask_about_swallowing(get_panel_from_menu_data(w, TRUE), 0, FALSE);
}

#ifdef FIXME
/* FIXME: This needs to be in the properties */
static void
convert_setup (BasePWidget *basep, GType type)
{
	basep->pos = gtk_type_new (type);
	basep->pos->basep = basep;
	basep_widget_pre_convert_hook (basep);
	basep_pos_connect_signals (basep);
	update_config_type (basep);
}
#endif

#ifdef FIXME
/* FIXME: This needs to be in the properties */
static void
convert_to_panel(GtkWidget *widget, gpointer data)
{
	PanelType type = GPOINTER_TO_INT(data);
	PanelData *pd;
	int x, y;
	int w, h;
	BasePWidget *basep;
	BasePPos *old_pos;
	PanelWidget *cur_panel = get_panel_from_menu_data(widget, TRUE);

	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(PANEL_IS_WIDGET(cur_panel));

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	g_assert (cur_panel->panel_parent);
	g_return_if_fail (BASEP_IS_WIDGET (cur_panel->panel_parent));

	basep = BASEP_WIDGET(cur_panel->panel_parent);

	pd = g_object_get_data (G_OBJECT (basep), "PanelData");
	if (pd->type == type)
		return;

	basep_widget_get_pos (basep, &x, &y);
	basep_widget_get_size (basep, &w, &h);

	old_pos = basep->pos;
	old_pos->basep = NULL;
	pd->type = type;

	/* for now, just ignore non-border types */
	switch (type) {
	case EDGE_PANEL: 
	{
		BorderEdge edge = BORDER_BOTTOM;
		convert_setup (basep, EDGE_TYPE_POS);

		if (BORDER_IS_POS (old_pos))
			edge = BORDER_POS (old_pos)->edge;
		else if (PANEL_WIDGET (cur_panel)->orient == GTK_ORIENTATION_HORIZONTAL)
			edge = (y - multiscreen_y (basep->screen) >
				(multiscreen_height (basep->screen) / 2))
				? BORDER_BOTTOM : BORDER_TOP;
		else
			edge = (x - multiscreen_x (basep->screen) >
				(multiscreen_width (basep->screen) / 2))
				? BORDER_RIGHT : BORDER_LEFT;

		border_widget_change_edge (BORDER_WIDGET (basep), edge);
		break;
	}
	case ALIGNED_PANEL: 
	{
		gint mid, max;
		BorderEdge edge = BORDER_BOTTOM;
		AlignedAlignment align;

		convert_setup (basep, ALIGNED_TYPE_POS);

		if (BORDER_IS_POS (old_pos))
			edge = BORDER_POS (old_pos)->edge;
		else if (PANEL_WIDGET (cur_panel)->orient == GTK_ORIENTATION_HORIZONTAL)
			edge = (y - multiscreen_y (basep->screen) >
				(multiscreen_height (basep->screen) / 2))
				? BORDER_BOTTOM : BORDER_TOP;
		else
			edge = (x - multiscreen_x (basep->screen) >
				(multiscreen_width (basep->screen) / 2))
				? BORDER_RIGHT : BORDER_LEFT;

		if (PANEL_WIDGET (cur_panel)->orient == GTK_ORIENTATION_HORIZONTAL) {
			mid = x + w / 2 - multiscreen_x (basep->screen);
			max = multiscreen_width (basep->screen);
		} else {
			mid = y + h / 2 - multiscreen_y (basep->screen);
			max = multiscreen_height (basep->screen);
		}
	
		if (mid < max / 3)
			align = ALIGNED_LEFT;
		else if (mid < 2 * (max / 3))
			align = ALIGNED_CENTER;
		else
			align = ALIGNED_RIGHT;
		aligned_widget_change_align_edge (
			ALIGNED_WIDGET (basep), align, edge);
		break;
	}
	case SLIDING_PANEL:
	{
		gint val, max;
		BorderEdge edge = BORDER_BOTTOM;
		SlidingAnchor anchor;
		gint16 offset;
		
		convert_setup (basep, SLIDING_TYPE_POS);
		
		if (BORDER_IS_POS (old_pos))
			edge = BORDER_POS (old_pos)->edge;
		else if (PANEL_WIDGET (cur_panel)->orient == GTK_ORIENTATION_HORIZONTAL)
			edge = (y - multiscreen_y (basep->screen) >
				(multiscreen_height (basep->screen) / 2))
				? BORDER_BOTTOM : BORDER_TOP;
		else
			edge = (x - multiscreen_x (basep->screen) >
				(multiscreen_width (basep->screen) / 2))
				? BORDER_RIGHT : BORDER_LEFT;
		
		if (PANEL_WIDGET (cur_panel)->orient == GTK_ORIENTATION_HORIZONTAL) {
			val = x - multiscreen_x (basep->screen);
			max = multiscreen_width (basep->screen);
		} else {
			val = y - multiscreen_y (basep->screen);
			max = multiscreen_height (basep->screen);
		}
		
		if (val > 0.9 * max) {
			offset = max - val;
			anchor = SLIDING_ANCHOR_RIGHT;
		} else {
			offset = val;
			anchor = SLIDING_ANCHOR_LEFT;
		}

		sliding_widget_change_anchor_offset_edge (
			SLIDING_WIDGET (basep), anchor, offset, edge);
		
		break;
	}
	case FLOATING_PANEL:
	{
		convert_setup (basep, FLOATING_TYPE_POS);
		floating_widget_change_coords (FLOATING_WIDGET (basep),
					       x, y);
		break;
	}
	default:
		g_assert_not_reached ();
		break;
	}

	g_object_unref (G_OBJECT (old_pos));
	gtk_widget_queue_resize (GTK_WIDGET (basep));
}
#endif

static void
make_add_submenu (GtkWidget *menu, gboolean fake_submenus)
{
	GtkWidget *menuitem, *submenu, *submenuitem, *m;

	/* Add Menu */

	m = create_applets_menu ();
	if (m) {
		menuitem = gtk_image_menu_item_new ();
		setup_menuitem_try_pixmap (menuitem, "gnome-applets.png",
					   _("Applet"));

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),m);
	}

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem, 
				   "gnome-gmenu.png",
				   _("Menu"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	submenu = menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);

	submenuitem = gtk_image_menu_item_new ();
	setup_menuitem_try_pixmap (submenuitem,
				   "gnome-logo-icon-transparent.png",
				   _("Main menu"));
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), submenuitem);
	g_signal_connect (G_OBJECT(submenuitem), "activate",
			   G_CALLBACK(add_menu_to_panel),
			   NULL);
	setup_internal_applet_drag(submenuitem, "MENU:MAIN");

	submenuitem = gtk_image_menu_item_new ();
	setup_menuitem_try_pixmap (submenuitem, 
				   "gnome-logo-icon-transparent.png",
				   _("Programs menu"));
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), submenuitem);
	g_signal_connect (G_OBJECT(submenuitem), "activate",
			   G_CALLBACK(add_menu_to_panel),
			   "applications:/");
	setup_internal_applet_drag(submenuitem, "MENU:applications:/");

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem, 
				   "launcher-program.png",
				   _("Launcher..."));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(ask_about_launcher_cb),NULL);
	setup_internal_applet_drag(menuitem, "LAUNCHER:ASK");

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem, "launcher-program.png",
				   _("Launcher from menu"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	m = create_add_launcher_menu (NULL, fake_submenus);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), m);
	g_signal_connect (G_OBJECT (m),"show",
			  G_CALLBACK (submenu_to_display), NULL);

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem, 
				   "panel-drawer.png",
				   _("Drawer"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_drawer_to_panel,
			   NULL);
	setup_internal_applet_drag(menuitem, "DRAWER:NEW");

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem,
				   "gnome-term-night.png",
				   _("Log out button"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(add_logout_to_panel),
			   NULL);
	setup_internal_applet_drag(menuitem, "LOGOUT:NEW");
	
	menuitem = gtk_image_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem, 
				   "gnome-lockscreen.png",
				   _("Lock button"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(add_lock_to_panel),
			   NULL);
	setup_internal_applet_drag(menuitem, "LOCK:NEW");

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem, 
			gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU),
			_("Swallowed app..."));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(ask_about_swallowing_cb),NULL);
	setup_internal_applet_drag(menuitem, "SWALLOW:ASK");

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem(menuitem, NULL, _("Status dock"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(try_add_status_to_panel),NULL);
	setup_internal_applet_drag(menuitem, "STATUS:TRY");
}

static void
add_to_panel_menu_tearoff_new_menu(GtkWidget *w, gpointer data)
{
	GtkWidget *menu;
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass();
	PanelWidget *menu_panel;

	menu = menu_new();
	make_add_submenu(menu, TRUE);
	
	/*set the panel to use as the data*/
	menu_panel = get_panel_from_menu_data(w, TRUE);
	g_object_set_data (G_OBJECT (menu), "menu_panel", menu_panel);
	panel_signal_connect_object_while_alive (G_OBJECT(menu_panel),
						 "destroy", G_CALLBACK(g_object_unref),
						 G_OBJECT(menu));
	show_tearoff_menu(menu, _("Add to panel"),TRUE,0,0, wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Add to panel"));
	tm->special = g_strdup("ADD_TO_PANEL");
	tm->wmclass = g_strdup(wmclass);
	g_signal_connect (G_OBJECT(menu), "destroy",
			   G_CALLBACK(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

/* just run the gnome-panel-properties */
static void
panel_config_global(void)
{
	char *argv[2] = {"gnome-panel-properties", NULL};
	if (gnome_execute_async (g_get_home_dir (), 1, argv) < 0)
		panel_error_dialog("cannot_exec_global_props",
				   _("Cannot execute panel global properties"));
}

static void
setup_remove_this_panel(GtkWidget *menu, GtkWidget *menuitem)
{
	PanelWidget *panel = get_panel_from_menu_data(menu, TRUE);
	GtkWidget *label;

	g_print ("woo, setting up remove this panel: %p\n", panel);
	
	g_assert(panel->panel_parent);

	if(!GTK_MENU(menu)->torn_off &&
	   !DRAWER_IS_WIDGET(panel->panel_parent) &&
	   base_panels == 1)
		gtk_widget_set_sensitive(menuitem, FALSE);
	else
		gtk_widget_set_sensitive(menuitem, TRUE);

	label = GTK_BIN(menuitem)->child;
	if(GTK_IS_BOX(label)) {
		GList *li, *list;
		list = gtk_container_get_children(GTK_CONTAINER(label));
		for(li = list; li; li = li->next) {
			if(GTK_IS_LABEL(li->data)) {
				label = li->data;
				break;
			}
		}
		g_list_free(list);
	}
	if(!GTK_IS_LABEL(label)) {
		g_warning("We can't find the label of a menu item");
		return;
	}

	/* this will not handle the case of menu being torn off
	 * and then the confirm_panel_remove changed, but oh well */
	if((GTK_MENU(menu)->torn_off || panel->applet_list) &&
	   global_config.confirm_panel_remove)
		gtk_label_set_text(GTK_LABEL(label), _("Remove this panel..."));
	else
		gtk_label_set_text(GTK_LABEL(label), _("Remove this panel"));
}

static void
show_panel_help (GtkWidget *w, gpointer data)
{
	panel_show_help ("index", NULL);
}

static void
panel_launch_nautilus (GtkWidget *widget, gpointer data)
{
	char *argv[3] = {"nautilus", data, NULL};
	if (gnome_execute_async (g_get_home_dir (), 2, argv) < 0)
		   panel_error_dialog ("cannot_launch_nautilus",
				       _("Cannot launch nautilus!"));
}

void
make_panel_submenu (GtkWidget *menu, gboolean fake_submenus, gboolean is_basep)
{
	GtkWidget *menuitem, *submenu;

	if ( ! commie_mode) {
		menuitem = gtk_image_menu_item_new ();
		setup_menuitem (menuitem,
				gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU),
				_("Add to panel"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

		submenu = menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
					   submenu);

		menu_add_tearoff (GTK_MENU (submenu),
				  G_CALLBACK(add_to_panel_menu_tearoff_new_menu),
				  NULL);

		make_add_submenu (submenu, fake_submenus);

		menuitem = gtk_image_menu_item_new ();
		setup_menuitem (menuitem, 
				gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU),
				_("Create panel"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
					   create_add_panel_submenu(TRUE));

		menuitem = gtk_image_menu_item_new ();

		setup_menuitem (menuitem, 
				gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU),
				_("Remove this panel"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
		g_signal_connect (G_OBJECT (menuitem), "activate",
				    G_CALLBACK (remove_panel_query),
				    NULL);
		g_signal_connect (G_OBJECT (menu), "show",
				    G_CALLBACK(setup_remove_this_panel),
				    menuitem);

		add_menu_separator(menu);

		if (is_basep) {
			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem,
					gtk_image_new_from_stock (GTK_STOCK_PROPERTIES,
								  GTK_ICON_SIZE_MENU),
					_("Properties..."));

			gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
			g_signal_connect (G_OBJECT (menuitem), "activate",
					    G_CALLBACK (current_panel_config), 
					    NULL);
		}

		menuitem = gtk_image_menu_item_new ();
		setup_menuitem (menuitem,
				gtk_image_new_from_stock (GTK_STOCK_PREFERENCES,
							  GTK_ICON_SIZE_MENU),
				_("Global Preferences..."));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
		g_signal_connect (G_OBJECT (menuitem), "activate",
				    G_CALLBACK(panel_config_global), 
				    NULL);

		menuitem = gtk_image_menu_item_new ();
		setup_menuitem_try_pixmap (menuitem,
					   "gnome-gmenu.png",
					   _("Edit menus..."));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
		g_signal_connect (G_OBJECT (menuitem), "activate",
				  G_CALLBACK (panel_launch_nautilus), 
				  "applications:/");

		if ( ! global_config.menu_check) {
			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem,
					NULL,
					_("Reread all menus"));
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
			g_signal_connect (G_OBJECT (menuitem), "activate",
					    G_CALLBACK (fr_force_reread), 
					    NULL);
		}

		add_menu_separator (menu);
	}

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			gtk_image_new_from_stock (GTK_STOCK_HELP,
						  GTK_ICON_SIZE_MENU),
			_("Panel Manual..."));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT (menuitem), "activate",
			    G_CALLBACK (show_panel_help), NULL);
}

void
panel_lock (GtkWidget *widget, gpointer data)
{
	char *argv[3] = {"xscreensaver-command", "-lock", NULL};
	if (gnome_execute_async (g_get_home_dir (), 2, argv) < 0)
		panel_error_dialog ("cannot_exec_xscreensaver",
				    _("Cannot execute xscreensaver"));
}

static void
panel_menu_tearoff_new_menu(GtkWidget *w, gpointer data)
{
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass();
	PanelWidget *menu_panel = get_panel_from_menu_data(w, TRUE);
	GtkWidget *menu = create_panel_submenu (
		NULL, TRUE, FALSE, BASEP_IS_WIDGET (menu_panel->panel_parent));
		
	/*set the panel to use as the data*/
	g_object_set_data (G_OBJECT(menu), "menu_panel", menu_panel);
	panel_signal_connect_object_while_alive (G_OBJECT(menu_panel),
						 "destroy", G_CALLBACK(g_object_unref),
						 G_OBJECT(menu));

	show_tearoff_menu (menu, _("Panel"), TRUE, 0, 0, wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Panel"));
	tm->special = g_strdup("PANEL_SUBMENU");
	tm->wmclass = g_strdup(wmclass);
	g_signal_connect (G_OBJECT(menu), "destroy",
			   G_CALLBACK(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

static void
desktop_menu_tearoff_new_menu (GtkWidget *w, gpointer data)
{
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass ();
	PanelWidget *menu_panel;
	GtkWidget *menu = create_desktop_menu (NULL, TRUE, FALSE);

	/*set the panel to use as the data*/
	menu_panel = get_panel_from_menu_data(w, TRUE);
	g_object_set_data (G_OBJECT (menu), "menu_panel", menu_panel);
	panel_signal_connect_object_while_alive (G_OBJECT(menu_panel),
						 "destroy", G_CALLBACK(g_object_unref),
						 G_OBJECT(menu));

	show_tearoff_menu (menu, _("Desktop"), TRUE, 0, 0, wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Desktop"));
	tm->special = g_strdup("DESKTOP");
	tm->wmclass = g_strdup(wmclass);
	g_signal_connect (G_OBJECT(menu), "destroy",
			   G_CALLBACK(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

static GtkWidget *
create_panel_submenu(GtkWidget *menu, gboolean fake_submenus, gboolean tearoff, 
		     gboolean is_basep)
{
	GtkWidget *menuitem;
	char *char_tmp;

	if (!menu) {
		menu = menu_new();
	}

	menu_add_tearoff (GTK_MENU (menu),
			  G_CALLBACK (panel_menu_tearoff_new_menu),
			  NULL);

	make_panel_submenu (menu, fake_submenus, is_basep);

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			gtk_image_new_from_stock (GNOME_STOCK_ABOUT, GTK_ICON_SIZE_MENU),
			_("About the panel..."));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT (menuitem), "activate",
			    G_CALLBACK(about_cb),
			    NULL);
	
	char_tmp = g_find_program_in_path ("gnome-about");
	if(!char_tmp)
		char_tmp = g_find_program_in_path ("guname");

	if (char_tmp) {
		menuitem = gtk_image_menu_item_new ();
		setup_menuitem (menuitem,
				gtk_image_new_from_stock (GNOME_STOCK_ABOUT, GTK_ICON_SIZE_MENU),
				_("About GNOME..."));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
		g_signal_connect_data (G_OBJECT (menuitem), "activate",
				       G_CALLBACK (about_gnome_cb),
				       char_tmp,
				       (GClosureNotify)g_free,
				       G_CONNECT_AFTER);
	}
	return menu;
}

static void
run_cb (GtkWidget *w, gpointer data)
{
	show_run_dialog ();
}

static GtkWidget *
create_desktop_menu (GtkWidget *menu, gboolean fake_submenus, gboolean tearoff)
{
	GtkWidget *menuitem;
	/* Panel entry */

	if (menu == NULL) {
		menu = menu_new ();
	}

	if (tearoff)
		menu_add_tearoff (GTK_MENU (menu),
				  G_CALLBACK (desktop_menu_tearoff_new_menu),
				  NULL);

	if (panel_is_program_in_path ("xscreensaver")) {
		menuitem = gtk_image_menu_item_new ();
		setup_menuitem_try_pixmap (menuitem,
					   "gnome-lockscreen.png",
					   _("Lock screen"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
		g_signal_connect (G_OBJECT (menuitem), "activate",
				    G_CALLBACK(panel_lock), 0);
		setup_internal_applet_drag(menuitem, "LOCK:NEW");
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      _("Lock the screen so that you can "
					"temporairly leave your computer"), NULL);
	}

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem,
				   "gnome-term-night.png",
				   _("Log out"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT (menuitem), "activate",
			    G_CALLBACK(panel_quit), 0);
	setup_internal_applet_drag(menuitem, "LOGOUT:NEW");
	gtk_tooltips_set_tip (panel_tooltips, menuitem,
			      _("Log out of this session to log in as "
				"a different user or to shut down your "
				"computer"),
			      NULL);

	return menu;
}

static void
add_distribution_submenu (GtkWidget *root_menu, gboolean fake_submenus,
			  gboolean launcher_add)
{
	GtkWidget *menu;
	GtkWidget *menuitem;
	const DistributionInfo *distribution_info = get_distribution_info ();

	if (distribution_info == NULL)
		return;

	menu = create_distribution_menu (NULL /* menu */,
					 fake_submenus,
					 TRUE /* fake */,
					 launcher_add);
	menuitem = gtk_image_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem,
				   distribution_info->menu_icon,
				   _(distribution_info->menu_name));
	gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   menu);
	if (distribution_info->menu_show_func)
		g_signal_connect (G_OBJECT(menu),"show",
				   G_CALLBACK(distribution_info->menu_show_func),
				   menuitem);
	g_signal_connect (G_OBJECT(menu),"show",
			  G_CALLBACK(submenu_to_display),
			  NULL);
}

static void
add_kde_submenu (GtkWidget *root_menu, gboolean fake_submenus,
		 gboolean launcher_add)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = create_kde_menu (NULL, fake_submenus, TRUE,
				TRUE, launcher_add);

	menuitem = gtk_image_menu_item_new ();

	panel_load_menu_image_deferred (menuitem, "go.png", "exec.xpm",
					FALSE /* force_image */);

	setup_menuitem (menuitem, NULL, _("KDE menus"));
	gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	g_signal_connect (G_OBJECT(menu),"show",
			  G_CALLBACK(submenu_to_display), NULL);
}

GtkWidget *
create_root_menu (GtkWidget *root_menu,
		  gboolean fake_submenus,
		  int flags,
		  gboolean tearoff,
		  gboolean is_basep,
		  gboolean run_item)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	gboolean has_inline = (flags & (MAIN_MENU_SYSTEM |
					MAIN_MENU_APPLETS |
					MAIN_MENU_KDE));

	gboolean has_subs = (flags & (MAIN_MENU_SYSTEM_SUB |
				      MAIN_MENU_APPLETS_SUB |
				      MAIN_MENU_KDE_SUB));

	gboolean has_inline2 = (flags & (MAIN_MENU_DESKTOP |
					 MAIN_MENU_PANEL));
	gboolean has_subs2 = (flags & (MAIN_MENU_DESKTOP_SUB |
				       MAIN_MENU_PANEL_SUB));

	const DistributionInfo *distribution_info = get_distribution_info ();

	if (distribution_info != NULL) {
		has_inline |= (flags & (MAIN_MENU_DISTRIBUTION));
		has_subs |= (flags & (MAIN_MENU_DISTRIBUTION_SUB));
	}

	if(!root_menu)
		root_menu = menu_new ();

	if (tearoff)
		menu_add_tearoff (GTK_MENU (root_menu),
				  G_CALLBACK (panel_tearoff_new_menu),
				  GINT_TO_POINTER(flags));

	if (flags & MAIN_MENU_SYSTEM)
		create_system_menu(root_menu, fake_submenus,
				   FALSE /* fake */,
				   FALSE /* launcher_add */);
	/* in commie mode the applets menu doesn't make sense */
	if ( ! commie_mode &&
	    flags & MAIN_MENU_APPLETS) {
		/* FIXME: this doesn't work! */
		/*create_applets_menu (); */
	}

	if (flags & MAIN_MENU_DISTRIBUTION &&
	    distribution_info != NULL) {
		if (distribution_info->menu_show_func)
			distribution_info->menu_show_func(NULL,NULL);

		create_distribution_menu(root_menu, fake_submenus, FALSE,
					 FALSE /* launcher_add */);
	}
	if (flags & MAIN_MENU_KDE)
		create_kde_menu(root_menu, fake_submenus, FALSE, FALSE,
				FALSE /* launcher_add */);

	/*others here*/

	if (has_subs && has_inline)
		add_menu_separator (root_menu);

	
	if (flags & MAIN_MENU_SYSTEM_SUB) {
		menu = create_system_menu(NULL, fake_submenus, TRUE,
					  FALSE /* launcher_add */);
		menuitem = gtk_image_menu_item_new ();
		setup_menuitem_try_pixmap (menuitem,
					   "gnome-logo-icon-transparent.png",
					   _("Programs"));
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		if(menu) {
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
						   menu);
			g_signal_connect (G_OBJECT (menu),"show",
					  G_CALLBACK (submenu_to_display),
					  NULL);
		}
	}

	/* in commie mode the applets menu doesn't make sense */
	if (!commie_mode && (flags & MAIN_MENU_APPLETS_SUB)) {
		menu = create_applets_menu ();
		if (menu) {
			menuitem = gtk_image_menu_item_new ();
			setup_menuitem_try_pixmap (menuitem, "gnome-applets.png",
			                           _("Applets"));
			gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		}
	}
	if (flags & MAIN_MENU_DISTRIBUTION_SUB) {
		add_distribution_submenu (root_menu, fake_submenus,
					  FALSE /*launcher_add */);
	}
	if (flags & MAIN_MENU_KDE_SUB) {
		add_kde_submenu (root_menu, fake_submenus,
				 FALSE /*launcher_add */);
	}

	if ( ! no_run_box && run_item) {
		menuitem = gtk_image_menu_item_new ();
		setup_menuitem_try_pixmap (menuitem, "gnome-run.png", 
					   _("Run..."));
		g_signal_connect (G_OBJECT (menuitem), "activate",
				    G_CALLBACK (run_cb), NULL);
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		setup_internal_applet_drag(menuitem, "RUN:NEW");
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      _("Execute a command line"), NULL);
	}

	if (((has_inline && !has_subs) || has_subs) && has_subs2)
		add_menu_separator (root_menu);

	if (flags & MAIN_MENU_PANEL_SUB) {
		menu = create_panel_submenu (NULL, fake_submenus, TRUE, is_basep);
		menuitem = gtk_image_menu_item_new ();
		setup_menuitem_try_pixmap (menuitem, "gnome-panel.png", 
					   _("Panel"));
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	}
	if (flags & MAIN_MENU_DESKTOP_SUB) {
		menu = create_desktop_menu (NULL, fake_submenus, TRUE);
		menuitem = gtk_image_menu_item_new ();
		setup_menuitem_try_pixmap (menuitem, "gnome-ccdesktop.png",
					   _("Desktop"));
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	}

	if (!has_inline2)
		return root_menu;

	if (has_subs2 || has_subs || has_inline)
		add_menu_separator (root_menu);

	if (flags & MAIN_MENU_PANEL) {
		make_panel_submenu (root_menu, fake_submenus, is_basep);
	}
	
	if (flags & MAIN_MENU_DESKTOP) {
		if (flags & MAIN_MENU_PANEL)
			add_menu_separator (root_menu);
		create_desktop_menu (root_menu, fake_submenus, FALSE);
	}
	
	return root_menu;
}
	
void
add_menu_widget (Menu *menu,
		 PanelWidget *panel,
		 GSList *menudirl,
		 gboolean fake_subs)
{
	GSList *li;

	/* one of these has to be there in order to get the panel of the
	   applet */
	g_return_if_fail (menu->menu || panel);

	if(menu->menu) {
		panel = get_panel_from_menu_data(menu->menu, TRUE);
		g_object_unref (G_OBJECT (menu->menu));
		menu->menu = NULL;
	}

	if(panel == NULL) {
		g_warning ("Menu is seriously weird");
		return;
	}

	if (menu->main_menu) {
		int flags;
		if (menu->global_main)
			flags = global_config.menu_flags;
		else
			flags = menu->main_menu_flags;
		menu->menu = create_root_menu(NULL,
			fake_subs, flags, TRUE,
			BASEP_IS_WIDGET (panel->panel_parent),
			TRUE /* run_item */);

		gtk_tooltips_set_tip (panel_tooltips, menu->button,
				      _("Main Menu"), NULL);
	} else {
		menu->menu = NULL;
		for(li = menudirl; li != NULL; li = li->next)
			menu->menu = create_menu_at (menu->menu, li->data,
						     FALSE /* applets */,
						     FALSE /* launcher_add */,
						     NULL, NULL,
						     fake_subs, FALSE);

		/* FIXME: A more descriptive name would be better */
		gtk_tooltips_set_tip (panel_tooltips, menu->button,
				      _("Menu"), NULL);

		if(menu->menu == NULL) {
			int flags;
			if (menu->global_main)
				flags = global_config.menu_flags;
			else
				flags = menu->main_menu_flags;
			g_warning(_("Can't create menu, using main menu!"));
			menu->menu = create_root_menu(NULL,
				fake_subs, flags, TRUE,
				BASEP_IS_WIDGET (panel->panel_parent),
				TRUE /* run_item */);
			gtk_tooltips_set_tip (panel_tooltips, menu->button,
					      _("Main Menu"), NULL);
		}

	}
	g_signal_connect (G_OBJECT (menu->menu), "deactivate",
			    G_CALLBACK (menu_deactivate), menu);

	g_object_set_data (G_OBJECT (menu->menu), "menu_panel", panel);
	panel_signal_connect_object_while_alive(
	      G_OBJECT (panel),
	      "destroy", G_CALLBACK(g_object_unref),
	      G_OBJECT (menu->menu));
}

static void
menu_button_pressed (GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	GdkEventButton *bevent = (GdkEventButton*)gtk_get_current_event();
	GtkWidget *wpanel = get_panel_parent(menu->button);
	int flags;
	const DistributionInfo *distribution_info = get_distribution_info ();

	if (menu->global_main)
		flags = global_config.menu_flags;
	else
		flags = menu->main_menu_flags;

	if (menu->menu == NULL) {
		char *this_menu = get_real_menu_path (menu->path, menu->main_menu);
		GSList *list = g_slist_append (NULL, this_menu);
		
		add_menu_widget (menu, PANEL_WIDGET(menu->button->parent),
				 list, TRUE);
		
		g_slist_free (list);

		g_free (this_menu);
	} else {
		if(flags & MAIN_MENU_DISTRIBUTION &&
		   ! (flags & MAIN_MENU_DISTRIBUTION_SUB) &&
		   distribution_info != NULL &&
		   distribution_info->menu_show_func)
			distribution_info->menu_show_func(NULL, NULL);

		check_and_reread_applet (menu);
	}

	/*so that the panel doesn't pop down until we're
	  done with the menu */
	if(BASEP_IS_WIDGET(wpanel)) {
		BASEP_WIDGET(wpanel)->autohide_inhibit = TRUE;
		basep_widget_autohide(BASEP_WIDGET(wpanel));
	}

	BUTTON_WIDGET(menu->button)->ignore_leave = TRUE;
	gtk_grab_remove(menu->button);

	menu->age = 0;

	gtk_menu_popup (GTK_MENU (menu->menu),
			NULL,
			NULL, 
			applet_menu_position,
			menu->info,
			bevent->button,
			bevent->time);

	gdk_event_free((GdkEvent *)bevent);
}

static void  
drag_data_get_cb (GtkWidget          *widget,
		  GdkDragContext     *context,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint               time,
		  gpointer            data)
{
	char *foo;

	foo = g_strdup_printf ("MENU:%d", find_applet (widget));

	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)foo,
				strlen (foo));

	g_free (foo);
}

static Menu *
create_panel_menu (PanelWidget *panel, const char *menudir, gboolean main_menu,
		   PanelOrient orient, int main_menu_flags,
		   gboolean global_main,
		   gboolean custom_icon, const char *custom_icon_file)
{
        static GtkTargetEntry dnd_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};
	Menu *menu;
	
	char *pixmap_name;

	menu = g_new0 (Menu, 1);

	menu->main_menu = main_menu;

	menu->path = g_strdup (menudir);


	menu->custom_icon = custom_icon;
	if ( ! string_empty (custom_icon_file))
		menu->custom_icon_file = g_strdup (custom_icon_file);
	else
		menu->custom_icon_file = NULL;

	if (menu->custom_icon &&
	    menu->custom_icon_file != NULL &&
	    g_file_test (menu->custom_icon_file, G_FILE_TEST_EXISTS))
		pixmap_name = g_strdup (menu->custom_icon_file);
	else
		pixmap_name = get_pixmap (menudir, main_menu);

	menu->main_menu_flags = main_menu_flags;
	menu->global_main = global_main;

	/*make the pixmap*/
	menu->button = button_widget_new (pixmap_name, -1, MENU_POBJECT,
					  TRUE, orient, _("Menu"));

	/*A hack since this function only pretends to work on window
	  widgets (which we actually kind of are) this will select
	  some (already selected) events on the panel instead of
	  the button window (where they are also selected) but
	  we don't mind*/
	GTK_WIDGET_UNSET_FLAGS (menu->button, GTK_NO_WINDOW);
	gtk_drag_source_set (menu->button,
			     GDK_BUTTON1_MASK,
			     dnd_targets, 1,
			     GDK_ACTION_MOVE);
	GTK_WIDGET_SET_FLAGS (menu->button, GTK_NO_WINDOW);

	if (main_menu)
		gtk_tooltips_set_tip (panel_tooltips, menu->button,
				      _("Main Menu"), NULL);
	else
		/* FIXME: A more descriptive name would be better */
		gtk_tooltips_set_tip (panel_tooltips, menu->button,
				      _("Menu"), NULL);

	g_signal_connect (G_OBJECT (menu->button), "drag_data_get",
			    G_CALLBACK (drag_data_get_cb),
			    NULL);

	g_signal_connect_after (G_OBJECT (menu->button), "pressed",
				G_CALLBACK (menu_button_pressed), menu);
	g_signal_connect (G_OBJECT (menu->button), "destroy",
			    G_CALLBACK (destroy_menu), menu);
	gtk_widget_show(menu->button);

	/*if we are allowed to be pigs and load all the menus to increase
	  speed, load them*/
	if(global_config.keep_menus_in_memory) {
		GSList *list = g_slist_append(NULL, (gpointer)menudir);
		add_menu_widget (menu, panel, list, TRUE);
		g_slist_free(list);
	}

	g_free (pixmap_name);

	return menu;
}

static Menu *
create_menu_applet (PanelWidget *panel,
		    const char *path,
		    PanelOrient orient,
		    gboolean main_menu,
		    int main_menu_flags,
		    gboolean global_main,
		    gboolean custom_icon, const char *custom_icon_file)
{
	Menu *menu;

	char *this_menu = get_real_menu_path (path, main_menu);

	if (this_menu == NULL)
		return NULL;

	if(gnome_folder == NULL)
		gnome_folder = gnome_program_locate_file (NULL, 
							  GNOME_FILE_DOMAIN_PIXMAP, 
							  "gnome-folder.png", 
							  TRUE, NULL);

	menu = create_panel_menu (panel, this_menu, main_menu,
				  orient, main_menu_flags, global_main,
				  custom_icon, custom_icon_file);

	g_free (this_menu);
	return menu;
}

void
set_menu_applet_orient(Menu *menu, PanelOrient orient)
{
	g_return_if_fail(menu!=NULL);

	button_widget_set_params(BUTTON_WIDGET(menu->button),
				 MENU_POBJECT, TRUE, orient);
}

void
load_menu_applet (const char  *params,
		  gboolean     main_menu,
		  int          main_menu_flags,
		  gboolean     global_main,
		  gboolean     custom_icon,
		  const char  *custom_icon_file,
		  PanelWidget *panel,
		  int          pos,
		  gboolean     exactpos,
		  const char  *gconf_key)
{
	Menu *menu;

	menu = create_menu_applet (panel, params, PANEL_ORIENT_UP,
				   main_menu, main_menu_flags, global_main,
				   custom_icon, custom_icon_file);

	if (menu != NULL) {
		AppletInfo *info;

		info = panel_applet_register (menu->button, menu, free_menu, panel, 
					      pos, exactpos, APPLET_MENU, gconf_key);
		if (!info)
			return;

		menu->info = info;

		if (!commie_mode) {
			panel_applet_add_callback (info, 
						   "properties",
						   GTK_STOCK_PROPERTIES,
						   _("Properties..."));

			if (menu->main_menu) {
				panel_applet_add_callback (info,
							   "edit_menus",
							    NULL,
							    _("Edit menus..."));
			}
		}

		panel_applet_add_callback (info, "help", GTK_STOCK_HELP, _("Help"));
	}
}

static void
image_menu_shown (GtkWidget *image, gpointer data)
{
	IconToLoad *icon = data;

	/* if we've already handled this */
	if (gtk_image_get_storage_type (GTK_IMAGE (image)) != GTK_IMAGE_EMPTY)
		return;

	icons_to_load = g_list_append (icons_to_load,
				       icon_to_load_copy (icon));
	if (load_icons_id == 0)
		load_icons_id = g_idle_add (load_icons_handler, NULL);
}


void
panel_load_menu_image_deferred (GtkWidget *image_menu_item,
				const char *image_filename,
				const char *fallback_image_filename,
				gboolean force_image)
{
  IconToLoad *icon;
  GtkWidget *image;

  icon = g_new (IconToLoad, 1);

  image = gtk_image_new ();
  image->requisition.width = ICON_SIZE;
  image->requisition.height = ICON_SIZE;

  /* this takes over the floating ref */
  icon->pixmap = g_object_ref (G_OBJECT (image));
  gtk_object_sink (GTK_OBJECT (image));

  icon->image = g_strdup (image_filename);
  icon->fallback_image = g_strdup (fallback_image_filename);
  icon->force_image = force_image;

  gtk_widget_show (image);

  g_object_set_data_full (G_OBJECT (image_menu_item),
			  "Panel:Image",
			  g_object_ref (G_OBJECT (image)),
			  (GDestroyNotify) g_free);
  if (force_image)
	  g_object_set_data (G_OBJECT (image_menu_item),
			     "Panel:ForceImage",
			     GINT_TO_POINTER (TRUE));
 
  if (force_image || panel_menu_have_icons ())
	  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (image_menu_item), image);

  g_signal_connect_data (G_OBJECT (image), "map",
			 G_CALLBACK (image_menu_shown),
			 icon,
			 (GClosureNotify) icon_to_load_free,
			 0 /* connect_flags */);
}

#ifdef FIXME
/* FIXME: We need to gconfize all the tearoff stuff still
 */ 

void
panel_menu_session_save_tornoffs (void)
{
	GSList *li;
	int i;

	gnome_config_push_prefix (PANEL_CONFIG_PATH "panel/Config/");

	gnome_config_set_int("tearoffs_count",g_slist_length(tearoffs));

	gnome_config_pop_prefix ();

	for(i = 0, li = tearoffs;
	    li != NULL;
	    i++, li = li->next) {
		TearoffMenu *tm = li->data;
		int x = 0, y = 0;
		GtkWidget *tw;
		int menu_panel = 0;
		gchar *menu_panel_id = NULL;
		PanelWidget *menu_panel_widget = NULL;
		GSList *l;
		char *s;
		int j;

		s = g_strdup_printf ("%spanel/TornoffMenu_%d/",
				     PANEL_CONFIG_PATH, i);
		gnome_config_push_prefix (s);
		g_free (s);

		tw = GTK_MENU(tm->menu)->tearoff_window;

		if (tw != NULL &&
		    tw->window != NULL) {
			gdk_window_get_root_origin (tw->window, &x, &y);
			/* unfortunately we must do this or set_uposition
			   will crap out */
			if (x < 0)
				x = 0;
			if (y < 0)
				y = 0;
		}

		gnome_config_set_string ("title", tm->title);
		gnome_config_set_string ("wmclass", tm->wmclass);
		gnome_config_set_int ("x", x);
		gnome_config_set_int ("y", y);

		menu_panel_widget = g_object_get_data (G_OBJECT(tm->menu),
						       "menu_panel");
		menu_panel = g_slist_index(panels, menu_panel_widget);
		
		if (menu_panel_widget != NULL)
			menu_panel_id = menu_panel_widget->unique_id;

		gnome_config_set_int("menu_panel", menu_panel);
		gnome_config_set_int("menu_unique_panel_id", menu_panel_id); 

		gnome_config_set_string("special",
					sure_string (tm->special));

		gnome_config_set_int("mfl_count", g_slist_length(tm->mfl));

		for(j=0,l=tm->mfl;l;j++,l=l->next) {
			MenuFinfo *mf = l->data;
			char name[256];
			g_snprintf(name, sizeof (name), "name_%d", j);
			gnome_config_set_string(name, mf->menudir);
			g_snprintf(name, sizeof (name), "dir_name_%d", j);
			gnome_config_set_string(name, mf->dir_name);
			g_snprintf(name, sizeof (name), "pixmap_name_%d", j);
			gnome_config_set_string(name, mf->pixmap_name);
			g_snprintf(name, sizeof (name), "applets_%d", j);
			gnome_config_set_bool(name, mf->applets);
			g_snprintf(name, sizeof (name), "launcher_add_%d", j);
			gnome_config_set_bool(name, mf->launcher_add);
		}

		gnome_config_pop_prefix();
	}
}

static GtkWidget *
create_special_menu (const char *special, PanelWidget *menu_panel_widget)
{
	GtkWidget *menu = NULL;

	if(strcmp(special, "ADD_PANEL")==0) {
		menu = create_add_panel_submenu(FALSE);
	} else if(strncmp(special, "PANEL", strlen("PANEL"))==0) {
		int flags;
		if(sscanf(special, "PANEL:%d", &flags) != 1)
			flags = global_config.menu_flags;
		menu = create_root_menu (NULL, TRUE, flags, FALSE,
					 BASEP_IS_WIDGET (menu_panel_widget->panel_parent),
					 TRUE /* run_item */);
	} else if(strcmp(special, "DESKTOP")==0) {
		menu = create_desktop_menu (NULL, TRUE, FALSE);
	} else if(strcmp(special, "ADD_TO_PANEL")==0) {
		menu = menu_new();
		make_add_submenu(menu, TRUE);
	} else if(strcmp(special, "PANEL_SUBMENU")==0) {
		menu = create_panel_submenu (
			NULL, TRUE, FALSE,
			BASEP_IS_WIDGET (menu_panel_widget->panel_parent));

	}

	return menu;
}

static void
load_tearoff_menu(void)
{
	GtkWidget *menu;
	char *title, *wmclass, *special;
	int x, y, i;
	int mfl_count;
	TearoffMenu *tm;
	gulong wmclass_num;
	PanelWidget *menu_panel_widget = NULL;


	if ( ! conditional_true ("Conditional"))
		return;

	title = conditional_get_string ("title", NULL, NULL);
	wmclass = conditional_get_string ("wmclass", NULL, NULL);

	if(string_empty (title) ||
	   string_empty (wmclass)) {
		g_free(title);
		g_free(wmclass);
		return;
	}

	x = conditional_get_int ("x", 0, NULL);
	y = conditional_get_int ("y", 0, NULL);

	/* FIXME : FUCK sake..too many ids */
	i = conditional_get_int("menu_panel_id", -1, NULL);
	if (i < 0) {
		i = conditional_get_int("menu_panel", 0, NULL);
		if (i < 0)
			i = 0;
		menu_panel_widget = g_slist_nth_data(panels, i);
	} else {
		/* FIXME : menu_panel_widget = panel_widget_get_by_id (i); */
	}
	if (menu_panel_widget == NULL)
		menu_panel_widget = panels->data;
	if ( ! PANEL_IS_WIDGET(menu_panel_widget))
		g_warning("panels list is on crack");

	mfl_count = conditional_get_int("mfl_count", 0, NULL);

	special = conditional_get_string("special", NULL, NULL);
	if (string_empty (special)) {
		g_free (special);
		special = NULL;
	}

	/* find out the wmclass_number that was used
	   for this wmclass and make our default one 1 higher
	   so that we will always get unique wmclasses */
	wmclass_num = 0;
	sscanf (wmclass, "panel_tearoff_%lu", &wmclass_num);
	if (wmclass_num >= wmclass_number)
		wmclass_number = wmclass_num+1;

	menu = NULL;

	if (special != NULL) {
		menu = create_special_menu (special, menu_panel_widget);
	} else {
		for(i = 0; i < mfl_count; i++) {
			char propname[256];
			char *name;
			gboolean applets;
			gboolean launcher_add;
			char *dir_name;
			char *pixmap_name;

			g_snprintf (propname, sizeof (propname), "name_%d", i);
			name = conditional_get_string(propname, NULL, NULL);
			g_snprintf (propname, sizeof (propname),
				    "applets_%d", i);
			applets = conditional_get_bool(propname, FALSE, NULL);
			g_snprintf (propname, sizeof (propname),
				    "launcher_add_%d", i);
			launcher_add = conditional_get_bool(propname, FALSE, NULL);
			g_snprintf (propname, sizeof (propname),
				    "dir_name_%d", i);
			dir_name = conditional_get_string(propname, NULL, NULL);
			g_snprintf (propname, sizeof (propname),
				    "pixmap_name_%d", i);
			pixmap_name = conditional_get_string(propname, NULL, NULL);

			if(!menu) {
				menu = menu_new ();
			}

			menu = create_menu_at (menu, name,
					       applets,
					       launcher_add,
					       dir_name,
					       pixmap_name, TRUE, FALSE);

			g_free(name);
			g_free(dir_name);
			g_free(pixmap_name);
		}

		if(menu != NULL&&
		   g_object_get_data (G_OBJECT (menu), "mf") == NULL) {
			g_object_unref (G_OBJECT (menu));
			menu = NULL;
		}
	}

	if (menu == NULL) {
		g_free (special);
		g_free (title);
		g_free (wmclass);
		return;
	}

	/*set the panel to use as the data, or we will use current_panel*/
	g_object_set_data (G_OBJECT(menu), "menu_panel",
			   menu_panel_widget);
	panel_signal_connect_object_while_alive(
	      G_OBJECT (menu_panel_widget),
	      "destroy", G_CALLBACK (g_object_unref),
	      G_OBJECT (menu));
	
	/* This is so that we get size of the menu right */
	show_tearoff_menu (menu, title, FALSE, x, y, wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = g_object_get_data (G_OBJECT (menu), "mf");
	tm->title = title;
	tm->special = special;
	tm->wmclass = wmclass;
	g_signal_connect (G_OBJECT (menu), "destroy",
			  G_CALLBACK (tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

void
panel_menu_session_load_tornoffs (void)
{
	char *s;
	int i, length;

	length = conditional_get_int("tearoffs_count", 0, NULL);
	gnome_config_pop_prefix();

	for (i = 0; i < length; i++) {
		char *prefix;
		const char *sep;

		s = g_strdup_printf ("%spanel%s/TornoffMenu_%d/",
				     prefix, sep, i);
		g_free (prefix);
		gnome_config_push_prefix (s);
		g_free (s);

		load_tearoff_menu ();

		gnome_config_pop_prefix ();
	}
}
#endif

#ifdef FIXME
/* FIXME: maybe should be removed, but I want to investigate the
 * possibility of multiscreen xinerama correctness for gtk menus
 * as well */
static void
menu_allocated (GtkWidget *menu, GtkAllocation *alloc)
{
	int screen = 0;
	PanelWidget *cur_panel = get_panel_from_menu_data (menu, FALSE);
	int x, y;
	GtkWidget *menutop;

	menutop = GTK_MENU_SHELL (menu)->active ?
		GTK_MENU (menu)->toplevel : GTK_MENU (menu)->tearoff_window;

	if (cur_panel == NULL) {
		screen = multiscreen_screen_from_pos (menutop->allocation.x,
						      menutop->allocation.y);
		if (screen < 0)
			screen = multiscreen_screen_from_pos
				(menutop->allocation.x +
				   menutop->allocation.width,
				 menutop->allocation.y);
		if (screen < 0)
			screen = multiscreen_screen_from_pos
				(menutop->allocation.x +
				   menutop->allocation.width,
				 menutop->allocation.y +
				   menutop->allocation.height);
		if (screen < 0)
			screen = multiscreen_screen_from_pos
				(menutop->allocation.x,
				 menutop->allocation.y +
				   menutop->allocation.height);
		if (screen < 0)
			screen = 0;
	} else if (BASEP_IS_WIDGET (cur_panel->panel_parent)) {
		screen = BASEP_WIDGET (cur_panel->panel_parent)->screen;
	} else if (FOOBAR_IS_WIDGET (cur_panel->panel_parent)) {
		screen = FOOBAR_WIDGET (cur_panel->panel_parent)->screen;
	}

	x = menutop->allocation.x;
	y = menutop->allocation.y;

	if (x + menutop->allocation.width >
	    multiscreen_x (screen) + multiscreen_width (screen))
		x = multiscreen_x (screen) + multiscreen_width (screen) -
			menutop->allocation.width;
	if (y + menutop->allocation.height >
	    multiscreen_y (screen) + multiscreen_height (screen))
		y = multiscreen_y (screen) + multiscreen_height (screen) -
			menutop->allocation.height;

	if (x < multiscreen_x (screen))
		x = multiscreen_x (screen);
	if (y < multiscreen_y (screen))
		y = multiscreen_y (screen);

	if (x != menutop->allocation.x ||
	    y != menutop->allocation.y) {
		gtk_widget_set_uposition (menutop, x, y);
	}
}
#endif

void
menu_save_to_gconf (Menu       *menu,
		    const char *gconf_key)
{
        GConfClient *client;
        char        *profile;
        char        *temp_key;

        client  = panel_gconf_get_client ();
        profile = session_get_current_profile ();

        temp_key = panel_gconf_objects_profile_get_full_key (profile, gconf_key, "path");
        gconf_client_set_string (client, temp_key, menu->path, NULL);
        g_free (temp_key);

        temp_key = panel_gconf_objects_profile_get_full_key (profile, gconf_key, "main-menu");
        gconf_client_set_bool (client, temp_key, menu->main_menu, NULL);
        g_free (temp_key);

        temp_key = panel_gconf_objects_profile_get_full_key (profile, gconf_key, "global-main");
        gconf_client_set_bool (client, temp_key, menu->global_main, NULL);
        g_free (temp_key);

        temp_key = panel_gconf_objects_profile_get_full_key (profile, gconf_key, "custom-icon");
        gconf_client_set_bool (client, temp_key, menu->custom_icon, NULL);
        g_free (temp_key);

	if (menu->custom_icon) {
		temp_key = panel_gconf_objects_profile_get_full_key (profile, gconf_key, "custom-icon-file");
		gconf_client_set_string (client, temp_key, menu->custom_icon_file, NULL);
		g_free (temp_key);
	}

        temp_key = panel_gconf_objects_profile_get_full_key (profile, gconf_key, "main-menu-flags");
        gconf_client_set_int (client, temp_key, menu->main_menu_flags, NULL);
        g_free (temp_key);
	g_free (profile);
}

void
menu_load_from_gconf (PanelWidget *panel_widget,
		      gint         position,
		      const char  *gconf_key,
		      gboolean     use_default)
{
        GConfClient *client;
        char        *profile;
        char        *temp_key;
	gboolean     main_menu;
	gboolean     global_main;
	gboolean     custom_icon;
        char        *path;
        char        *custom_icon_file = NULL;
	int          flags;

        g_return_if_fail (panel_widget);
        g_return_if_fail (gconf_key);

        client  = panel_gconf_get_client ();
	/* FIXME : screeen checks */
	if (use_default)
		profile = g_strdup ("medium");
	else
		profile = session_get_current_profile ();

        temp_key = panel_gconf_objects_get_full_key (profile, gconf_key, "path", use_default);
        path = gconf_client_get_string (client, temp_key, NULL);
        g_free (temp_key);

        temp_key = panel_gconf_objects_get_full_key (profile, gconf_key, "main-menu", use_default);
        main_menu = gconf_client_get_bool (client, temp_key, NULL);
        g_free (temp_key);

        temp_key = panel_gconf_objects_get_full_key (profile, gconf_key, "global-main", use_default);
        global_main = gconf_client_get_bool (client, temp_key, NULL);
        g_free (temp_key);

        temp_key = panel_gconf_objects_get_full_key (profile, gconf_key, "custom-icon", use_default);
        custom_icon = gconf_client_get_bool (client, temp_key, NULL);
        g_free (temp_key);

	if (custom_icon) {
        	temp_key = panel_gconf_objects_get_full_key (profile, gconf_key, "custom-icon-file", use_default);
	        custom_icon_file = gconf_client_get_string (client, temp_key, NULL);
	        g_free (temp_key);
	}

        temp_key = panel_gconf_objects_get_full_key (profile, gconf_key, "main-menu-flags", use_default);
        flags = gconf_client_get_int (client, temp_key, NULL);
        g_free (temp_key);

	load_menu_applet (path, main_menu, flags, global_main,
			  custom_icon, custom_icon_file,
			  panel_widget, position, TRUE, gconf_key);

        g_free (path);
        g_free (custom_icon_file);
	g_free (profile);
}
