/* Gnome panel: basep widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
#include <math.h>
#include <config.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gnome.h>
#include "panel-widget.h"
#include "basep-widget.h"
#include "panel-util.h"
#include "panel_config_global.h"
#include "gdkextra.h"

extern int panel_applet_in_drag;

static void basep_widget_class_init	(BasePWidgetClass *klass);
static void basep_widget_init		(BasePWidget      *basep);

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

static void
get_position(BasePWidget *basep, PanelOrientType hide_orient,
	     int *x, int *y, int w, int h)
{
	GtkWidget *widget = GTK_WIDGET(basep);

	*x = *y = 0;
	switch(hide_orient) {
	case ORIENT_UP:
		if(h < widget->allocation.height)
			*y -= widget->allocation.height - h;
		break;
	case ORIENT_DOWN:
		break;
	case ORIENT_LEFT:
		if(w < widget->allocation.width)
			*x -= widget->allocation.width - w;
		break;
	case ORIENT_RIGHT:
		break;
	default:
		break;
	}
}

void
basep_widget_add_fake(BasePWidget *basep,
		      PanelOrientType hide_orient,
		      int override, int x, int y, int w, int h,
		      int show)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	GdkWindow *window;
	GdkWindow *win = basep->fake;
	GtkWidget *widget = GTK_WIDGET(basep);
	int rx,ry;

	if(!override && !gnome_win_hints_wm_exists())
		override = TRUE;

	if(override)
		attributes.window_type = GDK_WINDOW_TEMP;
	else
		attributes.window_type = GDK_WINDOW_TOPLEVEL;

	if(x<0)
		attributes.x = widget->allocation.x;
	else
		attributes.x = x;
	if(y<0)
		attributes.y = widget->allocation.y;
	else
		attributes.y = y;
	if(w<0)
		attributes.width = widget->allocation.width;
	else
		attributes.width = w;
	if(h<0)
		attributes.height = widget->allocation.height;
	else
		attributes.height = h;

	attributes.wclass = GDK_INPUT_OUTPUT;

	attributes.visual = gtk_widget_get_visual(widget);
	attributes.colormap = gtk_widget_get_colormap(widget);

	attributes.event_mask = gtk_widget_get_events (widget);
	/*attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;*/

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	window = gdk_window_new (NULL, &attributes, attributes_mask);

	/*hack to set the right hints and stuff*/
	if(!override) {
		GdkWindow *orig = widget->window;

		/*pretend that that's the right window*/
		widget->window = window;
		gnome_win_hints_set_hints(widget, 
					  WIN_HINTS_SKIP_FOCUS |
					  WIN_HINTS_SKIP_WINLIST |
					  WIN_HINTS_SKIP_TASKBAR);
		gnome_win_hints_set_state(widget, 
					  WIN_STATE_STICKY |
					  WIN_STATE_FIXED_POSITION);
		gnome_win_hints_set_layer(widget, WIN_LAYER_ABOVE_DOCK);

		gnome_win_hints_set_expanded_size(widget, 0, 0, 0, 0);
		
		/*put the original window of the basep back*/
		widget->window = orig;

		gdk_window_set_decorations(window, 0);
	}
	
	basep_widget_set_fake_orient(basep,hide_orient);
	
	if(show) {
		gdk_window_show(window);
		gdk_flush();
	}
	
	get_position(basep, hide_orient, &rx, &ry,
		     attributes.width,attributes.height);

	gdk_window_reparent(widget->window,window,rx,ry);
	
	basep->fake = window;
	basep->fake_override = override;
	if(win) gdk_window_destroy(win);
}

void
basep_widget_set_infake_position(BasePWidget *basep,
				 PanelOrientType hide_orient,
				 int w, int h)
{
	int x,y;

	g_return_if_fail(basep->fake!=NULL);

	if(w<0)
		w = GTK_WIDGET(basep)->allocation.width;
	if(h<0)
		h = GTK_WIDGET(basep)->allocation.height;

	get_position(basep, hide_orient, &x, &y, w, h);

	gdk_window_show(GTK_WIDGET(basep)->window);
	gdk_flush();
	gdk_window_move(GTK_WIDGET(basep)->window,x,y);
}


void
basep_widget_set_fake_orient(BasePWidget *basep,
			     PanelOrientType hide_orient)
{
	GtkWidget *widget = GTK_WIDGET(basep);
	XSetWindowAttributes xattributes;

	switch(hide_orient) {
	case ORIENT_UP:
		xattributes.win_gravity = SouthGravity;
		break;
	case ORIENT_DOWN:
		xattributes.win_gravity = NorthGravity;
		break;
	case ORIENT_LEFT:
		xattributes.win_gravity = EastGravity;
		break;
	case ORIENT_RIGHT:
		xattributes.win_gravity = WestGravity;
		break;
	default:
		xattributes.win_gravity = NorthWestGravity;
		break;
	}

	XChangeWindowAttributes (GDK_WINDOW_XDISPLAY(widget->window),
				 GDK_WINDOW_XWINDOW(widget->window),
				 CWWinGravity,  &xattributes);
	
}

void
basep_widget_remove_fake(BasePWidget *basep)
{
	GtkWidget *wid = GTK_WIDGET(basep);
	gdk_window_reparent(wid->window,NULL,wid->allocation.x,wid->allocation.y);
	gdk_window_destroy(basep->fake);
	basep->fake = NULL;
}


static int
move_step(int src, int dest, int pos, int step)
{
	int range;
	int diff;
	int percentage;
	int ret;
	
	if(src == dest)
		return 0;

	range = abs(src-dest);
	diff = abs(range-abs(pos-src));
	percentage = (diff*100)/range;

	if(percentage>50)
		percentage = 100-percentage;
	
	ret = (src>dest?-1:1)*(((step>>1)*log((percentage/10.0)+1))+1);
	if(src>dest) {
		if(ret+pos<dest) ret = dest - pos;
	} else {
		if(ret+pos>dest) ret = dest - pos;
	}
	return ret;
}

void
basep_widget_do_hiding(BasePWidget *basep, PanelOrientType hide_orient,
		       int leftover, int step)
{
	GtkWidget *wid;
	int ox,oy,ow,oh;
	int x,y,w,h;
	int dx,dy,dw,dh;

	g_return_if_fail(basep != NULL);
	g_return_if_fail(IS_BASEP_WIDGET(basep));

	wid = GTK_WIDGET(basep);
	
	ox = x = wid->allocation.x;
	oy = y = wid->allocation.y;
	ow = w = wid->allocation.width;
	oh = h = wid->allocation.height;
	
	switch(hide_orient) {
	case ORIENT_UP:
		dx = x;
		dy = y;
		dw = w;
		dh = leftover;
		break;
	case ORIENT_DOWN:
		dx = x;
		dy = y+h-leftover;
		dw = w;
		dh = leftover;
		break;
	case ORIENT_LEFT:
		dx = x;
		dy = y;
		dw = leftover;
		dh = h;
		break;
	case ORIENT_RIGHT:
		dx = x+w-leftover;
		dy = y;
		dw = leftover;
		dh = h;
		break;
	default:
		/*fix warning*/ dx = dy = dw = dh = 0;
		g_assert_not_reached();
		break;
	}

	if(!pw_disable_animations && step != 0) {
		if(basep->fake && basep->fake_override) {
			gdk_window_show(basep->fake);
			gdk_flush();
			basep_widget_set_fake_orient(basep,hide_orient);
			basep_widget_set_infake_position(basep, hide_orient,
							 -1,-1);
		} else {
			basep_widget_add_fake(basep, hide_orient,
					      TRUE, -1,-1,-1,-1,TRUE);
		}
		while(x != dx ||
		      y != dy ||
		      w != dw ||
		      h != dh) {
			x += move_step(ox,dx,x,step);
			y += move_step(oy,dy,y,step);
			w += move_step(ow,dw,w,step);
			h += move_step(oh,dh,h,step);
			gdk_window_move_resize(basep->fake, x,y,w,h);
			gtk_widget_draw(wid, NULL);
		}
	}
	
	wid->allocation.x = dx;
	wid->allocation.y = dy;
	
	if(dw == 0 || dh == 0) {
		if(basep->fake) {
			if (gnome_win_hints_wm_exists()) {
				gdk_window_reparent(wid->window,NULL,
						    -ow-1,-oh-1);
				gdk_window_destroy(basep->fake);
				basep->fake = NULL;
			} else {
				gdk_window_hide(basep->fake);
			}
		}
		gtk_widget_hide(wid);
	} else {
		if(!basep->fake) {
			basep_widget_add_fake(basep, hide_orient,
					      FALSE, -1,-1,dw,dh,TRUE);
		} else {
			if (gnome_win_hints_wm_exists()) {
				basep_widget_add_fake(basep,
						      hide_orient,
						      FALSE,
						      -1,-1,dw,dh,TRUE);
			} else {
				gdk_window_resize(basep->fake,dw,dh);
			}
		}
	}
}

void
basep_widget_do_showing(BasePWidget *basep, PanelOrientType hide_orient,
			int leftover, int step)
{
	GtkWidget *wid;
	int ox,oy,ow,oh;
	int x,y,w,h;
	int dx,dy,dw,dh;

	g_return_if_fail(basep != NULL);
	g_return_if_fail(IS_BASEP_WIDGET(basep));

	wid = GTK_WIDGET(basep);

	ox = x = wid->allocation.x;
	oy = y = wid->allocation.y;
	dw = wid->allocation.width;
	dh = wid->allocation.height;
	
	switch(hide_orient) {
	case ORIENT_UP:
		dx = x;
		dy = y;
		ow = w = dw;
		oh = h = leftover;
		break;
	case ORIENT_DOWN:
		dx = x;
		dy = y - dh + leftover;
		ow = w = dw;
		oh = h = leftover;
		break;
	case ORIENT_LEFT:
		dx = x;
		dy = y;
		ow = w = leftover;
		oh = h = dh;
		break;
	case ORIENT_RIGHT:
		dx = x - dw + leftover;
		dy = y;
		ow = w = leftover;
		oh = h = dh;
		break;
	default:
		/*fix warning*/ dx = dy = ow = oh = w = h = 0;
		g_assert_not_reached();
		break;
	}
	
	if(!pw_disable_animations && step != 0) {
		//puts("1"); sleep(1);
		if(basep->fake && basep->fake_override && ow && oh) {
		//puts("2"); sleep(1);
			gdk_window_hide(basep->fake);
		//puts("3"); sleep(1);
			basep_widget_set_fake_orient(basep,hide_orient);
			basep_widget_set_infake_position(basep, hide_orient,
							 ow,oh);
		//puts("4"); sleep(1);
			gdk_window_resize(basep->fake,ow,oh);
		//puts("5"); sleep(1);
		} else {
		//puts("6"); sleep(1);
			basep_widget_add_fake(basep, hide_orient,
					      TRUE, -1,-1,ow,oh,FALSE);
		//puts("7"); sleep(1);
		}
		//puts("8"); sleep(1);
		/*make sure the window doesn't blink*/
		//puts("9"); sleep(1);
		gtk_widget_show_now(wid);
		//puts("10"); sleep(1);
		gdk_window_show(basep->fake);
		gdk_flush();
		//puts("11"); sleep(1);
		while(x != dx ||
		      y != dy ||
		      w != dw ||
		      h != dh) {
			x += move_step(ox,dx,x,step);
			y += move_step(oy,dy,y,step);
			w += move_step(ow,dw,w,step);
			h += move_step(oh,dh,h,step);
			gdk_window_move_resize(basep->fake, x,y,w,h);
			gtk_widget_draw(wid, NULL);
		}
	}
	
	wid->allocation.x = dx;
	wid->allocation.y = dy;
	
	if(basep->fake) {
		if (gnome_win_hints_wm_exists()) {
			gdk_window_reparent(wid->window,NULL,dx,dy);
			gdk_window_destroy(basep->fake);
			basep->fake = NULL;
		/*else we would have an override redirect window anyhow,
		  so just keep it to avoid a flash*/
		} else {
			gdk_window_move_resize(basep->fake,dx,dy,dw,dh);
		}
	} else {
		gtk_widget_set_uposition(wid,dx,dy);
		gtk_widget_show_now(wid);
	}
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
	
	basep->fake = NULL;
	basep->fake_override = FALSE;

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
