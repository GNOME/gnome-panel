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

/*#define PANEL_DEBUG 1*/

void
add_menu_separator (GtkWidget *menu)
{
	GtkWidget *menuitem;
	
	menuitem = gtk_menu_item_new ();
	gtk_widget_set_sensitive (menuitem, FALSE);
	gtk_widget_show (menuitem);
	gtk_menu_append (GTK_MENU (menu), menuitem);
}

void
panel_menu_position (GtkMenu *menu, gint *x, gint *y, gpointer data)
{
	GtkWidget *w = data;
	gint wx, wy;

	g_return_if_fail(w != NULL);

	gdk_window_get_origin (w->window, &wx, &wy);

	gtk_widget_get_pointer(w, x, y);
	if (IS_BASEP_WIDGET (w))
		basep_widget_get_menu_pos(BASEP_WIDGET(w), 
					  GTK_WIDGET(menu),
					  x,y,wx,wy,
					  w->allocation.width,
					  w->allocation.height);
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
	*x = *y = 0;
	if (IS_BASEP_WIDGET (w))
		basep_widget_get_menu_pos(BASEP_WIDGET(w),
					  GTK_WIDGET(menu),
					  x,y,wx,wy,
					  info->widget->allocation.width,
					  info->widget->allocation.height);
}

int
get_default_menu_flags ()
{
	int flags = MAIN_MENU_SYSTEM_SUB | MAIN_MENU_USER_SUB |
		MAIN_MENU_APPLETS_SUB | MAIN_MENU_PANEL_SUB |
		MAIN_MENU_DESKTOP;
	
	/*guess redhat menus*/
	if(g_file_exists(REDHAT_MENUDIR))
		flags |= MAIN_MENU_REDHAT_SUB;

	/*guess KDE menus*/
	if(g_file_exists(KDE_MENUDIR))
		flags |= MAIN_MENU_KDE_SUB;

	/*guess debian menus*/
	if (g_file_exists(DEBIAN_MENUDIR))
		flags |= MAIN_MENU_DEBIAN_SUB;
	
	return flags;
}
