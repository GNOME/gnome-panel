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
#include "drawer-widget.h"
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

static void
properties_apply_callback(gpointer data)
{
	Drawer       *drawer = data;

	GtkWidget    *pixentry = g_object_get_data (G_OBJECT (drawer->properties),
						    "pixmap");
	GtkWidget    *tipentry = g_object_get_data (G_OBJECT (drawer->properties),
						    "tooltip");
	char         *s;
	const char   *cs;

	g_free (drawer->pixmap);
	drawer->pixmap = NULL;
	g_free (drawer->tooltip);
	drawer->tooltip = NULL;
	s = gnome_icon_entry_get_filename (GNOME_ICON_ENTRY (pixentry));
	if (string_empty (s)) {
		drawer->pixmap = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
							    "panel-drawer.png", TRUE, NULL);
		button_widget_set_pixmap (BUTTON_WIDGET (drawer->button),
					  drawer->pixmap,-1);
	} else {
		if(button_widget_set_pixmap (BUTTON_WIDGET (drawer->button), s, -1))
			drawer->pixmap = g_strdup (s);
		else {
			drawer->pixmap = gnome_program_locate_file (NULL, 
								    GNOME_FILE_DOMAIN_PIXMAP, 
								    "panel-drawer.png",
								     TRUE, NULL);
			button_widget_set_pixmap (BUTTON_WIDGET (drawer->button),
						  drawer->pixmap, -1);
		}
	}
	g_free(s);
	cs = gtk_entry_get_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(tipentry))));
	if (string_empty (cs))
		drawer->tooltip = NULL;
	else
		drawer->tooltip = g_strdup (cs);

	gtk_tooltips_set_tip (panel_tooltips, drawer->button,
			      drawer->tooltip, NULL);
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

static void
drawer_click(GtkWidget *w, Drawer *drawer)
{
	DrawerWidget *drawerw = DRAWER_WIDGET(drawer->drawer);
	PanelWidget *parent = PANEL_WIDGET(drawer->button->parent);
	GtkWidget *panelw = parent->panel_parent;
#ifdef DRAWER_DEBUG
	printf ("Registering drawer click \n");
#endif	
	switch (BASEP_WIDGET (drawerw)->state) {
	case BASEP_SHOWN:
	case BASEP_AUTO_HIDDEN:
#ifdef DRAWER_DEBUG
	printf ("Drawer closing\n");
#endif
		drawer_widget_close_drawer (drawerw, panelw);
		break;
	case BASEP_HIDDEN_LEFT:
	case BASEP_HIDDEN_RIGHT:
#ifdef DRAWER_DEBUG
	printf ("Drawer opening\n");
#endif
		drawer_widget_open_drawer (drawerw, panelw);
		break;
	}
}

static void
destroy_drawer(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = data;
	GtkWidget *prop_dialog = drawer->properties;

	drawer->properties = NULL;

	if(prop_dialog)
		gtk_widget_destroy(prop_dialog);
}

static int
enter_notify_drawer(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	Drawer *drawer = data;
	BasePWidget *basep = BASEP_WIDGET (drawer->drawer);
#ifdef DRAWER_DEBUG
	printf ("Enter notify drawer\n");
#endif
	if (!xstuff_is_compliant_wm() || global_config.autoraise)
		gdk_window_raise(drawer->drawer->window);

	if (basep->moving)
		return FALSE;
	
	if ((basep->state != BASEP_AUTO_HIDDEN) ||
	    (event->detail == GDK_NOTIFY_INFERIOR) ||
	    (basep->mode != BASEP_AUTO_HIDE))
		return FALSE;

	if (basep->leave_notify_timer_tag != 0) {
		gtk_timeout_remove (basep->leave_notify_timer_tag);
		basep->leave_notify_timer_tag = 0;
	}

	basep_widget_autoshow (basep);

	return FALSE;
}

static int
leave_notify_drawer (GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	Drawer *drawer = data;
	BasePWidget *basep = BASEP_WIDGET (drawer->drawer);

	if (event->detail == GDK_NOTIFY_INFERIOR)
		return FALSE;

	basep_widget_queue_autohide (basep);

	return FALSE;
	
}

static void  
drag_data_get_cb (GtkWidget          *widget,
		  GdkDragContext     *context,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint               time,
		  gpointer            data)
{
	char *foo;

	foo = g_strdup_printf ("DRAWER:%d", panel_find_applet (widget));

	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)foo,
				strlen (foo));

	g_free (foo);
}

static Drawer *
create_drawer_applet(GtkWidget * drawer_panel,
		     const char *tooltip, const char *pixmap,
		     PanelOrient orient)
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

	if (string_empty (pixmap)) {
		drawer->pixmap = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
							    "panel-drawer.png", TRUE, NULL);
	} else {
		drawer->pixmap = g_strdup (pixmap);
	}
	drawer->button = button_widget_new (drawer->pixmap, -1,
					    TRUE, orient,
					    _("Drawer"));

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

	g_signal_connect (G_OBJECT (drawer->button), "drag_data_get",
			  G_CALLBACK (drag_data_get_cb),
			  NULL);

	gtk_widget_show(drawer->button);

	drawer->drawer = drawer_panel;

	g_signal_connect (G_OBJECT (drawer->button), "clicked",
			    G_CALLBACK (drawer_click), drawer);
	g_signal_connect (G_OBJECT (drawer->button), "destroy",
			    G_CALLBACK (destroy_drawer), drawer);
	g_signal_connect (G_OBJECT (drawer->button), "enter_notify_event",
			    G_CALLBACK (enter_notify_drawer), drawer);
	g_signal_connect (G_OBJECT (drawer->button), "leave_notify_event",
			    G_CALLBACK (leave_notify_drawer), drawer);

	g_object_set_data (G_OBJECT (drawer_panel), DRAWER_PANEL_KEY, drawer);
	gtk_widget_queue_resize (GTK_WIDGET (drawer_panel));

	return drawer;
}

static Drawer *
create_empty_drawer_applet(const char *tooltip, const char *pixmap,
			   PanelOrient orient)
{
	GtkWidget *dw = drawer_widget_new (NULL, orient,
					   BASEP_EXPLICIT_HIDE,
					   BASEP_SHOWN,
					   PANEL_SIZE_MEDIUM,
					   TRUE, TRUE,
					   PANEL_BACK_NONE, NULL,
					   TRUE, FALSE, TRUE, NULL);
	return create_drawer_applet (dw, tooltip, pixmap, orient);
}

void
set_drawer_applet_orient(Drawer *drawer, PanelOrient orient)
{
	g_return_if_fail (drawer != NULL);

	button_widget_set_params (BUTTON_WIDGET (drawer->button),
				  TRUE, orient);
	
	/*ignore orient events until we are realized, this will only
	  be the initial one and we have already set the orientation*/
	if(!GTK_WIDGET_REALIZED(drawer->drawer))
		return;
	
	drawer_widget_change_orient(DRAWER_WIDGET(drawer->drawer), orient);
}

static void
drawer_setup(Drawer *drawer)
{
	gtk_widget_queue_resize(drawer->drawer);
	if(BASEP_WIDGET(drawer->drawer)->state != BASEP_SHOWN) {
		GtkRequisition chreq;
		gtk_widget_size_request(drawer->drawer, &chreq);
		gtk_window_move (GTK_WINDOW (drawer->drawer),
				 -chreq.width - 1, -chreq.height - 1);
	}
	gtk_widget_show(drawer->drawer);
}

static void
button_size_alloc(GtkWidget *widget, GtkAllocation *alloc, Drawer *drawer)
{
	if(!GTK_WIDGET_REALIZED(widget))
		return;

	gtk_widget_queue_resize (drawer->drawer);

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
	Drawer      *drawer;
	PanelOrient  orient = get_applet_orient (panel);
	AppletInfo  *info;

	if (!mypanel_id) {
		drawer = create_empty_drawer_applet (tooltip, pixmap, orient);
		if (drawer != NULL)
			panel_setup (drawer->drawer);
		panels_to_sync = TRUE;
	} else {
		PanelData *dr_pd;

		dr_pd = panel_data_by_id (mypanel_id);

		if (dr_pd == NULL) {
			g_warning ("Can't find the panel for drawer, making a new panel");
			drawer = create_empty_drawer_applet(tooltip, pixmap, orient);
			if(drawer) panel_setup(drawer->drawer);
			panels_to_sync = TRUE;
		} else if ( ! DRAWER_IS_WIDGET (dr_pd->panel)) {
			g_warning ("I found a bogus panel for a drawer, making a new one");
			drawer = create_empty_drawer_applet(tooltip, pixmap, orient);
			if(drawer) panel_setup(drawer->drawer);
			panels_to_sync = TRUE;
		} else {
			drawer = create_drawer_applet (dr_pd->panel, tooltip,
						       pixmap, orient);

			drawer_widget_change_orient
				(DRAWER_WIDGET(dr_pd->panel), orient);
		}
	}

	if (!drawer)
		return NULL;

	{
		GtkWidget  *dw = drawer->drawer;

		info = panel_applet_register (drawer->button,
					      drawer,
					      (GDestroyNotify) g_free,
					      panel,
					      pos,
					      exactpos,
					      APPLET_DRAWER,
					      gconf_key);

		if (!info) {
			gtk_widget_destroy (dw);
			return NULL;
		}
	}

	g_signal_connect_after (G_OBJECT(drawer->button),
				"size_allocate",
				G_CALLBACK (button_size_alloc),
				drawer);

	/* this doesn't make sense anymore */
	if((BASEP_WIDGET(drawer->drawer)->state == BASEP_SHOWN) &&
	   (BASEP_IS_WIDGET (panel->panel_parent))) {
		/*pop up, if popped down, if it's not an autohidden
		  widget then it will just ignore this next call */
		basep_widget_autoshow(BASEP_WIDGET(panel->panel_parent));
	} 

	panel_widget_add_forbidden(PANEL_WIDGET(BASEP_WIDGET(drawer->drawer)->panel));

	gtk_tooltips_set_tip (panel_tooltips,drawer->button,
			      drawer->tooltip,NULL);
	drawer_setup (drawer);

	if (!commie_mode)
		panel_applet_add_callback (info,
					   "properties",
					   GTK_STOCK_PROPERTIES,
					   _("Properties..."));

	panel_applet_add_callback (info, "help", GTK_STOCK_HELP, _("Help"));

	return drawer;
}

void
drawer_save_to_gconf (Drawer     *drawer,
		      const char *gconf_key)
{
	GConfClient *client;
	const char  *profile;
	char        *temp_key;

	g_return_if_fail (drawer && BASEP_IS_WIDGET (drawer->drawer));
	g_return_if_fail (PANEL_IS_WIDGET (BASEP_WIDGET (drawer->drawer)->panel));

	client  = panel_gconf_get_client ();
	profile = session_get_current_profile ();

	temp_key = panel_gconf_objects_profile_get_full_key (profile, gconf_key, "parameters");
	gconf_client_set_int (client, temp_key,
			      g_slist_index (panels, BASEP_WIDGET (drawer->drawer)->panel),
			      NULL);
	g_free (temp_key);

	temp_key = panel_gconf_objects_profile_get_full_key (profile, gconf_key, "unique-drawer-panel-id");
	gconf_client_set_string (client, temp_key,
				 PANEL_WIDGET (BASEP_WIDGET (drawer->drawer)->panel)->unique_id,
				 NULL);
	g_free (temp_key);

	temp_key = panel_gconf_objects_profile_get_full_key (profile, gconf_key, "pixmap");
	gconf_client_set_string (client, temp_key, drawer->pixmap, NULL);
	g_free (temp_key);

	temp_key = panel_gconf_objects_profile_get_full_key (profile, gconf_key, "tooltip");
	gconf_client_set_string (client, temp_key, drawer->tooltip, NULL);
	g_free (temp_key);
}

void
drawer_load_from_gconf (PanelWidget *panel_widget,
			gint         position,
			const char  *gconf_key,
			gboolean     use_default)
{
	GConfClient *client;
	const char  *profile;
	char        *temp_key;
	int          panel;
	char        *panel_id;
	char        *pixmap;
	char        *tooltip;

	g_return_if_fail (panel_widget);
	g_return_if_fail (gconf_key);

	client  = panel_gconf_get_client ();

	/* FIXME: Screen widths etc.. */
	if (use_default)
		profile = "medium";
	else
		profile = session_get_current_profile ();

	temp_key = panel_gconf_objects_get_full_key (profile, gconf_key, "parameters", use_default);
	panel = gconf_client_get_int (client, temp_key, NULL);
	g_free (temp_key);

	temp_key = panel_gconf_objects_get_full_key (profile, gconf_key, "unique-drawer-panel-id", use_default);
	panel_id = gconf_client_get_string (client, temp_key, NULL);
	g_free (temp_key);

	temp_key = panel_gconf_objects_get_full_key (profile, gconf_key, "pixmap", use_default);
	pixmap = gconf_client_get_string (client, temp_key, NULL);
	g_free (temp_key);

	temp_key = panel_gconf_objects_get_full_key (profile, gconf_key, "tooltip", use_default);
	tooltip = gconf_client_get_string (client, temp_key, NULL);
	g_free (temp_key);

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
