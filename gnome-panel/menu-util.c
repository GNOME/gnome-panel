/*
 * GNOME panel menu module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <config.h>

#include "menu-util.h"

#include "basep-widget.h"
#include "foobar-widget.h"
#include "multiscreen-stuff.h"
#include "distribution.h"
#include "panel-config-global.h"
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
	gint screen_width;
	gint screen_height;
	int screen_basex, screen_basey;
	int screen;
	GtkRequisition requisition;

	gtk_widget_get_child_requisition (GTK_WIDGET (menu),
					  &requisition);

	screen = multiscreen_screen_from_pos (*x, *y);

	if (screen < 0) {
		screen_width = gdk_screen_width ();
		screen_height = gdk_screen_height ();
		screen_basex = 0;
		screen_basey = 0;
	} else {
		screen_width = multiscreen_width (screen);
		screen_height = multiscreen_height (screen);
		screen_basex = multiscreen_x (screen);
		screen_basey = multiscreen_y (screen);
	}

	*x -= screen_basex;
	*y -= screen_basey;

	*x -= 2;
	*y -= 2;

	if ((*x + requisition.width) > screen_width)
		*x -= ((*x + requisition.width) - screen_width);
	if (*x < 0)
		*x = 0;
	if ((*y + requisition.height) > screen_height)
		*y -= ((*y + requisition.height) - screen_height);
	if (*y < 0)
		*y = 0;

	*x += screen_basex;
	*y += screen_basey;
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
