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
#include <gnome.h>

#include "panel-include.h"

#define DRAWER_PROPERTIES "drawer_properties"

extern GArray *applets;
extern int applet_count;
extern GlobalConfig global_config;

extern GtkTooltips *panel_tooltips;

extern GList *panel_list;

static void
properties_apply_callback(GtkWidget *widget, int page, gpointer data)
{
	Drawer        *drawer = data;
	GtkWidget     *pixentry = gtk_object_get_data(GTK_OBJECT(widget),
						      "pixmap");
	GtkWidget     *tipentry = gtk_object_get_data(GTK_OBJECT(widget),
						      "tooltip");
	char         *s;

	if (page != -1)
		return;

	if(drawer->pixmap)
		g_free(drawer->pixmap);
	if(drawer->tooltip)
		g_free(drawer->tooltip);
	s = gtk_entry_get_text(GTK_ENTRY(pixentry));
	if(!s || !*s)
		drawer->pixmap =
			gnome_unconditional_pixmap_file ("panel-drawer.png");
	else
		drawer->pixmap = g_strdup(s);
	s = gtk_entry_get_text(GTK_ENTRY(tipentry));
	if(!s || !*s)
		drawer->tooltip = NULL;
	else
		drawer->tooltip = g_strdup(s);

	gtk_tooltips_set_tip (panel_tooltips,drawer->button,
			      drawer->tooltip,NULL);
	
	/*it also might be a button/label (in the future)*/
	if(GNOME_IS_PIXMAP(drawer->button))
		gnome_pixmap_load_file (GNOME_PIXMAP(drawer->button),
					drawer->pixmap);
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
	/*gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);*/
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
	panel = gtk_object_get_data(GTK_OBJECT(drawer->button),
				    PANEL_APPLET_PARENT_KEY);

	gdk_window_get_origin (drawer->button->window, &bx, &by);
	gdk_window_get_size (drawer->button->window, &bw, &bh);
	if(drawer->drawer->window)
		gdk_window_get_size (drawer->drawer->window, &dw, &dh);
	else
		dw = dh = 48;
	gdk_window_get_origin (GTK_WIDGET(panel)->window, &px, &py);
	gdk_window_get_size (GTK_WIDGET(panel)->window, &pw, &ph);
	
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
	
	drawer_widget_set_pos(DRAWER_WIDGET(drawer->drawer),x,y);
}

static void
drawer_click(Drawer *drawer)
{
	DrawerWidget *drawerw = DRAWER_WIDGET(drawer->drawer);
	PanelWidget *parent =
		gtk_object_get_data(GTK_OBJECT(drawer->button),
				    PANEL_APPLET_PARENT_KEY);
	GtkWidget *panelw = gtk_object_get_data(GTK_OBJECT(parent),
						PANEL_PARENT);
	
	reposition_drawer(drawer);

	if(drawerw->state == DRAWER_SHOWN) {
		drawer_widget_close_drawer(DRAWER_WIDGET(drawer->drawer));
		if(IS_SNAPPED_WIDGET(panelw))
			SNAPPED_WIDGET(panelw)->drawers_open++;
	} else {
		drawer_widget_open_drawer(DRAWER_WIDGET(drawer->drawer));
		if(IS_SNAPPED_WIDGET(panelw))
			SNAPPED_WIDGET(panelw)->drawers_open--;
	}
}

static int
drawer_event (GtkWidget *widget, GdkEvent *event, void *data)
{
	static int in_button = FALSE;
	Drawer *drawer = data;
	GdkEventButton *bevent = (GdkEventButton *)event;

	/*this is a release after a press inside a button*/
	if(event->type == GDK_BUTTON_RELEASE && in_button) {
		in_button = FALSE;
		gtk_grab_remove(widget);
		/*if it's inside it's a click*/
		if(bevent->x >= 0 &&
		   bevent->x < widget->allocation.width &&
		   bevent->y >= 0 &&
		   bevent->y < widget->allocation.height)
			drawer_click (drawer);
		return TRUE;
	/*if it's inside and press, set in_button*/
	} else if(event->type == GDK_BUTTON_PRESS &&
		  bevent->button == 1 &&
		  bevent->x >= 0 &&
		  bevent->x < widget->allocation.width &&
		  bevent->y >= 0 &&
		  bevent->y < widget->allocation.height) {
		gtk_grab_add(widget);
		in_button = TRUE;
		return TRUE;
	}
	return FALSE;
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

static Drawer *
create_drawer_applet(GtkWidget * drawer_panel, char *tooltip, char *pixmap,
		     PanelOrientType orient)
{
	Drawer *drawer;
	
	drawer = g_new(Drawer,1);

	if(!tooltip ||
	   !*tooltip)
		drawer->tooltip = NULL;
	else
		drawer->tooltip = g_strdup(tooltip);
	if(!pixmap ||
	   !*pixmap)
		drawer->pixmap =
			gnome_unconditional_pixmap_file ("panel-drawer.png");
	else
		drawer->pixmap = g_strdup(pixmap);

	/*we need to overlay the arrow*/
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

	/*make the pixmap*/
	drawer->button = gnome_pixmap_new_from_file (drawer->pixmap);
	gtk_widget_set_events(drawer->button,
			      gtk_widget_get_events(drawer->button) |
			      GDK_LEAVE_NOTIFY_MASK |
			      GDK_ENTER_NOTIFY_MASK |
			      GDK_BUTTON_PRESS_MASK |
			      GDK_BUTTON_RELEASE_MASK);
	gtk_widget_show(drawer->button);

	drawer->drawer = drawer_panel;

	gtk_signal_connect (GTK_OBJECT (drawer->button), "event",
			    GTK_SIGNAL_FUNC (drawer_event), drawer);
	gtk_signal_connect (GTK_OBJECT (drawer->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_drawer), drawer);
	gtk_signal_connect (GTK_OBJECT (drawer->button), "enter_notify_event",
			    GTK_SIGNAL_FUNC (enter_notify_drawer), drawer);

	gtk_object_set_user_data(GTK_OBJECT(drawer->button),drawer);
	gtk_object_set_data(GTK_OBJECT(drawer_panel),DRAWER_PANEL_KEY,drawer);

	return drawer;
}

static Drawer *
create_empty_drawer_applet(char *tooltip, char *pixmap,
			   PanelOrientType orient)
{
	PanelOrientation porient;
	DrawerDropZonePos drop_pos;
	switch(orient) {
	case ORIENT_UP:
		porient = PANEL_VERTICAL;
		drop_pos = DROP_ZONE_LEFT;
		break;
	case ORIENT_DOWN:
		porient = PANEL_VERTICAL;
		drop_pos = DROP_ZONE_RIGHT;
		break;
	case ORIENT_LEFT:
		porient = PANEL_HORIZONTAL;
		drop_pos = DROP_ZONE_LEFT;
		break;
	case ORIENT_RIGHT:
		porient = PANEL_HORIZONTAL;
		drop_pos = DROP_ZONE_RIGHT;
		break;
	default:
		return NULL;
	}
	return create_drawer_applet(drawer_widget_new(PANEL_HORIZONTAL,
						      DRAWER_SHOWN,
						      DROP_ZONE_RIGHT,
						      PANEL_BACK_NONE, NULL,
						      TRUE, NULL),
				    tooltip,pixmap,
				    orient);
}

void
set_drawer_applet_orient(Drawer *drawer, PanelOrientType orient)
{
	PanelOrientation porient;
	DrawerDropZonePos drop_pos;

	g_return_if_fail(drawer!=NULL);
	
	/*ignore orient events until we are realized, this will only
	  be the initial one and we have already set the orientation*/
	if(!GTK_WIDGET_REALIZED(drawer->drawer))
		return;

	drawer->orient = orient;

	switch(drawer->orient) {
	case ORIENT_UP:
		porient = PANEL_VERTICAL;
		drop_pos = DROP_ZONE_LEFT;
		break;
	case ORIENT_DOWN:
		porient = PANEL_VERTICAL;
		drop_pos = DROP_ZONE_RIGHT;
		break;
	case ORIENT_LEFT:
		porient = PANEL_HORIZONTAL;
		drop_pos = DROP_ZONE_LEFT;
		break;
	case ORIENT_RIGHT:
		porient = PANEL_HORIZONTAL;
		drop_pos = DROP_ZONE_RIGHT;
		break;
	}
	drawer_widget_change_orient(DRAWER_WIDGET(drawer->drawer),
				    porient);
	drawer_widget_change_drop_zone_pos(DRAWER_WIDGET(drawer->drawer),
					   drop_pos);
	
	/*this was when we were chaning the pixmap on the fly*/

		/*pixmap_name = gnome_unconditional_pixmap_file("gnome-menu-left.png");*/
		
	/*pixmap=GTK_BUTTON(drawer->button)->child;
	gtk_container_remove(GTK_CONTAINER(drawer->button),pixmap);

	pixmap = gnome_pixmap_new_from_file (pixmap_name);

	gtk_container_add (GTK_CONTAINER(drawer->button), pixmap);
	gtk_widget_show (pixmap);
	
	g_free(pixmap_name);*/
}

static void
drawer_realize_cb(GtkWidget *button, Drawer *drawer)
{
	reposition_drawer(drawer);
	if(DRAWER_WIDGET(drawer->drawer)->state == DRAWER_SHOWN)
		gtk_widget_show(drawer->drawer);
	else {
		if(!GTK_WIDGET_REALIZED(drawer->drawer))
			gtk_widget_realize(drawer->drawer);
		gtk_widget_hide(drawer->drawer);
	}
}

static void
drawer_move_foreach(GtkWidget *w, gpointer user_data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(w),
							    "applet_id"));
	AppletInfo *info = get_applet_info(applet_id);
	
	if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		DrawerWidget *dw = DRAWER_WIDGET(drawer->drawer);
		reposition_drawer(drawer);
		gtk_container_foreach(GTK_CONTAINER(dw->panel),
				      drawer_move_foreach,
				      NULL);
	}
}

static void
button_size_alloc(GtkWidget *widget, GtkAllocation *alloc, Drawer *drawer)
{
	if(!GTK_WIDGET_REALIZED(widget))
		return;
	drawer_move_foreach(widget,NULL);
}

void
load_drawer_applet(char *params, char *pixmap, char *tooltip,
		   int pos, PanelWidget *panel)
{
	Drawer *drawer;

	if(!params) {
		drawer = create_empty_drawer_applet(tooltip,pixmap,
						    get_applet_orient(panel));
		if(drawer) panel_setup(drawer->drawer);
	} else {
		int i;
		PanelData *dr_pd;

		sscanf(params,"%d",&i);
		dr_pd = g_list_nth(panel_list,i)->data;

		drawer=create_drawer_applet(dr_pd->panel,
					    tooltip,pixmap,
					    get_applet_orient(panel));
	}

	if(!drawer)
		return;

	g_return_if_fail(drawer != NULL);

	register_toy(drawer->button,drawer, pos, panel, APPLET_DRAWER);

	gtk_signal_connect_after(GTK_OBJECT(drawer->button),
				 "size_allocate",
				 GTK_SIGNAL_FUNC(button_size_alloc),
				 drawer);

	if(DRAWER_WIDGET(drawer->drawer)->state == DRAWER_SHOWN) {
		GtkWidget *wpanel;
		/*pop up, if popped down*/
		wpanel = gtk_object_get_data(GTK_OBJECT(panel),
					     PANEL_PARENT);
		if(IS_SNAPPED_WIDGET(wpanel)) {
			/*drawer is open so we track it*/
			SNAPPED_WIDGET(wpanel)->drawers_open++;
			snapped_widget_pop_up(SNAPPED_WIDGET(wpanel));
		}
	} 

	panel_widget_add_forbidden(PANEL_WIDGET(DRAWER_WIDGET(drawer->drawer)->panel));

	gtk_tooltips_set_tip (panel_tooltips,drawer->button,
			      drawer->tooltip,NULL);
	if(GTK_WIDGET_REALIZED(drawer->button)) {
		reposition_drawer(drawer);
		if(DRAWER_WIDGET(drawer->drawer)->state == DRAWER_SHOWN)
			gtk_widget_show(drawer->drawer);
		else {
			/*hmm ... weird but it works*/
			gtk_widget_set_uposition(drawer->drawer,
						 -100,-100);
			gtk_widget_show(drawer->drawer);
			/*gtk_widget_realize(drawer->drawer);*/
			gtk_widget_hide(drawer->drawer);
		}
	} else
		gtk_signal_connect_after(GTK_OBJECT(drawer->button),
					 "realize",
					 GTK_SIGNAL_FUNC(drawer_realize_cb),
					 drawer);
	gtk_signal_connect_after(GTK_OBJECT(drawer->drawer),
				 "realize",
				 GTK_SIGNAL_FUNC(drawer_realize_cb),
				 drawer);

	applet_add_callback(applet_count-1,"properties",
			    GNOME_STOCK_MENU_PROP,
			    _("Properties..."));
}

