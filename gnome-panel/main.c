/* Gnome panel: Initialization routines
 * (C) 1997 the Free Software Foundation
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 *          George Lebl
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "panel-include.h"

extern int config_sync_timeout;
extern int applets_to_sync;
extern int panels_to_sync;
extern int need_complete_save;

extern GSList *panels;

extern GSList *applets;
extern GSList *applets_last;
extern int applet_count;

extern GlobalConfig global_config;
extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

/*list of all panel widgets created*/
extern GSList *panel_list;

GtkTooltips *panel_tooltips = NULL;

GnomeClient *client = NULL;

/*a list of started extern applet child processes*/
extern GList * children;

GList *check_swallows = NULL;

static int
menu_age_timeout(gpointer data)
{
	GSList *li;
	for(li=applets;li!=NULL;li=g_slist_next(li)) {
		AppletInfo *info = li->data;
		if(info->menu && info->menu_age++>=6 &&
		   !GTK_WIDGET_VISIBLE(info->menu)) {
			gtk_widget_destroy(info->menu);
			info->menu = NULL;
			info->menu_age = 0;
		}
		/*if we are allowed to, don't destroy applet menus*/
		if(!global_config.hungry_menus &&
		   info->type == APPLET_MENU) {
			Menu *menu = info->data;
			if(menu->menu && menu->age++>=6 &&
			   !GTK_WIDGET_VISIBLE(menu->menu)) {
				gtk_widget_destroy(menu->menu);
				menu->menu = NULL;
				menu->age = 0;
			}
		}
	}
	
	/*skip panel menus if we are memory hungry*/
	if(global_config.hungry_menus)
		return TRUE;
	
	for(li = panel_list; li != NULL; li = g_slist_next(li)) {
		PanelData *pd = li->data;
		if(pd->menu && pd->menu_age++>=6 &&
		   !GTK_WIDGET_VISIBLE(pd->menu)) {
			gtk_widget_destroy(pd->menu);
			pd->menu = NULL;
			pd->menu_age = 0;
		}
	}

	return TRUE;
}

static int
try_config_sync(gpointer data)
{
	panel_config_sync();
	return TRUE;
}

static int
ignore_x_error(Display* d, XErrorEvent* e)
{
	return 0;
}

static int
get_window_id(Window win, char *title, guint32 *wid)
{
	Window root_return;
	Window parent_return;
	Window *children = NULL;
	unsigned int nchildren;
	unsigned int i;
	char *tit;
	int ret = FALSE;
	
	if(XFetchName(GDK_DISPLAY(), win, &tit) && tit) {
		if(strstr(tit,title)!=NULL) {
			if(wid) *wid = win;
			ret = TRUE;
		}
		XFree(tit);
	}
	
	if(ret) return TRUE;

	XQueryTree(GDK_DISPLAY(),
		   win,
		   &root_return,
		   &parent_return,
		   &children,
		   &nchildren);
	
	/*otherwise we got a problem*/
	if(children) {
		for(i=0;!ret && i<nchildren;i++)
			ret=get_window_id(children[i],title,wid);
		XFree(children);
	}
	return ret;
}

static GdkFilterReturn
event_filter(GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	XEvent *xevent;

	xevent = (XEvent *)gdk_xevent;
	if (xevent->type == MapNotify && check_swallows) {
		int (*oldErrorHandler)(Display*, XErrorEvent*);
		GList *li;
		int remove;

		oldErrorHandler = XSetErrorHandler(ignore_x_error);

		remove = 0;
		for(li = check_swallows; li; li = g_list_next(li)) {
			Swallow *swallow = li->data;
			if(get_window_id(xevent->xmap.window,swallow->title,
					 &(swallow->wid))) {
				gtk_socket_steal(GTK_SOCKET(swallow->socket),swallow->wid);
				li->data = NULL;
				remove++;
			}
		}
		while(remove--)
			check_swallows = g_list_remove(check_swallows,NULL);
		
		if(!check_swallows)
			XSelectInput(GDK_DISPLAY(), GDK_ROOT_WINDOW(), 0);
		
		XSetErrorHandler(oldErrorHandler);
	}
	if ((event->any.window) &&
	    (gdk_window_get_type(event->any.window) == GDK_WINDOW_FOREIGN))
		return GDK_FILTER_REMOVE;
	else
		return GDK_FILTER_CONTINUE;
}


int
main(int argc, char **argv)
{
	CORBA_ORB orb;
	CORBA_Environment ev;
	gint duplicate;
	gchar *real_global_path;
	
	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	CORBA_exception_init(&ev);
	orb = gnome_CORBA_init("panel", VERSION,
			       &argc, argv,
			       GNORBA_INIT_SERVER_FUNC, &ev);
	CORBA_exception_free(&ev);

	switch (panel_corba_gtk_init (orb)) {
	case 0: 
		duplicate = 0;
		break; /* success */
	case -4: {
		GtkWidget* box = gnome_question_dialog
			(_("I've detected a panel already running.\n"
			   "Start another panel as well?\n" 
			   "(The new panel will not be restarted.)"), NULL, NULL);
		if (gnome_dialog_run_and_close (GNOME_DIALOG (box)))
			return 0;
		duplicate = 1;
		break;
	}
	default: {
		GtkWidget *box = gnome_error_dialog
			(_("There was a problem registering the panel "
			   "with the GOAD server.\n"
			   "The panel will now exit."));
		gnome_dialog_run_and_close (GNOME_DIALOG (box));
		return 0;
		break;
	}
	}

	client = gnome_master_client ();

	gnome_client_set_restart_style (client, duplicate 
					? GNOME_RESTART_NEVER 
					: GNOME_RESTART_IMMEDIATELY);

	gnome_client_set_priority(client,40);


	if (gnome_client_get_flags(client) & GNOME_CLIENT_RESTORED)
		old_panel_cfg_path = g_strdup (gnome_client_get_config_prefix (client));
	else
		old_panel_cfg_path = g_strdup ("/panel.d/default/");

#ifndef PER_SESSION_CONFIGURATION
	real_global_path = gnome_config_get_real_path (old_panel_cfg_path);
	if (!g_file_exists (real_global_path)) {
		g_free (old_panel_cfg_path);
		old_panel_cfg_path = g_strdup ("/panel.d/default/");
	}
	g_free (real_global_path);
#endif /* !PER_SESSION_CONFIGURATION */

	gnome_client_set_global_config_prefix (client, PANEL_CONFIG_PATH);
	
	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
			    GTK_SIGNAL_FUNC (panel_session_save), NULL);
	gtk_signal_connect (GTK_OBJECT (client), "die",
			    GTK_SIGNAL_FUNC (panel_session_die), NULL);

	panel_tooltips = gtk_tooltips_new();

	gnome_win_hints_init ();

	/* read, convert and remove old config */
	convert_old_config();

	/*set the globals*/
	load_up_globals();
	
	init_menus();
	
	init_user_panels();

	init_user_applets();

	gnome_triggers_do("Session startup", NULL, "gnome", "login", NULL);

	/*add forbidden lists to ALL panels*/
	g_slist_foreach(panels,(GFunc)panel_widget_add_forbidden,NULL);

	/*this will make the drawers be hidden for closed panels etc ...*/
	send_state_change();

	/*attempt to sync the config every 10 seconds, only if a change was
	  indicated though*/
	config_sync_timeout = gtk_timeout_add(10*1000,try_config_sync,NULL);

	gtk_timeout_add(10*1000,menu_age_timeout,NULL);
	
	/*load these as the last thing to prevent some races any races from
	  starting multiple goad_id's at once are libgnorba's problem*/
	load_queued_externs();

	/* set up a filter on the root window to get map requests */
	/* we will select the events later when we actually need them */
	gdk_window_add_filter(GDK_ROOT_PARENT(), event_filter, NULL);
	
	/* I use the glue code to avoid making this a C++ file */
	gtk_main ();

	return 0;
}
