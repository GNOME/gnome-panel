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

#include "button-widget.h"
#include "distribution.h"
#include "launcher.h"
#include "nothing.h"
#include "menu-fentry.h"
#include "menu-util.h"
#include "menu-ditem.h"
#include "panel-util.h"
#include "panel-gconf.h"
#include "panel.h"
#include "drawer.h"
#include "panel-config-global.h"
#include "panel-applet-frame.h"
#include "quick-desktop-reader.h"
#include "xstuff.h"
#include "panel-stock-icons.h"
#include "panel-action-button.h"
#include "panel-recent.h"
#include "panel-menu-bar.h"
#include "panel-compatibility.h"
#include "panel-multiscreen.h"
#include "panel-toplevel.h"
#include "panel-profile.h"
#include "panel-menu-button.h"
#include "panel-globals.h"
#include "panel-properties-dialog.h"
#include "panel-run-dialog.h"
#include "panel-lockdown.h"
#include "panel-addto.h"

#undef MENU_DEBUG

static char *gnome_folder = NULL;

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
				      const char *dir_name,
				      gboolean fake_submenus,
				      gboolean force);

static GtkWidget * create_desktop_menu (GtkWidget *m);

static void add_distribution_submenu (GtkWidget *root_menu,
				      gboolean fake_submenus);

static gboolean panel_menu_key_press_handler (GtkWidget   *widget,
					      GdkEventKey *event);
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

	/*just load the menus from disk, don't make the widgets
	  this just reads the .desktops of the top most directory
	  and a level down*/
	fr_read_dir (NULL, "applications:/", 0, 2);

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

static void
show_about_dialog (GtkWidget *menuitem)
{
	static GtkWidget *about = NULL;
	char             *authors [] = {
		"Alex Larsson (alexl@redhat.com)",
		"Anders Carlsson (andersca@gnu.org)",
		"Arvind Samptur (arvind.samptur@wipro.com)",
		"Darin Adler (darin@bentspoon.com)",
		"Elliot Lee (sopwith@redhat.com)",
		"Federico Mena (quartic@gimp.org)",
		"George Lebl (jirka@5z.com)",
		"Glynn Foster (glynn.foster@sun.com)",
		"Ian Main (imain@gtk.org)",
		"Ian McKellar <yakk@yakk.net>",
		"Jacob Berkman (jberkman@andrew.cmu.edu)",
		"Mark McLoughlin (mark@skynet.ie)",
		"Martin Baulig (baulig@suse.de)",
		"Miguel de Icaza (miguel@kernel.org)",
		"Owen Taylor (otaylor@redhat.com)",
		"Padraig O'Briain (padraig.obriain@sun.com)",
		"Seth Nickell (snickell@stanford.edu)",
		"Stephen Browne (stephen.browne@sun.com)",
		"Tom Tromey (tromey@cygnus.com)",
		"Vincent Untz (vincent@vuntz.net)",
		N_("And many, many others..."),
		NULL
	};
	char *documenters[] = {
	        "Alexander Kirillov (kirillov@math.sunysb.edu)",
		"Dan Mueth (d-mueth@uchicago.edu)",
		"Dave Mason (dcm@redhat.com)",
		NULL
	  };
	/* Translator credits */
	char *translator_credits = _("translator_credits");
	int   i;

	if (about) {
		gtk_window_set_screen (
			GTK_WINDOW (about), menuitem_to_screen (menuitem));
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	for (i = 0; authors [i]; i++)
		authors [i] = _(authors [i]);

	about = gnome_about_new (_("The GNOME Panel"),
				 VERSION,
				 "Copyright \xc2\xa9 1997-2003 Free Software Foundation, Inc.",
				 _("This program is responsible for launching other "
				   "applications and embedding small applets within itself."),
				 (const char **) authors,
				 (const char **) documenters,
				 strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
				 NULL);

	gtk_window_set_wmclass (GTK_WINDOW (about), "about_dialog", "Panel");
	gtk_window_set_screen (GTK_WINDOW (about),
			       menuitem_to_screen (menuitem));
	g_signal_connect (about, "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &about);
	g_signal_connect (about, "event",
			  G_CALLBACK (check_for_screen), NULL);

	gtk_widget_show (about);
}

static void
about_gnome_cb (GtkWidget *menuitem,
		char      *program_path)
{
	GdkScreen *screen = menuitem_to_screen (menuitem);
	GError    *error = NULL;

	if (!gdk_spawn_command_line_on_screen (screen, program_path, &error)) {
		panel_error_dialog (screen,
				    "cannot_exec_about_gnome",
				    _("Cannot execute '%s'"),
				    _("%s: %s"),
				    _("About GNOME"),
				    program_path, error->message);
		g_error_free (error);
	}
}

static void
activate_app_def (GtkWidget  *menuitem,
		  const char *item_loc)
{
	GError           *error = NULL;
	GnomeDesktopItem *item;

	item = gnome_desktop_item_new_from_uri (
			item_loc,
			0,
			&error);

	if (item) {
		panel_ditem_launch (
			item, NULL, 0, menuitem_to_screen (menuitem), &error);
		if (error) {
			panel_error_dialog (menuitem_to_screen (menuitem),
					    "cannot_launch_entry",
					    _("Cannot launch entry"),
					    "%s",
					    error->message);

			g_clear_error (&error);
		}
		gnome_desktop_item_unref (item);
	} else {
		g_assert (error != NULL);

		panel_error_dialog (menuitem_to_screen (menuitem),
				    "cannot_load_entry",
				    _("Cannot load entry"),
				    "%s",
				    error->message);

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

	return gtk_window_get_screen (GTK_WINDOW (panel_widget->toplevel));
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
	GdkScreen      *screen;
	int             monitor;
	int             monitor_width;
	int             monitor_height;
	int             monitor_basex;
	int             monitor_basey;

	gtk_widget_get_child_requisition (GTK_WIDGET (menu), &req);

	screen  = gtk_widget_get_screen (GTK_WIDGET (menu));
	monitor = gdk_screen_get_monitor_at_point (screen, *x, *y);

	monitor_width  = panel_multiscreen_width (screen, monitor);
	monitor_height = panel_multiscreen_height (screen, monitor);
	monitor_basex  = panel_multiscreen_x (screen, monitor);
	monitor_basey  = panel_multiscreen_y (screen, monitor);

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
panel_create_menu (void)
{
	GtkWidget       *retval;
	static gboolean  registered_icon_theme_changer = FALSE;

	if (!registered_icon_theme_changer) {
		registered_icon_theme_changer = TRUE;

		g_signal_connect (panel_icon_theme, "changed",
				  G_CALLBACK (icon_theme_changed), NULL);
	}
	
	retval = gtk_menu_new ();
	
	panel_gconf_notify_add_while_alive ("/desktop/gnome/interface/menus_have_icons",
					    (GConfClientNotifyFunc) menus_have_icons_changed,
					    G_OBJECT (retval));

	g_signal_connect_after (retval, "show",
				G_CALLBACK (panel_make_sure_menu_within_screen),
				NULL);

	g_signal_connect (retval, "key_press_event",
			  G_CALLBACK (panel_menu_key_press_handler),
			  NULL);

	return retval;
}

static GtkWidget *
menu_new (void)
{
	GtkWidget *retval;

	retval = panel_create_menu ();

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
	GtkWidget *dialog;
	PanelWidget *panel;

	g_return_if_fail (sim->mf != NULL);

	panel = menu_get_panel (sim->menuitem);

	dialog = panel_new_launcher (sim->mf->menudir,
			    	     menuitem_to_screen (sim->menuitem));

	panel_widget_register_open_dialog (panel, dialog);
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

		panel_error_dialog (menuitem_to_screen (sim->menuitem),
				    "cannot_remove_menu_item",
				    _("Cannot remove menu item %s"),
				    "%s",
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
add_app_to_panel (GtkWidget    *item,
		  ShowItemMenu *sim)
{
	PanelWidget   *panel_widget;
	PanelToplevel *toplevel;
	PanelData     *pd;
	int            position;

	panel_widget = menu_get_panel (item);
	toplevel = panel_widget->toplevel;

	pd = g_object_get_data (G_OBJECT (toplevel), "PanelData");
	position = pd ?  pd->insertion_pos : -1;

	panel_launcher_create (toplevel, position, sim->item_loc);
}


static inline gboolean
vfs_text_uri_exists (const char *text_uri)
{
	GnomeVFSURI *uri;
	gboolean     retval;

	uri = gnome_vfs_uri_new (text_uri);
	retval = gnome_vfs_uri_exists (uri);
	gnome_vfs_uri_unref (uri);

	return retval;
}

static void
add_drawers_from_dir (const char  *dirname,
		      const char  *name,
		      int          pos,
		      const char  *toplevel_id)
{
	QuickDesktopItem *item_info;
	GSList           *mfiles, *l;
	const char       *subdir_name;
	char             *dentry_name;
	char             *pixmap_name;
	char             *uri;
	char             *attached_toplevel_id;

	dentry_name = g_build_path ("/", dirname, ".directory", NULL);
	item_info = quick_desktop_item_load_uri (dentry_name, FALSE);
	g_free (dentry_name);

	subdir_name = name ? name : (item_info ? item_info->name : NULL);
	pixmap_name = item_info ? item_info->icon : NULL;

	attached_toplevel_id = panel_drawer_create_with_id (toplevel_id,
							    pos,
							    pixmap_name,
							    pixmap_name != NULL,
							    subdir_name);
	if (!attached_toplevel_id)
		return;

	mfiles = get_mfiles_from_menudir (dirname, NULL);
	for (l = mfiles; l; l = l->next) {
		MFile *mfile = l->data;

		uri = g_build_filename (dirname, mfile->name, NULL);

		if (mfile->verified) {
			if (mfile->is_dir)
				add_drawers_from_dir (uri, NULL, G_MAXINT/2, attached_toplevel_id);

			else if ((g_str_has_suffix (mfile->name, ".desktop") ||
				  g_str_has_suffix (mfile->name, ".kdelnk")) &&
				 vfs_text_uri_exists (uri)) {
					
					QuickDesktopItem *qitem;

					qitem = quick_desktop_item_load_uri (uri, TRUE);

					/* Don't add the desktop entry if TryExec fails */
					if (qitem != NULL) {
						panel_launcher_create_with_id (
									   attached_toplevel_id, 
									   G_MAXINT/2, uri);
						quick_desktop_item_destroy (qitem);
					}
			}
		}

		g_free (uri);
	}

	free_mfile_list (mfiles);
	g_free (attached_toplevel_id);
}

static void
add_menudrawer_to_panel (GtkWidget *menuitem,
			 MenuFinfo *mf)

{
	PanelWidget *panel;
	PanelData   *pd;
	int          insertion_pos;

	g_return_if_fail (mf != NULL);
	
	panel = menu_get_panel (menuitem);

	pd = g_object_get_data (G_OBJECT (panel->toplevel), "PanelData");
	insertion_pos = pd ? pd->insertion_pos : -1;

	add_drawers_from_dir (mf->menudir,
			      mf->dir_name,
			      insertion_pos,
			      panel_profile_get_toplevel_id (panel->toplevel));
}

static void
add_menu_to_panel (GtkWidget  *widget,
		   MenuFinfo  *menu_info)
{
	PanelWidget   *panel_widget;
	PanelToplevel *toplevel;
	PanelData     *pd;
	int            position;
	char          *menu_path;
	gboolean       use_menu_path;
	char          *tooltip;

	panel_widget = menu_get_panel (widget);
	toplevel = panel_widget->toplevel;

	pd = g_object_get_data (G_OBJECT (toplevel), "PanelData");
	position = pd ? pd->insertion_pos : -1;

	if (!menu_info) {
		menu_path     = NULL;
		use_menu_path = FALSE;
		tooltip       = _("Main Menu");
	} else {
		menu_path     = menu_info->menudir;
		use_menu_path = TRUE;
		tooltip       = menu_info->dir_name ? menu_info->dir_name :
						      _("Menu");
	}

	panel_menu_button_create (toplevel, position, menu_path,
				  use_menu_path, tooltip);
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
          {
	    if (gdk_pointer_grab (xgrab_shell->window, TRUE,
				  GDK_BUTTON_PRESS_MASK |
				  GDK_BUTTON_RELEASE_MASK |
				  GDK_ENTER_NOTIFY_MASK |
				  GDK_LEAVE_NOTIFY_MASK,
				  NULL, NULL, 0) == 0)
              {
		if (gdk_keyboard_grab (xgrab_shell->window, TRUE,
				       GDK_CURRENT_TIME) == 0)
		  GTK_MENU_SHELL (xgrab_shell)->have_xgrab = TRUE;
		else
		  gdk_pointer_ungrab (GDK_CURRENT_TIME);
	      }
         }
	
	gtk_grab_add (GTK_WIDGET (menu));
}

static void
edit_dentry (GtkWidget    *widget,
	     ShowItemMenu *sim)
{
	GtkWidget *dialog;
	PanelWidget *panel;
	char *dir = NULL;

	g_return_if_fail (sim != NULL);
	g_return_if_fail (sim->item_loc != NULL);

	if (sim->mf)
		dir = sim->mf->menudir;

	panel = menu_get_panel (sim->menuitem);

	dialog = panel_edit_dentry (sim->item_loc, dir,
			   	    menuitem_to_screen (sim->menuitem));

	panel_widget_register_open_dialog (panel, dialog);
}

static void
edit_direntry (GtkWidget    *widget,
	       ShowItemMenu *sim)
{
	GtkWidget *dialog;
	PanelWidget *panel;

	g_return_if_fail (sim != NULL);
	g_return_if_fail (sim->mf != NULL);

	panel = menu_get_panel (sim->menuitem);

	dialog = panel_edit_direntry (sim->mf->menudir,
			     	      sim->mf->dir_name,
				      menuitem_to_screen (sim->menuitem));

	panel_widget_register_open_dialog (panel, dialog);
	
}

static gboolean
show_item_menu (GtkWidget      *item,
		GdkEventButton *bevent,
		ShowItemMenu   *sim)
{
	GtkWidget        *menuitem;
	GnomeDesktopItem *ii;
	PanelWidget      *panel_widget;

	if (panel_lockdown_get_locked_down ())
		return FALSE;

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
			gboolean objects_writable = panel_profile_list_is_writable (PANEL_GCONF_OBJECTS);

			ii = gnome_desktop_item_new_from_uri (sim->item_loc, 0, NULL);

			/* eek */
			if (ii == NULL) {
				g_object_unref (G_OBJECT (sim->menu));
				sim->menu = NULL;
				return TRUE;
			}

			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, panel_menu_icon_get_size (),
					NULL, _("Add this launcher to panel"), FALSE);
			gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu), menuitem);
			g_signal_connect (menuitem, "activate",
					  G_CALLBACK (add_app_to_panel),
					  sim);
			gtk_widget_set_sensitive (menuitem, objects_writable);

			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, panel_menu_icon_get_size (),
					NULL, _("Remove this item"), FALSE);
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

			if (!panel_lockdown_get_disable_command_line ()) {
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
						NULL, _("_Properties"), FALSE);
				gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu), menuitem);
			}

			gnome_desktop_item_unref (ii);
		}
		
		if (sim->mf != NULL) {
			GtkWidget *submenu;
			gboolean objects_writable = panel_profile_list_is_writable (PANEL_GCONF_OBJECTS);
			gboolean toplevels_writable = panel_profile_list_is_writable (PANEL_GCONF_TOPLEVELS);

			if (sim->type == 0) {
				submenu = sim->menu;
			} else {
				submenu = menu_new ();

				g_object_set_data (
					G_OBJECT (submenu), "menu_panel", panel_widget);

				menuitem = gtk_image_menu_item_new ();
				setup_menuitem (menuitem, panel_menu_icon_get_size (),
						NULL, _("Entire menu"), FALSE);
				gtk_menu_shell_append (GTK_MENU_SHELL (sim->menu), menuitem);
				gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
							   submenu);
			}

			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, panel_menu_icon_get_size (), NULL,
					_("Add this as drawer to panel"), FALSE);
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
			g_signal_connect (G_OBJECT(menuitem), "activate",
					  G_CALLBACK(add_menudrawer_to_panel),
						  sim->mf);
			gtk_widget_set_sensitive (menuitem,
						  objects_writable &&
						  toplevels_writable);

			menuitem = gtk_image_menu_item_new ();
			setup_menuitem (menuitem, panel_menu_icon_get_size (),
					NULL, _("Add this as menu to panel"), FALSE);
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
			g_signal_connect (G_OBJECT(menuitem), "activate",
					  G_CALLBACK(add_menu_to_panel),
					  sim->mf);
			gtk_widget_set_sensitive (menuitem,
						  objects_writable &&
						  toplevels_writable);

			if (!panel_lockdown_get_disable_command_line ()) {
				menuitem = gtk_image_menu_item_new ();
				setup_menuitem (menuitem, panel_menu_icon_get_size (),
						NULL, _("Add new item to this menu"), FALSE);
				gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
				/*when activated we must pop down the first menu*/
				g_signal_connect_swapped (G_OBJECT (menuitem),
							  "activate",
							  G_CALLBACK (gtk_menu_shell_deactivate),
							  G_OBJECT (item->parent));

				g_signal_connect (menuitem, "activate",
						  G_CALLBACK (add_new_app_to_menu),
						  sim);
				if (!panel_is_uri_writable (sim->mf->menudir))
					gtk_widget_set_sensitive (menuitem, FALSE);
			}

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
					NULL, _("Properties"), FALSE);
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
		}
	}

	gtk_menu_set_screen (
		GTK_MENU (sim->menu),
		gtk_window_get_screen (GTK_WINDOW (panel_widget->toplevel)));

	gtk_menu_popup (GTK_MENU (sim->menu),
			NULL,
			NULL,
			menu_item_menu_position,
			item,
			bevent->button,
			bevent->time);

	return TRUE;
}

gboolean
menu_dummy_button_press_event (GtkWidget      *menuitem,
				   GdkEventButton *event,
				   gpointer	 data)
{
	if (event->button == 3)
		return TRUE;

	return FALSE;
}

static gboolean
menuitem_button_press_event (GtkWidget      *menuitem,
			     GdkEventButton *event,
			     ShowItemMenu   *sim)
{
	if (event->button == 3)
		return show_item_menu (menuitem, event, sim);
	
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
  if (panel_global_config_get_tooltips_enabled ())
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
setup_mnemonic (GtkWidget *menuitem,
		GtkWidget *old_toplevel,
		gpointer   keyval_as_pointer)
{
	GtkWidget *toplevel;
	guint      keyval;

	keyval = GPOINTER_TO_UINT (keyval_as_pointer);

	if (old_toplevel != NULL)
		gtk_window_remove_mnemonic (GTK_WINDOW (old_toplevel),
					    keyval,
					    GTK_WIDGET (menuitem));

	toplevel = gtk_widget_get_toplevel (menuitem);
	if (GTK_WIDGET_TOPLEVEL (toplevel))
		gtk_window_add_mnemonic (GTK_WINDOW (toplevel),
					 keyval,
					 GTK_WIDGET (menuitem));
}

static void
setup_invisible_mnemonic (GtkWidget *menuitem,
			  char       unicode_char)
{
	guint accel_key;

	accel_key = gdk_keyval_to_lower (gdk_unicode_to_keyval (unicode_char));

	g_signal_connect (menuitem, "hierarchy_changed", 
			  G_CALLBACK (setup_mnemonic),
			  GUINT_TO_POINTER (accel_key));

	setup_mnemonic (menuitem, NULL, GUINT_TO_POINTER (accel_key));
}

static void
setup_full_menuitem (GtkWidget   *menuitem,
		     GtkIconSize  icon_size,
		     GtkWidget   *image,
		     const char  *title,
		     gboolean     invisible_mnemonic,
		     const char  *item_loc,
		     MenuFinfo   *mf)
			       
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "text/uri-list", 0, 0 }
	};

	GtkWidget *label;

	if (invisible_mnemonic)
		label = gtk_label_new (title);
	else
		label = gtk_label_new_with_mnemonic (title);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
       
	if (invisible_mnemonic && title)
		setup_invisible_mnemonic (menuitem, title [0]);
	
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

		if (!panel_lockdown_get_locked_down ()) {
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
		const char  *title,
		gboolean     invisible_mnemonic)
{
	setup_full_menuitem (menuitem, icon_size, image, title, invisible_mnemonic, NULL, NULL);
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


void
setup_internal_applet_drag (GtkWidget *menuitem, const char *applet_type)
{
	static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};

	if (!applet_type || panel_lockdown_get_locked_down ())
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

	/*this no longer constitutes a bad hack, now it's purely cool :)*/
	if( ! menu_need_reread(menuw))
		return;

	/* Note this MUST be destroy and not unref, unref would fuck
	 * up here, we don't hold a reference to them, so we must
	 * destroy them, menu shell will unref these */
	while(GTK_MENU_SHELL(menuw)->children)
		gtk_widget_destroy(GTK_MENU_SHELL(menuw)->children->data);

	mfl = g_object_get_data (G_OBJECT (menuw), "mf");
	g_object_set_data (G_OBJECT (menuw), "mf", NULL);

	for(list = mfl;
	    list != NULL;
	    list = list->next) {
		MenuFinfo *mf = list->data;
		list->data = NULL;

		menuw = create_menu_at_fr (menuw,
					   mf->fr,
					   mf->dir_name,
					   TRUE /*fake_submenus*/,
					   FALSE /*force*/);
		destroy_mf(mf);
	}
	g_slist_free(mfl);

	our_gtk_menu_position(GTK_MENU(menuw));
}

static GtkWidget *
create_fake_menu_at (const char *menudir,
		     const char *dir_name)
{	
	MenuFinfo *mf;
	GtkWidget *menu;
	GSList *list;
	
	menu = menu_new ();

	mf = g_new0 (MenuFinfo, 1);
	mf->menudir = g_strdup (menudir);
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
		 gboolean fake_submenus,
		 gboolean *add_separator,
		 int *first_item,
		 MenuFinfo *mf)
{
	GtkWidget  *menuitem, *sub;
	const char *icon, *fallback;
	char       *itemname;
	
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

	if (fr->type == FILE_REC_DIR) {
		if (!gnome_folder)
			gnome_folder = gnome_icon_theme_lookup_icon (
						panel_icon_theme,
                                               "gnome-fs-directory",
                                               48, NULL, NULL);

		if (!icon)
			icon = gnome_folder;
		else
			fallback = gnome_folder;

		if(fake_submenus)
			sub = create_fake_menu_at (fr->name,
						   itemname);

		else
			sub = create_menu_at_fr (NULL, fr,
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
		g_signal_connect (sub, "button_press_event",
				  G_CALLBACK (menu_dummy_button_press_event), NULL);
	}

	if (icon)
		panel_load_menu_image_deferred (
			menuitem, panel_menu_icon_get_size(), NULL, icon, fallback, FALSE);

	if (sub)
	        setup_full_menuitem (
			menuitem, panel_menu_icon_get_size (),
			NULL, itemname, TRUE, NULL, mf);

	else 
	        setup_full_menuitem (
			menuitem, panel_menu_icon_get_size(),
			NULL, itemname, TRUE, fr->name, mf);

	if(*add_separator) {
		add_menu_separator(menu);
		(*first_item)++;
		*add_separator = FALSE;
	}
	
	if (fr->comment && fr->comment [0])
		gtk_tooltips_set_tip (panel_tooltips, menuitem, fr->comment, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	if(!sub)
		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (activate_app_def),
				  fr->name);

	g_free (itemname);

	return TRUE;
}

GtkWidget *
create_menu_at (GtkWidget *menu,
		const char *menudir,
		const char *dir_name,
		gboolean fake_submenus,
		gboolean force)
{
	return create_menu_at_fr (menu,
				  fr_get_dir (menudir),
				  dir_name,
				  fake_submenus,
				  force);
}

static GtkWidget *
create_menu_at_fr (GtkWidget *menu,
		   FileRec *fr,
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
						  fake_submenus,
						  &add_separator,
						  &first_item,
						  mf))
				last_added = li;
		}
	}

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
create_new_panel (GtkWidget *menuitem)
{
	panel_profile_create_toplevel (gtk_widget_get_screen (menuitem));
}

void
setup_stock_menu_item (GtkWidget   *item,
		       GtkIconSize  icon_size,
		       const char  *stock_id,
		       const char  *title,
		       gboolean     invisible_mnemonic)
{
	if (stock_id)
		panel_load_menu_image_deferred (
			item, icon_size, stock_id, NULL, NULL, FALSE);

	setup_menuitem (item, icon_size, NULL, title, invisible_mnemonic);
}
	  
static GtkWidget *
create_system_menu (GtkWidget *menu,
		    gboolean fake_submenus,
		    gboolean fake)
{
	if ( ! fake ||
	     menu != NULL) {
		menu = create_menu_at (menu, "applications:/",
				       _("Applications"),
				       fake_submenus, FALSE);
	} else {
		menu = create_fake_menu_at ("applications:/",
					    _("Applications"));
	}

	return menu;
}

static GtkWidget *
create_distribution_menu (GtkWidget *menu,
			  gboolean fake_submenus,
			  gboolean fake)
{
	const DistributionInfo *info = get_distribution_info ();

	if (!info)
		return NULL;

	if (!fake || menu)
		menu = create_menu_at (menu, info->menu_path,
				       info->menu_name,
				       fake_submenus, FALSE);
	else
		menu = create_fake_menu_at (info->menu_path,
					    info->menu_name);

	return menu;
}

static void
remove_panel (GtkWidget *menuitem,
              gpointer data)
{
	PanelWidget   *panel_widget;
	PanelToplevel *toplevel;

	panel_widget = menu_get_panel (menuitem);
	toplevel     = panel_widget->toplevel;

	if (panel_toplevel_is_last_unattached (toplevel)) {
		panel_error_dialog (menuitem_to_screen (menuitem),
				    "cannot_remove_last_panel",
				    _("You cannot remove your last panel."),
				    NULL);
		return;
	}

        panel_delete (toplevel);
}

GtkWidget *
create_panel_root_menu (PanelWidget *panel)
{
	GtkWidget *menu;

	menu = create_root_menu (
			NULL, panel, TRUE, get_default_menu_flags (), TRUE);

	return menu;
}

static void
setup_remove_this_panel (GtkWidget *menu,
			 GtkWidget *menuitem)
{
	PanelWidget *panel_widget;
	GtkWidget   *label;
	gboolean     sensitive;

	panel_widget = menu_get_panel (menu);

	g_assert (PANEL_IS_TOPLEVEL (panel_widget->toplevel));

	sensitive =
		panel_toplevel_is_last_unattached (panel_widget->toplevel) ||
		!panel_lockdown_get_locked_down () ||
		!panel_profile_list_is_writable (PANEL_GCONF_TOPLEVELS);

	gtk_widget_set_sensitive (menuitem, sensitive);

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
	if (panel_widget->applet_list &&
	    panel_global_config_get_confirm_panel_remove ())
		gtk_label_set_text_with_mnemonic (
			GTK_LABEL (label), _("_Delete This Panel..."));
	else
		gtk_label_set_text_with_mnemonic (
			GTK_LABEL (label), _("_Delete This Panel"));
}

static void
make_panel_submenu (PanelWidget *panel_widget,
		    GtkWidget   *menu)
{
	GtkWidget             *menuitem;

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			GTK_ICON_SIZE_MENU,
			gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU),
			_("_Add to Panel..."),
			FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

        g_signal_connect (G_OBJECT (menuitem), "activate",
	      	       	  G_CALLBACK (panel_addto_present), panel_widget);

	if (!panel_profile_list_is_writable (PANEL_GCONF_APPLETS) &&
	    !panel_profile_list_is_writable (PANEL_GCONF_OBJECTS))
		gtk_widget_set_sensitive (menuitem, FALSE);

	menuitem = gtk_image_menu_item_new ();

	setup_menuitem (menuitem, 
			GTK_ICON_SIZE_MENU,
			gtk_image_new_from_stock (GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU),
			_("_Delete This Panel"),
			FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (G_OBJECT (menuitem), "activate",
			  G_CALLBACK (remove_panel),
			  NULL);
	g_signal_connect (G_OBJECT (menu), "show",
			  G_CALLBACK(setup_remove_this_panel),
			  menuitem);

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			GTK_ICON_SIZE_MENU,
			gtk_image_new_from_stock (
						  GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU),
			_("_Properties"),
			FALSE);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect_swapped (menuitem, "activate",
				  G_CALLBACK (panel_properties_dialog_present), 
				  panel_widget->toplevel);

	add_menu_separator (menu);

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem, 
			GTK_ICON_SIZE_MENU,
			gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU),
			_("_New Panel"),
			FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (create_new_panel), 
			  NULL);
	gtk_widget_set_sensitive (menuitem, 
				  panel_profile_list_is_writable (PANEL_GCONF_TOPLEVELS));

	add_menu_separator (menu);
}

static void
show_panel_help (GtkWidget *w, gpointer data)
{
	panel_show_help (
		gtk_widget_get_screen (w), "user-guide.xml", "gospanel-5");
}

GtkWidget *
create_panel_context_menu (PanelWidget *panel)
{
	GtkWidget *retval;
	GtkWidget *menuitem;
	char      *gnome_about;

	retval = menu_new ();

	if (!panel_lockdown_get_locked_down ())
		make_panel_submenu (panel, retval);

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			GTK_ICON_SIZE_MENU,
			gtk_image_new_from_stock (
				GTK_STOCK_HELP, GTK_ICON_SIZE_MENU),
				_("_Help"),
				FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (retval), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (show_panel_help), NULL);

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			GTK_ICON_SIZE_MENU,
			gtk_image_new_from_stock (
				GNOME_STOCK_ABOUT, GTK_ICON_SIZE_MENU),
				_("A_bout Panels"),
				FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (retval), menuitem);
	g_signal_connect (menuitem, "activate", G_CALLBACK (show_about_dialog), NULL);
	
	gnome_about = g_find_program_in_path ("gnome-about");
	if (gnome_about) {
		add_menu_separator (retval);

		menuitem = gtk_image_menu_item_new ();
		setup_menuitem (menuitem,
				GTK_ICON_SIZE_MENU,
				gtk_image_new_from_stock (
					GNOME_STOCK_ABOUT, GTK_ICON_SIZE_MENU),
					_("About _GNOME"),
					FALSE);
		gtk_menu_shell_append (GTK_MENU_SHELL (retval), menuitem);
		g_signal_connect_data (menuitem, "activate",
				       G_CALLBACK (about_gnome_cb),
				       gnome_about, (GClosureNotify) g_free, 0);
	}

	g_object_set_data (G_OBJECT (retval), "menu_panel", panel);

	return retval;
}

static void
append_lock_screen (GtkWidget *menu)
{
	GtkWidget  *menuitem;

	if (panel_lockdown_get_disable_lock_screen () ||
	    !panel_is_program_in_path ("xscreensaver"))
		return;

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (menuitem, panel_menu_icon_get_size (),
			       panel_action_get_stock_icon (PANEL_ACTION_LOCK),
			       panel_action_get_text (PANEL_ACTION_LOCK),
			       TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (panel_action_lock_screen), NULL);
	setup_internal_applet_drag (menuitem,
				    panel_action_get_drag_id (PANEL_ACTION_LOCK));
	gtk_tooltips_set_tip (panel_tooltips, menuitem,
			      panel_action_get_tooltip (PANEL_ACTION_LOCK),
			      NULL);
}

static void
append_log_out (GtkWidget *menu)
{
	GtkWidget  *menuitem;

	if (panel_lockdown_get_disable_log_out ())
		return;

	menuitem = gtk_image_menu_item_new ();

	setup_stock_menu_item (menuitem, panel_menu_icon_get_size (),
			       panel_action_get_stock_icon (PANEL_ACTION_LOGOUT),
			       panel_action_get_text (PANEL_ACTION_LOGOUT),
			       TRUE);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (panel_action_logout), NULL);
	setup_internal_applet_drag (menuitem,
				    panel_action_get_drag_id (PANEL_ACTION_LOGOUT));

	gtk_tooltips_set_tip (panel_tooltips, menuitem,
			      panel_action_get_tooltip (PANEL_ACTION_LOGOUT),
			      NULL);
}

static GtkWidget *
create_desktop_menu (GtkWidget *menu)
{
	if (!menu)
		menu = menu_new ();

	append_lock_screen (menu);
	append_log_out (menu);

	return menu;
}

static void
add_distribution_submenu (GtkWidget *root_menu,
			  gboolean fake_submenus)
{
	GtkWidget *menu;
	GtkWidget *menuitem;
	const DistributionInfo *distribution_info = get_distribution_info ();

	if (distribution_info == NULL)
		return;

	menu = create_distribution_menu (NULL /* menu */,
					 fake_submenus,
					 TRUE /* fake */);

	menuitem = gtk_image_menu_item_new ();
	setup_stock_menu_item (menuitem,
			       panel_menu_icon_get_size (),
			       distribution_info->stock_icon,
			       _(distribution_info->menu_name),
			       TRUE);

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

GtkWidget *
create_root_menu (GtkWidget   *root_menu,
		  PanelWidget *panel,
		  gboolean     fake_submenus,
		  int          flags,
		  gboolean     extra_items)
{
	const DistributionInfo *distribution_info;
	GtkWidget              *menu;
	GtkWidget              *menuitem;
	gboolean                has_inline;
	gboolean                has_subs;
	gboolean                has_subs2;

	if ((flags & MAIN_MENU_DISTRIBUTION_SUB) && !got_distro_menus ())
		flags &= ~MAIN_MENU_DISTRIBUTION_SUB;

	has_inline = (flags & MAIN_MENU_SYSTEM);

	has_subs = (flags & MAIN_MENU_SYSTEM_SUB);

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
				   FALSE /* fake */);

	if (flags & MAIN_MENU_DISTRIBUTION &&
	    distribution_info != NULL) {
		if (distribution_info->menu_show_func)
			distribution_info->menu_show_func(NULL,NULL);

		create_distribution_menu(root_menu, fake_submenus, FALSE);
	}

	/*others here*/

	if (has_subs && has_inline)
		add_menu_separator (root_menu);

	
	if (flags & MAIN_MENU_SYSTEM_SUB) {
		menu = create_system_menu(NULL, fake_submenus, TRUE);
		menuitem = gtk_image_menu_item_new ();
		setup_stock_menu_item (
			menuitem, panel_menu_icon_get_size (),
			PANEL_STOCK_APPLICATIONS, _("Applications"), TRUE);
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
		add_distribution_submenu (root_menu, fake_submenus);
	}

	if (!panel_lockdown_get_disable_command_line () && extra_items) {
		add_menu_separator (root_menu);
		menuitem = gtk_image_menu_item_new ();
		setup_stock_menu_item (
			menuitem, panel_menu_icon_get_size (),
			panel_action_get_stock_icon (PANEL_ACTION_RUN),
			panel_action_get_text (PANEL_ACTION_RUN),
			TRUE);
		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (panel_action_run_program),
				  NULL);
		setup_internal_applet_drag (menuitem,
					    panel_action_get_drag_id (PANEL_ACTION_RUN));
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      panel_action_get_tooltip (PANEL_ACTION_RUN),
				      NULL);
	}

	if (extra_items &&
	    panel_is_program_in_path  ("gnome-search-tool")) {
		menuitem = gtk_image_menu_item_new ();
		setup_stock_menu_item (
			menuitem, panel_menu_icon_get_size (),
			panel_action_get_stock_icon (PANEL_ACTION_SEARCH),
			panel_action_get_text (PANEL_ACTION_SEARCH),
			TRUE);
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      panel_action_get_tooltip (PANEL_ACTION_SEARCH),
				      NULL);
		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (panel_action_search), NULL);
		setup_internal_applet_drag (menuitem,
					    panel_action_get_drag_id (PANEL_ACTION_SEARCH));

		panel_recent_append_documents_menu (root_menu);
		add_menu_separator (root_menu);
	}

	if (extra_items &&
	    panel_is_program_in_path ("gnome-panel-screenshot")) {
		menuitem = gtk_image_menu_item_new ();
		setup_stock_menu_item (
			menuitem, panel_menu_icon_get_size (),
			panel_action_get_stock_icon (PANEL_ACTION_SCREENSHOT),
			panel_action_get_text (PANEL_ACTION_SCREENSHOT),
			TRUE);
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      panel_action_get_tooltip (PANEL_ACTION_SCREENSHOT),
				      NULL);
		g_signal_connect (menuitem, "activate",
				  G_CALLBACK (panel_action_screenshot), NULL);
		setup_internal_applet_drag (menuitem,
					    panel_action_get_drag_id (PANEL_ACTION_SCREENSHOT));
	}

	if (((has_inline && !has_subs) || has_subs) && has_subs2)
		add_menu_separator (root_menu);
	
	if (flags & MAIN_MENU_DESKTOP_SUB) {
		menu = create_desktop_menu (NULL);
		menuitem = gtk_image_menu_item_new ();
		setup_stock_menu_item (
			menuitem, panel_menu_icon_get_size (),
			PANEL_STOCK_DESKTOP, _("Desktop"), TRUE);
		gtk_menu_shell_append (GTK_MENU_SHELL (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	}

	if (flags & MAIN_MENU_DESKTOP)
		create_desktop_menu (root_menu);

	return root_menu;
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

static gboolean
panel_menu_key_press_handler (GtkWidget   *widget,
			      GdkEventKey *event)
{
	gboolean retval = FALSE;

	if ((event->keyval == GDK_Menu) ||
	    (event->keyval == GDK_F10 &&
	    (event->state & gtk_accelerator_get_default_mod_mask ()) == GDK_SHIFT_MASK)) {
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
