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
GSList *applets_to_sync = NULL;
int panels_to_sync = FALSE;
int globals_to_sync = FALSE;
int need_complete_save = FALSE;

extern GSList *panels;
extern GSList *applets;
extern GSList *applets_last;
extern int applet_count;

extern GtkTooltips *panel_tooltips;

extern GnomeClient *client;

GlobalConfig global_config;

char *panel_cfg_path=NULL;
char *old_panel_cfg_path=NULL;

/*list of all panel widgets created*/
extern GSList *panel_list;

/*send the tooltips state to all external applets*/
static void
send_tooltips_state(int enabled)
{
	GSList *li;

	for(li = applets; li!=NULL; li = g_slist_next(li)) {
		AppletInfo *info = li->data;
		if(info->type == APPLET_EXTERN) {
			Extern *ext = info->data;
			g_assert(ext);
			/*if it's not set yet, don't send it, it will be sent
			  when the ior is discovered anyhow, so this would be
			  redundant anyway*/
			if(ext->applet) {
				CORBA_Environment ev;
				CORBA_exception_init(&ev);
				GNOME_Applet_set_tooltips_state(ext->applet, enabled, &ev);
				if(ev._major)
					panel_clean_applet(ext->info);
				CORBA_exception_free(&ev);
			}
		}
	}
}

void
apply_global_config(void)
{
	int i;
	static int dot_buttons_old = 0; /*doesn't matter first time this is
					  done there are no menu applets*/
	static int small_icons_old = 0; /*same here*/
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
	/*if we changed dot_buttons/small_icons mark all menus as dirty
	  for rereading, hopefullly the user doesn't do this too often
	  so that he doesn't have to reread his menus all the time:)*/
	if(dot_buttons_old != global_config.show_dot_buttons ||
	   small_icons_old != global_config.show_small_icons) {
		GSList *li;
		for(li=applets;li!=NULL;li=g_slist_next(li)) {
			AppletInfo *info = li->data;
			if(info->menu) {
				gtk_widget_destroy(info->menu);
				info->menu = NULL;
				info->menu_age = 0;
			}
			if(info->type == APPLET_MENU) {
				Menu *menu = info->data;
				if(menu->menu) {
					gtk_widget_destroy(menu->menu);
					menu->menu = NULL;
					menu->age = 0;
				}
			}
		}
		for(li = panel_list; li != NULL; li = g_slist_next(li)) {
			PanelData *pd = li->data;
			if(pd->menu) {
				gtk_widget_destroy(pd->menu);
				pd->menu = NULL;
				pd->menu_age = 0;
			}
		}
	}
	dot_buttons_old = global_config.show_dot_buttons;
	small_icons_old = global_config.show_small_icons;
	send_tooltips_state(global_config.tooltips_enabled);

	for(i=0;i<LAST_TILE;i++) {
		button_widget_tile_enable(i, global_config.tiles_enabled[i]);
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

	prefix = g_strconcat ("=", dentry->location, "=/Desktop Entry", NULL);

	gnome_config_clean_section (prefix);

	prefix = g_strconcat (prefix, "/", NULL);
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

static int
send_applet_session_save (AppletInfo *info,
			  CORBA_Object obj,
			  const char *cfgpath,
			  const char *globcfgpath)
{
  CORBA_short retval;
  CORBA_Environment ev;

  CORBA_exception_init(&ev);
  retval = GNOME_Applet_session_save(obj,
				     (CORBA_char *)cfgpath,
				     (CORBA_char *)globcfgpath, &ev);
  if(ev._major)
    panel_clean_applet(info);
  CORBA_exception_free(&ev);

  return retval;
}



static void
save_applet_configuration(AppletInfo *info)
{
	GString       *buf;
	int            panel_num;
	PanelWidget   *panel;
	AppletData    *ad;
	
	g_return_if_fail(info!=NULL);

	buf = g_string_new(NULL);
	g_string_sprintf(buf, "%sApplet_Config/Applet_%d/", panel_cfg_path, info->applet_id+1);
	gnome_config_push_prefix(buf->str);

	/*obviously no need for saving*/
	if(info->type==APPLET_EXTERN_PENDING ||
	   info->type==APPLET_EXTERN_RESERVED ||
	   info->type==APPLET_EMPTY) {
		gnome_config_set_string("id", EMPTY_ID);
		gnome_config_pop_prefix();
		g_string_free(buf,TRUE);
		return;
	}

	panel = PANEL_WIDGET(info->widget->parent);
	ad = gtk_object_get_data(GTK_OBJECT(info->widget),PANEL_APPLET_DATA);

	if((panel_num = g_slist_index(panels,panel)) == -1) {
		gnome_config_set_string("id", EMPTY_ID);
		gnome_config_pop_prefix();
		g_string_free(buf,TRUE);
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

			globalcfg = g_concat_dir_and_file(panel_cfg_path,
							  "Applet_All_Extern/");

			/*this is the file path we pass to the applet for it's
			  own config, this is a separate file, so that we */
			g_string_sprintf(buf, "%sApplet_%d_Extern/",
					 panel_cfg_path, info->applet_id+1);
			/*have the applet do it's own session saving*/
			if(send_applet_session_save(info,ext->applet,
						    buf->str, globalcfg)) {

				gnome_config_set_string("id", EXTERN_ID);
				gnome_config_set_string("goad_id",
							ext->goad_id);
			} else {
				g_free(globalcfg);
				gnome_config_set_string("id", EMPTY_ID);
				gnome_config_pop_prefix();
				g_string_free(buf,TRUE);
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

			i = g_slist_index(panels,
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
			gnome_config_set_int("main_menu_flags",
					     menu->main_menu_flags);
			break;
		}
	case APPLET_LAUNCHER:
		{
			Launcher *launcher = info->data;
			/*we set the .desktop to be in the panel config
			  dir*/
			g_string_sprintf(buf, "%s/%sApplet_%d.desktop",
					 gnome_user_dir,panel_cfg_path,
					 info->applet_id+1);
			g_free(launcher->dentry->location);
			launcher->dentry->location = g_strdup(buf->str);
			gnome_desktop_entry_save_no_sync(launcher->dentry);

			gnome_config_set_string("id", LAUNCHER_ID);
			gnome_config_set_string("parameters", buf->str);
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
	g_string_free(buf,TRUE);
	gnome_config_pop_prefix();
}

static void
save_panel_configuration(gpointer data, gpointer user_data)
{
	GString       *buf;
	int           *num = user_data;
	PanelData     *pd = data;
	PanelWidget   *panel = get_def_panel_widget(pd->panel);
	
	buf = g_string_new(NULL);

	g_string_sprintf(buf, "%spanel/Panel_%d/", panel_cfg_path, (*num)++);
	gnome_config_clean_section(buf->str);

	gnome_config_push_prefix (buf->str);

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
		gnome_config_set_bool("hidebutton_pixmaps_enabled",
				      snapped->hidebutton_pixmaps_enabled);
		break;
		}
	case CORNER_PANEL:
		{
		CornerWidget *corner = CORNER_WIDGET(pd->panel);
		gnome_config_set_int("pos", corner->pos);
		gnome_config_set_int("orient",panel->orient);
		gnome_config_set_int("state", corner->state);
		gnome_config_set_bool("hidebuttons_enabled",
				      corner->hidebuttons_enabled);
		gnome_config_set_bool("hidebutton_pixmaps_enabled",
				      corner->hidebutton_pixmaps_enabled);
		break;
		}
	case DRAWER_PANEL:
		{
		DrawerWidget *drawer = DRAWER_WIDGET(pd->panel);
		gnome_config_set_int("orient",drawer->orient);
		gnome_config_set_int("state", drawer->state);
		gnome_config_set_bool("hidebutton_enabled",
				      drawer->hidebutton_enabled);
		gnome_config_set_bool("hidebutton_pixmap_enabled",
				      drawer->hidebutton_pixmap_enabled);
		break;
		}
	default:
		g_assert_not_reached();
	}
	gnome_config_set_bool("fit_pixmap_bg", panel->fit_pixmap_bg);

	gnome_config_set_string("backpixmap",
				panel->back_pixmap ? panel->back_pixmap : "");

	g_string_sprintf(buf, "#%02x%02x%02x",
			 (guint)panel->back_color.red/256,
			 (guint)panel->back_color.green/256,
			 (guint)panel->back_color.blue/256);
	gnome_config_set_string("backcolor", buf->str);

	gnome_config_set_int("back_type", panel->back_type);
	
	g_string_free(buf,TRUE);

	gnome_config_pop_prefix ();
}

static void
do_session_save(GnomeClient *client,
		int complete_sync,
		GSList *sync_applets,
		int sync_panels,
		int sync_globals)
{
	int num;
	char *s;
	char *session_id;
	int i;

	session_id = gnome_client_get_id (client);
	if(session_id) {
		char *new_args[3];

		g_free(panel_cfg_path);
		panel_cfg_path = g_strconcat("/panel.d/Session-",session_id,
					     "/",NULL);

		new_args[0] = (char *) gtk_object_get_data(GTK_OBJECT(client),
							   "argv0");
		new_args[1] = "--discard-session";
		new_args[2] = session_id;
		gnome_client_set_discard_command (client, 3, new_args);
	}
	
	/*DEBUG*/printf("Saving to [%s]\n",panel_cfg_path);

	gnome_config_clean_file(panel_cfg_path);

	/*DEBUG*/printf("Saving session: 1"); fflush(stdout);
	if(complete_sync) {
		GSList *li;
		for(li=applets;li!=NULL;li=g_slist_next(li))
			save_applet_configuration(li->data);
	} else {
		GSList *li;
		for(li = sync_applets; li!=NULL; li=g_slist_next(li))
			save_applet_configuration(li->data);
	}
	/*DEBUG*/printf(" 2"); fflush(stdout);

	s = g_concat_dir_and_file(panel_cfg_path,"panel/Config/");
	gnome_config_push_prefix (s);
	g_free(s);

	if(complete_sync || sync_applets)
		gnome_config_set_int ("applet_count", applet_count);
	/*DEBUG*/printf(" 3"); fflush(stdout);
	if(complete_sync || sync_panels) {
		num = 1;
		g_slist_foreach(panel_list, save_panel_configuration,&num);
		gnome_config_set_int("panel_count",num-1);
	}
	/*DEBUG*/printf(" 4"); fflush(stdout);

	if(complete_sync || sync_globals) {
		GString *buf;
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
		gnome_config_set_bool("show_dot_buttons",
				      global_config.show_dot_buttons);
		gnome_config_set_bool("off_panel_popups",
				      global_config.off_panel_popups);
		gnome_config_set_bool("prompt_for_logout",
				      global_config.prompt_for_logout);
		gnome_config_set_bool("disable_animations",
				      global_config.disable_animations);
		gnome_config_set_int("applet_padding",
				     global_config.applet_padding);
		buf = g_string_new(NULL);
		for(i=0;i<LAST_TILE;i++) {
			g_string_sprintf(buf,"tiles_enabled_%d",i);
			gnome_config_set_bool(buf->str,
					      global_config.tiles_enabled[i]);
			g_string_sprintf(buf,"tile_up_%d",i);
			gnome_config_set_string(buf->str,
						global_config.tile_up[i]);
			g_string_sprintf(buf,"tile_down_%d",i);
			gnome_config_set_string(buf->str,
						global_config.tile_down[i]);
			g_string_sprintf(buf,"tile_border_%d",i);
			gnome_config_set_int(buf->str,
					     global_config.tile_border[i]);
			g_string_sprintf(buf,"tile_depth_%d",i);
			gnome_config_set_int(buf->str,
					     global_config.tile_depth[i]);
		}
		g_string_free(buf,TRUE);
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
		GSList *_applets_to_sync = applets_to_sync;

		applets_to_sync = NULL;
		do_session_save(client,need_complete_save,
				_applets_to_sync,panels_to_sync,globals_to_sync);
		need_complete_save = FALSE;

		g_slist_free(_applets_to_sync);
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
	GSList *li;

	gtk_timeout_remove(config_sync_timeout);
  
	/*don't catch these any more*/
	signal(SIGCHLD, SIG_DFL);
	
	for(li=applets; li!=NULL; li=g_slist_next(li)) {
		AppletInfo *info = li->data;
		if(info->type == APPLET_EXTERN) {
			gtk_widget_destroy(info->widget);
		} else if(info->type == APPLET_SWALLOW) {
			Swallow *swallow = info->data;
			if(GTK_SOCKET(swallow->socket)->plug_window)
				XKillClient(GDK_DISPLAY(),
					    GDK_WINDOW_XWINDOW(GTK_SOCKET(swallow->socket)->plug_window));
		}
	}
			
	/*clean up corba stuff*/
	panel_corba_clean_up();
	
	gtk_main_quit();
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

static void
panel_really_logout_destroy(GtkWidget *w, gpointer data)
{
	GtkWidget **box=data;
	if(box)
		*box = NULL;
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
	gnome_dialog_set_default(GNOME_DIALOG(box), 0);
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
	char *def_launchers[] =
	      { "apps/gnome-help.desktop",
		"apps/System/gnomecc.desktop",
		"apps/Applications/Netscape.desktop",
		NULL };
	int i;
	int flags = MAIN_MENU_SYSTEM|MAIN_MENU_USER;

	/*guess redhat menus*/
	if(g_file_exists("/etc/X11/wmconfig"))
		flags |= MAIN_MENU_REDHAT|MAIN_MENU_REDHAT_SUB;
	load_menu_applet(NULL,flags, panels->data, 0);

	for(i=0;def_launchers[i]!=NULL;i++) {
		char *p = gnome_datadir_file (def_launchers[i]);
		int center = gdk_screen_width()/2;
		if(p) {
			load_launcher_applet(p,panels->data,center+i);
			g_free(p);
		}
	}

	load_extern_applet("gen_util_clock",NULL,
			   panels->data,INT_MAX/2/*right flush*/);
	/*we laoded default applets, so we didn't find the config or
	  something else was wrong, so do complete save when next syncing*/
	need_complete_save = TRUE;
}

void
init_user_applets(void)
{
	GString *buf;
	int   count,num;	

	buf = g_string_new(NULL);
	g_string_sprintf(buf,"%spanel/Config/applet_count=0",
			 old_panel_cfg_path);
	count=gnome_config_get_int(buf->str);
	for(num=1;num<=count;num++) {
		char *applet_name;
		int   pos=0,panel_num;
		PanelWidget *panel;

		g_string_sprintf(buf,"%sApplet_Config/Applet_%d/",
				 old_panel_cfg_path, num);
		gnome_config_push_prefix(buf->str);
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

		g_string_sprintf(buf,"position=%d", 0);
		pos = gnome_config_get_int(buf->str);
		panel_num = gnome_config_get_int("panel=0");
		{
			GSList *list = g_slist_nth(panels,panel_num);
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
			char *goad_id = gnome_config_get_string("goad_id");
			if(goad_id && *goad_id) {
				/*this is the config path to be passed to the
				  applet when it loads*/
				g_string_sprintf(buf,"%sApplet_%d_Extern/",
						 old_panel_cfg_path,num);
				load_extern_applet(goad_id,buf->str,panel,pos);
			}
			g_free(goad_id);
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
			int type =
				gnome_config_get_int("main_menu_type=-1");
			int flags =
				gnome_config_get_int("main_menu_flags=5");
			if(type>=0) {
				flags = 0;
				if(type == X_MAIN_MENU_BOTH) {
					flags |= MAIN_MENU_SYSTEM|MAIN_MENU_USER;
				} else if(type == X_MAIN_MENU_SYSTEM) {
					flags |= MAIN_MENU_SYSTEM|MAIN_MENU_USER|
						MAIN_MENU_USER_SUB;
				} else {
					flags |= MAIN_MENU_SYSTEM|MAIN_MENU_SYSTEM_SUB|
						MAIN_MENU_USER;
				}
				/*guess redhat menus*/
				if(g_file_exists("/etc/X11/wmconfig"))
					flags |= MAIN_MENU_REDHAT|MAIN_MENU_REDHAT_SUB;
			}
			load_menu_applet(params,flags,panel,pos);
			g_free(params);
		} else if(strcmp(applet_name,DRAWER_ID) == 0) {
			int mypanel = gnome_config_get_int("parameters=-1");
			char *pixmap = gnome_config_get_string("pixmap=");
			char *tooltip = gnome_config_get_string("tooltip=");
			load_drawer_applet(mypanel,pixmap,tooltip,panel,pos);
			g_free(pixmap);
			g_free(tooltip);
		} else
			g_warning("Unknown applet type!");
		gnome_config_pop_prefix();
		g_free(applet_name);
	}
	g_string_free(buf,TRUE);
}

void
init_user_panels(void)
{
	GString *buf;
	int   count,num;	
	GtkWidget *panel;

	buf = g_string_new(NULL);
	g_string_sprintf(buf,"%spanel/Config/panel_count=0",
			 old_panel_cfg_path);
	count=gnome_config_get_int(buf->str);

	/*load a default snapped panel on the bottom of the screen,
	  it is required to have at least one panel for this all
	  to work, so this is the way we find out if there was no
	  config from last time*/
	if(count<=0)  {
		panel = snapped_widget_new(SNAPPED_BOTTOM,
					   SNAPPED_EXPLICIT_HIDE,
					   SNAPPED_SHOWN,
					   TRUE,
					   TRUE,
					   PANEL_BACK_NONE,
					   NULL,
					   TRUE,
					   NULL);
		panel_setup(panel);
		gtk_widget_show(panel);

		/*load up default applets on the default panel*/
		load_default_applets();

		g_string_free(buf,TRUE);
		return;
	}

	for(num=1;num<=count;num++) {
		PanelType type;
		PanelBackType back_type;
		char *back_pixmap, *color;
		GdkColor back_color = {0,0,0,1};
		int fit_pixmap_bg;

		g_string_sprintf(buf,"%spanel/Panel_%d/",
				 old_panel_cfg_path, num);
		gnome_config_push_prefix (buf->str);
		
		back_pixmap = gnome_config_get_string ("backpixmap=");
		if (back_pixmap && *back_pixmap == '\0') {
			g_free(back_pixmap);
			back_pixmap = NULL;
		}

		color = gnome_config_get_string("backcolor=#ffffff");
		if(color && *color)
			gdk_color_parse(color, &back_color);

		g_string_sprintf(buf,"back_type=%d",PANEL_BACK_NONE);
		back_type=gnome_config_get_int(buf->str);
		fit_pixmap_bg = gnome_config_get_bool ("fit_pixmap_bg=TRUE");

		/*now for type specific config*/

		g_string_sprintf(buf,"type=%d", SNAPPED_PANEL);
		type = gnome_config_get_int(buf->str);

		switch(type) {
		case SNAPPED_PANEL:
			{
				SnappedPos pos;
				SnappedMode mode;
				SnappedState state;
				int hidebuttons_enabled;
				int hidebutton_pixmaps_enabled;

				g_string_sprintf(buf,"pos=%d", SNAPPED_BOTTOM);
				pos=gnome_config_get_int(buf->str);

				g_string_sprintf(buf,"mode=%d",
						 SNAPPED_EXPLICIT_HIDE);
				mode=gnome_config_get_int(buf->str);

				g_string_sprintf(buf,"state=%d", SNAPPED_SHOWN);
				state=gnome_config_get_int(buf->str);

				hidebuttons_enabled =
					gnome_config_get_bool("hidebuttons_enabled=TRUE");
				hidebutton_pixmaps_enabled =
					gnome_config_get_bool("hidebutton_pixmaps_enabled=TRUE");

				panel = snapped_widget_new(pos,
							   mode,
							   state,
							   hidebuttons_enabled,
							   hidebutton_pixmaps_enabled,
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
				int hidebutton_enabled;
				int hidebutton_pixmap_enabled;

				g_string_sprintf(buf,"state=%d", DRAWER_SHOWN);
				state=gnome_config_get_int(buf->str);

				g_string_sprintf(buf,"orient=%d", ORIENT_UP);
				orient=gnome_config_get_int(buf->str);

				hidebutton_enabled =
					gnome_config_get_bool("hidebutton_enabled=TRUE");
				hidebutton_pixmap_enabled =
					gnome_config_get_bool("hidebutton_pixmap_enabled=TRUE");

				panel = drawer_widget_new(orient,
							  state,
							  back_type,
							  back_pixmap,
							  fit_pixmap_bg,
							  &back_color,
							  hidebutton_pixmap_enabled,
							  hidebutton_enabled);
				break;
			}
		case CORNER_PANEL:
			{
				CornerPos pos;
				PanelOrientation orient;
				CornerState state;
				int hidebuttons_enabled;
				int hidebutton_pixmaps_enabled;
				
				g_string_sprintf(buf,"pos=%d", CORNER_NE);
				pos=gnome_config_get_int(buf->str);

				g_string_sprintf(buf,"orient=%d",
						 PANEL_HORIZONTAL);
				orient=gnome_config_get_int(buf->str);

				g_string_sprintf(buf,"state=%d", CORNER_SHOWN);
				state=gnome_config_get_int(buf->str);

				hidebuttons_enabled =
					gnome_config_get_bool("hidebuttons_enabled=TRUE");
				hidebutton_pixmaps_enabled =
					gnome_config_get_bool("hidebutton_pixmaps_enabled=TRUE");
				
				panel = corner_widget_new(pos,
							  orient,
							  state,
							  hidebuttons_enabled,
							  hidebutton_pixmaps_enabled,
							  back_type,
							  back_pixmap,
							  fit_pixmap_bg,
							  &back_color);
				break;
			}
		default: panel=NULL; break; /*fix warning*/
		}

		gnome_config_pop_prefix ();
		
		g_free(color);
		g_free(back_pixmap);

		panel_setup(panel);

		gtk_widget_show(panel);
	}
	g_string_free(buf,TRUE);
}

void
load_up_globals(void)
{
	GString *buf;
	char *tile_def[]={"normal","purple","green","blue"};
	int i;
	
	buf = g_string_new(NULL);

	/*set up global options*/
	
	g_string_sprintf(buf,"%spanel/Config/",old_panel_cfg_path);
	gnome_config_push_prefix(buf->str);

	global_config.tooltips_enabled =
		gnome_config_get_bool("tooltips_enabled=TRUE");

	global_config.show_small_icons =
		gnome_config_get_bool("show_small_icons=TRUE");

	global_config.show_dot_buttons =
		gnome_config_get_bool("show_dot_buttons=FALSE");

	global_config.off_panel_popups =
		gnome_config_get_bool("off_panel_popups=TRUE");
		
	global_config.prompt_for_logout =
		gnome_config_get_bool("prompt_for_logout=TRUE");

	global_config.disable_animations =
		gnome_config_get_bool("disable_animations=FALSE");
		
	g_string_sprintf(buf,"auto_hide_step_size=%d",
			 DEFAULT_AUTO_HIDE_STEP_SIZE);
	global_config.auto_hide_step_size=gnome_config_get_int(buf->str);
		
	g_string_sprintf(buf,"explicit_hide_step_size=%d",
			 DEFAULT_EXPLICIT_HIDE_STEP_SIZE);
	global_config.explicit_hide_step_size=gnome_config_get_int(buf->str);
		
	g_string_sprintf(buf,"drawer_step_size=%d",
			 DEFAULT_DRAWER_STEP_SIZE);
	global_config.drawer_step_size=gnome_config_get_int(buf->str);
		
	g_string_sprintf(buf,"minimize_delay=%d", DEFAULT_MINIMIZE_DELAY);
	global_config.minimize_delay=gnome_config_get_int(buf->str);
		
	g_string_sprintf(buf,"minimized_size=%d", DEFAULT_MINIMIZED_SIZE);
	global_config.minimized_size=gnome_config_get_int(buf->str);
		
	g_string_sprintf(buf,"movement_type=%d", PANEL_SWITCH_MOVE);
	global_config.movement_type=gnome_config_get_int(buf->str);

	global_config.applet_padding=gnome_config_get_int("applet_padding=3");

	for(i=0;i<LAST_TILE;i++) {
		g_string_sprintf(buf,"tiles_enabled_%d=TRUE",i);
		global_config.tiles_enabled[i] =
			gnome_config_get_bool(buf->str);

		g_free(global_config.tile_up[i]);
		g_string_sprintf(buf,"tile_up_%d=tiles/tile-%s-up.png",
			   i, tile_def[i]);
		global_config.tile_up[i] = gnome_config_get_string(buf->str);

		g_free(global_config.tile_down[i]);
		g_string_sprintf(buf,"tile_down_%d=tiles/tile-%s-down.png",
			   i,tile_def[i]);
		global_config.tile_down[i] = gnome_config_get_string(buf->str);

		g_string_sprintf(buf,"tile_border_%d=2",i);
		global_config.tile_border[i] = gnome_config_get_int(buf->str);
		g_string_sprintf(buf,"tile_depth_%d=2",i);
		global_config.tile_depth[i] = gnome_config_get_int(buf->str);
	}
	g_string_free(buf,TRUE);
		
	gnome_config_pop_prefix();

	apply_global_config();
}
