/* GNOME panel: foobar widGET
 * Copyright 1999,2000 Helix Code, Inc.
 * Copyright 2000 Eazel, Inc.
 *
 * Author: Jacob Berkman
 *
 */

/* since BASEP_IS_WIDGET() is used throughout, it makes life easier if we
 * have a GtkWindow of our own.
 */

#include <config.h>
#include <unistd.h>
#include <string.h>

#include "foobar-widget.h"

#include <libgnome/libgnome.h>
#include <gdk/gdkkeysyms.h>

#include "session.h"
#include "panel-widget.h"
#include "xstuff.h"
#include "basep-widget.h"
#include "panel-config-global.h"
#include "panel-util.h"
#include "drawer-widget.h"
#include "gnome-run.h"
#include "multiscreen-stuff.h"
#include "panel-marshal.h"
#include "egg-screen-exec.h"
#include "panel-stock-icons.h"
#include "panel-action-button.h"
#include "panel-recent.h"

extern GlobalConfig global_config;
extern GSList *panel_list;

static void foobar_widget_class_init	(FoobarWidgetClass *klass);
static void foobar_widget_instance_init (FoobarWidget *foo);
static void foobar_widget_realize	(GtkWidget *w);
static void foobar_widget_destroy	(GtkObject *o);
static void foobar_widget_size_allocate	(GtkWidget *toplevel,
					 GtkAllocation *allocation);
static void foobar_widget_size_request	(GtkWidget *toplevel,
					 GtkRequisition *requisition);
static gboolean foobar_widget_popup_panel_menu (FoobarWidget *foobar);

static GList *foobars = NULL;

static GtkWindowClass *foobar_widget_parent_class = NULL;

enum {
	POPUP_PANEL_MENU_SIGNAL,
	WIDGET_LAST_SIGNAL
};

GType
foobar_widget_get_type (void)
{
	static GType object_type = 0;

	if (object_type == 0) {
		static const GTypeInfo object_info = {
                    sizeof (FoobarWidgetClass),
                    (GBaseInitFunc)         NULL,
                    (GBaseFinalizeFunc)     NULL,
                    (GClassInitFunc)        foobar_widget_class_init,
                    NULL,                   /* class_finalize */
                    NULL,                   /* class_data */
                    sizeof (FoobarWidget),
                    0,                      /* n_preallocs */
                    (GInstanceInitFunc)     foobar_widget_instance_init
		};

		object_type = g_type_register_static (GTK_TYPE_WINDOW, "FoobarWidget", &object_info, 0);
        	foobar_widget_parent_class = g_type_class_ref (GTK_TYPE_WINDOW);

	}

	return object_type;
}

static guint foobar_widget_signals [WIDGET_LAST_SIGNAL] = { 0 };

static void
foobar_widget_class_init (FoobarWidgetClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
	GtkBindingSet  *binding_set;

	binding_set = gtk_binding_set_by_class (klass);

	klass->popup_panel_menu = foobar_widget_popup_panel_menu;

	object_class->destroy = foobar_widget_destroy;

	widget_class->realize       = foobar_widget_realize;
	widget_class->size_allocate = foobar_widget_size_allocate;
	widget_class->size_request  = foobar_widget_size_request;

	foobar_widget_signals [POPUP_PANEL_MENU_SIGNAL] =
		g_signal_new ("popup_panel_menu",
			     G_TYPE_FROM_CLASS (object_class),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			     G_STRUCT_OFFSET (FoobarWidgetClass, popup_panel_menu),
			     NULL,
			     NULL,
			     panel_marshal_BOOLEAN__VOID,
			     G_TYPE_BOOLEAN,
			     0);

	gtk_binding_entry_add_signal (binding_set, GDK_F10, GDK_CONTROL_MASK,
				     "popup_panel_menu", 0);
}

void
foobar_widget_update_winhints (FoobarWidget *foo)
{
	GtkWidget *w = GTK_WIDGET (foo);

	gtk_window_set_decorated (GTK_WINDOW (w), FALSE);
	gtk_window_stick (GTK_WINDOW (w));

	xstuff_set_wmspec_dock_hints (w->window, FALSE /* autohide */);
}

static void
foobar_widget_realize (GtkWidget *w)
{
	gtk_window_set_wmclass (GTK_WINDOW (w),
				"panel_window", "Panel");

	if (GTK_WIDGET_CLASS (foobar_widget_parent_class)->realize)
		GTK_WIDGET_CLASS (foobar_widget_parent_class)->realize (w);

	foobar_widget_update_winhints (FOOBAR_WIDGET (w));
	xstuff_set_no_group_and_no_input (w->window);

	xstuff_set_wmspec_strut (w->window,
				 0 /* left */,
				 0 /* right */,
				 w->allocation.height /* top */,
				 0 /* bottom */);
}

static void
foobar_widget_instance_init (FoobarWidget *foo)
{
	GtkWindow *window;
	GtkWidget *bufmap;
	GtkWidget *align;
	char      *path;

	window = GTK_WINDOW (foo);

	foo->screen  = 0;
	foo->monitor = 0;

	foo->compliant_wm = xstuff_is_compliant_wm ();
	if(foo->compliant_wm)
		GTK_WINDOW(foo)->type = GTK_WINDOW_TOPLEVEL;
	else
		GTK_WINDOW(foo)->type = GTK_WINDOW_POPUP;

	window->allow_shrink = TRUE;
	window->allow_grow   = TRUE;

	g_signal_connect (G_OBJECT (foo), "delete_event",
			  G_CALLBACK (gtk_true), NULL);

	/* panel widget */
	foo->panel = panel_widget_new (NULL, FALSE, GTK_ORIENTATION_HORIZONTAL,
				       PANEL_SIZE_X_SMALL, PANEL_BACK_NONE,
				       NULL, FALSE, FALSE, FALSE, NULL);
	PANEL_WIDGET (foo->panel)->panel_parent = GTK_WIDGET (foo);
	PANEL_WIDGET (foo->panel)->drop_widget = GTK_WIDGET (foo);
	panel_set_atk_name_desc (foo->panel, _("Menu Panel"), _("GNOME Menu Panel"));

	foo->ebox = gtk_event_box_new ();
	foo->hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_set_direction (foo->hbox, GTK_TEXT_DIR_LTR);
	gtk_container_add(GTK_CONTAINER(foo->ebox), foo->hbox);

	path = panel_pixmap_discovery ("panel-corner-left.png", FALSE /* fallback */);
	if (path != NULL) {
		bufmap = gtk_image_new_from_file (path);
		g_free (path);
		align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
		gtk_container_add (GTK_CONTAINER (align), bufmap);
		gtk_box_pack_start (GTK_BOX (foo->hbox), align, FALSE, FALSE, 0);
	}

	gtk_container_add (GTK_CONTAINER (foo->hbox), foo->panel);

	path = panel_pixmap_discovery ("panel-corner-right.png", FALSE /* fallback */);
	if (path != NULL) {
		bufmap = gtk_image_new_from_file (path);
		g_free (path);
		align = gtk_alignment_new (1.0, 0.0, 1.0, 0.0);
		gtk_container_add (GTK_CONTAINER (align), bufmap);
		gtk_box_pack_end (GTK_BOX (foo->hbox), align, FALSE, FALSE, 0);
	}

	gtk_container_add (GTK_CONTAINER (foo), foo->ebox);

	gtk_widget_show_all (foo->ebox);
}

static void
queue_panel_resize (gpointer data, gpointer user_data)
{
	PanelData *pd = data;
	GtkWidget *panel;

	g_assert (pd);

	panel = pd->panel;

	g_return_if_fail (GTK_IS_WIDGET (panel));

	if (!DRAWER_IS_WIDGET (panel) && !FOOBAR_IS_WIDGET (panel))
		gtk_widget_queue_resize (panel);
}

static void
foobar_widget_destroy (GtkObject *o)
{
	FoobarWidget *foo = FOOBAR_WIDGET (o);

	foobars = g_list_remove (foobars, foo);

	g_slist_foreach (panel_list, queue_panel_resize, NULL);

	if (GTK_OBJECT_CLASS (foobar_widget_parent_class)->destroy)
		GTK_OBJECT_CLASS (foobar_widget_parent_class)->destroy (o);
}

void
foobar_widget_screen_size_changed (FoobarWidget *foo,
				   GdkScreen    *screen)
{
	GtkWindow *window;
	int        w, h;

	window = GTK_WINDOW (foo);
	
	gtk_window_get_size (window, &w, &h);

	gtk_window_move (window,
			 multiscreen_x (foo->screen, foo->monitor),
			 multiscreen_y (foo->screen, foo->monitor));

	gtk_window_set_resizable (window, TRUE);
	gtk_window_resize (window,
			   multiscreen_width (foo->screen, foo->monitor),
			   h);
}

static void
foobar_widget_size_allocate (GtkWidget     *toplevel,
			     GtkAllocation *allocation)
{
	FoobarWidget *foo;

	GTK_WIDGET_CLASS (foobar_widget_parent_class)->size_allocate (toplevel, allocation);

	if (!GTK_WIDGET_REALIZED (toplevel))
		return;

	foo = FOOBAR_WIDGET (toplevel);

	g_slist_foreach (panel_list, queue_panel_resize, NULL);
	basep_border_queue_recalc (foo->screen, foo->monitor);

	xstuff_set_wmspec_strut (toplevel->window, 0, 0, allocation->height, 0);
}

static void
foobar_widget_size_request (GtkWidget      *toplevel,
			    GtkRequisition *requisition)
{
	FoobarWidget *foo;
	int           old_height;

	foo = FOOBAR_WIDGET (toplevel);

	old_height = toplevel->requisition.height;

	GTK_WIDGET_CLASS (foobar_widget_parent_class)->size_request (toplevel, requisition);

	if (!GTK_WIDGET_REALIZED (toplevel))
		return;

	requisition->width = multiscreen_width (foo->screen, foo->monitor);

	if (requisition->height != old_height)
		gtk_window_resize (GTK_WINDOW (toplevel),
				   requisition->width,
				   requisition->height);

	xstuff_set_pos_size (toplevel->window,
			     multiscreen_x (foo->screen, foo->monitor),
			     multiscreen_y (foo->screen, foo->monitor),
			     requisition->width,
			     requisition->height);
}

GtkWidget *
foobar_widget_new_dud (const char *panel_id,
		       int         screen,
		       int         monitor)
{
	FoobarWidget *foo;

	g_return_val_if_fail (screen >= 0, NULL);
	g_return_val_if_fail (monitor >= 0, NULL);

	if (foobar_widget_exists (screen, monitor))
		return NULL;

	foo = g_object_new (FOOBAR_TYPE_WIDGET, NULL);

	if (panel_id)
		panel_widget_set_id (PANEL_WIDGET (foo->panel), panel_id);

	foo->screen  = screen;
	foo->monitor = monitor;

	gtk_window_set_screen (GTK_WINDOW (foo),
			       panel_screen_from_number (screen));

	gtk_window_move (GTK_WINDOW (foo),
			 multiscreen_x (screen, monitor),
			 multiscreen_y (screen, monitor));

	foobars = g_list_prepend (foobars, foo);

	return GTK_WIDGET (foo);
}

gboolean
foobar_widget_exists (int screen, int monitor)
{
	GList *l;

	g_return_val_if_fail (screen  >= 0, 0);
	g_return_val_if_fail (monitor >= 0, 0);

	for (l = foobars; l; l = l->next) {
		FoobarWidget *foo = l->data;

		if (foo->screen == screen &&
		    foo->monitor == monitor)
			return TRUE;
	}

	return FALSE;
}

int
foobar_widget_get_height (int screen, int monitor)
{
	GList *l;

	g_return_val_if_fail (screen  >= 0, 0);
	g_return_val_if_fail (monitor >= 0, 0);

	for (l = foobars; l; l = l->next) {
		FoobarWidget *foo = FOOBAR_WIDGET (l->data);

		if (foo->screen  == screen &&
		    foo->monitor == monitor)
			return GTK_WIDGET (foo)->allocation.height;
	}

	return 0; 
}

static void
reparent_button_widgets(GtkWidget *w, gpointer data)
{
	GdkWindow *newwin = data;
	if (BUTTON_IS_WIDGET (w)) {
		GtkButton *button = GTK_BUTTON (w);
		/* we can just reparent them all to 0,0 as the next thing
		 * that will happen is a queue_resize and on size allocate
		 * they will be put into their proper place */
		gdk_window_reparent (button->event_window, newwin, 0, 0);
	}
}

void
foobar_widget_redo_window(FoobarWidget *foo)
{
	GtkWindow *window;
	GtkWidget *widget;
	GdkWindowAttr attributes;
	gint attributes_mask;
	GdkWindow *oldwin;
	GdkWindow *newwin;
	gboolean comp;

	comp = xstuff_is_compliant_wm();
	if (comp == foo->compliant_wm)
		return;

	window = GTK_WINDOW(foo);
	widget = GTK_WIDGET(foo);

	foo->compliant_wm = comp;
	if (foo->compliant_wm) {
		window->type = GTK_WINDOW_TOPLEVEL;
		attributes.window_type = GDK_WINDOW_TOPLEVEL;
	} else {
		window->type = GTK_WINDOW_POPUP;
		attributes.window_type = GDK_WINDOW_TEMP;
	}

	if (widget->window == NULL)
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
	gdk_window_reparent(foo->ebox->window, newwin, 0, 0);
	/* reparent all the base event windows as they are also children of
	 * the foobar */
	gtk_container_foreach(GTK_CONTAINER(foo->panel),
			      reparent_button_widgets,
			      newwin);


	widget->window = newwin;

	gdk_window_set_user_data(oldwin, NULL);
	gdk_window_destroy(oldwin);

	widget->style = gtk_style_attach(widget->style, widget->window);
	gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	gtk_widget_queue_resize(widget);

	foobar_widget_update_winhints (foo);

	xstuff_set_wmspec_strut (widget->window,
				 0 /* left */,
				 0 /* right */,
				 widget->allocation.height /* top */,
				 0 /* bottom */);

	gtk_drag_dest_set (widget, 0, NULL, 0, 0);

	gtk_widget_map(widget);
}

static gboolean
foobar_widget_popup_panel_menu (FoobarWidget *foobar)
{
 	gboolean retval;

	g_signal_emit_by_name (foobar->panel, "popup_menu", &retval);

	return retval;
}
