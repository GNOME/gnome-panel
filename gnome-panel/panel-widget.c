/* Gnome panel: panel widget
 * (C) 1997,1998,1999,2000 the Free Software Foundation
 * (C) 2000 Eazel, Inc.
 *
 * Authors:  George Lebl
 */
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include "applet.h"
#include "panel-widget.h"
#include "panel.h"
#include "panel-bindings.h"
#include "panel-util.h"
#include "panel-typebuiltins.h"
#include "panel-applet-frame.h"
#include "panel-lockdown.h"

typedef enum {
	PANEL_SWITCH_MOVE = 0,
	PANEL_PUSH_MOVE
} PanelMovementType;

G_DEFINE_TYPE (PanelWidget, panel_widget, GTK_TYPE_FIXED);

enum {
	APPLET_MOVE_SIGNAL,
	APPLET_ADDED_SIGNAL,
	APPLET_REMOVED_SIGNAL,
	PUSH_MOVE_SIGNAL,
	SWITCH_MOVE_SIGNAL,
	TAB_MOVE_SIGNAL,
	END_MOVE_SIGNAL,
	POPUP_PANEL_MENU_SIGNAL,
	LAST_SIGNAL
};

static guint panel_widget_signals [LAST_SIGNAL] = {0};

static GSList *panels = NULL;

/*define for some debug output*/
#undef PANEL_WIDGET_DEBUG

static gboolean panel_applet_in_drag = FALSE;
static GtkWidget *saved_focus_widget = NULL;

static void panel_widget_size_allocate        (GtkWidget        *widget,
					       GtkAllocation    *allocation);
static void panel_widget_cadd                 (GtkContainer     *container,
					       GtkWidget        *widget);
static void panel_widget_cremove              (GtkContainer     *container,
					       GtkWidget        *widget);
static void panel_widget_dispose              (GObject          *obj);

static void panel_widget_push_move_applet   (PanelWidget      *panel,
                                             GtkDirectionType  dir);
static void panel_widget_switch_move_applet (PanelWidget      *panel,
                                             GtkDirectionType  dir);
static void panel_widget_tab_move           (PanelWidget      *panel,
                                             gboolean          next);
static void panel_widget_end_move           (PanelWidget      *panel);
static gboolean panel_widget_real_focus     (GtkWidget        *widget,
                                             GtkDirectionType  direction);

static void panel_widget_update_positions   (PanelWidget      *panel);

/************************
 convenience functions
 ************************/
static int
applet_data_compare (AppletData *ad1, AppletData *ad2)
{
	if (ad1->pack_type != ad2->pack_type)
		return ad1->pack_type - ad2->pack_type; /* start < center < end */
	else if (ad1->pack_type != PANEL_OBJECT_PACK_END)
		return ad1->pack_index - ad2->pack_index;
	else
		return ad2->pack_index - ad1->pack_index;
}

static void
emit_applet_moved (PanelWidget *panel_widget,
		   AppletData  *applet)
{
	/* we always want to queue a draw after moving, so do it here instead
	 * of after the signal emission in all callers */
	gtk_widget_queue_draw (applet->applet);

	g_signal_emit (panel_widget,
		       panel_widget_signals [APPLET_MOVE_SIGNAL], 0,
		       applet->applet);
}

/************************
 widget core
 ************************/

static void
add_tab_bindings (GtkBindingSet    *binding_set,
   	          GdkModifierType   modifiers,
		  gboolean          next)
{
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Tab, modifiers,
				      "tab_move", 1,
				      G_TYPE_BOOLEAN, next);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Tab, modifiers,
				      "tab_move", 1,
				      G_TYPE_BOOLEAN, next);
}

static void
add_move_bindings (GtkBindingSet    *binding_set,
		   GdkModifierType   modifiers,
		   const gchar      *name)
{
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Up, modifiers,
                                      name, 1,
                                      GTK_TYPE_DIRECTION_TYPE, GTK_DIR_UP);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Down, modifiers,
                                      name, 1,
                                      GTK_TYPE_DIRECTION_TYPE, GTK_DIR_DOWN);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Left, modifiers,
                                      name, 1,
                                      GTK_TYPE_DIRECTION_TYPE, GTK_DIR_LEFT);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Right, modifiers,
                                      name, 1,
                                      GTK_TYPE_DIRECTION_TYPE, GTK_DIR_RIGHT);
}

static void
add_all_move_bindings (PanelWidget *panel)
{
	GtkWidgetClass *class;
	GtkBindingSet *binding_set;
	GtkWidget *focus_widget;

	class = GTK_WIDGET_GET_CLASS (panel);

	binding_set = gtk_binding_set_by_class (class);

	add_move_bindings (binding_set, GDK_SHIFT_MASK, "push_move");
	add_move_bindings (binding_set, GDK_CONTROL_MASK, "switch_move");

	add_tab_bindings (binding_set, 0, TRUE);
	add_tab_bindings (binding_set, GDK_SHIFT_MASK, FALSE);

	gtk_binding_entry_add_signal (binding_set,
                                      GDK_KEY_Escape, 0,
                                      "end_move", 0);
	gtk_binding_entry_add_signal (binding_set,
                                      GDK_KEY_KP_Enter, 0,
                                      "end_move", 0);
	gtk_binding_entry_add_signal (binding_set,
                                      GDK_KEY_Return, 0,
                                      "end_move", 0);
	gtk_binding_entry_add_signal (binding_set,
                                      GDK_KEY_KP_Space, 0,
                                      "end_move", 0);
	gtk_binding_entry_add_signal (binding_set,
                                      GDK_KEY_space, 0,
                                      "end_move", 0);

	focus_widget = gtk_window_get_focus (GTK_WINDOW (panel->toplevel));
	if (GTK_IS_SOCKET (focus_widget)) {
		/*
		 * If the focus widget is a GtkSocket, i.e. the
		 * focus is in an applet in another process then
		 * key bindings do not work. We get around this by
		 * by setting the focus to the PanelWidget for the
		 * duration of the move.
		 */
		gtk_widget_set_can_focus (GTK_WIDGET (panel), TRUE);
		gtk_widget_grab_focus (GTK_WIDGET (panel));
		saved_focus_widget = focus_widget;
	}
}

static void
panel_widget_force_grab_focus (GtkWidget *widget)
{
	gboolean can_focus = gtk_widget_get_can_focus (widget);
	/*
	 * This follows what gtk_socket_claim_focus() does
	 */
	if (!can_focus)
		gtk_widget_set_can_focus (widget, TRUE);
	gtk_widget_grab_focus (widget);
	if (!can_focus)
		gtk_widget_set_can_focus (widget, FALSE);
}

static void
panel_widget_reset_saved_focus (PanelWidget *panel)
{
	if (saved_focus_widget) {
		gtk_widget_set_can_focus (GTK_WIDGET (panel), FALSE);
		panel_widget_force_grab_focus (saved_focus_widget);
		saved_focus_widget = NULL;
	}
}

static void
remove_tab_bindings (GtkBindingSet    *binding_set,
		     GdkModifierType   modifiers,
		     gboolean          next)
{
	gtk_binding_entry_remove (binding_set, GDK_KEY_Tab, modifiers);
	gtk_binding_entry_remove (binding_set, GDK_KEY_KP_Tab, modifiers);
}

static void
remove_move_bindings (GtkBindingSet    *binding_set,
		      GdkModifierType   modifiers)
{
	gtk_binding_entry_remove (binding_set, GDK_KEY_Up, modifiers);
	gtk_binding_entry_remove (binding_set, GDK_KEY_Down, modifiers);
	gtk_binding_entry_remove (binding_set, GDK_KEY_Left, modifiers);
	gtk_binding_entry_remove (binding_set, GDK_KEY_Right, modifiers);
}

static void
remove_all_move_bindings (PanelWidget *panel)
{
	GtkWidgetClass *class;
	GtkBindingSet *binding_set;

	class = GTK_WIDGET_GET_CLASS (panel);

	binding_set = gtk_binding_set_by_class (class);

	panel_widget_reset_saved_focus (panel);

	remove_move_bindings (binding_set, GDK_SHIFT_MASK);
	remove_move_bindings (binding_set, GDK_CONTROL_MASK);
	remove_move_bindings (binding_set, GDK_MOD1_MASK);
	remove_move_bindings (binding_set, 0);
	remove_tab_bindings (binding_set, 0, TRUE);
	remove_tab_bindings (binding_set, GDK_SHIFT_MASK, FALSE);

	gtk_binding_entry_remove (binding_set, GDK_KEY_Escape, 0);
	gtk_binding_entry_remove (binding_set, GDK_KEY_KP_Enter, 0);
	gtk_binding_entry_remove (binding_set, GDK_KEY_Return, 0);
	gtk_binding_entry_remove (binding_set, GDK_KEY_KP_Space, 0);
	gtk_binding_entry_remove (binding_set, GDK_KEY_space, 0);
}

static GtkSizeRequestMode
panel_widget_get_request_mode (GtkWidget *widget)
{
  PanelWidget *self;

  self = PANEL_WIDGET (widget);

  if (self->orient == GTK_ORIENTATION_VERTICAL)
    return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;

  return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
panel_widget_get_preferred_height (GtkWidget *widget,
                                   gint      *minimum_height,
                                   gint      *natural_height)
{
  PanelWidget *self;
  int min_height;
  int nat_height;

  self = PANEL_WIDGET (widget);

  min_height = 0;
  nat_height = 0;

  if (self->orient == GTK_ORIENTATION_HORIZONTAL)
    {
      min_height = MAX (self->sz, 12);
      nat_height = MAX (self->sz, 12);
    }
  else if (self->orient == GTK_ORIENTATION_VERTICAL)
    {
      GList *l;

      for (l = self->applet_list; l != NULL; l = g_list_next (l))
        {
          AppletData *ad;
          int child_min_height;
          int child_nat_height;

          ad = l->data;

          gtk_widget_get_preferred_height (ad->applet,
                                           &child_min_height,
                                           &child_nat_height);

          if (ad->expand_major)
            {
              min_height += child_min_height;
              nat_height += child_nat_height;
            }
          else
            {
              min_height += child_min_height;
              nat_height += child_min_height;
            }
        }
    }
  else
    {
      g_assert_not_reached ();
    }

  *minimum_height = min_height;
  *natural_height = nat_height;
}

static void
panel_widget_get_preferred_width_for_height (GtkWidget *widget,
                                             gint       height,
                                             gint      *minimum_width,
                                             gint      *natural_width)
{
  PanelWidget *self;
  int min_width;
  int nat_width;

  self = PANEL_WIDGET (widget);

  min_width = 0;
  nat_width = 0;

  if (self->orient == GTK_ORIENTATION_VERTICAL)
    {
      min_width = MAX (self->sz, 12);
      nat_width = MAX (self->sz, 12);
    }
  else if (self->orient == GTK_ORIENTATION_HORIZONTAL)
    {
      GList *l;

      for (l = self->applet_list; l != NULL; l = g_list_next (l))
        {
          AppletData *ad;
          int child_min_width;
          int child_nat_width;

          ad = l->data;

          gtk_widget_get_preferred_width_for_height (ad->applet,
                                                     height,
                                                     &child_min_width,
                                                     &child_nat_width);

          if (ad->expand_major)
            {
              min_width += child_min_width;
              nat_width += child_nat_width;
            }
          else
            {
              min_width += child_min_width;
              nat_width += child_min_width;
            }
        }
    }
  else
    {
      g_assert_not_reached ();
    }

  *minimum_width = min_width;
  *natural_width = nat_width;
}

static void
panel_widget_get_preferred_width (GtkWidget *widget,
                                  gint      *minimum_width,
                                  gint      *natural_width)
{
  PanelWidget *self;
  int min_width;
  int nat_width;

  self = PANEL_WIDGET (widget);

  min_width = 0;
  nat_width = 0;

  if (self->orient == GTK_ORIENTATION_VERTICAL)
    {
      min_width = MAX (self->sz, 12);
      nat_width = MAX (self->sz, 12);
    }
  else if (self->orient == GTK_ORIENTATION_HORIZONTAL)
    {
      GList *l;

      for (l = self->applet_list; l != NULL; l = g_list_next (l))
        {
          AppletData *ad;
          int child_min_width;
          int child_nat_width;

          ad = l->data;

          gtk_widget_get_preferred_width (ad->applet,
                                          &child_min_width,
                                          &child_nat_width);

          if (ad->expand_major)
            {
              min_width += child_min_width;
              nat_width += child_nat_width;
            }
          else
            {
              min_width += child_min_width;
              nat_width += child_min_width;
            }
        }
    }
  else
    {
      g_assert_not_reached ();
    }

  *minimum_width = min_width;
  *natural_width = nat_width;
}

static void
panel_widget_get_preferred_height_for_width (GtkWidget *widget,
                                             gint       width,
                                             gint      *minimum_height,
                                             gint      *natural_height)
{
  PanelWidget *self;
  int min_height;
  int nat_height;

  self = PANEL_WIDGET (widget);

  min_height = 0;
  nat_height = 0;

  if (self->orient == GTK_ORIENTATION_HORIZONTAL)
    {
      min_height = MAX (self->sz, 12);
      nat_height = MAX (self->sz, 12);
    }
  else if (self->orient == GTK_ORIENTATION_VERTICAL)
    {
      GList *l;

      for (l = self->applet_list; l != NULL; l = g_list_next (l))
        {
          AppletData *ad;
          int child_min_height;
          int child_nat_height;

          ad = l->data;

          gtk_widget_get_preferred_height_for_width (ad->applet,
                                                     width,
                                                     &child_min_height,
                                                     &child_nat_height);

          if (ad->expand_major)
            {
              min_height += child_min_height;
              nat_height += child_nat_height;
            }
          else
            {
              min_height += child_min_height;
              nat_height += child_min_height;
            }
        }
    }
  else
    {
      g_assert_not_reached ();
    }

  *minimum_height = min_height;
  *natural_height = nat_height;
}

static void
panel_widget_class_init (PanelWidgetClass *class)
{
	GObjectClass *object_class = (GObjectClass*) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass*) class;
	GtkContainerClass *container_class = (GtkContainerClass*) class;

	panel_widget_signals[APPLET_MOVE_SIGNAL] =
                g_signal_new ("applet_move",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PanelWidgetClass, applet_move),
                              NULL,
                              NULL, 
                              NULL,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_POINTER); 

	panel_widget_signals[APPLET_ADDED_SIGNAL] =
                g_signal_new ("applet_added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PanelWidgetClass, applet_added),
                              NULL,
                              NULL, 
                              NULL,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_POINTER); 

	panel_widget_signals[APPLET_REMOVED_SIGNAL] =
                g_signal_new ("applet_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PanelWidgetClass, applet_removed),
                              NULL,
                              NULL, 
                              NULL,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_POINTER); 

	panel_widget_signals[PUSH_MOVE_SIGNAL] =
		g_signal_new ("push_move",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (PanelWidgetClass, push_move),
                              NULL,
                              NULL,
                              NULL,
                              G_TYPE_NONE,
                              1,
                              GTK_TYPE_DIRECTION_TYPE);
	
	panel_widget_signals[SWITCH_MOVE_SIGNAL] =
                g_signal_new ("switch_move",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (PanelWidgetClass, switch_move),
                              NULL,
                              NULL,
                              NULL,
                              G_TYPE_NONE,
                              1,
                              GTK_TYPE_DIRECTION_TYPE);

	panel_widget_signals[TAB_MOVE_SIGNAL] =
                g_signal_new ("tab_move",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (PanelWidgetClass, tab_move),
                              NULL,
                              NULL,
                              NULL,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_BOOLEAN);

	panel_widget_signals[END_MOVE_SIGNAL] =
		g_signal_new ("end_move",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (PanelWidgetClass, end_move),
                              NULL,
                              NULL,
                              NULL,
                              G_TYPE_NONE,
                              0);

	class->applet_move = NULL;
	class->applet_added = NULL;
	class->applet_removed = NULL;
	class->push_move = panel_widget_push_move_applet;
	class->switch_move = panel_widget_switch_move_applet;
	class->tab_move = panel_widget_tab_move;
	class->end_move = panel_widget_end_move;

	object_class->dispose = panel_widget_dispose;

	widget_class->get_request_mode = panel_widget_get_request_mode;
	widget_class->get_preferred_height = panel_widget_get_preferred_height;
	widget_class->get_preferred_width_for_height = panel_widget_get_preferred_width_for_height;
	widget_class->get_preferred_width = panel_widget_get_preferred_width;
	widget_class->get_preferred_height_for_width = panel_widget_get_preferred_height_for_width;
	widget_class->size_allocate = panel_widget_size_allocate;
	widget_class->focus = panel_widget_real_focus;

	container_class->add = panel_widget_cadd;
	container_class->remove = panel_widget_cremove;
}

static void
panel_widget_reset_focus (GtkContainer *container,
                          GtkWidget    *widget)
{
	PanelWidget *panel = PANEL_WIDGET (container);

	if (gtk_container_get_focus_child (container) == widget) {
		GList *children;

		children = gtk_container_get_children (container);

		/* More than one element on the list */
		if (children && children->next) {
			GList *l;

			/* There are still object on the panel */
			for (l = children; l; l = l->next) {
				GtkWidget *child_widget;

				child_widget = l->data;
				if (child_widget == widget)
					break;
			}
			if (l) {
				GtkWidget *next_widget;

				if (l->next)
					next_widget = l->next->data;
				else
					next_widget = l->prev->data;

				gtk_widget_child_focus (next_widget,
						        GTK_DIR_TAB_FORWARD);
			}
		} else
			panel_widget_focus (panel);

		g_list_free (children);
	}
}

static void
panel_widget_cadd (GtkContainer *container,
		   GtkWidget    *widget)
{
	g_return_if_fail (PANEL_IS_WIDGET (container));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	panel_widget_add (PANEL_WIDGET (container), widget,
			  PANEL_OBJECT_PACK_START, 0, FALSE);
}

static void
panel_widget_cremove (GtkContainer *container, GtkWidget *widget)
{
	AppletData *ad;
	PanelWidget *panel;

	g_return_if_fail (PANEL_IS_WIDGET (container));
	g_return_if_fail (GTK_IS_WIDGET (widget));
	
	panel = PANEL_WIDGET (container);
	
	ad = g_object_get_data (G_OBJECT (widget), PANEL_APPLET_DATA);

	panel_widget_reset_focus (container, widget);

	if(panel->currently_dragged_applet == ad)
		panel_widget_applet_drag_end(panel);

	g_object_ref (widget);
	if (GTK_CONTAINER_CLASS (panel_widget_parent_class)->remove)
		(* GTK_CONTAINER_CLASS (panel_widget_parent_class)->remove) (container,
								widget);
	if (ad)
		panel->applet_list = g_list_remove (panel->applet_list, ad);

	panel_widget_update_positions (panel);

	g_signal_emit (G_OBJECT (container),
		       panel_widget_signals[APPLET_REMOVED_SIGNAL],
		       0, widget);
	g_object_unref (widget);
}


/* data should be freed with g_list_free() */
static GList *
get_applet_list_pack (PanelWidget         *panel,
		      PanelObjectPackType  pack_type)
{
	GList *ret;
	GList *l;
	GList *prev;

	g_return_val_if_fail (PANEL_IS_WIDGET (panel), NULL);

	for (l = panel->applet_list; l; l = l->next) {
		AppletData *ad = l->data;

		if (ad->pack_type == pack_type)
			break;
	}

	if (!l)
		return NULL;

	ret = g_list_copy (l);
	for (l = ret; l; l = l->next) {
		AppletData *ad = l->data;
		if (ad->pack_type != pack_type)
			break;
	}

	if (!l)
		return ret;

	prev = l->prev;
	if (prev)
		prev->next = NULL;
	g_list_free (l);

	return ret;
}

static void
panel_widget_compress_pack_indexes_list (PanelWidget *panel,
					 GList       *list)
{
	GList *l;
	AppletData *ad;
	int index;

	for (l = list, index = 0; l; l = l->next, index++) {
		ad = l->data;
		if (ad->pack_index != index) {
			ad->pack_index = index;
			emit_applet_moved (panel, ad);
		}
	}
}

/* make sure our lists always start at 0 */
static void
panel_widget_compress_pack_indexes (PanelWidget *panel)
{
	GList *list;

	list = get_applet_list_pack (panel, PANEL_OBJECT_PACK_START);
	panel_widget_compress_pack_indexes_list (panel, list);
	g_list_free (list);

	list = get_applet_list_pack (panel, PANEL_OBJECT_PACK_CENTER);
	panel_widget_compress_pack_indexes_list (panel, list);
	g_list_free (list);

	list = get_applet_list_pack (panel, PANEL_OBJECT_PACK_END);
	list = g_list_reverse (list);
	panel_widget_compress_pack_indexes_list (panel, list);
	g_list_free (list);
}

/* Note: this can only be called at the beginning of size_allocate, which means
 * that ad->position doesn't matter yet (it will be set to the correct
 * value in size_allocate). */
static void
panel_widget_update_positions_packed_start (PanelWidget *panel)
{
	GList *list,*l;
	AppletData *ad;
	int size_all = 0;
	int pos_next;

	if (panel->packed)
		return;

	list = get_applet_list_pack (panel, PANEL_OBJECT_PACK_START);

	/* get size used by the objects */
	for (l = list; l; l = l->next) {
		ad = l->data;
		size_all += ad->size;
	}

	/* update absolute position of all applets based on this information,
	 * starting with the first object */
	pos_next = 0;
	l = list;

	while (l) {
		ad = l->data;
		ad->position = pos_next;
		pos_next += ad->size;
		l = l->next;
	}

	g_list_free (list);
}

/* Note: only use this function when you can; see comment above
 * panel_widget_update_positions_packed_start()
 * For center specifically, we require ad->size to be set.
 */
static void
panel_widget_update_positions_packed_center (PanelWidget *panel)
{
	GList *list,*l;
	AppletData *ad;
	int size_all = 0;
	int pos_next;

	if (panel->packed)
		return;

	list = get_applet_list_pack (panel, PANEL_OBJECT_PACK_CENTER);

	/* get size used by the objects */
	for (l = list; l; l = l->next) {
		ad = l->data;
		size_all += ad->size;
	}

	/* update absolute position of all applets based on this information,
	 * starting with the first centered object */
	pos_next = (panel->size - size_all) / 2;
	l = list;

	while (l) {
		ad = l->data;
		ad->position = pos_next;
		pos_next += ad->size;
		l = l->next;
	}

	g_list_free (list);
}

/* Note: only use this function when you can; see comment above
 * panel_widget_update_positions_packed_start() */
static void
panel_widget_update_positions_packed_end (PanelWidget *panel)
{
	GList *list,*l;
	AppletData *ad;
	int size_all = 0;
	int pos_next;

	if (panel->packed)
		return;

	list = get_applet_list_pack (panel, PANEL_OBJECT_PACK_END);

	/* get size used by the objects */
	for (l = list; l; l = l->next) {
		ad = l->data;
		size_all += ad->size;
	}

	/* update absolute position of all applets based on this information,
	 * starting with the first object */
	pos_next = panel->size - size_all;
	l = list;

	while (l) {
		ad = l->data;
		ad->position = pos_next;
		pos_next += ad->size;
		l = l->next;
	}

	g_list_free (list);
}

static void
panel_widget_update_positions (PanelWidget *panel)
{
	int i = 0;
	GList *list;
	AppletData *ad;

	i = 0;

	if (panel->packed) {
		/* keep in sync with code in size_allocate */
		for (list = panel->applet_list;
		     list != NULL;
		     list = g_list_next (list)) {
			ad = list->data;
			ad->position = i;
			i += ad->size;
		}
	} else {
		/* Re-compute the ideal position of objects, based on their size */
		panel_widget_update_positions_packed_start (panel);
		panel_widget_update_positions_packed_center (panel);
		panel_widget_update_positions_packed_end (panel);

		/* Second pass: try to position from the start, to make sure
		 * there's enough room. We don't use size hints yet. */
		for (list = panel->applet_list;
		     list != NULL;
		     list = g_list_next (list)) {
			ad = list->data;
			if (ad->position < i)
				ad->position = i;

			i = ad->position + ad->size;
		}

		/* Third pass: try to push applets to the left to see if
		 * there is enough room. */
		if (i > panel->size) {
			i = panel->size;
			for (list = g_list_last (panel->applet_list);
			     list != NULL;
			     list = g_list_previous (list)) {
				ad = list->data;

				if (ad->position + ad->size > i)
					ad->position = MAX (i - ad->size, 0);

				i = ad->position;
			}
		}

		/* EEEEK, there's not enough room, so shift applets even
		 * at the expense of perhaps running out of room on the
		 * right if there is no free space in the middle */
		if (i < 0) {
			i = 0;
			for (list = panel->applet_list;
			     list != NULL;
			     list = g_list_next (list)) {
				ad = list->data;

				if (ad->position < i)
					ad->position = i;

				i = ad->position + ad->size;
			}
		}
	}
}

static inline int
panel_widget_get_moveby (PanelWidget *panel,
			 AppletData  *ad)
{
	/* move relative to the center of the object */
	return panel_widget_get_cursorloc (panel) - ad->position - ad->size / 2;
}

static int
panel_widget_move_get_pos_pack (PanelWidget *panel,
				PanelObjectPackType pack_type)
{
	switch (pack_type) {
	case PANEL_OBJECT_PACK_START:
		return 0;
		break;
	case PANEL_OBJECT_PACK_CENTER:
		return panel->size / 2;
		break;
	case PANEL_OBJECT_PACK_END:
		return panel->size;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return 0;
}

static int
panel_widget_move_get_pos_next_pack (PanelWidget *panel,
				     AppletData  *ad,
				     AppletData  *nad)
{
	if (!nad || nad->pack_type > ad->pack_type + 1)
		return panel_widget_move_get_pos_pack (panel, ad->pack_type + 1);

	return nad->position;
}

static int
panel_widget_move_get_pos_prev_pack (PanelWidget *panel,
				     AppletData  *ad,
				     AppletData  *pad)
{
	if (!pad || pad->pack_type < ad->pack_type - 1)
		return panel_widget_move_get_pos_pack (panel, ad->pack_type - 1);

	return pad->position + pad->size;
}

static void
panel_widget_move_to_pack (PanelWidget         *panel,
			   AppletData          *ad,
			   PanelObjectPackType  new_pack_type,
			   int                  pack_index)
{
	GList *l;

	if (pack_index >= 0) {
		for (l = panel->applet_list; l; l = l->next) {
			AppletData *ad_to_move = l->data;
			if (ad_to_move->pack_type == new_pack_type &&
			    ad_to_move->pack_index >= pack_index) {
				ad_to_move->pack_index++;
				emit_applet_moved (panel, ad_to_move);
			}
		}
	} else
		pack_index = panel_widget_get_new_pack_index (panel, new_pack_type);

	ad->pack_type = new_pack_type;
	ad->pack_index = pack_index;
}

/*
 * Switch move
 */

static GList *
panel_g_list_swap_next (GList *list,
                        GList *dl)
{
	GList *t;

	if (!dl)
		return list;

	if (!dl->next)
		return list;

	if (dl->prev)
		dl->prev->next = dl->next;
	t = dl->prev;
	dl->prev = dl->next;
	dl->next->prev = t;
	if (dl->next->next)
		dl->next->next->prev = dl;
	t = dl->next->next;
	dl->next->next = dl;
	dl->next = t;

	if (list == dl)
		return dl->prev;

	return list;
}

/* if force_switch is set, moveby will be ignored */
static gboolean
panel_widget_switch_applet_right (PanelWidget *panel,
				  GList       *list,
				  int          moveby,
				  gboolean     force_switch)
{
	AppletData *ad;
	AppletData *nad;
	int         swap_index;
	int         next_pos;

	ad = list->data;

	if (panel->packed && !list->next)
		return FALSE;

	if (ad->pack_type == PANEL_OBJECT_PACK_END && !list->next)
		return FALSE;

	/* count moveby from end of object => remove distance to go there */
	moveby -= ad->size / 2;

	nad = list->next ? list->next->data : NULL;

	/* Move inside same pack */
	if (nad && nad->pack_type == ad->pack_type) {
		if (force_switch ||
		    (moveby >= nad->size / 2)) {
			swap_index = ad->pack_index;
			ad->pack_index = nad->pack_index;
			nad->pack_index = swap_index;

			panel->applet_list = panel_g_list_swap_next (panel->applet_list, list);

			emit_applet_moved (panel, nad);
			emit_applet_moved (panel, ad);

			panel_widget_update_positions (panel);
			gtk_widget_queue_resize (GTK_WIDGET (panel));

			return TRUE;
		} else
			return FALSE;
	}

	/* Move to next pack */
	next_pos = panel_widget_move_get_pos_next_pack (panel, ad, nad);
	if (force_switch ||
	    (moveby >= (next_pos - (ad->position + ad->size)) / 2)) {
		if (ad->pack_type + 1 == PANEL_OBJECT_PACK_END)
			panel_widget_move_to_pack (panel, ad, ad->pack_type + 1, -1);
		else
			panel_widget_move_to_pack (panel, ad, ad->pack_type + 1, 0);

		emit_applet_moved (panel, ad);

		panel_widget_update_positions (panel);
		gtk_widget_queue_resize (GTK_WIDGET (panel));

		return TRUE;
	}

	return FALSE;
}

static GList *
panel_g_list_swap_prev (GList *list,
                        GList *dl)
{
	GList *t;

	if (!dl)
		return list;

	if (!dl->prev)
		return list;

	if (dl->next)
		dl->next->prev = dl->prev;
	t = dl->next;
	dl->next = dl->prev;
	dl->prev->next = t;
	if (dl->prev->prev)
		dl->prev->prev->next = dl;
	t = dl->prev->prev;
	dl->prev->prev = dl;
	dl->prev = t;

	if (list == dl->next)
		return dl;

	return list;
}

/* if force_switch is set, moveby will be ignored */
static gboolean
panel_widget_switch_applet_left (PanelWidget *panel,
				 GList       *list,
				 int          moveby,
				 gboolean     force_switch)
{
	AppletData *ad;
	AppletData *pad;
	int         swap_index;
	int         prev_pos;

	ad = list->data;

	if (panel->packed && !list->prev)
		return FALSE;

	if (ad->pack_type == PANEL_OBJECT_PACK_START && !list->prev)
		return FALSE;

	/* count moveby from start of object => add distance to go there */
	moveby += ad->size / 2;

	pad = list->prev ? list->prev->data : NULL;

	/* Move inside same pack */
	if (pad && pad->pack_type == ad->pack_type) {
		if (force_switch ||
		    (moveby <= - pad->size / 2)) {
			swap_index = ad->pack_index;
			ad->pack_index = pad->pack_index;
			pad->pack_index = swap_index;

			panel->applet_list = panel_g_list_swap_prev (panel->applet_list, list);

			emit_applet_moved (panel, ad);
			emit_applet_moved (panel, pad);

			panel_widget_update_positions (panel);
			gtk_widget_queue_resize (GTK_WIDGET (panel));

			return TRUE;
		} else
			return FALSE;
	}

	/* Move to prev pack */
	prev_pos = panel_widget_move_get_pos_prev_pack (panel, ad, pad);
	if (force_switch ||
	    (moveby <= - ((ad->position - prev_pos) / 2))) {
		panel_widget_move_to_pack (panel, ad, ad->pack_type - 1, -1);
		emit_applet_moved (panel, ad);

		panel_widget_update_positions (panel);
		gtk_widget_queue_resize (GTK_WIDGET (panel));

		return TRUE;
	}

	return FALSE;
}

static void
panel_widget_switch_move (PanelWidget *panel,
			  AppletData  *ad)
{
	GList *list;
	gboolean moved;
	int      moveby;

	g_return_if_fail (ad != NULL);
	g_return_if_fail (PANEL_IS_WIDGET (panel));

	list = g_list_find (panel->applet_list, ad);
	g_return_if_fail (list != NULL);

	moveby = panel_widget_get_moveby (panel, ad);

	if (moveby > ad->size / 2) {
		moved = TRUE;
		while (moved && moveby > ad->size / 2) {
			moved = panel_widget_switch_applet_right (panel, list,
								  moveby, FALSE);
			moveby = panel_widget_get_moveby (panel, ad);
		}
	} else {
		moved = TRUE;
		while (moved && moveby < - ad->size / 2) {
			moved = panel_widget_switch_applet_left (panel, list,
								 moveby, FALSE);
			moveby = panel_widget_get_moveby (panel, ad);
		}
	}
}

static int
panel_widget_push_applet_right (PanelWidget *panel,
				GList       *list,
				int          moveby,
				gboolean     force_switch)
{
	AppletData *ad;
	AppletData *nad;
	PanelObjectPackType new_pack_type;
	GList *l;
	GList *last_in_pack;
	int next_pos;

	ad = list->data;

	if (ad->pack_type == PANEL_OBJECT_PACK_END)
		return FALSE;

	/* count moveby from end of object => remove distance to go there */
	moveby -= ad->size / 2;

	new_pack_type = ad->pack_type + 1;

	for (l = list; l && l->next; l = l->next) {
		nad = l->next->data;
		if (nad->pack_type != ad->pack_type)
			break;
	}

	last_in_pack = l;

	nad = last_in_pack->next ? last_in_pack->next->data : NULL;
	next_pos = panel_widget_move_get_pos_next_pack (panel, ad, nad);

	if (!force_switch &&
	    (moveby < (next_pos - (ad->position + ad->size)) / 2))
		return FALSE;

	for (l = last_in_pack; l; l = l->prev) {
		ad = l->data;

		if (new_pack_type == PANEL_OBJECT_PACK_END)
			panel_widget_move_to_pack (panel, ad, new_pack_type, -1);
		else
			panel_widget_move_to_pack (panel, ad, new_pack_type, 0);

		emit_applet_moved (panel, ad);

		if (l == list)
			break;
	}

	panel_widget_update_positions (panel);
	gtk_widget_queue_resize (GTK_WIDGET (panel));

	return TRUE;
}

static int
panel_widget_push_applet_left (PanelWidget *panel,
			       GList       *list,
			       int          moveby,
			       gboolean     force_switch)
{
	AppletData *ad;
	AppletData *pad;
	PanelObjectPackType new_pack_type;
	GList *l;
	GList *first_in_pack;
	int prev_pos;

	ad = list->data;

	if (ad->pack_type == PANEL_OBJECT_PACK_START)
		return FALSE;

	/* count moveby from start of object => add distance to go there */
	moveby += ad->size / 2;

	new_pack_type = ad->pack_type - 1;

	for (l = list; l && l->prev; l = l->prev) {
		pad = l->prev->data;
		if (pad->pack_type != ad->pack_type)
			break;
	}

	first_in_pack = l;

	pad = first_in_pack->prev ? first_in_pack->prev->data : NULL;
	prev_pos = panel_widget_move_get_pos_prev_pack (panel, ad, pad);

	if (!force_switch &&
	    (moveby > - ((ad->position - prev_pos) / 2)))
		return FALSE;

	for (l = first_in_pack; l; l = l->next) {
		ad = l->data;

		panel_widget_move_to_pack (panel, ad, new_pack_type, -1);
		emit_applet_moved (panel, ad);

		if (l == list)
			break;
	}

	panel_widget_update_positions (panel);
	gtk_widget_queue_resize (GTK_WIDGET (panel));

	return TRUE;
}

static void
panel_widget_push_move (PanelWidget *panel,
			AppletData  *ad,
			int          direction)
{
	GList *list;
	gboolean moved;
	int      moveby;

	/* direction is only used when we move with keybindings */

	g_return_if_fail (ad != NULL);
	g_return_if_fail (PANEL_IS_WIDGET (panel));

	list = g_list_find (panel->applet_list, ad);
	g_return_if_fail (list != NULL);

	moveby = panel_widget_get_moveby (panel, ad);

	if (direction > 0 || moveby > ad->size / 2) {
		moved = TRUE;
		while (direction > 0 ||
		       (moved && moveby > ad->size / 2)) {
			moved = panel_widget_push_applet_right (panel, list,
								moveby,
								direction != 0);
			moveby = panel_widget_get_moveby (panel, ad);

			/* a keybinding pushes only once */
			if (direction != 0)
				break;
		}
	} else {
		moved = TRUE;
		while (direction < 0 ||
		       (moved && moveby < ad->size / 2)) {
			moved = panel_widget_push_applet_left (panel, list,
							       moveby,
							       direction != 0);
			moveby = panel_widget_get_moveby (panel, ad);

			/* a keybinding pushes only once */
			if (direction != 0)
				break;
		}
	}
}

static void
queue_resize_on_all_applets(PanelWidget *panel)
{
	GList *li;
	for(li = panel->applet_list; li != NULL;
	    li = g_list_next(li)) {
		AppletData *ad = li->data;
		gtk_widget_queue_resize (ad->applet);
	}
}

static void
get_preferred_size (PanelWidget    *self,
                    AppletData     *ad,
                    GtkRequisition *minimum_size,
                    GtkRequisition *natural_size)
{
  if (self->orient == GTK_ORIENTATION_HORIZONTAL)
    {
      if (ad->expand_minor)
        {
          minimum_size->height = self->sz;
          natural_size->height = self->sz;
        }
      else
        {
          gtk_widget_get_preferred_height (ad->applet,
                                           &minimum_size->height,
                                           &natural_size->height);
        }

      gtk_widget_get_preferred_width_for_height (ad->applet,
                                                 minimum_size->height,
                                                 &minimum_size->width,
                                                 &natural_size->width);

      if (!ad->expand_major)
        natural_size->width = minimum_size->width;
    }
  else if (self->orient == GTK_ORIENTATION_VERTICAL)
    {
      if (ad->expand_minor)
        {
          minimum_size->width = self->sz;
          natural_size->width = self->sz;
        }
      else
        {
          gtk_widget_get_preferred_width (ad->applet,
                                          &minimum_size->width,
                                          &natural_size->width);
        }

      gtk_widget_get_preferred_height_for_width (ad->applet,
                                                 minimum_size->width,
                                                 &minimum_size->height,
                                                 &natural_size->height);

      if (!ad->expand_major)
        natural_size->height = minimum_size->height;
    }
  else
    {
      g_assert_not_reached ();
    }
}

static void
panel_widget_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  PanelWidget *self;
  int remaining_space;
  GArray *requested_sizes;
  GList *l;
  guint i;
  gboolean ltr;

  self = PANEL_WIDGET (widget);

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }

  if (self->orient == GTK_ORIENTATION_HORIZONTAL)
    self->size = allocation->width;
  else
    self->size = allocation->height;

  remaining_space = self->size;
  requested_sizes = g_array_new (FALSE, FALSE, sizeof (GtkRequestedSize));

  for (l = self->applet_list; l != NULL; l = g_list_next (l))
    {
      AppletData *ad;
      GtkRequisition minimum;
      GtkRequisition natural;
      GtkRequestedSize request;

      ad = l->data;

      get_preferred_size (self, ad, &minimum, &natural);

      if (self->orient == GTK_ORIENTATION_HORIZONTAL)
        {
          request.minimum_size = minimum.width;
          request.natural_size = natural.width;
        }
      else
        {
          request.minimum_size = minimum.height;
          request.natural_size = natural.height;
        }

      request.data = ad;
      remaining_space -= request.minimum_size;

      g_array_append_val (requested_sizes, request);
    }

  gtk_distribute_natural_allocation (remaining_space,
                                     requested_sizes->len,
                                     (GtkRequestedSize *) requested_sizes->data);

  for (i = 0; i < requested_sizes->len; i++)
    {
      GtkRequestedSize *request;
      AppletData *ad;

      request = &g_array_index (requested_sizes, GtkRequestedSize, i);
      ad = request->data;

      ad->size = request->minimum_size;
    }

  g_array_free (requested_sizes, TRUE);
  panel_widget_update_positions (self);

  ltr = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR;

  for (l = self->applet_list; l != NULL; l = g_list_next (l))
    {
      AppletData *ad;
      GtkRequisition minimum;
      GtkRequisition natural;
      GtkAllocation challoc;

      ad = l->data;

      get_preferred_size (self, ad, &minimum, &natural);

      if (self->orient == GTK_ORIENTATION_HORIZONTAL)
        {
          challoc.width = ad->size;
          challoc.height = minimum.height;

          challoc.x = ltr ? ad->position : self->size - ad->position - challoc.width;
          challoc.y = allocation->height / 2 - challoc.height / 2;
        }
      else
        {
          challoc.width = minimum.width;
          challoc.height = ad->size;

          challoc.x = allocation->width / 2 - challoc.width / 2;
          challoc.y = ad->position;
        }

      gtk_widget_size_allocate (ad->applet, &challoc);
    }
}

gboolean
panel_widget_is_cursor(PanelWidget *panel, int overlap)
{
	GtkWidget       *widget;
	GtkAllocation   allocation;
	GdkSeat         *seat;
	GdkDevice       *device;
	int             x,y;
	int             w,h;

	g_return_val_if_fail(PANEL_IS_WIDGET(panel),FALSE);

	widget = panel->drop_widget;
	
	if(!widget ||
	   !GTK_IS_WIDGET(widget) ||
	   !gtk_widget_get_visible(widget))
		return FALSE;

	seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
	device = gdk_seat_get_pointer (seat);
	gdk_window_get_device_position(gtk_widget_get_window (widget), device, &x, &y, NULL);

	gtk_widget_get_allocation (widget, &allocation);
	w = allocation.width;
	h = allocation.height;

	if((x+overlap)>=0 &&
	   (x-overlap)<=w &&
	   (y+overlap)>=0 &&
	   (y-overlap)<=h)
		return TRUE;
	return FALSE;
}

static void
panel_widget_open_dialog_destroyed (PanelWidget *panel_widget,
				    GtkWidget   *dialog)
{
	g_return_if_fail (panel_widget->open_dialogs != NULL);

	panel_widget->open_dialogs = g_slist_remove (panel_widget->open_dialogs, dialog);
}

static void
panel_widget_destroy_open_dialogs (PanelWidget *panel_widget)
{
	GSList *l, *list;

	list = panel_widget->open_dialogs;
	panel_widget->open_dialogs = NULL;

	for (l = list; l; l = l->next) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (l->data),
				G_CALLBACK (panel_widget_open_dialog_destroyed),
				panel_widget);
		gtk_widget_destroy (l->data);
	}
	g_slist_free (list);

}

static void
panel_widget_dispose (GObject *obj)
{
	PanelWidget *panel = PANEL_WIDGET (obj);

	panels = g_slist_remove (panels, panel);

	panel_widget_destroy_open_dialogs (panel);

        G_OBJECT_CLASS (panel_widget_parent_class)->dispose (obj);
}

static void
panel_widget_init (PanelWidget *panel)
{
	GtkWidget *widget = (GtkWidget *) panel;

	gtk_widget_set_events (
		widget,
		gtk_widget_get_events (widget) | GDK_BUTTON_RELEASE_MASK);
	
	panel->packed        = FALSE;
	panel->orient        = GTK_ORIENTATION_HORIZONTAL;
	panel->size          = 0;
	panel->applet_list   = NULL;
	panel->drop_widget   = widget;
	panel->open_dialogs  = NULL;

	panels = g_slist_append (panels, panel);
}

GtkWidget *
panel_widget_new (PanelToplevel  *toplevel,
		  gboolean        packed,
		  GtkOrientation  orient,
		  int             sz)
{
	PanelWidget *panel;

	panel = g_object_new (PANEL_TYPE_WIDGET, NULL);

	gtk_widget_set_has_window (GTK_WIDGET (panel), TRUE);
	gtk_widget_set_can_focus (GTK_WIDGET (panel), TRUE);

	panel->orient = orient;
	panel->sz = sz;

	panel->packed = packed;
	panel->size = 0;

	panel->toplevel    = toplevel;
	panel->drop_widget = GTK_WIDGET (toplevel);

	return GTK_WIDGET (panel);
}

static guint moving_timeout = 0;
static gboolean been_moved = FALSE;
static gboolean repeat_if_outside = FALSE;

static gboolean
panel_widget_applet_drag_start_no_grab (PanelWidget *panel,
					GtkWidget *applet)
{
	AppletData *ad;
	AppletInfo *info;

	g_return_val_if_fail (PANEL_IS_WIDGET (panel), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (panel), FALSE);

	ad = g_object_get_data (G_OBJECT (applet), PANEL_APPLET_DATA);
	g_return_val_if_fail (ad != NULL, FALSE);

	/* Check if we can actually move this object in the
	   configuration */
	info = g_object_get_data (G_OBJECT (applet), "applet_info");
	if (info != NULL &&
	    ! panel_applet_can_freely_move (info))
		return FALSE;

	if (moving_timeout != 0) {
		g_source_remove (moving_timeout);
		moving_timeout = 0;
		been_moved = FALSE;
	}

#ifdef PANEL_WIDGET_DEBUG
	g_message("Starting drag on a %s at %p\n",
		  g_type_name(G_TYPE_FROM_INSTANCE (applet)), applet);
#endif
	panel->currently_dragged_applet = ad;

	add_all_move_bindings (panel);

	panel_applet_in_drag = TRUE;
	
	return TRUE;
}


static void
panel_widget_applet_drag_end_no_grab (PanelWidget *panel)
{
	g_return_if_fail (panel != NULL);
	g_return_if_fail (PANEL_IS_WIDGET (panel));

#ifdef PANEL_WIDGET_DEBUG
	g_message("Ending drag\n");
#endif
	panel->currently_dragged_applet = NULL;
	panel_applet_in_drag = FALSE;

	remove_all_move_bindings (panel);
	if (moving_timeout != 0) {
		g_source_remove (moving_timeout);
		moving_timeout = 0;
		been_moved = FALSE;
	}

	/* Make sure we keep our indexes in a 0:n range, instead of a x:x+n
	 * range, with a growing x. Note that this is useful not only because
	 * of moves, but also because of removal of objects (object 0 could be
	 * removed, but if there is still object 1, we start growing the
	 * range). But doing this compress only once in a while, after moving
	 * objects is good enough, since this is nothing urgent and the user
	 * will move objects before this becomes a real annoying issue. */
	panel_widget_compress_pack_indexes (panel);
}

void
panel_widget_applet_drag_start (PanelWidget *panel,
				GtkWidget   *applet,
				guint32      time_)
{
	GdkWindow *window;

	g_return_if_fail (PANEL_IS_WIDGET (panel));
	g_return_if_fail (GTK_IS_WIDGET (applet));

#ifdef PANEL_WIDGET_DEBUG
	g_message("Starting drag [grabbed] on a %s at %p\n",
		  g_type_name(G_TYPE_FROM_INSTANCE(applet)), applet);
#endif

	if (!panel_widget_applet_drag_start_no_grab (panel, applet))
		return;

	panel_toplevel_push_autohide_disabler (panel->toplevel);

	gtk_grab_add (applet);

	window = gtk_widget_get_window (applet);
	if (window) {
		GdkGrabStatus  status;
		GdkDisplay    *display;
		GdkCursor     *fleur_cursor;
		GdkSeat       *seat;

		display = gdk_display_get_default ();
		fleur_cursor = gdk_cursor_new_for_display (display, GDK_FLEUR);
		seat = gdk_display_get_default_seat (display);

		status = gdk_seat_grab (seat, window, GDK_SEAT_CAPABILITY_POINTER,
		                        FALSE, fleur_cursor, NULL, NULL, NULL);

		g_object_unref (fleur_cursor);
		gdk_display_flush (display);

		if (status != GDK_GRAB_SUCCESS) {
			g_warning (G_STRLOC ": failed to grab pointer (errorcode: %d)",
				   status);
			panel_widget_applet_drag_end (panel);
		}
	}
}

void
panel_widget_applet_drag_end (PanelWidget *panel)
{
	GdkDisplay *display;
	GdkSeat *seat;

	g_return_if_fail (PANEL_IS_WIDGET (panel));

	if (panel->currently_dragged_applet == NULL)
		return;

	display = gtk_widget_get_display (GTK_WIDGET (panel));
	seat = gdk_display_get_default_seat (display);

	gdk_seat_ungrab (seat);
	gtk_grab_remove (panel->currently_dragged_applet->applet);
	panel_widget_applet_drag_end_no_grab (panel);
	panel_toplevel_pop_autohide_disabler (panel->toplevel);
	gdk_display_flush (display);
}

/*get pos of the cursor location in panel coordinates*/
int
panel_widget_get_cursorloc (PanelWidget *panel)
{
	int             x, y;
	gboolean        rtl;
	GdkSeat        *seat;
	GdkDevice      *device;

	g_return_val_if_fail (PANEL_IS_WIDGET (panel), -1);

	seat = gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (panel)));
	device = gdk_seat_get_pointer (seat);
	gdk_window_get_device_position(gtk_widget_get_window (GTK_WIDGET (panel)), device, &x, &y, NULL);
	rtl = gtk_widget_get_direction (GTK_WIDGET (panel)) == GTK_TEXT_DIR_RTL;
	
	if (panel->orient == GTK_ORIENTATION_HORIZONTAL)
		return (rtl ? panel->size - x : x);
	else
		return y;
}

/* get pack type & index for insertion at the cursor location in panel */
void
panel_widget_get_insert_at_cursor (PanelWidget         *widget,
				   PanelObjectPackType *pack_type,
				   int                 *pack_index)
{
	int         pos;
	GList      *l;
	AppletData *ad;

	g_return_if_fail (PANEL_IS_WIDGET (widget));

	pos = panel_widget_get_cursorloc (widget);

	/* check if cursor is in an object; in this case, return the pack type
	 * of the object */
	for (l = widget->applet_list; l; l = l->next) {
		ad = l->data;

		if (ad->position <= pos) {
			if (ad->position + ad->size > pos) {
				*pack_type = ad->pack_type;
				*pack_index = ad->pack_index;
			}
		} else
			break;
	}

	if (pos <= widget->size / 2)
		*pack_type = PANEL_OBJECT_PACK_START;
	else
		*pack_type = PANEL_OBJECT_PACK_END;

	*pack_index = panel_widget_get_new_pack_index (widget, *pack_type);
}

/* get index for insertion with pack type */
int
panel_widget_get_new_pack_index (PanelWidget         *panel,
				 PanelObjectPackType  pack_type)
{
	GList      *l;
	AppletData *ad;
	int         max_pack_index = -1;

	for (l = panel->applet_list; l; l = l->next) {
		ad = l->data;
		if (ad->pack_type == pack_type)
			max_pack_index = MAX (max_pack_index, ad->pack_index);
	}

	return max_pack_index + 1;
}

/* schedule to run the below function */
static void schedule_try_move (PanelWidget *panel, gboolean repeater);

/*find the cursor position and move the applet to that position*/
static void
panel_widget_applet_move_to_cursor (PanelWidget *panel)
{
	int movement;
	GtkWidget *applet;
	AppletData *ad;

	g_return_if_fail(PANEL_IS_WIDGET(panel));

	if (panel->currently_dragged_applet == NULL)
		return;

	ad = panel->currently_dragged_applet;

	applet = ad->applet;
	g_assert(GTK_IS_WIDGET(applet));

	if(!panel_widget_is_cursor(panel,10) &&
	   !panel_lockdown_get_panels_locked_down_s ()) {
		GSList *list;

		for(list=panels;
		    list!=NULL;
		    list=g_slist_next(list)) {
			PanelWidget *new_panel =
				PANEL_WIDGET(list->data);

			if (panel != new_panel &&
			    panel_widget_is_cursor (new_panel,10)) {
				PanelObjectPackType pack_type = PANEL_OBJECT_PACK_START;
				int                 pack_index = 0;

				panel_widget_get_insert_at_cursor (new_panel,
								   &pack_type,
								   &pack_index);

				panel_widget_applet_drag_end (panel);

				/*disable reentrancy into this function*/
				if (!panel_widget_reparent (panel, new_panel, applet,
							    pack_type, pack_index)) {
					panel_widget_applet_drag_start (
						panel, applet, GDK_CURRENT_TIME);
					continue;
				}

				panel_widget_applet_drag_start (
					new_panel, applet, GDK_CURRENT_TIME);
				schedule_try_move (new_panel, TRUE);

				return;
			}
		}
	}

	movement = PANEL_SWITCH_MOVE;

	if (panel->packed) {
		movement = PANEL_SWITCH_MOVE;
	} else {
		if (panel->dragged_state & GDK_CONTROL_MASK)
			movement = PANEL_SWITCH_MOVE;
		else if (panel->dragged_state & GDK_SHIFT_MASK)
			movement = PANEL_PUSH_MOVE;
	}
	
	switch (movement) {
	case PANEL_SWITCH_MOVE:
		panel_widget_switch_move (panel, ad);
		break;
	case PANEL_PUSH_MOVE:
		panel_widget_push_move (panel, ad, 0);
		break;
	default:
		break;
	}
}

static int
move_timeout_handler(gpointer data)
{
	PanelWidget   *panel = data;

	g_return_val_if_fail(PANEL_IS_WIDGET(data),FALSE);

	if(been_moved &&
	   panel->currently_dragged_applet) {
		panel_widget_applet_move_to_cursor(panel);
		been_moved = FALSE;
		return TRUE;
	}
	been_moved = FALSE;

	if(panel->currently_dragged_applet && repeat_if_outside) {
		GtkWidget       *widget;
		GtkAllocation   allocation;
		GdkSeat         *seat;
		GdkDevice       *device;
		int             x,y;
		int             w,h;

		widget = panel->currently_dragged_applet->applet;

		seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
		device = gdk_seat_get_pointer (seat);
		gdk_window_get_device_position(gtk_widget_get_window (widget), device, &x, &y, NULL);

		gtk_widget_get_allocation (widget, &allocation);
		w = allocation.width;
		h = allocation.height;

		/* if NOT inside return TRUE, this means we will be
		 * kept inside the timeout until we hit the damn widget
		 * or the drag ends */
		if(!(x>=0 && x<=w && y>=0 && y<=h))
			return TRUE;
	}

	moving_timeout = 0;
	
	return FALSE;
}

static void
schedule_try_move(PanelWidget *panel, gboolean repeater)
{
	if (!panel->currently_dragged_applet)
		return;
	repeat_if_outside = repeater;
	if(moving_timeout == 0) {
		been_moved = FALSE;
		panel_widget_applet_move_to_cursor(panel);
		moving_timeout =
			g_timeout_add (50, move_timeout_handler, panel);
	} else
		been_moved = TRUE;
}

static gboolean
panel_widget_applet_button_press_event (GtkWidget      *widget,
					GdkEventButton *event)
{
	GtkWidget   *parent;
	PanelWidget *panel;
	guint        modifiers;
	guint32      event_time;

	parent = gtk_widget_get_parent (widget);

	g_return_val_if_fail (PANEL_IS_WIDGET (parent), FALSE);

	panel = PANEL_WIDGET (parent);

	/* don't propagate this event */
	if (panel->currently_dragged_applet) {
		g_signal_stop_emission (G_OBJECT (widget), 
					g_signal_lookup ("button-press-event",
							 G_OBJECT_TYPE (widget)),
					0);
		return TRUE;
	}

	modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

	/* Begin drag if the middle mouse button and modifier are pressed,
	 * unless the panel is locked down or a grab is active (meaning a menu
	 * is open) */
	if (panel_lockdown_get_panels_locked_down_s () ||
	    event->button != 2 ||
	    modifiers != panel_bindings_get_mouse_button_modifier_keymask () ||
	    gtk_grab_get_current() != NULL)
		return FALSE;

	/* time on sent events seems to be bogus */
	event_time = event->time;
	if (event->send_event)
		event_time = GDK_CURRENT_TIME;

	panel_widget_applet_drag_start (panel, widget, event_time);

	return TRUE;
}

static gboolean
panel_widget_applet_button_release_event (GtkWidget      *widget,
					  GdkEventButton *event)
{
	GtkWidget   *parent;
	PanelWidget *panel;

	parent = gtk_widget_get_parent (widget);

	g_return_val_if_fail (PANEL_IS_WIDGET (parent), FALSE);

	panel = PANEL_WIDGET (parent);
	
	/* don't propagate this event */
	if (panel->currently_dragged_applet) {
		g_signal_stop_emission (G_OBJECT (widget),
					g_signal_lookup ("button-release-event",
							 G_OBJECT_TYPE (widget)),
					0);
		panel_widget_applet_drag_end (panel);
		return TRUE;
	}

	return FALSE;
}

static gboolean
panel_widget_applet_motion_notify_event (GtkWidget *widget,
					 GdkEvent  *event)
{
	GtkWidget   *parent;
	PanelWidget *panel;

	parent = gtk_widget_get_parent (widget);

	g_return_val_if_fail (PANEL_IS_WIDGET (parent), FALSE);

	if (gdk_event_get_screen (event) != gtk_widget_get_screen (widget))
		return FALSE;

	panel = PANEL_WIDGET (parent);
	panel->dragged_state = ((GdkEventMotion *) event)->state & GDK_MODIFIER_MASK;
	
	schedule_try_move (panel, FALSE);

	return FALSE;
}

static gboolean
panel_widget_applet_key_press_event (GtkWidget   *widget,
				     GdkEventKey *event)
{
	GtkWidget   *parent;
	PanelWidget *panel;
	
	parent = gtk_widget_get_parent (widget);

	g_return_val_if_fail (PANEL_IS_WIDGET (parent), FALSE);

	panel = PANEL_WIDGET (parent);

	if (!panel_applet_in_drag)
		return FALSE;
	
	return gtk_bindings_activate (G_OBJECT (panel),
				      ((GdkEventKey *)event)->keyval, 
				      ((GdkEventKey *)event)->state);	
}

static int
panel_sub_event_handler(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	g_return_val_if_fail(GTK_IS_WIDGET(widget),FALSE);
	g_return_val_if_fail(event!=NULL,FALSE);

	/*pass these to the parent!*/
	if (event->type == GDK_BUTTON_PRESS ||
	    event->type == GDK_BUTTON_RELEASE ||
	    event->type == GDK_MOTION_NOTIFY) {
		GdkEventButton *bevent = (GdkEventButton *) event;

		if (bevent->button != 1 || panel_applet_in_drag)
			return gtk_widget_event (data, event);
	} else if (event->type == GDK_KEY_PRESS) {
		if (panel_applet_in_drag)
			return gtk_widget_event(data, event);
	}

	return FALSE;
}


static void
bind_applet_events(GtkWidget *widget, gpointer data)
{
	g_return_if_fail(GTK_IS_WIDGET(widget));

	/* XXX: This is more or less a hack.  We need to be able to
	 * capture events over applets so that we can drag them with
	 * the mouse and such.  So we need to force the applet's
	 * widgets to recursively send the events back to their parent
	 * until the event gets to the applet wrapper (the
	 * GtkEventBox) for processing by us.
	 */
	
	if (gtk_widget_get_has_window (widget))
		g_signal_connect (G_OBJECT(widget), "event",
				  G_CALLBACK (panel_sub_event_handler),
				  data);
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, data);
}

static void
panel_widget_applet_destroy (GtkWidget *applet, gpointer data)
{
	AppletData *ad;
	GtkWidget  *parent;

	g_return_if_fail (GTK_IS_WIDGET (applet));

	ad = g_object_get_data (G_OBJECT (applet), PANEL_APPLET_DATA);
	g_object_set_data (G_OBJECT (applet), PANEL_APPLET_DATA, NULL);

	parent = gtk_widget_get_parent (applet);
	/*if it wasn't yet removed*/
	if(parent) {
		PanelWidget *panel = PANEL_WIDGET (parent);

		if (panel->currently_dragged_applet == ad)
			panel_widget_applet_drag_end (panel);

		panel->applet_list = g_list_remove (panel->applet_list,ad);
	}

	g_free (ad);
}

static void
bind_top_applet_events (GtkWidget *widget)
{
	g_return_if_fail(GTK_IS_WIDGET(widget));

	g_signal_connect (G_OBJECT(widget), "destroy",
			  G_CALLBACK (panel_widget_applet_destroy),
			  NULL);

	g_signal_connect (widget, "button-press-event",
			  G_CALLBACK (panel_widget_applet_button_press_event),
			  NULL);

	g_signal_connect (widget, "button-release-event",
			  G_CALLBACK (panel_widget_applet_button_release_event),
			  NULL);
	g_signal_connect (widget, "motion-notify-event",
			  G_CALLBACK (panel_widget_applet_motion_notify_event),
			  NULL);
	g_signal_connect (widget, "key-press-event",
			  G_CALLBACK (panel_widget_applet_key_press_event),
			  NULL);

	/* XXX: This is more or less a hack.  We need to be able to
	 * capture events over applets so that we can drag them with
	 * the mouse and such.  So we need to force the applet's
	 * widgets to recursively send the events back to their parent
	 * until the event gets to the applet wrapper (the
	 * GtkEventBox) for processing by us.
	 */

	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, widget);
}

void
panel_widget_add (PanelWidget         *panel,
		  GtkWidget           *applet,
		  PanelObjectPackType  pack_type,
		  int                  pack_index,
		  gboolean             use_pack_index)
{
	AppletData *ad = NULL;

	g_return_if_fail (PANEL_IS_WIDGET (panel));
	g_return_if_fail (GTK_IS_WIDGET (applet));

	ad = g_object_get_data (G_OBJECT (applet), PANEL_APPLET_DATA);

	if (ad != NULL) {
		pack_type = ad->pack_type;
		pack_index = ad->pack_index;
	}

	if (!use_pack_index || pack_index < 0) {
		if (panel->packed) {
			/* add at the end of packed panels */
			AppletData *tmp;
			GList *list,*l;

			list = get_applet_list_pack (panel, PANEL_OBJECT_PACK_END);
			if (list) {
				for (l = list; l; l = l->next) {
					tmp = l->data;
					tmp->pack_index += 1;
					emit_applet_moved (panel, tmp);
				}
				pack_type = PANEL_OBJECT_PACK_END;
				pack_index = 0;
			} else {
				list = get_applet_list_pack (panel, PANEL_OBJECT_PACK_CENTER);
				if (!list)
					list = get_applet_list_pack (panel, PANEL_OBJECT_PACK_START);

				if (!list) {
					pack_type = PANEL_OBJECT_PACK_START;
					pack_index = 0;
				} else {
					l = g_list_last (list);
					tmp = l->data;
					pack_type = tmp->pack_type;
					pack_index = tmp->pack_index + 1;
				}
			}

			g_list_free (list);
		} else {
			GList *list,*l;

			/* only support adding to start/end packs if no index
			 * is provided */
			if (pack_type == PANEL_OBJECT_PACK_CENTER)
				pack_type = PANEL_OBJECT_PACK_START;

			list = get_applet_list_pack (panel, pack_type);

			l = NULL;
			if (pack_type == PANEL_OBJECT_PACK_START)
				l = g_list_last (list);
			else if  (pack_type == PANEL_OBJECT_PACK_END)
				l = list;
			else
				g_assert_not_reached ();

			if (!l)
				pack_index = 0;
			else {
				AppletData *tmp = l->data;
				pack_index = tmp->pack_index + 1;
			}

			g_list_free (list);
		}
	}

	if (ad == NULL) {
		ad = g_new (AppletData, 1);
		ad->applet = applet;
		ad->size = 1;
		ad->pack_type = pack_type;
		ad->pack_index = pack_index;
		ad->position = 0;
		ad->expand_major = FALSE;
		ad->expand_minor = FALSE;

		g_object_set_data (G_OBJECT (applet),
				   PANEL_APPLET_DATA, ad);
		
		/*this is a completely new applet, which was not yet bound*/
		bind_top_applet_events (applet);
	}

	panel->applet_list =
		g_list_insert_sorted(panel->applet_list,ad,
				     (GCompareFunc)applet_data_compare);

	/* the applet will be positioned on size allocate */
	gtk_fixed_put (GTK_FIXED (panel), applet, 0, 0);

	gtk_widget_queue_resize(GTK_WIDGET(panel));

	g_signal_emit (G_OBJECT(panel),
		       panel_widget_signals[APPLET_ADDED_SIGNAL],
		       0, applet);
}

gboolean
panel_widget_reparent (PanelWidget         *old_panel,
		       PanelWidget         *new_panel,
		       GtkWidget           *applet,
		       PanelObjectPackType  pack_type,
		       int                  pack_index)
{
	AppletData *ad;
	GtkWidget *focus_widget = NULL;
	AppletInfo* info;
	GdkDisplay *display;

	g_return_val_if_fail(PANEL_IS_WIDGET(old_panel), FALSE);
	g_return_val_if_fail(PANEL_IS_WIDGET(new_panel), FALSE);
	g_return_val_if_fail(GTK_IS_WIDGET(applet), FALSE);
	g_return_val_if_fail(pack_index>=0, FALSE);

	ad = g_object_get_data (G_OBJECT (applet), PANEL_APPLET_DATA);
	g_return_val_if_fail(ad!=NULL, FALSE);

	/* Don't try and reparent to an explicitly hidden panel,
	 * very confusing for the user ...
	 */
	if (panel_toplevel_get_is_hidden (new_panel->toplevel))
		return FALSE;
	
	info = g_object_get_data (G_OBJECT (ad->applet), "applet_info");

	panel_widget_move_to_pack (new_panel, ad, pack_type, pack_index);

	/* with reparent, we'll call cremove/cadd, which will reinsert ad at
	 * the right place in the list */

	gtk_widget_queue_resize (GTK_WIDGET (new_panel));
	gtk_widget_queue_resize (GTK_WIDGET (old_panel));

	panel_widget_reset_saved_focus (old_panel);
	if (gtk_container_get_focus_child (GTK_CONTAINER (old_panel)) == applet)
		focus_widget = gtk_window_get_focus (GTK_WINDOW (old_panel->toplevel));

	g_object_ref (applet);
	gtk_container_remove (GTK_CONTAINER (old_panel), applet);
	gtk_container_add (GTK_CONTAINER (new_panel), applet);
	g_object_unref (applet);

	if (info != NULL)
		panel_applet_frame_set_panel (PANEL_APPLET_FRAME (ad->applet), new_panel);

	if (gtk_widget_get_can_focus (GTK_WIDGET (new_panel)))
		gtk_widget_set_can_focus (GTK_WIDGET (new_panel), FALSE);
	if (focus_widget) {
		panel_widget_force_grab_focus (focus_widget);
	} else {
		gboolean return_val;

		g_signal_emit_by_name (applet, "focus",
				       GTK_DIR_TAB_FORWARD,
				       &return_val);
	}
 	gtk_window_present (GTK_WINDOW (new_panel->toplevel));

	display = gdk_display_get_default ();
	gdk_display_flush (display);

	emit_applet_moved (new_panel, ad);

	return TRUE;
}

void
panel_widget_set_packed (PanelWidget *panel_widget,
			 gboolean     packed)
{
	panel_widget->packed = packed;

	gtk_widget_queue_resize (GTK_WIDGET (panel_widget));
}

void
panel_widget_set_orientation (PanelWidget    *panel_widget,
			      GtkOrientation  orientation)
{
	panel_widget->orient = orientation;

	gtk_widget_queue_resize (GTK_WIDGET (panel_widget));
}

void
panel_widget_set_size (PanelWidget *panel_widget,
		       int          size)
{
	g_return_if_fail (PANEL_IS_WIDGET (panel_widget));

	if (size == panel_widget->sz)
		return;

	panel_widget->sz = size;

	queue_resize_on_all_applets (panel_widget);
	gtk_widget_queue_resize (GTK_WIDGET (panel_widget));
}

static void 
panel_widget_push_move_applet (PanelWidget     *panel,
                               GtkDirectionType dir)
{
	AppletData *applet;
	int         direction = 0;

	applet = panel->currently_dragged_applet;
	g_return_if_fail (applet);

	switch (dir) {
	case GTK_DIR_LEFT:
	case GTK_DIR_UP:
		direction = -1;
		break;
	case GTK_DIR_RIGHT:
	case GTK_DIR_DOWN:
		direction = 1;
		break;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_TAB_BACKWARD:
	default:
		return;
	}

	panel_widget_push_move (panel, applet, direction);
}

static void 
panel_widget_switch_move_applet (PanelWidget      *panel,
                                 GtkDirectionType  dir)
{
	AppletData *applet;
	GList      *list;

	applet = panel->currently_dragged_applet;
	g_return_if_fail (applet != NULL);

	list = g_list_find (panel->applet_list, applet);
	g_return_if_fail (list != NULL);

	switch (dir) {
	case GTK_DIR_LEFT:
	case GTK_DIR_UP:
		panel_widget_switch_applet_left (panel, list, -1, TRUE);
		break;
	case GTK_DIR_RIGHT:
	case GTK_DIR_DOWN:
		panel_widget_switch_applet_right (panel, list, -1, TRUE);
		break;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_TAB_BACKWARD:
	default:
		return;
	}
}

static void
panel_widget_tab_move (PanelWidget *panel,
                       gboolean     next)
{
	PanelWidget *new_panel = NULL;
	PanelWidget *previous_panel = NULL;
	AppletData  *ad;
	GSList      *l;

	if (panel_lockdown_get_panels_locked_down_s ())
		return;

	ad = panel->currently_dragged_applet;

	if (!ad)
		return;	
	
	for (l = panels; l; l = l->next) {
		PanelWidget *panel_in_list = l->data;

		if (panel_in_list == panel) {
			if (next) {
				if (l->next)
					new_panel = l->next->data;
				else
					new_panel = ((GSList *)panels)->data;
				
			} else {
				if (previous_panel)
					new_panel = previous_panel;
				else
					continue;
			}
			break;
		} else {		
			if (!next)
				previous_panel = panel_in_list;
		}
	}

	g_return_if_fail (l != NULL);

	if (!new_panel && previous_panel)
		new_panel = previous_panel;
	
	if (new_panel &&
	    (new_panel != panel))
		panel_widget_reparent (panel, new_panel, ad->applet,
				       PANEL_OBJECT_PACK_START, 0);
}

static void
panel_widget_end_move (PanelWidget *panel)
{
	panel_widget_applet_drag_end (panel);
}

static gboolean
panel_widget_real_focus (GtkWidget        *widget,
                         GtkDirectionType  direction)
{
	if (gtk_widget_get_can_focus (widget) && gtk_container_get_children (GTK_CONTAINER (widget))) {
		gtk_widget_set_can_focus (widget, FALSE);
	}
	return GTK_WIDGET_CLASS (panel_widget_parent_class)->focus (widget, direction);
}

void 
panel_widget_focus (PanelWidget *panel_widget)
{
	/*
         * Set the focus back on the panel; we unset the focus child so that
	 * the next time focus is inside the panel we do not remember the
	 * previously focused child. We also need to set GTK_CAN_FOCUS flag
	 * on the panel as it is unset when this function is called.
	 */
	gtk_container_set_focus_child (GTK_CONTAINER (panel_widget), NULL);
	gtk_widget_set_can_focus (GTK_WIDGET (panel_widget), TRUE);
	gtk_widget_grab_focus (GTK_WIDGET (panel_widget));
}


PanelOrientation
panel_widget_get_applet_orientation (PanelWidget *panel)
{
	g_return_val_if_fail (PANEL_IS_WIDGET (panel), PANEL_ORIENTATION_TOP);
	g_return_val_if_fail (PANEL_IS_TOPLEVEL (panel->toplevel), PANEL_ORIENTATION_TOP);

	return panel_toplevel_get_orientation (panel->toplevel);
}

void
panel_widget_set_applet_expandable (PanelWidget *panel,
				    GtkWidget   *applet,
				    gboolean     major,
				    gboolean     minor)
{
	AppletData *ad;

	ad = g_object_get_data (G_OBJECT (applet), PANEL_APPLET_DATA);
	if (!ad)
		return;

	major = major != FALSE;
	minor = minor != FALSE;

	if (ad->expand_major == major && ad->expand_minor == minor)
		return;

	ad->expand_major = major;
	ad->expand_minor = minor;

	gtk_widget_queue_resize (GTK_WIDGET (panel));
}

gboolean
panel_applet_is_in_drag (void)
{
	return panel_applet_in_drag;
}

void 
panel_widget_register_open_dialog (PanelWidget *panel,
				   GtkWidget   *dialog)
{
	/* the window is for a panel, so it should be shown in the taskbar. See
	 * HIG: An alert should not appear in the panel window list unless it
	 * is, or may be, the only window shown by an application. */
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);

	panel->open_dialogs = g_slist_append (panel->open_dialogs,
					      dialog);
	
	g_signal_connect_object (dialog, "destroy",
				 G_CALLBACK (panel_widget_open_dialog_destroyed),
				 panel,
				 G_CONNECT_SWAPPED);
}

GSList *panel_widget_get_panels (void)
{
  return panels;
}
