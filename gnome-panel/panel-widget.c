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

		panel_widget_type = gtk_type_unique (gtk_fixed_get_type (), &panel_widget_info);
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
	switch(snapped) {
		case PANEL_TOP:
			gtk_widget_set_uposition(GTK_WIDGET(panel->window),
					         0,
					         0);
			break;
		case PANEL_BOTTOM:
			gtk_widget_set_uposition(GTK_WIDGET(panel->window),
					         0,
					         gdk_screen_height() -
					         PANEL_CELL_SIZE);
			break;
		case PANEL_LEFT:
			gtk_widget_set_uposition(GTK_WIDGET(panel->window),
					         0,
					         0);
			break;
		case PANEL_RIGHT:
			gtk_widget_set_uposition(GTK_WIDGET(panel->window),
					         gdk_screen_width() -
					         PANEL_CELL_SIZE,
					         0);
			break;
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

	g_return_val_if_fail(size>=0,NULL);

	panel = gtk_type_new(panel_widget_get_type());
	panel->window = gtk_window_new(WINDOW_POPUP);
	gtk_widget_show(panel->window);

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
			gtk_widget_set_uposition(GTK_WIDGET(panel->window),
					         0,
					         0);
			break;
		case PANEL_BOTTOM:
			gtk_widget_set_uposition(GTK_WIDGET(panel->window),
					         0,
					         gdk_screen_height() -
					         PANEL_CELL_SIZE);
			break;
		case PANEL_LEFT:
			gtk_widget_set_uposition(GTK_WIDGET(panel->window),
					         0,
					         0);
			break;
		case PANEL_RIGHT:
			gtk_widget_set_uposition(GTK_WIDGET(panel->window),
					         gdk_screen_width() -
					         PANEL_CELL_SIZE,
					         0);
			break;
	}

	
	panel->table = gtk_table_new(3,3,FALSE);
	gtk_container_add(GTK_CONTAINER(panel->window),panel->table);
	gtk_widget_show(panel->table);





	panel_widget->orient = orient;
	panel_widget->snapped = snapped;
	panel_widget->mode = mode;
	panel_widget->state = PANEL_SHOWN; /*state;*/
				/*FIXME make it appear at the right
				state as well*/


	panel->size = size;
	panel->orientation = orient;

	panel->applets = g_new(AppletRecord *,size);
	for(i=0;i<size;i++) {

		panel->applets[i] = g_new(AppletRecord,1);
		panel->applets[i]->eventbox = gtk_event_box_new();
		panel->applets[i]->container = gtk_frame_new(NULL);
		panel->applets[i]->applet = NULL;
		panel_widget_place_widget(panel,panel->applets[i]->eventbox,i);

		gtk_signal_connect_after(GTK_OBJECT(panel->applets[i]->eventbox),
					 "realize",
					 (GtkSignalFunc)
					 panel_widget_placeholder_realized,
					 panel);
		gtk_container_add(GTK_CONTAINER(panel->applets[i]->eventbox),
				  panel->applets[i]->container);
		gtk_widget_show(panel->applets[i]->container);
		gtk_widget_show(panel->applets[i]->eventbox);
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
