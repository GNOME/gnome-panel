#include <gtk/gtk.h>
#include "panel-widget.h"

char *possible_drag_types[] = {"application/panel-applet-pointer"};
char *accepted_drop_types[] = {"application/panel-applet-pointer"};

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

		panel_widget_type = gtk_type_unique (gtk_event_box_get_type (), &panel_widget_info);
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
	panel_widget->table = NULL;
	panel_widget->applets = NULL;
	panel_widget->applet_count = 0;
	panel_widget->orientation = PANEL_HORIZONTAL;
	panel_widget->size = 0;
}

static void
panel_widget_place_applet(PanelWidget *panel, AppletRecord *applet, int pos)
{
	if(!panel || !applet || !applet->widget)
		return;

	if(panel->orientation == PANEL_HORIZONTAL)
		gtk_table_attach(GTK_TABLE(panel->table),applet->widget,
				 pos,pos+1,0,1,
				 GTK_SHRINK|(!applet->is_applet?
				  GTK_EXPAND|GTK_FILL:0),
				 GTK_SHRINK|(!applet->is_applet?
				  GTK_EXPAND|GTK_FILL:0),
				 0,0);
	else
		gtk_table_attach(GTK_TABLE(panel->table),applet->widget,
				 0,1,pos,pos+1,
				 GTK_SHRINK|(!applet->is_applet?
				  GTK_EXPAND|GTK_FILL:0),
				 GTK_SHRINK|(!applet->is_applet?
				  GTK_EXPAND|GTK_FILL:0),
				 0,0);
}

static void
panel_widget_dnd_drop (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	GtkWidget *dragfrom;
	PanelWidget *panel;
	gint from,to;

	dragfrom = *(GtkWidget **)event->dropdataavailable.data;
	panel = (PanelWidget *)user_data;


	from = panel_widget_get_pos(panel,dragfrom);
	for(to=0;to<panel->size;to++)
		if(panel->applets[to]->widget == widget)
			break;

	panel->applets[from]->is_applet = FALSE;
	panel->applets[to]->is_applet = TRUE;

	gtk_widget_reparent(dragfrom,widget);
}

static void
panel_widget_dnd_drag_request (GtkWidget *widget, GdkEvent *event,
	gpointer data)
{
	gtk_widget_dnd_data_set(GTK_WIDGET(data), event, &data,
				sizeof(GtkWidget *));
}

void
panel_widget_applet_realized (GtkWidget *widget,gpointer data)
{
	gtk_signal_connect(GTK_OBJECT (widget),
			   "drag_request_event",
			   GTK_SIGNAL_FUNC(panel_widget_dnd_drag_request),
			   data);
	gtk_widget_dnd_drag_set(widget, TRUE, possible_drag_types, 1);
}

void
panel_widget_placeholder_realized (GtkWidget *widget,gpointer data)
{
	gtk_signal_connect(GTK_OBJECT (widget),
			   "drop_data_available_event",
			   GTK_SIGNAL_FUNC(panel_widget_dnd_drop),
			   data);
	gtk_widget_dnd_drop_set(widget, TRUE, accepted_drop_types, 1, FALSE);
}

GtkWidget*
panel_widget_new (gint size, PanelOrientation orient)
{
	PanelWidget *panel;
	gint i;

	g_return_val_if_fail(size>0,NULL);
	g_return_val_if_fail(
		(orient==PANEL_HORIZONTAL || orient==PANEL_VERTICAL),
		NULL);

	panel = gtk_type_new(panel_widget_get_type());

	if(orient == PANEL_HORIZONTAL)
		panel->table = gtk_table_new(1,size,TRUE);
	else
		panel->table = gtk_table_new(size,1,TRUE);


	gtk_container_add(GTK_CONTAINER(panel),panel->table);
	gtk_widget_show(panel->table);

	panel->size = size;
	panel->orientation = orient;

	panel->applets = g_new(AppletRecord *,size);
	for(i=0;i<size;i++) {
		panel->applets[i] = g_new(AppletRecord,1);
		panel->applets[i]->widget = gtk_event_box_new();
		panel->applets[i]->is_applet = FALSE;
		panel_widget_place_applet(panel,panel->applets[i],i);

		gtk_signal_connect_after(GTK_OBJECT(panel->applets[i]->widget),
					 "realize",
					 (GtkSignalFunc)
					 panel_widget_placeholder_realized,
					 panel);
		gtk_widget_show(panel->applets[i]->widget);
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
		if(!panel->applets[i]->is_applet)
			break;

	/*panel is full to the right*/
	if(i==panel->size) {
		for(i=pos-1;i>=0;i--)
			if(!panel->applets[i]->is_applet)
				break;
		/*panel is full!*/
		if(i<=0)
			return -1;
	}

	gtk_container_add(GTK_CONTAINER(panel->applets[i]->widget),applet);
	panel->applets[i]->is_applet = TRUE;

	if(GTK_WIDGET_REALIZED(applet))
		panel_widget_applet_realized(applet,panel->applets[i]->widget);
	else
		gtk_signal_connect_after(GTK_OBJECT(applet),
					 "realize",
					 (GtkSignalFunc)
					 panel_widget_applet_realized,
					 panel->applets[i]->widget);

	return i;
}

gint
panel_widget_remove (PanelWidget *panel, GtkWidget *applet)
{
	gint i;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(applet,-1);

	for(i=0;i<panel->size;i++)
		if(GTK_BIN(panel->applets[i]->widget)->child == applet)
			break;

	/*applet not found*/
	if(i==panel->size)
		return -1;

	/*remove applet*/
	gtk_container_remove(GTK_CONTAINER(panel->applets[i]->widget),applet);

	return i;
}

gint
panel_widget_get_pos(PanelWidget *panel, GtkWidget *applet)
{
	gint i;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(applet,-1);

	for(i=0;i<panel->size;i++)
		if(GTK_BIN(panel->applets[i]->widget)->child == applet)
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
		if(panel->applets[i]->widget != NULL)
			list = g_list_prepend(list,panel->applets[i]->widget);

	return list;
}

void
panel_widget_foreach(PanelWidget *panel, GFunc func, gpointer user_data)
{
	gint i;

	g_return_if_fail(panel);
	g_return_if_fail(func);

	for(i=0;i<panel->size;i++)
		if(panel->applets[i]->widget != NULL)
			(*func)(panel->applets[i]->widget,user_data);
}
