/* Gnome panel: Initialization routines
 * (C) 1997,1998,1999,2000 the Free Software Foundation
 * (C) 2000 Eazel, Inc.
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 *          George Lebl
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>

#include <libbonoboui.h>
#include <bonobo-activation/bonobo-activation.h>

#include "panel-main.h"

#include "conditional.h"
#include "drawer-widget.h"
#include "menu-fentry.h"
#include "menu.h"
#include "multiscreen-stuff.h"
#include "panel-util.h"
#include "panel-gconf.h"
#include "panel-config-global.h"
#include "panel-shell.h"
#include "session.h"
#include "status.h"
#include "xstuff.h"

extern int config_sync_timeout;

extern GSList *panels;

extern GSList *applets;

extern gboolean commie_mode;
extern GlobalConfig global_config;

/*list of all panel widgets created*/
extern GSList *panel_list;

GtkTooltips *panel_tooltips = NULL;

GnomeClient *client = NULL;

char *kde_menudir = NULL;
char *kde_icondir = NULL;
char *kde_mini_icondir = NULL;

static gchar *profile_name;

static gboolean
menu_age_timeout(gpointer data)
{
	GSList *li;
	for(li=applets;li!=NULL;li=g_slist_next(li)) {
		AppletInfo *info = li->data;
		if(info->menu && info->menu_age++>=6 &&
		   !GTK_WIDGET_VISIBLE(info->menu)) {
			gtk_widget_unref(info->menu);
			info->menu = NULL;
			info->menu_age = 0;
		}
		/*if we are allowed to, don't destroy applet menus*/
		if(!global_config.keep_menus_in_memory &&
		   info->type == APPLET_MENU) {
			Menu *menu = info->data;
			if(menu->menu && menu->age++>=6 &&
			   !GTK_WIDGET_VISIBLE(menu->menu)) {
				gtk_widget_unref(menu->menu);
				menu->menu = NULL;
				menu->age = 0;
			}
		}
	}
	
	/*skip panel menus if we are memory hungry*/
	if(global_config.keep_menus_in_memory)
		return TRUE;
	
	for(li = panel_list; li != NULL; li = g_slist_next(li)) {
		PanelData *pd = li->data;
		if(pd->menu && pd->menu_age++>=6 &&
		   !GTK_WIDGET_VISIBLE(pd->menu)) {
			gtk_widget_unref(pd->menu);
			pd->menu = NULL;
			pd->menu_age = 0;
		}
	}

	return TRUE;
}

/* Note: similar function is in gnome-desktop-item !!! */

static void
find_kde_directory (void)
{
	int i;
	const char *kdedir = g_getenv ("KDEDIR");
	char *try_prefixes[] = {
		"/usr",
		"/opt/kde",
		"/usr/local",
		"/kde",
		NULL
	};
	if (kdedir != NULL) {
		kde_menudir = g_build_filename (kdedir, "share", "applnk", NULL);
		kde_icondir = g_build_filename (kdedir, "share", "icons", NULL);
		kde_mini_icondir = g_build_filename (kdedir, "share", "icons", "mini", NULL);
		return;
	}

	/* if what configure gave us works use that */
	if (g_file_test (KDE_MENUDIR, G_FILE_TEST_IS_DIR)) {
		kde_menudir = g_strdup (KDE_MENUDIR);
		kde_icondir = g_strdup (KDE_ICONDIR);
		kde_mini_icondir = g_strdup (KDE_MINI_ICONDIR);
		return;
	}

	for (i = 0; try_prefixes[i] != NULL; i++) {
		char *try;
		try = g_build_filename (try_prefixes[i], "share", "applnk", NULL);
		if (g_file_test (try, G_FILE_TEST_IS_DIR)) {
			kde_menudir = try;
			kde_icondir = g_build_filename (try_prefixes[i], "share", "icons", NULL);
			kde_mini_icondir = g_build_filename (try_prefixes[i], "share", "icons", "mini", NULL);
			return;
		}
		g_free(try);
	}

	/* absolute fallback, these don't exist, but we're out of options
	   here */
	kde_menudir = g_strdup (KDE_MENUDIR);
	kde_icondir = g_strdup (KDE_ICONDIR);
	kde_mini_icondir = g_strdup (KDE_MINI_ICONDIR);
}

static void
kill_free_drawers (void)
{
	GSList *li;
	GSList *to_destroy = NULL;
	
	for (li = panel_list; li != NULL; li = li->next) {
		PanelData *pd = li->data;
		if (DRAWER_IS_WIDGET (pd->panel) &&
		    PANEL_WIDGET (BASEP_WIDGET (pd->panel)->panel)->master_widget == NULL) {
			status_unparent (pd->panel);
			to_destroy = g_slist_prepend (to_destroy, pd->panel);
		}
	}

	g_slist_foreach (to_destroy, (GFunc)gtk_widget_destroy, NULL);
	g_slist_free (to_destroy);
}

static void
tell_user_Im_on_crack (void)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new
	    (NULL /* parent */,
	     0 /* flags */,
	     GTK_MESSAGE_WARNING,
	     GTK_BUTTONS_YES_NO,
	     _("This is the GNOME2 panel, it is likely that it will crash,\n"
	       "destroy configuration, cause another world war, and most\n"
	       "likely just plain not work.  Use at your own risk.\n\n"
	       "Do you really want to run it?"));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_YES)
		exit (0);

	gtk_widget_destroy (dialog);
}

static void
session_notify_global_changes (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data) {
	session_read_global_config ();
}

static const struct poptOption options[] = {
  {"profile", '\0', POPT_ARG_STRING, &profile_name, 0, N_("Specify a profile name to load"), NULL},
  {NULL, '\0', 0, NULL, 0}
};


int
main(int argc, char **argv)
{
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("panel", VERSION,
			    LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_POPT_TABLE, options,
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    NULL);

	bonobo_activate ();
	
	if (g_getenv ("I_LOVE_PANEL_CRACK") == NULL)
		tell_user_Im_on_crack ();

	if (profile_name == NULL) {
		/* FIXME: Do this properly. just a temporary hack */
		printf ("Couldn't find profile! - Loading up default . . .\n");
		session_set_current_profile ("default");
	} else {
		session_set_current_profile (profile_name);
	}

	/*
	 * Let applets spew.
	 */
	putenv ("BONOBO_ACTIVATION_DEBUG_OUTPUT=1");

	if (!panel_shell_register ())
		return -1;
	find_kde_directory();

	client = gnome_master_client ();

	/* FIXME - get the session stuff working properly */

	g_signal_connect (G_OBJECT (client), "save_yourself",
			  G_CALLBACK (panel_session_save), NULL);
	g_signal_connect (G_OBJECT (client), "die",
			  G_CALLBACK (panel_session_die), NULL);

	panel_tooltips = gtk_tooltips_new ();

	xstuff_init ();
	multiscreen_init ();

	/* FIXME - need to start using gconf to this 
	load_system_wide (); */


	panel_gconf_add_dir ("/apps/panel/global");
	panel_gconf_add_dir ("/desktop/gnome/menus");

	/* set the globals, it is important this is before
	 * init_user_applets */
	session_read_global_config ();

	/* Do the notification stuff */
	panel_gconf_notify_add ("/apps/panel/global", session_notify_global_changes, NULL);

	init_menus ();

	session_load ();	

	kill_free_drawers ();
#ifdef FIXME
	panel_menu_session_load_tornoffs ();
#endif
	/*add forbidden lists to ALL panels*/
	g_slist_foreach (panels,
			 (GFunc)panel_widget_add_forbidden,
			 NULL);

	/*this will make the drawers be hidden for closed panels etc ...*/
	send_state_change ();

	panel_session_setup_config_sync ();

	/* add some timeouts */
	gtk_timeout_add (10*1000, menu_age_timeout, NULL);

	status_applet_create_offscreen ();

	gtk_main ();

	return 0;
}
