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
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include "gnome.h"
#include "panel-widget.h"
#include "panel-util.h"
#include "panel.h"
#include "panel_config_global.h"
#include "drawer.h"
#include "panel-util.h"

#define DRAWER_PROPERTIES "drawer_properties"

extern GArray *applets;
extern int applet_count;
extern GlobalConfig global_config;

extern GtkTooltips *panel_tooltips;
static char *default_drawer_pixmap=NULL;

static void
properties_apply_callback(GtkWidget *widget, int page, gpointer data)
{
	Drawer        *drawer = data;
	GtkWidget     *pixmap;
	GtkWidget     *pixentry = gtk_object_get_data(GTK_OBJECT(widget),"pixmap");
	GtkWidget     *tipentry = gtk_object_get_data(GTK_OBJECT(widget),"tooltip");
	char         *s;

	if (page != -1)
		return;

	if(drawer->pixmap)
		g_free(drawer->pixmap);
	if(drawer->tooltip)
		g_free(drawer->tooltip);
	s = gtk_entry_get_text(GTK_ENTRY(pixentry));
	if(!s || !*s)
		drawer->pixmap = gnome_unconditional_pixmap_file ("gnome-default.png");
	else
		drawer->pixmap = g_strdup(s);
	s = gtk_entry_get_text(GTK_ENTRY(tipentry));
	if(!s || !*s)
		drawer->tooltip = NULL;
	else
		drawer->tooltip = g_strdup(s);

	gtk_tooltips_set_tip (panel_tooltips,drawer->button->parent,
			      drawer->tooltip,NULL);
	
	pixmap=GTK_BUTTON(drawer->button)->child;

	gtk_container_remove(GTK_CONTAINER(drawer->button),pixmap);

	pixmap = gnome_pixmap_new_from_file (drawer->pixmap);
	if (!pixmap) {
		if (default_drawer_pixmap)
			pixmap = gnome_pixmap_new_from_file (default_drawer_pixmap);
		else
			pixmap = gtk_label_new (_("Drawer"));
	}
	gtk_container_add (GTK_CONTAINER(drawer->button), pixmap);
	gtk_widget_show(pixmap);

	/*FIXME: a bad hack to keep it all 48x48*/
	gtk_widget_set_usize (drawer->button, 48, 48);

	/*gtk_widget_set_usize (prop->launcher->button, pixmap->requisition.width,
			      pixmap->requisition.height);*/
}

static int
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = data;
	GtkWidget *pixentry = gtk_object_get_data(GTK_OBJECT(widget),"pixmap");
	GtkWidget *tipentry = gtk_object_get_data(GTK_OBJECT(widget),"tooltip");
	gtk_object_set_data(GTK_OBJECT(drawer->button),
			    DRAWER_PROPERTIES,NULL);
	gtk_signal_disconnect_by_data(GTK_OBJECT(pixentry),widget);
	gtk_signal_disconnect_by_data(GTK_OBJECT(tipentry),widget);
	return FALSE;
}
static GtkWidget *
create_properties_dialog(Drawer *drawer)
{
	GtkWidget  *dialog;
	GtkWidget  *table;
	GtkWidget  *w;

	dialog = gnome_property_box_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Drawer properties"));
	gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);

	table = gtk_table_new(2, 2, FALSE);
	gtk_container_border_width(GTK_CONTAINER(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);

	w = create_text_entry(table, "drawer_name", 0, _("Tooltip/Name"),
			      drawer->tooltip, dialog);
	gtk_object_set_data(GTK_OBJECT(dialog),"tooltip",w);
	
	w = create_file_entry(table, "icon", 1, _("Icon"), drawer->pixmap,
			      dialog);
	gtk_object_set_data(GTK_OBJECT(dialog),"pixmap",w);

	gnome_property_box_append_page (GNOME_PROPERTY_BOX (dialog),
					table, gtk_label_new (_("Drawer")));
	
	gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
			   (GtkSignalFunc) properties_close_callback,
			   drawer);

	gtk_signal_connect(GTK_OBJECT(dialog), "apply",
			   GTK_SIGNAL_FUNC(properties_apply_callback),
			   drawer);

	return dialog;
}

void
drawer_properties(Drawer *drawer)
{
	GtkWidget         *dialog;

	dialog = gtk_object_get_data(GTK_OBJECT(drawer->button),
				     DRAWER_PROPERTIES);
	if(dialog) {
		gdk_window_raise(dialog->window);
		return;
	}

	dialog = create_properties_dialog(drawer);
	gtk_object_set_data(GTK_OBJECT(drawer->button),
			    DRAWER_PROPERTIES,dialog);
	gtk_widget_show_all (dialog);
}


void
reposition_drawer(Drawer *drawer)
{
	int x=0,y=0;
	int bx, by, bw, bh;
	int dw, dh;
	int px, py, pw, ph;
	PanelWidget *panel; /*parent panel*/

	/*get's the panel data from the event box that is the applet's
	  parent*/
	panel = gtk_object_get_data(GTK_OBJECT(drawer->button->parent),
				    PANEL_APPLET_PARENT_KEY);

	gdk_window_get_origin (drawer->button->window, &bx, &by);
	gdk_window_get_size (drawer->button->window, &bw, &bh);
	gdk_window_get_size (drawer->drawer->window, &dw, &dh);
	gdk_window_get_geometry (GTK_WIDGET(panel)->window, &px, &py, &pw, &ph,
				 NULL);

	switch(drawer->orient) {
		case ORIENT_UP:
			x = bx+(bw-dw)/2;
			y = py - dh;
			break;
		case ORIENT_DOWN:
			x = bx+(bw-dw)/2;
			y = py + ph;
			break;
		case ORIENT_LEFT:
			x = px - dw;
			y = by+(bh-dh)/2;
			break;
		case ORIENT_RIGHT:
			x = px + pw;
			y = by+(bh-dh)/2;
			break;
	}

	panel_widget_set_drawer_pos(PANEL_WIDGET(drawer->drawer),x,y);
}

static int
drawer_click(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = data;

	reposition_drawer(drawer);

	if(PANEL_WIDGET(drawer->drawer)->state == PANEL_SHOWN)
		panel_widget_close_drawer(PANEL_WIDGET(drawer->drawer));
	else
		panel_widget_open_drawer(PANEL_WIDGET(drawer->drawer));
	return TRUE;
}

static int
destroy_drawer(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = data;
	g_free(drawer);
	return FALSE;
}

static int
enter_notify_drawer(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	Drawer *drawer = data;
	/*FIXME: do we want this autoraise piece?*/
	gdk_window_raise(drawer->drawer->window);
	return TRUE;
}

Drawer *
create_drawer_applet(GtkWidget * drawer_panel, char *tooltip, char *pixmap,
		     PanelOrientType orient)
{
	GtkWidget *pix;
	Drawer *drawer;
	
	drawer = g_new(Drawer,1);

	if (!default_drawer_pixmap)
		default_drawer_pixmap = gnome_pixmap_file ("gnome-unknown.png");

	if(!tooltip ||
	   !*tooltip)
		drawer->tooltip = NULL;
	else
		drawer->tooltip = g_strdup(tooltip);
	if(!pixmap ||
	   !*pixmap)
		drawer->pixmap =
			gnome_unconditional_pixmap_file ("gnome-default.png");
	else
		drawer->pixmap = g_strdup(pixmap);

	/*well ... I think we should just drop this*/
	/*switch(orient) {
		case ORIENT_DOWN:
			pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-down.png");
			break;
		case ORIENT_UP:
			pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-up.png");
			break;
		case ORIENT_RIGHT:
			pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-right.png");
			break;
		case ORIENT_LEFT:
			pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-left.png");
			break;
	}*/
		
	drawer->orient = orient;

	/* main button */
	drawer->button = gtk_button_new ();
	
	/*make the pixmap*/
	pix = gnome_pixmap_new_from_file (drawer->pixmap);
	if (!pix) {
		if (default_drawer_pixmap)
			pix = gnome_pixmap_new_from_file (default_drawer_pixmap);
		else
			pix = gtk_label_new (_("Drawer"));
	}
	gtk_widget_show(pix);
	/*FIXME:this is not right, but it's how we can get the buttons to
	  be 48x48 (given the icons are 48x48)*/
	gtk_widget_set_usize (drawer->button,48,48);
	/*gtk_widget_set_usize (drawer->button, pixmap->requisition.width,
			      pixmap->requisition.height);*/

	/* put pixmap in button */
	gtk_container_add (GTK_CONTAINER(drawer->button), pix);
	gtk_widget_show (drawer->button);

	drawer->drawer = drawer_panel;

	gtk_signal_connect (GTK_OBJECT (drawer->button), "clicked",
			    GTK_SIGNAL_FUNC (drawer_click), drawer);
	gtk_signal_connect (GTK_OBJECT (drawer->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_drawer), drawer);
	gtk_signal_connect (GTK_OBJECT (drawer->button), "enter_notify_event",
			    GTK_SIGNAL_FUNC (enter_notify_drawer), drawer);

	gtk_object_set_user_data(GTK_OBJECT(drawer->button),drawer);

	if(PANEL_WIDGET(drawer_panel)->state == PANEL_SHOWN)
		gtk_widget_show(drawer_panel);
	else
		gtk_widget_hide(drawer_panel);

	gtk_object_set_data(GTK_OBJECT(drawer_panel),DRAWER_PANEL,drawer);
	return drawer;
}

Drawer *
create_empty_drawer_applet(char *tooltip, char *pixmap,
			   PanelOrientType orient)
{
	switch(orient) {
	case ORIENT_UP:
		return create_drawer_applet(panel_widget_new(0,
						PANEL_VERTICAL,
						PANEL_DRAWER,
						PANEL_EXPLICIT_HIDE,
						PANEL_SHOWN,
						0, 0, 
						DROP_ZONE_LEFT,
						PANEL_BACK_NONE, NULL, TRUE, NULL),
					    tooltip,pixmap,
					    orient);
	case ORIENT_DOWN:
		return create_drawer_applet(panel_widget_new(0,
						PANEL_VERTICAL,
						PANEL_DRAWER,
						PANEL_EXPLICIT_HIDE,
						PANEL_SHOWN,
						0, 0, 
						DROP_ZONE_RIGHT,
						PANEL_BACK_NONE, NULL, TRUE, NULL),
					    tooltip,pixmap,
					    orient);
	case ORIENT_LEFT:
		return create_drawer_applet(panel_widget_new(0,
						PANEL_HORIZONTAL,
						PANEL_DRAWER,
						PANEL_EXPLICIT_HIDE,
						PANEL_SHOWN,
						0, 0, 
						DROP_ZONE_LEFT,
						PANEL_BACK_NONE, NULL, TRUE, NULL),
					    tooltip,pixmap,
					    orient);
	case ORIENT_RIGHT:
		return create_drawer_applet(panel_widget_new(0,
						PANEL_HORIZONTAL,
						PANEL_DRAWER,
						PANEL_EXPLICIT_HIDE,
						PANEL_SHOWN,
						0, 0, 
						DROP_ZONE_RIGHT,
						PANEL_BACK_NONE, NULL, TRUE, NULL),
					    tooltip,pixmap,
					    orient);
	}
	return NULL;
}

void
set_drawer_applet_orient(Drawer *drawer, PanelOrientType orient)
{
	g_return_if_fail(drawer!=NULL);

	drawer->orient = orient;

	switch (drawer->orient) {
	case ORIENT_DOWN:
		/*pixmap_name = gnome_unconditional_pixmap_file("gnome-menu-down.png");*/
		panel_widget_change_orient(PANEL_WIDGET(drawer->drawer),
					   PANEL_VERTICAL);
		panel_widget_change_drop_zone_pos(PANEL_WIDGET(drawer->
							       drawer),
						  DROP_ZONE_RIGHT);

		break;
	case ORIENT_UP:
		/*pixmap_name = gnome_unconditional_pixmap_file("gnome-menu-up.png");*/
		panel_widget_change_orient(PANEL_WIDGET(drawer->drawer),
					   PANEL_VERTICAL);
		panel_widget_change_drop_zone_pos(PANEL_WIDGET(drawer->
							       drawer),
						  DROP_ZONE_LEFT);
		break;
	case ORIENT_RIGHT:
		/*pixmap_name = gnome_unconditional_pixmap_file("gnome-menu-right.png");*/
		panel_widget_change_orient(PANEL_WIDGET(drawer->drawer),
					   PANEL_HORIZONTAL);
		panel_widget_change_drop_zone_pos(PANEL_WIDGET(drawer->
							       drawer),
						  DROP_ZONE_RIGHT);
		break;
	case ORIENT_LEFT:
		/*pixmap_name = gnome_unconditional_pixmap_file("gnome-menu-left.png");*/
		panel_widget_change_orient(PANEL_WIDGET(drawer->drawer),
					   PANEL_HORIZONTAL);
		panel_widget_change_drop_zone_pos(PANEL_WIDGET(drawer->
							       drawer),
						  DROP_ZONE_LEFT);
		break;
	}
		
	/*pixmap=GTK_BUTTON(drawer->button)->child;
	gtk_container_remove(GTK_CONTAINER(drawer->button),pixmap);

	pixmap = gnome_pixmap_new_from_file (pixmap_name);

	gtk_container_add (GTK_CONTAINER(drawer->button), pixmap);
	gtk_widget_show (pixmap);
	
	g_free(pixmap_name);*/
}
