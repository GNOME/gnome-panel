/*
 * GNOME panel menu module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <config.h>
#include <gnome.h>

#include "panel-include.h"
#include "multiscreen-stuff.h"

/*#define PANEL_DEBUG 1*/

extern char *kde_menudir;
extern char *kde_icondir;
extern char *kde_mini_icondir;

extern GlobalConfig global_config;

GtkWidget *
add_menu_separator (GtkWidget *menu)
{
	GtkWidget *menuitem;
	
	menuitem = gtk_menu_item_new ();
	gtk_widget_set_sensitive (menuitem, FALSE);
	gtk_widget_show (menuitem);
	gtk_menu_append (GTK_MENU (menu), menuitem);

	return menuitem;
}

static void
panel_standard_menu_pos (GtkMenu *menu, gint *x, gint *y, gpointer data)
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

void
panel_menu_position (GtkMenu *menu, gint *x, gint *y, gpointer data)
{
	GtkWidget *w = data;
	gint wx, wy;

	g_return_if_fail (w != NULL);

	if ( ! global_config.off_panel_popups) {
		panel_standard_menu_pos (menu, x, y, data);
		return;
	}

	gdk_window_get_origin (w->window, &wx, &wy);

	gtk_widget_get_pointer(w, x, y);
	if (IS_BASEP_WIDGET (w)) {
		basep_widget_get_menu_pos(BASEP_WIDGET(w), 
					  GTK_WIDGET(menu),
					  x,y,wx,wy,
					  w->allocation.width,
					  w->allocation.height);
	} else if (IS_FOOBAR_WIDGET (w)) {
		GtkRequisition req;
		FoobarWidget *foo = FOOBAR_WIDGET (w);
		gtk_widget_get_child_requisition (GTK_WIDGET (menu), &req);
		*x = MIN (*x,
			  multiscreen_width (foo->screen) +
			  multiscreen_x (foo->screen) -  req.width);
		*y = w->allocation.height + multiscreen_y (foo->screen);
	}
}

void
applet_menu_position (GtkMenu *menu, gint *x, gint *y, gpointer data)
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
	if (IS_BASEP_WIDGET (w)) {
		*x = *y = 0;
		basep_widget_get_menu_pos(BASEP_WIDGET(w),
					  GTK_WIDGET(menu),
					  x,y,wx,wy,
					  info->widget->allocation.width,
					  info->widget->allocation.height);
       	} else if (IS_FOOBAR_WIDGET (w)) {
		GtkRequisition req;
		FoobarWidget *foo = FOOBAR_WIDGET (w);
		gtk_widget_get_child_requisition (GTK_WIDGET (menu), &req);
		*x = MIN (*x,
			  multiscreen_width (foo->screen) +
			  multiscreen_x (foo->screen) -  req.width);
		*y = w->allocation.height + multiscreen_y (foo->screen);
	}
}

/* This function is marked as const as get_distribution_type is const and
 * it would never change while the program is running.  If you change this
 * do remember to take out the G_GNUC_CONST in menu-util.h */
int
get_default_menu_flags (void)
{
	DistributionType distribution = get_distribution_type();

	int flags = MAIN_MENU_SYSTEM_SUB | MAIN_MENU_USER_SUB |
		MAIN_MENU_APPLETS_SUB | MAIN_MENU_PANEL_SUB |
		MAIN_MENU_DESKTOP;
	
	/*guess distribution menus*/
	if(distribution != DISTRIBUTION_UNKNOWN)
		flags |= MAIN_MENU_DISTRIBUTION_SUB;

	/*guess KDE menus*/
	if (panel_file_exists (kde_menudir))
		flags |= MAIN_MENU_KDE_SUB;

	return flags;
}
