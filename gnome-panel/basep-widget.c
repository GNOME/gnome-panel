/* Gnome panel: basep widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
#include <math.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include "panel-widget.h"
#include "basep-widget.h"
#include "panel-util.h"
#include "panel_config_global.h"
#include "gdkextra.h"

extern int panel_applet_in_drag;

static void basep_widget_class_init	(BasePWidgetClass *klass);
static void basep_widget_init		(BasePWidget      *basep);
static void basep_widget_size_request	(GtkWidget         *widget,
					 GtkRequisition    *requisition);
static void basep_widget_size_allocate	(GtkWidget         *widget,
					 GtkAllocation     *allocation);

static GtkWindowClass *parent_class = NULL;

/*global settings*/
extern int pw_explicit_step;
extern int pw_drawer_step;
extern int pw_auto_step;
extern int pw_minimized_size;
extern int pw_minimize_delay;
extern int pw_disable_animations;
extern PanelMovementType pw_movement_type;

extern GlobalConfig global_config;

/************************
 widget core
 ************************/

guint
basep_widget_get_type ()
{
	static guint basep_widget_type = 0;

	if (!basep_widget_type) {
		GtkTypeInfo basep_widget_info = {
			"BasePWidget",
			sizeof (BasePWidget),
			sizeof (BasePWidgetClass),
			(GtkClassInitFunc) basep_widget_class_init,
			(GtkObjectInitFunc) basep_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};

		basep_widget_type = gtk_type_unique (gtk_window_get_type (),
						     &basep_widget_info);
	}

	return basep_widget_type;
}

static void
basep_widget_realize(GtkWidget *w)
{
	BasePWidget *basep = BASEP_WIDGET(w);
	GTK_WIDGET_CLASS(parent_class)->realize(w);

	set_frame_colors(PANEL_WIDGET(basep->panel),
			 basep->frame,
			 basep->hidebutton_n,
			 basep->hidebutton_e,
			 basep->hidebutton_w,
			 basep->hidebutton_s);
}

static void
basep_widget_class_init (BasePWidgetClass *class)
{
	GtkObjectClass *object_class = (GtkObjectClass*) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass*) class;

        parent_class = gtk_type_class (gtk_window_get_type ());

	class->set_hidebuttons = NULL;
	
	widget_class->realize = basep_widget_realize;
}


static int
basep_enter_notify(GtkWidget *basep,
		   GdkEventCrossing *event,
		   gpointer data)
{
	if (!gnome_win_hints_wm_exists() &&
	    global_config.autoraise)
		gdk_window_raise(basep->window);
	return FALSE;
}


static GtkWidget *
make_hidebutton(BasePWidget *basep,
		char *pixmaparrow,
		int horizontal)
{
	GtkWidget *w;
	GtkWidget *pixmap;
	char *pixmap_name;

	w=gtk_button_new();
	GTK_WIDGET_UNSET_FLAGS(w,GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
	if(horizontal)
		gtk_widget_set_usize(w,0,PANEL_MINIMUM_WIDTH);
	else
		gtk_widget_set_usize(w,PANEL_MINIMUM_WIDTH,0);

	pixmap_name=gnome_unconditional_pixmap_file(pixmaparrow);
	pixmap = gnome_pixmap_new_from_file(pixmap_name);
	g_free(pixmap_name);
	gtk_widget_show(pixmap);

	gtk_container_add(GTK_CONTAINER(w),pixmap);
	gtk_object_set_user_data(GTK_OBJECT(w), pixmap);
	return w;
}

static void
basep_widget_init (BasePWidget *basep)
{
	/*if we set the gnomewm hints it will have to be changed to TOPLEVEL*/
	gnome_win_hints_init();
	if (gnome_win_hints_wm_exists())
		GTK_WINDOW(basep)->type = GTK_WINDOW_TOPLEVEL;
	else
		GTK_WINDOW(basep)->type = GTK_WINDOW_POPUP;
	GTK_WINDOW(basep)->allow_shrink = TRUE;
	GTK_WINDOW(basep)->allow_grow = TRUE;
	GTK_WINDOW(basep)->auto_shrink = TRUE;

	/*this makes the popup "pop down" once the button is released*/
	gtk_widget_set_events(GTK_WIDGET(basep),
			      gtk_widget_get_events(GTK_WIDGET(basep)) |
			      GDK_BUTTON_RELEASE_MASK);

	basep->table = gtk_table_new(3,3,FALSE);
	gtk_container_add(GTK_CONTAINER(basep),basep->table);
	gtk_widget_show(basep->table);

	basep->frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(basep->frame),GTK_SHADOW_OUT);

	gtk_table_attach(GTK_TABLE(basep->table),basep->frame,1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);



	gtk_signal_connect(GTK_OBJECT(basep), "enter_notify_event",
			   GTK_SIGNAL_FUNC(basep_enter_notify),
			   NULL);

	basep->hidebuttons_enabled = TRUE;
	basep->hidebutton_pixmaps_enabled = TRUE;
}

static void
show_hidebutton_pixmap(GtkWidget *hidebutton, int show)
{
	GtkWidget *pixmap;

	pixmap = gtk_object_get_user_data(GTK_OBJECT(hidebutton));

	if (!pixmap) return;

	if (show)
		gtk_widget_show(pixmap);
	else
		gtk_widget_hide(pixmap);
}

static void
basep_widget_show_hidebutton_pixmaps(BasePWidget *basep)
{
	int show = basep->hidebutton_pixmaps_enabled;
	show_hidebutton_pixmap(basep->hidebutton_n, show);
	show_hidebutton_pixmap(basep->hidebutton_e, show);
	show_hidebutton_pixmap(basep->hidebutton_w, show);
	show_hidebutton_pixmap(basep->hidebutton_s, show);
}

static void
b_back_change(PanelWidget *panel,
	      PanelBackType type,
	      char *pixmap,
	      GdkColor *color,
	      BasePWidget *basep)
{
	if(type == PANEL_BACK_PIXMAP &&
	   basep->panel->parent == basep->frame) {
		gtk_widget_hide(basep->frame);
		gtk_widget_ref(basep->panel);
		gtk_container_remove(GTK_CONTAINER(basep->frame),
				     basep->panel);
		gtk_table_attach(GTK_TABLE(basep->table),basep->panel,
				 1,2,1,2,
				 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
				 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
				 0,0);
		gtk_widget_unref(basep->panel);
	} else if(type != PANEL_BACK_PIXMAP &&
		  basep->panel->parent == basep->table) {
		gtk_widget_ref(basep->panel);
		gtk_container_remove(GTK_CONTAINER(basep->table),
				     basep->panel);
		gtk_container_add(GTK_CONTAINER(basep->frame),
				  basep->panel);
		gtk_widget_unref(basep->panel);
		gtk_widget_show(basep->frame);
	}

	set_frame_colors(panel,
			 basep->frame,
			 basep->hidebutton_n,
			 basep->hidebutton_e,
			 basep->hidebutton_w,
			 basep->hidebutton_s);
}

GtkWidget*
basep_widget_construct (BasePWidget *basep,
			int packed,
			int reverse_arrows,
			PanelOrientation orient,
			int hidebuttons_enabled,
			int hidebutton_pixmaps_enabled,
			PanelBackType back_type,
			char *back_pixmap,
			int fit_pixmap_bg,
			GdkColor *back_color)
{
	basep->panel = panel_widget_new(packed,
					orient,
					back_type,
					back_pixmap,
					fit_pixmap_bg,
					back_color);
	gtk_signal_connect_after(GTK_OBJECT(basep->panel), "back_change",
				 GTK_SIGNAL_FUNC(b_back_change),
				 basep);
	gtk_object_set_data(GTK_OBJECT(basep->panel),PANEL_PARENT,
			    basep);
	PANEL_WIDGET(basep->panel)->drop_widget = GTK_WIDGET(basep);

	gtk_widget_show(basep->panel);

	if(back_type != PANEL_BACK_PIXMAP) {
		gtk_widget_show(basep->frame);
		gtk_container_add(GTK_CONTAINER(basep->frame),basep->panel);
	} else {
		gtk_table_attach(GTK_TABLE(basep->table),basep->panel,
				 1,2,1,2,
				 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
				 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
				 0,0);
	}

	/*we add all the hide buttons to the table here*/
	/*EAST*/
	basep->hidebutton_e = make_hidebutton(basep,
					      reverse_arrows?
					      "panel-arrow-right.png":
					      "panel-arrow-left.png",
					      TRUE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_e,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	/*NORTH*/
	basep->hidebutton_n = make_hidebutton(basep,
					      reverse_arrows?
					      "panel-arrow-down.png":
					      "panel-arrow-up.png",
					      FALSE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_n,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);
	/*WEST*/
	basep->hidebutton_w = make_hidebutton(basep,
					      reverse_arrows?
					      "panel-arrow-left.png":
					      "panel-arrow-right.png",
					      TRUE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_w,
			 2,3,1,2,GTK_FILL,GTK_FILL,0,0);
	/*SOUTH*/
	basep->hidebutton_s = make_hidebutton(basep,
					      reverse_arrows?
					      "panel-arrow-up.png":
					      "panel-arrow-down.png",
					      FALSE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_s,
			 1,2,2,3,GTK_FILL,GTK_FILL,0,0);

	basep->hidebuttons_enabled = hidebuttons_enabled;
	basep->hidebutton_pixmaps_enabled = hidebutton_pixmaps_enabled;

	basep_widget_set_hidebuttons(basep);
	basep_widget_show_hidebutton_pixmaps(basep);

	return GTK_WIDGET(basep);
}

#include "drawer-widget.h"

void
basep_widget_change_params(BasePWidget *basep,
			   PanelOrientation orient,
			   int hidebuttons_enabled,
			   int hidebutton_pixmaps_enabled,
			   PanelBackType back_type,
			   char *pixmap_name,
			   int fit_pixmap_bg,
			   GdkColor *back_color)
{
	g_return_if_fail(basep);
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(basep)));
	
	basep->hidebuttons_enabled = hidebuttons_enabled;
	basep->hidebutton_pixmaps_enabled = hidebutton_pixmaps_enabled;

	panel_widget_change_params(PANEL_WIDGET(basep->panel),
				   orient,
				   back_type,
				   pixmap_name,
				   fit_pixmap_bg,
				   back_color);

	basep_widget_set_hidebuttons(basep);
	basep_widget_show_hidebutton_pixmaps(basep);
}

void
basep_widget_enable_buttons(BasePWidget *basep)
{
	gtk_widget_set_sensitive(basep->hidebutton_n,TRUE);
	gtk_widget_set_sensitive(basep->hidebutton_e,TRUE);
	gtk_widget_set_sensitive(basep->hidebutton_w,TRUE);
	gtk_widget_set_sensitive(basep->hidebutton_s,TRUE);
}


void
basep_widget_disable_buttons(BasePWidget *basep)
{
	gtk_widget_set_sensitive(basep->hidebutton_n,FALSE);
	gtk_widget_set_sensitive(basep->hidebutton_e,FALSE);
	gtk_widget_set_sensitive(basep->hidebutton_w,FALSE);
	gtk_widget_set_sensitive(basep->hidebutton_s,FALSE);
}

void
basep_widget_set_hidebuttons(BasePWidget *basep)
{
	BasePWidgetClass *class = BASEP_WIDGET_CLASS(GTK_OBJECT(basep)->klass);
	if(class->set_hidebuttons)
		(*class->set_hidebuttons)(basep);
}
