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

#include "panel-include.h"

extern int config_sync_timeout;
extern GList *applets_to_sync;
extern int panels_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

extern GArray *applets;
extern int applet_count;

extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

GtkTooltips *panel_tooltips = NULL;

GnomeClient *client = NULL;

/*a list of started extern applet child processes*/
extern GList * children;

/* True if parsing determined that all the work is already done.  */
char *just_exit = NULL;

/* The security cookie */
char *cookie = NULL;


/*I guess this should be called after we load up, but the problem is
  we never know when all the applets are going to finish loading and
  we don't want to clean the file before they load up, so now we
  only call it on the discard cmdline argument*/
static void
discard_session (char *id)
{
	char *sess;

	/*FIXME: hmm this won't work ... there needs to be a clean_dir*/
	sess = g_copy_strings ("/panel.d/Session-", id, NULL);
	/*gnome_config_clean_file (sess);*/
	g_free (sess);

	gnome_config_sync ();

	return;
}

static void
parse_an_arg (poptContext state,
              enum poptCallbackReason reason,
              const struct poptOption *opt,
              const char *arg, void *data)
{
  if(opt->val == 0) {
    *((char **)data) = arg;
  }
}

static struct poptOption options[] = {
  { NULL, '\0', POPT_ARG_CALLBACK, parse_an_arg, 0},
  {"discard-session", '\0', POPT_ARG_STRING, &just_exit,0, N_("Discard session"),
   N_("ID")},
  {NULL, '\0', 0, NULL, 0}
};

static int
try_config_sync(gpointer data)
{
	panel_config_sync();
	return TRUE;
}


int
main(int argc, char **argv)
{
	char buf[256];
	
	panel_cfg_path = g_strdup("/panel.d/default/");
	old_panel_cfg_path = g_strdup("/panel.d/default/");

	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	panel_corba_register_arguments ();

	gnome_init_with_popt_table("panel", VERSION,
				   argc, argv, options, 0, NULL);

	/* Setup the cookie */
	cookie = create_cookie ();
	g_snprintf(buf,256,"/panel/Secret/cookie-DISPLAY-%s",getenv("DISPLAY"));
	gnome_config_private_set_string (buf, cookie);
	gnome_config_sync();
	
	panel_corba_gtk_init();

	if (just_exit) {
	  gnome_client_disable_master_connection ();
	  discard_session (just_exit);
	  return 0;
	}

	client= gnome_master_client ();

	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
			    GTK_SIGNAL_FUNC (panel_session_save), NULL);
	gtk_object_set_data(GTK_OBJECT(client),"argv0",g_strdup(argv[0]));
	gtk_signal_connect (GTK_OBJECT (client), "die",
			    GTK_SIGNAL_FUNC (panel_session_die), NULL);

	if (GNOME_CLIENT_CONNECTED (client)) {
		char *session_id;

		if (gnome_cloned_client ())
		  {
		    /* This client has been resumed or is a clone of
                       another panel (i.e. gnome_cloned_client !=
                       NULL).  */
		    session_id= gnome_client_get_id (gnome_cloned_client ());
		  }
		else
		  session_id= NULL;

		if(session_id) {
			g_free(old_panel_cfg_path);
			old_panel_cfg_path = g_copy_strings("/panel.d/Session-",
							    session_id,"/",
							    NULL);
		}
		puts("connected to session manager");
	}

	/* Tell session manager how to run us.  */
	gnome_client_set_clone_command (client, 1, argv);
	gnome_client_set_restart_command (client, 1, argv);

	applets = g_array_new(FALSE, FALSE, sizeof(AppletInfo));
	applet_count = 0;

	panel_tooltips = gtk_tooltips_new();

	/*set the globals*/
	load_up_globals();
	
	init_user_panels();

	init_user_applets();

	/*add forbidden lists to ALL panels*/
	g_list_foreach(panels,(GFunc)panel_widget_add_forbidden,NULL);

	/*this will make the drawers be hidden for closed panels etc ...*/
	send_state_change();
	
	/*attempt to sync the config every 10 seconds, only if a change was
	  indicated though*/
	config_sync_timeout = gtk_timeout_add(10*1000,try_config_sync,NULL);
	
	/* I use the glue code to avoid making this a C++ file */
	panel_corba_gtk_main ("IDL:GNOME/Panel:1.0");

	return 0;
}
