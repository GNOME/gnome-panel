/*
 * GNOME panel menu module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <config.h>

#include <libgnome/gnome-desktop-item.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-exec.h>

#include "menu-util.h"
#include "quick-desktop-reader.h"

#include "basep-widget.h"
#include "foobar-widget.h"
#include "multiscreen-stuff.h"
#include "distribution.h"
#include "panel-config-global.h"
#include "panel-main.h"
#include "panel-util.h"
#include "menu.h"

#undef MENU_UTIL_DEBUG

extern char *kde_menudir;

extern GlobalConfig global_config;

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
panel_standard_menu_pos (GtkMenu *menu, gint *x, gint *y)
{
	GtkRequisition requisition;
	int            screen;
	int            monitor;
	int            monitor_basex;
	int            monitor_basey;
	int            monitor_width;
	int            monitor_height;

	gtk_widget_get_child_requisition (GTK_WIDGET (menu),
					  &requisition);

	screen = gdk_screen_get_number (
			gtk_widget_get_screen (GTK_WIDGET (menu)));
	monitor = multiscreen_locate_coords (screen, *x, *y);

	monitor_basex  = multiscreen_x (screen, monitor);
	monitor_basey  = multiscreen_y (screen, monitor);
	monitor_width  = multiscreen_width (screen, monitor);
	monitor_height = multiscreen_height (screen, monitor);

	*x -= monitor_basex;
	*y -= monitor_basey;

	*x -= 2;
	*y -= 2;

	if ((*x + requisition.width) > monitor_width)
		*x -= ((*x + requisition.width) - monitor_width);
	if (*x < 0)
		*x = 0;
	if ((*y + requisition.height) > monitor_height)
		*y -= ((*y + requisition.height) - monitor_height);
	if (*y < 0)
		*y = 0;

	*x += monitor_basex;
	*y += monitor_basey;
}

static void
standard_position_within (GtkMenu  *menu,
			  gint     *x,
			  gint     *y,
			  gboolean *push_in,
			  GtkWidget *widget)
{
	gint wx, wy;

	g_return_if_fail (widget != NULL);

	gdk_window_get_origin (widget->window, &wx, &wy);
	if(GTK_WIDGET_NO_WINDOW(widget)) {
		wx += widget->allocation.x;
		wy += widget->allocation.y;
	}

	/*
	 * Make sure that the popup position is in the widget
	 * as the menu may be popped up by a keystroke
	 */
	if (*x < wx)
		*x = wx;
	else if (*x > wx + widget->allocation.width)
		*x = wx + widget->allocation.width;

	if (*y < wy)
		*y = wy;
	else if (*y > wy + widget->allocation.height)
		*y = wy + widget->allocation.height;

	*push_in = TRUE;
	
	panel_standard_menu_pos (menu, x, y);
}

void
panel_menu_position (GtkMenu  *menu,
		     gint     *x,
		     gint     *y,
		     gboolean *push_in,
		     gpointer  data)
{
	GtkWidget *w = data;

	standard_position_within (menu, x, y, push_in, w);
}

void
menu_item_menu_position (GtkMenu  *menu,
		        gint     *x,
		        gint     *y,
		        gboolean *push_in,
		        gpointer  data)
{
	GtkWidget *w = data;

	standard_position_within (menu, x, y, push_in, w);
}

void
applet_menu_position (GtkMenu  *menu,
		      gint     *x,
		      gint     *y,
		      gboolean *push_in,
		      gpointer  data)
{
	AppletInfo *info = data;
	PanelWidget *panel;
	GtkWidget *w; /*the panel window widget*/

	g_return_if_fail(info != NULL);
	g_return_if_fail(info->widget != NULL);

	panel = PANEL_WIDGET(info->widget->parent);
	g_return_if_fail(panel != NULL);
	
	w = panel->panel_parent;

	standard_position_within (menu, x, y, push_in, w);
}

void
applet_menu_position_outside (GtkMenu  *menu,
			      gint     *x,
			      gint     *y,
			      gboolean *push_in,
			      gpointer  data)
{
	AppletInfo *info = data;
	int wx, wy;
	PanelWidget *panel;
	GtkWidget *w; /*the panel window widget*/

	g_return_if_fail(info != NULL);
	g_return_if_fail(info->widget != NULL);

	panel = PANEL_WIDGET(info->widget->parent);
	g_return_if_fail(panel != NULL);
	
	w = panel->panel_parent;

	gdk_window_get_origin (info->widget->window, &wx, &wy);
	if(GTK_WIDGET_NO_WINDOW(info->widget)) {
		wx += info->widget->allocation.x;
		wy += info->widget->allocation.y;
	}

#ifdef MENU_UTIL_DEBUG
	g_print ("applet_menu_position_outside: origin x = %d, y = %d\n", wx, wy);
#endif

	if (BASEP_IS_WIDGET (w)) {
		*x = *y = 0;
		basep_widget_get_menu_pos(BASEP_WIDGET(w),
					  GTK_WIDGET(menu),
					  x,y,wx,wy,
					  info->widget->allocation.width,
					  info->widget->allocation.height);
       	} else if (FOOBAR_IS_WIDGET (w)) {
		GtkRequisition req;
		FoobarWidget *foo = FOOBAR_WIDGET (w);
		gtk_widget_get_child_requisition (GTK_WIDGET (menu), &req);
		*x = MIN (*x,
			  multiscreen_width (foo->screen, foo->monitor) +
			  multiscreen_x (foo->screen, foo->monitor) - req.width);
		*y = w->allocation.height + multiscreen_y (foo->screen, foo->monitor);
	}

	*push_in = TRUE;
 
#ifdef MENU_UTIL_DEBUG
	g_print ("applet_menu_position_outside: x = %d, y = %d, push_in = %s\n",
		 *x, *y, *push_in ? "(true)" : "(false)");
#endif
}

int
get_default_menu_flags (void)
{
	DistributionType distribution = get_distribution_type();

	int flags = MAIN_MENU_SYSTEM_SUB |
		MAIN_MENU_PANEL_SUB | MAIN_MENU_DESKTOP;
	
	/*guess distribution menus*/
	if (distribution != DISTRIBUTION_UNKNOWN)
		flags |= MAIN_MENU_DISTRIBUTION_SUB;

	/*guess KDE menus*/
	if (g_file_test (kde_menudir, G_FILE_TEST_IS_DIR))
		flags |= MAIN_MENU_KDE_SUB;

	return flags;
}

gboolean
got_kde_menus (void)
{
	/*guess KDE menus*/
	if (g_file_test (kde_menudir, G_FILE_TEST_IS_DIR))
		return TRUE;
	else
		return FALSE;
}

gboolean
got_distro_menus (void)
{
	/*guess distribution menus*/
	if (get_distribution_type () != DISTRIBUTION_UNKNOWN)
		return TRUE;
	else
		return FALSE;
}

GtkWidget *
stock_menu_item_new (const char *text,
		     const char *stock_id,
		     gboolean    force_image)
{
        GtkWidget *item;
        GtkWidget *label;

        item = gtk_image_menu_item_new ();

        panel_load_menu_image_deferred (item, stock_id, NULL, NULL, force_image);

        if (text) {
                label = gtk_label_new (text);
                gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
                gtk_container_add (GTK_CONTAINER (item), label);
        }

        return item;
}

char *
get_real_menu_path (const char *arguments, gboolean main_menu)
{
        if (main_menu || !arguments)
                return g_strdup ("applications:");

        if (arguments [0] == '~')
                /* FIXME: this needs to be a URI */
                return g_build_filename (g_get_home_dir(),
                                              &arguments[1], NULL);
        else
                return g_strdup (arguments);
}

char *
get_pixmap (const char *menudir, gboolean main_menu)
{
        char *pixmap_name = NULL;

        if (main_menu) {
                pixmap_name = panel_pixmap_discovery ("gnome-logo-icon-transparent.png",
                                                      TRUE /* fallback */);
        } else {
                char *dentry_name;
                QuickDesktopItem *qitem;

                dentry_name = g_build_path ("/",
                                            menudir,
                                            ".directory",
                                            NULL);
                qitem = quick_desktop_item_load_uri (dentry_name,
                                                     NULL /* expected_type */,
                                                     FALSE /* run_tryexec */);
                g_free (dentry_name);

                if (qitem != NULL)
			pixmap_name = gnome_desktop_item_find_icon (panel_icon_loader,
								    qitem->icon,
                                                                    20 /* desired size */,
                                                                    0 /* flags */);

                if (pixmap_name == NULL)
                        pixmap_name = panel_pixmap_discovery ("gnome-folder.png",
                                                              TRUE /* fallback */);

                if (qitem != NULL)
                        quick_desktop_item_destroy (qitem);
        }
        return pixmap_name;
}
