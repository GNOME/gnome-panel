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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <gnome.h>

#include "panel-include.h"

#define DRAWER_PROPERTIES "drawer_properties"

extern GSList *applets;
extern GSList *applets_last;
extern int applet_count;
extern GlobalConfig global_config;

extern GtkTooltips *panel_tooltips;

extern GSList *panel_list;

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
	s = gnome_icon_entry_get_filename(GNOME_ICON_ENTRY(pixentry));
	if(!s || !*s) {
		drawer->pixmap = gnome_pixmap_file ("panel-drawer.png");
		button_widget_set_pixmap_from_file (BUTTON_WIDGET(drawer->button),
						    drawer->pixmap,-1);
	} else {
		if(button_widget_set_pixmap_from_file(BUTTON_WIDGET(drawer->button), s, -1))
			drawer->pixmap = g_strdup(s);
		else {
			drawer->pixmap = gnome_pixmap_file ("panel-drawer.png");
			button_widget_set_pixmap_from_file (BUTTON_WIDGET(drawer->button),
							    drawer->pixmap,-1);
		}
	}
	g_free(s);
	s = gtk_entry_get_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(tipentry))));
	if(!s || !*s)
		drawer->tooltip = NULL;
	else
		drawer->tooltip = g_strdup(s);

	gtk_tooltips_set_tip (panel_tooltips,drawer->button,
			      drawer->tooltip,NULL);
}

static void
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = data;
	gtk_object_set_data(GTK_OBJECT(drawer->button),
			    DRAWER_PROPERTIES,NULL);
}

static void
set_toggle (GtkWidget *widget, gpointer data)
{
	PerPanelConfig *ppc = gtk_object_get_user_data(GTK_OBJECT(widget));
	int *the_toggle = data;

	*the_toggle = GTK_TOGGLE_BUTTON(widget)->active;
	if (ppc->register_changes)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (ppc->config_window));
}

static void
set_sensitive_toggle (GtkWidget *widget, GtkWidget *widget2)
{
	gtk_widget_set_sensitive(widget2,GTK_TOGGLE_BUTTON(widget)->active);
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
	gtk_container_set_border_width(GTK_CONTAINER(table), GNOME_PAD_SMALL);

	w = create_text_entry(table, "drawer_name", 0, _("Tooltip/Name"),
			      drawer->tooltip, dialog);
	gtk_object_set_data(GTK_OBJECT(dialog),"tooltip",w);
	
	w = create_icon_entry(table, "icon", 1, _("Icon"), drawer->pixmap,
			      dialog);
	gtk_object_set_data(GTK_OBJECT(dialog),"pixmap",w);

	f = gtk_frame_new(_("Applet appearance"));
	gtk_container_add(GTK_CONTAINER(f),table);

	box = gtk_vbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(box), GNOME_PAD_SMALL);
	gtk_box_pack_start(GTK_BOX(box),f,FALSE,FALSE,0);

	f = gtk_frame_new(_("Drawer handle"));
	box_in = gtk_vbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(box_in), GNOME_PAD_SMALL);
	/*we store this in w for later use!, so don't use w as temp from now
	  on*/
	w = button = gtk_check_button_new_with_label (_("Enable hidebutton"));
	gtk_object_set_user_data(GTK_OBJECT(button),ppc);
	if (ppc->hidebuttons)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle),
			    &ppc->hidebuttons);
	gtk_box_pack_start (GTK_BOX (box_in), button, TRUE, FALSE, 0);

	button = gtk_check_button_new_with_label (_("Enable hidebutton arrow"));
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (set_sensitive_toggle),
			    button);
	if (!ppc->hidebuttons)
		gtk_widget_set_sensitive(button,FALSE);
	gtk_object_set_user_data(GTK_OBJECT(button),ppc);
	if (ppc->hidebutton_pixmaps)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle),
			    &ppc->hidebutton_pixmaps);
	gtk_box_pack_start (GTK_BOX (box_in), button, TRUE, TRUE, 0);
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
	
	switch (BASEP_WIDGET (drawerw)->state) {
	case BASEP_SHOWN:
	case BASEP_AUTO_HIDDEN:
		drawer_widget_close_drawer (drawerw, BASEP_WIDGET (panelw));
		break;
	case BASEP_HIDDEN_LEFT:
	case BASEP_HIDDEN_RIGHT:
		drawer_widget_open_drawer (drawerw, BASEP_WIDGET (panelw));
		break;
	case BASEP_MOVING:
		g_assert_not_reached ();
		break;
	}
}

static void
destroy_drawer(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = data;
	GtkWidget *prop_dialog = gtk_object_get_data(GTK_OBJECT(drawer->button),
						     DRAWER_PROPERTIES);
	if(prop_dialog)
		gtk_widget_destroy(prop_dialog);
	g_free(drawer);
}

static int
enter_notify_drawer(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	Drawer *drawer = data;
	BasePWidget *basep = BASEP_WIDGET (drawer->drawer);

	if (!gnome_win_hints_wm_exists())
		gdk_window_raise(drawer->drawer->window);

	if (basep->state == BASEP_MOVING)
		return FALSE;
	
	if ((basep->state != BASEP_AUTO_HIDDEN) ||
	    (event->detail == GDK_NOTIFY_INFERIOR) ||
	    (basep->mode != BASEP_AUTO_HIDE))
		return FALSE;

	if (basep->leave_notify_timer_tag != 0) {
		gtk_timeout_remove (basep->leave_notify_timer_tag);
		basep->leave_notify_timer_tag = 0;
	}

	basep_widget_autoshow (basep);

	return FALSE;
}

static int
leave_notify_drawer (GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	Drawer *drawer = data;
	BasePWidget *basep = BASEP_WIDGET (drawer->drawer);

	if (event->detail == GDK_NOTIFY_INFERIOR)
		return FALSE;

	basep_widget_queue_autohide (basep);

	return FALSE;
	
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
		drawer->pixmap = gnome_pixmap_file ("panel-drawer.png");
	} else {
		drawer->pixmap = g_strdup(pixmap);
	}
	drawer->button = button_widget_new_from_file (drawer->pixmap, -1,
						      DRAWER_TILE,
						      TRUE,orient,
						      _("Drawer"));
		gtk_widget_show(drawer->button);

	drawer->drawer = drawer_panel;

	gtk_signal_connect (GTK_OBJECT (drawer->button), "clicked",
			    GTK_SIGNAL_FUNC (drawer_click), drawer);
	gtk_signal_connect (GTK_OBJECT (drawer->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_drawer), drawer);
	gtk_signal_connect (GTK_OBJECT (drawer->button), "enter_notify_event",
			    GTK_SIGNAL_FUNC (enter_notify_drawer), drawer);
	gtk_signal_connect (GTK_OBJECT (drawer->button), "leave_notify_event",
			    GTK_SIGNAL_FUNC (leave_notify_drawer), drawer);
	gtk_object_set_user_data(GTK_OBJECT(drawer->button),drawer);
	gtk_object_set_data(GTK_OBJECT(drawer_panel),DRAWER_PANEL_KEY,drawer);
	gtk_widget_queue_resize(GTK_WIDGET(drawer_panel));

	return drawer;
}

static Drawer *
create_empty_drawer_applet(char *tooltip, char *pixmap,
			   PanelOrientType orient)
{
	GtkWidget *dw = drawer_widget_new(orient,
					  BASEP_EXPLICIT_HIDE,
					  BASEP_SHOWN,
					  SIZE_STANDARD,
					  TRUE, TRUE,
					  PANEL_BACK_NONE, NULL,
					  TRUE, NULL);
	return create_drawer_applet(dw, tooltip,pixmap,orient);
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
drawer_setup(Drawer *drawer)
{
	gtk_widget_queue_resize(drawer->drawer);
	if(BASEP_WIDGET(drawer->drawer)->state != BASEP_SHOWN) {
		GtkRequisition chreq;
		gtk_widget_size_request(drawer->drawer, &chreq);
		gtk_widget_set_uposition(drawer->drawer,
					 -chreq.width - 1, -chreq.height - 1);
	}
	gtk_widget_show(drawer->drawer);
}

static void
drawer_move_foreach(GtkWidget *w, gpointer user_data)
{
	AppletInfo *info = gtk_object_get_data(GTK_OBJECT(w), "applet_info");
	
	if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		gtk_widget_queue_resize(drawer->drawer);
	}
}

static void
button_size_alloc(GtkWidget *widget, GtkAllocation *alloc, Drawer *drawer)
{
	if(!GTK_WIDGET_REALIZED(widget))
		return;
	drawer_move_foreach(widget,NULL);
	gtk_object_set_data(GTK_OBJECT(widget),"allocated",GINT_TO_POINTER(1));
}

void
load_drawer_applet(int mypanel, char *pixmap, char *tooltip,
		   PanelWidget *panel, int pos)
{
	Drawer *drawer;
	PanelOrientType orient = get_applet_orient(panel);

	if(mypanel < 0) {
		drawer = create_empty_drawer_applet(tooltip,pixmap,orient);
		if(drawer) panel_setup(drawer->drawer);
	} else {
		PanelData *dr_pd;
		GSList *li;

		li = g_slist_nth(panel_list,mypanel);
		g_return_if_fail(li != NULL);
		dr_pd = li->data;

		drawer=create_drawer_applet(dr_pd->panel, tooltip,
					    pixmap, orient);

		drawer_widget_change_orient(DRAWER_WIDGET(dr_pd->panel),
					    orient);
	}

	if(!drawer)
		return;

	register_toy(drawer->button,drawer, panel, pos, APPLET_DRAWER);

	gtk_signal_connect_after(GTK_OBJECT(drawer->button),
				 "size_allocate",
				 GTK_SIGNAL_FUNC(button_size_alloc),
				 drawer);

	if(BASEP_WIDGET(drawer->drawer)->state == BASEP_SHOWN) {
		GtkWidget *wpanel;
		/*pop up, if popped down*/
		wpanel = gtk_object_get_data(GTK_OBJECT(panel),
					     PANEL_PARENT);
	} 

	panel_widget_add_forbidden(PANEL_WIDGET(BASEP_WIDGET(drawer->drawer)->panel));

	gtk_tooltips_set_tip (panel_tooltips,drawer->button,
			      drawer->tooltip,NULL);
	drawer_setup(drawer);

	g_return_if_fail(applets_last!=NULL);

	applet_add_callback(applets_last->data,"properties",
			    GNOME_STOCK_MENU_PROP,
			    _("Properties..."));
}

