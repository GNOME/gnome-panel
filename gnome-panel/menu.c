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
#include <libgnome/gnome-desktop-item.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomeui/gnome-ditem-edit.h>
#include <gconf/gconf-client.h>
#include <libbonobo.h>

#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "menu.h"

#include "aligned-widget.h"
#include "button-widget.h"
#include "distribution.h"
#include "drawer-widget.h"
#include "edge-widget.h"
#include "floating-widget.h"
#include "foobar-widget.h"
#include "gnome-run.h"
#include "launcher.h"
#include "nothing.h"
#include "menu-fentry.h"
#include "menu-util.h"
#include "menu-ditem.h"
#include "multiscreen-stuff.h"
#include "panel-util.h"
#include "panel-gconf.h"
#include "panel-main.h"
#include "panel.h"
#include "panel-config-global.h"
#include "session.h"
#include "sliding-widget.h"
#include "panel-applet-frame.h"
#include "quick-desktop-reader.h"
#include "xstuff.h"
#include "egg-screen-exec.h"
#include "panel-stock-icons.h"
#include "panel-action-button.h"
#include "panel-recent.h"

#undef MENU_DEBUG

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

typedef struct _ShowItemMenu ShowItemMenu;
struct _ShowItemMenu {
	int type;
	const char *item_loc;
	MenuFinfo *mf;
	GtkWidget *menu;
	GtkWidget *menuitem;
};

typedef struct {
	GtkWidget   *pixmap;
	const char  *stock_id;
	char        *image;
	char        *fallback_image;
	gboolean     force_image;
	GtkIconSize  icon_size;
} IconToLoad;

typedef struct {
	GtkWidget   *image;
	const char  *stock_id;
	GdkPixbuf   *pixbuf;
	GtkIconSize  icon_size;
} IconToAdd;

static guint load_icons_id = 0;
static GHashTable *loaded_icons = NULL;
static GList *icons_to_load = NULL;
static GList *icons_to_add = NULL;

static GSList *image_menu_items = NULL;

static GtkWidget * create_menu_at_fr (GtkWidget *menu,
				      FileRec *fr,
				      gboolean launcher_add,
				      const char *dir_name,
				      gboolean fake_submenus,
				      gboolean force);

static GtkWidget * create_desktop_menu (GtkWidget *m,
					gboolean fake_sub);

static void add_kde_submenu (GtkWidget *root_menu,
			     gboolean fake_submenus,
			     gboolean launcher_add);
static void add_distribution_submenu (GtkWidget *root_menu,
				      gboolean fake_submenus,
				      gboolean launcher_add);

static GtkWidget * create_add_launcher_menu (GtkWidget *menu,
					     gboolean fake_submenus);

static void setup_stock_menu_item     (GtkWidget   *item,
				       GtkIconSize  icon_size,
				       const char  *stock_id,
				       const char  *title);
static gboolean panel_menu_key_press_handler (GtkWidget   *widget,
					      GdkEventKey *event);
static void         make_add_submenu   (GtkWidget             *menu,
				        Bonobo_ServerInfoList *applet_list);
static PanelWidget *menu_get_panel     (GtkWidget *menu);
static GdkScreen   *menuitem_to_screen (GtkWidget *menuitem);

static inline gboolean
panel_menu_have_icons (void)
{
	return gconf_client_get_bool (
			panel_gconf_get_client (),
			"/desktop/gnome/interface/menus_have_icons",
			NULL);
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
about_cb (GtkWidget *menuitem, gpointer data)
{
	static GtkWidget *about;
	GtkWidget *hbox, *l;
	GdkPixbuf *logo;
	GString *comment;
	char *logo_file;
	char *authors[] = {
	  "George Lebl (jirka@5z.com)",
	  "Jacob Berkman (jberkman@andrew.cmu.edu)",
	  "Miguel de Icaza (miguel@kernel.org)",
	  "Federico Mena (quartic@gimp.org)",
	  "Tom Tromey (tromey@cygnus.com)",
	  "Ian Main (imain@gtk.org)",
	  "Elliot Lee (sopwith@redhat.com)",
	  "Owen Taylor (otaylor@redhat.com)",
	  "Mark McLoughlin (mark@skynet.ie)",
	  "Alex Larsson (alexl@redhat.com)",
	  "Martin Baulig (baulig@suse.de)",
	  "Seth Nickell (snickell@stanford.edu)",
	  "Darin Adler (darin@bentspoon.com)",
	  "Glynn Foster (glynn.foster@sun.com)",
	  "Stephen Browne (stephen.browne@sun.com)",
	  "Anders Carlsson (andersca@gnu.org)",
	  "Padraig O'Briain (padraig.obriain@sun.com)",
	  "Ian McKellar <yakk@yakk.net>",
	N_("Many, many others..."),
	/* ... from the Monty Pythons show...  */
	N_("and finally, The Knights Who Say... NI!"),
	  NULL
	  };
	char *documenters[] = {
		"Dave Mason (dcm@redhat.com)",
		"Dan Mueth (d-mueth@uchicago.edu)",
	        "Alexander Kirillov (kirillov@math.sunysb.edu)",
		NULL
	  };
	/* Translator credits */
	char *translator_credits = _("translator_credits");

	if (about) {
		gtk_window_set_screen (
			GTK_WINDOW (about), menuitem_to_screen (menuitem));
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	{
		int i=0;
		while (authors[i] != NULL) {
		       	authors[i]=_(authors[i]);
			i++;
		}
	}

	logo = NULL;
	logo_file = panel_pixmap_discovery ("gnome-gegl2.png",
					    FALSE /* fallback */);
	if (logo_file != NULL) {
		logo = gdk_pixbuf_new_from_file (logo_file, NULL /* error */);
		g_free (logo_file);
	}

	comment = g_string_new (_("This program is responsible for launching "
				  "other applications, embedding small applets "
				  "within itself, world peace, and random X crashes."));

	if (commie_mode) {
		g_string_append (comment,
				 _("\n\nRunning in \"Lockdown\" mode.  This "
				   "means your system administrator has "
				   "prohibited any changes to the panel's "
				   "configuration to take place."));
	}
	
	about = gnome_about_new ( _("The GNOME Panel"), VERSION,
			"Copyright \xc2\xa9 1997-2002 Free Software Foundation, Inc.",
			comment->str,
			(const char **)authors,
			(const char **)documenters,
			strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
			logo);

	g_object_unref (logo);
	g_string_free (comment, TRUE);

	gtk_window_set_wmclass (GTK_WINDOW (about), "about_dialog", "Panel");
	gtk_window_set_screen (GTK_WINDOW (about),
			       menuitem_to_screen (menuitem));
	g_signal_connect (about, "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &about);
	g_signal_connect (about, "event",
			  G_CALLBACK (check_for_screen), NULL);

	hbox = gtk_hbox_new (TRUE, 0);
	l = gnome_href_new ("http://www.wfp.org/",
			    _("End world hunger"));
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (about)->vbox),
			    hbox, TRUE, FALSE, 0);
	gtk_widget_show_all (hbox);

	gtk_widget_show (about);
}

static void
about_gnome_cb (GtkWidget *menuitem,
		char      *program_path)
{
	GdkScreen *screen = menuitem_to_screen (menuitem);

	if (egg_screen_execute_async (screen, g_get_home_dir (), 1, &program_path) < 0)
		panel_error_dialog (screen,
				    "cannot_exec_about_gnome",
				    _("<b>Can't execute 'About GNOME'</b>\n\n"
				    "Details: %s probably does not exist"),
				    program_path);
}

static void
activate_app_def (GtkWidget  *menuitem,
		  const char *item_loc)
{
	GError           *error = NULL;
	GnomeDesktopItem *item;

	item = gnome_desktop_item_new_from_uri (
			item_loc,
			GNOME_DESKTOP_ITEM_LOAD_NO_TRANSLATIONS,
			&error);

	if (item) {
		panel_ditem_launch (
			item, NULL, 0, menuitem_to_screen (menuitem), &error);
		if (error) {
			panel_error_dialog (
				menuitem_to_screen (menuitem),
				"cant_launch_entry",
				_("<b>Can't launch entry</b>\n\n"
				  "Details: %s"), error->message);
			g_clear_error (&error);
		}
		gnome_desktop_item_unref (item);
	} else {
		g_assert (error != NULL);
		panel_error_dialog (
			menuitem_to_screen (menuitem),
			"cant_load_entry",
			_("<b>Can't load entry</b>\n\n"
			  "Details: %s"), error->message);
		g_clear_error (&error);
	}
}

static PanelWidget *
menu_get_panel (GtkWidget *menu)
{
	PanelWidget *retval = NULL;

	g_return_val_if_fail (menu != NULL, NULL);

	if (GTK_IS_MENU_ITEM (menu))
		menu = menu->parent;

	g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

	while (menu) {
		retval = g_object_get_data (G_OBJECT (menu), "menu_panel");
		if (retval)
			break;

		menu = gtk_menu_get_attach_widget (GTK_MENU (menu))->parent;
		if (!GTK_IS_MENU (menu))
			break;
	}

	if (retval && !PANEL_IS_WIDGET (retval)) {
		g_warning ("Invalid PanelWidget associated with menu");
		retval = NULL;
	}

	if (!retval) {
		g_warning ("Cannot find the PanelWidget associated with menu");
		retval = panels->data;
	}

	return retval;
}

static void
setup_menu_panel (GtkWidget *menu)
{
	PanelWidget *panel;

	panel = g_object_get_data (G_OBJECT (menu), "menu_panel");
	if (panel)
		return;

	panel = menu_get_panel (menu);
	g_object_set_data (G_OBJECT (menu), "menu_panel", panel);
}

static GdkScreen *
menuitem_to_screen (GtkWidget *menuitem)
{
	PanelWidget *panel_widget;

	panel_widget = menu_get_panel (menuitem);

	return gtk_window_get_screen (
			GTK_WINDOW (panel_widget->panel_parent));
}

static void
menus_have_icons_changed (GConfClient *client,
			  guint        cnxn_id,
			  GConfEntry  *entry,
			  GtkWidget   *menu)
{
	GConfValue *value;
	GList      *list, *l;
	gboolean    have_icons = TRUE;

	value = gconf_entry_get_value (entry);

	if (value->type == GCONF_VALUE_BOOL)
		have_icons = gconf_value_get_bool (value);

	list = g_list_copy (GTK_MENU_SHELL (menu)->children);
	for (l = list; l; l = l->next) {
		GtkWidget *item = l->data;
		GtkWidget *cur_image;
		GtkWidget *image;

		if (!GTK_IS_IMAGE_MENU_ITEM (item))
			continue;

		image = g_object_get_data (G_OBJECT (item), "Panel:Image");
		if (!image)
			continue;

		/* A forced image is always on */
		if (g_object_get_data (G_OBJECT (item), "Panel:ForceImage"))
			continue;

		cur_image = gtk_image_menu_item_get_image (
					GTK_IMAGE_MENU_ITEM (item));

		if (have_icons) {
			if (cur_image != image) {
				gtk_image_menu_item_set_image (
					GTK_IMAGE_MENU_ITEM (item), image);
				gtk_widget_show (image);
			}
		} else {
			gtk_image_menu_item_set_image (
				GTK_IMAGE_MENU_ITEM (item), NULL);
		}
	}
	g_list_free (list);
}

typedef struct
{
	GtkMenuPositionFunc orig_func;
	gpointer orig_data;
} MenuReposition;

/* FIXME:
 * Stolen mostly from GTK+ and modified for our purposes of multiscreen
 * things.  Kind of evil, but oh well
 *
 * This is deprecated in gtk now, that's why we need the GTK_MENU_INTERNALS
 * define in Makefile.am
 */
static void
our_menu_item_position_menu (GtkMenu  *menu,
			     int       monitor_width,
			     int       monitor_height,
			     int       monitor_basex,
			     int       monitor_basey,
			     int      *x,
			     int      *y)
{
	GtkMenuItem *menu_item;
	GtkWidget *widget;
	GtkWidget *parent_menu_item;
	int twidth, theight;
	int tx, ty;

	g_return_if_fail (menu != NULL);
	g_return_if_fail (x != NULL);
	g_return_if_fail (y != NULL);

	menu_item = GTK_MENU_ITEM (menu->parent_menu_item);
	widget = GTK_WIDGET (menu_item);

	twidth = GTK_WIDGET (menu)->requisition.width;
	theight = GTK_WIDGET (menu)->requisition.height;

	if ( ! gdk_window_get_origin (widget->window, &tx, &ty)) {
		g_warning ("Menu not on screen");
		return;
	}

	tx -= monitor_basex;
	ty -= monitor_basey;

	tx += widget->allocation.x;
	ty += widget->allocation.y;

	switch (menu_item->submenu_placement)
	{
	case GTK_TOP_BOTTOM:
		if ((ty + widget->allocation.height + theight) <= monitor_height)
			ty += widget->allocation.height;
		else if ((ty - theight) >= 0)
			ty -= theight;
		else if (monitor_height - (ty + widget->allocation.height) > ty)
			ty += widget->allocation.height;
		else
			ty -= theight;
		break;

	case GTK_LEFT_RIGHT:
		menu_item->submenu_direction = GTK_DIRECTION_RIGHT;
		parent_menu_item = GTK_MENU (widget->parent)->parent_menu_item;
		if (parent_menu_item)
			menu_item->submenu_direction = GTK_MENU_ITEM (parent_menu_item)->submenu_direction;

		switch (menu_item->submenu_direction)
		{
		case GTK_DIRECTION_LEFT:
			if ((tx - twidth) >= 0)
				tx -= twidth;
			else
			{
				menu_item->submenu_direction = GTK_DIRECTION_RIGHT;
				tx += widget->allocation.width - 5;
			}
			break;

		case GTK_DIRECTION_RIGHT:
			if ((tx + widget->allocation.width + twidth - 5) <= monitor_width)
				tx += widget->allocation.width - 5;
			else
			{
				menu_item->submenu_direction = GTK_DIRECTION_LEFT;
				tx -= twidth;
			}
			break;
		}

		ty += widget->allocation.height / 4;

		/* If the height of the menu doesn't fit we move it upward. */
		ty = CLAMP (ty, 0, MAX (0, monitor_height - theight));
		break;
	}

	/* If we have negative, tx, here it is because we can't get
	 * the menu all the way on screen. Favor the left portion.
	 */
	tx = CLAMP (tx, 0, MAX (0, monitor_width - twidth));

	*x = tx + monitor_basex;
	*y = ty + monitor_basey;
}

static void
menu_on_screen (GtkMenu  *menu,
		gint     *x,
		gint     *y,
		gboolean *push_in,
		gpointer  data)
{
	MenuReposition *repo = data;
	GtkRequisition  req;
	int             screen;
	int             monitor;
	int             monitor_width;
	int             monitor_height;
	int             monitor_basex;
	int             monitor_basey;

	gtk_widget_get_child_requisition (GTK_WIDGET (menu), &req);

	screen = gdk_screen_get_number (
			gtk_widget_get_screen (GTK_WIDGET (menu)));
	monitor = multiscreen_locate_coords (screen, *x, *y);

	monitor_width  = multiscreen_width (screen, monitor);
	monitor_height = multiscreen_height (screen, monitor);
	monitor_basex  = multiscreen_x (screen, monitor);
	monitor_basey  = multiscreen_y (screen, monitor);

	if (repo->orig_func != NULL) {
		repo->orig_func (menu, x, y, push_in, repo->orig_data);

		if (menu->parent_menu_item != NULL) {
			/* This is a submenu so behave submenuish */
			if (*x < monitor_basex ||
			    *x + req.width > monitor_basex + monitor_width ||
			    *y < monitor_basey ||
			    *y + req.height > monitor_basey + monitor_height) {
				/* Offscreen! EVIL, ignore the position
				 * and recalculate using our hack */
				our_menu_item_position_menu (menu,
							     monitor_width,
							     monitor_height,
							     monitor_basex,
							     monitor_basey,
							     x,
							     y);
			}
		} else {
			/* just make sure the menu is within screen */
			*x -= monitor_basex;
			*y -= monitor_basey;

			if ((*x + req.width) > monitor_width)
				*x -= ((*x + req.width) - monitor_width);
			if (*x < 0)
				*x = 0;
			if ((*y + req.height) > monitor_height)
				*y -= ((*y + req.height) - monitor_height);
			if (*y < 0)
				*y = 0;

			*x += monitor_basex;
			*y += monitor_basey;
		}
	} else {
		*x -= monitor_basex;
		*y -= monitor_basey;
		*x = CLAMP (*x - 2, 0, MAX (0, monitor_width - req.width));
		*y = CLAMP (*y - 2, 0, MAX (0, monitor_height - req.height));
		*x += monitor_basex;
		*y += monitor_basey;
		*push_in = TRUE;
	}
}

void
panel_make_sure_menu_within_screen (GtkMenu *menu)
{
	MenuReposition *repo;

	/* if already set to a standard pos func, just ignore */
	if (menu->position_func == menu_on_screen ||
	    menu->position_func == panel_menu_position)
		return;

	repo = g_new0 (MenuReposition, 1);
	g_object_weak_ref (G_OBJECT (menu),
			   (GWeakNotify) g_free,
			   repo);

	repo->orig_func = menu->position_func;
	repo->orig_data = menu->position_func_data;

	menu->position_func = menu_on_screen;
	menu->position_func_data = repo;

	our_gtk_menu_position (menu);
}

static void
reload_image_menu_items (void)
{
	GSList *l;

	for (l = image_menu_items; l; l = l->next) {
		GtkWidget *image = l->data;
		gboolean   is_mapped;
      
		is_mapped = GTK_WIDGET_MAPPED (image);

		if (is_mapped)
			gtk_widget_unmap (image);

		gtk_image_set_from_pixbuf (GTK_IMAGE (image), NULL);
    
		if (is_mapped)
			gtk_widget_map (image);

	}
}

static void
icon_theme_changed (GnomeIconTheme *icon_theme,
		    gpointer        data)
{
	reload_image_menu_items ();
}

GtkWidget *
panel_menu_new (void)
{
	GtkWidget       *menu;
	static gboolean  registred_icon_theme_changer = FALSE;

	if (!registred_icon_theme_changer) {
		registred_icon_theme_changer = TRUE;

		g_signal_connect (panel_icon_theme, "changed",
				  G_CALLBACK (icon_theme_changed), NULL);
	}
	
	menu = gtk_menu_new ();

	panel_gconf_notify_add_while_alive ("/desktop/gnome/interface/menus_have_icons",
					    (GConfClientNotifyFunc) menus_have_icons_changed,
					    G_OBJECT (menu));

	g_signal_connect_after (menu, "show",
				G_CALLBACK (panel_make_sure_menu_within_screen),
				NULL);

	g_signal_connect (menu, "key_press_event",
			  G_CALLBACK (panel_menu_key_press_handler),
			  NULL);

	return menu;
}

static GtkWidget *
menu_new (void)
{
	GtkWidget *retval;

	retval = panel_menu_new ();

	g_signal_connect (
		retval, "show", G_CALLBACK (setup_menu_panel), NULL);

	return retval;
}

static void
icon_to_load_free (IconToLoad *icon)
{
	if (!icon)
		return;

	g_object_unref (icon->pixmap); icon->pixmap = NULL;
	g_free (icon->image);          icon->image = NULL;
	g_free (icon->fallback_image); icon->fallback_image = NULL;
	g_free (icon);
}

static IconToLoad *
icon_to_load_copy (IconToLoad *icon)
{
	IconToLoad *retval;

	if (!icon)
		return NULL;

	retval = g_new0 (IconToLoad, 1);

	retval->pixmap         = g_object_ref (icon->pixmap);
	retval->image          = g_strdup (icon->image);
	retval->fallback_image = g_strdup (icon->fallback_image);
	retval->force_image    = icon->force_image;
	retval->stock_id       = icon->stock_id;
	retval->icon_size      = icon->icon_size;

	return retval;
}

static void
remove_pixmap_from_loaded (gpointer data, GObject *where_the_object_was)
{
	char *key = data;

	if (loaded_icons != NULL)
		g_hash_table_remove (loaded_icons, key);

	g_free (key);
}

GdkPixbuf *
panel_make_menu_icon (const char *icon,
		      const char *fallback,
		      int         size,
		      gboolean   *long_operation)
{
	GdkPixbuf *pb;
	char *file, *key;
	gboolean loaded;

	g_return_val_if_fail (size > 0, NULL);

	if (long_operation != NULL)
		*long_operation = TRUE;

	file = gnome_desktop_item_find_icon (panel_icon_theme,
					     icon,
					     size /* desired size */,
					     0 /* flags */);
	if (file == NULL && fallback != NULL)
		file = gnome_desktop_item_find_icon (panel_icon_theme,
						     fallback,
						     size /* desired size */,
						     0 /* flags */);

	if (file == NULL) {
		/* we didn't do anything long/hard */
		if (long_operation != NULL)
			*long_operation = FALSE;
		return NULL;
	}

	pb = NULL;

	loaded = FALSE;

	key = g_strdup_printf ("%d:%s", size, file);

	if (loaded_icons != NULL &&
	    (pb = g_hash_table_lookup (loaded_icons, key)) != NULL) {
		if (pb != NULL)
			g_object_ref (G_OBJECT (pb));
	}

	if (pb == NULL) {
		pb = gdk_pixbuf_new_from_file (file, NULL);
		
		/* add icon to the hash table so we don't load it again */
		loaded = TRUE;
	}

	if (pb == NULL) {
		g_free (file);
		g_free (key);
		return NULL;
	}

	if (loaded &&
	    (gdk_pixbuf_get_width (pb) != size ||
	     gdk_pixbuf_get_height (pb) != size)) {
		GdkPixbuf *pb2;
		pb2 = gdk_pixbuf_scale_simple (pb, size, size,
					       GDK_INTERP_BILINEAR);
		g_object_unref (G_OBJECT (pb));
		pb = pb2;
	}

	if (loaded) {
		if (loaded_icons == NULL)
			loaded_icons = g_hash_table_new_full
				(g_str_hash, g_str_equal,
				 (GDestroyNotify) g_free,
				 (GDestroyNotify) g_object_unref);
		g_hash_table_replace (loaded_icons,
				      g_strdup (key),
				      g_object_ref (G_OBJECT (pb)));
		g_object_weak_ref (G_OBJECT (pb),
				   (GWeakNotify) remove_pixmap_from_loaded,
				   g_strdup (key));
	} else {
		/* we didn't load from disk */
		if (long_operation != NULL)
			*long_operation = FALSE;
	}

	g_free (file);
	g_free (key);

	return pb;
}

static void
menu_item_style_set (GtkImage *image,
		     gpointer  data)
{
	GtkWidget   *widget;
	GdkPixbuf   *pixbuf;
	GtkIconSize  icon_size = (GtkIconSize) GPOINTER_TO_INT (data);
	int          icon_height;
	gboolean     is_mapped;

	if (!gtk_icon_size_lookup (icon_size, NULL, &icon_height))
		return;

	pixbuf = gtk_image_get_pixbuf (image);
	if (!pixbuf)
		return;

	if (gdk_pixbuf_get_height (pixbuf) == icon_height)
		return;

	widget = GTK_WIDGET (image);

	is_mapped = GTK_WIDGET_MAPPED (widget);
	if (is_mapped)
		gtk_widget_unmap (widget);

	gtk_image_set_from_pixbuf (image, NULL);
    
	if (is_mapped)
		gtk_widget_map (widget);
}

static void
do_icons_to_add (void)
{
	while (icons_to_add) {
		IconToAdd *icon_to_add = icons_to_add->data;

		icons_to_add = g_list_delete_link (icons_to_add, icons_to_add);

		if (icon_to_add->stock_id)
			gtk_image_set_from_stock (
				GTK_IMAGE (icon_to_add->image),
				icon_to_add->stock_id,
				icon_to_add->icon_size);
		else {
			g_assert (icon_to_add->pixbuf);

			gtk_image_set_from_pixbuf (
				GTK_IMAGE (icon_to_add->image),
				icon_to_add->pixbuf);

			g_signal_connect (icon_to_add->image, "style-set",
					  G_CALLBACK (menu_item_style_set),
					  GINT_TO_POINTER (icon_to_add->icon_size));

			g_object_unref (icon_to_add->pixbuf);
		}

		g_object_unref (icon_to_add->image);
		g_free (icon_to_add);
	}
}

static gboolean
load_icons_handler (gpointer data)
{
	IconToLoad *icon;
	gboolean    long_operation = FALSE;

load_icons_handler_again:

	if (!icons_to_load) {
		load_icons_id = 0;
		do_icons_to_add ();

		return FALSE;
	}

	icon = icons_to_load->data;
	icons_to_load->data = NULL;
	/* pop */
	icons_to_load = g_list_delete_link (icons_to_load, icons_to_load);

	/* if not visible anymore, just ignore */
	if ( ! GTK_WIDGET_VISIBLE (icon->pixmap)) {
		icon_to_load_free (icon);
		/* we didn't do anything long/hard, so just do this again,
		 * this is fun, don't go back to main loop */
		goto load_icons_handler_again;
	}

	if (icon->stock_id) {
		IconToAdd *icon_to_add;

		icon_to_add            = g_new (IconToAdd, 1);
		icon_to_add->image     = g_object_ref (icon->pixmap);
		icon_to_add->stock_id  = icon->stock_id;
		icon_to_add->pixbuf    = NULL;
		icon_to_add->icon_size = icon->icon_size;

		icons_to_add = g_list_prepend (icons_to_add, icon_to_add);
	} else {
		IconToAdd *icon_to_add;
		GdkPixbuf *pb;
		int        icon_height = PANEL_DEFAULT_MENU_ICON_SIZE;

		gtk_icon_size_lookup (icon->icon_size, NULL, &icon_height);

		pb = panel_make_menu_icon (icon->image,
					   icon->fallback_image,
					   icon_height,
					   &long_operation);
		if (!pb) {
			icon_to_load_free (icon);
			if (long_operation)
				/* this may have been a long operation so jump back to
				 * the main loop for a while */
				return TRUE;
			else
				/* we didn't do anything long/hard, so just do this again,
				 * this is fun, don't go back to main loop */
				goto load_icons_handler_again;
		}

		icon_to_add            = g_new (IconToAdd, 1);
		icon_to_add->image     = g_object_ref (icon->pixmap);
		icon_to_add->stock_id  = NULL;
		icon_to_add->pixbuf    = pb;
		icon_to_add->icon_size = icon->icon_size;

		icons_to_add = g_list_prepend (icons_to_add, icon_to_add);
	}

	icon_to_load_free (icon);

	if (!long_operation)
		/* we didn't do anything long/hard, so just do this again,
		 * this is fun, don't go back to main loop */
		goto load_icons_handler_again;

	/* if still more we'll come back */
	return TRUE;
}

static void
add_new_app_to_menu (GtkWidget    *widget,
		     ShowItemMenu *sim)
{
	g_return_if_fail (sim->mf != NULL);

	panel_new_launcher (sim->mf->menudir,
			    menuitem_to_screen (sim->menuitem));
}

static void
remove_menuitem (GtkWidget *widget, ShowItemMenu *sim)
{
	char *file;
	char *dir, *directory_file;
	GnomeDesktopItem *ditem;
	GnomeVFSResult result;

	g_return_if_fail (sim->item_loc != NULL);
	g_return_if_fail (sim->menuitem != NULL);

	gtk_widget_hide (sim->menuitem);

	result = gnome_vfs_unlink (sim->item_loc);

	if (result != GNOME_VFS_OK) {
		char *esc;

		esc = g_markup_escape_text (sim->item_loc, -1);

		panel_error_dialog (
			menuitem_to_screen (sim->menuitem),
			"cant_remove_menu_item",
			_("<b>Could not remove the menu item %s</b>\n\n"
			  "Details: %s\n"), 
			esc,
			gnome_vfs_result_to_string (result));
		g_free (esc);
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
add_to_run_dialog (GtkWidget    *widget,
		   ShowItemMenu *sim)
{
	GError           *error = NULL;
	GnomeDesktopItem *item;

	item = gnome_desktop_item_new_from_uri (sim->item_loc,
						GNOME_DESKTOP_ITEM_LOAD_NO_TRANSLATIONS,
						&error);
	if (item != NULL) {
		const char *exec;

		exec = gnome_desktop_item_get_string (
				item, GNOME_DESKTOP_ITEM_EXEC);
		if (!exec)
			exec = gnome_desktop_item_get_string (
				item, GNOME_DESKTOP_ITEM_URL);

		if (exec != NULL)
			show_run_dialog_with_text (menuitem_to_screen (sim->menuitem), exec);
		else
			panel_error_dialog (
				menuitem_to_screen (sim->menuitem),
				"no_exec_or_url_field",
				_("<b>Can't add to run box</b>\n\n"
				  "Details: No 'Exec' or 'URL' field in entry"));

		gnome_desktop_item_unref (item);
	} else {
		g_assert (error != NULL);
		panel_error_dialog (
			menuitem_to_screen (sim->menuitem),
			"cant_load_entry",
			_("<b>Can't load entry</b>\n\n"
			  "Details: %s"), error->message);
		g_clear_error (&error);
	}
}

static void
show_help_on (GtkWidget    *widget,
	      ShowItemMenu *sim)
{
	GError           *error = NULL;
	GnomeDesktopItem *item;
	GdkScreen        *screen;

	screen = menuitem_to_screen (sim->menuitem);

	item = gnome_desktop_item_new_from_uri (sim->item_loc,
						GNOME_DESKTOP_ITEM_LOAD_NO_TRANSLATIONS,
						&error);
	if (item != NULL) {
		const char *docpath = gnome_desktop_item_get_string
			(item, "X-GNOME-DocPath");
		if ( ! panel_show_gnome_kde_help (screen, docpath, &error)) {
			panel_error_dialog (
				screen,
				"cannot_show_gnome_kde_help",
				_("<b>Cannot display help document</b>\n\n"
				  "Details: %s"), error->message);
			g_clear_error (&error);
		}

		gnome_desktop_item_unref (item);
	} else {
		g_assert (error != NULL);
		panel_error_dialog (
			screen,
			"cant_load_entry",
			_("<b>Can't load entry</b>\n\n"
			  "Details: %s"), error->message);
		g_clear_error (&error);
	}
}

static void
add_app_to_panel (GtkWidget    *widget,
		  ShowItemMenu *sim)
{
	Launcher *launcher;
	PanelWidget *panel = menu_get_panel (widget);
	PanelData *pd;
	int insertion_pos = -1;

	pd = g_object_get_data (G_OBJECT (panel->panel_parent), "PanelData");
	if (pd != NULL)
		insertion_pos = pd->insertion_pos;

	launcher = load_launcher_applet (sim->item_loc, panel, insertion_pos, FALSE, NULL);

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

	list = get_mfiles_from_menudir (dirname, NULL /* sorted */);
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
add_menudrawer_to_panel(GtkWidget *widget, gpointer data)
{
	MenuFinfo *mf = data;
	PanelWidget *panel = menu_get_panel (widget);
	PanelData *pd;
	int insertion_pos = -1;

	g_return_if_fail (mf != 0);

	pd = g_object_get_data (G_OBJECT (panel->panel_parent), "PanelData");
	if (pd != NULL)
		insertion_pos = pd->insertion_pos;

	add_drawers_from_dir (mf->menudir, mf->dir_name, insertion_pos, panel);
}

static void
add_menu_to_panel (GtkWidget *widget, gpointer data)
{
	const char *menudir = data;
	gboolean main_menu;
	PanelWidget *panel;
	PanelData *pd;
	int insertion_pos = -1;
	int flags = get_default_menu_flags ();

	panel = menu_get_panel (widget);

	pd = g_object_get_data (G_OBJECT (panel->panel_parent), "PanelData");
	if (pd != NULL)
		insertion_pos = pd->insertion_pos;

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
			  panel /* panel */,
			  insertion_pos /* pos */,
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
	if (xgrab_shell && (GTK_MENU_SHELL (xgrab_shell)->have_xgrab))
		GTK_MENU_SHELL (xgrab_shell)->have_xgrab = 
			(gdk_pointer_grab (xgrab_shell->window, TRUE,
					   GDK_BUTTON_PRESS_MASK |
					   GDK_BUTTON_RELEASE_MASK |
					   GDK_ENTER_NOTIFY_MASK |
					   GDK_LEAVE_NOTIFY_MASK,
					   NULL, NULL, 0) == 0);
	
	gtk_grab_add (GTK_WIDGET (menu));
}

static void
edit_dentry (GtkWidget    *widget,
	     ShowItemMenu *sim)
{
	char *dir = NULL;

	g_return_if_fail (sim != NULL);
	g_return_if_fail (sim->item_loc != NULL);

	if (sim->mf)
		dir = sim->mf->menudir;

	panel_edit_dentry (sim->item_loc, dir,
			   menuitem_to_screen (sim->menuitem));
}

static void
edit_direntry (GtkWidget    *widget,
	       ShowItemMenu *sim)
{
	g_return_if_fail (sim != NULL);
	g_return_if_fail (sim->mf != NULL);

	panel_edit_direntry (sim->mf->menudir,
			     sim->mf->dir_name,
			     menuitem_to_screen (sim->menuitem));
}

static void
show_item_menu (GtkWidget *item, GdkEventButton *bevent, ShowItemMenu *sim)
{
	GtkWidget        *menuitem;
	GnomeDesktopItem *ii;
	PanelWidget      *panel_widget;

	panel_widget = menu_get_panel (sim->menuitem);

	if (!sim->menu) {
		sim->menu = menu_new ();
		g_object_ref (sim->menu);
		gtk_object_sink (GTK_OBJECT (sim->menu));

		g_object_set_data (
			G_OBJECT (sim->menu), "menu_panel", panel_widget);
		
		g_signal_connect (G_OBJECT (sim->menu), "deactivate",
				  G_CALLBACK (restore_grabs),
				  item);

		if (sim->type == 1) {
			char *tmp;

			ii = gnome_desktop_item_new_from_uri (sim->item_loc, 0, NULL);

			/* eek */
			if (ii == NULL) {
				g_object_unref (G_OBJECT (sim->menu));
				sim->menu = NULL;
				return;
			}

			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, panel_menu_icon_get_size (),
					NULL, _("Add this launcher to panel"));
			gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu), menuitem);
			g_signal_connect (menuitem, "activate",
					  G_CALLBACK (add_app_to_panel),
					  sim);

			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, panel_menu_icon_get_size (),
					NULL, _("Remove this item"));
			gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu), menuitem);
			g_signal_connect (G_OBJECT(menuitem), "activate",
					    G_CALLBACK (remove_menuitem),
					    sim);
			tmp = g_path_get_dirname(sim->item_loc);
			if ( ! panel_is_uri_writable (tmp))
				gtk_widget_set_sensitive(menuitem,FALSE);
			g_free (tmp);
			g_signal_connect_swapped (G_OBJECT (menuitem),
						   "activate",
						   G_CALLBACK (gtk_menu_shell_deactivate),
						   G_OBJECT (item->parent));

			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, panel_menu_icon_get_size (),
					NULL, _("Put into run dialog"));
			gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu),
					       menuitem);
			g_signal_connect (menuitem, "activate",
					  G_CALLBACK (add_to_run_dialog),
					  sim);
			g_signal_connect_swapped
				(G_OBJECT(menuitem),
				 "activate",
				 G_CALLBACK(gtk_menu_shell_deactivate),
				 G_OBJECT(item->parent));

			if (gnome_desktop_item_get_string (ii, "X-GNOME-DocPath") != NULL) {
				char *title;
				const char *name;

				menuitem = gtk_image_menu_item_new ();
				name = gnome_desktop_item_get_localestring (ii, GNOME_DESKTOP_ITEM_NAME);
				title = g_strdup_printf (_("Help on %s"),
							 (name != NULL) ? name : _("Application"));
				setup_menuitem (menuitem, panel_menu_icon_get_size (),
						NULL, title);
				g_free (title);
				gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu),
						 menuitem);
				g_signal_connect (menuitem, "activate",
					          G_CALLBACK (show_help_on),
					          sim);
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
			setup_menuitem (menuitem, panel_menu_icon_get_size (),
					NULL, _("_Properties"));
			gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu), menuitem);

			gnome_desktop_item_unref (ii);
		}
		
		if (sim->mf != NULL) {
			GtkWidget *submenu;

			if (sim->type == 0) {
				submenu = sim->menu;
			} else {
				submenu = menu_new ();

				g_object_set_data (
					G_OBJECT (submenu), "menu_panel", panel_widget);

				menuitem = gtk_image_menu_item_new ();
				setup_menuitem (menuitem, panel_menu_icon_get_size (),
						NULL, _("Entire menu"));
				gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu), menuitem);
				gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
							   submenu);
			}


			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, panel_menu_icon_get_size (), NULL,
					_("Add this as drawer to panel"));
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
			g_signal_connect (G_OBJECT(menuitem), "activate",
				   G_CALLBACK(add_menudrawer_to_panel),
				   sim->mf);

			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, panel_menu_icon_get_size (),
					NULL, _("Add this as menu to panel"));
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
			g_signal_connect (G_OBJECT(menuitem), "activate",
					   G_CALLBACK(add_menu_to_panel),
					   sim->mf->menudir);

			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, panel_menu_icon_get_size (),
					NULL, _("Add new item to this menu"));
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
			/*when activated we must pop down the first menu*/
			g_signal_connect_swapped (G_OBJECT (menuitem),
						 "activate",
						 G_CALLBACK (gtk_menu_shell_deactivate),
						 G_OBJECT (item->parent));

			g_signal_connect (menuitem, "activate",
					  G_CALLBACK (add_new_app_to_menu),
					  sim);
			if ( ! panel_is_uri_writable (sim->mf->menudir))
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
			setup_menuitem (menuitem, panel_menu_icon_get_size (),
					NULL, _("Properties"));
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
		}
	}

	gtk_menu_set_screen (
		GTK_MENU (sim->menu),
		panel_screen_from_toplevel (panel_widget->panel_parent));

	gtk_menu_popup (GTK_MENU (sim->menu),
			NULL,
			NULL,
			menu_item_menu_position,
			item,
			bevent->button,
			bevent->time);
}

static gboolean
menuitem_button_press_event (GtkWidget      *menuitem,
			     GdkEventButton *event,
			     ShowItemMenu   *sim)
{
	/* no item menu in commie mode */
	if (commie_mode)
		return FALSE;
	
	if (event->button == 3)
		show_item_menu (menuitem, event, sim);
	
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

  /* FIXME: workaround for a possible gtk+ bug
   *    See bugs #92085(gtk+) and #91184(panel) for details.
   */
  if (global_config.tooltips_enabled)
    gtk_tooltips_enable (panel_tooltips);

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
drag_data_get_menu_cb (GtkWidget        *widget,
		       GdkDragContext   *context,
		       GtkSelectionData *selection_data,
		       guint             info,
		       guint             time,
		       char             *item_loc)
{
	char *uri_list;

	uri_list = g_strconcat (item_loc, "\r\n", NULL);

	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)uri_list,
				strlen (uri_list));
	g_free (uri_list);

	/* FIXME: workaround for a possible gtk+ bug
	 *    See bugs #92085(gtk+) and #91184(panel) for details.
	 */
	gtk_tooltips_disable (panel_tooltips);
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
image_menuitem_size_request (GtkWidget      *menuitem,
			     GtkRequisition *requisition,
			     gpointer        data)
{
	GtkIconSize icon_size = (GtkIconSize) GPOINTER_TO_INT (data);
	int         icon_height;

	if (!gtk_icon_size_lookup (icon_size, NULL, &icon_height))
		return;

	/* If we don't have a pixmap for this menuitem
	 * at least make sure its the same height as
	 * the rest.
	 */
	requisition->height = MAX (requisition->height, icon_height);
}

static void
setup_full_menuitem (GtkWidget   *menuitem,
		     GtkIconSize  icon_size,
		     GtkWidget   *image,
		     const char  *title,
		     const char  *item_loc,
		     MenuFinfo   *mf)
			       
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "text/uri-list", 0, 0 }
	};

	GtkWidget *label;

	label = gtk_label_new_with_mnemonic (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);
	
	gtk_container_add (GTK_CONTAINER (menuitem), label);

	if (image) {
		g_object_set_data_full (G_OBJECT (menuitem),
					"Panel:Image",
					g_object_ref (image),
					(GDestroyNotify) g_object_unref);
		gtk_widget_show (image);
		if (panel_menu_have_icons ())
			gtk_image_menu_item_set_image (
				GTK_IMAGE_MENU_ITEM (menuitem), image);
	} else
		g_signal_connect (menuitem, "size_request",
				  G_CALLBACK (image_menuitem_size_request),
				  GINT_TO_POINTER (icon_size));

	if (item_loc != NULL) {
		ShowItemMenu *sim = g_new0 (ShowItemMenu, 1);
		sim->type = 1;
		sim->item_loc = item_loc; /*make sure you don't free this,
					    it's not ours!*/
		sim->mf = mf;
		sim->menuitem = menuitem;
                g_object_set_data (G_OBJECT (menuitem), "sim", sim);
		g_signal_connect (menuitem, "button_press_event",
				  G_CALLBACK (menuitem_button_press_event),
				  sim);
		g_signal_connect (G_OBJECT (menuitem), "destroy",
				  G_CALLBACK (destroy_item_menu),
				  sim);

		if ( ! commie_mode) {
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
setup_menuitem (GtkWidget   *menuitem,
		GtkIconSize  icon_size,
		GtkWidget   *image,
		const char  *title)
{
	setup_full_menuitem (menuitem, icon_size, image, title, NULL, NULL);
}

static void
setup_applet_drag (GtkWidget *menuitem, const char *iid)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet-iid", 0, 0 }
	};
	
	if (iid == NULL ||
	    commie_mode)
		return;
	
	gtk_drag_source_set (menuitem,
			     GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			     menu_item_targets, 1,
			     GDK_ACTION_COPY);
	
	g_signal_connect_data (G_OBJECT (menuitem), "drag_data_get",
			       G_CALLBACK (drag_data_get_string_cb),
			       g_strdup (iid),
			       (GClosureNotify)g_free,
			       0 /* connect_flags */);
	g_signal_connect (G_OBJECT (menuitem), "drag_end",
			  G_CALLBACK (drag_end_menu_cb), NULL);

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
add_drawer_to_panel (GtkWidget *widget, gpointer data)
{
	PanelWidget *panel = menu_get_panel (widget);
	PanelData *pd;
	int insertion_pos = -1;

	pd = g_object_get_data (G_OBJECT (panel->panel_parent), "PanelData");
	if (pd != NULL)
		insertion_pos = pd->insertion_pos;
	
	load_drawer_applet (NULL, NULL, NULL,
			    panel, insertion_pos, FALSE, NULL);
}

static void
add_action_button_to_panel (GtkWidget *widget,
			    gpointer   data)
{
	PanelWidget *panel = menu_get_panel (widget);
	PanelData *pd;
	int insertion_pos = -1;

	pd = g_object_get_data (G_OBJECT (panel->panel_parent), "PanelData");
	if (pd != NULL)
		insertion_pos = pd->insertion_pos;
	
	panel_action_button_load (
		GPOINTER_TO_INT (data), panel, insertion_pos, FALSE, NULL, FALSE);
}

static void
add_launcher (GtkWidget *widget, const char *item_loc)
{
	Launcher *launcher;
	PanelWidget *panel;
	PanelData *pd;
	int insertion_pos = -1;

	panel = menu_get_panel (widget);

	pd = g_object_get_data (G_OBJECT (panel->panel_parent), "PanelData");
	if (pd != NULL)
		insertion_pos = pd->insertion_pos;

	launcher = load_launcher_applet (item_loc, panel, insertion_pos, FALSE, NULL);

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
	g_free(mf);
}


static void
menu_destroy(GtkWidget *menu, gpointer data)
{
	GSList *mfl = g_object_get_data (G_OBJECT (menu), "mf");
	GSList *li;

	g_object_set_data (G_OBJECT (menu), "mf", NULL);

	for (li = mfl; li != NULL; li = li->next) {
		MenuFinfo *mf = li->data;
		li->data = NULL;

		destroy_mf (mf);
	}
	g_slist_free (mfl);
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

			panel_g_slist_deep_free (dirlist);
		}
	}
}

/* XXX: hmmm the gtk_menu_reposition only calls
   gtk_menu_position if the widget is drawable, but that's not the
   case when we want to do it*/
void
our_gtk_menu_position (GtkMenu *menu)
{
	GtkWidget *window = GTK_MENU_SHELL (menu)->active ?
		menu->toplevel : menu->tearoff_hbox;

	if (window != NULL) {
		/* HACK! reposition works even when not
		 * yet visible/mapped, as long as we have a window
		 * since is uses gtk_window_move.  Yes, we
		 * depend on how gtk works internally, but hey,
		 * who cares. */
		gboolean old_visible = GTK_WIDGET_VISIBLE (menu);
		gboolean old_mapped = GTK_WIDGET_MAPPED (menu);
		GTK_WIDGET_SET_FLAGS (menu, GTK_VISIBLE | GTK_MAPPED);
		gtk_menu_reposition (menu);
		if ( ! old_visible)
			GTK_WIDGET_UNSET_FLAGS (menu, GTK_VISIBLE);
		if ( ! old_mapped)
			GTK_WIDGET_UNSET_FLAGS (menu, GTK_MAPPED);
	}
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

static void
submenu_to_display (GtkWidget *menuw, gpointer data)
{
	GSList *mfl, *list;
	gboolean add_launcher_hack;

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

		mfl = g_object_get_data (G_OBJECT (menuw), "mf");

		g_object_set_data (G_OBJECT (menuw), "mf", NULL);
		for(list = mfl;
		    list != NULL;
		    list = list->next) {
			MenuFinfo *mf = list->data;
			list->data = NULL;

			menuw = create_menu_at_fr (menuw,
						   mf->fr,
						   mf->launcher_add,
						   mf->dir_name,
						   TRUE /*fake_submenus*/,
						   FALSE /*force*/);
			destroy_mf(mf);
		}
		g_slist_free(mfl);
	}

	our_gtk_menu_position(GTK_MENU(menuw));
}

static GtkWidget *
create_fake_menu_at (const char *menudir,
		     gboolean launcher_add,
		     const char *dir_name)
{	
	MenuFinfo *mf;
	GtkWidget *menu;
	GSList *list;
	
	menu = menu_new ();

	mf = g_new0 (MenuFinfo, 1);
	mf->menudir = g_strdup (menudir);
	mf->launcher_add = launcher_add;
	mf->dir_name = g_strdup (dir_name);
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
						   launcher_add,
						   itemname);

		else
			sub = create_menu_at_fr (NULL, fr,
						 launcher_add,
						 itemname,
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

	if (icon)
		panel_load_menu_image_deferred (
			menuitem, panel_menu_icon_get_size(), NULL, icon, fallback, FALSE);

	if (sub)
	        setup_full_menuitem (
			menuitem, panel_menu_icon_get_size (),
			NULL, itemname, NULL, mf);

	else 
	        setup_full_menuitem (
			menuitem, panel_menu_icon_get_size(),
			NULL, itemname, fr->name, mf);


	if(*add_separator) {
		add_menu_separator(menu);
		(*first_item)++;
		*add_separator = FALSE;
	}
	
	if (fr->comment && fr->comment [0])
		gtk_tooltips_set_tip (panel_tooltips, menuitem, fr->comment, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	if(!sub) {
		if (launcher_add)
			g_signal_connect (G_OBJECT (menuitem), "activate",
					    G_CALLBACK (add_launcher),
					    fr->name);
		else
			g_signal_connect (G_OBJECT (menuitem), "activate",
					    G_CALLBACK (activate_app_def),
					    fr->name);
	}

	g_free (itemname);

	return TRUE;
}

static GtkWidget *
create_menu_at (GtkWidget *menu,
		const char *menudir,
		gboolean launcher_add,
		const char *dir_name,
		gboolean fake_submenus,
		gboolean force)
{
	return create_menu_at_fr (menu,
				  fr_get_dir (menudir),
				  launcher_add,
				  dir_name,
				  fake_submenus,
				  force);
}

static GtkWidget *
create_menu_at_fr (GtkWidget *menu,
		   FileRec *fr,
		   gboolean launcher_add,
		   const char *dir_name,
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
	char *menubase;

	g_return_val_if_fail(!(fr&&fr->type!=FILE_REC_DIR),menu);
	
	if( ! force &&
	   fr == NULL)
		return menu;

	/* unfilled out, but the pointer will be correct */
	mf = g_new0 (MenuFinfo, 1);

	menubase = g_path_get_basename (fr->name);
	
	/*get this info ONLY if we haven't gotten it already*/
	if (dir_name == NULL)
		dir_name = (fr != NULL && fr->fullname != NULL)
			? fr->fullname
			: menubase;
	
	if (menu == NULL) {
		menu = menu_new ();

		g_signal_connect (G_OBJECT(menu), "destroy",
				    G_CALLBACK (menu_destroy), NULL);
	} else {
		first_item = g_list_length(GTK_MENU_SHELL(menu)->children);
		mfl = g_object_get_data (G_OBJECT (menu), "mf");
		if (GTK_MENU_SHELL(menu)->children != NULL)
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
						  launcher_add,
						  fake_submenus,
						  &add_separator,
						  &first_item,
						  mf))
				last_added = li;
		}
	}

	mf->launcher_add   = launcher_add;
	mf->dir_name       = g_strdup (dir_name);
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

	g_free (menubase);

	return menu;
}

static void
destroy_menu (GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	GtkWidget *prop_dialog = menu->prop_dialog;

	menu->prop_dialog = NULL;

	if (prop_dialog != NULL)
		gtk_widget_destroy (prop_dialog);

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
	GTK_BUTTON (menu->button)->in_button = FALSE;
	BUTTON_WIDGET (menu->button)->ignore_leave = FALSE;
	gtk_button_released (GTK_BUTTON (menu->button));
	menu->age = 0;
}

static const char *
applet_menu_get_category_icon (const char *untranslated_category)
{
	static GHashTable *hash = NULL;

	if (!hash) {
		hash = g_hash_table_new (g_str_hash, g_str_equal);
		g_hash_table_insert (hash, "Accessories", PANEL_STOCK_ACCESSORIES); 
		g_hash_table_insert (hash, "Amusements", PANEL_STOCK_AMUSEMENTS);
		g_hash_table_insert (hash, "Multimedia", PANEL_STOCK_MULTIMEDIA);
		g_hash_table_insert (hash, "Internet",   PANEL_STOCK_INTERNET);
		g_hash_table_insert (hash, "Utility",    PANEL_STOCK_UTILITY);
	}

	return g_hash_table_lookup (hash, untranslated_category);
}

static GtkWidget *
applet_menu_append (GtkWidget  *menu,
		    const char *name,
		    const char *description,
		    const char *stock_icon,
		    const char *icon)
{
	GtkWidget *menuitem;

	menuitem = gtk_image_menu_item_new ();

	if (icon || stock_icon)
		panel_load_menu_image_deferred (
			menuitem, GTK_ICON_SIZE_MENU, stock_icon, icon, NULL, FALSE);

	setup_full_menuitem (menuitem, GTK_ICON_SIZE_MENU, NULL, name, NULL, NULL);

	gtk_widget_show_all (menuitem);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	if (description)
		gtk_tooltips_set_tip (panel_tooltips, menuitem, description, NULL);

	return menuitem;
}

static void
add_bonobo_applet (GtkWidget  *widget,
		   const char *iid)
{
	PanelWidget *panel;
	PanelData *pd;
	int insertion_pos = -1;

	panel = menu_get_panel (widget);

	pd = g_object_get_data (G_OBJECT (panel->panel_parent), "PanelData");
	if (pd != NULL)
		insertion_pos = pd->insertion_pos;

	panel_applet_frame_load (iid, panel, insertion_pos, FALSE, NULL);
}

static const char applet_requirements [] = 
	"has_all (repo_ids, ['IDL:Bonobo/Control:1.0',"
	"		     'IDL:GNOME/Vertigo/PanelAppletShell:1.0']) && "
	"defined (panel:icon) && defined (panel:category)";

static char *applet_sort_criteria [] = {
	"panel:category",
	"name",
	NULL
	};

#ifdef FIXME /* Disabled for the 2.2.0 release. See bug #103159 */
typedef struct {
	Bonobo_ServerInfoList *applet_list;
	GtkMenuItem           *item;
	GtkWidget             *menu;
	int                    timeout_id;
} ReloadData;

static gboolean
Bonobo_ServerInfoList_equals (Bonobo_ServerInfoList *l1,
			      Bonobo_ServerInfoList *l2)
{
	CORBA_any         a1, a2;
	CORBA_Environment env;
	gboolean          retval;

	CORBA_exception_init (&env);

	a1._release = FALSE;
	a1._type    = TC_Bonobo_ServerInfoList;
	a1._value   = l1;

	a2._release = FALSE;
	a2._type    = TC_Bonobo_ServerInfoList;
	a2._value   = l2;

	retval = ORBit_any_equivalent (&a1, &a2, &env);
	if (BONOBO_EX (&env)) {
		g_warning ("comparison returned exception %s\n", BONOBO_EX_REPOID (&env));
		CORBA_exception_free (&env);
		return FALSE;
	}

	return retval;
}

static gboolean
recheck_applet_list (ReloadData *reload_data)
{
	Bonobo_ServerInfoList *applet_list;
	CORBA_Environment      env;

	if (GTK_WIDGET_VISIBLE (reload_data->menu))
		return TRUE;

	CORBA_exception_init (&env);

	applet_list = bonobo_activation_query (applet_requirements,
					       applet_sort_criteria,
					       &env);
	if (BONOBO_EX (&env)) {
		g_warning (_("query returned exception %s\n"), BONOBO_EX_REPOID (&env));
		CORBA_exception_free (&env);
		return TRUE;
	}

	CORBA_exception_free (&env);

	if (Bonobo_ServerInfoList_equals (applet_list, reload_data->applet_list)) {
		CORBA_free (applet_list);
		return TRUE;
	}

	gtk_widget_destroy (reload_data->menu);
	g_object_unref (reload_data->menu);

	reload_data->menu = menu_new ();
	gtk_menu_item_set_submenu (reload_data->item, reload_data->menu);

	CORBA_free (reload_data->applet_list);
	reload_data->applet_list = applet_list;

	make_add_submenu (reload_data->menu, applet_list);

	return TRUE;
}

static void
menu_item_destroyed (GtkMenuItem *item,
		     ReloadData  *reload_data)
{
	g_source_remove (reload_data->timeout_id);

	g_object_unref (reload_data->menu);

	CORBA_free (reload_data->applet_list);

	g_free (reload_data);
}

static Bonobo_ServerInfoList *
instrument_add_submenu_for_reload (GtkMenuItem *item,
				   GtkWidget   *menu)
{
	CORBA_Environment  env;
	ReloadData        *reload_data;

	reload_data = g_new0 (ReloadData, 1);

	CORBA_exception_init (&env);

	reload_data->applet_list =
		bonobo_activation_query (applet_requirements,
					 applet_sort_criteria,
					 &env);
	if (BONOBO_EX (&env)) {
		g_warning (_("query returned exception %s\n"), BONOBO_EX_REPOID (&env));

		CORBA_exception_free (&env);
		g_free (reload_data);

		return NULL;
	}


	reload_data->timeout_id =
		g_timeout_add (5 * 1000, (GSourceFunc) recheck_applet_list, reload_data);

	reload_data->item = item;
	reload_data->menu = g_object_ref (menu);

	g_signal_connect (item, "destroy",
			  G_CALLBACK (menu_item_destroyed), reload_data);

	CORBA_exception_free (&env);

	return reload_data->applet_list;
}
#else /* FIXME: Disabled for the 2.2.0 release. See bug #103159 */
static Bonobo_ServerInfoList *
instrument_add_submenu_for_reload (GtkMenuItem *item,
				   GtkWidget   *menu)
{
	Bonobo_ServerInfoList *retval;
	CORBA_Environment      env;

	CORBA_exception_init (&env);

	retval = bonobo_activation_query (applet_requirements,
					  applet_sort_criteria,
					  &env);
	if (BONOBO_EX (&env)) {
		g_warning (_("query returned exception %s\n"), BONOBO_EX_REPOID (&env));

		CORBA_exception_free (&env);

		return NULL;
	}

	CORBA_exception_free (&env);

	return retval;
}
#endif /* FIXME: Disabled for the 2.2.0 release. See bug #103159 */

static GtkWidget *
create_applets_menu (GtkWidget             *menu,
		     Bonobo_ServerInfoList *applet_list)
{
	GtkWidget         *prev_menu = NULL;
	const char        *prev_category = NULL;
	int                i;
	const GList       *langs_glist;
	GSList            *langs_gslist;

	if (!applet_list)
		return NULL;

	if (!menu) menu = menu_new ();

	g_signal_connect (G_OBJECT (menu), "destroy", G_CALLBACK (menu_destroy), NULL);

	/* Evil evil evil evil, we need to convert to
	 * a GSList from a GList */
	langs_glist = gnome_i18n_get_language_list ("LC_MESSAGES");
	langs_gslist = NULL;
	while (langs_glist != NULL) {
		langs_gslist = g_slist_append (langs_gslist, langs_glist->data);
		langs_glist = langs_glist->next;
	}

	for (i = 0; i < applet_list->_length; i++) {
		Bonobo_ServerInfo *info;
		GtkWidget         *menuitem;
		const char        *name;
		const char        *description;
		const char        *icon;
		const char        *category;
		const char        *untranslated_category;
		const char        *iid;

		info = &applet_list->_buffer [i];

		iid = info->iid;

		name     = bonobo_server_info_prop_lookup (info, "name", langs_gslist);
		category = bonobo_server_info_prop_lookup (info, "panel:category", langs_gslist);
	        description = bonobo_server_info_prop_lookup (info, "description", langs_gslist);

		icon = bonobo_server_info_prop_lookup (info, "panel:icon", NULL);
		untranslated_category =
			bonobo_server_info_prop_lookup (info, "panel:category", NULL);

		if (!name)
			continue;

		if (string_empty (category)) {
			applet_menu_append (menu, name, description, NULL, icon);
			continue;
		}

		if (!prev_category || strcmp (prev_category, untranslated_category)) {
			const char *cat_icon;

			prev_category = untranslated_category;
			prev_menu = menu_new ();

			cat_icon = applet_menu_get_category_icon (untranslated_category);

			menuitem = applet_menu_append (menu, category, NULL, cat_icon, NULL);

			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), prev_menu);
		}

		menuitem = applet_menu_append (prev_menu, name, description, NULL, icon);

		setup_applet_drag (menuitem, iid);

		g_signal_connect_data (menuitem, "activate", G_CALLBACK (add_bonobo_applet),
				       g_strdup (iid), (GClosureNotify) g_free, 0);
	}

	g_slist_free (langs_gslist);

	return menu;
}

static void
find_empty_pos_array (int screen,
		      int monitor,
		      int posscore[3][3])
{
	GSList *li;
	int i,j;
	PanelData *pd;
	BasePWidget *basep;
	
	int tx, ty;
	int w, h;
	gfloat sw, sw2, sh, sh2;

	if (foobar_widget_exists (screen, monitor)) {
		posscore[0][0] += 5;
		posscore[1][0] += 5;
		posscore[2][0] += 5;
	}

	sw2 = 2 * (sw = multiscreen_width (screen, monitor) / 3);
	sh2 = 2 * (sh = multiscreen_height (screen, monitor) / 3);
	
	for (li = panel_list; li != NULL; li = li->next) {
		pd = li->data;

		if (DRAWER_IS_WIDGET(pd->panel) ||
		    FOOBAR_IS_WIDGET (pd->panel))
			continue;

		basep = BASEP_WIDGET (pd->panel);
		
		if (basep->screen  != screen &&
		    basep->monitor != monitor)
			continue;

		basep_widget_get_pos (basep, &tx, &ty);
		tx -= multiscreen_x (screen, monitor);
		ty -= multiscreen_y (screen, monitor);
		basep_widget_get_size (basep, &w, &h);

		if (PANEL_WIDGET (basep->panel)->orient == GTK_ORIENTATION_HORIZONTAL) {
			j = MIN (ty / sh, 2);
			ty = tx + w;
			if (tx < sw) posscore[0][j]+=10;
			if (tx < sw2 && ty > sw) posscore[1][j]+=10;
			if (ty > sw2) posscore[2][j]+=10;
		} else {
			i = MIN (tx / sw, 2);
			tx = ty + h;
			if (ty < sh) posscore[i][0]+=10;
			if (ty < sh2 && tx > sh) posscore[i][1]+=10;
			if (tx > sh2) posscore[i][2]+=10;
		}
	}
}

static void
find_empty_pos (int     screen,
		int     monitor,
		gint16 *x,
		gint16 *y)
{
	int posscore[3][3] = { {1,2,0}, {1,4096,0}, {1,2,0}};
	int i, j, lowi= 0, lowj = 2;

	find_empty_pos_array (screen, monitor, posscore);

	for (j = 2; j >= 0; j--) {
		for (i = 0; i < 3; i++) {
			if (posscore[i][j] < posscore[lowi][lowj]) {
				lowi = i;
				lowj = j;
			}
		}
	}

	*x = ((float)lowi * multiscreen_width (screen, monitor)) / 2.0;
	*y = ((float)lowj * multiscreen_height (screen, monitor)) / 2.0;

	*x += multiscreen_x (screen, monitor);
	*y += multiscreen_y (screen, monitor);
}

static BorderEdge
find_empty_edge (int screen, int monitor)
{
	int posscore[3][3] = { {1,2,0}, {1,4096,0}, {1,2,0}};
	int escore [4] = { 0, 0, 0, 0};
	BorderEdge edge = BORDER_BOTTOM;
	int low=4096, i;

	find_empty_pos_array (screen, monitor, posscore);

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

static BorderEdge
find_empty_border_and_anchor (int            screen,
			      int            monitor,
			      SlidingAnchor *anchor)
{
	BorderEdge edge;
	int        posscore[3][3] = { {0, 4096, 0}, {4096, 4096, 4096}, {0, 4096, 0}};

	edge = find_empty_edge (screen, monitor);

	find_empty_pos_array (screen, monitor, posscore);

	switch (edge) {
	case BORDER_TOP:
		*anchor = (posscore [0][0] < posscore [2][0]) ? SLIDING_ANCHOR_LEFT : SLIDING_ANCHOR_RIGHT;
		break;
	case BORDER_BOTTOM:
		*anchor = (posscore [0][2] < posscore [2][2]) ? SLIDING_ANCHOR_LEFT : SLIDING_ANCHOR_RIGHT;
		break;
	case BORDER_LEFT:
		*anchor = (posscore [0][2] < posscore [0][0]) ? SLIDING_ANCHOR_RIGHT : SLIDING_ANCHOR_LEFT;
		break;
	case BORDER_RIGHT:
		*anchor = (posscore [2][0] < posscore [2][2]) ? SLIDING_ANCHOR_LEFT : SLIDING_ANCHOR_RIGHT;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return edge;
}

static void
create_new_panel (GtkWidget *w, gpointer data)
{
	PanelType  type = GPOINTER_TO_INT (data);
	PanelColor bcolor = { { 0, 0, 0, 0 }, 0xffff };
	gint16     x, y;
	GtkWidget *panel = NULL;
	int        screen;
	int        monitor;

	g_return_if_fail (type != DRAWER_PANEL);

	screen = gdk_screen_get_number (
			gtk_widget_get_screen (w));
	monitor = multiscreen_locate_widget (
			screen, GTK_WIDGET (menu_get_panel (w)));

	switch (type) {
	case ALIGNED_PANEL: 
		find_empty_pos (screen, monitor, &x, &y);
		panel = aligned_widget_new (NULL,
					    screen,
					    monitor,
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
		panel_save_to_gconf (panel_setup (panel));
		gtk_window_set_title (GTK_WINDOW (panel), _("Aligned Panel"));
		gtk_widget_show (panel);
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);

		panel_set_atk_name_desc (BASEP_WIDGET (panel)->panel,
					 _("Aligned Panel"),
					 _("GNOME Aligned Panel"));
		break;
	case EDGE_PANEL: 
		panel = edge_widget_new (NULL,
					 screen,
					 monitor,
					 find_empty_edge (screen, monitor),
					 BASEP_EXPLICIT_HIDE,
					 BASEP_SHOWN,
					 PANEL_SIZE_MEDIUM,
					 TRUE,
					 TRUE,
					 PANEL_BACK_NONE,
					 NULL,
					 TRUE, FALSE, TRUE,
					 &bcolor);
		panel_save_to_gconf (panel_setup (panel));
		gtk_window_set_title (GTK_WINDOW (panel), _("Edge Panel"));
		gtk_widget_show (panel);

		panel_set_atk_name_desc (BASEP_WIDGET (panel)->panel,
					 _("Edge Panel"),
					 _("GNOME Edge Panel"));
		break;
	case SLIDING_PANEL: {
		BorderEdge    edge;
		SlidingAnchor anchor;

		edge = find_empty_border_and_anchor (screen, monitor, &anchor);

		panel = sliding_widget_new (NULL,
					    screen,
					    monitor,
					    anchor, 0,
					    edge,
					    BASEP_EXPLICIT_HIDE,
					    BASEP_SHOWN,
					    PANEL_SIZE_MEDIUM,
					    TRUE, TRUE,
					    PANEL_BACK_NONE,
					    NULL, TRUE, FALSE, TRUE,
					    &bcolor);
		panel_save_to_gconf (panel_setup (panel));
		gtk_window_set_title (GTK_WINDOW (panel), _("Sliding Panel"));
		gtk_widget_show (panel);

		panel_set_atk_name_desc (BASEP_WIDGET (panel)->panel,
					 _("Sliding Panel"),
					 _("GNOME Sliding Panel"));
		}
		break;
	case FLOATING_PANEL:
		find_empty_pos (screen, monitor, &x, &y);
		panel = floating_widget_new (NULL,
					     screen,
					     monitor,
					     x, y,
					     GTK_ORIENTATION_VERTICAL,
					     BASEP_EXPLICIT_HIDE,
					     BASEP_SHOWN,
					     PANEL_SIZE_MEDIUM,
					     TRUE, TRUE,
					     PANEL_BACK_NONE,
					     NULL, TRUE, FALSE, TRUE,
					     &bcolor);
		panel_save_to_gconf (panel_setup (panel));
		gtk_window_set_title (GTK_WINDOW (panel), _("Floating Panel"));
		gtk_widget_show (panel);
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);

		panel_set_atk_name_desc (BASEP_WIDGET (panel)->panel,
					 _("Floating Panel"),
					 _("GNOME Floating Panel"));
		break;
	case FOOBAR_PANEL: {
		GtkWidget *dialog;

		if (!foobar_widget_exists (screen, monitor)) {
			const char *panel_id;
			
			panel = foobar_widget_new (NULL, screen, monitor);
			panel_id = PANEL_WIDGET (FOOBAR_WIDGET (panel)->panel)->unique_id;
			
			panel_save_to_gconf (panel_setup (panel));
			gtk_window_set_title (GTK_WINDOW (panel), _("Menu Panel"));
			gtk_widget_show (panel);
			break;
		}

		dialog = panel_error_dialog (
				gdk_screen_get_default (),
				"only_one_foobar",
				_("You can only have one menu panel at a time."));
		break;
	}
	default:
		break;
	}

	if (panel == NULL)
		return;
		
	panels_to_sync = TRUE;
	gtk_window_present (GTK_WINDOW (panel));
}

static void
foobar_item_showhide (GtkWidget *widget, gpointer data)
{
	GtkWidget   *menuitem = data;
	PanelWidget *panel;
	int          screen = 0;
	int          monitor = 0;

	panel = menu_get_panel (menuitem);
	if (panel) {
		screen = gdk_screen_get_number (
				gtk_widget_get_screen (GTK_WIDGET (panel)));
		monitor = multiscreen_locate_widget (
				screen, panel->panel_parent);
	}

	if (!foobar_widget_exists (screen, monitor))
		gtk_widget_show (menuitem);
	else
		gtk_widget_hide (menuitem);
}

static GtkWidget *
create_add_panel_submenu (void)
{
	GtkWidget *menu, *menuitem;

	menu = menu_new ();

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_CORNER_PANEL, _("C_orner Panel"));
	gtk_tooltips_set_tip (panel_tooltips, menuitem, 
			      _("Create corner panel"), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(create_new_panel),
			   GINT_TO_POINTER(ALIGNED_PANEL));

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_EDGE_PANEL, _("_Edge Panel"));
	gtk_tooltips_set_tip (panel_tooltips, menuitem, 
			      _("Create edge panel"), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(create_new_panel),
			   GINT_TO_POINTER(EDGE_PANEL));

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_FLOATING_PANEL, _("_Floating Panel"));
	gtk_tooltips_set_tip (panel_tooltips, menuitem, 
			      _("Create floating panel"), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(create_new_panel),
			   GINT_TO_POINTER(FLOATING_PANEL));

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_SLIDING_PANEL, _("_Sliding Panel"));
	gtk_tooltips_set_tip (panel_tooltips, menuitem, 
			      _("Create sliding panel"), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(create_new_panel),
			   GINT_TO_POINTER(SLIDING_PANEL));

	menuitem = add_menu_separator (menu);

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_MENU_PANEL, _("_Menu Panel"));
	gtk_tooltips_set_tip (panel_tooltips, menuitem, 
			      _("Create menu panel"), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (create_new_panel),
			  GINT_TO_POINTER (FOOBAR_PANEL));

	/* HACK, initial hide/show based on screen 0, monitor 0,
	 * this works most of the time and we get it correctly in
	 * the show/hide thingie below */
	if (foobar_widget_exists (0, 0))
		gtk_widget_hide (menuitem);

	g_signal_connect (G_OBJECT (menu), "show",
			  G_CALLBACK (foobar_item_showhide),
			  menuitem);

	return menu;
}

static void
setup_stock_menu_item (GtkWidget   *item,
		       GtkIconSize  icon_size,
		       const char  *stock_id,
		       const char  *title)
{
	if (stock_id)
		panel_load_menu_image_deferred (
			item, icon_size, stock_id, NULL, NULL, FALSE);

	setup_menuitem (item, icon_size, NULL, title);
}
	  
static GtkWidget *
create_system_menu (GtkWidget *menu, gboolean fake_submenus,
		    gboolean fake, gboolean launcher_add)
{
	if ( ! fake ||
	     menu != NULL) {
		menu = create_menu_at (menu, "applications:/",
				       launcher_add,
				       _("Applications"),
				       fake_submenus, FALSE);
	} else {
		menu = create_fake_menu_at ("applications:/",
					    launcher_add,
					    _("Applications"));
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

	if (!info)
		return NULL;

	if (!fake || menu)
		menu = create_menu_at (menu, info->menu_path,
				       launcher_add,
				       info->menu_name,
				       fake_submenus, FALSE);
	else
		menu = create_fake_menu_at (info->menu_path,
					    launcher_add,
					    info->menu_name);

	return menu;
}

static GtkWidget *
create_kde_menu (GtkWidget *menu, gboolean fake_submenus,
		 gboolean force, gboolean fake,
		 gboolean launcher_add)
{
	char *uri;

	uri = gnome_vfs_get_uri_from_local_path (kde_menudir);

	if (!fake || menu)
		menu = create_menu_at (menu, 
				       uri,
				       launcher_add,
				       _("KDE Menu"), 
				       fake_submenus,
				       force);
	else
		menu = create_fake_menu_at (uri,
					    launcher_add,
					    _("KDE Menu"));

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

static GtkWidget *
create_button_menu (void)
{
	GtkWidget *menuitem;
	GtkWidget *retval;
	
	retval = menu_new ();

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_LOGOUT, _("Log Out"));
	gtk_menu_shell_append (GTK_MENU_SHELL (retval), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (add_action_button_to_panel),
			  GINT_TO_POINTER (PANEL_ACTION_LOGOUT));
	setup_internal_applet_drag (menuitem, "ACTION:logout:NEW");
	
	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_LOCKSCREEN, _("Lock"));
	gtk_menu_shell_append (GTK_MENU_SHELL (retval), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (add_action_button_to_panel),
			  GINT_TO_POINTER (PANEL_ACTION_LOCK));
	setup_internal_applet_drag (menuitem, "ACTION:lock:NEW");

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_SCREENSHOT, _("Screenshot"));
	gtk_menu_shell_append (GTK_MENU_SHELL (retval), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (add_action_button_to_panel),
			  GINT_TO_POINTER (PANEL_ACTION_SCREENSHOT));
	setup_internal_applet_drag (menuitem, "ACTION:screenshot:NEW");

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_SEARCHTOOL, _("Search"));
	gtk_menu_shell_append (GTK_MENU_SHELL (retval), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (add_action_button_to_panel),
			  GINT_TO_POINTER (PANEL_ACTION_SEARCH));
	setup_internal_applet_drag (menuitem, "ACTION:search:NEW");

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_RUN, _("Run"));
	gtk_menu_shell_append (GTK_MENU_SHELL (retval), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (add_action_button_to_panel),
			  GINT_TO_POINTER (PANEL_ACTION_RUN));
	setup_internal_applet_drag (menuitem, "ACTION:run:NEW");

	return retval;
}	
		
static void
remove_panel_accept (GtkWidget *w,
		     int        response,
		     GtkWidget *panel)
{
	if (response == GTK_RESPONSE_OK) {

		/* Destroy the drawers button before destroying the drawer */
		if (DRAWER_IS_WIDGET (panel)) {
			PanelWidget *panel_widget = NULL;

			if (BASEP_IS_WIDGET (panel))
				panel_widget = PANEL_WIDGET (BASEP_WIDGET (panel)->panel);

			else if (FOOBAR_IS_WIDGET (panel))
				panel_widget = PANEL_WIDGET (FOOBAR_WIDGET (panel)->panel);

			if (panel_widget && panel_widget->master_widget) {
				AppletInfo *info;

				info = g_object_get_data (
						G_OBJECT (panel_widget->master_widget),
						"applet_info");
				((Drawer *) info->data)->drawer = NULL;
				panel_applet_clean (info, TRUE);

				g_assert (panel_widget->master_widget == NULL);
			}
		}

		panel_push_window_busy (w);

		if (BASEP_IS_WIDGET (panel))
			panel_remove_from_gconf (
				PANEL_WIDGET (BASEP_WIDGET (panel)->panel));

		else if (FOOBAR_IS_WIDGET (panel))
			panel_remove_from_gconf (
				PANEL_WIDGET (FOOBAR_WIDGET (panel)->panel));

		gtk_widget_destroy (panel);
		panel_pop_window_busy (w);
	}

	gtk_widget_destroy (w);
}

static void
remove_panel_query (GtkWidget *menuitem,
		    gpointer   data)
{
	PanelWidget *panel_widget;
	GtkWidget   *dialog;
	GtkWidget   *panel;

	panel_widget = menu_get_panel (menuitem);
	panel = panel_widget->panel_parent;

	if (!DRAWER_IS_WIDGET (panel) && base_panels == 1) {
		panel_error_dialog (
			menuitem_to_screen (menuitem),
			"cannot_remove_last_panel",
			_("You cannot remove your last panel."));
		return;
	}

	if (!global_config.confirm_panel_remove) {
		gtk_widget_destroy (panel);
		return;
	}

	dialog = gtk_message_dialog_new (
			NULL, 0 /* flags */,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			_("When a panel is deleted, the panel "
			"and its\n settings are lost. "
			"Delete this panel?"));
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_DELETE, GTK_RESPONSE_OK,
				NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Delete Panel"));

	gtk_window_set_wmclass (GTK_WINDOW (dialog),
				"panel_remove_query", "Panel");
	gtk_window_set_screen (GTK_WINDOW (dialog),
			       gtk_window_get_screen (GTK_WINDOW (panel)));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (remove_panel_accept),
			  panel);
	panel_signal_connect_object_while_alive (
			G_OBJECT (panel), "destroy",
			G_CALLBACK (gtk_widget_destroy),
			G_OBJECT (dialog));
	gtk_widget_show_all (dialog);
}

GtkWidget *
create_panel_root_menu (PanelWidget *panel)
{
	GtkWidget *menu;

	menu = create_root_menu (
			NULL, panel, TRUE, get_default_menu_flags (),
			BASEP_IS_WIDGET (panel->panel_parent),
			TRUE /* extra_items */);

	return menu;
}

static void
setup_remove_this_panel(GtkWidget *menu, GtkWidget *menuitem)
{
	PanelWidget *panel = menu_get_panel (menu);
	GtkWidget *label;

	g_assert(panel->panel_parent);

	if( ! DRAWER_IS_WIDGET(panel->panel_parent) &&
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
	if (panel->applet_list != NULL &&
	    global_config.confirm_panel_remove)
		gtk_label_set_text_with_mnemonic (
			GTK_LABEL (label), _("_Delete This Panel..."));
	else
		gtk_label_set_text_with_mnemonic (
			GTK_LABEL (label), _("_Delete This Panel"));
}

static void
current_panel_config(GtkWidget *w, gpointer data)
{
	PanelWidget *panel = menu_get_panel(w);
	GtkWidget *parent = panel->panel_parent;
	panel_config(parent);
}

static void
make_panel_submenu (GtkWidget *menu,
		    gboolean   show_properties)
{
	Bonobo_ServerInfoList *applet_list;
	GtkWidget             *menuitem, *submenu;

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			GTK_ICON_SIZE_MENU,
			gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU),
			_("_Add to Panel"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	submenu = menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   submenu);

	applet_list = instrument_add_submenu_for_reload (GTK_MENU_ITEM (menuitem), submenu);

	make_add_submenu (submenu, applet_list);

	menuitem = gtk_image_menu_item_new ();

	setup_menuitem (menuitem, 
			GTK_ICON_SIZE_MENU,
			gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU),
			_("_Delete This Panel"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT (menuitem), "activate",
			    G_CALLBACK (remove_panel_query),
			    NULL);
	g_signal_connect (G_OBJECT (menu), "show",
			    G_CALLBACK(setup_remove_this_panel),
			    menuitem);

	if (show_properties) {
		menuitem = gtk_image_menu_item_new ();
		setup_menuitem (menuitem,
				GTK_ICON_SIZE_MENU,
				gtk_image_new_from_stock (
					GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU),
				_("_Properties"));

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
		g_signal_connect (G_OBJECT (menuitem), "activate",
				    G_CALLBACK (current_panel_config), 
				    NULL);
	}

	add_menu_separator (menu);

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem, 
			GTK_ICON_SIZE_MENU,
			gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU),
			_("_New Panel"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   create_add_panel_submenu ());

	add_menu_separator (menu);
}

static void
show_panel_help (GtkWidget *w, gpointer data)
{
	panel_show_help (
		gtk_widget_get_screen (w), "wgospanel.xml", NULL);
}

GtkWidget *
create_panel_context_menu (PanelWidget *panel)
{
	GtkWidget *retval;
	GtkWidget *menuitem;
	char      *gnome_about;

	retval = menu_new ();

	if (!commie_mode)
		make_panel_submenu (retval, BASEP_IS_WIDGET (panel->panel_parent));

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			GTK_ICON_SIZE_MENU,
			gtk_image_new_from_stock (
				GTK_STOCK_HELP, GTK_ICON_SIZE_MENU),
			_("_Help"));
	gtk_menu_shell_append (GTK_MENU_SHELL (retval), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (show_panel_help), NULL);

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			GTK_ICON_SIZE_MENU,
			gtk_image_new_from_stock (
				GNOME_STOCK_ABOUT, GTK_ICON_SIZE_MENU),
			_("_About Panels"));
	gtk_menu_shell_append (GTK_MENU_SHELL (retval), menuitem);
	g_signal_connect (menuitem, "activate", G_CALLBACK (about_cb), NULL);
	
	gnome_about = g_find_program_in_path ("gnome-about");
	if (gnome_about) {
		add_menu_separator (retval);

		menuitem = gtk_image_menu_item_new ();
		setup_menuitem (menuitem,
				GTK_ICON_SIZE_MENU,
				gtk_image_new_from_stock (
					GNOME_STOCK_ABOUT, GTK_ICON_SIZE_MENU),
				_("About _GNOME"));
		gtk_menu_shell_append (GTK_MENU_SHELL (retval), menuitem);
		g_signal_connect_data (menuitem, "activate",
				       G_CALLBACK (about_gnome_cb),
				       gnome_about, (GClosureNotify) g_free, 0);
	}

	g_object_set_data (G_OBJECT (retval), "menu_panel", panel);

	return retval;
}

static void
ask_about_launcher_cb(GtkWidget *widget, gpointer data)
{
	PanelWidget *panel;
	PanelData *pd;
	int insertion_pos = -1;

	panel = menu_get_panel (widget);

	pd = g_object_get_data (G_OBJECT (panel->panel_parent), "PanelData");
	if (pd != NULL)
		insertion_pos = pd->insertion_pos;

	ask_about_launcher(NULL, panel, insertion_pos, FALSE);
}

static void
make_add_submenu (GtkWidget             *menu,
		  Bonobo_ServerInfoList *applet_list)
{
	GtkWidget *menuitem, *m;

	/* Add Menu */

	m = create_applets_menu (menu, applet_list);

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_LAUNCHER, _("Launcher..."));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(ask_about_launcher_cb),NULL);
	setup_internal_applet_drag(menuitem, "LAUNCHER:ASK");

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_LAUNCHER, _("Launcher from menu"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	m = create_add_launcher_menu (NULL, TRUE);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), m);
	g_signal_connect (G_OBJECT (m),"show",
			  G_CALLBACK (submenu_to_display), NULL);

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, NULL, _("Button"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	
	m = create_button_menu ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), m);
	g_signal_connect (G_OBJECT (m), "show",
			  G_CALLBACK (submenu_to_display), NULL);
	
  	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_GNOME_LOGO, _("Main Menu"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   G_CALLBACK(add_menu_to_panel),
			   NULL);
	setup_internal_applet_drag(menuitem, "MENU:MAIN");

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, GTK_ICON_SIZE_MENU, PANEL_STOCK_DRAWER, _("Drawer"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_drawer_to_panel,
			   NULL);
	setup_internal_applet_drag(menuitem, "DRAWER:NEW");
}

static GtkWidget *
create_desktop_menu (GtkWidget *menu, gboolean fake_submenus)
{
	GtkWidget *menuitem;
	/* Panel entry */

	if (menu == NULL) {
		menu = menu_new ();
	}

	if (panel_is_program_in_path ("xscreensaver")) {
		menuitem = gtk_image_menu_item_new ();
		setup_stock_menu_item (
			menuitem, panel_menu_icon_get_size (),
			PANEL_STOCK_LOCKSCREEN, _("Lock Screen"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (panel_action_lock_screen), NULL);
		setup_internal_applet_drag(menuitem, "ACTION:lock:NEW");
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      _("Lock the screen so that you can "
					"temporarily leave your computer"), NULL);
	}

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (
		menuitem, panel_menu_icon_get_size (), PANEL_STOCK_LOGOUT, _("Log Out"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (panel_action_logout), 0);
	setup_internal_applet_drag(menuitem, "ACTION:logout:NEW");
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
	setup_stock_menu_item (menuitem,
			       panel_menu_icon_get_size (),
			       distribution_info->stock_icon,
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

	panel_load_menu_image_deferred (
		menuitem, panel_menu_icon_get_size (), PANEL_STOCK_KDE, NULL, NULL, FALSE);

	setup_menuitem (menuitem, panel_menu_icon_get_size (), NULL, _("KDE Menu"));
	gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	g_signal_connect (G_OBJECT(menu),"show",
			  G_CALLBACK(submenu_to_display), NULL);
}

GtkWidget *
create_root_menu (GtkWidget   *root_menu,
		  PanelWidget *panel,
		  gboolean     fake_submenus,
		  int          flags,
		  gboolean     is_basep,
		  gboolean     extra_items)
{
	const DistributionInfo *distribution_info;
	GtkWidget              *menu;
	GtkWidget              *menuitem;
	gboolean                has_inline;
	gboolean                has_subs;
	gboolean                has_subs2;

	if ((flags & MAIN_MENU_KDE_SUB) && !got_kde_menus ())
		flags &= ~MAIN_MENU_KDE_SUB;

	if ((flags & MAIN_MENU_DISTRIBUTION_SUB) && !got_distro_menus ())
		flags &= ~MAIN_MENU_DISTRIBUTION_SUB;

	has_inline = (flags & (MAIN_MENU_SYSTEM |
			       MAIN_MENU_KDE));

	has_subs = (flags & (MAIN_MENU_SYSTEM_SUB |
			     MAIN_MENU_KDE_SUB));

	has_subs2 = (flags & (MAIN_MENU_DESKTOP_SUB |
			      MAIN_MENU_PANEL_SUB));

	if ((distribution_info = get_distribution_info ())) {
		has_inline |= (flags & (MAIN_MENU_DISTRIBUTION));
		has_subs |= (flags & (MAIN_MENU_DISTRIBUTION_SUB));
	}

	if (!root_menu)
		root_menu = menu_new ();

	g_object_set_data (G_OBJECT (root_menu), "menu_panel", panel);

	if (flags & MAIN_MENU_SYSTEM)
		create_system_menu(root_menu, fake_submenus,
				   FALSE /* fake */,
				   FALSE /* launcher_add */);

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
		setup_stock_menu_item (
			menuitem, panel_menu_icon_get_size (),
			PANEL_STOCK_GNOME_LOGO, _("Applications"));
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		if(menu) {
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
						   menu);
			g_signal_connect (G_OBJECT (menu),"show",
					  G_CALLBACK (submenu_to_display),
					  NULL);
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

	if ( ! no_run_box && extra_items) {
		add_menu_separator (root_menu);
		menuitem = gtk_image_menu_item_new ();
		setup_stock_menu_item (
			menuitem, panel_menu_icon_get_size (),
			PANEL_STOCK_RUN, _("Run Program..."));
		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (panel_action_run_program),
				  NULL);
		setup_internal_applet_drag (menuitem, "ACTION:run:NEW");
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      _("Run a command"), NULL);
	}

	if (extra_items &&
	    panel_is_program_in_path  ("gnome-search-tool")) {

		menuitem = gtk_image_menu_item_new ();
		setup_stock_menu_item (
			menuitem, panel_menu_icon_get_size (),
			PANEL_STOCK_SEARCHTOOL, _("Search for Files..."));
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      _("Find files, folders, and documents "
					"on your computer"),
				      NULL);
		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (panel_action_search), NULL);
		setup_internal_applet_drag (menuitem, "ACTION:search:NEW");

		panel_recent_append_documents_menu (root_menu);
		add_menu_separator (root_menu);
	}

	if (extra_items &&
	    panel_is_program_in_path ("gnome-panel-screenshot")) {
		menuitem = gtk_image_menu_item_new ();
		setup_stock_menu_item (
			menuitem, panel_menu_icon_get_size (),
			PANEL_STOCK_SCREENSHOT, _("Screenshot..."));
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      _("Take a screenshot of your desktop"),
				      NULL);
		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (panel_action_screenshot), NULL);
		setup_internal_applet_drag (menuitem, "ACTION:screenshot:NEW");
	}

	if (((has_inline && !has_subs) || has_subs) && has_subs2)
		add_menu_separator (root_menu);
	
	if (flags & MAIN_MENU_DESKTOP_SUB) {
		menu = create_desktop_menu (NULL, fake_submenus);
		menuitem = gtk_image_menu_item_new ();
		setup_stock_menu_item (
			menuitem, panel_menu_icon_get_size (),
			PANEL_STOCK_DESKTOP, _("Desktop"));
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	}

	if (flags & MAIN_MENU_DESKTOP)
		create_desktop_menu (root_menu, fake_submenus);

	return root_menu;
}

static char *
get_menu_tooltip (GtkWidget *menu)
{
	GString *tooltip;
	GSList *mfl;

	if (menu == NULL)
		return g_strdup (_("Menu"));

	tooltip = g_string_new (_("Menu:"));

	mfl = g_object_get_data (G_OBJECT (menu), "mf");
	while (mfl != NULL) {
		MenuFinfo *mf = mfl->data;

		g_string_append_c (tooltip, ' ');
		g_string_append (tooltip, mf->dir_name);

		mfl = mfl->next;
	}

	return g_string_free (tooltip, FALSE);
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

	if (menu->menu) {
		panel = menu_get_panel (menu->menu);
		g_object_unref (menu->menu);
		menu->menu = NULL;
	}

	if(panel == NULL) {
		g_warning ("Menu is seriously weird");
		return;
	}

	if (menu->main_menu) {
		int flags;
		if (menu->global_main)
			flags = get_default_menu_flags ();
		else
			flags = menu->main_menu_flags;
		menu->menu = create_root_menu (
				NULL, panel, fake_subs, flags,
				BASEP_IS_WIDGET (panel->panel_parent),
				TRUE /* extra_items */);

		gtk_tooltips_set_tip (panel_tooltips, menu->button,
				      _("Main Menu"), NULL);
	} else {
		char *tooltip;

		menu->menu = NULL;
		for(li = menudirl; li != NULL; li = li->next)
			menu->menu = create_menu_at (menu->menu, li->data,
						     FALSE /* launcher_add */,
						     NULL,
						     fake_subs, FALSE);

		tooltip = get_menu_tooltip (menu->menu);
		gtk_tooltips_set_tip (panel_tooltips, menu->button,
				      tooltip, NULL);
		g_free (tooltip);

		if(menu->menu == NULL) {
			int flags;
			if (menu->global_main)
				flags = get_default_menu_flags ();
			else
				flags = menu->main_menu_flags;
			g_warning(_("Can't create menu, using main menu!"));
			menu->menu = create_root_menu (
					NULL, panel, fake_subs, flags,
					BASEP_IS_WIDGET (panel->panel_parent),
					TRUE /* extra_items */);
			gtk_tooltips_set_tip (panel_tooltips, menu->button,
					      _("Main Menu"), NULL);
		}

	}

	/* sink the menu, none of this floating */
	g_object_ref (G_OBJECT (menu->menu));
	gtk_object_sink (GTK_OBJECT (menu->menu));

	g_signal_connect (G_OBJECT (menu->menu), "deactivate",
			    G_CALLBACK (menu_deactivate), menu);

	g_object_set_data (G_OBJECT (menu->menu), "menu_panel", panel);
}

static void 
menu_button_menu_popup (Menu    *menu,
			guint    button,
			guint32  activate_time)
{
	GtkWidget *panel;
	int        flags;

	if (menu->global_main)
		flags = get_default_menu_flags ();
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
		const DistributionInfo *distribution_info = get_distribution_info ();

		if(flags & MAIN_MENU_DISTRIBUTION &&
		   ! (flags & MAIN_MENU_DISTRIBUTION_SUB) &&
		   distribution_info != NULL &&
		   distribution_info->menu_show_func)
			distribution_info->menu_show_func(NULL, NULL);

		check_and_reread_applet (menu);
	}

	panel = get_panel_parent (menu->button);

	if (BASEP_IS_WIDGET (panel)) {
		BASEP_WIDGET (panel)->autohide_inhibit = TRUE;
		basep_widget_queue_autohide (BASEP_WIDGET (panel));
	}

	BUTTON_WIDGET(menu->button)->ignore_leave = TRUE;

	menu->age = 0;

	gtk_menu_set_screen (GTK_MENU (menu->menu),
			     panel_screen_from_toplevel (panel));

	gtk_menu_popup (GTK_MENU (menu->menu),
			NULL,
			NULL, 
			(GtkMenuPositionFunc) panel_position_applet_menu,
			menu->info->widget,
			button,
			activate_time);
}

static void
menu_button_pressed (GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	GdkEventButton *bevent = (GdkEventButton*)gtk_get_current_event();

	gtk_grab_remove(menu->button);
	menu_button_menu_popup (menu, bevent->button, bevent->time);
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

	foo = g_strdup_printf ("MENU:%d", panel_find_applet (widget));

	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)foo,
				strlen (foo));

	g_free (foo);
}

static void
menu_applet_reparented (GtkWidget *applet,
			GtkWidget *previous_panel,
			Menu      *menu)
{
	g_return_if_fail (menu != NULL && GTK_IS_MENU (menu->menu));

	panel_applet_menu_set_recurse (
		GTK_MENU (menu->menu), "menu_panel", applet->parent);
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
	menu->button = button_widget_new (pixmap_name, -1,
					  TRUE, orient);
	g_free (pixmap_name);
	if (!menu->button) {
		free_menu (menu);
		return NULL;
	}

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

	g_signal_connect (G_OBJECT (menu->button), "drag_data_get",
			    G_CALLBACK (drag_data_get_cb),
			    NULL);

	g_signal_connect_after (menu->button, "pressed",
				G_CALLBACK (menu_button_pressed), menu);

	g_signal_connect (menu->button, "clicked",
			  G_CALLBACK (menu_button_pressed), menu);
	g_signal_connect (menu->button, "destroy",
			  G_CALLBACK (destroy_menu), menu);
	g_signal_connect (menu->button, "parent_set",
			  G_CALLBACK (menu_applet_reparented), menu);

	gtk_widget_show (menu->button);

	/*if we are allowed to be pigs and load all the menus to increase
	  speed, load them*/
	if(global_config.keep_menus_in_memory) {
		GSList *list = g_slist_append(NULL, (gpointer)menudir);
		add_menu_widget (menu, panel, list, TRUE);
		g_slist_free(list);
	}

	if (main_menu) {
		gtk_tooltips_set_tip (panel_tooltips, menu->button,
				      _("Main Menu"), NULL);
	} else {
		char *tooltip = get_menu_tooltip (menu->menu);
		gtk_tooltips_set_tip (panel_tooltips, menu->button,
				      tooltip, NULL);
		g_free (tooltip);
	}

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
	g_return_if_fail (menu != NULL);

	button_widget_set_params (BUTTON_WIDGET (menu->button),
				  TRUE, orient);
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

		panel_applet_add_callback (info, "help", GTK_STOCK_HELP, _("_Help"));
	}
}

static GList *
find_in_load_list (GtkWidget *image)
{
	GList *li;
	for (li = icons_to_load; li != NULL; li = li->next) {
		IconToLoad *icon = li->data;
		if (icon->pixmap == image)
			return li;
	}
	return NULL;
}

static void
image_menu_shown (GtkWidget *image, gpointer data)
{
	IconToLoad *icon = data;

	/* if we've already handled this */
	if (gtk_image_get_storage_type (GTK_IMAGE (image)) != GTK_IMAGE_EMPTY)
		return;

	if (find_in_load_list (image) == NULL) {
		icons_to_load = g_list_append (icons_to_load,
					       icon_to_load_copy (icon));
	}
	if (load_icons_id == 0)
		load_icons_id = g_idle_add (load_icons_handler, NULL);
}

static void
image_menu_destroy (GtkWidget *image, gpointer data)
{
	image_menu_items = g_slist_remove (image_menu_items, image);
}

void
panel_load_menu_image_deferred (GtkWidget   *image_menu_item,
				GtkIconSize  icon_size,
				const char  *stock_id,
				const char  *image_filename,
				const char  *fallback_image_filename,
				gboolean     force_image)
{
	IconToLoad *icon;
	GtkWidget *image;
	int        icon_height = PANEL_DEFAULT_MENU_ICON_SIZE;

	icon = g_new (IconToLoad, 1);

	gtk_icon_size_lookup (icon_size, NULL, &icon_height);

	image = gtk_image_new ();
	image->requisition.width  = icon_height;
	image->requisition.height = icon_height;

	/* this takes over the floating ref */
	icon->pixmap = g_object_ref (G_OBJECT (image));
	gtk_object_sink (GTK_OBJECT (image));

	icon->stock_id       = stock_id;
	icon->image          = g_strdup (image_filename);
	icon->fallback_image = g_strdup (fallback_image_filename);
	icon->force_image    = force_image;
	icon->icon_size      = icon_size;

	gtk_widget_show (image);

	g_object_set_data_full (G_OBJECT (image_menu_item),
				"Panel:Image",
				g_object_ref (image),
				(GDestroyNotify) g_object_unref);
	if (force_image)
		g_object_set_data (G_OBJECT (image_menu_item),
				   "Panel:ForceImage",
				   GINT_TO_POINTER (TRUE));
 
	if (force_image || panel_menu_have_icons ())
		gtk_image_menu_item_set_image (
			GTK_IMAGE_MENU_ITEM (image_menu_item), image);

	g_signal_connect_data (image, "map",
			       G_CALLBACK (image_menu_shown), icon,
			       (GClosureNotify) icon_to_load_free, 0);

	g_signal_connect (image, "destroy",
			  G_CALLBACK (image_menu_destroy), NULL);

	image_menu_items = g_slist_prepend (image_menu_items, image);
}

void
menu_save_to_gconf (Menu       *menu,
		    const char *gconf_key)
{
	GConfClient *client;
	const char  *profile;
	const char  *temp_key;

	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "path");
	gconf_client_set_string (client, temp_key, menu->path, NULL);

	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "main-menu");
	gconf_client_set_bool (client, temp_key, menu->main_menu, NULL);

	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "global-main");
	gconf_client_set_bool (client, temp_key, menu->global_main, NULL);

	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "custom-icon");
	gconf_client_set_bool (client, temp_key, menu->custom_icon, NULL);

	if (menu->custom_icon && menu->custom_icon_file) {
		temp_key = panel_gconf_full_key (
				PANEL_GCONF_OBJECTS, profile, gconf_key, "custom-icon-file");
		gconf_client_set_string (client, temp_key, menu->custom_icon_file, NULL);
	}

	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "main-menu-flags");
	gconf_client_set_int (client, temp_key, menu->main_menu_flags, NULL);
}

void
menu_load_from_gconf (PanelWidget *panel_widget,
		      gint         position,
		      const char  *gconf_key)
{
	GConfClient *client;
	const char  *profile;
	const char  *temp_key;
	gboolean     main_menu;
	gboolean     global_main;
	gboolean     custom_icon;
	char        *path;
	char        *custom_icon_file = NULL;
	int          flags;

	g_return_if_fail (panel_widget != NULL);
	g_return_if_fail (gconf_key != NULL);

	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "path");
	path = gconf_client_get_string (client, temp_key, NULL);
	if (!path) {
		g_printerr (_("No path set at %s for panel menu object\n"), temp_key);
		return;
	}
        
	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "main-menu");
	main_menu = gconf_client_get_bool (client, temp_key, NULL);

	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "global-main");
	global_main = gconf_client_get_bool (client, temp_key, NULL);

	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "custom-icon");
	custom_icon = gconf_client_get_bool (client, temp_key, NULL);

	if (custom_icon) {
		temp_key = panel_gconf_full_key (
				PANEL_GCONF_OBJECTS, profile,
				gconf_key, "custom-icon-file");
		custom_icon_file = gconf_client_get_string (
					client, temp_key, NULL);
	}

	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "main-menu-flags");
	flags = gconf_client_get_int (client, temp_key, NULL);

	load_menu_applet (path, main_menu, flags, global_main,
			  custom_icon, custom_icon_file,
			  panel_widget, position, TRUE, gconf_key);

	g_free (path);
	g_free (custom_icon_file);
}

static gboolean
panel_menu_key_press_handler (GtkWidget   *widget,
			      GdkEventKey *event)
{
	gboolean retval = FALSE;

	if (event->keyval == GDK_F10 && event->state == GDK_SHIFT_MASK) {
		GtkMenuShell *menu_shell = GTK_MENU_SHELL (widget);

		retval = TRUE;
		if (menu_shell->active_menu_item &&
		    GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu == NULL) {
			ShowItemMenu* sim;
			GtkWidget *menu_item = menu_shell->active_menu_item;
 
			sim = 	g_object_get_data (G_OBJECT (menu_item), "sim");
			if (sim) {
				GdkEventButton bevent;

				bevent.button = 3;
				bevent.time = GDK_CURRENT_TIME;
				show_item_menu (menu_item, &bevent, sim);
			}
		}
		
	}
	return retval;
}
