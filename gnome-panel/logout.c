/* logout.c - Panel applet to end current session.  */
/* Original author unknown. CORBAized by Elliot Lee */
/* uncorbized by George Lebl */

#include <config.h>
#include <string.h>

#include <libgnome/libgnome.h>

#include "logout.h"

#include "applet.h"
#include "drawer-widget.h"
#include "menu.h"
#include "panel-config-global.h"
#include "panel.h"
#include "session.h"

extern GtkTooltips *panel_tooltips;

extern GlobalConfig global_config;
extern gboolean commie_mode;

static void
logout (GtkWidget *widget)
{
	g_signal_handlers_block_by_func (G_OBJECT (widget), logout, NULL);

	if (global_config.drawer_auto_close) {
		GtkWidget *parent = PANEL_WIDGET(widget->parent)->panel_parent;
		g_return_if_fail(parent!=NULL);
		if(DRAWER_IS_WIDGET(parent)) {
			BasePWidget *basep = BASEP_WIDGET(parent);
			GtkWidget *grandparent = PANEL_WIDGET(basep->panel)->master_widget->parent;
			GtkWidget *grandparentw =
				PANEL_WIDGET(grandparent)->panel_parent;
			drawer_widget_close_drawer (DRAWER_WIDGET (parent),
						    grandparentw);
		}
	}

	panel_quit();

	g_signal_handlers_unblock_by_func (G_OBJECT (widget), logout, NULL);
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

	foo = g_strdup_printf ("%s:%d", type, panel_find_applet (widget));

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

	pixmap_name = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
						 "gnome-term-night.png", TRUE, NULL);

	button = button_widget_new (pixmap_name, -1,
				    FALSE,
				    PANEL_ORIENT_UP,
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

	g_signal_connect (G_OBJECT (button), "drag_data_get",
			    G_CALLBACK (drag_data_get_cb),
			    "LOGOUT");

	g_free (pixmap_name);
	gtk_tooltips_set_tip (panel_tooltips, button, _("Log out of GNOME"), NULL);

	g_signal_connect (G_OBJECT (button), "clicked",
			    G_CALLBACK (logout), NULL);

	return button;
}

void
load_logout_applet (PanelWidget *panel,
		    gint         pos,
		    gboolean     exactpos,
		    gboolean     use_default)
{
	GtkWidget  *logout;
	AppletInfo *info;

	logout = create_logout_widget ();
	if (!logout)
		return;

	info = panel_applet_register (logout, NULL, NULL, panel,
				      pos, exactpos, APPLET_LOGOUT,
				      NULL);
	if (!info)
		return;

	panel_applet_add_callback (info, "help", GTK_STOCK_HELP, _("Help"));
}

static GtkWidget *
create_lock_widget(void)
{
        static GtkTargetEntry dnd_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};
	GtkWidget *button;
	char *pixmap_name;

	pixmap_name = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
						 "gnome-lockscreen.png",
						 TRUE, NULL);

	button = button_widget_new (pixmap_name, -1,
				    FALSE,
				    PANEL_ORIENT_UP,
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

	g_signal_connect (G_OBJECT (button), "drag_data_get",
			    G_CALLBACK (drag_data_get_cb),
			    "LOCK");

	g_free (pixmap_name);
	gtk_tooltips_set_tip (panel_tooltips, button, _("Lock screen"), NULL);

	g_signal_connect (G_OBJECT (button), "clicked",
			    G_CALLBACK (panel_lock), NULL);

	return button;
}

void
load_lock_applet (PanelWidget *panel,
		  gint         pos,
		  gboolean     exactpos,
		  gboolean     use_default)
{
	GtkWidget  *lock;
	AppletInfo *info;

	lock = create_lock_widget ();

	if (!lock)
		return;

	info = panel_applet_register (lock, NULL, NULL, panel, pos,
				      exactpos, APPLET_LOCK, NULL);
	if (!info)
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

	panel_applet_add_callback (info, "activate", NULL, _("Blank Screen Now"));
	panel_applet_add_callback (info, "lock", NULL, _("Lock Screen Now"));
	panel_applet_add_callback (info, "exit", NULL, _("Kill Daemon"));
	panel_applet_add_callback (info, "restart", NULL, _("Restart Daemon"));

	if (!commie_mode)
		panel_applet_add_callback (info, "prefs", NULL, _("Preferences"));

	panel_applet_add_callback (info, "help", GTK_STOCK_HELP, _("Help"));
}
