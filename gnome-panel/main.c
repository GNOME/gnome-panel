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

#include "panel-include.h"

extern int config_sync_timeout;
extern GSList *applets_to_sync;
extern int panels_to_sync;
extern int globals_to_sync;
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

/* True if parsing determined that all the work is already done.  */
char *just_exit = NULL;

/*I guess this should be called after we load up, but the problem is
  we never know when all the applets are going to finish loading and
  we don't want to clean the file before they load up, so now we
  only call it on the discard cmdline argument*/
static void
discard_session (char *id)
{
	char *sess;

	sess = g_strconcat (gnome_user_dir,"/panel.d/Session-", id, NULL);
	remove_directory(sess,FALSE);
	g_free (sess);

	gnome_config_sync ();
}

static void
parse_an_arg (poptContext state,
              enum poptCallbackReason reason,
              const struct poptOption *opt,
              const char *arg, void *data)
{
  if(opt->val == 0) {
    *((char **)data) = (char *)arg;
  }
}

static struct poptOption options[] = {
  { NULL, '\0', POPT_ARG_CALLBACK, parse_an_arg, 0},
  {"discard-session", '\0', POPT_ARG_STRING, &just_exit,0, N_("Discard session"),
   N_("ID")},
  {NULL, '\0', 0, NULL, 0}
};

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

int
main(int argc, char **argv)
{
	CORBA_ORB orb;
	CORBA_Environment ev;
	
	panel_cfg_path = g_strdup("/panel.d/default/");
	old_panel_cfg_path = g_strdup("/panel.d/default/");

	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	CORBA_exception_init(&ev);
	orb = gnome_CORBA_init_with_popt_table("panel", VERSION,
					       &argc, argv, options, 0, NULL,
					       GNORBA_INIT_SERVER_FUNC, &ev);
	CORBA_exception_free(&ev);

	panel_corba_gtk_init(orb);

	if (just_exit) {
	  gnome_client_disable_master_connection ();
	  discard_session (just_exit);
	  return 0;
	}

	client = gnome_master_client ();

	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
			    GTK_SIGNAL_FUNC (panel_session_save), NULL);
	gtk_object_set_data(GTK_OBJECT(client),"argv0",g_strdup(argv[0]));
	gtk_signal_connect (GTK_OBJECT (client), "die",
			    GTK_SIGNAL_FUNC (panel_session_die), NULL);

	if (GNOME_CLIENT_CONNECTED (client)) {
		char *session_id;

		session_id= gnome_client_get_previous_id (client);

		if(session_id) {
			g_free(old_panel_cfg_path);
			old_panel_cfg_path = g_strconcat("/panel.d/Session-",
							 session_id,"/",
							 NULL);
		}
		puts("connected to session manager");
	}

	/* Tell session manager how to run us.  */
	gnome_client_set_clone_command (client, 1, argv);
	gnome_client_set_restart_command (client, 1, argv);

	panel_tooltips = gtk_tooltips_new();

	/*set the globals*/
	load_up_globals();
	
	init_user_panels();

	init_user_applets();

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
	
	/* I use the glue code to avoid making this a C++ file */
	gtk_main ();

	return 0;
}
