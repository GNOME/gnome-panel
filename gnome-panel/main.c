/* Gnome panel: Initialization routines
 * (C) 1997 the Free Software Foundation
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 */

#include <config.h>
#include <string.h>
#include "gnome.h"
#include "applet_files.h"
#include "panel_cmds.h"
#include "applet_cmds.h"
#include "panel-widget.h"
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
load_drawer(char *name, char *iconopen, char *iconclosed, int step_size,
	int pos)
{
	PanelCommand  cmd;

	cmd.cmd = PANEL_CMD_CREATE_DRAWER;
	cmd.params.create_drawer.name       = name;
	cmd.params.create_drawer.iconopen   = iconopen;
	cmd.params.create_drawer.iconclosed = iconclosed;
	cmd.params.create_drawer.step_size  = step_size;
	cmd.params.create_drawer.pos        = pos;

	panel_command(&cmd);
}

static void
load_default_applets(void)
{
	/* XXX: the IDs for these applets are hardcoded here. */

	/* Here we use NULL to request querying of default applet parameters */
	load_applet("Menu", NULL, PANEL_UNKNOWN_APPLET_POSITION);
	load_applet("Clock", NULL, PANEL_UNKNOWN_APPLET_POSITION);
	load_applet("Mail check", NULL, PANEL_UNKNOWN_APPLET_POSITION);
}

static void
load_default_drawers(void)
{
}

static void
init_user_drawers(void)
{
	char *drawer_name;
	char *drawer_iconopen;
	char *drawer_iconclosed;
	int   pos;
	int   step_size;
	char  buf[256];
	int   count,num;	

	count=gnome_config_get_int("/panel/Config/drawer_count=0");
	if(count<=0)
		load_default_drawers();
	for(num=1;num<=count;num++) {
		sprintf(buf,"/panel/Drawer_%d/name=Drawer",num);
		drawer_name = gnome_config_get_string(buf);
		sprintf(buf,"/panel/Drawer_%d/iconopen=",num);
		drawer_iconopen = gnome_config_get_string(buf);
		sprintf(buf,"/panel/Drawer_%d/iconclosed=",num);
		drawer_iconclosed = gnome_config_get_string(buf);

		sprintf(buf,"/panel/Drawer_%d/step_size=%d",num,
			PANEL_UNKNOWN_STEP_SIZE);
		step_size = gnome_config_get_int(buf);
		sprintf(buf,"/panel/Drawer_%d/geometry=%d",num,
			PANEL_UNKNOWN_APPLET_POSITION);
		pos = gnome_config_get_int(buf);
		load_drawer(drawer_name,drawer_iconopen,drawer_iconclosed,
			    step_size,pos);
		g_free(drawer_name);
		g_free(drawer_iconopen);
		g_free(drawer_iconclosed);
	}
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
		sprintf(buf,"/panel/Applet_%d/geometry=%d",num,
			PANEL_UNKNOWN_APPLET_POSITION);
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
	textdomain(PACKAGE);

	init_session_management (argc, argv);

	applet_files_init();
	panel_init();
	panel_init_applet_modules();
	init_user_drawers();
	init_user_applets();

	gtk_widget_show(GTK_WIDGET(the_panel));

	gtk_main();
	return 0;
}
