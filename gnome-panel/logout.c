/* logout.c - Panel applet to end current session.  */
/* Original author unknown. CORBAized by Elliot Lee */
/* uncorbized by George Lebl */

#include <config.h>
#include <gnome.h>

#include "panel-include.h"

extern GSList *applets;
extern GSList *applets_last;

extern GtkTooltips *panel_tooltips;

extern GlobalConfig global_config;
extern gboolean commie_mode;

static void
logout (GtkWidget *widget)
{
	gtk_signal_handler_block_by_func (GTK_OBJECT (widget), GTK_SIGNAL_FUNC(logout), NULL);
	if (global_config.drawer_auto_close) {
		GtkWidget *parent = PANEL_WIDGET(widget->parent)->panel_parent;
		g_return_if_fail(parent!=NULL);
		if(IS_DRAWER_WIDGET(parent)) {
			BasePWidget *basep = BASEP_WIDGET(parent);
			GtkWidget *grandparent = PANEL_WIDGET(basep->panel)->master_widget->parent;
			GtkWidget *grandparentw =
				PANEL_WIDGET(grandparent)->panel_parent;
			drawer_widget_close_drawer (DRAWER_WIDGET (parent),
						    grandparentw);
		}
	}

	panel_quit();
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (widget), GTK_SIGNAL_FUNC(logout), NULL);
}

static void  
drag_data_get_cb (GtkWidget          *widget,
		  GdkDragContext     *context,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint               time,
		  gpointer            data)
{
	char *type = data;
	char *foo;

	g_return_if_fail (type != NULL);

	foo = g_strdup_printf ("%s:%d", type, find_applet (widget));

	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)foo,
				strlen (foo));

	g_free (foo);
}

static GtkWidget *
create_logout_widget (void)
{
        static GtkTargetEntry dnd_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};
	GtkWidget *button;
	char *pixmap_name;

	pixmap_name = gnome_pixmap_file ("gnome-term-night.png");

	button = button_widget_new (pixmap_name, -1,
				    MISC_TILE,
				    FALSE,
				    ORIENT_UP,
				    _("Log out"));

	/*A hack since this function only pretends to work on window
	  widgets (which we actually kind of are) this will select
	  some (already selected) events on the panel instead of
	  the button window (where they are also selected) but
	  we don't mind*/
	GTK_WIDGET_UNSET_FLAGS (button, GTK_NO_WINDOW);
	gtk_drag_source_set (button,
			     GDK_BUTTON1_MASK,
			     dnd_targets, 1,
			     GDK_ACTION_COPY | GDK_ACTION_MOVE);
	GTK_WIDGET_SET_FLAGS (button, GTK_NO_WINDOW);

	gtk_signal_connect (GTK_OBJECT (button), "drag_data_get",
			    GTK_SIGNAL_FUNC (drag_data_get_cb),
			    "LOGOUT");

	g_free (pixmap_name);
	gtk_tooltips_set_tip (panel_tooltips, button, _("Log out of GNOME"), NULL);

	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (logout), NULL);

	return button;
}

void
load_logout_applet(PanelWidget *panel, int pos, gboolean exactpos)
{
	GtkWidget *logout;

	logout = create_logout_widget();
	if(!logout)
		return;

	if (!register_toy(logout, NULL, NULL, panel,
			  pos, exactpos, APPLET_LOGOUT))
		return;

	applet_add_callback(applets_last->data, "help",
			    GNOME_STOCK_PIXMAP_HELP,
			    _("Help"));
}

static GtkWidget *
create_lock_widget(void)
{
        static GtkTargetEntry dnd_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};
	GtkWidget *button;
	char *pixmap_name;

	pixmap_name = gnome_pixmap_file("gnome-lockscreen.png");

	button = button_widget_new (pixmap_name, -1,
				    MISC_TILE,
				    FALSE,
				    ORIENT_UP,
				    _("Lock screen"));

	/*A hack since this function only pretends to work on window
	  widgets (which we actually kind of are) this will select
	  some (already selected) events on the panel instead of
	  the button window (where they are also selected) but
	  we don't mind*/
	GTK_WIDGET_UNSET_FLAGS (button, GTK_NO_WINDOW);
	gtk_drag_source_set (button,
			     GDK_BUTTON1_MASK,
			     dnd_targets, 1,
			     GDK_ACTION_COPY | GDK_ACTION_MOVE);
	GTK_WIDGET_SET_FLAGS (button, GTK_NO_WINDOW);

	gtk_signal_connect (GTK_OBJECT (button), "drag_data_get",
			    GTK_SIGNAL_FUNC (drag_data_get_cb),
			    "LOCK");

	g_free (pixmap_name);
	gtk_tooltips_set_tip (panel_tooltips, button, _("Lock screen"), NULL);

	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (panel_lock), NULL);

	return button;
}

void
load_lock_applet(PanelWidget *panel, int pos, gboolean exactpos)
{
	GtkWidget *lock;
	lock = create_lock_widget();

	if(!lock)
		return;
	if (!register_toy(lock, NULL, NULL, panel, pos, 
			  exactpos, APPLET_LOCK))
		return;

        /*
	  <jwz> Blank Screen Now
	  <jwz> Lock Screen Now
	  <jwz> Kill Daemon
	  <jwz> Restart Daemon
	  <jwz> Preferences
	  <jwz> (or "configuration" instead?  whatever word you use)
	  <jwz> those should do xscreensaver-command -activate, -lock, -exit...
	  <jwz> and "xscreensaver-command -exit ; xscreensaver &"
	  <jwz> and "xscreensaver-demo"
	*/

	applet_add_callback(applets_last->data, "activate",
			    NULL, _("Blank Screen Now"));
	applet_add_callback(applets_last->data, "lock",
			    NULL, _("Lock Screen Now"));
	applet_add_callback(applets_last->data, "exit",
			    NULL, _("Kill Daemon"));
	applet_add_callback(applets_last->data, "restart",
			    NULL, _("Restart Daemon"));
	if ( ! commie_mode)
		applet_add_callback(applets_last->data, "prefs",
				    NULL, _("Preferences"));
	applet_add_callback(applets_last->data, "help",
			    GNOME_STOCK_PIXMAP_HELP,
			    _("Help"));
}


