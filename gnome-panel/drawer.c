/*
 * GNOME panel drawer module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 *          George Lebl
 */

#include <config.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include "gnome.h"
#include "panel-widget.h"
#include "panel.h"
#include "panel_config_global.h"
#include "drawer.h"


#define DRAWER_PANEL "drawer_panel"

static char *gnome_folder = NULL;

extern GlobalConfig global_config;

void
reposition_drawer(Drawer *drawer)
{
	if(PANEL_WIDGET(drawer->drawer)->state == PANEL_SHOWN) {
		gint x,y;
		gint wx, wy;

		gdk_window_get_origin (drawer->button->window, &wx, &wy);

		switch(drawer->orient) {
			case DRAWER_UP:
				x = wx;
				y = wy - drawer->drawer->allocation.height;
				break;
			case DRAWER_DOWN:
				x = wx;
				y = wy + drawer->button->allocation.height;
				break;
			case DRAWER_LEFT:
				x = wx - drawer->drawer->allocation.width;
				y = wy;
				break;
			case DRAWER_RIGHT:
				x = wx + drawer->button->allocation.width;
				y = wy;
				break;
		}

		gtk_widget_set_uposition(drawer->drawer,x,y);
	}
}

static gint
drawer_click(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = data;

	if(PANEL_WIDGET(drawer->drawer)->state == PANEL_SHOWN) {
		gtk_widget_hide(drawer->drawer);
		PANEL_WIDGET(drawer->drawer)->state = PANEL_HIDDEN;
	} else {
		PANEL_WIDGET(drawer->drawer)->state = PANEL_SHOWN;
		reposition_drawer(drawer);
		gtk_widget_show(drawer->drawer);
	}
	return TRUE;
}

static void
destroy_drawer(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = data;
	g_free(drawer);
}

Drawer *
create_drawer_applet(GtkWidget *window, GtkWidget * drawer_panel,
	DrawerOrient orient)
{
	GtkWidget *pixmap;
	Drawer *drawer;
	
	char *pixmap_name;

	drawer = g_new(Drawer,1);

	/*FIXME: drawers should have their own pixmaps I guess*/
	switch(orient) {
		case DRAWER_DOWN:
			pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-down.xpm");
			break;
		case DRAWER_UP:
			pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-up.xpm");
			break;
		case DRAWER_RIGHT:
			pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-right.xpm");
			break;
		case DRAWER_LEFT:
			pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-left.xpm");
			break;
	}
		
	drawer->orient = orient;

	/* main button */
	drawer->button = gtk_button_new ();
	
	/*make the pixmap*/
	pixmap = gnome_pixmap_new_from_file (pixmap_name);
	gtk_widget_show(pixmap);
	/*FIXME:this is not right, but it's how we can get the buttons to
	  be 48x48 (given the icons are 48x48)*/
	gtk_widget_set_usize (drawer->button, pixmap->requisition.width,
			      pixmap->requisition.height);

	/* put pixmap in button */
	gtk_container_add (GTK_CONTAINER(drawer->button), pixmap);
	gtk_widget_show (drawer->button);

	drawer->drawer = drawer_panel;

	gtk_signal_connect (GTK_OBJECT (drawer->button), "clicked",
			    GTK_SIGNAL_FUNC (drawer_click), drawer);
	gtk_signal_connect (GTK_OBJECT (drawer->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_drawer), drawer);

	gtk_object_set_user_data(GTK_OBJECT(drawer->button),drawer);

	/*FIXME: this don't work, make sure the drawer is shown if
	  it is supposed to be
	if(PANEL_WIDGET(drawer->drawer)->state == PANEL_SHOWN) {
		PANEL_WIDGET(drawer->drawer)->state = PANEL_HIDDEN;
		gtk_signal_connect_after(GTK_OBJECT(drawer->button),
					 "size_allocate",
					 GTK_SIGNAL_FUNC(drawer_click),
					 drawer);
	}
	*/

	g_free (pixmap_name);
	return drawer;
}

Drawer *
create_empty_drawer_applet(GtkWidget *window, DrawerOrient orient)
{
	if(orient == DRAWER_UP)
		return create_drawer_applet(window,panel_widget_new(0,
						PANEL_VERTICAL,
						PANEL_DRAWER,
						PANEL_EXPLICIT_HIDE,
						PANEL_SHOWN,
						global_config.explicit_hide_step_size,
						0, 0, 0, 0, 
						DROP_ZONE_LEFT),
					    orient);
	else if(orient == DRAWER_DOWN)
		return create_drawer_applet(window,panel_widget_new(0,
						PANEL_VERTICAL,
						PANEL_DRAWER,
						PANEL_EXPLICIT_HIDE,
						PANEL_SHOWN,
						global_config.explicit_hide_step_size,
						0, 0, 0, 0, 
						DROP_ZONE_RIGHT),
					    orient);
	else if(orient == DRAWER_LEFT)
		return create_drawer_applet(window,panel_widget_new(0,
						PANEL_HORIZONTAL,
						PANEL_DRAWER,
						PANEL_EXPLICIT_HIDE,
						PANEL_SHOWN,
						global_config.explicit_hide_step_size,
						0, 0, 0, 0, 
						DROP_ZONE_LEFT),
					    orient);
	else if(orient == DRAWER_RIGHT)
		return create_drawer_applet(window,panel_widget_new(0,
						PANEL_HORIZONTAL,
						PANEL_DRAWER,
						PANEL_EXPLICIT_HIDE,
						PANEL_SHOWN,
						global_config.explicit_hide_step_size,
						0, 0, 0, 0, 
						DROP_ZONE_RIGHT),
					    orient);

}

void
set_drawer_applet_orient(Drawer *drawer, DrawerOrient orient)
{
	GtkWidget *pixmap;
	char *pixmap_name;

	g_return_if_fail(drawer!=NULL);

	drawer->orient = orient;

	switch (drawer->orient) {
		case DRAWER_DOWN:
			pixmap_name = gnome_unconditional_pixmap_file(
				"gnome-menu-down.xpm");
			break;
		case DRAWER_UP:
			pixmap_name = gnome_unconditional_pixmap_file(
				"gnome-menu-up.xpm");
			break;
		case DRAWER_RIGHT:
			pixmap_name = gnome_unconditional_pixmap_file(
				"gnome-menu-right.xpm");
			break;
		case DRAWER_LEFT:
			pixmap_name = gnome_unconditional_pixmap_file(
				"gnome-menu-left.xpm");
			break;
	}
		
	pixmap=GTK_BUTTON(drawer->button)->child;
	gtk_container_remove(GTK_CONTAINER(drawer->button),pixmap);
	gtk_widget_destroy(pixmap);

	/*make the pixmap*/
	pixmap = gnome_pixmap_new_from_file (pixmap_name);

	gtk_container_add (GTK_CONTAINER(drawer->button), pixmap);
	gtk_widget_show (pixmap);
	
	g_free(pixmap_name);
}
