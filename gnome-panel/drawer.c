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
#include "panel-profile.h"
#include "panel-util.h"
#include "xstuff.h"
#include "panel-globals.h"

#undef DRAWER_DEBUG

#ifdef FIXME_FOR_NEW_CONFIG
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

	/* FIXME_FOR_NEW_CONFIG: save changes to GConf */
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
#endif /* FIXME_FOR_NEW_CONFIG */

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
	GtkOrientation orient;

	if (event->state == gtk_accelerator_get_default_mod_mask ())
		return FALSE;

	orient = PANEL_WIDGET (drawer->button->parent)->orient;

	switch (event->keyval) {
	case GDK_Up:
	case GDK_KP_Up:
		if (orient == GTK_ORIENTATION_HORIZONTAL) {
			if (!panel_toplevel_get_is_hidden (drawer->toplevel))
				drawer_focus_panel_widget (drawer, GTK_DIR_TAB_BACKWARD);
		} else {
			/* let default focus movement happen */
			retval = FALSE;
		}
		break;
	case GDK_Left:
	case GDK_KP_Left:
		if (orient == GTK_ORIENTATION_VERTICAL) {
			if (!panel_toplevel_get_is_hidden (drawer->toplevel))
				drawer_focus_panel_widget (drawer, GTK_DIR_TAB_BACKWARD);
		} else {
			/* let default focus movement happen */
			retval = FALSE;
		}
		break;
	case GDK_Down:
	case GDK_KP_Down:
		if (orient == GTK_ORIENTATION_HORIZONTAL) {
			if (!panel_toplevel_get_is_hidden (drawer->toplevel))
				drawer_focus_panel_widget (drawer, GTK_DIR_TAB_FORWARD);
		} else {
			/* let default focus movement happen */
			retval = FALSE;
		}
		break;
	case GDK_Right:
	case GDK_KP_Right:
		if (orient == GTK_ORIENTATION_VERTICAL) {
			if (!panel_toplevel_get_is_hidden (drawer->toplevel))
				drawer_focus_panel_widget (drawer, GTK_DIR_TAB_FORWARD);
		} else {
			/* let default focus movement happen */
			retval = FALSE;
		}
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

	foo = g_strdup_printf ("DRAWER:%d", panel_find_applet_index (widget));

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

static PanelToplevel *
create_drawer_toplevel (const char *drawer_id)
{
	PanelToplevel *toplevel;
	GConfClient   *client;
	const char    *profile;
	const char    *key;
	char          *profile_dir;
	char          *toplevel_id;

	client  = panel_gconf_get_client ();
	profile = panel_profile_get_name ();

	profile_dir = gconf_concat_dir_and_key (PANEL_CONFIG_DIR, profile);

	toplevel_id = panel_profile_find_new_id (PANEL_GCONF_TOPLEVELS);

	panel_profile_load_toplevel (client, profile_dir, PANEL_GCONF_TOPLEVELS, toplevel_id);

	/* takes ownership of toplevel_id */
	toplevel = panel_profile_get_toplevel_by_id (toplevel_id);

	g_free (profile_dir);

	if (!toplevel)
		return NULL;

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, drawer_id, "attached_panel_id");
	gconf_client_set_string (client, key, toplevel_id, NULL);

	panel_profile_set_toplevel_enable_buttons (toplevel, TRUE);
	panel_profile_set_toplevel_enable_arrows (toplevel, TRUE);

	return toplevel;
}

void
set_drawer_applet_orientation (Drawer           *drawer,
			       PanelOrientation  orientation)
{
	g_return_if_fail (drawer != NULL);

	button_widget_set_params (BUTTON_WIDGET (drawer->button), TRUE, orientation);
}

static void
drawer_button_size_allocated (GtkWidget     *widget,
			      GtkAllocation *alloc,
			      Drawer        *drawer)
{
	if (!GTK_WIDGET_REALIZED (widget))
		return;

	gtk_widget_queue_resize (GTK_WIDGET (drawer->toplevel));

	g_object_set_data (G_OBJECT (widget), "allocated", GINT_TO_POINTER (TRUE));
}

static void
load_drawer_applet (char          *toplevel_id,
		    const char    *pixmap,
		    const char    *tooltip,
		    PanelToplevel *parent_toplevel,
		    int            pos,
		    gboolean       exactpos,
		    const char    *id)
{
	PanelOrientation  orientation;
	PanelToplevel    *toplevel = NULL;
	Drawer           *drawer = NULL;

	orientation = panel_toplevel_get_orientation (parent_toplevel);

	if (toplevel_id)
		toplevel = panel_profile_get_toplevel_by_id (toplevel_id);

	if (!toplevel)
		toplevel = create_drawer_toplevel (id);

	if (toplevel) 
		drawer = create_drawer_applet (toplevel, parent_toplevel, tooltip, pixmap, orientation);

	if (!drawer)
		return;

	drawer->info = panel_applet_register (drawer->button, drawer,
					      (GDestroyNotify) free_drawer,
					      panel_toplevel_get_panel_widget (parent_toplevel),
					      pos, exactpos, PANEL_OBJECT_DRAWER, id);

	if (!drawer->info) {
		gtk_widget_destroy (GTK_WIDGET (toplevel));
		return;
	}

	g_signal_connect_after (drawer->button, "size_allocate",
				G_CALLBACK (drawer_button_size_allocated), drawer);

	panel_widget_add_forbidden (panel_toplevel_get_panel_widget (drawer->toplevel));

	gtk_tooltips_set_tip (panel_tooltips, drawer->button, drawer->tooltip, NULL);
	gtk_window_present (GTK_WINDOW (drawer->toplevel));

	if (!commie_mode)
		panel_applet_add_callback (drawer->info,
					   "properties",
					   GTK_STOCK_PROPERTIES,
					   _("_Properties"));

	panel_applet_add_callback (
		drawer->info, "help", GTK_STOCK_HELP, _("_Help"));
}

void
panel_drawer_create (PanelToplevel *toplevel,
		     int            position,
		     const char    *custom_icon,
		     gboolean       use_custom_icon,
		     const char    *tooltip)
{
	GConfClient *client;
	const char  *profile;
	const char  *key;
	char        *id;

	client  = panel_gconf_get_client ();
	profile = panel_profile_get_name ();

	id = panel_profile_prepare_object (PANEL_OBJECT_DRAWER, toplevel, position);

	if (tooltip) {
		key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "tooltip");
		gconf_client_set_string (client, key, tooltip, NULL);
	}

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "use_custom_icon");
	gconf_client_set_bool (client, key, use_custom_icon, NULL);

	if (custom_icon) {
		key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "custom_icon");
		gconf_client_set_string (client, key, custom_icon, NULL);
	}

	/* frees id */
	panel_profile_add_to_list (PANEL_GCONF_OBJECTS, id);
}


void
drawer_load_from_gconf (PanelWidget *panel_widget,
			gint         position,
			const char  *id)
{
	GConfClient *client;
	const char  *profile;
	const char  *key;
	char        *profile_dir;
	char        *toplevel_id;
	char        *pixmap;
	char        *tooltip;

	g_return_if_fail (panel_widget != NULL);
	g_return_if_fail (id != NULL);

	client  = panel_gconf_get_client ();
	profile = panel_profile_get_name ();

	profile_dir = gconf_concat_dir_and_key (PANEL_CONFIG_DIR, profile);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "attached_toplevel_id");
	toplevel_id = gconf_client_get_string (client, key, NULL);

	panel_profile_load_toplevel (client, profile_dir, PANEL_GCONF_TOPLEVELS, toplevel_id);

	/* FIXME_FOR_NEW_TOLEVEL: get the use_custom_icon setting */
	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "custom_icon");
	pixmap = gconf_client_get_string (client, key, NULL);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "tooltip");
	tooltip = gconf_client_get_string (client, key, NULL);

	load_drawer_applet (toplevel_id,
			    pixmap,
			    tooltip,
			    panel_widget->toplevel,
			    position,
			    TRUE,
			    id);

	g_free (profile_dir);
	g_free (toplevel_id);
	g_free (pixmap);
	g_free (tooltip);
}
