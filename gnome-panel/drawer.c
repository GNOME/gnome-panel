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

/* Used for all the packing and padding options */
#define CONFIG_PADDING_SIZE 3

extern GList *applets;
extern GList *applets_last;
extern int applet_count;
extern GlobalConfig global_config;

extern GtkTooltips *panel_tooltips;

extern GList *panel_list;

static void
properties_apply_callback(GtkWidget *widget, int page, gpointer data)
{
	Drawer       *drawer = data;
	GtkWidget    *pixentry = gtk_object_get_data(GTK_OBJECT(widget),
						      "pixmap");
	GtkWidget    *tipentry = gtk_object_get_data(GTK_OBJECT(widget),
						      "tooltip");
	char         *s;

	if (page != -1)
		return;

	if(drawer->pixmap)
		g_free(drawer->pixmap);
	if(drawer->tooltip)
		g_free(drawer->tooltip);
	s = gnome_pixmap_entry_get_filename(GNOME_PIXMAP_ENTRY(pixentry));
	if(!s || !*s) {
		drawer->pixmap =
			gnome_unconditional_pixmap_file ("panel-drawer.png");
		button_widget_set_pixmap_from_file (BUTTON_WIDGET(drawer->button),
						    drawer->pixmap);
	} else {
		if(button_widget_set_pixmap_from_file(BUTTON_WIDGET(drawer->button), s))
			drawer->pixmap = g_strdup(s);
		else {
			drawer->pixmap =
				gnome_unconditional_pixmap_file ("panel-drawer.png");
			button_widget_set_pixmap_from_file (BUTTON_WIDGET(drawer->button),
							    drawer->pixmap);
		}
	}
	g_free(s);
	s = gtk_entry_get_text(GTK_ENTRY(tipentry));
	if(!s || !*s)
		drawer->tooltip = NULL;
	else
		drawer->tooltip = g_strdup(s);

	gtk_tooltips_set_tip (panel_tooltips,drawer->button,
			      drawer->tooltip,NULL);
}

static int
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = data;
	/*GtkWidget *pixentry = gtk_object_get_data(GTK_OBJECT(widget),"pixmap");
	GtkWidget *gtkpixentry =
		gnome_pixmap_entry_gtk_entry(GNOME_PIXMAP_ENTRY(pixentry));
	GtkWidget *tipentry = gtk_object_get_data(GTK_OBJECT(widget),"tooltip");*/
	gtk_object_set_data(GTK_OBJECT(drawer->button),
			    DRAWER_PROPERTIES,NULL);
	/*gtk_signal_disconnect_by_data(GTK_OBJECT(gtkpixentry),widget);
	gtk_signal_disconnect_by_data(GTK_OBJECT(tipentry),widget);*/
	return FALSE;
}

static void
set_toggle_not (GtkWidget *widget, gpointer data)
{
	PerPanelConfig *ppc = gtk_object_get_user_data(GTK_OBJECT(widget));
	int *the_toggle = data;

	*the_toggle = !(GTK_TOGGLE_BUTTON(widget)->active);
	if (ppc->register_changes)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (ppc->config_window));
}

void
add_drawer_properties_page(PerPanelConfig *ppc, Drawer *drawer)
{
        GtkWidget *dialog = ppc->config_window;
        GtkWidget *table;
	GtkWidget *f;
	GtkWidget *box, *box_in;
	GtkWidget *nbook;
	GtkWidget *w;
	GtkWidget *button;
	
	table = gtk_table_new(2, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);

	w = create_text_entry(table, "drawer_name", 0, _("Tooltip/Name"),
			      drawer->tooltip, dialog);
	gtk_object_set_data(GTK_OBJECT(dialog),"tooltip",w);
	
	w = create_pixmap_entry(table, "icon", 1, _("Icon"), drawer->pixmap,
				dialog,64,64);
	gtk_object_set_data(GTK_OBJECT(dialog),"pixmap",w);

	f = gtk_frame_new(_("Applet appearance"));
	gtk_container_add(GTK_CONTAINER(f),table);

	box = gtk_vbox_new(FALSE,5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_box_pack_start(GTK_BOX(box),f,FALSE,FALSE,0);

	f = gtk_frame_new(_("Drawer handle"));
	box_in = gtk_vbox_new(FALSE,5);
	button = gtk_check_button_new_with_label (_("Disable hidebutton"));
	gtk_object_set_user_data(GTK_OBJECT(button),ppc);
	if (!ppc->drawer_hidebutton)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_not),
			    &ppc->drawer_hidebutton);
	gtk_box_pack_start (GTK_BOX (box_in), button, TRUE, FALSE,
			    CONFIG_PADDING_SIZE);

	button = gtk_check_button_new_with_label (_("Disable hidebutton arrow"));
	gtk_object_set_user_data(GTK_OBJECT(button),ppc);
	if (!ppc->drawer_hidebutton_pixmap)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_not),
			    &ppc->drawer_hidebutton_pixmap);
	gtk_box_pack_start (GTK_BOX (box_in), button, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);
	gtk_container_add(GTK_CONTAINER(f),box_in);
	gtk_box_pack_start (GTK_BOX (box),f,FALSE,FALSE,0);

	
	nbook = GNOME_PROPERTY_BOX (dialog)->notebook;
	gtk_notebook_append_page (GTK_NOTEBOOK(nbook),
				  box, gtk_label_new (_("Drawer")));
	
	gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
			   (GtkSignalFunc) properties_close_callback,
			   drawer);

	gtk_signal_connect(GTK_OBJECT(dialog), "apply",
			   GTK_SIGNAL_FUNC(properties_apply_callback),
			   drawer);

	gtk_object_set_data(GTK_OBJECT(drawer->button),
			    DRAWER_PROPERTIES,dialog);
}

static void
drawer_click(GtkWidget *w, Drawer *drawer)
{
	DrawerWidget *drawerw = DRAWER_WIDGET(drawer->drawer);
	PanelWidget *parent = PANEL_WIDGET(drawer->button->parent);
	GtkWidget *panelw = gtk_object_get_data(GTK_OBJECT(parent),
						PANEL_PARENT);
	
	gtk_widget_queue_resize(drawer->drawer);

	if(drawerw->state == DRAWER_SHOWN) {
		drawer_widget_close_drawer(DRAWER_WIDGET(drawer->drawer));
		if(IS_SNAPPED_WIDGET(panelw))
			SNAPPED_WIDGET(panelw)->drawers_open--;
	} else {
		drawer_widget_open_drawer(DRAWER_WIDGET(drawer->drawer));
		if(IS_SNAPPED_WIDGET(panelw))
			SNAPPED_WIDGET(panelw)->drawers_open++;
	}
}

static int
destroy_drawer(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = data;
	GtkWidget *prop_dialog = gtk_object_get_data(GTK_OBJECT(drawer->button),
						     DRAWER_PROPERTIES);
	if(prop_dialog)
		gtk_widget_destroy(prop_dialog);
	g_free(drawer);
	return FALSE;
}

static int
enter_notify_drawer(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
  Drawer *drawer = data;

  if (!gnome_win_hints_wm_exists())
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

	if(!pixmap || !*pixmap) {
		drawer->pixmap =
			gnome_unconditional_pixmap_file ("panel-drawer.png");
		drawer->button = button_widget_new_from_file (drawer->pixmap,
							      DRAWER_TILE,
							      TRUE,orient);
	} else {
		drawer->button = button_widget_new_from_file(pixmap,
							     DRAWER_TILE,
							     TRUE,orient);
		if(drawer->button)
			drawer->pixmap = g_strdup(pixmap);
		else {
			drawer->pixmap =
				gnome_unconditional_pixmap_file ("panel-drawer.png");
			drawer->button =
				button_widget_new_from_file (drawer->pixmap,
							     DRAWER_TILE,
							     TRUE,orient);
		}
	}

	gtk_widget_show(drawer->button);

	drawer->drawer = drawer_panel;

	gtk_signal_connect (GTK_OBJECT (drawer->button), "clicked",
			    GTK_SIGNAL_FUNC (drawer_click), drawer);
	gtk_signal_connect (GTK_OBJECT (drawer->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_drawer), drawer);
	gtk_signal_connect (GTK_OBJECT (drawer->button), "enter_notify_event",
			    GTK_SIGNAL_FUNC (enter_notify_drawer), drawer);

	gtk_object_set_user_data(GTK_OBJECT(drawer->button),drawer);
	gtk_object_set_data(GTK_OBJECT(drawer_panel),DRAWER_PANEL_KEY,drawer);
	gtk_widget_queue_resize(GTK_WIDGET(drawer_panel));

	return drawer;
}

static Drawer *
create_empty_drawer_applet(char *tooltip, char *pixmap,
			   PanelOrientType orient)
{
	return create_drawer_applet(drawer_widget_new(orient,
						      DRAWER_SHOWN,
						      PANEL_BACK_NONE, NULL,
						      TRUE, NULL, TRUE, TRUE),
				    tooltip,pixmap,orient);
}

void
set_drawer_applet_orient(Drawer *drawer, PanelOrientType orient)
{
	g_return_if_fail(drawer!=NULL);

	button_widget_set_params(BUTTON_WIDGET(drawer->button),
				 DRAWER_TILE,TRUE,orient);
	
	/*ignore orient events until we are realized, this will only
	  be the initial one and we have already set the orientation*/
	if(!GTK_WIDGET_REALIZED(drawer->drawer))
		return;
	
	drawer_widget_change_orient(DRAWER_WIDGET(drawer->drawer), orient);
}

static void
drawer_realize_cb(GtkWidget *button, Drawer *drawer)
{
	gtk_widget_queue_resize(drawer->drawer);
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
	AppletInfo *info = gtk_object_get_data(GTK_OBJECT(w), "applet_info");
	
	if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		DrawerWidget *dw = DRAWER_WIDGET(drawer->drawer);
		gtk_widget_queue_resize(drawer->drawer);
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
		   PanelWidget *panel, int pos)
{
	Drawer *drawer;
	PanelOrientType orient = get_applet_orient(panel);

	if(!params) {
		drawer = create_empty_drawer_applet(tooltip,pixmap,orient);
		if(drawer) panel_setup(drawer->drawer);
	} else {
		int i;
		PanelData *dr_pd;

		sscanf(params,"%d",&i);
		dr_pd = g_list_nth(panel_list,i)->data;
		

		drawer=create_drawer_applet(dr_pd->panel, tooltip,pixmap,
					    orient);

		drawer_widget_change_orient(DRAWER_WIDGET(dr_pd->panel),
					    orient);
	}

	if(!drawer)
		return;

	g_return_if_fail(drawer != NULL);

	register_toy(drawer->button,drawer, panel, pos, APPLET_DRAWER);

	gtk_signal_connect_after(GTK_OBJECT(drawer->button),
				 "size_allocate",
				 GTK_SIGNAL_FUNC(button_size_alloc),
				 drawer);

	if(DRAWER_WIDGET(drawer->drawer)->state == DRAWER_SHOWN) {
		GtkWidget *wpanel;
		/*pop up, if popped down*/
		wpanel = gtk_object_get_data(GTK_OBJECT(panel),
					     PANEL_PARENT);
	} 

	panel_widget_add_forbidden(PANEL_WIDGET(DRAWER_WIDGET(drawer->drawer)->panel));

	gtk_tooltips_set_tip (panel_tooltips,drawer->button,
			      drawer->tooltip,NULL);
	if(GTK_WIDGET_REALIZED(drawer->button)) {
		gtk_widget_queue_resize(drawer->drawer);
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

	g_return_if_fail(applets_last!=NULL);

	applet_add_callback(applets_last->data,"properties",
			    GNOME_STOCK_MENU_PROP,
			    _("Properties..."));
}

