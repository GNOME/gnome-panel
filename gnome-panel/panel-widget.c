#include <gtk/gtk.h>


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

GtkWidget*
panel_widget_new (gint size, PanelOrientation orient)
{
	PanelWidget *panel;
	gint i;

	panel = gtk_type_new(panel_widget_get_type());
	if(orient == PANEL_HORIZONTAL)
		panel->table = gtk_table_new(1,size,TRUE);
	else
		panel->table = gtk_table_new(size,1,TRUE);

	panel->size = size;
	panel->orientation = orient;

	panel->applets = g_new(AppletRecord *,size);
	for(i=0;i<size;i++) {
		panel->applets[i] = g_new(AppletRecord,1);
		panel->applets[i]->widget = gtk_event_box();
		panel->applets[i]->is_applet = FALSE;
		panel_widget_place_applet(panel,panel->applets[i],i);
		gtk_widget_show(panel->applets[i]->widget);
	}

	return GTK_WIDGET(panel);
}

gint
panel_widget_add (PanelWidget *panel, GtkWidget *applet, gint pos)
{
	gint i;

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

	if(panel->applets[i]->widget) {
		/*remove placeholder*/
		gtk_container_remove(GTK_CONTAINER(panel->table),
				     panel->applets[i]->widget);
		gtk_widget_destroy(panel->applets[i]->widget);
	}

	panel->applets[i]->widget = applet;
	panel->applets[i]->is_applet = TRUE;
	panel_widget_place_applet(panel,panel->applets[i],i);

	return i;
}

gint
panel_widget_add (PanelWidget *panel, GtkWidget *applet, gint pos)
{
	gint i;

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

	if(panel->applets[i]->widget) {
		/*remove placeholder*/
		gtk_container_remove(GTK_CONTAINER(panel->table),
				     panel->applets[i]->widget);
		gtk_widget_destroy(panel->applets[i]->widget);
	}

	panel->applets[i]->widget = applet;
	panel->applets[i]->is_applet = TRUE;
	panel_widget_place_applet(panel,panel->applets[i],i);

	return i;
}

gint
panel_widget_remove (PanelWidget *panel, GtkWidget *applet)
{
	gint i;

	if(!panel || !applet)
		return -1;

	for(i=0;i<panel->size;i++)
		if(panel->applets[i]->widget == applet)
			break;

	/*applet not found*/
	if(i==panel->size)
		return -1;

	/*remove applet*/
	gtk_container_remove(GTK_CONTAINER(panel->table),
			     panel->applets[i]->widget);
	gtk_widget_destroy(panel->applets[i]->widget);

	/*put placeholder in place*/
	panel->applets[i]->widget = gtk_event_box();
	panel->applets[i]->is_applet = FALSE;
	panel_widget_place_applet(panel,panel->applets[i],i);
	gtk_widget_show(panel->applets[i]->widget);

	return i;
}
