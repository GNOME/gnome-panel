#include <gtk/gtk.h>
#include "panel-widget.h"

static void panel_widget_class_init	(PanelWidgetClass *klass);
static void panel_widget_init		(PanelWidget      *panel_widget);

guint
panel_widget_get_type ()
{
	static guint panel_widget_type = 0;

	if (!panel_widget_type) {
		GtkTypeInfo panel_widget_info = {
			"PanelWidget",
			sizeof (PanelWidget),
			sizeof (PanelWidgetClass),
			(GtkClassInitFunc) panel_widget_class_init,
			(GtkObjectInitFunc) panel_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};

		panel_widget_type = gtk_type_unique (gtk_window_get_type (), &panel_widget_info);
	}

	return panel_widget_type;
}

static void
panel_widget_class_init (PanelWidgetClass *class)
{
	GtkWidgetClass *widget_class;

	widget_class = (GtkWidgetClass*) class;
}

static void
panel_widget_init (PanelWidget *panel_widget)
{
	panel_widget->applet_count = 0;
	panel_widget->orient= PANEL_HORIZONTAL;
	panel_widget->snapped = PANEL_BOTTOM;
	panel_widget->mode = PANEL_EXPLICIT_HIDE;
	panel_widget->state = PANEL_SHOWN;
	panel_widget->size = 0;
}

void
panel_widget_applet_realized (GtkWidget *widget,gpointer data)
{
}

static void
panel_widget_set_hidebuttons(PanelWidget *panel)
{
	if(panel->snapped == PANEL_FREE)
		return;
	if(panel->orient == PANEL_HORIZONTAL) {
		gtk_widget_hide(panel->hidebutton_n);
		gtk_widget_show(panel->hidebutton_e);
		gtk_widget_show(panel->hidebutton_w);
		gtk_widget_hide(panel->hidebutton_s);
	} else {
		gtk_widget_show(panel->hidebutton_n);
		gtk_widget_hide(panel->hidebutton_e);
		gtk_widget_hide(panel->hidebutton_w);
		gtk_widget_show(panel->hidebutton_s);
	}
}

GtkWidget*
panel_widget_new (gint length,
		  PanelOrientation orient,
		  PanelSnapped snapped,
		  PanelMode mode,
		  PanelState state)
{
	PanelWidget *panel;
	gint i;
	gchar *pixmap_name;
	GtkWidget *pixmap;

	g_return_val_if_fail(size>=0,NULL);

	panel = gtk_type_new(panel_widget_get_type());
	GTK_WINDOW(panel)->type = WINDOW_POPUP;

	switch(snapped) {
		case PANEL_FREE:
			if(orient == PANEL_HORIZONTAL)
				gtk_widget_set_usize(GTK_WIDGET(panel->window),
						     length, PANEL_CELL_SIZE);
			else
				gtk_widget_set_usize(GTK_WIDGET(panel->window),
						     PANEL_CELL_SIZE, length);
			break;
		case PANEL_TOP:
		case PANEL_BOTTOM:
			orient == PANEL_HORIZONTAL;
			gtk_widget_set_usize(GTK_WIDGET(panel->window),
					     gdk_screen_width(),
					     PANEL_CELL_SIZE);
			break;
		case PANEL_LEFT:
		case PANEL_RIGHT:
			orient == PANEL_VERTICAL;
			gtk_widget_set_usize(GTK_WIDGET(panel->window),
					     PANEL_CELL_SIZE,
					     gdk_screen_height());
			break;
	}

	switch(snapped) {
		case PANEL_TOP:
			gtk_widget_set_uposition(GTK_WIDGET(panel),
					         0,
					         0);
			break;
		case PANEL_BOTTOM:
			gtk_widget_set_uposition(GTK_WIDGET(panel),
					         0,
					         gdk_screen_height() -
					         PANEL_CELL_SIZE);
			break;
		case PANEL_LEFT:
			gtk_widget_set_uposition(GTK_WIDGET(panel),
					         0,
					         0);
			break;
		case PANEL_RIGHT:
			gtk_widget_set_uposition(GTK_WIDGET(panel),
					         gdk_screen_width() -
					         PANEL_CELL_SIZE,
					         0);
			break;
	}

	panel->table = gtk_table_new(3,3,FALSE);
	gtk_container_add(GTK_CONTAINER(panel),panel->table);
	gtk_widget_show(panel->table);

	panel->fixed = gtk_fixed_new();
	gtk_table_attach(GTK_TABLE(panel->table),panel->fixed,1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);
	gtk_widget_show(panel->fixed);

	/*EAST*/
	panel->hidebutton_e=gtk_button_new();
	pixmap_name=gnome_unconditional_pixmap_file("panel-arrow-left.xpm");
	pixmap = gnome_create_pixmap_widget(GTK_WIDGET(panel),
					    panel->hidebutton_e,
					    pixmap_name);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(panel->hidebutton_e),pixmap);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(panel->hidebutton_e), "clicked",
			   GTK_SIGNAL_FUNC(panel_widget_hide_east),NULL);
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_e,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	/*NORTH*/
	panel->hidebutton_n=gtk_button_new();
	pixmap_name=gnome_unconditional_pixmap_file("panel-arrow-up.xpm");
	pixmap = gnome_create_pixmap_widget(GTK_WIDGET(panel),
					    panel->hidebutton_n,
					    pixmap_name);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(panel->hidebutton_n),pixmap);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(panel->hidebutton_n), "clicked",
			   GTK_SIGNAL_FUNC(panel_widget_hide_north),NULL);
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_n,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);
	/*WEST*/
	panel->hidebutton_w=gtk_button_new();
	pixmap_name=gnome_unconditional_pixmap_file("panel-arrow-right.xpm");
	pixmap = gnome_create_pixmap_widget(GTK_WIDGET(panel),
					    panel->hidebutton_w,
					    pixmap_name);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(panel->hidebutton_w),pixmap);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(panel->hidebutton_w), "clicked",
			   GTK_SIGNAL_FUNC(panel_widget_hide_west),NULL);
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_w,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	/*SOUTH*/
	panel->hidebutton_s=gtk_button_new();
	pixmap_name=gnome_unconditional_pixmap_file("panel-arrow-down.xpm");
	pixmap = gnome_create_pixmap_widget(GTK_WIDGET(panel),
					    panel->hidebutton_s,
					    pixmap_name);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(panel->hidebutton_s),pixmap);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(panel->hidebutton_s), "clicked",
			   GTK_SIGNAL_FUNC(panel_widget_hide_south),NULL);
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_s,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);

	panel->orient = orient;
	panel->snapped = snapped;
	panel->mode = mode;
	panel->state = PANEL_SHOWN; /*state;*/
				/*FIXME make it appear at the right
				state as well*/

	panel_widget_set_hidebuttons(panel)

	for(i=0;i<PANEL_MAX;i++) {
		panel->applets[i]->applet = NULL;
		panel->applets[i]->drawer = NULL;
	}

	return GTK_WIDGET(panel);
}


gint
panel_widget_add (PanelWidget *panel, GtkWidget *applet, gint pos)
{
	gint i;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(applet,-1);
	g_return_val_if_fail(pos>=0,-1);
	g_return_val_if_fail(pos<panel->size,-1);

	for(i=pos;i<panel->size;i++)
		if(!panel->applets[i]->applet)
			break;

	/*panel is full to the right*/
	if(i==panel->size) {
		for(i=pos-1;i>=0;i--)
			if(!panel->applets[i]->applet)
				break;
		/*panel is full!*/
		if(i<=0)
			return -1;
	}

	gtk_container_add(GTK_CONTAINER(panel->applets[i]->container),applet);
	panel->applets[i]->applet = applet;

	if(GTK_WIDGET_REALIZED(applet))
		panel_widget_applet_realized(applet,NULL);
	else
		gtk_signal_connect_after(GTK_OBJECT(applet),
					 "realize",
					 (GtkSignalFunc)
					 panel_widget_applet_realized,
					 NULL);

	return i;
}

gint
panel_widget_remove (PanelWidget *panel, GtkWidget *applet)
{
	gint i;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(applet,-1);

	for(i=0;i<panel->size;i++)
		if(panel->applets[i]->applet == applet)
			break;

	/*applet not found*/
	if(i==panel->size)
		return -1;

	/*remove applet*/
	gtk_container_remove(GTK_CONTAINER(panel->applets[i]->container),				     applet);

	return i;
}

gint
panel_widget_get_pos(PanelWidget *panel, GtkWidget *applet)
{
	gint i;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(applet,-1);

	for(i=0;i<panel->size;i++)
		if(panel->applets[i]->applet == applet)
			break;

	/*applet not found*/
	if(i==panel->size)
		return -1;

	return i;
}

GList*
panel_widget_get_applets(PanelWidget *panel)
{
	GList *list=NULL;
	gint i;

	g_return_val_if_fail(panel,NULL);

	for(i=0;i<panel->size;i++)
		if(panel->applets[i]->applet != NULL)
			list = g_list_prepend(list,panel->applets[i]->applet);

	return list;
}

void
panel_widget_foreach(PanelWidget *panel, GFunc func, gpointer user_data)
{
	gint i;

	g_return_if_fail(panel);
	g_return_if_fail(func);

	for(i=0;i<panel->size;i++)
		if(panel->applets[i]->applet != NULL)
			(*func)(panel->applets[i]->applet,user_data);
}
