/* Gnome panel: Initialization routines
 * (C) 1997 the Free Software Foundation
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 */

#include <string.h>
#include "gnome.h"
#include "applet_files.h"
#include "panel_cmds.h"
#include "applet_cmds.h"
#include "panel.h"


static void
load_applet(char *id, char *params, int pos)
{
	PanelCommand  cmd;

	cmd.cmd = PANEL_CMD_CREATE_APPLET;
	cmd.params.create_applet.id     = id;
	cmd.params.create_applet.params = params;
	cmd.params.create_applet.pos    = pos;

	panel_command(&cmd);
}


static void
load_default_applets(void)
{
	/* XXX: the IDs for these applets are hardcoded here. */

	/* Here we use NULL to request querying of default applet parameters */
	/* as position we give unknown or 0 to load from left or
	   PANEL_TABLE_SIZE-1 to load from right */
	load_applet("Menu", NULL, PANEL_UNKNOWN_APPLET_POSITION);
	load_applet("Clock", NULL, PANEL_UNKNOWN_APPLET_POSITION);
	load_applet("Mail check", NULL, PANEL_TABLE_SIZE-1);
}


static void
init_user_applets(void)
{
	char *applet_name;
	char *applet_params;
	int   pos=0;
	char  buf[256];
	int   count,num;	

	count=gnome_config_get_int("/panel/Config/applet_count=0");
	if(count<=0)
		load_default_applets();
	for(num=1;num<=count;num++) {
		sprintf(buf,"/panel/Applet_%d/id=Unknown",num);
		applet_name = gnome_config_get_string(buf);
		sprintf(buf,"/panel/Applet_%d/parameters=",num);
		applet_params = gnome_config_get_string(buf);
		sprintf(buf,"/panel/Applet_%d/geometry=0",num);
		pos = gnome_config_get_int(buf);
		load_applet(applet_name, applet_params, pos);
		g_free(applet_name);
		g_free(applet_params);
	}
}


/* FIXME: session management not complete.  In particular, we should:
   1. Actually save state in a useful way.
   2. Parse argv to get our session management key.  */
static void
init_session_management (int argc, char *argv[])
{
  char *previous_id = NULL;
  char *session_id;

  session_id = gnome_session_init (panel_session_save, NULL, NULL, NULL,
				   previous_id);
}

int
main(int argc, char **argv)
{
	gnome_init(&argc, &argv);

	init_session_management (argc, argv);

	applet_files_init();
	panel_init();
	panel_init_applet_modules();
	init_user_applets();

	gtk_widget_show(the_panel->panel->window);

	gtk_main();
	return 0;
}
