#include <gtk/gtk.h>
#include <gnome.h>
#include "panel-widget.h"
#include "gdkextra.h"

static void panel_widget_class_init	(PanelWidgetClass *klass);
static void panel_widget_init		(PanelWidget      *panel_widget);

static GdkCursor *fleur_cursor;

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

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
	panel_widget->leave_notify_timer_tag = 0;
	panel_widget->currently_dragged_applet = NULL;
}

static void
panel_widget_shrink_wrap(PanelWidget *panel,
			 gint width,
			 gint pos)
{
	gint i;

	if(width%PANEL_CELL_SIZE) width--; /*just so that I get
					     the right size*/
	/*convert width from pixels to cells*/
	width = (width/PANEL_CELL_SIZE) + 1;

	if(width == panel->applets[pos].cells)
		return;

	for(i=pos+width;i<(pos+panel->applets[pos].cells-1);i++) {
		panel->applets[i].applet=NULL;
		panel->applets[i].drawer=NULL;
		panel->applets[i].cells=1;
	}
}

static gint
panel_widget_seize_space(PanelWidget *panel,
			 gint width,
			 gint pos)
{
	gint allocated=1;
	gint i;
	GtkWidget *drawer;
	GtkWidget *applet;

	drawer = panel->applets[pos].drawer;
	applet = panel->applets[pos].applet;

	if(width%PANEL_CELL_SIZE) width--; /*just so that I get
					     the right size*/
	/*convert width from pixels to cells*/
	width = (width/PANEL_CELL_SIZE) + 1;

	for(i=1;(pos+i < panel->size) &&
		(allocated < width) &&
		(panel->applets[pos+i].applet == applet ||
		 panel->applets[pos+i].applet == NULL);i++)
		allocated++;
	for(i=1;(pos-i >= 0) &&
		(allocated < width) &&
		(panel->applets[pos-i].applet == applet ||
		 panel->applets[pos-i].applet == NULL);i++)
		allocated++;
	pos = pos-i+1;

	if(allocated < width) {
		/*FIXME: push apart other applets if neccessary!*/
	}

	for(i=0;i<allocated;i++) {
		panel->applets[pos+i].applet = applet;
		panel->applets[pos+i].applet = drawer;
		panel->applets[pos+i].cells = allocated;
	}
	return pos;
}

static gint
panel_widget_switch_applet_right(PanelWidget *panel, gint pos)
{
	gint i;
	gint rightn;
	AppletRecord tmp;

	tmp.applet = panel->applets[pos].applet;
	tmp.drawer = panel->applets[pos].drawer;
	tmp.cells = panel->applets[pos].cells;

	rightn = pos + panel->applets[pos].cells;

	for(i=0;i<panel->applets[rightn].cells;i++) {
		panel->applets[pos+i].applet = panel->applets[rightn+i].applet;
		panel->applets[pos+i].drawer = panel->applets[rightn+i].drawer;
		panel->applets[pos+i].cells = panel->applets[rightn+i].cells;
	}

	pos = pos + i;

	for(i=0;i<tmp.cells;i++) {
		panel->applets[pos+i].applet = tmp.applet;
		panel->applets[pos+i].drawer = tmp.drawer;
		panel->applets[pos+i].cells = tmp.cells;
	}

	return pos;
}

static gint
panel_widget_switch_applet_left(PanelWidget *panel, gint pos)
{
	pos -= panel->applets[pos-1].cells;
	panel_widget_switch_applet_right(panel,pos);
	return pos;
}

static gint
panel_widget_switch_move(PanelWidget *panel, gint pos, gint moveby)
{
	gint width;
	gint finalpos;

	g_return_val_if_fail(pos>=0,-1);
	g_return_val_if_fail(panel,-1);

	width = panel->applets[pos].cells;
	finalpos = pos+moveby;

	if(finalpos > panel->size)
		finalpos = panel->size-1;
	else if(finalpos < 0)
		finalpos = 0;

	for(;;) {
		if((pos+width-1)<finalpos)
			pos = panel_widget_switch_applet_right(panel,pos);
		else if(pos>finalpos)
			pos = panel_widget_switch_applet_left(panel,pos);
		else
			break;
	}

	return pos;
}


static void
panel_widget_adjust_applet(PanelWidget *panel, GtkWidget *applet)
{
	gint width, height;
	gint x,y;
	gint panel_thick;
	gint pos;

	width = applet->allocation.width;
	height = applet->allocation.height;
	pos = panel_widget_get_pos(panel,applet);
	if(panel->orient==PANEL_HORIZONTAL) {
		panel_thick = GTK_WIDGET(panel)->allocation.height;
		if(panel_thick<PANEL_CELL_SIZE)
			panel_thick = PANEL_CELL_SIZE;

		if(height > panel_thick) {
			/*FIXME: schedhule window resize, this has to
			  be done after all applets has been added,
			  so probably this would be done by an idle
			  function which then adjusts the vertical
			  position of applets*/
		}

		/*if smaller then it's allocation, we are OK*/
		if(width<=(PANEL_CELL_SIZE*panel->applets[pos].cells)) {
			panel_widget_shrink_wrap(panel,width,pos);
			x = (PANEL_CELL_SIZE*pos) +
			    ((PANEL_CELL_SIZE*panel->applets[pos].cells)/2) -
			    (width/2);
		        y = (panel_thick/2) - (height/2);
		} else {
			pos = panel_widget_seize_space(panel,width,pos);
			x = (PANEL_CELL_SIZE*pos) +
			    ((PANEL_CELL_SIZE*panel->applets[pos].cells)/2) -
			    (width/2);
		        y = (panel_thick/2) - (height/2);
		}
	} else { /* panel->orient==PANEL_VERTICAL */
		panel_thick = GTK_WIDGET(panel)->allocation.width;
		if(panel_thick<PANEL_CELL_SIZE)
			panel_thick = PANEL_CELL_SIZE;

		if(width > panel_thick) {
			/*FIXME: schedhule window resize, this has to
			  be done after all applets has been added,
			  so probably this would be done by an idle
			  function which then adjusts the vertical
			  position of applets*/
		}

		/*if smaller then it's allocation, we are OK*/
		if(height<=(PANEL_CELL_SIZE*panel->applets[pos].cells)) {
			panel_widget_shrink_wrap(panel,width,pos);
		        x = (panel_thick/2) - (width/2);
			y = (PANEL_CELL_SIZE*pos) +
			    ((PANEL_CELL_SIZE*panel->applets[pos].cells)/2) -
			    (height/2);
		} else {
			pos = panel_widget_seize_space(panel,width,pos);
		        x = (panel_thick/2) - (width/2);
			y = (PANEL_CELL_SIZE*pos) +
			    ((PANEL_CELL_SIZE*panel->applets[pos].cells)/2) -
			    (height/2);
		}
	}
	gtk_fixed_move(GTK_FIXED(panel->fixed),applet,x,y);
}

static void
panel_widget_applet_size_allocate (GtkWidget *widget,gpointer data)
{
	panel_widget_adjust_applet((PanelWidget *)data,widget);
}

static void
move_window(GtkWindow *window, int x, int y)
{
	GtkWidget *widget;

	widget = GTK_WIDGET(window);
	
	gdk_window_set_hints(widget->window, x, y, 0, 0, 0, 0, GDK_HINT_POS);
	gdk_window_move(widget->window, x, y);
	gtk_widget_draw(widget, NULL); /* FIXME: this should draw only the newly exposed area! */
}


static void
move_horiz(PanelWidget *panel, int src_x, int dest_x)
{
	int x;

	if (panel->step_size != 0)
		if (src_x < dest_x)
			for (x = src_x; x < dest_x; x += panel->step_size) {
				move_window(GTK_WINDOW(panel), x,
					    GTK_WIDGET(panel)->allocation.y); 
				/* FIXME: do delay */
			}
		else
			for (x = src_x; x > dest_x; x -= panel->step_size) {
				move_window(GTK_WINDOW(panel), x,
					    GTK_WIDGET(panel)->allocation.y); 
				/* FIXME: do delay */
			}
	
	move_window(GTK_WINDOW(panel), dest_x, GTK_WIDGET(panel)->allocation.y);
}


static void
move_vert(PanelWidget *panel, int src_y, int dest_y)
{
	int y;

	if (panel->step_size != 0)
		if (src_y < dest_y)
			for (y = src_y; y < dest_y; y += panel->step_size) {
				move_window(GTK_WINDOW(panel),
					    GTK_WIDGET(panel)->allocation.x, y);
				/* FIXME: do delay */
			}
		else
			for (y = src_y; y > dest_y; y -= panel->step_size) {
				move_window(GTK_WINDOW(panel),
					    GTK_WIDGET(panel)->allocation.x, y);
				/* FIXME: do delay */
			}

	move_window(GTK_WINDOW(panel), GTK_WIDGET(panel)->allocation.x, dest_y);
}



static void
panel_widget_pop_up(PanelWidget *panel)
{
	int width, height;
	int swidth, sheight;

	if ((panel->state == PANEL_MOVING) ||
	    (panel->state == PANEL_SHOWN) ||
	    (panel->snapped == PANEL_FREE))
		return;

	panel->state = PANEL_MOVING;

	width   = GTK_WIDGET(panel)->allocation.width;
	height  = GTK_WIDGET(panel)->allocation.height;
	swidth  = gdk_screen_width();
	sheight = gdk_screen_height();

	switch (panel->snapped) {
		case PANEL_TOP:
		        move_vert(panel, -height + panel->minimized_size, 0);
			break;

		case PANEL_BOTTOM:
			move_vert(panel, sheight - panel->minimized_size, 
				  sheight - height);
			break;

		case PANEL_LEFT:
			move_horiz(panel, -width + panel->minimized_size, 0);
			break;

		case PANEL_RIGHT:
			move_horiz(panel, swidth - panel->minimized_size, 
				   swidth - width);
			break;
		default: break; /*to get rid of a warning*/
	}

	panel->state = PANEL_SHOWN;
}

static gint
panel_widget_pop_down(PanelWidget *panel)
{
	int width, height;
	int swidth, sheight;

	if ((panel->state == PANEL_MOVING) ||
	    (panel->state == PANEL_HIDDEN) ||
	    (panel->snapped == PANEL_FREE))
		return FALSE;

	panel->state = PANEL_MOVING;

	width   = GTK_WIDGET(panel)->allocation.width;
	height  = GTK_WIDGET(panel)->allocation.height;
	swidth  = gdk_screen_width();
	sheight = gdk_screen_height();

	switch (panel->snapped) {
		case PANEL_TOP:
			move_vert(panel, 0, -height + panel->minimized_size);
			break;

		case PANEL_BOTTOM:
			move_vert(panel, sheight - height, 
				  sheight - panel->minimized_size);
			break;

		case PANEL_LEFT:
			move_horiz(panel, 0, -width + panel->minimized_size);
			break;

		case PANEL_RIGHT:
			move_horiz(panel, swidth - width, 
				   swidth - panel->minimized_size);
			break;
		default: break; /*to get rid of a warning*/
	}

	panel->state = PANEL_HIDDEN;

	panel->leave_notify_timer_tag = 0;
  
	return FALSE;
}


static void
panel_widget_pop_show(PanelWidget *panel, int fromright)
{
	int width, height;

	if ((panel->state == PANEL_MOVING) ||
	    (panel->state == PANEL_SHOWN) ||
	    (panel->snapped == PANEL_FREE))
		return;

	panel->state = PANEL_MOVING;

	width   = GTK_WIDGET(panel)->allocation.width;
	height  = GTK_WIDGET(panel)->allocation.height;

	if(panel->orient == PANEL_HORIZONTAL) {
		if(fromright)
			move_horiz(panel, -width +
				   panel->hidebutton_w->allocation.width, 0);
		else
			move_horiz(panel, width -
				   panel->hidebutton_e->allocation.width, 0);
	} else {
		if(fromright)
			move_vert(panel, -height +
				  panel->hidebutton_s->allocation.height, 0);
		else
			move_vert(panel, height -
				  panel->hidebutton_n->allocation.height, 0);
	}

	panel->state = PANEL_SHOWN;
}

static void
panel_widget_pop_hide(PanelWidget *panel, int fromright)
{
	int width, height;

	if ((panel->state == PANEL_MOVING) ||
	    (panel->state == PANEL_HIDDEN) ||
	    (panel->snapped == PANEL_FREE))
		return;

	panel->state = PANEL_MOVING;

	width   = GTK_WIDGET(panel)->allocation.width;
	height  = GTK_WIDGET(panel)->allocation.height;

	if(panel->orient == PANEL_HORIZONTAL) {
		if(fromright)
			move_horiz(panel, 0, -width +
				   panel->hidebutton_w->allocation.width);
		else
			move_horiz(panel, 0, width -
				   panel->hidebutton_e->allocation.width);
	} else {
		if(fromright)
			move_vert(panel, 0, -height +
				  panel->hidebutton_s->allocation.height);
		else
			move_vert(panel, 0, height -
				  panel->hidebutton_n->allocation.height);
	}

	panel->state = PANEL_HIDDEN;
}

static void
panel_show_hide_right(GtkWidget *widget, gpointer data)
{
	PanelWidget *panel = data;
	if(panel->state == PANEL_MOVING) 
		return;
	else if(panel->state == PANEL_HIDDEN)
		panel_widget_pop_show(panel,FALSE);
	else
		panel_widget_pop_hide(panel,TRUE);
}

static void
panel_show_hide_left(GtkWidget *widget, gpointer data)
{
	PanelWidget *panel = data;
	if(panel->state == PANEL_MOVING) 
		return;
	else if(panel->state == PANEL_HIDDEN)
		panel_widget_pop_show(panel,TRUE);
	else
		panel_widget_pop_hide(panel,FALSE);
}

/* FIXME: autohide functions! */

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
		  PanelState state,
		  gint step_size,
		  gint minimized_size,
		  gint minimize_delay,
		  gint tooltips_enabled)
{
	PanelWidget *panel;
	gint i;
	gchar *pixmap_name;
	GtkWidget *pixmap;

	g_return_val_if_fail(length>=0,NULL);

	panel = gtk_type_new(panel_widget_get_type());
	GTK_WINDOW(panel)->type = GTK_WINDOW_POPUP;

	switch(snapped) {
		case PANEL_FREE:
			if(orient == PANEL_HORIZONTAL)
				gtk_widget_set_usize(GTK_WIDGET(panel),
						     length, PANEL_CELL_SIZE);
			else
				gtk_widget_set_usize(GTK_WIDGET(panel),
						     PANEL_CELL_SIZE, length);
			panel->size = length/PANEL_CELL_SIZE;
			break;
		case PANEL_TOP:
		case PANEL_BOTTOM:
			orient = PANEL_HORIZONTAL;
			gtk_widget_set_usize(GTK_WIDGET(panel),
					     gdk_screen_width(),
					     PANEL_CELL_SIZE);
			panel->size = (gdk_screen_width()/PANEL_CELL_SIZE) - 1;
			break;
		case PANEL_LEFT:
		case PANEL_RIGHT:
			orient = PANEL_VERTICAL;
			gtk_widget_set_usize(GTK_WIDGET(panel),
					     PANEL_CELL_SIZE,
					     gdk_screen_height());
			panel->size = (gdk_screen_height()/PANEL_CELL_SIZE) - 1;
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
		default: break; /*to get rid of a warning*/
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
			   GTK_SIGNAL_FUNC(panel_show_hide_right),NULL);
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
			   GTK_SIGNAL_FUNC(panel_show_hide_right),NULL);
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
			   GTK_SIGNAL_FUNC(panel_show_hide_left),NULL);
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_w,
			 2,3,1,2,GTK_FILL,GTK_FILL,0,0);
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
			   GTK_SIGNAL_FUNC(panel_show_hide_left),NULL);
	gtk_table_attach(GTK_TABLE(panel->table),panel->hidebutton_s,
			 1,2,2,3,GTK_FILL,GTK_FILL,0,0);

	panel->orient = orient;
	panel->snapped = snapped;
	panel->mode = mode;
	panel->state = PANEL_SHOWN; /*state;*/
				/*FIXME make it appear at the right
				state as well*/
	panel->step_size = step_size;
	panel->minimized_size = minimized_size;
	panel->minimize_delay = minimize_delay;
	panel->tooltips_enabled = tooltips_enabled;

	panel_widget_set_hidebuttons(panel);

	for(i=0;i<PANEL_MAX;i++) {
		panel->applets[i].applet = NULL;
		panel->applets[i].drawer = NULL;
		panel->applets[i].cells = 1;
	}

	if(!fleur_cursor)
		fleur_cursor = gdk_cursor_new(GDK_FLEUR);

	return GTK_WIDGET(panel);
}

gint
panel_widget_add_with_drawer (PanelWidget *panel, GtkWidget *button, gint pos)
{
	/*FIXME:*/
	return -1;
}

static void
panel_widget_applet_drag_start(PanelWidget *panel, GtkWidget *applet, int warp)
{
	panel->currently_dragged_applet = applet;
	panel->currently_dragged_applet_pos =
		panel_widget_get_pos(panel,applet);

	if (warp)
		gdk_pointer_warp(NULL, applet->window,
				 0, 0, 0, 0,
				 applet->allocation.width / 2,
				 applet->allocation.height / 2);
	
	gtk_grab_add(applet);
	gdk_pointer_grab(applet->window,
			 TRUE,
			 APPLET_EVENT_MASK,
			 NULL,
			 fleur_cursor,
			 GDK_CURRENT_TIME);
}

static void
panel_widget_applet_drag_end(PanelWidget *panel)
{
	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gtk_grab_remove(panel->currently_dragged_applet);
	panel->currently_dragged_applet = NULL;
}

static gint
panel_widget_applet_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	PanelWidget *panel = data;
	GdkEventButton *bevent;


	switch (event->type) {
		case GDK_BUTTON_PRESS:
			bevent = (GdkEventButton *) event;

			if (panel->currently_dragged_applet) {
				panel_widget_applet_drag_end(panel);
				return TRUE;
			}

			switch (bevent->button) {
				case 2: /* Start drag */
					panel_widget_applet_drag_start(
						panel, widget, FALSE);
					return TRUE;

				case 3: /* Applet menu */
					/*FIXME: APPLET MENU*/
					/*show_applet_menu(widget);*/
					return TRUE;
			}

			break;

		case GDK_BUTTON_RELEASE:
			if (panel->currently_dragged_applet) {
				panel_widget_applet_drag_end(panel);
				return TRUE;
			}

			break;

		case GDK_MOTION_NOTIFY:
			if (panel->currently_dragged_applet) {
				gint x,y;
				gint moveby;
				gint pos = panel->currently_dragged_applet_pos;

				gtk_widget_get_pointer(panel->fixed, &x, &y);

				if(panel->orient == PANEL_HORIZONTAL)
					moveby = (x/PANEL_CELL_SIZE)- pos;
				else
					moveby = (y/PANEL_CELL_SIZE)- pos;

				panel_widget_switch_move(panel, pos, moveby);
				return TRUE;
			}
			break;

		default:
			break;
	}

	return FALSE;
}


static GtkWidget *
listening_parent(GtkWidget *widget)
{
	if (GTK_WIDGET_NO_WINDOW(widget))
		return listening_parent(widget->parent);

	return widget;
}

static gint
panel_sub_event_handler(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	switch (event->type) {
		/*pass these to the parent!*/
		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
		case GDK_MOTION_NOTIFY:
			return gtk_widget_event(
				listening_parent(widget->parent), event);

			break;

		default:
			break;
	}

	return FALSE;
}


static void
bind_applet_events(GtkWidget *widget, void *data)
{
	/* XXX: This is more or less a hack.  We need to be able to
	 * capture events over applets so that we can drag them with
	 * the mouse and such.  So we need to force the applet's
	 * widgets to recursively send the events back to their parent
	 * until the event gets to the applet wrapper (the
	 * GtkEventBox) for processing by us.
	 */
	
	if (!GTK_WIDGET_NO_WINDOW(widget)) {
		gtk_widget_set_events(widget, gtk_widget_get_events(widget) |
				      APPLET_EVENT_MASK);
		gtk_signal_connect(GTK_OBJECT(widget), "event",
				   (GtkSignalFunc) panel_sub_event_handler,
				   NULL);
	}
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, 0);
}

static void
bind_child_applet_events(GtkWidget *widget, void *data)
{
	/* XXX: This is more or less a hack.  We need to be able to
	 * capture events over applets so that we can drag them with
	 * the mouse and such.  So we need to force the applet's
	 * widgets to recursively send the events back to their parent
	 * until the event gets to the applet wrapper (the
	 * GtkEventBox) for processing by us.
	 */
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, 0);
}


gint
panel_widget_add (PanelWidget *panel, GtkWidget *applet, gint pos)
{
	gint i;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(applet,-1);
	g_return_val_if_fail(pos>=0,-1);
	if(pos>=panel->size)
		pos = panel->size - 1;

	for(i=0;i<panel->size;i += panel->applets[i].cells)
		if(!panel->applets[i].applet)
			break;

	/*panel is full to the right*/
	if(i==panel->size) {
		for(i=pos-1;i>=0;i -= panel->applets[i].cells)
			if(!panel->applets[i].applet)
				break;
		/*panel is full!*/
		if(i<=0)
			return -1;
	}

	/*this will get done on size allocate!*/
	gtk_fixed_put(GTK_FIXED(panel->fixed),applet,0,0);
	panel->applets[i].applet = applet;
	panel->applets[i].drawer = NULL;
	panel->applets[i].cells = 1;

	gtk_signal_connect(GTK_OBJECT(applet),
			   "size_allocate",
			   GTK_SIGNAL_FUNC(panel_widget_applet_size_allocate),
			   panel);

	gtk_signal_connect(GTK_OBJECT(applet),
			   "event",
			   GTK_SIGNAL_FUNC(panel_widget_applet_event),
			   panel);

	gtk_signal_connect_after(GTK_OBJECT(applet),
				 "realize",
				 GTK_SIGNAL_FUNC(bind_child_applet_events),
				 NULL);

	return i;
}

gint
panel_widget_remove (PanelWidget *panel, GtkWidget *applet)
{
	gint i,n,w;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(applet,-1);

	for(i=0;i<panel->size;i++)
		if(panel->applets[i].applet == applet)
			break;

	/*applet not found*/
	if(i==panel->size)
		return -1;

	/*FIXME: remove a drawer if applet was a drawer!*/
	w = panel->applets[i].cells;
	for(n=0;n<w;n++) {
		panel->applets[i+n].applet = NULL;
		panel->applets[i+n].drawer = NULL;
		panel->applets[i+n].cells = 1;
	}

	/*remove applet*/
	gtk_container_remove(GTK_CONTAINER(panel->fixed),applet);

	return i;
}

gint
panel_widget_get_pos(PanelWidget *panel, GtkWidget *applet)
{
	gint i;

	g_return_val_if_fail(panel,-1);
	g_return_val_if_fail(applet,-1);

	for(i=0;i<panel->size;i++)
		if(panel->applets[i].applet == applet)
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

	/*FIXME: go into drawers*/
	for(i=0;i<panel->size;i += panel->applets[i].cells)
		if(panel->applets[i].applet && !panel->applets[i].drawer)
			list = g_list_prepend(list,panel->applets[i].applet);

	return list;
}

void
panel_widget_foreach(PanelWidget *panel, GFunc func, gpointer user_data)
{
	gint i;

	g_return_if_fail(panel);
	g_return_if_fail(func);

	/*FIXME: go into drawers*/
	for(i=0;i<panel->size;i += panel->applets[i].cells)
		if(panel->applets[i].applet && !panel->applets[i].drawer)
			(*func)(panel->applets[i].applet,user_data);
}
