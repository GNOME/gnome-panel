/*
 * Copyright (C) 1997 - 2000 The Free Software Foundation
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2004 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include "menu.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/gnome-desktop-item.h>
#include <gconf/gconf-client.h>

#include "launcher.h"
#include "panel-util.h"
#include "panel.h"
#include "drawer.h"
#include "panel-config-global.h"
#include "panel-stock-icons.h"
#include "panel-action-button.h"
#include "panel-recent.h"
#include "panel-profile.h"
#include "panel-menu-button.h"
#include "panel-globals.h"
#include "panel-run-dialog.h"
#include "panel-lockdown.h"

typedef struct {
	GtkWidget   *pixmap;
	const char  *stock_id;
	char        *image;
	char        *fallback_image;
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

static GtkWidget *populate_menu_from_directory (GtkWidget         *menu,
						MenuTreeDirectory *directory);

static void panel_load_menu_image_deferred (GtkWidget   *image_menu_item,
					    GtkIconSize  icon_size,
					    const char  *stock_id,
					    const char  *image_filename,
					    const char  *fallback_image_filename);

static gboolean panel_menu_key_press_handler (GtkWidget   *widget,
					      GdkEventKey *event);

static inline gboolean
panel_menu_have_icons (void)
{	
	return gconf_client_get_bool (
			panel_gconf_get_client (),
			"/desktop/gnome/interface/menus_have_icons",
			NULL);
}

GtkWidget *
add_menu_separator (GtkWidget *menu)
{
	GtkWidget *menuitem;
	
	menuitem = gtk_separator_menu_item_new ();
	gtk_widget_set_sensitive (menuitem, FALSE);
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	return menuitem;
}

static void
activate_app_def (GtkWidget     *menuitem,
		  MenuTreeEntry *entry)
{
	GnomeDesktopItem *item;
	GError           *error;
	const char       *path;

	path = menu_tree_entry_get_desktop_file_path (entry);

	error = NULL;
	item = gnome_desktop_item_new_from_file (path, 0, &error);
	if (item) {
		g_assert (error == NULL);

		panel_ditem_launch (
			item, NULL, 0, menuitem_to_screen (menuitem), &error);
		if (error) {
			panel_error_dialog (menuitem_to_screen (menuitem),
					    "cannot_launch_entry", TRUE,
					    _("Cannot launch entry"),
					    "%s",
					    error->message);

			g_error_free (error);
		}
		gnome_desktop_item_unref (item);
	} else {
		g_assert (error != NULL);

		panel_error_dialog (menuitem_to_screen (menuitem),
				    "cannot_load_entry", TRUE,
				    _("Cannot load entry"),
				    "%s",
				    error->message);
		g_error_free (error);
	}
}

PanelWidget *
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

GdkScreen *
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

	g_signal_connect (retval, "key_press_event",
			  G_CALLBACK (panel_menu_key_press_handler),
			  NULL);

	return retval;
}

GtkWidget *
create_empty_menu (void)
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

	file = NULL;
	if (icon != NULL)
		file = gnome_desktop_item_find_icon (panel_icon_theme,
						     icon,
						     size /* desired size */,
						     0 /* flags */);
	if (file == NULL && fallback != NULL)
		file = gnome_desktop_item_find_icon (panel_icon_theme,
						     fallback,
						     size /* desired size */,
						     0 /* flags */);

	if (file == NULL)
		return NULL;

	if (long_operation != NULL)
		*long_operation = TRUE;

	pb = NULL;

	loaded = FALSE;

	key = g_strdup_printf ("%d:%s", size, file);

	if (loaded_icons != NULL &&
	    (pb = g_hash_table_lookup (loaded_icons, key)) != NULL) {
		if (pb != NULL)
			g_object_ref (G_OBJECT (pb));
	}

	if (pb == NULL) {
		pb = gdk_pixbuf_new_from_file_at_size (file, size, size, NULL);
		
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
add_app_to_panel (GtkWidget     *item,
		  MenuTreeEntry *entry)
{
	PanelWidget   *panel_widget;
	PanelToplevel *toplevel;
	PanelData     *pd;
	int            position;

	panel_widget = menu_get_panel (item);
	toplevel = panel_widget->toplevel;

	pd = g_object_get_data (G_OBJECT (toplevel), "PanelData");
	position = pd ?  pd->insertion_pos : -1;

	panel_launcher_create (toplevel,
			       position,
			       menu_tree_entry_get_desktop_file_path (entry));
}


static void
add_drawers_from_dir (MenuTreeDirectory *directory,
		      int                pos,
		      const char        *toplevel_id)
{
	const char *name;
	const char *icon;
	GSList     *entries;
	GSList     *subdirs;
	GSList     *l;
	char       *attached_toplevel_id;

	name = menu_tree_directory_get_name (directory);
	icon = menu_tree_directory_get_icon (directory);

	attached_toplevel_id = panel_drawer_create_with_id (toplevel_id,
							    pos,
							    icon,
							    icon != NULL,
							    name);
	if (!attached_toplevel_id)
		return;

	entries = menu_tree_directory_get_entries (directory);
	for (l = entries; l; l = l->next) {
		MenuTreeEntry *entry = l->data;

		panel_launcher_create_with_id (attached_toplevel_id,
					       G_MAXINT/2,
					       menu_tree_entry_get_desktop_file_path (entry));

		menu_tree_entry_unref (entry);
	}
	g_slist_free (entries);

	subdirs = menu_tree_directory_get_subdirs (directory);
	for (l = subdirs; l; l = l->next) {
		MenuTreeDirectory *subdir = l->data;

		add_drawers_from_dir (subdir, G_MAXINT/2, attached_toplevel_id);

		menu_tree_directory_unref (subdir);
	}
	g_slist_free (subdirs);

	g_free (attached_toplevel_id);
}

static void
add_menudrawer_to_panel (GtkWidget     *menuitem,
			 MenuTreeEntry *entry)

{
	MenuTreeDirectory *directory;
	PanelWidget       *panel;
	PanelData         *pd;
	int                insertion_pos;

	directory = menu_tree_entry_get_parent (entry);

	panel = menu_get_panel (menuitem);

	pd = g_object_get_data (G_OBJECT (panel->toplevel), "PanelData");
	insertion_pos = pd ? pd->insertion_pos : -1;

	add_drawers_from_dir (directory,
			      insertion_pos,
			      panel_profile_get_toplevel_id (panel->toplevel));

	menu_tree_directory_unref (directory);
}

static void
add_menu_to_panel (GtkWidget     *menuitem,
		   MenuTreeEntry *entry)
{
	MenuTreeDirectory *directory;
	PanelWidget       *panel;
	PanelData         *pd;
	int                insertion_pos;
	char              *menu_path;

	directory = menu_tree_entry_get_parent (entry);

	panel = menu_get_panel (menuitem);

	pd = g_object_get_data (G_OBJECT (panel->toplevel), "PanelData");
	insertion_pos = pd ? pd->insertion_pos : -1;

	menu_path = menu_tree_directory_make_path (directory, NULL);

	panel_menu_button_create (panel->toplevel,
				  insertion_pos,
				  menu_path,
				  TRUE,
				  menu_tree_directory_get_name (directory));

	g_free (menu_path);
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

static GtkWidget *
create_item_context_menu (GtkWidget   *item,
			  PanelWidget *panel_widget)
{
	MenuTreeEntry *entry;
	GtkWidget     *menu;
	GtkWidget     *submenu;
	GtkWidget     *menuitem;
	gboolean       id_lists_writable;

	id_lists_writable = panel_profile_id_lists_are_writable ();

	entry = g_object_get_data (G_OBJECT (item), "panel-menu-tree-entry");
	g_assert (entry != NULL);


	menu = create_empty_menu ();
	g_object_set_data_full (G_OBJECT (item),
				"panel-item-context-menu",
				g_object_ref (menu),
				(GDestroyNotify) g_object_unref);
	gtk_object_sink (GTK_OBJECT (menu));

	g_object_set_data (G_OBJECT (menu), "menu_panel", panel_widget);


	g_signal_connect (menu, "deactivate",
			  G_CALLBACK (restore_grabs), item);

	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			panel_menu_icon_get_size (),
			NULL,
			_("Add this launcher to panel"),
			FALSE);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (add_app_to_panel), entry);
	gtk_widget_set_sensitive (menuitem, id_lists_writable);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);


	submenu = create_empty_menu ();

	g_object_set_data (G_OBJECT (submenu), "menu_panel", panel_widget);


	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			panel_menu_icon_get_size (),
			NULL,
			_("Entire menu"),
			FALSE);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);


	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			panel_menu_icon_get_size (),
			NULL,
			_("Add this as drawer to panel"),
			FALSE);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (add_menudrawer_to_panel), entry);
	gtk_widget_set_sensitive (menuitem, id_lists_writable);
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);


	menuitem = gtk_image_menu_item_new ();
	setup_menuitem (menuitem,
			panel_menu_icon_get_size (),
			NULL,
			_("Add this as menu to panel"),
			FALSE);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (add_menu_to_panel), entry);
	gtk_widget_set_sensitive (menuitem, id_lists_writable);
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);

	return menu;
}

static gboolean
show_item_menu (GtkWidget      *item,
		GdkEventButton *bevent)
{
	PanelWidget *panel_widget;
	GtkWidget   *menu;

	if (panel_lockdown_get_locked_down ())
		return FALSE;

	panel_widget = menu_get_panel (item);

	menu = g_object_get_data (G_OBJECT (item), "panel-item-context-menu");

	if (!menu)
		menu = create_item_context_menu (item, panel_widget);

	gtk_menu_set_screen (GTK_MENU (menu),
			     gtk_window_get_screen (GTK_WINDOW (panel_widget->toplevel)));

	gtk_menu_popup (GTK_MENU (menu),
			NULL, NULL, NULL, NULL,
			bevent->button,
			bevent->time);

	return TRUE;
}

gboolean
menu_dummy_button_press_event (GtkWidget      *menuitem,
			       GdkEventButton *event)
{
	if (event->button == 3)
		return TRUE;

	return FALSE;
}

static gboolean
menuitem_button_press_event (GtkWidget      *menuitem,
			     GdkEventButton *event)
{
	if (event->button == 3)
		return show_item_menu (menuitem, event);
	
	return FALSE;
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
		       MenuTreeEntry    *entry)
{
	char *uri_list;

	uri_list = g_strconcat (menu_tree_entry_get_desktop_file_path (entry),
				"\r\n",
				NULL);

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

void
setup_menuitem (GtkWidget   *menuitem,
		GtkIconSize  icon_size,
		GtkWidget   *image,
		const char  *title,
		gboolean     invisible_mnemonic)
			       
{
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

	gtk_widget_show (menuitem);
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
setup_internal_applet_drag (GtkWidget             *menuitem,
			    PanelActionButtonType  type)
{
	static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};

	if (panel_lockdown_get_locked_down ())
		return;

	gtk_drag_source_set (menuitem,
			     GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			     menu_item_targets, 1,
			     GDK_ACTION_COPY);

	if (panel_action_get_stock_icon (type)  != NULL)
		gtk_drag_source_set_icon_stock (menuitem,
						panel_action_get_stock_icon (type));
	/* FIXME: waiting for bug #116577
	else
		gtk_drag_source_set_icon_name (GTK_WIDGET (button),
					panel_action_get_icon_name (type);
					*/
	
	g_signal_connect_data (G_OBJECT (menuitem), "drag_data_get",
			       G_CALLBACK (drag_data_get_string_cb),
			       g_strdup (panel_action_get_drag_id (type)),
			       (GClosureNotify)g_free,
			       0 /* connect_flags */);
	g_signal_connect (G_OBJECT (menuitem), "drag_end",
			  G_CALLBACK (drag_end_menu_cb), NULL);
}

static void
submenu_to_display (GtkWidget *menu)
{
	MenuTree          *tree;
	MenuTreeDirectory *directory;
	const char        *menu_path;

	if (!g_object_get_data (G_OBJECT (menu), "panel-menu-needs-loading"))
		return;

	g_object_set_data (G_OBJECT (menu), "panel-menu-needs-loading", NULL);

	if (!(directory = g_object_get_data (G_OBJECT (menu), "panel-menu-tree-directory")) &&
	     (menu_path = g_object_get_data (G_OBJECT (menu), "panel-menu-tree-path"))) {
		if (!(tree = g_object_get_data (G_OBJECT (menu), "panel-menu-tree")))
			return;

		directory = menu_tree_get_directory_from_path (tree, menu_path);

		g_object_set_data_full (G_OBJECT (menu), "panel-menu-tree-directory",
					directory,
					(GDestroyNotify) menu_tree_directory_unref);
	}

	if (directory)
		populate_menu_from_directory (menu, directory);
}

static GtkWidget *
create_fake_menu (MenuTreeDirectory *directory)
{	
	GtkWidget *menu;
	
	menu = create_empty_menu ();

	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-tree-directory",
				menu_tree_directory_ref (directory),
				(GDestroyNotify) menu_tree_directory_unref);
	
	g_object_set_data (G_OBJECT (menu),
			   "panel-menu-needs-loading",
			   GUINT_TO_POINTER (TRUE));

	g_signal_connect (menu, "show",
			  G_CALLBACK (submenu_to_display), NULL);
	g_signal_connect (menu, "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	return menu;
}

static void
create_submenu (GtkWidget         *menu,
		MenuTreeDirectory *directory)
{
	GtkWidget *menuitem;
	GtkWidget *submenu;

	menuitem = gtk_image_menu_item_new ();

	g_object_set_data_full (G_OBJECT (menuitem),
				"panel-menu-tree-directory",
				menu_tree_directory_ref (directory),
				(GDestroyNotify) menu_tree_directory_unref);

	panel_load_menu_image_deferred (menuitem,
					panel_menu_icon_get_size (),
					NULL,
					menu_tree_directory_get_icon (directory),
					"gnome-fs-directory");

	setup_menuitem (menuitem,
			panel_menu_icon_get_size (),
			NULL,
			menu_tree_directory_get_name (directory),
			TRUE);

	if (menu_tree_directory_get_comment (directory))
		gtk_tooltips_set_tip (panel_tooltips,
				      menuitem,
				      menu_tree_directory_get_comment (directory),
				      NULL);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	submenu = create_fake_menu (directory);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
}

static void
create_menuitem (GtkWidget     *menu,
		 MenuTreeEntry *entry)
{
	GtkWidget  *menuitem;
	
	menuitem = gtk_image_menu_item_new ();

	g_object_set_data_full (G_OBJECT (menuitem),
				"panel-menu-tree-entry",
				menu_tree_entry_ref (entry),
				(GDestroyNotify) menu_tree_entry_unref);

	panel_load_menu_image_deferred (menuitem,
					panel_menu_icon_get_size (),
					NULL,
					menu_tree_entry_get_icon (entry),
					NULL);

	setup_menuitem (menuitem,
			panel_menu_icon_get_size (),
			NULL,
			menu_tree_entry_get_name (entry),
			TRUE);

	if (menu_tree_entry_get_comment (entry))
		gtk_tooltips_set_tip (panel_tooltips,
				      menuitem,
				      menu_tree_entry_get_comment (entry),
				      NULL);

	g_signal_connect (menuitem, "button_press_event",
			  G_CALLBACK (menuitem_button_press_event), NULL);

	if (!panel_lockdown_get_locked_down ()) {
		static GtkTargetEntry menu_item_targets[] = {
			{ "text/uri-list", 0, 0 }
		};

		gtk_drag_source_set (menuitem,
				     GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
				     menu_item_targets, 1,
				     GDK_ACTION_COPY);

		/* FIXME: waiting for bug #116577
		gtk_drag_source_set_icon_name (GTK_WIDGET (button),
					menu_tree_entry_get_icon (entry));
		*/
		g_signal_connect (menuitem, "drag_data_get",
				  G_CALLBACK (drag_data_get_menu_cb), entry);
		g_signal_connect (menuitem, "drag_end",
				  G_CALLBACK (drag_end_menu_cb), NULL);
	}

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (activate_app_def), entry);
}

static void
handle_menu_tree_changed (MenuTree  *tree,
			  GtkWidget *menu)
{
	while (GTK_MENU_SHELL (menu)->children)
                gtk_widget_destroy (GTK_MENU_SHELL (menu)->children->data);

	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-tree-directory",
				NULL, NULL);

	g_object_set_data (G_OBJECT (menu),
			   "panel-menu-needs-loading",
			   GUINT_TO_POINTER (TRUE));
}

static void
remove_menu_tree_monitor (GtkWidget *menu,
			  MenuTree  *tree)
{
	menu_tree_remove_monitor (tree,
				  (MenuTreeChangedFunc) handle_menu_tree_changed,
				  menu);
}

GtkWidget *
create_applications_menu (const char *menu_file,
			  const char *menu_path)
{
	MenuTree  *tree;
	GtkWidget *menu;

	menu = create_empty_menu ();

	tree = menu_tree_lookup (menu_file);

	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-tree",
				menu_tree_ref (tree),
				(GDestroyNotify) menu_tree_unref);

	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-tree-path",
				g_strdup (menu_path ? menu_path : "/"),
				(GDestroyNotify) g_free);
	
	g_object_set_data (G_OBJECT (menu),
			   "panel-menu-needs-loading",
			   GUINT_TO_POINTER (TRUE));

	g_signal_connect (menu, "show",
			  G_CALLBACK (submenu_to_display), NULL);
	g_signal_connect (menu, "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	menu_tree_add_monitor (tree,
			       (MenuTreeChangedFunc) handle_menu_tree_changed,
			       menu);
	g_signal_connect (menu, "destroy",
			  G_CALLBACK (remove_menu_tree_monitor), tree);

	menu_tree_unref (tree);

	return menu;
}

static GtkWidget *
populate_menu_from_directory (GtkWidget         *menu,
			      MenuTreeDirectory *directory)
{	
	GSList   *subdirs;
	GSList   *entries;
	GSList   *l;
	gboolean  add_separator;

	add_separator = (GTK_MENU_SHELL (menu)->children != NULL);

	subdirs = menu_tree_directory_get_subdirs (directory);
	for (l = subdirs; l; l = l->next) {
		MenuTreeDirectory *subdir = l->data;

		if (add_separator) {
			add_menu_separator (menu);
			add_separator = FALSE;
		}

		create_submenu (menu, subdir);

		menu_tree_directory_unref (subdir);
	}
	g_slist_free (subdirs);

	entries = menu_tree_directory_get_entries (directory);
	for (l = entries; l; l = l->next) {
		MenuTreeEntry *entry = l->data;

		if (add_separator) {
			add_menu_separator (menu);
			add_separator = FALSE;
		}

		create_menuitem (menu, entry);

		menu_tree_entry_unref (entry);
	}
	g_slist_free (entries);

	return menu;
}

void
setup_menu_item_with_icon (GtkWidget   *item,
			   GtkIconSize  icon_size,
			   const char  *icon_name,
			   const char  *stock_id,
			   const char  *title,
			   gboolean     invisible_mnemonic)
{
	if (icon_name || stock_id)
		panel_load_menu_image_deferred (item, icon_size,
						stock_id, icon_name, NULL);

	setup_menuitem (item, icon_size, NULL, title, invisible_mnemonic);
}
  
GtkWidget *
menu_create_action_item (PanelActionButtonType action_type)
{
	GtkWidget *item;

	if (panel_action_get_is_disabled (action_type))
		return NULL;

	item = gtk_image_menu_item_new ();
        setup_menu_item_with_icon (item,
				   panel_menu_icon_get_size (),
				   panel_action_get_icon_name (action_type),
				   panel_action_get_stock_icon (action_type),
				   panel_action_get_text (action_type),
				   TRUE);

	gtk_tooltips_set_tip (panel_tooltips,
			      item,
			      panel_action_get_tooltip (action_type),
			      NULL);

	g_signal_connect (item, "activate",
			  panel_action_get_invoke (action_type), NULL);
	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);
	setup_internal_applet_drag (item, action_type);

	return item;
}

GtkWidget *
create_main_menu (PanelWidget *panel)
{
	GtkWidget *main_menu;
	GtkWidget *applications_menu;
	GtkWidget *menuitem;

	main_menu = create_empty_menu ();
	g_object_set_data (G_OBJECT (main_menu), "menu_panel", panel);

	menuitem = gtk_image_menu_item_new ();
	setup_menu_item_with_icon (menuitem,
				   panel_menu_icon_get_size (),
				   "gnome-applications",
				   NULL,
				   _("Applications"),
				   TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), menuitem);

	applications_menu = create_applications_menu ("applications.menu",
						      NULL);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), applications_menu);

	add_menu_separator (main_menu);

	menuitem = menu_create_action_item (PANEL_ACTION_RUN);
	if (menuitem != NULL)
		gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), menuitem);

	if (panel_is_program_in_path  ("gnome-search-tool")) {
		menuitem = menu_create_action_item (PANEL_ACTION_SEARCH);
		if (menuitem != NULL)
			gtk_menu_shell_append (GTK_MENU_SHELL (main_menu),
					       menuitem);
	}

	panel_recent_append_documents_menu (main_menu);

	add_menu_separator (main_menu);

	if (panel_is_program_in_path ("gnome-screenshot")) {
		menuitem = menu_create_action_item (PANEL_ACTION_SCREENSHOT);
		if (menuitem != NULL)
			gtk_menu_shell_append (GTK_MENU_SHELL (main_menu),
					       menuitem);

		add_menu_separator (main_menu);
	}

	if (panel_is_program_in_path  ("xscreensaver")) {
		menuitem = menu_create_action_item (PANEL_ACTION_LOCK);
		if (menuitem != NULL)
			gtk_menu_shell_append (GTK_MENU_SHELL (main_menu),
					       menuitem);
	}

	menuitem = menu_create_action_item (PANEL_ACTION_SHUTDOWN);
	if (menuitem != NULL)
		gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), menuitem);

	menuitem = menu_create_action_item (PANEL_ACTION_REBOOT);
	if (menuitem != NULL)
		gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), menuitem);

	menuitem = menu_create_action_item (PANEL_ACTION_LOGOUT);
	if (menuitem != NULL)
		gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), menuitem);
	
	return main_menu;
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

static void
panel_load_menu_image_deferred (GtkWidget   *image_menu_item,
				GtkIconSize  icon_size,
				const char  *stock_id,
				const char  *image_filename,
				const char  *fallback_image_filename)
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
	icon->icon_size      = icon_size;

	gtk_widget_show (image);

	g_object_set_data_full (G_OBJECT (image_menu_item),
				"Panel:Image",
				g_object_ref (image),
				(GDestroyNotify) g_object_unref);
 
	if (panel_menu_have_icons ())
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
			GdkEventButton bevent;

			bevent.button = 3;
			bevent.time = GDK_CURRENT_TIME;
			show_item_menu (menu_shell->active_menu_item, &bevent);
		}
		
	}
	return retval;
}
