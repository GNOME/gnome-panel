/*
 * GNOME panel drawer module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 *          George Lebl
 */

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include <libgnomeui/libgnomeui.h>
#include <libgnome/libgnome.h>

#include "drawer.h"

#include "applet.h"
#include "button-widget.h"
#include "panel-config-global.h"
#include "panel-gconf.h"
#include "panel-util.h"
#include "session.h"
#include "xstuff.h"

extern GlobalConfig global_config;
extern gboolean commie_mode;

extern int panels_to_sync;

extern GtkTooltips *panel_tooltips;
extern GSList *panels;

#undef DRAWER_DEBUG

#ifdef FIXME_FOR_NEW_TOPLEVEL
static void
properties_apply_callback(gpointer data)
{
	Drawer       *drawer = data;

	GtkWidget    *pixentry = g_object_get_data (G_OBJECT (drawer->properties),
						    "pixmap");
	GtkWidget    *tipentry = g_object_get_data (G_OBJECT (drawer->properties),
						    "tooltip");
	const char   *cs;

	g_free (drawer->pixmap);
	drawer->pixmap = NULL;
	g_free (drawer->tooltip);
	drawer->tooltip = NULL;

	drawer->pixmap = gnome_icon_entry_get_filename (GNOME_ICON_ENTRY (pixentry));
	if (string_empty (drawer->pixmap) ||
	    access (drawer->pixmap, R_OK) != 0) {
		drawer->pixmap = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
							    "panel-drawer.png", TRUE, NULL);
	}

	button_widget_set_pixmap (BUTTON_WIDGET (drawer->button), drawer->pixmap);

	cs = gtk_entry_get_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(tipentry))));
	if (string_empty (cs))
		drawer->tooltip = NULL;
	else
		drawer->tooltip = g_strdup (cs);

	gtk_tooltips_set_tip (panel_tooltips, drawer->button,
			      drawer->tooltip, NULL);

	drawer_save_to_gconf (drawer, drawer->info->gconf_key);
}

static void
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = data;
	drawer->properties = NULL;
}

static void
set_toggle (GtkWidget *widget, gpointer data)
{
	PerPanelConfig *ppc = g_object_get_data (G_OBJECT (widget), "PerPanelConfig");
	int *the_toggle = data;

	*the_toggle = GTK_TOGGLE_BUTTON(widget)->active;

	panel_config_register_changes (ppc);
}

static void
set_sensitive_toggle (GtkWidget *widget, GtkWidget *widget2)
{
	gtk_widget_set_sensitive(widget2,GTK_TOGGLE_BUTTON(widget)->active);
}

void
add_drawer_properties_page (PerPanelConfig *ppc, GtkNotebook *prop_nbook, Drawer *drawer)
{
        GtkWidget *dialog;
        GtkWidget *table;
	GtkWidget *f;
	GtkWidget *box, *box_in;
	GtkWidget *w;
	GtkWidget *button;

	g_return_if_fail (ppc != NULL);

        dialog = ppc->config_window;

	box = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (box), GNOME_PAD_SMALL);

	box_in = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (box_in), GNOME_PAD_SMALL);

	w = make_size_widget (ppc);
	gtk_box_pack_start (GTK_BOX (box_in), w, FALSE, FALSE, 0);

	f = gtk_frame_new (_("Size and Position"));
	gtk_container_add (GTK_CONTAINER (f), box_in);
	gtk_box_pack_start (GTK_BOX (box), f, FALSE, FALSE, 0);

	
	table = gtk_table_new (3, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), GNOME_PAD_SMALL);

	w = create_text_entry(table, "drawer_name", 0, _("Tooltip/Name"),
			      drawer->tooltip,
			      (UpdateFunction)panel_config_register_changes,
			      ppc);
	g_object_set_data (G_OBJECT (dialog), "tooltip", w);
	
	w = create_icon_entry(table, "icon", 0, 2, _("Icon"),
			      NULL, drawer->pixmap,
			      (UpdateFunction)panel_config_register_changes,
			      ppc);
	g_object_set_data (G_OBJECT (dialog), "pixmap", w);

	f = gtk_frame_new(_("Applet appearance"));
	gtk_container_add(GTK_CONTAINER(f),table);

	gtk_box_pack_start(GTK_BOX(box),f,FALSE,FALSE,0);

	f = gtk_frame_new(_("Drawer handle"));
	box_in = gtk_vbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(box_in), GNOME_PAD_SMALL);
	/*we store this in w for later use!, so don't use w as temp from now
	  on*/
	w = button = gtk_check_button_new_with_label (_("Enable hidebutton"));
	g_object_set_data (G_OBJECT (button), "PerPanelConfig", ppc);
	if (ppc->hidebuttons)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	g_signal_connect (G_OBJECT (button), "toggled", 
			    G_CALLBACK (set_toggle),
			    &ppc->hidebuttons);
	gtk_box_pack_start (GTK_BOX (box_in), button, TRUE, FALSE, 0);

	button = gtk_check_button_new_with_label (_("Enable hidebutton arrow"));
	g_signal_connect (G_OBJECT (w), "toggled", 
			    G_CALLBACK (set_sensitive_toggle),
			    button);
	if (!ppc->hidebuttons)
		gtk_widget_set_sensitive(button,FALSE);
	g_object_set_data (G_OBJECT (button), "PerPanelConfig", ppc);
	if (ppc->hidebutton_pixmaps)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	g_signal_connect (G_OBJECT (button), "toggled", 
			    G_CALLBACK (set_toggle),
			    &ppc->hidebutton_pixmaps);
	gtk_box_pack_start (GTK_BOX (box_in), button, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(f),box_in);
	gtk_box_pack_start (GTK_BOX (box),f,FALSE,FALSE,0);

	
	gtk_notebook_append_page (GTK_NOTEBOOK(prop_nbook),
				  box, gtk_label_new (_("Drawer")));
	
	g_signal_connect (G_OBJECT(dialog), "destroy",
			  G_CALLBACK (properties_close_callback),
			   drawer);

	ppc->update_function = properties_apply_callback;
	ppc->update_data = drawer;

	drawer->properties = dialog;
}
#endif /* FIXME_FOR_NEW_TOPLEVEL */

static void
drawer_click (GtkWidget *w, Drawer *drawer)
{
	if (!panel_toplevel_get_is_hidden (drawer->toplevel))
		panel_toplevel_hide (drawer->toplevel, FALSE, -1);
	else 
		panel_toplevel_unhide (drawer->toplevel);
}

static void
destroy_drawer(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = data;
	GtkWidget *prop_dialog = drawer->properties;

	drawer->properties = NULL;

	if (drawer->close_timeout_id)
		g_source_remove (drawer->close_timeout_id);
	drawer->close_timeout_id = 0;

	if(prop_dialog)
		gtk_widget_destroy(prop_dialog);
}

static void
drawer_focus_panel_widget (Drawer           *drawer,
			   GtkDirectionType  direction)
{
	PanelWidget *panel_widget;

	panel_widget = panel_toplevel_get_panel_widget (drawer->toplevel);

	gtk_window_present (GTK_WINDOW (drawer->toplevel));
	gtk_container_set_focus_child (GTK_CONTAINER (panel_widget), NULL);
	gtk_widget_child_focus (GTK_WIDGET (panel_widget), direction);
}

static gboolean
key_press_drawer (GtkWidget   *widget,
		  GdkEventKey *event,
		  Drawer      *drawer)
{
	gboolean retval = TRUE;

	if (event->state == gtk_accelerator_get_default_mod_mask ())
		return FALSE;

	switch (event->keyval) {
	case GDK_Up:
	case GDK_KP_Up:
	case GDK_Left:
	case GDK_KP_Left:
		if (!panel_toplevel_get_is_hidden (drawer->toplevel))
			drawer_focus_panel_widget (drawer, GTK_DIR_TAB_BACKWARD);
		break;
	case GDK_Down:
	case GDK_KP_Down:
	case GDK_Right:
	case GDK_KP_Right:
		if (!panel_toplevel_get_is_hidden (drawer->toplevel))
			drawer_focus_panel_widget (drawer, GTK_DIR_TAB_FORWARD);
		break;
	case GDK_Escape:
		panel_toplevel_hide (drawer->toplevel, FALSE, -1);
		break;
	default:
		retval = FALSE;
		break;
	}

	return retval;
}

/*
 * This function implements Esc moving focus from the drawer to the drawer
 * icon and closing the drawer and Shift+Esc moving focus from the drawer
 * to the drawer icon without closing the drawer when focus is in the drawer.
 */
static gboolean
key_press_drawer_widget (GtkWidget   *widget,
			 GdkEventKey *event,
			 Drawer      *drawer)
{
	PanelWidget *panel_widget;

	if (event->keyval != GDK_Escape)
		return FALSE;

	panel_widget = panel_toplevel_get_panel_widget (drawer->toplevel);

	gtk_window_present (GTK_WINDOW (panel_widget->toplevel));

	if (event->state == GDK_SHIFT_MASK ||
	    panel_toplevel_get_is_hidden (drawer->toplevel))
		return TRUE;

	panel_toplevel_hide (drawer->toplevel, FALSE, -1);

	return TRUE;
}

static void 
drag_data_received_cb (GtkWidget          *widget,
		       GdkDragContext     *context,
		       gint                x,
		       gint                y,
		       GtkSelectionData   *selection_data,
		       guint               info,
		       guint               time_,
		       Drawer             *drawer)
{
	PanelWidget *panel_widget;

	if (!panel_check_dnd_target_data (widget, context, &info, NULL)) {
		gtk_drag_finish (context, FALSE, FALSE, time_);
		return;
	}

	panel_widget = panel_toplevel_get_panel_widget (drawer->toplevel);

	panel_receive_dnd_data (
		panel_widget, info, -1, selection_data, context, time_);
}

static gboolean
drag_motion_cb (GtkWidget          *widget,
		GdkDragContext     *context,
		int                 x,
		int                 y,
		guint               time_,
		Drawer             *drawer)
{
	PanelWidget *panel_widget;
	guint        info = 0;

	if (!panel_check_dnd_target_data (widget, context, &info, NULL))
		return FALSE;

	panel_widget = panel_toplevel_get_panel_widget (drawer->toplevel);

	if (!panel_check_drop_forbidden (panel_widget, context, info, time_))
		return FALSE;

	if (drawer->close_timeout_id)
		g_source_remove (drawer->close_timeout_id);
	drawer->close_timeout_id = 0;

	button_widget_set_dnd_highlight (BUTTON_WIDGET (widget), TRUE);

	if (panel_toplevel_get_is_hidden (drawer->toplevel)) {
		panel_toplevel_unhide (drawer->toplevel);
		drawer->opened_for_drag = TRUE;
	}

	return TRUE;
}

static gboolean
close_drawer_in_idle (gpointer data)
{
	Drawer *drawer = (Drawer *) data;

	drawer->close_timeout_id = 0;

	if (drawer->opened_for_drag) {
		PanelWidget *button_parent;

		button_parent = PANEL_WIDGET (drawer->button->parent);

		panel_toplevel_hide (drawer->toplevel, FALSE, -1);
		drawer->opened_for_drag = FALSE;
	}

	return FALSE;
}

static void
queue_drawer_close_for_drag (Drawer *drawer)
{
	if (!drawer->close_timeout_id)
		drawer->close_timeout_id =
			g_timeout_add (1 * 1000, close_drawer_in_idle, drawer);
}

static void
drag_leave_cb (GtkWidget      *widget,
	       GdkDragContext *context,
	       guint           time_,
	       Drawer         *drawer)
{
	queue_drawer_close_for_drag (drawer);

	button_widget_set_dnd_highlight (BUTTON_WIDGET (widget), FALSE);
}

static gboolean
drag_drop_cb (GtkWidget      *widget,
	      GdkDragContext *context,
	      int             x,
	      int             y,
	      guint           time_,
	      Drawer         *drawer)
{
	GdkAtom atom = 0;

	if (!panel_check_dnd_target_data (widget, context, NULL, &atom))
		return FALSE;

	gtk_drag_get_data (widget, context, atom, time_);

	return TRUE;
}

static void  
drag_data_get_cb (GtkWidget          *widget,
		  GdkDragContext     *context,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint               time,
		  Drawer             *drawer)
{
	char *foo;

	foo = g_strdup_printf ("DRAWER:%d", panel_find_applet (widget));

	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)foo,
				strlen (foo));

	g_free (foo);
}

static void
free_drawer (Drawer *drawer)
{
	if (drawer->tooltip)
		g_free (drawer->tooltip);

	if (drawer->pixmap)
		g_free (drawer->pixmap);

	g_free (drawer);
}

static Drawer *
create_drawer_applet (PanelToplevel    *toplevel,
		      PanelToplevel    *parent_toplevel,
		      const char       *tooltip,
		      const char       *pixmap,
		      PanelOrientation  orientation)
{
        static GtkTargetEntry dnd_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};
	Drawer *drawer;
	
	drawer = g_new0 (Drawer, 1);
	
	drawer->properties = NULL;

	if (string_empty (tooltip))
		drawer->tooltip = NULL;
	else
		drawer->tooltip = g_strdup (tooltip);

	if (!pixmap || !pixmap [0])
		drawer->pixmap = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
							    "panel-drawer.png", TRUE, NULL);
	else
		drawer->pixmap = g_strdup (pixmap);

	drawer->button = button_widget_new (drawer->pixmap, -1,
					    TRUE, orientation);
	if (!drawer->button) {
		free_drawer (drawer);
		return NULL;
	}

	/*A hack since this function only pretends to work on window
	  widgets (which we actually kind of are) this will select
	  some (already selected) events on the panel instead of
	  the button window (where they are also selected) but
	  we don't mind*/ 
	GTK_WIDGET_UNSET_FLAGS (drawer->button, GTK_NO_WINDOW);
 
	gtk_drag_source_set (drawer->button,
			     GDK_BUTTON1_MASK,
			     dnd_targets, 1,
			     GDK_ACTION_MOVE);
	GTK_WIDGET_SET_FLAGS (drawer->button, GTK_NO_WINDOW);

	gtk_drag_dest_set (drawer->button, 0, NULL, 0, 0); 

	g_signal_connect (drawer->button, "drag_data_get",
			  G_CALLBACK (drag_data_get_cb), drawer);
	g_signal_connect (drawer->button, "drag_data_received",
			  G_CALLBACK (drag_data_received_cb), drawer);
	g_signal_connect (drawer->button, "drag_motion",
			  G_CALLBACK (drag_motion_cb), drawer);
	g_signal_connect (drawer->button, "drag_leave",
			  G_CALLBACK (drag_leave_cb), drawer);
	g_signal_connect (drawer->button, "drag_drop",
			  G_CALLBACK (drag_drop_cb), drawer);

	g_signal_connect (drawer->button, "clicked",
			  G_CALLBACK (drawer_click), drawer);
	g_signal_connect (drawer->button, "destroy",
			  G_CALLBACK (destroy_drawer), drawer);
	g_signal_connect (drawer->button, "key_press_event",
			  G_CALLBACK (key_press_drawer), drawer);

	gtk_widget_show (drawer->button);

	drawer->toplevel = toplevel;

	g_signal_connect (drawer->toplevel, "key_press_event",
			  G_CALLBACK (key_press_drawer_widget), drawer);

	panel_toplevel_attach_to_widget (
		toplevel, parent_toplevel, GTK_WIDGET (drawer->button));

	return drawer;
}

static Drawer *
create_empty_drawer_applet (PanelWidget      *container,
			    PanelToplevel    *parent_toplevel,
			    const char       *tooltip,
			    const char       *pixmap,
			    PanelOrientation  orientation)
{
	PanelToplevel *toplevel;
	GdkScreen     *screen;
	int            monitor;
	int            screen_width, screen_height;

	screen  = panel_screen_from_panel_widget (container);
	monitor = panel_monitor_from_panel_widget (container);

	screen_width  = gdk_screen_get_width  (screen);
	screen_height = gdk_screen_get_height (screen);

	toplevel = g_object_new (PANEL_TYPE_TOPLEVEL,
				 "x", screen_width  + 10,
				 "y", screen_height + 10,
				 NULL);

	return create_drawer_applet (toplevel, parent_toplevel,
				     tooltip, pixmap, orientation);
}

void
set_drawer_applet_orientation (Drawer           *drawer,
			       PanelOrientation  orientation)
{
	g_return_if_fail (drawer != NULL);

	button_widget_set_params (BUTTON_WIDGET (drawer->button), TRUE, orientation);
}

static void
button_size_alloc(GtkWidget *widget, GtkAllocation *alloc, Drawer *drawer)
{
	if(!GTK_WIDGET_REALIZED(widget))
		return;

	gtk_widget_queue_resize (GTK_WIDGET (drawer->toplevel));

	g_object_set_data (G_OBJECT (widget), "allocated", GINT_TO_POINTER (TRUE));
}

Drawer *
load_drawer_applet (gchar       *mypanel_id,
		    const char  *pixmap,
		    const char  *tooltip,
		    PanelWidget *panel,
		    int          pos,
		    gboolean     exactpos,
		    const char  *gconf_key)
{
	PanelOrientation  orientation;
	Drawer           *drawer;

	orientation = panel_widget_get_applet_orientation (panel);

	if (!mypanel_id) {
		drawer = create_empty_drawer_applet (
				panel, panel->toplevel, tooltip, pixmap, orientation);
		if (drawer != NULL)
			panel_setup (drawer->toplevel);
		panels_to_sync = TRUE;
	} else {
		PanelData *dr_pd;

		dr_pd = panel_data_by_id (mypanel_id);

		if (dr_pd == NULL) {
			g_warning ("Can't find the panel for drawer, making a new panel");
			drawer = create_empty_drawer_applet(
					panel, panel->toplevel, tooltip, pixmap, orientation);
			if(drawer) panel_setup(drawer->toplevel);
			panels_to_sync = TRUE;
		} else {
			drawer = create_drawer_applet (
					PANEL_TOPLEVEL (dr_pd->panel),
					panel->toplevel,
					tooltip, pixmap, orientation);

			panel_toplevel_set_orientation (
				PANEL_TOPLEVEL (dr_pd->panel), orientation);
		}
	}

	if (!drawer)
		return NULL;

	{
		GtkWidget *dw = GTK_WIDGET (drawer->toplevel);

		drawer->info = panel_applet_register (
					drawer->button, drawer,
					(GDestroyNotify) free_drawer,
					panel, pos, exactpos,
					APPLET_DRAWER, gconf_key);

		if (!drawer->info) {
			gtk_widget_destroy (dw);
			return NULL;
		}
	}

	g_signal_connect_after (G_OBJECT(drawer->button),
				"size_allocate",
				G_CALLBACK (button_size_alloc),
				drawer);

	panel_widget_add_forbidden (panel_toplevel_get_panel_widget (drawer->toplevel));

	gtk_tooltips_set_tip (panel_tooltips,drawer->button,
			      drawer->tooltip,NULL);
	gtk_window_present (GTK_WINDOW (drawer->toplevel));

	if (!commie_mode)
		panel_applet_add_callback (drawer->info,
					   "properties",
					   GTK_STOCK_PROPERTIES,
					   _("_Properties"));

	panel_applet_add_callback (
		drawer->info, "help", GTK_STOCK_HELP, _("_Help"));

	return drawer;
}

#ifdef FIXME_FOR_NEW_TOPLEVEL
void
drawer_save_to_gconf (Drawer     *drawer,
		      const char *gconf_key)
{
	PanelWidget *panel_widget;
	GConfClient *client;
	const char  *profile;
	const char  *temp_key;

	g_return_if_fail (drawer && PANEL_IS_TOPLEVEL (drawer->toplevel));

	panel_widget = panel_toplevel_get_panel_widget (drawer->toplevel);

	g_return_if_fail (PANEL_IS_WIDGET (panel_widget));

	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "parameters");
	gconf_client_set_int (client, temp_key,
			      g_slist_index (panels, panel_widget),
			      NULL);

	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "unique-drawer-panel-id");
	gconf_client_set_string (client, temp_key,
				 panel_widget->unique_id,
				 NULL);

	temp_key = panel_gconf_full_key (
			PANEL_GCONF_OBJECTS, profile, gconf_key, "pixmap");
	gconf_client_set_string (client, temp_key, drawer->pixmap, NULL);

	if (drawer->tooltip) {
		temp_key = panel_gconf_full_key (
				PANEL_GCONF_OBJECTS, profile, gconf_key, "tooltip");
		gconf_client_set_string (client, temp_key, drawer->tooltip, NULL);
	}

	panel_save_to_gconf (g_object_get_data (G_OBJECT (drawer->toplevel), "PanelData"));
}

void
drawer_load_from_gconf (PanelWidget *panel_widget,
			gint         position,
			const char  *gconf_key)
{
	GConfClient *client;
	const char  *profile;
	const char  *temp_key;
	int          panel;
	char        *panel_id;
	char        *pixmap;
	char        *tooltip;

	g_return_if_fail (panel_widget != NULL);
	g_return_if_fail (gconf_key != NULL);

	client  = panel_gconf_get_client ();
	profile = panel_gconf_get_profile ();

	temp_key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile,
					 gconf_key, "parameters");
	panel = gconf_client_get_int (client, temp_key, NULL);

	temp_key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile,
					 gconf_key, "unique-drawer-panel-id");
	panel_id = gconf_client_get_string (client, temp_key, NULL);

	temp_key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile,
					 gconf_key, "pixmap");
	pixmap = gconf_client_get_string (client, temp_key, NULL);

	temp_key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile,
					 gconf_key, "tooltip");
	tooltip = gconf_client_get_string (client, temp_key, NULL);

	if (!panel_id < 0 && panel >= 0) {
		PanelWidget *pw = g_slist_nth_data (panels, panel);

		if (pw)
			panel_id = g_strdup (pw->unique_id);
	}

	load_drawer_applet (panel_id, pixmap, tooltip, panel_widget, position, TRUE, gconf_key);

	g_free (panel_id);
	g_free (pixmap);
	g_free (tooltip);
}
#endif
