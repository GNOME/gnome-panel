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

#include "panel-widget.h"
#include "snapped-widget.h"
#include "drawer-widget.h"
#include "corner-widget.h"
#include "gdkextra.h"
#include "panel.h"
#include "main.h"
#include "menu.h"
#include "launcher.h"
#include "drawer.h"
#include "swallow.h"
#include "mulapp.h"
#include "mico-glue.h"
#include "panel_config.h"
#include "panel_config_global.h"
#include <gdk/gdkx.h>

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

extern int config_sync_timeout;
extern int panels_to_sync;
extern GList *applets_to_sync;
extern int globals_to_sync;
extern int need_complete_save;


extern GArray *applets;
extern int applet_count;

extern GtkTooltips *panel_tooltips;
extern int tooltips_enabled;

extern GnomeClient *client;

extern GtkWidget *root_menu;
extern GList *small_icons;

extern GlobalConfig global_config;

extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

/*list of started applets*/
extern GList * children;

/*list of all panel widgets created*/
extern GList *panel_list;


/*send the tooltips state to all external applets*/
static void
send_tooltips_state(int enabled)
{
	GList *list;
	
	for(list = children;list!=NULL;list = g_list_next(list)) {
		AppletChild *child = list->data;
		AppletInfo *info = get_applet_info(child->applet_id);
		send_applet_tooltips_state(info->id_str,enabled);
	}
}

void
apply_global_config(void)
{
	panel_widget_change_global(global_config.explicit_hide_step_size,
				   global_config.auto_hide_step_size,
				   global_config.drawer_step_size,
				   global_config.minimized_size,
				   global_config.minimize_delay,
				   global_config.movement_type,
				   global_config.disable_animations);
	if(global_config.tooltips_enabled)
		gtk_tooltips_enable(panel_tooltips);
	else
		gtk_tooltips_disable(panel_tooltips);
	g_list_foreach(small_icons,set_show_small_icons,NULL);
	send_tooltips_state(global_config.tooltips_enabled);
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

	g_snprintf(path,256, "%sApplet_%d/", panel_cfg_path, num+1);
	gnome_config_push_prefix(path);

	/*obviously no need for saving*/
	if(info->type==APPLET_EXTERN_PENDING ||
	   info->type==APPLET_EXTERN_RESERVED ||
	   info->type==APPLET_EMPTY) {
		gnome_config_set_string("config/id", EMPTY_ID);
		gnome_config_pop_prefix();
		return;
	}

	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);
	ad = gtk_object_get_data(GTK_OBJECT(info->widget),PANEL_APPLET_DATA);

	if((panel_num = g_list_index(panels,panel)) == -1) {
		gnome_config_set_string("config/id", EMPTY_ID);
		gnome_config_pop_prefix();
		return;
	}

	if(info->type==APPLET_EXTERN) {
		char *globalcfg;
		/*sync before the applet does it's stuff*/
		gnome_config_sync();
		/*I think this should be done at sync and also that there
		  should be some flocking ... but this works for now*/
		gnome_config_drop_all();
		
		globalcfg = g_copy_strings(panel_cfg_path,"Applet_All/",NULL);

		/*have the applet do it's own session saving*/
		if(send_applet_session_save(info->id_str,info->applet_id,path,
					    globalcfg)) {
			
			gnome_config_set_string("config/id", EXTERN_ID);
			gnome_config_set_int("config/position", ad->pos);
			gnome_config_set_int("config/panel", panel_num);
			gnome_config_set_string("config/execpath", info->path);
			gnome_config_set_string("config/parameters",
						info->params);
			gnome_config_set_bool("config/right_stick",
			      panel_widget_is_applet_stuck(panel,
							   info->widget));
		} else
			gnome_config_set_string("config/id", EMPTY_ID);
		g_free(globalcfg);
	} else {
		gnome_config_set_string("config/id", info->id_str);
		gnome_config_set_int("config/position", ad->pos);
		gnome_config_set_int("config/panel", panel_num);
		gnome_config_set_bool("config/right_stick",
			      panel_widget_is_applet_stuck(panel,
							   info->widget));

		if(strcmp(info->id_str,DRAWER_ID) == 0) {
			int i;
			Drawer *drawer = info->data;

			i = g_list_index(panels,
					 DRAWER_WIDGET(info->assoc)->panel);
			if(i>=0)
				gnome_config_set_int("config/parameters",i);
			else
				g_warning("Drawer not associated with applet!");
			gnome_config_set_string("config/pixmap",
						drawer->pixmap);
			gnome_config_set_string("config/tooltip",
						drawer->tooltip);
		} else if(strcmp(info->id_str,SWALLOW_ID) == 0) {
			Swallow *swallow = info->data;
			gnome_config_set_string("config/parameters",
						info->params);
			gnome_config_set_string("config/execpath",
						info->path);
			gnome_config_set_int("config/width",swallow->width);
			gnome_config_set_int("config/height",swallow->height);
		} else if(strcmp(info->id_str,LAUNCHER_ID) == 0) {
			Launcher *launcher = info->data;
			char *s;
			/*get rid of the trailing slash*/
			path[strlen(path)-1]='\0';
			s = g_concat_dir_and_file(gnome_user_dir,path);
			gnome_config_set_string("config/parameters", s);
			g_free(launcher->dentry->location);
			launcher->dentry->location = s;
			gnome_config_sync();
			gnome_desktop_entry_save(launcher->dentry);
		} else {
			if(info->params)
				gnome_config_set_string("config/parameters",
							info->params);
		}
	}
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
	
	if(pd->type == SNAPPED_PANEL) {
		SnappedWidget *snapped = SNAPPED_WIDGET(pd->panel);
		gnome_config_set_int("pos", snapped->pos);
		gnome_config_set_int("mode", snapped->mode);
		gnome_config_set_int("state", snapped->state);
	} else if(pd->type == CORNER_PANEL) {
		CornerWidget *corner = CORNER_WIDGET(pd->panel);
		gnome_config_set_int("pos", corner->pos);
		gnome_config_set_int("orient",panel->orient);
		gnome_config_set_int("state", corner->state);
	} else if(pd->type == DRAWER_PANEL) {
		DrawerWidget *drawer = DRAWER_WIDGET(pd->panel);
		gnome_config_set_int("orient",panel->orient);
		gnome_config_set_int("state", drawer->state);
		gnome_config_set_int("drop_zone_pos",
				     drawer->drop_zone_pos);

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
		GList *sync_applets,
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
		while(sync_applets) {
			save_applet_configuration(GPOINTER_TO_INT(sync_applets->data));
			sync_applets = g_list_remove_link(sync_applets,
							  sync_applets);
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
		gnome_config_set_int("minimized_size", global_config.minimized_size);
		gnome_config_set_int("minimize_delay", global_config.minimize_delay);
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
				applets_to_sync,panels_to_sync,globals_to_sync);
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
		if(info->type == APPLET_EXTERN ||
		   info->type == APPLET_SWALLOW)
			gtk_container_remove(GTK_CONTAINER(info->widget),
					     info->applet_widget);
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

	if(button!=0)
		gnome_dialog_close(GNOME_DIALOG(w));
	else {
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
	gtk_window_position(GTK_WINDOW(box), GTK_WIN_POS_CENTER);
	gtk_window_set_policy(GTK_WINDOW(box), FALSE, FALSE, TRUE);

	gtk_signal_connect (GTK_OBJECT (box), "clicked",
		            GTK_SIGNAL_FUNC (panel_really_logout), &box);

	but = gtk_check_button_new_with_label(_("Ask next time"));
	gtk_widget_show(but);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(box)->vbox),but,
		           FALSE, TRUE, GNOME_PAD_SMALL);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(but),TRUE);
	gtk_signal_connect(GTK_OBJECT(but),"toggled",
			   GTK_SIGNAL_FUNC(ask_next_time),NULL);

	/* gnome_dialog_set_modal (GNOME_DIALOG(box)); */
	/* gnome_dialog_close_hides(GNOME_DIALOG(box), TRUE); */
	gtk_widget_show (box);
}

static void
move_applet_callback(GtkWidget *widget, gpointer data)
{
	AppletInfo     *info = get_applet_info(GPOINTER_TO_INT(data));
	PanelWidget    *panel;

	g_return_if_fail(info != NULL);

	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);
	g_return_if_fail(panel!=NULL);

	panel_widget_applet_drag_start(panel,info->widget);
	panel_widget_applet_move_use_idle(panel);
}

void
panel_clean_applet(int applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);
	AppletType type;

	g_return_if_fail(info != NULL);

	/*fixes reentrancy problem with this routine*/
	if(info->type == APPLET_EMPTY)
		return;

	type = info->type;
	info->type = APPLET_EMPTY;

	if(info->widget) {
		PanelWidget *panel;
		GtkWidget *w = info->widget;

		info->widget = NULL;

		gtk_signal_disconnect(GTK_OBJECT(w),info->destroy_callback);
		info->destroy_callback = -1;

		panel = gtk_object_get_data(GTK_OBJECT(w),
					    PANEL_APPLET_PARENT_KEY);

		if(panel)
			panel_widget_remove(panel,w);
	}
	info->applet_widget = NULL;
	if(type == APPLET_DRAWER && info->assoc) {
		GtkWidget *dw = info->assoc;
		info->assoc=NULL;
		PANEL_WIDGET(DRAWER_WIDGET(dw)->panel)->master_widget = NULL;
		gtk_widget_destroy(dw);
	}
	info->assoc=NULL;
	if(info->menu)
		gtk_widget_unref(info->menu);
	info->menu=NULL;
	info->remove_item = NULL;

	if(info->id_str) g_free(info->id_str);
	info->id_str=NULL;
	if(info->path) g_free(info->path);
	info->path=NULL;
	if(info->params) g_free(info->params);
	info->params=NULL;
	if(info->cfg) g_free(info->cfg);
	info->cfg=NULL;

	info->data=NULL;

	/*free the user menu*/
	while(info->user_menu) {
		AppletUserMenu *umenu = info->user_menu->data;
		if(umenu->name)
			g_free(umenu->name);
		if(umenu->stock_item)
			g_free(umenu->stock_item);
		if(umenu->text)
			g_free(umenu->text);
		g_free(umenu);
		info->user_menu = g_list_remove_link(info->user_menu,
						     info->user_menu);
	}
	mulapp_remove_empty_from_list();
}

static void
remove_applet_callback(GtkWidget *widget, gpointer data)
{
	panel_clean_applet(GPOINTER_TO_INT(data));
}

static void
applet_callback_callback(GtkWidget *widget, gpointer data)
{
	AppletUserMenu *menu = data;
	AppletInfo *info = get_applet_info(menu->applet_id);

	g_return_if_fail(info != NULL);

	switch(info->type) {
	case APPLET_EXTERN:
		send_applet_do_callback(info->id_str,
					info->applet_id,
					menu->name);
		break;
	case APPLET_LAUNCHER:
		if(strcmp(menu->name,"properties")==0)
			launcher_properties(info->data);
		break;
	case APPLET_DRAWER:
		if(strcmp(menu->name,"properties")==0)
			drawer_properties(info->data);
		break;
	default: break;
	}
}

static void
applet_menu_deactivate(GtkWidget *w, gpointer data)
{
	GtkWidget *applet = data;
	GtkWidget *panel = get_panel_parent(applet);
	
	if(IS_SNAPPED_WIDGET(panel))
		SNAPPED_WIDGET(panel)->autohide_inhibit = FALSE;
}

static AppletUserMenu *
applet_get_callback(GList *user_menu, char *name)
{
	GList *list;
	for(list=user_menu;list!=NULL;list=g_list_next(list)) {
		AppletUserMenu *menu = list->data;
		if(strcmp(menu->name,name)==0)
			return menu;
	}
	return NULL;	
}

void
applet_add_callback(int applet_id,
		    char *callback_name,
		    char *stock_item,
		    char *menuitem_text)
{
	AppletUserMenu *menu;
	AppletInfo *info = get_applet_info(applet_id);

	g_return_if_fail(info != NULL);
	
	if((menu=applet_get_callback(info->user_menu,callback_name))==NULL) {
		menu = g_new(AppletUserMenu,1);
		menu->name = g_strdup(callback_name);
		menu->stock_item = g_strdup(stock_item);
		menu->text = g_strdup(menuitem_text);
		menu->applet_id = applet_id;
		menu->menuitem = NULL;
		menu->submenu = NULL;
		info->user_menu = g_list_append(info->user_menu,menu);
	} else {
		if(menu->stock_item)
			g_free(menu->stock_item);
		if(menu->text)
			g_free(menu->text);
		menu->text = g_strdup(menuitem_text);
		menu->stock_item = g_strdup(stock_item);
	}

	/*make sure the menu is rebuilt*/
	if(info->menu) {
		GList *list;
		for(list=info->user_menu;list!=NULL;list=g_list_next(list)) {
			AppletUserMenu *menu = list->data;
			menu->menuitem=NULL;
			menu->submenu=NULL;
		}
		gtk_widget_unref(info->menu);
		info->menu=NULL;
	}
}

void
applet_remove_callback(int applet_id, char *callback_name)
{
	AppletUserMenu *menu;
	AppletInfo *info = get_applet_info(applet_id);

	g_return_if_fail(info != NULL);
	
	if((menu=applet_get_callback(info->user_menu,callback_name))!=NULL) {
		info->user_menu = g_list_remove(info->user_menu,menu);
		if(menu->name)
			g_free(menu->name);
		if(menu->stock_item)
			g_free(menu->stock_item);
		if(menu->text)
			g_free(menu->text);
		g_free(menu);
	}

	/*make sure the menu is rebuilt*/
	if(info->menu) {
		GList *list;
		for(list=info->user_menu;list!=NULL;list=g_list_next(list)) {
			AppletUserMenu *menu = list->data;
			menu->menuitem=NULL;
			menu->submenu=NULL;
		}
		gtk_widget_unref(info->menu);
		info->menu=NULL;
	}
}

static void
add_to_submenus(int applet_id,
		char *path,
		char *name,
	       	AppletUserMenu *menu,
	       	GtkWidget *submenu,
		GList *user_menu)
{
	char *n = g_strdup(name);
	char *p = strchr(n,'/');
	char *t;
	AppletUserMenu *s_menu;
	
	/*this is the last one*/
	if(p==NULL ||
	   p==(n + strlen(n) - 1)) {
		g_free(n);

		menu->menuitem = gtk_menu_item_new ();
		if(menu->stock_item && *(menu->stock_item))
			setup_menuitem (menu->menuitem,
					gnome_stock_pixmap_widget(submenu,
								  menu->stock_item),
					menu->text);
		else
			setup_menuitem (menu->menuitem,
					NULL,
					menu->text);
		if(menu->submenu)
			gtk_menu_item_set_submenu (GTK_MENU_ITEM(menu->menuitem),
						   menu->submenu);

		if(submenu)
			gtk_menu_append (GTK_MENU (submenu), menu->menuitem);
		/*if an item not a submenu*/
		if(p==NULL) {
			gtk_signal_connect(GTK_OBJECT(menu->menuitem), "activate",
					   (GtkSignalFunc) applet_callback_callback,
					   menu);
		/* if the item is a submenu and doesn't have it's menu created yet*/
		} else if(!menu->submenu) {
			menu->submenu = gtk_menu_new();
			gtk_menu_item_set_submenu (GTK_MENU_ITEM(menu->menuitem),
						   menu->submenu);
		}
		return;
	}
	
	*p = '\0';
	p++;
	
	t = g_copy_strings(path,n,"/",NULL);
	s_menu = applet_get_callback(user_menu,t);
	/*the user did not give us this sub menu, whoops, will create an empty
	  one then*/
	if(!s_menu) {
		AppletInfo *info = get_applet_info(applet_id);
		s_menu = g_new(AppletUserMenu,1);
		s_menu->name = g_strdup(t);
		s_menu->text = g_strdup(_("???"));
		s_menu->stock_item = NULL;
		s_menu->applet_id = applet_id;
		s_menu->menuitem = NULL;
		s_menu->submenu = NULL;
		info->user_menu = g_list_append(info->user_menu,s_menu);
		user_menu = info->user_menu;
	}
	
	if(!s_menu->submenu)
		s_menu->submenu = gtk_menu_new();
	
	add_to_submenus(applet_id,t,p,menu,s_menu->submenu,user_menu);
	
	g_free(t);
	g_free(n);
}

static void
create_applet_menu(AppletInfo *info)
{
	GtkWidget *menuitem;
	GList *user_menu = info->user_menu;

	info->menu = gtk_menu_new();

	menuitem = gtk_menu_item_new();
	setup_menuitem(menuitem,NULL,_("Remove from panel"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) remove_applet_callback,
			   GINT_TO_POINTER(info->applet_id));
	gtk_menu_append(GTK_MENU(info->menu), menuitem);
	info->remove_item = menuitem;
	
	menuitem = gtk_menu_item_new();
	setup_menuitem(menuitem,NULL,_("Move applet"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) move_applet_callback,
			   GINT_TO_POINTER(info->applet_id));
	gtk_menu_append(GTK_MENU(info->menu), menuitem);
	
	if(user_menu) {
		menuitem = gtk_menu_item_new();
		gtk_menu_append(GTK_MENU(info->menu), menuitem);
		gtk_widget_show(menuitem);
	}

	for(;user_menu!=NULL;user_menu = g_list_next(user_menu)) {
		AppletUserMenu *menu=user_menu->data;
		add_to_submenus(info->applet_id,"",menu->name,menu,info->menu,
				info->user_menu);
	}

	/*connect the deactivate signal, so that we can "re-allow" autohide
	  when the menu is deactivated*/
	gtk_signal_connect(GTK_OBJECT(info->menu),"deactivate",
			   GTK_SIGNAL_FUNC(applet_menu_deactivate),
			   info->widget);
}

static void
applet_menu_position (GtkMenu *menu, int *x, int *y, gpointer data)
{
	int wx, wy;
	AppletInfo *info = get_applet_info(GPOINTER_TO_INT(data));
	PanelWidget *panel;
	GtkWidget *w; /*the panel window widget*/

	g_return_if_fail(info != NULL);
	g_return_if_fail(info->widget != NULL);

	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);

	g_return_if_fail(panel != NULL);
	
	w = gtk_object_get_data(GTK_OBJECT(panel), PANEL_PARENT);

	gdk_window_get_origin (info->widget->window, &wx, &wy);

	if(IS_DRAWER_WIDGET(w)) {
		if(panel->orient==PANEL_VERTICAL) {
			*x = wx + info->widget->allocation.width;
			*y = wy;
		} else {
			*x = wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
		}
	} else if(IS_SNAPPED_WIDGET(w)) {
		switch(SNAPPED_WIDGET(w)->pos) {
		case SNAPPED_BOTTOM:
			*x = wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
			break;
		case SNAPPED_TOP:
			*x = wx;
			*y = wy + info->widget->allocation.height;
			break;
		case SNAPPED_LEFT:
			*x = wx + info->widget->allocation.width;
			*y = wy;
			break;
		case SNAPPED_RIGHT:
			*x = wx - GTK_WIDGET (menu)->allocation.width;
			*y = wy;
			break;
		}
	} else if(IS_CORNER_WIDGET(w)) {
		if(panel->orient==PANEL_HORIZONTAL) {
			switch(CORNER_WIDGET(w)->pos) {
			case CORNER_SE:
			case CORNER_SW:
				*x = wx;
				*y = wy - GTK_WIDGET (menu)->allocation.height;
				break;
			case CORNER_NE:
			case CORNER_NW:
				*x = wx;
				*y = wy + info->widget->allocation.height;
				break;
			}
		} else {
			switch(CORNER_WIDGET(w)->pos) {
			case CORNER_NW:
			case CORNER_SW:
				*x = wx + info->widget->allocation.width;
				*y = wy;
				break;
			case CORNER_NE:
			case CORNER_SE:
				*x = wx - GTK_WIDGET (menu)->allocation.width;
				*y = wy;
				break;
			}
		}
	}

	if(*x + GTK_WIDGET (menu)->allocation.width > gdk_screen_width())
		*x=gdk_screen_width() - GTK_WIDGET (menu)->allocation.width;
	if(*x < 0) *x =0;

	if(*y + GTK_WIDGET (menu)->allocation.height > gdk_screen_height())
		*y=gdk_screen_height() - GTK_WIDGET (menu)->allocation.height;
	if(*y < 0) *y =0;
}


static void
show_applet_menu(int applet_id, GdkEventButton *event)
{
	AppletInfo *info = get_applet_info(applet_id);
	GtkWidget *panel;

	g_return_if_fail(info!=NULL);

	panel = get_panel_parent(info->widget);

	if (!info->menu)
		create_applet_menu(info);

	if((info->type == APPLET_DRAWER &&
	    panel_widget_get_applet_count(
	      PANEL_WIDGET(DRAWER_WIDGET(info->assoc)->panel)) > 0))
	   	gtk_widget_set_sensitive(info->remove_item,FALSE);
	else
	   	gtk_widget_set_sensitive(info->remove_item,TRUE);

	if(IS_SNAPPED_WIDGET(panel)) {
		SNAPPED_WIDGET(panel)->autohide_inhibit = TRUE;
		snapped_widget_queue_pop_down(SNAPPED_WIDGET(panel));
	}
	gtk_menu_popup(GTK_MENU(info->menu), NULL, NULL, applet_menu_position,
		       GINT_TO_POINTER(applet_id), event->button, event->time);
}



static int
applet_button_press(GtkWidget *widget,GdkEventButton *event, gpointer data)
{
	if(event->button==3) {
		if(!panel_applet_in_drag)
			show_applet_menu(GPOINTER_TO_INT(data), event);
	}
	/*stop all button press events here so that they don't get to the
	  panel*/
	return TRUE;
}

void
applet_show_menu(int applet_id)
{
	static GdkCursor *arrow = NULL;
	AppletInfo *info = get_applet_info(applet_id);
	GtkWidget *panel;

	g_return_if_fail(info != NULL);

	if (!info->menu)
		create_applet_menu(info);

	if(!arrow)
		arrow = gdk_cursor_new(GDK_ARROW);

	panel = get_panel_parent(info->widget);
	if(IS_SNAPPED_WIDGET(panel)) {
		SNAPPED_WIDGET(panel)->autohide_inhibit = TRUE;
		snapped_widget_queue_pop_down(SNAPPED_WIDGET(panel));
	}

	gtk_menu_popup(GTK_MENU(info->menu), NULL, NULL, applet_menu_position,
		       GINT_TO_POINTER(applet_id), 0/*3*/, time(NULL));
	gtk_grab_add(info->menu);
	gdk_pointer_grab(info->menu->window,
			 TRUE,
			 APPLET_EVENT_MASK,
			 NULL,
			 arrow,
			 GDK_CURRENT_TIME);
}

PanelOrientType
applet_get_panel_orient(int applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);
	PanelWidget *panel;

	g_return_val_if_fail(info != NULL,ORIENT_UP);

	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);

	g_return_val_if_fail(panel != NULL,ORIENT_UP);

	return get_applet_orient(panel);
}


int
applet_get_panel(int applet_id)
{
	int panel;
	GList *list;
	AppletInfo *info = get_applet_info(applet_id);
	gpointer p;

	g_return_val_if_fail(info != NULL,-1);

	p = gtk_object_get_data(GTK_OBJECT(info->widget),
				PANEL_APPLET_PARENT_KEY);

	for(panel=0,list=panels;list!=NULL;list=g_list_next(list),panel++)
		if(list->data == p)
			return panel;
	return -1;
}

void
applet_abort_id(int applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);

	g_return_if_fail(info != NULL);

	/*only reserved spots can be canceled, if an applet
	  wants to chance a pending applet it needs to first
	  user reserve spot to obtain id and make it EXTERN_RESERVED*/
	if(info->type != APPLET_EXTERN_RESERVED)
		return;

	panel_clean_applet(applet_id);
}


int
applet_get_pos(int applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);
	AppletData *ad;

	g_return_val_if_fail(info != NULL,-1);

	ad = gtk_object_get_data(GTK_OBJECT(info->widget),
				 PANEL_APPLET_DATA);
	if(!ad)
		return -1;
	return ad->pos;
}

void
applet_drag_start(int applet_id)
{
	PanelWidget *panel;
	AppletInfo *info = get_applet_info(applet_id);

	g_return_if_fail(info != NULL);

	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);
	g_return_if_fail(panel!=NULL);

	panel_widget_applet_drag_start(panel,info->widget);
	panel_widget_applet_drag_end(panel);
	panel_widget_applet_drag_start(panel,info->widget);
	panel_widget_applet_move_use_idle(panel);
}

void
applet_drag_stop(int applet_id)
{
	PanelWidget *panel;
	AppletInfo *info = get_applet_info(applet_id);

	g_return_if_fail(info != NULL);

	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);
	g_return_if_fail(panel!=NULL);

	panel_widget_applet_drag_end(panel);
}

static int
compare_params(const char *p1,const char *p2)
{
	if(!p1) {
		if(!p2 || *p2=='\0')
			return TRUE;
		else
			return FALSE;
	} else if(!p2) {
		if(!p1 || *p1=='\0')
			return TRUE;
		else
			return FALSE;
	}
	return (strcmp(p1,p2)==0);
}

int
applet_request_id (const char *path, const char *param,
		   int dorestart, char **cfgpath,
		   char **globcfgpath, guint32 * winid)
{
	AppletInfo *info;
	AppletChild *child;
	int i;

	for(info=(AppletInfo *)applets->data,i=0;i<applet_count;i++,info++) {
		if(info && info->type == APPLET_EXTERN_PENDING &&
		   strcmp(info->path,path)==0 &&
		   compare_params(param,info->params)) {
			/*we started this and already reserved a spot
			  for it, including the socket widget*/
			*cfgpath = info->cfg;
			info->cfg = NULL;
			*globcfgpath = g_strdup(old_panel_cfg_path);
			info->type = APPLET_EXTERN_RESERVED;
			*winid=GDK_WINDOW_XWINDOW(info->applet_widget->window);
			if(!dorestart && !mulapp_is_in_list(path))
				mulapp_add_to_list(path);

			return i;
		}
	}

	*winid = reserve_applet_spot (EXTERN_ID, path, param, panels->data, 0,
				      NULL, APPLET_EXTERN_RESERVED);
	if(*winid == 0) {
		*globcfgpath = NULL;
		*cfgpath = NULL;
		return -1;
	}
	*cfgpath = g_copy_strings(old_panel_cfg_path,"Applet_Dummy/",NULL);
	*globcfgpath = g_strdup(old_panel_cfg_path);

	info = get_applet_info(applet_count-1);
	if(!dorestart && !mulapp_is_in_list(path))
		mulapp_add_to_list(path);

	/*add to list of children, we haven't started this one so make
	  pid -1*/
	child = g_new(AppletChild,1);
	child->pid = -1;
	child->applet_id = i;
	children = g_list_prepend(children,child);

	return i;
}

void
applet_register (const char * ior, int applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);
	PanelWidget *panel;

	/*start the next applet in queue*/
	exec_queue_done(applet_id);

	g_return_if_fail(info != NULL);

 	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);
	g_return_if_fail(panel!=NULL);

	/*no longer pending*/
	info->type = APPLET_EXTERN;

	/*set the ior*/
	g_free(info->id_str);
	info->id_str = g_strdup(ior);

	orientation_change(applet_id,panel);
	back_change(applet_id,panel);
	send_applet_tooltips_state(info->id_str,
				   global_config.tooltips_enabled);

	mulapp_add_ior_and_free_queue(info->path, info->id_str);
}

/*note that type should be APPLET_EXTERN_RESERVED or APPLET_EXTERN_PENDING
  only*/
guint32
reserve_applet_spot (const char *id_str, const char *path, const char *param,
		     PanelWidget *panel, int pos, char *cfgpath,
		     AppletType type)
{
	GtkWidget *socket;

	socket = gtk_socket_new();

	g_return_val_if_fail(socket!=NULL,0);

	gtk_widget_show (socket);
	
	/*we save the ior in the id field of the appletinfo and the 
	  path in the path field*/
	if(!register_toy(socket,NULL,NULL,
			 g_strdup(id_str),g_strdup(path),
			 g_strdup(param), pos,panel,cfgpath, type)) {
		g_warning("Couldn't add applet");
		return 0;
	}

	return GDK_WINDOW_XWINDOW(socket->window);
}

static void
panel_add_main_menu(GtkWidget *w, gpointer data)
{
	PanelWidget *panel = get_def_panel_widget(data);

	load_applet(MENU_ID,NULL,NULL,0,0,NULL,NULL,
		    PANEL_UNKNOWN_APPLET_POSITION,
		    panel,NULL);
}	

GtkWidget *
create_panel_root_menu(GtkWidget *panel)
{
	GtkWidget *menuitem;
	GtkWidget *panel_menu;

	panel_menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("This panel properties..."));
	gtk_signal_connect_object(GTK_OBJECT(menuitem), "activate",
				  GTK_SIGNAL_FUNC(panel_config),
				  GTK_OBJECT(panel));
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new_with_label(_("Global properties..."));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(panel_config_global),
			   NULL);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new_with_label(_("Remove this panel"));
	gtk_signal_connect_object(GTK_OBJECT(menuitem), "activate",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy),
				  GTK_OBJECT(panel));
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);
	gtk_object_set_data(GTK_OBJECT(panel),"remove_item",menuitem);

	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new_with_label(_("Add main menu applet"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(panel_add_main_menu),
			   panel);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	/*FIXME: fix GTK so we can do this safely!
	menuitem = gtk_menu_item_new_with_label(_("Main menu"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), root_menu);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);
	*/

	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);
	
	menuitem = gtk_menu_item_new_with_label(_("Log out"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(panel_quit),
			   NULL);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	return panel_menu;
}


void
applet_set_tooltip(int applet_id, const char *tooltip)
{
	AppletInfo *info = get_applet_info(applet_id);
	g_return_if_fail(info != NULL);

	gtk_tooltips_set_tip (panel_tooltips,info->widget,tooltip,NULL);
}

static int
applet_destroy(GtkWidget *w, gpointer data)
{
	int applet_id = GPOINTER_TO_INT(data);
	AppletInfo *info = get_applet_info(applet_id);

	g_return_val_if_fail(info!=NULL,FALSE);

	info->widget = NULL;

	panel_clean_applet(applet_id);

	return FALSE;
}

int
register_toy(GtkWidget *applet,
	     GtkWidget *assoc,
	     gpointer data,
	     char *id_str,
	     char *path,
	     char *params,
	     int pos,
	     PanelWidget *panel,
	     char *cfgpath,
	     AppletType type)
{
	GtkWidget     *eventbox;
	AppletInfo    info;
	int           bind_lower;
	
	g_return_val_if_fail(applet != NULL, FALSE);
	g_return_val_if_fail(id_str != NULL, FALSE);

	/* We wrap the applet in a GtkEventBox so that we can capture
	   events over it */
	eventbox = gtk_event_box_new();
	gtk_widget_set_events(eventbox, (gtk_widget_get_events(eventbox) |
					 APPLET_EVENT_MASK) &
			      ~( GDK_POINTER_MOTION_MASK |
				 GDK_POINTER_MOTION_HINT_MASK));
	gtk_container_add(GTK_CONTAINER(eventbox), applet);

	info.applet_id = applet_count;
	info.type = type;
	info.widget = eventbox;
	info.applet_widget = applet;
	info.assoc = assoc;
	info.menu = NULL;
	info.data = data;
	info.id_str = g_strdup(id_str);
	if(path)
		info.path = g_strdup(path);
	else
		info.path = NULL;
	if(params)
		info.params = g_strdup(params);
	else
		info.params = NULL;
	if(cfgpath)
		info.cfg = g_strdup(cfgpath);
	else
		info.cfg = NULL;
	info.user_menu = NULL;

	gtk_object_set_user_data(GTK_OBJECT(eventbox),GINT_TO_POINTER(applet_count));


	if(type == APPLET_DRAWER) {
		PanelWidget *assoc_panel = PANEL_WIDGET(DRAWER_WIDGET(assoc)->panel);

		gtk_object_set_data(GTK_OBJECT(eventbox),
				    PANEL_APPLET_ASSOC_PANEL_KEY,assoc_panel);
		assoc_panel->master_widget = eventbox;
	}
	gtk_object_set_data(GTK_OBJECT(eventbox),
			    PANEL_APPLET_FORBIDDEN_PANELS,NULL);
		
	if(pos==PANEL_UNKNOWN_APPLET_POSITION)
		pos = 0;

	/*add to the array of applets*/
	applets = g_array_append_val(applets,AppletInfo,info);
	applet_count++;

	/*for a menu we don't bind other events except for the
	  eventbox, the menu itself takes care of passing the relevant
	  ones thrugh*/
	bind_lower = (type == APPLET_MENU?FALSE:TRUE);
	if(panel_widget_add_full(panel, eventbox, pos, bind_lower)==-1) {
		GList *list;
		for(list = panels; list != NULL; list = g_list_next(list))
			if(panel_widget_add_full(panel, eventbox, pos, bind_lower)!=-1)
				break;
		if(!list) {
			/*can't put it anywhere, clean up*/
			AppletInfo *inf = get_applet_info(applet_count-1);
			gtk_widget_unref(eventbox);
			inf->widget = NULL;
			inf->applet_widget = NULL;
			panel_clean_applet(applet_count-1);
			g_warning("Can't find an empty spot");
			return FALSE;
		}
		panel = PANEL_WIDGET(list->data);
	}

	gtk_signal_connect(GTK_OBJECT(eventbox),
			   "button_press_event",
			   GTK_SIGNAL_FUNC(applet_button_press),
			   GINT_TO_POINTER(applet_count-1));

	info.destroy_callback = gtk_signal_connect(GTK_OBJECT(eventbox),
			   			   "destroy",
			   			   GTK_SIGNAL_FUNC(
			   			   	applet_destroy),
			   			   GINT_TO_POINTER(applet_count-1));

	gtk_widget_show_all(eventbox);

	/*gtk_signal_connect (GTK_OBJECT (eventbox), 
			    "drag_request_event",
			    GTK_SIGNAL_FUNC(panel_dnd_drag_request),
			    info);

	gtk_widget_dnd_drag_set (GTK_WIDGET(eventbox), TRUE,
				 applet_drag_types, 1);*/

	orientation_change(applet_count-1,panel);

	return TRUE;
}
