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
#include <limits.h>
#include <gnome.h>
#include <gdk/gdkx.h>

#include "panel-include.h"

/*#define PANEL_DEBUG 1*/

int config_sync_timeout = 0;
int applets_to_sync = FALSE;
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

#if 0
char *panel_cfg_path=NULL;
char *old_panel_cfg_path=NULL;
#endif

/*list of all panel widgets created*/
extern GSList *panel_list;

int ss_cur_applet = 0;
int ss_done_save = FALSE;
gushort ss_cookie = 0;
GtkWidget *ss_timeout_dlg = NULL;
static int ss_interactive = FALSE;
static int ss_timeout = 500;

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
	static int keep_bottom_old = -1;
	GSList *li;
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

	if(keep_bottom_old == -1 ||
	   keep_bottom_old != global_config.keep_bottom) {
		for(li = panel_list; li != NULL; li = g_slist_next(li)) {
			PanelData *pd = li->data;
			if(!GTK_WIDGET_REALIZED(pd->panel))
				continue;
			if (IS_BASEP_WIDGET (pd->panel))
				basep_widget_update_winhints (BASEP_WIDGET (pd->panel));
		}
	}
	keep_bottom_old = global_config.keep_bottom;

	for(i=0;i<LAST_TILE;i++) {
		button_widget_set_flags(i, global_config.tiles_enabled[i],
					1,0);
		button_widget_load_tile(i, global_config.tile_up[i],
					global_config.tile_down[i],
					global_config.tile_border[i],
					global_config.tile_depth[i]);
	}

	for(li = panel_list; li != NULL; li = g_slist_next(li)) {
		PanelData *pd = li->data;
		if (IS_BASEP_WIDGET (pd->panel))
			basep_update_frame (BASEP_WIDGET (pd->panel));
	}
}

static void
ss_cancel_wait(GtkWidget *dlg, gpointer data)
{
	int cookie = GPOINTER_TO_INT(data);
	ss_timeout_dlg = NULL;
	if(cookie == ss_cookie) {
		/*increment cookie so that the done_session_save will fail*/
		ss_cookie++;
		g_warning(_("Timed out on sending session save to an applet"));
		save_next_applet();
	}
}

static int
session_save_timeout(gpointer data)
{
	int cookie = GPOINTER_TO_INT(data);
	if(cookie == ss_cookie) {
#ifdef PANEL_DEBUG	
		printf("SAVE TIMEOUT (%u)\n",ss_cookie);
#endif
		if(ss_interactive) {
			ss_timeout_dlg =
				gnome_message_box_new(_("An applet is not "
							"responding to a "
							"save request,\n"
							"should I stop "
							"waiting and cancel "
							"the applet?"),
						      GNOME_MESSAGE_BOX_WARNING,
						      GNOME_STOCK_BUTTON_CANCEL,
						      NULL);
			gtk_signal_connect(GTK_OBJECT(ss_timeout_dlg),"destroy",
					   GTK_SIGNAL_FUNC(ss_cancel_wait),
					   GINT_TO_POINTER(cookie));
			gnome_dialog_close_hides(GNOME_DIALOG(ss_timeout_dlg),
						 FALSE);
			gnome_dialog_set_close(GNOME_DIALOG(ss_timeout_dlg),
					       TRUE);
			gtk_widget_show(ss_timeout_dlg);
			return FALSE;
		}

		/*increment cookie so that the done_session_save will fail*/
		ss_cookie++;
		g_warning(_("Timed out on sending session save to an applet"));
		save_next_applet();
	}
	return FALSE;
}

static void
send_applet_session_save (AppletInfo *info,
			  CORBA_Object obj,
			  const char *cfgpath,
			  const char *globcfgpath)
{
	CORBA_Environment ev;
	
	/*new unique cookie*/
	ss_cookie++;
	
#ifdef PANEL_DEBUG	
	printf("SENDING_SESSION_SAVE (%u)\n",ss_cookie);
#endif

	gtk_timeout_add(ss_timeout,session_save_timeout,GINT_TO_POINTER((int)ss_cookie));

	CORBA_exception_init(&ev);
	GNOME_Applet_session_save(obj,
				  (CORBA_char *)cfgpath,
				  (CORBA_char *)globcfgpath,
				  ss_cookie, &ev);
	if(ev._major) {
		panel_clean_applet(info);
		CORBA_exception_free(&ev);
		save_next_applet();
		return;
	}

	CORBA_exception_free(&ev);
}



/*returns TRUE if the save was completed, FALSE if we need to wait
  for the applet to respond*/
static int
save_applet_configuration(AppletInfo *info)
{
	GString       *buf;
	int            panel_num;
	PanelWidget   *panel;
	AppletData    *ad;
	
	g_return_val_if_fail(info!=NULL,TRUE);

	buf = g_string_new(NULL);
	g_string_sprintf(buf, "%sApplet_Config/Applet_%d/", PANEL_CONFIG_PATH, info->applet_id+1);
	gnome_config_clean_section(buf->str);
	gnome_config_push_prefix(buf->str);

	/*obviously no need for saving*/
	if(info->type==APPLET_EXTERN_PENDING ||
	   info->type==APPLET_EXTERN_RESERVED ||
	   info->type==APPLET_EMPTY) {
		gnome_config_set_string("id", EMPTY_ID);
		gnome_config_pop_prefix();
		g_string_free(buf,TRUE);
		return TRUE;
	}

	panel = PANEL_WIDGET(info->widget->parent);
	ad = gtk_object_get_data(GTK_OBJECT(info->widget),PANEL_APPLET_DATA);

	if((panel_num = g_slist_index(panels,panel)) == -1) {
		gnome_config_set_string("id", EMPTY_ID);
		gnome_config_pop_prefix();
		g_string_free(buf,TRUE);
		return TRUE;
	}

	switch(info->type) {
	case APPLET_EXTERN:
		{
			char *globalcfg;
			Extern *ext = info->data;

			globalcfg = g_concat_dir_and_file(PANEL_CONFIG_PATH,
							  "Applet_All_Extern/");

			/*this is the file path we pass to the applet for it's
			  own config, this is a separate file, so that we */
			g_string_sprintf(buf, "%sApplet_%d_Extern/",
					 PANEL_CONFIG_PATH, info->applet_id+1);
			gnome_config_clean_file(buf->str);
			/*just in case the applet times out*/
			gnome_config_set_string("id", EMPTY_ID);
			gnome_config_pop_prefix();
			gnome_config_sync();
			/*have the applet do it's own session saving*/
			send_applet_session_save(info,ext->applet,
						 buf->str, globalcfg);
			g_free(globalcfg);
			return FALSE; /*here we'll wait for done_session_save*/
		}
	case APPLET_DRAWER: 
		{
			int i;
			Drawer *drawer = info->data;

			gnome_config_set_string("id", DRAWER_ID);

			i = g_slist_index(panels,
					  BASEP_WIDGET(drawer->drawer)->panel);
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
					 gnome_user_dir,PANEL_CONFIG_PATH,
					 info->applet_id+1);
			g_free(launcher->dentry->location);
			launcher->dentry->location = g_strdup(buf->str);
			gnome_desktop_entry_save(launcher->dentry);

			gnome_config_set_string("id", LAUNCHER_ID);
			gnome_config_set_string("parameters", buf->str);
			break;
		}
	case APPLET_LOGOUT:
		gnome_config_set_string("id", LOGOUT_ID);
		break;
	case APPLET_LOCK:
		gnome_config_set_string("id", LOCK_ID);
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
	
	return TRUE;
}

static void
save_panel_configuration(gpointer data, gpointer user_data)
{
	GString       *buf;
	int           *num = user_data;
	PanelData     *pd = data;
	BasePWidget   *basep = BASEP_WIDGET(pd->panel);
	PanelWidget   *panel = PANEL_WIDGET(basep->panel);
	
	buf = g_string_new(NULL);

	g_string_sprintf(buf, "%spanel/Panel_%d/", PANEL_CONFIG_PATH, (*num)++);
	gnome_config_clean_section(buf->str);

	gnome_config_push_prefix (buf->str);

	gnome_config_set_int("type",pd->type);

	gnome_config_set_bool("hidebuttons_enabled",
			      basep->hidebuttons_enabled);
	gnome_config_set_bool("hidebutton_pixmaps_enabled",
			      basep->hidebutton_pixmaps_enabled);
	

	gnome_config_set_int ("mode", basep->mode);
	gnome_config_set_int ("state", basep->state);

	gnome_config_set_int("sz", panel->sz);

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

	/* now do different types */
	if (IS_BORDER_WIDGET(basep))
		gnome_config_set_int("edge", BORDER_POS(basep->pos)->edge);

	switch (pd->type) {
	case ALIGNED_PANEL:
		gnome_config_set_int ("align", ALIGNED_POS (basep->pos)->align);
		break;
	case SLIDING_PANEL:
		gnome_config_set_int ("offset", SLIDING_POS (basep->pos)->offset);
		gnome_config_set_int ("anchor", SLIDING_POS (basep->pos)->anchor);
		break;
	case DRAWER_PANEL:
		gnome_config_set_int ("orient", DRAWER_POS (basep->pos)->orient);
		/*gnome_config_set_int ("temp_hidden", DRAWER_POS (basep->pos)->temp_state);*/
		break;
	default:
		break;
	}

	gnome_config_pop_prefix ();
}

void
save_next_applet(void)
{
	GSList *cur;

	ss_cur_applet++;
	
	if(g_slist_length(applets)<=ss_cur_applet) {
		ss_done_save = TRUE;
		return;
	}
	
	cur = g_slist_nth(applets,ss_cur_applet);
	
	if(!cur) {
		ss_done_save = TRUE;
		return;
	}
	
	if(save_applet_configuration(cur->data))
		save_next_applet();

	gnome_config_sync();
	gnome_config_drop_all();
}

static void
do_session_save(GnomeClient *client,
		int complete_sync,
		int sync_applets,
		int sync_panels,
		int sync_globals)
{
	int num;
	char *s;
	int i;
#if 0
	gchar *new_args[] = { "rm", "-r", NULL };

	if (panel_cfg_path)
		g_free(panel_cfg_path);

	if (gnome_client_get_flags(client) & GNOME_CLIENT_IS_CONNECTED &&
	    GNOME_CLIENT (client)->restart_style != GNOME_RESTART_NEVER)
		panel_cfg_path = g_strdup (gnome_client_get_config_prefix (client));
	else
		;
	panel_cfg_path = g_strdup ("/panel.d/default/");

	new_args[2] = gnome_config_get_real_path (panel_cfg_path);
	gnome_client_set_discard_command (client, 3, new_args);
#endif

#ifdef PANEL_DEBUG	
	printf("Saving to [%s]\n",PANEL_CONFIG_PATH);

	printf("Saving session: 1"); fflush(stdout);

	printf(" 2"); fflush(stdout);
#endif

	s = g_concat_dir_and_file(PANEL_CONFIG_PATH,"panel/Config/");
	gnome_config_push_prefix (s);
	g_free(s);

	if(complete_sync || sync_applets)
		gnome_config_set_int ("applet_count", applet_count);
#ifdef PANEL_DEBUG
	printf(" 3"); fflush(stdout);
#endif
	if(complete_sync || sync_panels) {
		num = 1;
		g_slist_foreach(panel_list, save_panel_configuration,&num);
		gnome_config_set_int("panel_count",num-1);
	}
#ifdef PANEL_DEBUG
	printf(" 4\n"); fflush(stdout);
#endif
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
		gnome_config_set_bool("hungry_menus",
				      global_config.hungry_menus);
		gnome_config_set_bool("off_panel_popups",
				      global_config.off_panel_popups);
		gnome_config_set_bool("disable_animations",
				      global_config.disable_animations);
		gnome_config_set_int("applet_padding",
				     global_config.applet_padding);
		gnome_config_set_bool("autoraise",
				      global_config.autoraise);
		gnome_config_set_bool("keep_bottom",
				      global_config.keep_bottom);
		gnome_config_set_bool("drawer_auto_close",
				      global_config.drawer_auto_close);
		gnome_config_set_bool("simple_movement",
				      global_config.simple_movement);
		gnome_config_set_bool("hide_panel_frame",
				      global_config.hide_panel_frame);
		gnome_config_set_bool("tile_when_over",
				      global_config.tile_when_over);
		gnome_config_set_bool("show_startup_hints",
				      global_config.show_startup_hints);
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
	
	if(complete_sync || sync_applets) {
		ss_cur_applet = -1;
		ss_done_save = FALSE;
		save_next_applet();
	}

#if 0 /*PANEL_DEBUG*/
	puts("");
#endif
}

void
panel_config_sync(void)
{
	if(need_complete_save ||
	   applets_to_sync ||
	   panels_to_sync ||
	   globals_to_sync) {
		if (!(gnome_client_get_flags(client) & 
		      GNOME_CLIENT_IS_CONNECTED)) {
			int ncs = need_complete_save;
			int ats = applets_to_sync;
			int pts = panels_to_sync;
			int gts = globals_to_sync;
			need_complete_save = FALSE;
			applets_to_sync = FALSE;
			panels_to_sync = FALSE;
			globals_to_sync = FALSE;
			do_session_save(client,ncs,ats,pts,gts);
		} else {
			/*prevent possible races by doing this before requesting
			  save*/
			need_complete_save = FALSE;
			applets_to_sync = FALSE;
			panels_to_sync = FALSE;
			globals_to_sync = FALSE;
			gnome_client_request_save (client, GNOME_SAVE_LOCAL, FALSE,
						   GNOME_INTERACT_NONE, FALSE, FALSE);
		}
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
	if(is_shutdown) {
		ss_timeout = 1500;
		ss_interactive = TRUE;
	}
	do_session_save(client,TRUE,FALSE,FALSE,FALSE);
	if(is_shutdown) {
		GSList *li;
		for(li=panel_list;li;li=g_slist_next(li)) {
			PanelData *pd = li->data;
			gtk_widget_hide(pd->panel);
			if (pd->menu)
				gtk_widget_hide(pd->menu);
		}
		while(!ss_done_save)
			gtk_main_iteration_do(TRUE);
	}
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

/* the logout function */
void
panel_quit(void)
{
	gnome_client_request_save (client, GNOME_SAVE_BOTH, 1,
				   GNOME_INTERACT_ANY, 0, 1);
}

static void
load_default_applets1(PanelWidget *panel)
{
	char *def_launchers[] =
	      { "gnome/apps/gnome-help.desktop",
		"gnome/apps/Utilities/gnome-terminal.desktop",
		"gnome/apps/Settings/gnomecc.desktop",
		"gnome/apps/Applications/Netscape.desktop",
		NULL };
	int i;
	int flags = MAIN_MENU_SYSTEM|MAIN_MENU_USER|
		MAIN_MENU_SYSTEM_SUB|MAIN_MENU_USER_SUB;
	char *p;

	/*guess redhat menus*/
	if(g_file_exists("/etc/X11/wmconfig"))
		flags |= MAIN_MENU_REDHAT|MAIN_MENU_REDHAT_SUB;
	/*guess KDE menus */
	if(g_file_exists(KDE_MENUDIR))
		flags |= MAIN_MENU_KDE|MAIN_MENU_KDE_SUB;
	/*guess debian menus*/
	if (g_file_exists("/etc/menu-methods/gnome"))
		flags |= MAIN_MENU_DEBIAN|MAIN_MENU_DEBIAN_SUB;
	load_menu_applet(NULL,flags, panels->data, 0);

	/*load a Programs menu*/
	p = gnome_datadir_file ("gnome/apps");
	/*it really should exist*/
	if(g_file_exists(p))
		load_menu_applet(p,flags, panel, 1);
	g_free(p);
	
	if(gdk_screen_width()>800) {
		/*load up some launchers, but only if screen larger then 800*/
		for(i=0;def_launchers[i]!=NULL;i++) {
			p = gnome_datadir_file (def_launchers[i]);
			/*int center = gdk_screen_width()/2;*/
			if(p) {
				load_launcher_applet(p,panel,48*3+i*48);
				g_free(p);
			}
		}
	}

	load_extern_applet("tasklist_applet",NULL,
			   panel,INT_MAX/2/*flush right*/,TRUE);
	load_extern_applet("gen_util_clock",NULL,
			   panel,INT_MAX/2 + 1000/*flush right*/,TRUE);
	load_extern_applet("gen_util_mailcheck",NULL,
			   panel,INT_MAX/2 + 2000/*flush right*/,TRUE);
}

static void
load_default_applets2(PanelWidget *panel)
{
	load_extern_applet("deskguide_applet",NULL,
			   panel,0,TRUE);
}

void
init_user_applets(void)
{
	GString *buf;
	int   count,num;	

	buf = g_string_new(NULL);
	g_string_sprintf(buf,"%spanel/Config/applet_count=0",
			 PANEL_CONFIG_PATH);
	count=gnome_config_get_int(buf->str);
	for(num=1;num<=count;num++) {
		char *applet_name;
		int   pos=0,panel_num;
		PanelWidget *panel;

		g_string_sprintf(buf,"%sApplet_Config/Applet_%d/",
				 PANEL_CONFIG_PATH, num);
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
						 PANEL_CONFIG_PATH,num);
				load_extern_applet(goad_id,buf->str,panel,pos,TRUE);
			}
			g_free(goad_id);
		} else if(strcmp(applet_name,LAUNCHER_ID) == 0) { 
			char *params = gnome_config_get_string("parameters=");
			load_launcher_applet(params,panel,pos);
			g_free(params);
		} else if(strcmp(applet_name,LOGOUT_ID) == 0) { 
			load_logout_applet(panel,pos);
		} else if(strcmp(applet_name,LOCK_ID) == 0) {
			load_lock_applet(panel,pos);
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
				/*guess KDE menus */
				if(g_file_exists(KDE_MENUDIR))
					flags |= MAIN_MENU_KDE|MAIN_MENU_KDE_SUB;
				/*guess debian menus*/
				if (g_file_exists("/etc/menu-methods/gnome"))
					flags |= MAIN_MENU_DEBIAN|MAIN_MENU_DEBIAN_SUB;
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
	GtkWidget *panel=NULL;

	buf = g_string_new(NULL);
	g_string_sprintf(buf,"%spanel/Config/panel_count=0",
			 PANEL_CONFIG_PATH);
	count=gnome_config_get_int(buf->str);

	/*load a default snapped panel on the bottom of the screen,
	  it is required to have at least one panel for this all
	  to work, so this is the way we find out if there was no
	  config from last time*/
	if(count<=0)  {
		panel = edge_widget_new(BORDER_BOTTOM,
					BASEP_EXPLICIT_HIDE,
					BASEP_SHOWN,
					SIZE_STANDARD,
					TRUE,
					TRUE,
					PANEL_BACK_NONE,
					NULL,
					TRUE,
					NULL);
		panel_setup(panel);
		gtk_widget_show(panel);

		/*load up default applets on the default panel*/
		load_default_applets1(PANEL_WIDGET(BASEP_WIDGET(panel)->panel));

		panel = aligned_widget_new(ALIGNED_LEFT,
					   BORDER_RIGHT,
					   BASEP_EXPLICIT_HIDE,
					   BASEP_SHOWN,
					   SIZE_STANDARD,
					   TRUE,
					   TRUE,
					   PANEL_BACK_NONE,
					   NULL,
					   TRUE,
					   NULL);
		panel_setup(panel);
		gtk_widget_show(panel);

		/*load up default applets on the second default panel*/
		load_default_applets2(PANEL_WIDGET(BASEP_WIDGET(panel)->panel));

		/*we laoded default applets, so we didn't find the config
		  or something else was wrong, so do complete save when
		  next syncing*/
		need_complete_save = TRUE;

		g_string_free(buf,TRUE);
		return;
	}

	for(num=1;num<=count;num++) {
		PanelType type;
		PanelBackType back_type;
		PanelSizeType sz;
		BasePState state;
		BasePMode mode;
		BorderEdge edge;
		char *back_pixmap, *color;
		GdkColor back_color = {0,0,0,1};
		int fit_pixmap_bg;
		int hidebuttons_enabled;
		int hidebutton_pixmaps_enabled;
		
		g_string_sprintf(buf,"%spanel/Panel_%d/",
				 PANEL_CONFIG_PATH, num);
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

		g_string_sprintf(buf,"sz=%d", SIZE_STANDARD);
		sz=gnome_config_get_int(buf->str);
		
		/*now for type specific config*/

		g_string_sprintf(buf,"type=%d", EDGE_PANEL);
		type = gnome_config_get_int(buf->str);

		hidebuttons_enabled =
			gnome_config_get_bool("hidebuttons_enabled=TRUE");
		hidebutton_pixmaps_enabled =
			gnome_config_get_bool("hidebutton_pixmaps_enabled=TRUE");

		state = gnome_config_get_int("state=0");
		mode = gnome_config_get_int("mode=0");
#if 0 /* i guess we can't easily do this for now */
		pos = basep_widget_load_pos_settings();
#endif
		switch (type) {
			
		case EDGE_PANEL:
			g_string_sprintf (buf, "edge=%d", BORDER_BOTTOM);
			edge = gnome_config_get_int (buf->str);
			panel = edge_widget_new (edge, 
						 mode, state, sz,
						 hidebuttons_enabled,
						 hidebutton_pixmaps_enabled,
						 back_type, back_pixmap,
						 fit_pixmap_bg, &back_color);
			break;
		case ALIGNED_PANEL: {
			AlignedAlignment align;
			g_string_sprintf (buf, "edge=%d", BORDER_BOTTOM);
			edge = gnome_config_get_int (buf->str);
			
			g_string_sprintf (buf, "align=%d", ALIGNED_LEFT);
			align = gnome_config_get_int (buf->str);

			panel = aligned_widget_new (align, edge,
						    mode, state, sz,
						    hidebuttons_enabled,
						    hidebutton_pixmaps_enabled,
						    back_type, back_pixmap,
						    fit_pixmap_bg, &back_color);
			break;
		}
		case SLIDING_PANEL: {
			gint16 offset;
			SlidingAnchor anchor;
			g_string_sprintf (buf, "edge=%d", BORDER_BOTTOM);
			edge = gnome_config_get_int (buf->str);
			
			g_string_sprintf (buf, "anchor=%d", SLIDING_ANCHOR_LEFT);
			anchor = gnome_config_get_int (buf->str);

			offset = gnome_config_get_int ("offset=0");

			panel = sliding_widget_new (anchor, offset, edge,
						    mode, state, sz,
						    hidebuttons_enabled,
						    hidebutton_pixmaps_enabled,
						    back_type, back_pixmap,
						    fit_pixmap_bg, &back_color);
			break;
		}
		case DRAWER_PANEL: {
			PanelOrientType orient;
			/*BasePState temp_state;*/

			g_string_sprintf (buf, "orient=%d", ORIENT_UP);
			orient = gnome_config_get_int (buf->str);

#warning FIXME: there are some issues with auto hiding drawers
			panel = drawer_widget_new (orient,
						   BASEP_EXPLICIT_HIDE, 
						   state, sz,
						   hidebuttons_enabled,
						   hidebutton_pixmaps_enabled,
						   back_type, back_pixmap,
						   fit_pixmap_bg, &back_color);
#if 0
			g_string_sprintf (buf, "temp_state=%d", BASEP_SHOWN);
			temp_state = gnome_config_get_int (buf->str);
			DRAWER_POS (BASEP_WIDGET (panel)->pos)->temp_state = temp_state;
#endif
			break;
		}
#if 0
		case FREE_PANEL:
			break;
#endif
		default:
			g_assert_not_reached ();
			break;
		}

		gnome_config_pop_prefix ();
		
		g_free(color);
		g_free(back_pixmap);

		if (panel) {
			panel_setup(panel);
			gtk_widget_show(panel);
		}
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
	
	g_string_sprintf(buf,"%spanel/Config/",PANEL_CONFIG_PATH);
	gnome_config_push_prefix(buf->str);

	global_config.tooltips_enabled =
		gnome_config_get_bool("tooltips_enabled=TRUE");

	global_config.show_small_icons =
		gnome_config_get_bool("show_small_icons=TRUE");

	global_config.show_dot_buttons =
		gnome_config_get_bool("show_dot_buttons=FALSE");

	global_config.hungry_menus =
		gnome_config_get_bool("hungry_menus=FALSE");

	global_config.off_panel_popups =
		gnome_config_get_bool("off_panel_popups=TRUE");
		
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

	global_config.autoraise = gnome_config_get_bool("autoraise=TRUE");

	global_config.keep_bottom = gnome_config_get_bool("keep_bottom=FALSE");

	global_config.drawer_auto_close = gnome_config_get_bool("drawer_auto_close=FALSE");
	global_config.simple_movement = gnome_config_get_bool("simple_movement=FALSE");
	global_config.hide_panel_frame = gnome_config_get_bool("hide_panel_frame=FALSE");
	global_config.tile_when_over = gnome_config_get_bool("tile_when_over=FALSE");
	global_config.show_startup_hints = gnome_config_get_bool("show_startup_hints=TRUE");
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
