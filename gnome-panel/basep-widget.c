/* Gnome panel: basep widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Jacob Berkman
 */
#include <config.h>
#include <math.h>
#include <string.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-macros.h>
#include "panel-marshal.h"
#include "panel-widget.h"
#include "basep-widget.h"
#include "panel-util.h"
#include "panel-config-global.h"
#include "foobar-widget.h"
#include "drawer-widget.h"
#include "border-widget.h"
#include "edge-widget.h"
#include "aligned-widget.h"
#include "xstuff.h"
#include "multiscreen-stuff.h"
#include "panel-typebuiltins.h"
#include "panel-gconf.h"
#include "panel-stock-icons.h"

extern GSList *panel_list;

extern int panels_to_sync;

/*global settings*/

extern GtkTooltips *panel_tooltips;
extern GlobalConfig global_config;

static GtkWindowClass *basep_widget_parent_class = NULL;
static GtkObjectClass *basep_pos_parent_class = NULL;

static void basep_widget_class_init (BasePWidgetClass *klass);
static void basep_widget_instance_init (BasePWidget *basep);

static void basep_pos_class_init (BasePPosClass *klass);
static void basep_pos_instance_init (BasePPos *pos);

/* Forward declare some static functions for use in the class init */
static void basep_widget_mode_change (BasePWidget *basep, BasePMode mode);
static void basep_widget_state_change (BasePWidget *basep, BasePState state);
static void basep_widget_real_screen_change (BasePWidget *basep, int screen, int monitor);
static gboolean basep_widget_popup_panel_menu (BasePWidget *basep);
static void basep_widget_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void basep_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void basep_widget_realize (GtkWidget *w);
static void basep_widget_map (GtkWidget *w);
static gboolean basep_enter_notify (GtkWidget *widget, GdkEventCrossing *event);
static gboolean basep_leave_notify (GtkWidget *widget, GdkEventCrossing *event);
static void basep_style_set (GtkWidget *widget, GtkStyle *previous_style);
static void basep_widget_destroy (GtkObject *o);
static int  basep_widget_focus_in_event (GtkWidget     *widget,
					 GdkEventFocus *event);

static void basep_widget_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void basep_widget_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

static BasePPosClass * basep_widget_get_pos_class (BasePWidget *basep);

enum {
	/*TYPE_CHANGE_SIGNAL,*/
	MOVE_FOCUS_OUT_SIGNAL,
	MODE_CHANGE_SIGNAL,
	STATE_CHANGE_SIGNAL,
	SCREEN_CHANGE_SIGNAL,
	POPUP_PANEL_MENU_SIGNAL,
	WIDGET_LAST_SIGNAL
};

enum { 
	PROP_0,
        PROP_MODE,
        PROP_STATE,
        PROP_LEVEL,
        PROP_AVOID_ON_MAXIMIZE,
        PROP_HIDEBUTTONS_ENABLED,
        PROP_HIDEBUTTON_PIXMAPS_ENABLED,
};

#define PANEL_SLOW_STEP_SIZE 10
#define PANEL_MEDIUM_STEP_SIZE 60
#define PANEL_FAST_STEP_SIZE 80

/************************
 widget core
 ************************/

GType
basep_widget_get_type (void)					
{
	static GType object_type = 0;
	if (object_type == 0) {
		static const GTypeInfo object_info = {
		    sizeof (BasePWidgetClass),
		    (GBaseInitFunc)         NULL,
		    (GBaseFinalizeFunc)     NULL,
		    (GClassInitFunc)        basep_widget_class_init,
		    NULL,                   /* class_finalize */
		    NULL,                   /* class_data */
		    sizeof (BasePWidget),
		    0,                      /* n_preallocs */
		    (GInstanceInitFunc)     basep_widget_instance_init 
		};
		object_type = g_type_register_static
		    (GTK_TYPE_WINDOW, "BasePWidget", &object_info, 0);
		basep_widget_parent_class = g_type_class_ref (GTK_TYPE_WINDOW);
	}
	return object_type;
}

static guint basep_widget_signals[WIDGET_LAST_SIGNAL] = { 0 };

static void
basep_widget_class_init (BasePWidgetClass *klass)
{
	GObjectClass   *object_class = (GObjectClass *) klass;
	GtkObjectClass *gtk_object_class = (GtkObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
	GtkBindingSet  *binding_set;

	binding_set = gtk_binding_set_by_class (klass);

	basep_widget_parent_class = g_type_class_ref (gtk_window_get_type ());

	klass->mode_change = basep_widget_mode_change;
	klass->state_change = basep_widget_state_change;
	klass->screen_change = basep_widget_real_screen_change;
	klass->popup_panel_menu = basep_widget_popup_panel_menu;

	widget_class->size_request = basep_widget_size_request;
	widget_class->size_allocate = basep_widget_size_allocate;
	widget_class->realize = basep_widget_realize;
	widget_class->map = basep_widget_map;
	widget_class->enter_notify_event = basep_enter_notify;
	widget_class->leave_notify_event = basep_leave_notify;
	widget_class->focus_in_event = basep_widget_focus_in_event;
	widget_class->style_set = basep_style_set;

	gtk_object_class->destroy = basep_widget_destroy;

	object_class->set_property = basep_widget_set_property;
	object_class->get_property = basep_widget_get_property;

        g_object_class_install_property (object_class,
                                        PROP_MODE,
                                        g_param_spec_enum ("mode",
                                                             _("Mode"),
                                                             _("Mode of this panel"),
							    PANEL_TYPE_PMODE,
                                                            BASEP_EXPLICIT_HIDE,
                                                            G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                        PROP_STATE,
                                        g_param_spec_enum ("state",
                                                             _("State"),
                                                             _("Current state of this panel"),
							     PANEL_TYPE_PSTATE,
                                                             BASEP_SHOWN,
                                                             G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
        			  	PROP_HIDEBUTTONS_ENABLED,
                                        g_param_spec_boolean ("hidebuttons_enabled",
                                                             _("Hidebuttons enabled"),
                                                             _("Are hidebuttons (buttons that hide or show the panel) enabled?"),
                                                             TRUE,
                                                             G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
        			  	PROP_HIDEBUTTON_PIXMAPS_ENABLED,
                                        g_param_spec_boolean ("hidebutton_pixmaps_enabled",
                                                             _("Hidebutton pixmaps enabled"),
                                                             _("Hidebuttons have pixmaps"),
                                                             TRUE,
                                                             G_PARAM_READWRITE));

	/*basep_widget_signals[TYPE_CHANGE_SIGNAL] = 
		g_signal_new ("type_change",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BasePWidgetClass, type_change),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__ENUM,
			      G_TYPE_NONE,
			      1,
			      PANEL_TYPE_OBJECT_TYPE); */

	basep_widget_signals[MODE_CHANGE_SIGNAL] = 
		g_signal_new	("mode_change",
			       	G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BasePWidgetClass, mode_change),
				NULL,
				NULL,
				g_cclosure_marshal_VOID__ENUM,
				G_TYPE_NONE,
				1,
				PANEL_TYPE_PMODE);

	basep_widget_signals[STATE_CHANGE_SIGNAL] = 
		g_signal_new	("state_change",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BasePWidgetClass, state_change),
				NULL,
				NULL,
				g_cclosure_marshal_VOID__ENUM,
				G_TYPE_NONE,
				1,
				PANEL_TYPE_PSTATE);

	basep_widget_signals [SCREEN_CHANGE_SIGNAL] = 
		g_signal_new   ("screen_change",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BasePWidgetClass, screen_change),
				NULL,
				NULL,
				panel_marshal_VOID__INT_INT,
				G_TYPE_NONE,
				2,
				G_TYPE_INT,
				G_TYPE_INT);

	basep_widget_signals [POPUP_PANEL_MENU_SIGNAL] =
		g_signal_new ("popup_panel_menu",
			     G_TYPE_FROM_CLASS (object_class),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			     G_STRUCT_OFFSET (BasePWidgetClass, popup_panel_menu),
			     NULL,
			     NULL,
			     panel_marshal_BOOLEAN__VOID,
			     G_TYPE_BOOLEAN,
			     0);

	gtk_binding_entry_add_signal (binding_set, GDK_F10, GDK_CONTROL_MASK,
				     "popup_panel_menu", 0);
}

static void
basep_widget_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{

		/* Does nothing yet */
}

static void
basep_widget_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	BasePWidget *basep;

	basep = BASEP_WIDGET (object);

	switch (prop_id) {
        	case PROP_MODE:
			basep->mode = g_value_get_enum (value);
			break;
        	case PROP_STATE:
			basep->state = g_value_get_enum (value);
			break;
        	case PROP_HIDEBUTTONS_ENABLED:
			basep->hidebuttons_enabled = g_value_get_boolean (value);
			break;
		case PROP_HIDEBUTTON_PIXMAPS_ENABLED:
			basep->hidebutton_pixmaps_enabled = g_value_get_boolean (value);
			break;
		default:
               		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

void
basep_widget_screen_size_changed (BasePWidget *basep,
				  GdkScreen   *screen)
{
	GtkWindow *window;
	int        w, h;
	int        x, y;

	window = GTK_WINDOW (basep);

	basep_widget_get_pos (basep, &x, &y);
	basep_widget_get_size (basep, &w, &h);

	gtk_window_move (window, x, y);

	gtk_window_set_resizable (window, TRUE);
	gtk_widget_set_size_request (GTK_WIDGET (basep), w, h);
	gtk_window_resize (window, w, h);
}

static void
basep_widget_realize (GtkWidget *w)
{
	BasePWidget *basep = BASEP_WIDGET (w);
	BasePPosClass *klass;

	g_return_if_fail (BASEP_IS_WIDGET (basep));

	gtk_window_set_wmclass (GTK_WINDOW (basep),
				"panel_window", "Panel");

	GTK_WIDGET_CLASS (basep_widget_parent_class)->realize (w);

	basep_widget_update_winhints (basep);
	xstuff_set_no_group_and_no_input (w->window);

	panel_set_frame_colors (PANEL_WIDGET (basep->panel),
				basep->frame,
				basep->hidebutton_n,
				basep->hidebutton_e,
				basep->hidebutton_w,
				basep->hidebutton_s);

	if (basep->strut_left != 0 ||
	    basep->strut_right != 0 ||
	    basep->strut_top != 0 ||
	    basep->strut_bottom != 0) {
		xstuff_set_wmspec_strut (w->window,
					 basep->strut_left,
					 basep->strut_right,
					 basep->strut_top,
					 basep->strut_bottom);
	}
	
	klass = basep_widget_get_pos_class (basep);
	g_return_if_fail (klass);
	if (klass->realize != NULL)
		klass->realize (w);
}

static void
basep_widget_map (GtkWidget *w)
{
        BasePWidget *basep = BASEP_WIDGET (w);

        g_return_if_fail (BASEP_IS_WIDGET (basep));

        if (GTK_WIDGET_CLASS (basep_widget_parent_class)->map != NULL)
		GTK_WIDGET_CLASS (basep_widget_parent_class)->map (w);

        basep_widget_update_winhints (basep);
}

static void
basep_widget_size_request (GtkWidget *widget,
			   GtkRequisition *requisition)
{
	GtkRequisition chreq;

	BasePWidget *basep = BASEP_WIDGET(widget);
	BasePPosClass *klass = basep_widget_get_pos_class (basep);

	g_assert (klass);

	if (basep->request_cube) {
		requisition->width = requisition->height =
			PANEL_MINIMUM_WIDTH;
		basep->request_cube = FALSE;
		return;
	}

	gtk_widget_size_request (basep->ebox, &chreq);

	/* this typically only does stuff on edge panels */
	if (klass->get_size) {
		int w,h;
		w = chreq.width;
		h = chreq.height;
		klass->get_size(basep, &w, &h);
		chreq.width = w;
		chreq.height = h;
	}

	if (basep->state != BASEP_SHOWN) {
		int w,h;
		PanelOrient hide_orient =
			klass->get_hide_orient (basep);
		w = chreq.width;
		h = chreq.height;
		klass->get_hide_size (basep, hide_orient, &w, &h);
		chreq.width = w;
		chreq.height = h;
	}

	requisition->width = chreq.width;
	requisition->height = chreq.height;
}

static void
basep_widget_size_allocate (GtkWidget *widget,
			    GtkAllocation *allocation)
{
	GtkAllocation challoc;
	GtkRequisition chreq;
	
	BasePWidget *basep = BASEP_WIDGET(widget);
	BasePPosClass *klass = basep_widget_get_pos_class (basep);
	
	/*we actually want to ignore the size_reqeusts since they
	  are sometimes a cube for the flicker prevention*/

	gtk_widget_size_request (basep->ebox, &chreq);

	if (klass->get_size) {
		int w,h;
		w = chreq.width;
		h = chreq.height;
		klass->get_size(basep, &w, &h);
		chreq.width = w;
		chreq.height = h;
	}
	if (klass->get_pos) {
		int x,y;
		klass->get_pos (basep, &x, &y,
				chreq.width,
				chreq.height);
		allocation->x = x;
		allocation->y = y;
	}

	allocation->width = challoc.width = chreq.width;
	allocation->height = challoc.height = chreq.height;
	challoc.x = challoc.y = 0;

	basep->shown_alloc = *allocation;

	if (basep->state != BASEP_SHOWN) {
		int w,h,x,y;
		PanelOrient hide_orient = 
			klass->get_hide_orient (basep);

		w = allocation->width;
		h = allocation->height;
		klass->get_hide_size (basep, hide_orient, &w, &h);
		allocation->width = w;
		allocation->height = h;

		x = allocation->x;
		y = allocation->y;
		klass->get_hide_pos (basep, hide_orient,
				     &x, &y,
				     basep->shown_alloc.width,
				     basep->shown_alloc.height);
		allocation->x = x;
		allocation->y = y;

		basep_widget_get_position (basep, hide_orient,
					   &x, &y,
					   allocation->width,
					   allocation->height);
		challoc.x = x;
		challoc.y = y;
	}

	if (basep->keep_in_screen) {
		gint16 max;

		max = multiscreen_width (basep->screen, basep->monitor) -
			allocation->width +
			multiscreen_x (basep->screen, basep->monitor);

		if (allocation->x < multiscreen_x (basep->screen, basep->monitor))
			allocation->x = multiscreen_x (basep->screen, basep->monitor);
		else if (allocation->x > max)
			allocation->x = max;


		max = multiscreen_height (basep->screen, basep->monitor) -
			allocation->height +
			multiscreen_y (basep->screen, basep->monitor);

		if (allocation->y < multiscreen_y (basep->screen, basep->monitor))
			allocation->y = multiscreen_y (basep->screen, basep->monitor);
		else if (allocation->y > max)
			allocation->y = max;
				       
	}
	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED(widget)) {
		xstuff_set_pos_size (widget->window,
				     allocation->x, 
				     allocation->y,
				     allocation->width,
				     allocation->height);
	}

	gtk_widget_size_allocate (basep->ebox, &challoc);

	/* FIXME: this should be handled in a nicer way */
	if (BORDER_IS_WIDGET (basep)) {
		int left = 0, right = 0, top = 0, bottom = 0;
		switch (BORDER_POS (basep->pos)->edge) {
		case BORDER_LEFT:
			if (basep->mode == BASEP_AUTO_HIDE)
				left = global_config.minimized_size ;
			else
				left = basep->shown_alloc.width;
                        left += allocation->x;
			break;
		case BORDER_RIGHT:
			if (basep->mode == BASEP_AUTO_HIDE)
				right = global_config.minimized_size ;
			else
				right = basep->shown_alloc.width;
                        right += multiscreen_width (basep->screen, basep->monitor) -
                          (allocation->x + allocation->width);
			break;
		case BORDER_TOP:
			if (basep->mode == BASEP_AUTO_HIDE)
				top = global_config.minimized_size ;
			else
				top = basep->shown_alloc.height;
                        top += allocation->y;
			break;
		case BORDER_BOTTOM:
			if (basep->mode == BASEP_AUTO_HIDE)
				bottom = global_config.minimized_size ;
			else
				bottom = basep->shown_alloc.height;
                        bottom += multiscreen_height (basep->screen, basep->monitor) -
                          (allocation->y + allocation->height);
			break;
		}
		if (basep->strut_left != left ||
		    basep->strut_right != right ||
		    basep->strut_top != top ||
		    basep->strut_bottom != bottom) {
			basep->strut_left = left;
			basep->strut_right = right;
			basep->strut_top = top;
			basep->strut_bottom = bottom;
			if (GTK_WIDGET_REALIZED (widget)) {
				xstuff_set_wmspec_strut (widget->window,
							 left, right,
							 top, bottom);
			}
		}
	} else if (basep->strut_left != 0 ||
		   basep->strut_right != 0 ||
		   basep->strut_top != 0 ||
		   basep->strut_bottom != 0) {
		/* If not border widget and it seems to be
		 * set to some bad values just whack the
		 * _NET_WM_STRUT */
		basep->strut_left =
			basep->strut_right =
			basep->strut_top =
			basep->strut_bottom = 0;
		if (GTK_WIDGET_REALIZED (widget)) {
			xstuff_delete_property (widget->window,
						"_NET_WM_STRUT");
		}
	}
}

static void
basep_widget_mode_change (BasePWidget *basep, BasePMode old_mode)
{
	if (BORDER_IS_WIDGET (basep))
		basep_border_queue_recalc (basep->screen, basep->monitor);
}

static void
set_tip (GtkWidget *widget, gboolean showing) 
{
	if (showing)
		gtk_tooltips_set_tip (panel_tooltips, widget,
		      _("Hide this panel"), NULL);
	else
		gtk_tooltips_set_tip (panel_tooltips, widget,
		      _("Show this panel"), NULL);
}

static void
setup_hidebuttons (BasePWidget *basep)
{
	/*
	 * If the state is SHOWN or HIDDEN_LEFT or HIDDEN_RIGHT
	 * we update the tooltips on the buttons, and sensitivity.  Offscreen
	 * stuff is insensitive.
	 */
	if (basep->state == BASEP_SHOWN) {
		gtk_widget_set_sensitive (basep->panel, TRUE);
		gtk_widget_set_sensitive (basep->hidebutton_w, TRUE);
		gtk_widget_set_sensitive (basep->hidebutton_n, TRUE);
		gtk_widget_set_sensitive (basep->hidebutton_s, TRUE);
		gtk_widget_set_sensitive (basep->hidebutton_e, TRUE);

		set_tip (basep->hidebutton_w, TRUE);
		set_tip (basep->hidebutton_n, TRUE);
		set_tip (basep->hidebutton_s, TRUE);
		set_tip (basep->hidebutton_e, TRUE);

		/* move focus nicely in case orientation changed on us */
		if (PANEL_WIDGET (basep->panel)->orient ==
		    GTK_ORIENTATION_VERTICAL) {
			if (GTK_WINDOW (basep)->focus_widget == basep->hidebutton_e)
				gtk_window_set_focus (GTK_WINDOW (basep),
						      basep->hidebutton_s);
			else if (GTK_WINDOW (basep)->focus_widget == basep->hidebutton_w)
				gtk_window_set_focus (GTK_WINDOW (basep),
						      basep->hidebutton_n);
		} else {
			if (GTK_WINDOW (basep)->focus_widget == basep->hidebutton_n)
				gtk_window_set_focus (GTK_WINDOW (basep),
						      basep->hidebutton_w);
			else if (GTK_WINDOW (basep)->focus_widget == basep->hidebutton_s)
				gtk_window_set_focus (GTK_WINDOW (basep),
						      basep->hidebutton_e);
		}
	} else if (basep->state == BASEP_HIDDEN_LEFT) {
		GtkWidget *hb;
		gboolean set_focus = FALSE;

		if (PANEL_WIDGET (basep->panel)->orient ==
		    GTK_ORIENTATION_VERTICAL) {
			hb = basep->hidebutton_s;
			if (GTK_WINDOW (basep)->focus_widget == basep->hidebutton_n ||
			    GTK_WINDOW (basep)->focus_widget == basep->hidebutton_e ||
			    GTK_WINDOW (basep)->focus_widget == basep->hidebutton_w)
				set_focus = TRUE;
		} else {
			hb = basep->hidebutton_e;
			if (GTK_WINDOW (basep)->focus_widget == basep->hidebutton_w ||
			    GTK_WINDOW (basep)->focus_widget == basep->hidebutton_n ||
			    GTK_WINDOW (basep)->focus_widget == basep->hidebutton_s)
				set_focus = TRUE;
		}


		gtk_widget_set_sensitive (basep->panel, FALSE);

		gtk_widget_set_sensitive (basep->hidebutton_w, FALSE);
		gtk_widget_set_sensitive (basep->hidebutton_n, FALSE);
		gtk_widget_set_sensitive (basep->hidebutton_s, FALSE);
		gtk_widget_set_sensitive (basep->hidebutton_e, FALSE);

		set_tip (hb, FALSE);
		gtk_widget_set_sensitive (hb, TRUE);
		if (set_focus)
			gtk_window_set_focus (GTK_WINDOW (basep), hb);
	} else if (basep->state == BASEP_HIDDEN_RIGHT) {
		GtkWidget *hb;
		gboolean set_focus = FALSE;

		if (PANEL_WIDGET (basep->panel)->orient ==
		    GTK_ORIENTATION_VERTICAL) {
			hb = basep->hidebutton_n;
			if (GTK_WINDOW (basep)->focus_widget == basep->hidebutton_s ||
			    GTK_WINDOW (basep)->focus_widget == basep->hidebutton_e ||
			    GTK_WINDOW (basep)->focus_widget == basep->hidebutton_w)
				set_focus = TRUE;
		} else {
			hb = basep->hidebutton_w;
			if (GTK_WINDOW (basep)->focus_widget == basep->hidebutton_e ||
			    GTK_WINDOW (basep)->focus_widget == basep->hidebutton_n ||
			    GTK_WINDOW (basep)->focus_widget == basep->hidebutton_s)
				set_focus = TRUE;
		}

		gtk_widget_set_sensitive (basep->panel, FALSE);

		gtk_widget_set_sensitive (basep->hidebutton_w, FALSE);
		gtk_widget_set_sensitive (basep->hidebutton_n, FALSE);
		gtk_widget_set_sensitive (basep->hidebutton_s, FALSE);
		gtk_widget_set_sensitive (basep->hidebutton_e, FALSE);

		set_tip (hb, FALSE);
		gtk_widget_set_sensitive (hb, TRUE);
		if (set_focus)
			gtk_window_set_focus (GTK_WINDOW (basep), hb);
	}
}


static void
basep_widget_state_change (BasePWidget *basep, BasePState old_state)
{
	if (BORDER_IS_WIDGET (basep))
		basep_border_queue_recalc (basep->screen, basep->monitor);

	setup_hidebuttons (basep);
}

static void
basep_widget_real_screen_change (BasePWidget *basep,
				 int          old_screen,
				 int          old_monitor)
{
	if (basep->screen != old_screen) {
		gtk_widget_hide (GTK_WIDGET (basep));
		gtk_window_set_screen (
			GTK_WINDOW (basep),
			panel_screen_from_number (basep->screen));
		gtk_widget_show (GTK_WIDGET (basep));
	}

	basep_border_queue_recalc (old_screen, old_monitor);

	/* this will queue border recalc in the new screen */
	gtk_widget_queue_resize (GTK_WIDGET (basep));
	panels_to_sync = TRUE;

	update_config_screen (basep);
}

/* pos core */

static BasePPosClass *
basep_widget_get_pos_class (BasePWidget *basep) {
	BasePPosClass *klass;

	g_return_val_if_fail (BASEP_IS_WIDGET(basep), NULL);
	g_return_val_if_fail (BASEP_IS_POS(basep->pos), NULL);

	klass = BASEP_POS_GET_CLASS(basep);

	g_return_val_if_fail (BASEP_IS_POS_CLASS (klass), NULL);

	return klass;
}

GType
basep_pos_get_type (void)					
{
	static GType object_type = 0;
	if (object_type == 0) {
		static const GTypeInfo object_info = {
		    sizeof (BasePPosClass),
		    (GBaseInitFunc)         NULL,
		    (GBaseFinalizeFunc)     NULL,
		    (GClassInitFunc)        basep_pos_class_init,
		    NULL,                   /* class_finalize */
		    NULL,                   /* class_data */
		    sizeof (BasePPos),
		    0,                      /* n_preallocs */
		    (GInstanceInitFunc)     basep_pos_instance_init 
		};
		object_type = g_type_register_static
		    (G_TYPE_OBJECT, "BasePPos", &object_info, 0);
		basep_pos_parent_class = g_type_class_ref (G_TYPE_OBJECT);
	}
	return object_type;
}

static void
basep_pos_get_hide_size (BasePWidget *basep, 
			 PanelOrient hide_orient,
			 int *w, int *h)
{
	switch (hide_orient) {
	case PANEL_ORIENT_UP:
	case PANEL_ORIENT_DOWN:
		*h = (basep->state == BASEP_AUTO_HIDDEN)
			? global_config.minimized_size
			: get_requisition_height (basep->hidebutton_n);
		break;
	case PANEL_ORIENT_RIGHT:
	case PANEL_ORIENT_LEFT:
		*w = (basep->state == BASEP_AUTO_HIDDEN)
			? global_config.minimized_size
			: get_requisition_width (basep->hidebutton_e);
		break;
	}
	*w = MAX (*w, 1);
	*h = MAX (*h, 1);
}

static void
basep_pos_get_hide_pos (BasePWidget *basep,
			PanelOrient hide_orient,
			int *x, int *y,
			int w, int h)
{
	switch (hide_orient) {
	case PANEL_ORIENT_UP:
	case PANEL_ORIENT_LEFT:
		break;
	case PANEL_ORIENT_RIGHT:
		*x += w - ((basep->state == BASEP_AUTO_HIDDEN)
			   ? global_config.minimized_size 
			   : get_requisition_width (basep->hidebutton_w));
		break;
	case PANEL_ORIENT_DOWN:
		*y += h - ((basep->state == BASEP_AUTO_HIDDEN)
			   ? global_config.minimized_size
			   : get_requisition_height (basep->hidebutton_s));
		break;
	}
}
		
static void
basep_pos_class_init (BasePPosClass *klass)
{
	basep_pos_parent_class = g_type_class_ref (gtk_object_get_type ());

	klass->get_hide_size = basep_pos_get_hide_size;
	klass->get_hide_pos = basep_pos_get_hide_pos;
}	

/* nothing to see here... */
static void
basep_pos_instance_init (BasePPos *pos)
{
	return;
}

static int
basep_widget_focus_in_event (GtkWidget     *widget,
			     GdkEventFocus *event)
{
#if 0
	BasePWidget *basep = BASEP_WIDGET (widget);

        /* FIXME: #89180 - we should show explicitly hidden
	 *	  panels here.
	 * 
	 *        When we have focus and hit ctrl-alt-tab with
         *        Metacity, it does a grab and we a FocusChange
         *        event because gdk doesn't ignore NotifyGrab
         *        FocusChange events. See #ifdefed out case
         *        in gdkevents-x11.c. This is detailed in #89179,
	 *        which blocks #89180.
         */
        else if (basep->state == BASEP_HIDDEN_LEFT ||
                 basep->state == BASEP_HIDDEN_RIGHT)
                basep_widget_explicit_show (basep);
#endif

	return GTK_WIDGET_CLASS (basep_widget_parent_class)->focus_in_event (widget, event);
}

static BasePWidget *
find_non_drawer_parent_panel (BasePWidget *basep)
{
	BasePWidget *retval;
	Drawer      *drawer;

	g_assert (DRAWER_IS_WIDGET (basep));

	drawer = drawer_widget_get_drawer (DRAWER_WIDGET (basep));

	retval = BASEP_WIDGET (PANEL_WIDGET (drawer->button->parent)->panel_parent);

	if (DRAWER_IS_WIDGET (retval))
		retval = find_non_drawer_parent_panel (retval);

	return retval;
}

static gboolean
basep_leave_notify (GtkWidget *widget,
		    GdkEventCrossing *event)
{
	BasePWidget *basep = BASEP_WIDGET (widget);

	if (event->detail == GDK_NOTIFY_INFERIOR)
		return FALSE;

	if (DRAWER_IS_WIDGET (basep))
		basep = find_non_drawer_parent_panel (basep);
	
	basep_widget_queue_autohide (basep);

	if (GTK_WIDGET_CLASS (basep_widget_parent_class)->leave_notify_event)
		return GTK_WIDGET_CLASS (basep_widget_parent_class)->leave_notify_event (widget, event);
	else
		return FALSE;
}

static gboolean
basep_enter_notify (GtkWidget *widget,
		    GdkEventCrossing *event)
{
	BasePWidget *basep = BASEP_WIDGET (widget);

	if (basep->state == BASEP_AUTO_HIDDEN &&
	    event->detail != GDK_NOTIFY_INFERIOR) {

		g_assert (basep->mode == BASEP_AUTO_HIDE);

		if (basep->leave_notify_timer_tag != 0) {
			g_source_remove (basep->leave_notify_timer_tag);
			basep->leave_notify_timer_tag = 0;
		}

		basep_widget_queue_autoshow (basep);
	}  

	if (GTK_WIDGET_CLASS (basep_widget_parent_class)->enter_notify_event)
		return GTK_WIDGET_CLASS (basep_widget_parent_class)->enter_notify_event (widget, event);
	else
		return FALSE;
}

void
basep_widget_get_position (BasePWidget *basep, PanelOrient hide_orient,
			   int *x, int *y, int w, int h)
{
	*x = *y = 0;
	switch(hide_orient) {
	case PANEL_ORIENT_UP:
		if(h < basep->shown_alloc.height)
			*y = h - basep->shown_alloc.height;
		break;
	case PANEL_ORIENT_LEFT:
		if(w < basep->shown_alloc.width)
			*x = w - basep->shown_alloc.width;
		break;
	default:
		break;
	}
}

static void
basep_widget_set_ebox_orient(BasePWidget *basep,
			     PanelOrient hide_orient)
{
	XSetWindowAttributes xattributes;

	switch(hide_orient) {
	case PANEL_ORIENT_UP:
	case PANEL_ORIENT_LEFT:
		xattributes.win_gravity = SouthEastGravity;
		break;
	case PANEL_ORIENT_DOWN:
	case PANEL_ORIENT_RIGHT:
	default:
		xattributes.win_gravity = NorthWestGravity;
		break;
	}

	XChangeWindowAttributes (GDK_WINDOW_XDISPLAY(basep->ebox->window),
				 GDK_WINDOW_XWINDOW(basep->ebox->window),
				 CWWinGravity,  &xattributes);
	
}

static int
move_step(int src, int dest, long start_time, long end_time, long cur_time)
{
	double n, d, percentage;
	
	if(src == dest)
		return dest;

	n = cur_time-start_time;
	d = end_time-start_time;

	if(n<d) {
		/*blah blah blah just a simple function to make the
		  movement more "sin" like ... we run it twice to
		  pronounce make it more pronounced*/
		percentage = sin(M_PI*(n/d)-M_PI/2)/2+0.5;
		percentage = sin(M_PI*percentage-M_PI/2)/2+0.5;
		if(percentage<0.0)
			percentage = 0.0;
		else if(percentage>1.0)
			percentage = 1.0;
	} else
		percentage = 1.0;

	return  src + ((dest - src)*percentage);
}

void
basep_widget_do_hiding(BasePWidget *basep, PanelOrient hide_orient,
		       int leftover, PanelSpeed animation_step)
{
	GtkWidget *wid;
	int ox,oy,ow,oh;
	int x,y,w,h;
	int dx,dy,dw,dh;
	int diff;
	int step;
	
	g_return_if_fail(BASEP_IS_WIDGET(basep));

	switch (animation_step) {
	case PANEL_SPEED_SLOW:
		step = PANEL_SLOW_STEP_SIZE;
		break;
	case PANEL_SPEED_MEDIUM:
		step = PANEL_MEDIUM_STEP_SIZE;
		break;
	case PANEL_SPEED_FAST:
		step = PANEL_FAST_STEP_SIZE;
		break;
	default:
		step = PANEL_MEDIUM_STEP_SIZE; 
		break;
	}

	wid = GTK_WIDGET(basep);
	
	ox = x = basep->shown_alloc.x;
	oy = y = basep->shown_alloc.y;
	ow = w = basep->shown_alloc.width;
	oh = h = basep->shown_alloc.height;
	
	switch(hide_orient) {
	case PANEL_ORIENT_UP:
		diff = h-leftover;
		dx = x;
		dy = y;
		dw = w;
		dh = leftover;
		break;
	case PANEL_ORIENT_DOWN:
		diff = h-leftover;
		dx = x;
		dy = y+h-leftover;
		dw = w;
		dh = leftover;
		break;
	case PANEL_ORIENT_LEFT:
		diff = w-leftover;
		dx = x;
		dy = y;
		dw = leftover;
		dh = h;
		break;
	case PANEL_ORIENT_RIGHT:
		diff = w-leftover;
		dx = x+w-leftover;
		dy = y;
		dw = leftover;
		dh = h;
		break;
	default:
		/*fix warning*/ dx = dy = dw = dh = 0;
		diff = 1;
		g_assert_not_reached();
		break;
	}

	if (global_config.enable_animations && step != 0) {
		GTimeVal tval;
		long start_secs;
		long start_time;
		long end_time;
		long cur_time;

		g_get_current_time(&tval);
		
		start_secs = tval.tv_sec;
		start_time = tval.tv_usec;
		
		end_time = start_time +
			(diff/1000.0)*200*(10001-(step*step));

		basep_widget_set_ebox_orient(basep, hide_orient);

		while(x != dx ||
		      y != dy ||
		      w != dw ||
		      h != dh) {
			g_get_current_time(&tval);
			
			cur_time = ((tval.tv_sec-start_secs)*1000000) +
				tval.tv_usec;

			x = move_step(ox,dx,start_time,end_time,cur_time);
			y = move_step(oy,dy,start_time,end_time,cur_time);
			w = move_step(ow,dw,start_time,end_time,cur_time);
			h = move_step(oh,dh,start_time,end_time,cur_time);
			xstuff_set_pos_size (wid->window, x, y, w, h);
			g_usleep (1000);
		}

		xstuff_set_pos_size (wid->window,
				     dx, dy, dw, dh);
		basep_widget_set_ebox_orient(basep, -1);
	}
	
	gtk_widget_queue_resize(wid);
	gtk_widget_queue_draw (basep->table);
}

void
basep_widget_do_showing(BasePWidget *basep, PanelOrient hide_orient,
			int leftover, PanelSpeed animation_step)
{
	GtkWidget *wid;
	int x,y, dx,dy, ox,oy;
	int w,h, dw,dh, ow,oh;
	int diff;
	int step;

	g_return_if_fail(BASEP_IS_WIDGET(basep));
	
	switch (animation_step) {
		case PANEL_SPEED_MEDIUM:
				step = PANEL_MEDIUM_STEP_SIZE;
				break;
		case PANEL_SPEED_SLOW:
				step = PANEL_SLOW_STEP_SIZE;
				break;
		case PANEL_SPEED_FAST:
				step = PANEL_FAST_STEP_SIZE;
				break;
		default:
				step = PANEL_MEDIUM_STEP_SIZE;
	}

	wid = GTK_WIDGET(basep);
	
	ox = dx = x = basep->shown_alloc.x;
	oy = dy = y = basep->shown_alloc.y;
	dw = basep->shown_alloc.width;
	dh = basep->shown_alloc.height;
			      
	switch(hide_orient) {
	case PANEL_ORIENT_UP:
		ow = w = dw;
		oh = h = leftover;
		diff = dh-leftover;
		break;
	case PANEL_ORIENT_DOWN:
		oy = y + dh - leftover;
		ow = w = dw;
		oh = h = leftover;
		diff = dh-leftover;
		break;
	case PANEL_ORIENT_LEFT:
		ow = w = leftover;
		oh = h = dh;
		diff = dw-leftover;
		break;
	case PANEL_ORIENT_RIGHT:
		ox = x + dw - leftover;
		ow = w = leftover;
		oh = h = dh;
		diff = dw-leftover;
		break;
	default:
		/*fix warning*/ dx = dy = ow = oh = w = h = 0;
		diff = 1;
		g_assert_not_reached();
		break;
	}
	
	if (global_config.enable_animations && step != 0) {
		int i;
		GTimeVal tval;
		long start_secs;
		long start_time;
		long end_time;
		long cur_time;

		g_get_current_time(&tval);
		
		start_secs = tval.tv_sec;
		start_time = tval.tv_usec;
		
		end_time = start_time +
			(diff/1000.0)*200*(10001-(step*step));
		
		basep_widget_set_ebox_orient(basep, hide_orient);
		gdk_window_show(wid->window);
		xstuff_set_pos_size (wid->window,
				     ox, oy, ow, oh);

		gtk_widget_show_now(wid);

		gdk_window_show(wid->window);
		i = 0;
		while(x != dx ||
		      y != dy ||
		      w != dw ||
		      h != dh) {
			g_get_current_time(&tval);
			
			cur_time = ((tval.tv_sec-start_secs)*1000000) +
				tval.tv_usec;
			
			x = move_step(ox,dx,start_time,end_time,cur_time);
			y = move_step(oy,dy,start_time,end_time,cur_time);
			w = move_step(ow,dw,start_time,end_time,cur_time);
			h = move_step(oh,dh,start_time,end_time,cur_time);
			xstuff_set_pos_size (wid->window, x, y, w, h);
			
			/*drawing the entire table flickers, so don't
			  do it often*/
			/* FIXME: does it still flicker? */
			/* FIXME: sometimes this updates, sometimes it doesn't
			 * see bug #81654 */
			if (i++ % 10) {
				/* FIXME: is invalidation needed, and if so it should only be
				 * partial */
				gdk_window_invalidate_rect (basep->panel->window,
							    NULL, TRUE);
				gdk_window_process_updates (basep->panel->window, TRUE);
			} else {
				gdk_window_invalidate_rect (GTK_WIDGET (basep)->window,
							    NULL, TRUE);
				gdk_window_process_updates (GTK_WIDGET (basep)->window, TRUE);
			}
			gdk_flush();
			g_usleep (1000);
		}

		xstuff_set_pos_size (wid->window,
				     dx, dy, dw, dh);

		basep_widget_set_ebox_orient(basep, -1);
	}
	
	gtk_widget_queue_draw (basep->table);
	gtk_widget_queue_resize (wid);
}


static GtkWidget *
make_hidebutton (BasePWidget *basep,
		 const char  *arrow_stock_id,
		 gboolean     horizontal)
{
	GtkWidget *button;
	GtkWidget *pixmap;

	button = gtk_button_new ();
	GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_DEFAULT);

	if (horizontal)
		gtk_widget_set_size_request (button, -1, PANEL_MINIMUM_WIDTH);
	else
		gtk_widget_set_size_request (button, PANEL_MINIMUM_WIDTH, -1);

	pixmap = gtk_image_new_from_stock (
			arrow_stock_id, panel_button_icon_get_size ());
	gtk_widget_show (pixmap);

	gtk_container_add (GTK_CONTAINER (button), pixmap);
	g_object_set_data (G_OBJECT (button), "gnome_disable_sound_events",
			   GINT_TO_POINTER  (TRUE));

	set_tip (button, TRUE);

	return button;
}

static void
basep_widget_destroy (GtkObject *o)
{
	BasePWidget *basep = BASEP_WIDGET (o);
        /* check if there's a timeout set, and delete it if 
	 * there was */
	if (basep->leave_notify_timer_tag != 0)
		g_source_remove (basep->leave_notify_timer_tag);
	basep->leave_notify_timer_tag = 0;

	if (basep->pos) {
		if (BORDER_IS_WIDGET (basep))
			basep_border_queue_recalc (
				basep->screen, basep->monitor);
		g_object_unref (basep->pos);
	}
	basep->pos = NULL;

	if (GTK_OBJECT_CLASS (basep_widget_parent_class)->destroy)
		GTK_OBJECT_CLASS (basep_widget_parent_class)->destroy (o);

}	

static void
reparent_button_widgets(GtkWidget *w, gpointer data)
{
	GdkWindow *newwin = data;
	if(BUTTON_IS_WIDGET (w)) {
		GtkButton *button = GTK_BUTTON (w);
		/* we can just reparent them all to 0,0 as the next thing
		 * that will happen is a queue_resize and on size allocate
		 * they will be put into their proper place */
		gdk_window_reparent(button->event_window, newwin, 0, 0);
	}
}

void
basep_widget_redo_window(BasePWidget *basep)
{
	GtkWindow *window;
	GtkWidget *widget;
	GdkWindowAttr attributes;
	gint attributes_mask;
	GdkWindow *oldwin;
	GdkWindow *newwin;
	gboolean comp;

	comp = xstuff_is_compliant_wm();
	if(comp == basep->compliant_wm)
		return;

	window = GTK_WINDOW(basep);
	widget = GTK_WIDGET(basep);

	basep->compliant_wm = comp;
	if(basep->compliant_wm) {
		window->type = GTK_WINDOW_TOPLEVEL;
		attributes.window_type = GDK_WINDOW_TOPLEVEL;
	} else {
		window->type = GTK_WINDOW_POPUP;
		attributes.window_type = GDK_WINDOW_TEMP;
	}

	if(!widget->window)
		return;

	/* this is mostly copied from gtkwindow.c realize method */
	attributes.title = window->title;
	attributes.wmclass_name = window->wmclass_name;
	attributes.wmclass_class = window->wmclass_class;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= (GDK_EXPOSURE_MASK |
				  GDK_KEY_PRESS_MASK |
				  GDK_ENTER_NOTIFY_MASK |
				  GDK_LEAVE_NOTIFY_MASK |
				  GDK_FOCUS_CHANGE_MASK |
				  GDK_STRUCTURE_MASK);

	attributes_mask = GDK_WA_VISUAL | GDK_WA_COLORMAP;
	attributes_mask |= (window->title ? GDK_WA_TITLE : 0);
	attributes_mask |= (window->wmclass_name ? GDK_WA_WMCLASS : 0);
   
	oldwin = widget->window;

	newwin = gdk_window_new(NULL, &attributes, attributes_mask);
	gdk_window_set_user_data(newwin, window);

	xstuff_set_no_group_and_no_input (newwin);

	/* reparent our main panel window */
	gdk_window_reparent(basep->ebox->window, newwin, 0, 0);
	/* reparent all the base event windows as they are also children of
	 * the basep */
	gtk_container_foreach(GTK_CONTAINER(basep->panel),
			      reparent_button_widgets,
			      newwin);


	widget->window = newwin;

	gdk_window_set_user_data(oldwin, NULL);
	gdk_window_destroy(oldwin);

	widget->style = gtk_style_attach(widget->style, widget->window);
	gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	gtk_widget_queue_resize(widget);

	basep_widget_update_winhints (basep);

	if (basep->strut_left != 0 ||
	    basep->strut_right != 0 ||
	    basep->strut_top != 0 ||
	    basep->strut_bottom != 0) {
		xstuff_set_wmspec_strut (widget->window,
					 basep->strut_left,
					 basep->strut_right,
					 basep->strut_top,
					 basep->strut_bottom);
	}

	gtk_drag_dest_set (widget, 0, NULL, 0, 0);

	gtk_widget_map(widget);
}


static void
basep_widget_instance_init (BasePWidget *basep)
{
	basep->screen = 0;
	basep->monitor = 0;

	/*if we set the gnomewm hints it will have to be changed to TOPLEVEL*/
	basep->compliant_wm = xstuff_is_compliant_wm();
	if(basep->compliant_wm)
		GTK_WINDOW(basep)->type = GTK_WINDOW_TOPLEVEL;
	else
		GTK_WINDOW(basep)->type = GTK_WINDOW_POPUP;

	GTK_WINDOW(basep)->allow_shrink = TRUE;
	GTK_WINDOW(basep)->allow_grow = TRUE;

	/*don't let us close the window*/                                       
	
	g_signal_connect(G_OBJECT(basep),"delete_event",                    
			 G_CALLBACK (gtk_true), NULL);                     

	basep->shown_alloc.x = basep->shown_alloc.y =
		basep->shown_alloc.width = basep->shown_alloc.height = 0;
	
	/*this makes the popup "pop down" once the button is released*/
	gtk_widget_set_events(GTK_WIDGET(basep),
			      gtk_widget_get_events(GTK_WIDGET(basep)) |
			      GDK_BUTTON_RELEASE_MASK);

	basep->ebox = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(basep),basep->ebox);
	gtk_widget_show(basep->ebox);

	basep->table = gtk_table_new(3,3,FALSE);
	gtk_widget_set_direction (basep->table, GTK_TEXT_DIR_LTR);
	gtk_container_add(GTK_CONTAINER(basep->ebox),basep->table);
	gtk_widget_show(basep->table);

	basep->frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(basep->frame),GTK_SHADOW_OUT);

	gtk_table_attach(GTK_TABLE(basep->table),basep->frame,1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);

	basep->innerebox = gtk_event_box_new();

	gtk_table_attach(GTK_TABLE(basep->table),basep->innerebox,1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);
	
	g_object_set (G_OBJECT (basep),
		      "mode", BASEP_EXPLICIT_HIDE,
		      "state", BASEP_SHOWN,
		      "hidebuttons_enabled", TRUE,
		      "hidebutton_pixmaps_enabled", TRUE,
		      NULL);

	basep->leave_notify_timer_tag = 0;
	basep->autohide_inhibit = FALSE;
	basep->drawers_open = 0;
}

static void
show_hidebutton_pixmap (GtkWidget *hidebutton, gboolean show)
{
	GtkWidget *pixmap = GTK_BIN (hidebutton)->child;

	if (pixmap == NULL)
		return;

	if (show)
		gtk_widget_show (pixmap);
	else
		gtk_widget_hide (pixmap);
}

static void
basep_widget_show_hidebutton_pixmaps (BasePWidget *basep)
{
	gboolean show = basep->hidebutton_pixmaps_enabled;
	show_hidebutton_pixmap (basep->hidebutton_n, show);
	show_hidebutton_pixmap (basep->hidebutton_e, show);
	show_hidebutton_pixmap (basep->hidebutton_w, show);
	show_hidebutton_pixmap (basep->hidebutton_s, show);
}

void
basep_widget_update_winhints (BasePWidget *basep)
{
	GtkWidget *w = GTK_WIDGET (basep);

	gtk_window_set_decorated (GTK_WINDOW (w), FALSE);
	gtk_window_stick (GTK_WINDOW (w));

	xstuff_set_wmspec_dock_hints (w->window,
				      (basep->mode == BASEP_AUTO_HIDE));

	/* FIXME: non-compliance should be tested! */

	if (BASEP_POS_GET_CLASS (basep)->update_winhints != NULL)
		BASEP_POS_GET_CLASS (basep)->update_winhints (basep);
	
}

void
basep_update_frame (BasePWidget *basep)
{
	PanelWidget *panel;
	gboolean     hide_frame = FALSE;

	g_return_if_fail (PANEL_IS_WIDGET (basep->panel));

	panel = PANEL_WIDGET (basep->panel);

	if (panel->background.type == PANEL_BACK_IMAGE ||
	    panel->background.type == PANEL_BACK_COLOR)
		hide_frame = TRUE;

	if (hide_frame && GTK_WIDGET_VISIBLE (basep->frame)) {
		gtk_widget_show (basep->innerebox);
		gtk_widget_reparent (GTK_WIDGET (panel), basep->innerebox);
		gtk_widget_hide (basep->frame);
	} else if (!hide_frame && !GTK_WIDGET_VISIBLE (basep->frame)) {
		gtk_widget_show (basep->frame);
		gtk_widget_reparent (GTK_WIDGET (panel), basep->frame);
		gtk_widget_hide (basep->innerebox);
	}
}

static void
basep_back_change (PanelWidget *panel, gpointer data)
{
	BasePWidget *basep = BASEP_WIDGET (data);
	basep_update_frame (basep);

	panel_set_frame_colors (panel,
				basep->frame,
				basep->hidebutton_n,
				basep->hidebutton_e,
				basep->hidebutton_w,
				basep->hidebutton_s);
}

static void
basep_orient_change (PanelWidget *panel, gpointer data)
{
	BasePWidget *basep = BASEP_WIDGET (data);
	setup_hidebuttons (basep);
}

static void
basep_style_set (GtkWidget *widget, GtkStyle *previous_style)
{
	BasePWidget *basep;
	PanelWidget *panel;

	g_return_if_fail (BASEP_IS_WIDGET (widget));

	basep = BASEP_WIDGET (widget);

	g_return_if_fail (basep->panel != NULL);
	g_return_if_fail (PANEL_IS_WIDGET (basep->panel));

	panel = PANEL_WIDGET (basep->panel);

	if (GTK_WIDGET_CLASS (basep_widget_parent_class)->style_set)
		GTK_WIDGET_CLASS (basep_widget_parent_class)->style_set (widget, previous_style);

	panel_set_frame_colors (panel,
				basep->frame,
				basep->hidebutton_n,
				basep->hidebutton_e,
				basep->hidebutton_w,
				basep->hidebutton_s);
}

static void
basep_widget_north_clicked (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep = data;
	BasePPosClass *klass =
		basep_widget_get_pos_class (basep);

	gtk_widget_set_state (widget, GTK_STATE_NORMAL);
	gtk_widget_queue_draw (widget);

	if (klass && klass->north_clicked)
		klass->north_clicked(basep);
}

static void
basep_widget_south_clicked (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep = data;
	BasePPosClass *klass =
		basep_widget_get_pos_class (basep);

	gtk_widget_set_state (widget, GTK_STATE_NORMAL);
	gtk_widget_queue_draw (widget);
	
	if (klass && klass->south_clicked)
		klass->south_clicked(basep);
}

static void
basep_widget_east_clicked (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep = data;
	BasePPosClass *klass =
		basep_widget_get_pos_class (basep);

	gtk_widget_set_state (widget, GTK_STATE_NORMAL);
	gtk_widget_queue_draw (widget);
	
	if (klass && klass->east_clicked)
		klass->east_clicked(basep);
}

static void
basep_widget_west_clicked (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep = data;
	BasePPosClass *klass =
		basep_widget_get_pos_class (basep);

	gtk_widget_set_state (widget, GTK_STATE_NORMAL);
	gtk_widget_queue_draw (widget);
	
	if (klass && klass->west_clicked)
		klass->west_clicked(basep);
}

GtkWidget*
basep_widget_construct (const char *panel_id,
			BasePWidget *basep,
			gboolean packed,
			gboolean reverse_arrows,
			int screen,
			int monitor, 
			GtkOrientation orient,
			int sz,
			BasePMode mode,
			BasePState state,
			gboolean hidebuttons_enabled,
			gboolean hidebutton_pixmaps_enabled,
			PanelBackgroundType back_type,
			const char *back_pixmap,
			gboolean fit_pixmap_bg,
			gboolean stretch_pixmap_bg,
			gboolean rotate_pixmap_bg,
			PanelColor *back_color)
{
	BasePPosClass *klass = basep_widget_get_pos_class (basep);
	GList         *focus_chain = NULL;
	int            x = 0, y = 0;

	gtk_window_set_screen (GTK_WINDOW (basep),
			       panel_screen_from_number (screen));

	basep->panel = panel_widget_new(panel_id,
					packed,
					orient,
					sz,
					back_type,
					back_pixmap,
					fit_pixmap_bg,
					stretch_pixmap_bg,
					rotate_pixmap_bg,
					back_color);

	g_signal_connect_after (G_OBJECT (basep->panel), "orient_change",
				G_CALLBACK (basep_orient_change),
				basep);

	g_signal_connect_after (G_OBJECT (basep->panel), "back_change",
				G_CALLBACK (basep_back_change),
				basep);

	PANEL_WIDGET(basep->panel)->panel_parent = GTK_WIDGET(basep);
	PANEL_WIDGET(basep->panel)->drop_widget = GTK_WIDGET(basep);

	gtk_widget_show(basep->panel);

	if (back_type != PANEL_BACK_IMAGE &&
	    back_type != PANEL_BACK_COLOR) {
		gtk_widget_show (basep->frame);
		gtk_container_add (GTK_CONTAINER (basep->frame), basep->panel);
	} else {
		gtk_widget_show (basep->innerebox);
		gtk_container_add (GTK_CONTAINER (basep->innerebox), basep->panel);
	}

	/*we add all the hide buttons to the table here*/
	/*WEST*/
	basep->hidebutton_w = make_hidebutton(basep,
					      reverse_arrows?
					      PANEL_STOCK_ARROW_RIGHT:
					      PANEL_STOCK_ARROW_LEFT,
					      TRUE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_w,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	g_signal_connect (G_OBJECT (basep->hidebutton_w), "clicked",
			  G_CALLBACK (basep_widget_west_clicked),
			  basep);
	/*NORTH*/
	basep->hidebutton_n = make_hidebutton(basep,
					      reverse_arrows?
					      PANEL_STOCK_ARROW_DOWN:
					      PANEL_STOCK_ARROW_UP,  
					      FALSE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_n,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);
	g_signal_connect (G_OBJECT (basep->hidebutton_n), "clicked",
			  G_CALLBACK (basep_widget_north_clicked),
			    basep);
	/*EAST*/
	basep->hidebutton_e = make_hidebutton(basep,
					      reverse_arrows?
					      PANEL_STOCK_ARROW_LEFT: 
					      PANEL_STOCK_ARROW_RIGHT, 
					      TRUE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_e,
			 2,3,1,2,GTK_FILL,GTK_FILL,0,0);
	g_signal_connect (G_OBJECT (basep->hidebutton_e), "clicked",
			  G_CALLBACK (basep_widget_east_clicked),
			    basep);
	/*SOUTH*/
	basep->hidebutton_s = make_hidebutton (basep,
					      reverse_arrows ?
					      PANEL_STOCK_ARROW_UP: 
					      PANEL_STOCK_ARROW_DOWN, 
					      FALSE);
	gtk_table_attach(GTK_TABLE(basep->table), basep->hidebutton_s,
			 1, 2, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect (G_OBJECT (basep->hidebutton_s), "clicked",
			  G_CALLBACK (basep_widget_south_clicked),
			    basep);

	basep->screen = screen;
	basep->monitor = monitor;

	basep->hidebuttons_enabled = hidebuttons_enabled;
	basep->hidebutton_pixmaps_enabled = hidebutton_pixmaps_enabled;

	/*
	 * Set focus chain so that focus goes initially to panel.
	 */
	focus_chain = g_list_prepend (focus_chain, basep->hidebutton_s);
	focus_chain = g_list_prepend (focus_chain, basep->hidebutton_e);
	focus_chain = g_list_prepend (focus_chain, basep->hidebutton_n);
	focus_chain = g_list_prepend (focus_chain, basep->hidebutton_w);
	focus_chain = g_list_prepend (focus_chain, basep->frame);
	focus_chain = g_list_prepend (focus_chain, basep->innerebox);
	gtk_container_set_focus_chain (GTK_CONTAINER (basep->table), focus_chain);
	g_list_free (focus_chain);

	basep_widget_set_hidebuttons (basep);
	basep_widget_show_hidebutton_pixmaps (basep);

	basep->mode = mode;
	basep->state = state;

	g_object_ref (basep->pos);
	basep->pos->basep = basep;

	if (state == BASEP_AUTO_HIDDEN &&
	    mode != BASEP_AUTO_HIDE)
		basep->state = BASEP_SHOWN;

	/* setup the hide buttons stuff according to state */
	setup_hidebuttons (basep);
	       
	if (klass->get_pos)
		klass->get_pos (basep, &x, &y, 
				PANEL_MINIMUM_WIDTH,
				PANEL_MINIMUM_WIDTH);
	gtk_window_move (GTK_WINDOW (basep), x, y);
	
	return GTK_WIDGET (basep);
}

void
basep_widget_change_params (BasePWidget *basep,
			    int screen,
			    int monitor,
			    GtkOrientation orient,
			    int sz,
			    BasePMode mode,
			    BasePState state,
			    gboolean hidebuttons_enabled,
			    gboolean hidebutton_pixmaps_enabled,
			    PanelBackgroundType back_type,
			    const char *pixmap_name,
			    gboolean fit_pixmap_bg,
			    gboolean stretch_pixmap_bg,
			    gboolean rotate_pixmap_bg,
			    PanelColor *back_color)
{
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(basep)));

	if (PANEL_WIDGET (basep->panel)->orient != orient)
		basep->request_cube = TRUE;
	
	basep->hidebuttons_enabled = hidebuttons_enabled;
	basep->hidebutton_pixmaps_enabled = hidebutton_pixmaps_enabled;

	if (state == BASEP_AUTO_HIDDEN &&
	    mode != BASEP_AUTO_HIDE)
		state = BASEP_SHOWN;

	if (mode != basep->mode) {
		BasePMode old_mode = basep->mode;
		basep->mode = mode;
		if (mode == BASEP_AUTO_HIDE)
			basep_widget_queue_autohide (basep);
		g_signal_emit (G_OBJECT(basep),
			       basep_widget_signals[MODE_CHANGE_SIGNAL],
			       0, old_mode);
	}
	
	if (state != basep->state) {
		BasePState old_state = basep->state;
		basep->state = state;
		if (state != BASEP_AUTO_HIDDEN)
			basep_widget_autoshow (basep);
		g_signal_emit (G_OBJECT(basep),
			       basep_widget_signals[STATE_CHANGE_SIGNAL],
			       0, old_state);
		panels_to_sync = TRUE;
	}

	panel_widget_change_params(PANEL_WIDGET(basep->panel),
				   orient,
				   sz,
				   back_type,
				   pixmap_name,
				   fit_pixmap_bg,
				   stretch_pixmap_bg,
				   rotate_pixmap_bg,
				   back_color);

	basep_widget_set_hidebuttons (basep);
	basep_widget_show_hidebutton_pixmaps (basep);

	basep_widget_screen_change (basep, screen, monitor);

	gtk_widget_queue_resize (GTK_WIDGET (basep));
}

#if FIXME 
gboolean
basep_widget_convert_to (BasePWidget *basep,
			 PanelType type)
{
	BasePPosClass *klass =
		basep_widget_get_pos_class (basep);
	BasePPos *old_pos, *new_pos;
	gint16 x=0, y=0;
	gboolean temp_keep;

	g_return_val_if_fail (BASEP_IS_WIDGET(basep), FALSE);

	g_return_val_if_fail (create_panel_type[type], FALSE);

	basep_widget_get_pos(basep, &x, &y);

	old_pos = basep->pos;
	new_pos = gtk_type_new (create_panel_type[type] ());

	if (!new_pos)
		return FALSE;

	basep->pos = new_pos;
	new_pos->basep = basep;

	g_object_unref (G_OBJECT (old_pos));

	klass = basep_widget_get_pos_class (basep);
	if (klass->pre_convert_hook)
		klass->pre_convert_hook (basep);

	temp_keep = basep->keep_in_screen;
	basep->keep_in_screen = FALSE;
	gtk_widget_set_uposition (GTK_WIDGET (basep), -100, -100);
	gdk_flush ();
	basep_widget_set_pos (basep, -100, -100);
	gdk_flush ();
	g_print ("-------------------------------------\n");
	basep_widget_set_pos (basep, x, y);
	basep->keep_in_screen = temp_keep;
	g_signal_emit (G_OBJECT(basep),
		       basep_widget_signals[TYPE_CHANGE_SIGNAL],
		       0, type);

	/*gtk_widget_queue_resize (GTK_WIDGET (basep));*/
	return TRUE;
}
#endif

void
basep_widget_enable_buttons (BasePWidget *basep,
			     gboolean     enabled)
{
	gtk_widget_set_sensitive (basep->hidebutton_n, enabled);
	gtk_widget_set_sensitive (basep->hidebutton_e, enabled);
	gtk_widget_set_sensitive (basep->hidebutton_w, enabled);
	gtk_widget_set_sensitive (basep->hidebutton_s, enabled);
}

void
basep_widget_set_hidebuttons (BasePWidget *basep)
{
	BasePPosClass *klass = basep_widget_get_pos_class (basep);
	if (!basep->hidebuttons_enabled) {
		gtk_widget_hide(basep->hidebutton_n);
		gtk_widget_hide(basep->hidebutton_e);
		gtk_widget_hide(basep->hidebutton_w);
		gtk_widget_hide(basep->hidebutton_s);	

		/* if we removed hidebuttons we need to show ourselves,
		 * except for the drawers case that is */
		if ((basep->state == BASEP_HIDDEN_LEFT ||
		     basep->state == BASEP_HIDDEN_RIGHT) &&
		    ! DRAWER_IS_WIDGET (basep))
			basep_widget_explicit_show (basep);
	} else {
		g_return_if_fail (klass && klass->set_hidebuttons);
		klass->set_hidebuttons(basep);
	}
}

/* FIXME: perhaps we could get rid of the MOVING state, it's kind of
 * useless, isn't it? */
void
basep_widget_explicit_hide (BasePWidget *basep, BasePState state)
{
	BasePState old_state;

	g_assert ( (state == BASEP_HIDDEN_RIGHT) ||
		   (state == BASEP_HIDDEN_LEFT) );

	if ((basep->state != BASEP_SHOWN))
		return;

	if (basep->moving)
		return;

	basep->moving = TRUE;

	old_state = basep->state;
	basep->state = state;

	g_signal_emit (G_OBJECT (basep),
		       basep_widget_signals[STATE_CHANGE_SIGNAL],
		       0, old_state);
	panels_to_sync = TRUE;

	/* if the app did any updating of the interface, flush that for us*/
	gdk_flush();

	if (GTK_WIDGET_REALIZED(GTK_WIDGET(basep))) {
		BasePPosClass *klass = basep_widget_get_pos_class (basep);
		PanelOrient hide_orient;
		int w, h, size;

		hide_orient = klass->get_hide_orient (basep);
		basep_widget_get_size (basep, &w, &h);
		klass->get_hide_size (basep,
				      hide_orient,
				      &w, &h);

		size = (hide_orient == PANEL_ORIENT_UP ||
			hide_orient == PANEL_ORIENT_DOWN) ?
			h : w;
		
		basep_widget_update_winhints (basep);
		basep_widget_do_hiding (basep, hide_orient,
					size, 
					global_config.animation_speed);
	}

	/*
	 * If the button being hidden has focus move it to the button which
	 * remains shown.
	 */
	if (state == BASEP_HIDDEN_RIGHT) {
		if (gtk_widget_is_focus (basep->hidebutton_e))
			gtk_widget_grab_focus (basep->hidebutton_w);
		else if (gtk_widget_is_focus (basep->hidebutton_s))
			gtk_widget_grab_focus (basep->hidebutton_n);
	} else if (state == BASEP_HIDDEN_LEFT) {
		if (gtk_widget_is_focus (basep->hidebutton_w))
			gtk_widget_grab_focus (basep->hidebutton_e);
		else if (gtk_widget_is_focus (basep->hidebutton_n))
			gtk_widget_grab_focus (basep->hidebutton_s);
	}

	basep->moving = FALSE;

	basep_widget_update_winhints (basep);
}

void
basep_widget_explicit_show (BasePWidget *basep)
{
	BasePState old_state;

	if ( (basep->state != BASEP_HIDDEN_LEFT &&
	      basep->state != BASEP_HIDDEN_RIGHT))
		return;
 
	if (basep->moving)
		return;

	basep->moving = TRUE;

	if (GTK_WIDGET_REALIZED(GTK_WIDGET(basep))) {
		BasePPosClass *klass = basep_widget_get_pos_class (basep);
		PanelOrient hide_orient;
		int w, h, size;

		hide_orient = klass->get_hide_orient (basep);
		basep_widget_get_size (basep, &w, &h);
		klass->get_hide_size (basep,
				      hide_orient,
				      &w, &h);

		size = (hide_orient == PANEL_ORIENT_UP ||
			hide_orient == PANEL_ORIENT_DOWN) ?
			h : w;

		basep_widget_update_winhints (basep);
		basep_widget_do_showing (basep, hide_orient,
					 size, 
					 global_config.animation_speed);
					 
	}

	basep->moving = FALSE;
	old_state = basep->state;
	basep->state = BASEP_SHOWN;
	basep_widget_update_winhints (basep);

	g_signal_emit (G_OBJECT(basep),
		       basep_widget_signals[STATE_CHANGE_SIGNAL],
		       0, old_state);
	panels_to_sync = TRUE;
}

gboolean
basep_widget_autoshow (gpointer data)
{
	BasePState old_state;
	BasePWidget *basep = data;

	g_return_val_if_fail (BASEP_IS_WIDGET(basep), FALSE);

	if (basep->moving)
		return TRUE;
	
	if ( (basep->mode != BASEP_AUTO_HIDE) ||
	     (basep->state != BASEP_AUTO_HIDDEN))
		return TRUE;

	basep->moving = TRUE;

	if (GTK_WIDGET_REALIZED(basep)) {
		BasePPosClass *klass = basep_widget_get_pos_class (basep);
		PanelOrient hide_orient;
		int w, h, size;

		hide_orient = klass->get_hide_orient (basep);
		basep_widget_get_size (basep, &w, &h);
		klass->get_hide_size (basep, 
				      hide_orient,
				      &w, &h);

		size = (hide_orient == PANEL_ORIENT_UP ||
			hide_orient == PANEL_ORIENT_DOWN) ?
			h : w;

		basep_widget_update_winhints (basep);
		basep_widget_do_showing (basep,
					 hide_orient,
					 size,
					 global_config.animation_speed);
	}
	
	basep->moving = FALSE;

	old_state = basep->state;
	basep->state = BASEP_SHOWN;
	basep_widget_update_winhints (basep);

	g_signal_emit (G_OBJECT(basep),
		       basep_widget_signals[STATE_CHANGE_SIGNAL],
		       0, old_state);

	basep->enter_notify_timer_tag = 0;
	return FALSE;
}

/* FIXME - there's a problem if the show and hide delays are set to 0 */

void
basep_widget_queue_autoshow (BasePWidget *basep)
{
        /* check if there's already a timeout set, and delete it if 
         * there was */

	if (basep->moving)
		return; 

        if (basep->leave_notify_timer_tag != 0) {
                g_source_remove (basep->leave_notify_timer_tag);
                basep->leave_notify_timer_tag = 0;
	}

        if (basep->enter_notify_timer_tag != 0) {
                g_source_remove (basep->enter_notify_timer_tag);
	}

        if ((basep->mode != BASEP_AUTO_HIDE) ||
            (basep->state == BASEP_SHOWN))
                return;

	if (global_config.show_delay <= 0)
		basep->enter_notify_timer_tag =
			g_idle_add (basep_widget_autoshow, basep);
	else
		basep->enter_notify_timer_tag =
			g_timeout_add (global_config.show_delay,
				       basep_widget_autoshow, basep);
}

gboolean
basep_widget_autohide (gpointer data)
{
	BasePState old_state;
	BasePWidget *basep = data;

	g_return_val_if_fail (BASEP_IS_WIDGET(basep), TRUE);

	if (basep->drawers_open > 0)
		return FALSE;

	if (basep->state != BASEP_SHOWN ||
	    basep->mode != BASEP_AUTO_HIDE ||
	    panel_widget_is_cursor (PANEL_WIDGET (basep->panel), 0))
		return FALSE;

	if (basep->autohide_inhibit)
		return TRUE;

	if (basep->moving)
		return TRUE;

	if (panel_applet_in_drag)
		return TRUE;

	if (!gdk_pointer_is_grabbed ()) {
		if (gdk_pointer_grab (gdk_get_default_root_window (), FALSE, 
				      0, NULL, NULL, GDK_CURRENT_TIME)
		    != GrabSuccess) {
			return TRUE;
		} else {
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
		}
	}

	basep->moving = TRUE;
	old_state = basep->state;
	basep->state = BASEP_AUTO_HIDDEN;

	g_signal_emit(G_OBJECT(basep),
		      basep_widget_signals[STATE_CHANGE_SIGNAL],
		      0, old_state);

	/* if the app did any updating of the interface, flush that for us*/
	gdk_flush();

	if (GTK_WIDGET_REALIZED(basep)) {
		BasePPosClass *klass = basep_widget_get_pos_class (basep);
		PanelOrient hide_orient;
		int w, h, size;

		hide_orient = klass->get_hide_orient (basep);
		basep_widget_get_size (basep, &w, &h);
		klass->get_hide_size (basep, 
				      hide_orient,
				      &w, &h);
		size =  (hide_orient == PANEL_ORIENT_UP ||
			 hide_orient == PANEL_ORIENT_DOWN) 
			? h : w;

		basep_widget_update_winhints (basep);
		basep_widget_do_hiding (basep,
					hide_orient,
					size,
					global_config.animation_speed);
	}

	basep->moving = FALSE;

	basep_widget_update_winhints (basep);

	basep->leave_notify_timer_tag = 0;
	return FALSE;
}

void
basep_widget_queue_autohide (BasePWidget *basep)
{
	if (basep->moving)
		return; 

        if (basep->enter_notify_timer_tag) {
                g_source_remove (basep->enter_notify_timer_tag);
                basep->enter_notify_timer_tag = 0;
	}

        if (basep->leave_notify_timer_tag) {
                g_source_remove (basep->leave_notify_timer_tag);
		basep->leave_notify_timer_tag = 0;
	}

        if (basep->mode != BASEP_AUTO_HIDE ||
            basep->state != BASEP_SHOWN)
                return;
                
	if (global_config.hide_delay <= 0)
		basep->leave_notify_timer_tag =
			g_idle_add (basep_widget_autohide, basep);
	else
		basep->leave_notify_timer_tag =
			g_timeout_add (global_config.hide_delay,
				       basep_widget_autohide, basep);
}

PanelOrient
basep_widget_get_applet_orient (BasePWidget *basep)
{
	BasePPosClass *klass = 
		basep_widget_get_pos_class (basep);

	g_return_val_if_fail (klass &&
			      klass->get_applet_orient, -1);

	return klass->get_applet_orient(basep);
}

void
basep_widget_get_size (BasePWidget *basep,
		       int *w, int *h)
{
	GtkRequisition req;
	BasePPosClass *klass = basep_widget_get_pos_class (basep);

	gtk_widget_size_request (basep->ebox, &req);
	*w = req.width;
	*h = req.height;
	
	g_return_if_fail (klass);
	if (klass->get_size)
		klass->get_size(basep, w, h);
}

void
basep_widget_get_pos (BasePWidget *basep,
		      int *x, int *y)
{
	int w, h;
	BasePPosClass *klass = 
		basep_widget_get_pos_class (basep);

	g_return_if_fail (klass && klass->get_pos);

	basep_widget_get_size (basep, &w, &h);
	klass->get_pos(basep, x, y, w, h);
}

void
basep_widget_init_offsets (BasePWidget *basep)
{
	g_return_if_fail (BASEP_IS_WIDGET (basep));

	if (GTK_WIDGET (basep)->window != NULL) {
		int x, y;
		gdk_window_get_pointer (GTK_WIDGET (basep)->window,
					&x, &y, NULL);
		basep->offset_x = x;
		basep->offset_y = y;
	} else {
		basep->offset_x = GTK_WIDGET (basep)->requisition.width / 2;
		basep->offset_y = GTK_WIDGET (basep)->requisition.height / 2;
	}
}

void
basep_widget_set_pos (BasePWidget *basep,
		      int x, int y)
{
	BasePPosClass *klass = basep_widget_get_pos_class (basep);
	gboolean       force = FALSE;
	int            w, h;
	int            new_monitor;

	g_return_if_fail (klass && klass->set_pos);

	/* first take care of switching monitors */
	new_monitor = multiscreen_locate_coords (basep->screen, x, y);
	if (new_monitor >= 0 && new_monitor != basep->monitor) {
		force = TRUE;
		basep_widget_screen_change (basep, basep->screen, new_monitor);
	}

	basep_widget_get_size (basep, &w, &h);
	klass->set_pos (basep, x, y, w, h, force);
}

void
basep_widget_pre_convert_hook (BasePWidget *basep)
{
	BasePPosClass  *klass = basep_widget_get_pos_class (basep);
	g_return_if_fail (klass && klass->pre_convert_hook);

	klass->pre_convert_hook (basep);
}

void
basep_widget_screen_change (BasePWidget *basep,
			    int          screen,
			    int          monitor)
{
	int old_screen;
	int old_monitor;

	g_return_if_fail (BASEP_IS_WIDGET (basep));
	g_return_if_fail (screen >= 0);
	g_return_if_fail (monitor >= 0);

	if (basep->screen == screen && basep->monitor == monitor)
		return;

	old_screen  = basep->screen;  basep->screen  = screen;
	old_monitor = basep->monitor; basep->monitor = monitor;

	g_signal_emit (basep, basep_widget_signals [SCREEN_CHANGE_SIGNAL],
		       0, old_screen, old_monitor);
}

/*****
 * Collision avoidance stuff
 *****/
typedef struct {
	int left;
	int center;
	int right;
} Border;

typedef struct {
	int screen;
	int monitor;
	Border borders[4];
	int left;
	int right;
	int top;
	int bottom;
} ScreenBorders;

typedef struct {
	int screen;
	int monitor;
} Recalc;

static GList *border_list = NULL;

static ScreenBorders *
get_borders (int screen, int monitor)
{
	ScreenBorders *retval;
	GList         *l;

	for (l = border_list; l; l = l->next) {
		ScreenBorders *sb = l->data;

		if (sb->screen == screen &&
		    sb->monitor == monitor)
			return sb;
	}

	retval = g_new0 (ScreenBorders, 1);
	retval->screen  = screen;
	retval->monitor = monitor;
	border_list = g_list_prepend (border_list, retval);

	return retval;
}

static void
basep_calculate_borders (int screen, int monitor)
{
	ScreenBorders *sb;
	GSList        *l;

	sb = get_borders (screen, monitor);

	for (l = panel_list; l; l = l->next) {
		PanelData      *pd = l->data;
		BasePWidget    *basep;
		GtkRequisition  chreq;
		BorderEdge      edge;
		int             size = 0;

		g_assert (pd != NULL);

		if (!EDGE_IS_WIDGET (pd->panel) &&
		    !ALIGNED_IS_WIDGET (pd->panel))
			continue;

		basep = BASEP_WIDGET (pd->panel);

		if (basep->mode == BASEP_AUTO_HIDE ||
		    basep->screen != screen ||
		    basep->monitor != monitor)
			continue;

		gtk_widget_get_child_requisition (basep->ebox, &chreq);

		edge = BORDER_POS (basep->pos)->edge;

		switch (edge) {
		case BORDER_RIGHT:
		case BORDER_LEFT:
			size = chreq.width;
			break;
		case BORDER_TOP:
		case BORDER_BOTTOM:
			size = chreq.height;
			break;
		default:
			g_assert_not_reached ();
			break;
		}

		if (EDGE_IS_WIDGET (basep)) {
			BasePState state = basep->state;

			switch (state) {
			case BASEP_HIDDEN_RIGHT:
				sb->borders [edge].right = MAX (sb->borders [edge].right, size);
				break;
			case BASEP_HIDDEN_LEFT:
				sb->borders [edge].left = MAX (sb->borders [edge].left, size);
				break;
			default:
				sb->borders [edge].right  = MAX (sb->borders [edge].right, size);
				sb->borders [edge].center = MAX (sb->borders [edge].center, size);
				sb->borders [edge].left   = MAX (sb->borders [edge].left, size);
				break;
			}
		} else /* ALIGNED */ {
			AlignedAlignment align = ALIGNED_POS (basep->pos)->align;

			switch (align) {
			case ALIGNED_LEFT:
				sb->borders [edge].left = MAX (sb->borders [edge].left, size);
				break;
			case ALIGNED_RIGHT:
				sb->borders [edge].right = MAX (sb->borders [edge].right, size);
				break;
			case ALIGNED_CENTER:
				sb->borders [edge].center = MAX (sb->borders [edge].center, size);
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		}
	}
}

static int
border_max (ScreenBorders *sb,
	    BorderEdge     edge)
{
	return MAX (sb->borders [edge].center,
			MAX (sb->borders [edge].left,
			     sb->borders [edge].right));
}

void
basep_border_recalc (int screen, int monitor)
{
	ScreenBorders *sb;
	ScreenBorders  old;
	GSList        *l;
	int            i;

	sb = get_borders (screen, monitor);

	memcpy (&old, sb, sizeof (ScreenBorders));

	for (i = 0; i < 4; i++) {
		sb->borders [i].left = 0;
		sb->borders [i].center = 0;
		sb->borders [i].right = 0;
	}

	basep_calculate_borders (screen, monitor);

	sb->left   = border_max (sb, BORDER_LEFT);
	sb->right  = border_max (sb, BORDER_RIGHT);
	sb->bottom = border_max (sb, BORDER_BOTTOM);
	sb->top    = border_max (sb, BORDER_TOP) +
				foobar_widget_get_height (screen, monitor);

	if (!memcmp (&old, sb, sizeof (ScreenBorders)))
		return;

	for (l = panel_list; l; l = l->next) {
		PanelData *pdata = l->data;
		GtkWidget *panel;

		g_assert (pdata != NULL);

		panel = pdata->panel;

		if (BORDER_IS_WIDGET (panel) &&
		    BASEP_WIDGET (panel)->screen == screen &&
		    BASEP_WIDGET (panel)->monitor == monitor)
			gtk_widget_queue_resize (panel);
	}
}

static guint  queue_recalc_id = 0;
static GList *pending_recalc_list = NULL;

static gboolean
queue_recalc_handler (gpointer data)
{
	GList *list, *l;

	queue_recalc_id = 0;

	list = pending_recalc_list;
	pending_recalc_list = NULL;

	for (l = list; l; l = l->next) {
		Recalc *recalc = l->data;

		basep_border_recalc (recalc->screen, recalc->monitor);
		g_free (recalc);
	}

	g_list_free (list);

	return FALSE;
}

void
basep_border_queue_recalc (int screen, int monitor)
{
	Recalc *new_recalc;
	GList  *l;

	for (l = pending_recalc_list; l; l = l->next) {
		Recalc *recalc = l->data;

		if (recalc->screen == screen &&
		    recalc->monitor == monitor)
			return;
	}

	new_recalc = g_new0 (Recalc, 1);
	new_recalc->screen  = screen;
	new_recalc->monitor = monitor;

	pending_recalc_list = g_list_prepend (
					pending_recalc_list, new_recalc);
	if (!queue_recalc_id)
		queue_recalc_id = g_idle_add (queue_recalc_handler, NULL);
}

void
basep_border_get (BasePWidget *basep,
		  BorderEdge   edge,
		  int         *left,
		  int         *center,
		  int         *right)
{
	ScreenBorders *sb;

	g_assert (BASEP_IS_WIDGET (basep));
	g_assert (basep->screen >=0);
	g_assert (basep->monitor >=0);
	g_assert (edge == BORDER_TOP    ||
		  edge == BORDER_BOTTOM ||
		  edge == BORDER_RIGHT  ||
		  edge == BORDER_LEFT);

	sb = get_borders (basep->screen, basep->monitor);

	if (left)
		*left = sb->borders [edge].left;

	if (center)
		*center = sb->borders [edge].center;

	if (right)
		*right = sb->borders [edge].right;

}

static gboolean
basep_widget_popup_panel_menu (BasePWidget *basep)
{
 	gboolean retval;

	g_signal_emit_by_name (basep->panel, "popup_menu", &retval);

	return retval;
}
