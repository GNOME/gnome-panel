/* Gnome panel: panel functionality
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Federico Mena
 *           Miguel de Icaza
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <gnome.h>
#include <gdk/gdkx.h>

#include "panel-include.h"

int config_sync_timeout = 0;
GList *applets_to_sync = NULL;
int panels_to_sync = FALSE;
int globals_to_sync = FALSE;
int need_complete_save = FALSE;

extern GArray *applets;
extern int applet_count;

extern GtkTooltips *panel_tooltips;

extern GnomeClient *client;

GlobalConfig global_config = {
		DEFAULT_AUTO_HIDE_STEP_SIZE,
		DEFAULT_EXPLICIT_HIDE_STEP_SIZE,
		DEFAULT_MINIMIZED_SIZE,
		DEFAULT_MINIMIZE_DELAY,
		TRUE, /*tooltips*/
		TRUE, /*show small icons*/
		TRUE, /*logout question*/
		PANEL_SWITCH_MOVE,
		FALSE, /*disable anims*/
		3 /*applet padding*/
	};

char *panel_cfg_path=NULL;
char *old_panel_cfg_path=NULL;

/*list of all panel widgets created*/
extern GList *panel_list;

/*a list of started extern applet child processes*/
extern GList * children;

/*send the tooltips state to all external applets*/
static void
send_tooltips_state(int enabled)
{
	GList *list;
	
	for(list = children;list!=NULL;list = g_list_next(list)) {
		AppletChild *child = list->data;
		AppletInfo *info = get_applet_info(child->applet_id);
		Extern *ext = info->data;
		g_assert(ext);
		/*if it's not set yet, don't send it, it will be sent when
		  the ior is discovered anyhow, so this would be redundant
		  anyway*/
		if(ext->ior)
			send_applet_tooltips_state(ext->ior,enabled);
	}
}

void
apply_global_config(void)
{
	int i;
	panel_widget_change_global(global_config.explicit_hide_step_size,
				   global_config.auto_hide_step_size,
				   global_config.drawer_step_size,
				   global_config.minimized_size,
				   global_config.minimize_delay,
				   global_config.movement_type,
				   global_config.disable_animations,
				   global_config.applet_padding);
	if(global_config.tooltips_enabled)
		gtk_tooltips_enable(panel_tooltips);
	else
		gtk_tooltips_disable(panel_tooltips);
	set_show_small_icons();
	send_tooltips_state(global_config.tooltips_enabled);
	button_widget_tile_enable(global_config.tiles_enabled);
	for(i=0;i<LAST_TILE;i++) {
		button_widget_load_tile(i, global_config.tile_up[i],
					global_config.tile_down[i],
					global_config.tile_border[i],
					global_config.tile_depth[i]);
	}
}

/*shouldn't this be in gnome-dentry?? :)*/
static void
gnome_desktop_entry_save_no_sync (GnomeDesktopEntry *dentry)
{
	char *prefix;
	
	g_assert (dentry != NULL);
	g_assert (dentry->location != NULL);

	prefix = g_copy_strings ("=", dentry->location, "=/Desktop Entry", NULL);

	gnome_config_clean_section (prefix);

	prefix = g_copy_strings (prefix, "/", NULL);
	gnome_config_push_prefix (prefix);
	g_free (prefix);

	if (dentry->name)
		gnome_config_set_translated_string ("Name", dentry->name);

	if (dentry->comment)
		gnome_config_set_translated_string ("Comment", dentry->comment);

	if (dentry->exec)
		gnome_config_set_vector ("Exec", dentry->exec_length,
					 (const char * const *) dentry->exec);

	if (dentry->tryexec)
		gnome_config_set_string ("TryExec", dentry->tryexec);

	if (dentry->icon)
		gnome_config_set_string ("Icon", dentry->icon);

	if (dentry->geometry)
		gnome_config_set_string ("Geometry", dentry->geometry);
	
	if (dentry->docpath)
		gnome_config_set_string ("DocPath", dentry->docpath);

	gnome_config_set_bool ("Terminal", dentry->terminal);
	gnome_config_set_bool ("MultipleArgs", dentry->multiple_args);
	
	if (dentry->type)
		gnome_config_set_string ("Type", dentry->type);

	gnome_config_pop_prefix ();
}


static void
save_applet_configuration(int num)
{
	char           path[256];
	int            panel_num;
	PanelWidget   *panel;
	AppletData    *ad;
	AppletInfo    *info = get_applet_info(num);
	
	g_return_if_fail(info!=NULL);

	g_snprintf(path,256, "%sApplet_Config/Applet_%d/", panel_cfg_path, num+1);
	gnome_config_push_prefix(path);

	/*obviously no need for saving*/
	if(info->type==APPLET_EXTERN_PENDING ||
	   info->type==APPLET_EXTERN_RESERVED ||
	   info->type==APPLET_EMPTY) {
		gnome_config_set_string("id", EMPTY_ID);
		gnome_config_pop_prefix();
		return;
	}

	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);
	ad = gtk_object_get_data(GTK_OBJECT(info->widget),PANEL_APPLET_DATA);

	if((panel_num = g_list_index(panels,panel)) == -1) {
		gnome_config_set_string("id", EMPTY_ID);
		gnome_config_pop_prefix();
		return;
	}

	switch(info->type) {
	case APPLET_EXTERN:
		{
			char *globalcfg;
			Extern *ext = info->data;
/*this should no longer be needed on our side, we don't write anything
  to the applet's file and it doesn't write anything to ours*/
#if 0
			/*sync before the applet does it's stuff*/
			gnome_config_sync();
			/*I think this should be done at sync and also that
			  there should be some flocking ... but this works
			  for now*/
			gnome_config_drop_all();
#endif

			globalcfg = g_copy_strings(panel_cfg_path,
						   "Applet_All_Extern/",NULL);

			/*this is the file path we pass to the applet for it's
			  own config, this is a separate file, so that we */
			g_snprintf(path,256, "%sApplet_%d_Extern/",
				   panel_cfg_path, num+1);
			/*have the applet do it's own session saving*/
			if(send_applet_session_save(ext->ior,info->applet_id,
						    path, globalcfg)) {

				gnome_config_set_string("id", EXTERN_ID);
				gnome_config_set_string("execpath",
							ext->path);
				gnome_config_set_string("parameters",
							ext->params);
			} else {
				g_free(globalcfg);
				gnome_config_set_string("id", EMPTY_ID);
				gnome_config_pop_prefix();
				return;
			}
			g_free(globalcfg);
			break;
		}
	case APPLET_DRAWER: 
		{
			int i;
			Drawer *drawer = info->data;

			gnome_config_set_string("id", DRAWER_ID);

			i = g_list_index(panels,
					 DRAWER_WIDGET(drawer->drawer)->panel);
			if(i>=0)
				gnome_config_set_int("parameters",i);
			else
				g_warning("Drawer not associated with applet!");
			gnome_config_set_string("pixmap",
						drawer->pixmap);
			gnome_config_set_string("tooltip",
						drawer->tooltip);
			break;
		}
	case APPLET_SWALLOW:
		{
			Swallow *swallow = info->data;
			gnome_config_set_string("id", SWALLOW_ID);
			gnome_config_set_string("parameters",
						swallow->title);
			gnome_config_set_string("execpath",
						swallow->path);
			gnome_config_set_int("width",swallow->width);
			gnome_config_set_int("height",swallow->height);
			break;
		}
	case APPLET_MENU:
		{
			Menu *menu = info->data;
			gnome_config_set_string("id", MENU_ID);
			gnome_config_set_string("parameters",
						menu->path);
			gnome_config_set_int("main_menu_type",
					     menu->main_menu_type);
			break;
		}
	case APPLET_LAUNCHER:
		{
			Launcher *launcher = info->data;
			/*we set the .desktop to be in the panel config
			  dir*/
			g_snprintf(path,256, "%s/%sApplet_%d.desktop",
				   gnome_user_dir,panel_cfg_path, num+1);
			g_free(launcher->dentry->location);
			launcher->dentry->location = g_strdup(path);
			gnome_desktop_entry_save_no_sync(launcher->dentry);

			gnome_config_set_string("id", LAUNCHER_ID);
			gnome_config_set_string("parameters", path);
			break;
		}
	case APPLET_LOGOUT:
		gnome_config_set_string("id", LOGOUT_ID);
		break;
	default:
		g_assert_not_reached();
	}
	gnome_config_set_int("position", ad->pos);
	gnome_config_set_int("panel", panel_num);
	gnome_config_set_bool("right_stick",
			      panel_widget_is_applet_stuck(panel,
							   info->widget));
	gnome_config_pop_prefix();
}

static void
save_panel_configuration(gpointer data, gpointer user_data)
{
	char           path[256];
	char           buf[32];
	int           *num = user_data;
	PanelData     *pd = data;
	PanelWidget   *panel = get_def_panel_widget(pd->panel);

	g_snprintf(path,256, "%spanel/Panel_%d/", panel_cfg_path, (*num)++);
	gnome_config_clean_section(path);

	gnome_config_push_prefix (path);

	gnome_config_set_int("type",pd->type);
	
	switch(pd->type) {
	case SNAPPED_PANEL:
		{
		SnappedWidget *snapped = SNAPPED_WIDGET(pd->panel);
		gnome_config_set_int("pos", snapped->pos);
		gnome_config_set_int("mode", snapped->mode);
		gnome_config_set_int("state", snapped->state);
		gnome_config_set_bool("hidebuttons_enabled",
				      snapped->hidebuttons_enabled);
		break;
		}
	case CORNER_PANEL:
		{
		CornerWidget *corner = CORNER_WIDGET(pd->panel);
		gnome_config_set_int("pos", corner->pos);
		gnome_config_set_int("orient",panel->orient);
		gnome_config_set_int("state", corner->state);
		break;
		}
	case DRAWER_PANEL:
		{
		DrawerWidget *drawer = DRAWER_WIDGET(pd->panel);
		gnome_config_set_int("orient",drawer->orient);
		gnome_config_set_int("state", drawer->state);
		break;
		}
	default:
		g_assert_not_reached();
	}
	gnome_config_set_bool("fit_pixmap_bg", panel->fit_pixmap_bg);

	gnome_config_set_string("backpixmap",
				panel->back_pixmap ? panel->back_pixmap : "");

	g_snprintf(buf, sizeof(buf), "#%02x%02x%02x",
		   (guint)panel->back_color.red/256,
		   (guint)panel->back_color.green/256,
		   (guint)panel->back_color.blue/256);
	gnome_config_set_string("backcolor", buf);

	gnome_config_set_int("back_type", panel->back_type);

	gnome_config_pop_prefix ();
}

static void
do_session_save(GnomeClient *client,
		int complete_sync,
		GList **sync_applets,
		int sync_panels,
		int sync_globals)
{
	int num;
	char *buf;
	char *session_id;
	int i;

	session_id = gnome_client_get_id (client);
	if(session_id) {
		char *new_args[3];

		g_free(panel_cfg_path);
		panel_cfg_path = g_copy_strings("/panel.d/Session-",session_id,
						"/",NULL);

		new_args[0] = (char *) gtk_object_get_data(GTK_OBJECT(client),
							   "argv0");
		new_args[1] = "--discard-session";
		new_args[2] = session_id;
		gnome_client_set_discard_command (client, 3, new_args);
	}
	
	printf("Saving to [%s]\n",panel_cfg_path);

	/*take out the trailing / then call the clean_file function,
	  otherwise it will make runaway directories*/
	buf = g_strdup(panel_cfg_path);
	if(buf && *buf)
		buf[strlen(buf)-1]='\0';
	gnome_config_clean_file(buf);
	g_free(buf);

	/*DEBUG*/printf("Saving session: 1"); fflush(stdout);
	if(complete_sync) {
		for(i=0;i<applet_count;i++)
			save_applet_configuration(i);
	} else {
		while(sync_applets && *sync_applets) {
			/*printf("\nsaving: %d\n",GPOINTER_TO_INT((*sync_applets)->data));*/
			save_applet_configuration(GPOINTER_TO_INT((*sync_applets)->data));
			*sync_applets = my_g_list_pop_first(*sync_applets);
		}
	}
	/*DEBUG*/printf(" 2"); fflush(stdout);

	buf = g_copy_strings(panel_cfg_path,"panel/Config/",NULL);
	gnome_config_push_prefix (buf);
	g_free(buf);

	if(complete_sync)
		gnome_config_set_int ("applet_count", applet_count);
	/*DEBUG*/printf(" 3"); fflush(stdout);
	if(complete_sync || sync_panels) {
		num = 1;
		g_list_foreach(panel_list, save_panel_configuration,&num);
		gnome_config_set_int("panel_count",num-1);
	}
	/*DEBUG*/printf(" 4"); fflush(stdout);

	if(complete_sync || sync_globals) {
		/*global options*/
		gnome_config_set_int("auto_hide_step_size",
				     global_config.auto_hide_step_size);
		gnome_config_set_int("explicit_hide_step_size",
				     global_config.explicit_hide_step_size);
		gnome_config_set_int("drawer_step_size",
				     global_config.drawer_step_size);
		gnome_config_set_int("minimized_size",
				     global_config.minimized_size);
		gnome_config_set_int("minimize_delay",
				     global_config.minimize_delay);
		gnome_config_set_int("movement_type",
				     (int)global_config.movement_type);
		gnome_config_set_bool("tooltips_enabled",
				      global_config.tooltips_enabled);
		gnome_config_set_bool("show_small_icons",
				      global_config.show_small_icons);
		gnome_config_set_bool("prompt_for_logout",
				      global_config.prompt_for_logout);
		gnome_config_set_bool("disable_animations",
				      global_config.disable_animations);
		gnome_config_set_int("applet_padding",
				     global_config.applet_padding);
		gnome_config_set_bool("tiles_enabled",
				      global_config.tiles_enabled);
		for(i=0;i<LAST_TILE;i++) {
			char buf[256];
			g_snprintf(buf,256,"tile_up_%d",i);
			gnome_config_set_string(buf,global_config.tile_up[i]);
			g_snprintf(buf,256,"tile_down_%d",i);
			gnome_config_set_string(buf,global_config.tile_down[i]);
			g_snprintf(buf,256,"tile_border_%d",i);
			gnome_config_set_int(buf,global_config.tile_border[i]);
			g_snprintf(buf,256,"tile_depth_%d",i);
			gnome_config_set_int(buf,global_config.tile_depth[i]);
		}
	}

	gnome_config_pop_prefix ();
	gnome_config_sync();
	
	/*DEBUG*/puts("");
}

void
panel_config_sync(void)
{
	if(need_complete_save ||
	   applets_to_sync ||
	   panels_to_sync ||
	   globals_to_sync) {
		do_session_save(client,need_complete_save,
				&applets_to_sync,panels_to_sync,globals_to_sync);
		need_complete_save = FALSE;
		g_list_free(applets_to_sync);
		applets_to_sync = NULL;
		panels_to_sync = FALSE;
		globals_to_sync = FALSE;
	}
}


/* This is called when the session manager requests a shutdown.  It
   can also be run directly when we don't detect a session manager.
   We assume no interaction is done by the applets.  And we ignore the
   other arguments for now.  Yes, this is lame.  */
/* update: some SM stuff implemented but we still ignore most of the
   arguments now*/
int
panel_session_save (GnomeClient *client,
		    int phase,
		    GnomeSaveStyle save_style,
		    int is_shutdown,
		    GnomeInteractStyle interact_style,
		    int is_fast,
		    gpointer client_data)
{
	do_session_save(client,TRUE,NULL,FALSE,FALSE);
	/* Always successful.  */
	return TRUE;
}

int
panel_session_die (GnomeClient *client,
		   gpointer client_data)
{
	AppletInfo *info;
	int i;

	gtk_timeout_remove(config_sync_timeout);
  
	/*don't catch these any more*/
	signal(SIGCHLD, SIG_DFL);
	
	for(i=0,info=(AppletInfo *)applets->data;
	    i<applet_count;
	    i++,info++) {
		if(info->type == APPLET_EXTERN) {
			gtk_container_remove(GTK_CONTAINER(info->widget),
					     GTK_BIN(info->widget)->child);
		} else if(info->type == APPLET_SWALLOW) {
			Swallow *swallow = info->data;
			XKillClient(GDK_DISPLAY(),
				    GDK_WINDOW_XWINDOW(GTK_SOCKET(swallow->socket)->plug_window));
		}
	}
			
	/*clean up corba stuff*/
	panel_corba_clean_up();
	
	panel_corba_gtk_main_quit();
	return TRUE;
}

/*save ourselves*/
static int
panel_really_logout(GtkWidget *w, int button, gpointer data)
{
	GtkWidget **box=data;

	if(button==0) {
		if (! GNOME_CLIENT_CONNECTED (client)) {
			panel_session_save (client, 1, GNOME_SAVE_BOTH, 1,
					    GNOME_INTERACT_NONE, 0, NULL);
			panel_session_die (client, NULL);
		} else {
			/* We request a completely interactive, full,
			   slow shutdown.  */
			gnome_client_request_save (client, GNOME_SAVE_BOTH, 1,
						   GNOME_INTERACT_ANY, 0, 1);
		}
	}
	if(box)
		*box = NULL;

	return TRUE;
}

static int
panel_really_logout_destroy(GtkWidget *w, gpointer data)
{
	GtkWidget **box=data;
	if(box)
		*box = NULL;
	return FALSE;
}


static void
ask_next_time(GtkWidget *w,gpointer data)
{
	global_config.prompt_for_logout = GTK_TOGGLE_BUTTON(w)->active!=FALSE;
	
	globals_to_sync = TRUE;
}

/* the logout function */
void
panel_quit(void)
{
	static GtkWidget *box = NULL;
	GtkWidget *but = NULL;

	if(!global_config.prompt_for_logout) {
		panel_really_logout(NULL,0,NULL);
		return;
	}

	if(box) {
		gdk_window_raise(box->window);
		return;
	}

	box = gnome_message_box_new (_("Really log out?"),
				     GNOME_MESSAGE_BOX_QUESTION,
				     GNOME_STOCK_BUTTON_YES,
				     GNOME_STOCK_BUTTON_NO,
				     NULL);
	/*gtk_window_position(GTK_WINDOW(box), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(box), FALSE, FALSE, TRUE);

	gtk_signal_connect (GTK_OBJECT (box), "clicked",
		            GTK_SIGNAL_FUNC (panel_really_logout), &box);
	gtk_signal_connect (GTK_OBJECT (box), "destroy",
		            GTK_SIGNAL_FUNC (panel_really_logout_destroy),
			    &box);

	but = gtk_check_button_new_with_label(_("Ask next time"));
	gtk_widget_show(but);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(box)->vbox),but,
		           FALSE, TRUE, GNOME_PAD_SMALL);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(but),TRUE);
	gtk_signal_connect(GTK_OBJECT(but),"toggled",
			   GTK_SIGNAL_FUNC(ask_next_time),NULL);

	gtk_widget_show (box);
}

static void
load_default_applets(void)
{
	load_menu_applet(NULL,0, panels->data, 0);
	load_extern_applet("gen_util_applet","--clock",NULL,
			   panels->data,INT_MAX/2/*right flush*/);
	/*we laoded default applets, so we didn't find the config or
	  something else was wrong, so do complete save when next syncing*/
	need_complete_save = TRUE;
}

void
init_user_applets(void)
{
	char  buf[256];
	int   count,num;	

	g_snprintf(buf,256,"%spanel/Config/applet_count=0",old_panel_cfg_path);
	count=gnome_config_get_int(buf);
	for(num=1;num<=count;num++) {
		char *applet_name;
		int   pos=0,panel_num;
		PanelWidget *panel;

		g_snprintf(buf,256, "%sApplet_Config/Applet_%d/",
			   old_panel_cfg_path, num);
		gnome_config_push_prefix(buf);
		applet_name = gnome_config_get_string("id=Unknown");
		
		if(strcmp(applet_name,EMPTY_ID)==0) {
			g_free(applet_name);
			gnome_config_pop_prefix();
			continue;
		} else if(strcmp(applet_name,"Unknown")==0) {
			g_warning("Unknown applet type!");
			g_free(applet_name);
			gnome_config_pop_prefix();
			continue;
		}

		g_snprintf(buf,256,"position=%d", 0);
		pos = gnome_config_get_int(buf);
		panel_num = gnome_config_get_int("panel=0");
		{
			GList *list = g_list_nth(panels,panel_num);
			if(!list) {
				g_warning("Can't find panel, "
					  "putting applet on the first one");
				panel = panels->data;
			} else
				panel = list->data;
		}
		
		/*if we are to right stick this, make the number large, 
		 INT_MAX/2 should allways be large enough */
		pos += gnome_config_get_bool("right_stick=false")?INT_MAX/2:0;
		
		if(strcmp(applet_name,EXTERN_ID) == 0) {
			char *path = gnome_config_get_string("execpath=");
			char *params = gnome_config_get_string("parameters=");
			/*this is the config path to be passed to the applet
			  when it loads*/
			g_snprintf(buf,256,"%sApplet_%d_Extern/",
				   old_panel_cfg_path,num);
			load_extern_applet(path,params,buf,panel,pos);
			g_free(path);
			g_free(params);
		} else if(strcmp(applet_name,LAUNCHER_ID) == 0) { 
			char *params = gnome_config_get_string("parameters=");
			load_launcher_applet(params,panel,pos);
			g_free(params);
		} else if(strcmp(applet_name,LOGOUT_ID) == 0) { 
			load_logout_applet(panel,pos);
		} else if(strcmp(applet_name,SWALLOW_ID) == 0) {
			char *path = gnome_config_get_string("execpath=");
			char *params = gnome_config_get_string("parameters=");
			int width = gnome_config_get_int("width=0");
			int height = gnome_config_get_int("height=0");
			load_swallow_applet(path,params,width,height,panel,pos);
			g_free(path);
			g_free(params);
		} else if(strcmp(applet_name,MENU_ID) == 0) {
			char *params = gnome_config_get_string("parameters=");
			int main_menu_type =
				gnome_config_get_int("main_menu_type=0");
			load_menu_applet(params,main_menu_type,panel,pos);
			g_free(params);
		} else if(strcmp(applet_name,DRAWER_ID) == 0) {
			char *params = gnome_config_get_string("parameters=");
			char *pixmap = gnome_config_get_string("pixmap=");
			char *tooltip = gnome_config_get_string("tooltip=");
			load_drawer_applet(params,pixmap,tooltip,panel,pos);
			g_free(params);
			g_free(pixmap);
			g_free(tooltip);
		} else
			g_warning("Unknown applet type!");
		gnome_config_pop_prefix();
	}
}

void
init_user_panels(void)
{
	char  buf[256];
	int   count,num;	
	GtkWidget *panel;

	g_snprintf(buf,256,"%spanel/Config/panel_count=0",old_panel_cfg_path);
	count=gnome_config_get_int(buf);

	/*load a default snapped panel on the bottom of the screen,
	  it is required to have at least one panel for this all
	  to work, so this is the way we find out if there was no
	  config from last time*/
	if(count<=0)  {
		panel = snapped_widget_new(SNAPPED_BOTTOM,
					   SNAPPED_EXPLICIT_HIDE,
					   SNAPPED_SHOWN,
					   TRUE,
					   PANEL_BACK_NONE,
					   NULL,
					   TRUE,
					   NULL);
		panel_setup(panel);
		gtk_widget_show(panel);

		/*load up default applets on the default panel*/
		load_default_applets();

		return;
	}

	for(num=1;num<=count;num++) {
		PanelType type;
		PanelBackType back_type;
		char *back_pixmap, *color;
		GdkColor back_color = {0,0,0,1};
		int fit_pixmap_bg;

		g_snprintf(buf,256,"%spanel/Panel_%d/", old_panel_cfg_path, num);
		gnome_config_push_prefix (buf);
		
		back_pixmap = gnome_config_get_string ("backpixmap=");
		if (back_pixmap && *back_pixmap == '\0') {
			g_free(back_pixmap);
			back_pixmap = NULL;
		}

		color = gnome_config_get_string("backcolor=#ffffff");
		if(color && *color)
			gdk_color_parse(color, &back_color);

		g_snprintf(buf,256,"back_type=%d",PANEL_BACK_NONE);
		back_type=gnome_config_get_int(buf);
		fit_pixmap_bg = gnome_config_get_bool ("fit_pixmap_bg=TRUE");

		/*now for type specific config*/

		g_snprintf(buf,256,"type=%d", SNAPPED_PANEL);
		type = gnome_config_get_int(buf);

		switch(type) {
		case SNAPPED_PANEL:
			{
				SnappedPos pos;
				SnappedMode mode;
				SnappedState state;
				int hidebuttons_enabled;

				g_snprintf(buf,256,"pos=%d", SNAPPED_BOTTOM);
				pos=gnome_config_get_int(buf);

				g_snprintf(buf,256,"mode=%d", SNAPPED_EXPLICIT_HIDE);
				mode=gnome_config_get_int(buf);

				g_snprintf(buf,256,"state=%d", SNAPPED_SHOWN);
				state=gnome_config_get_int(buf);

				hidebuttons_enabled =
					gnome_config_get_bool("hidebuttons_enabled=TRUE");

				panel = snapped_widget_new(pos,
							   mode,
							   state,
							   hidebuttons_enabled,
							   back_type,
							   back_pixmap,
							   fit_pixmap_bg,
							   &back_color);
				break;
			}
		case DRAWER_PANEL:
			{
				DrawerState state;
				PanelOrientType orient;

				g_snprintf(buf,256,"state=%d", DRAWER_SHOWN);
				state=gnome_config_get_int(buf);

				g_snprintf(buf,256,"orient=%d", ORIENT_UP);
				orient=gnome_config_get_int(buf);

				panel = drawer_widget_new(orient,
							  state,
							  back_type,
							  back_pixmap,
							  fit_pixmap_bg,
							  &back_color);
				break;
			}
		case CORNER_PANEL:
			{
				CornerPos pos;
				PanelOrientation orient;
				CornerState state;
				
				g_snprintf(buf,256,"pos=%d", CORNER_NE);
				pos=gnome_config_get_int(buf);

				g_snprintf(buf,256,"orient=%d",
					   PANEL_HORIZONTAL);
				orient=gnome_config_get_int(buf);

				g_snprintf(buf,256,"state=%d", CORNER_SHOWN);
				state=gnome_config_get_int(buf);
				
				panel = corner_widget_new(pos,
							  orient,
							  state,
							  back_type,
							  back_pixmap,
							  fit_pixmap_bg,
							  &back_color);
				break;
			}
		default: break;
		}

		gnome_config_pop_prefix ();
		
		g_free(color);
		g_free(back_pixmap);

		panel_setup(panel);

		gtk_widget_show(panel);
	}
}

void
load_up_globals(void)
{
	char buf[256];
	char *tile_def[]={"normal","purple","green"};
	int i;

	/*set up global options*/
	
	g_snprintf(buf,256,"%spanel/Config/",old_panel_cfg_path);
	gnome_config_push_prefix(buf);

	global_config.tooltips_enabled =
		gnome_config_get_bool("tooltips_enabled=TRUE");

	global_config.show_small_icons =
		gnome_config_get_bool("show_small_icons=TRUE");
		
	global_config.prompt_for_logout =
		gnome_config_get_bool("prompt_for_logout=TRUE");

	global_config.disable_animations =
		gnome_config_get_bool("disable_animations=FALSE");
		
	g_snprintf(buf,256,"auto_hide_step_size=%d",
		   DEFAULT_AUTO_HIDE_STEP_SIZE);
	global_config.auto_hide_step_size=gnome_config_get_int(buf);
		
	g_snprintf(buf,256,"explicit_hide_step_size=%d",
		   DEFAULT_EXPLICIT_HIDE_STEP_SIZE);
	global_config.explicit_hide_step_size=gnome_config_get_int(buf);
		
	g_snprintf(buf,256,"drawer_step_size=%d",
		   DEFAULT_DRAWER_STEP_SIZE);
	global_config.drawer_step_size=gnome_config_get_int(buf);
		
	g_snprintf(buf,256,"minimize_delay=%d", DEFAULT_MINIMIZE_DELAY);
	global_config.minimize_delay=gnome_config_get_int(buf);
		
	g_snprintf(buf,256,"minimized_size=%d", DEFAULT_MINIMIZED_SIZE);
	global_config.minimized_size=gnome_config_get_int(buf);
		
	g_snprintf(buf,256,"movement_type=%d", PANEL_SWITCH_MOVE);
	global_config.movement_type=gnome_config_get_int(buf);

	global_config.applet_padding=gnome_config_get_int("applet_padding=3");

	global_config.tiles_enabled =
		gnome_config_get_bool("tiles_enabled=TRUE");
	
	for(i=0;i<LAST_TILE;i++) {
		g_free(global_config.tile_up[i]);
		g_snprintf(buf,256,"tile_up_%d=tile-%s-up.png",i,tile_def[i]);
		global_config.tile_up[i] = gnome_config_get_string(buf);

		g_free(global_config.tile_down[i]);
		g_snprintf(buf,256,"tile_down_%d=tile-%s-down.png",i,tile_def[i]);
		global_config.tile_down[i] = gnome_config_get_string(buf);

		g_snprintf(buf,256,"tile_border_%d=2",i);
		global_config.tile_border[i] = gnome_config_get_int(buf);
		g_snprintf(buf,256,"tile_depth_%d=2",i);
		global_config.tile_depth[i] = gnome_config_get_int(buf);
	}
		
	gnome_config_pop_prefix();

	apply_global_config();
}
