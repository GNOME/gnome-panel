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
#include "gdkextra.h"
#include "panel.h"
#include "menu.h"
#include "mico-glue.h"
#include "panel_config.h"
#include "panel_config_global.h"
#include <gdk/gdkx.h>

/*FIXME: BAD HACK!!*/
#include <gdk/gdkprivate.h>

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

/*extern GList *panels;*/
extern GArray *applets;
extern gint applet_count;

extern GtkTooltips *panel_tooltips;
extern gint tooltips_enabled;

extern GnomeClient *client;

extern GtkWidget *root_menu;
extern GList *small_icons;

extern GlobalConfig global_config;

extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

extern gint main_menu_count;

void
apply_global_config(void)
{
	panel_widget_change_global(global_config.explicit_hide_step_size,
				   global_config.auto_hide_step_size,
				   global_config.minimized_size,
				   global_config.minimize_delay,
				   global_config.movement_type);
	if(global_config.tooltips_enabled)
		gtk_tooltips_enable(panel_tooltips);
	else
		gtk_tooltips_disable(panel_tooltips);
	g_list_foreach(small_icons,set_show_small_icons,NULL);
	
}

static gint
find_panel(PanelWidget *panel)
{
	gint i;
	GList *list;

	for(i=0,list=panels;list!=NULL;list=g_list_next(list),i++)
		if(list->data == panel)
			return i; 
	return -1;
}

static void
save_applet_configuration(AppletInfo *info, gint *num)
{
	char          *fullpath;
	char           path[256];
	int            pos;
	int            panel;
	GList         *list;
	PanelWidget   *p;
	AppletData    *ad;

	/*obviously no need for saving*/
	if(info->type==APPLET_EXTERN_PENDING ||
	   info->type==APPLET_EXTERN_RESERVED ||
	   info->type==APPLET_EMPTY)
		return;

	p = gtk_object_get_data(GTK_OBJECT(info->widget),
				PANEL_APPLET_PARENT_KEY);
	ad = gtk_object_get_data(GTK_OBJECT(info->widget),PANEL_APPLET_DATA);

	pos = -1;
	for(panel=0,list=panels;list!=NULL;list=g_list_next(list),panel++)
	    	if(list->data == p) {
	    		pos = ad->pos;
	    		break;
	    	}

	/*not found*/
	if(pos == -1)
		return;

	g_snprintf(path,256, "%sApplet_%d/", panel_cfg_path, (*num)++);

	gnome_config_clean_section(path);

	if(info->type==APPLET_EXTERN) {
		/*sync before the applet does it's stuff*/
		gnome_config_sync();
		/*I think this should be done at sync and also that there
		  should be some flocking ... but this works for now*/
		gnome_config_drop_all();

		/*have the applet do it's own session saving*/
		if(send_applet_session_save(info->id_str,info->applet_id,path,
					    panel_cfg_path)) {

			fullpath = g_copy_strings(path,"id",NULL);
			gnome_config_set_string(fullpath, EXTERN_ID);
			g_free(fullpath);

			fullpath = g_copy_strings(path,"position",NULL);
			gnome_config_set_int(fullpath, pos);
			g_free(fullpath);

			fullpath = g_copy_strings(path,"panel",NULL);
			gnome_config_set_int(fullpath, panel);
			g_free(fullpath);

			fullpath = g_copy_strings(path,"parameters",NULL);
			gnome_config_set_string(fullpath, info->params);
			g_free(fullpath);
		} else
			(*num)--;
	} else {
		fullpath = g_copy_strings(path,"id",NULL);
		gnome_config_set_string(fullpath, info->id_str);
		g_free(fullpath);

		fullpath = g_copy_strings(path,"position",NULL);
		gnome_config_set_int(fullpath, pos);
		g_free(fullpath);

		fullpath = g_copy_strings(path,"panel",NULL);
		gnome_config_set_int(fullpath, panel);
		g_free(fullpath);

		fullpath = g_copy_strings(path,"parameters",NULL);
		if(strcmp(info->id_str,DRAWER_ID) == 0) {
			int i;

			i = find_panel(PANEL_WIDGET(info->assoc));
			if(i>=0)
				gnome_config_set_int(fullpath,i);
			else
				g_warning("Drawer not associated with applet!");
		} else {
			if(info->params)
				gnome_config_set_string(fullpath, info->params);
		}
		g_free(fullpath);
	}
}

static void
save_panel_configuration(gpointer data, gpointer user_data)
{
	char          *fullpath;
	char           path[256];
	int            x,y;
	int           *num = user_data;
	PanelWidget   *panel = data;

	g_snprintf(path,256, "%sPanel_%d/", panel_cfg_path, (*num)++);

	gnome_config_push_prefix (path);
	
	gnome_config_set_int("orient",panel->orient);
	gnome_config_set_int("snapped", panel->snapped);
	gnome_config_set_int("mode", panel->mode);
	gnome_config_set_int("state", panel->state);
	gnome_config_set_int("size", panel->size);

	if (panel->back_pixmap)
		gnome_config_set_string("backpixmap", panel->back_pixmap ? panel->back_pixmap : "");

	/*FIXME: this should be allocation.[xy] but those don't work!!!
	  probably a gtk bug*/
	gdk_window_get_position(GTK_WIDGET(panel)->window,&x,&y);
	
	gnome_config_set_int("position_x",x);
	gnome_config_set_int("position_y",y);
	gnome_config_set_int("drawer_drop_zone_pos",
			     panel->drawer_drop_zone_pos);
	
	gnome_config_pop_prefix ();
}

static void
destroy_widget_list(gpointer data, gpointer user_data)
{
	gtk_widget_destroy(GTK_WIDGET(data));
}

/* This is called when the session manager requests a shutdown.  It
   can also be run directly when we don't detect a session manager.
   We assume no interaction is done by the applets.  And we ignore the
   other arguments for now.  Yes, this is lame.  */
/* update: some SM stuff implemented but we still ignore most of the
   arguments now*/
gint
panel_session_save (GnomeClient *client,
		    gint phase,
		    GnomeSaveStyle save_style,
		    gint is_shutdown,
		    GnomeInteractStyle interact_style,
		    gint is_fast,
		    gpointer client_data)
{
	gint num;
	char *buf;
	char *session_id;
	gint i;

	session_id = gnome_client_get_id (client);
	if(session_id) {
		char *new_args[3];

		g_free(panel_cfg_path);
		panel_cfg_path = g_copy_strings("/panel-Session-",session_id,
						"/",NULL);

		new_args[0] = (char *) client_data;
		new_args[1] = "--discard-session";
		new_args[2] = session_id;
		gnome_client_set_discard_command (client, 3, new_args);
	}

	gnome_config_clean_file(panel_cfg_path);

	puts("S 1");
	for(num=1,i=0;i<applet_count;i++)
		save_applet_configuration(&g_array_index(applets,AppletInfo,i),
					  &num);
	puts("S 2");

	buf = g_copy_strings(panel_cfg_path,"Config/",NULL);
	gnome_config_push_prefix (buf);
	g_free(buf);

	gnome_config_set_int ("applet_count", num-1);
	num = 1;
	puts("S 3");
	g_list_foreach(panels, save_panel_configuration,&num);
	puts("S 4");
	gnome_config_set_int("panel_count",num-1);

	/*global options*/
	gnome_config_set_int("auto_hide_step_size",
			     global_config.auto_hide_step_size);
	gnome_config_set_int("explicit_hide_step_size",
			     global_config.explicit_hide_step_size);
	gnome_config_set_int("minimized_size", global_config.minimized_size);
	gnome_config_set_int("minimize_delay", global_config.minimize_delay);
	gnome_config_set_bool("tooltips_enabled",
			      global_config.tooltips_enabled);
	gnome_config_set_bool("show_small_icons",
			      global_config.show_small_icons);
	gnome_config_set_bool("prompt_for_logout",
			      global_config.prompt_for_logout);

	gnome_config_pop_prefix ();
	gnome_config_sync();

	if(is_shutdown) {
		GList *list;
		AppletInfo *info;

		/*don't catch these any more*/
		signal(SIGCHLD, SIG_DFL);

		puts("1");

		/*don't catch these either*/
		for(i=0,info=(AppletInfo *)applets->data;i<applet_count;
		    i++,info++) {
		    	if(info->widget)
				gtk_signal_disconnect(GTK_OBJECT(info->widget),
						      info->destroy_callback);
			/*FIXME: VERY BAD HACK*/
			if(info->widget && GTK_IS_SOCKET(info->applet_widget))
				((GdkWindowPrivate *)
					GTK_SOCKET(info->applet_widget)->
						plug_window)->colormap = NULL;
		}

		puts("2");

		g_list_foreach(panels,destroy_widget_list,NULL);

		puts("3");

		/*clean up corba stuff*/
		panel_corba_clean_up();

		gtk_exit (0);
	}

	/* Always successful.  */
	return TRUE;
}

static gint
panel_really_logout(GtkWidget *w, gint button, gpointer data)
{
	GtkWidget **box=data;

	if(button!=0)
		gnome_dialog_close(GNOME_DIALOG(w));
	else {
		if (! GNOME_CLIENT_CONNECTED (client)) {
			panel_session_save (client, 1, GNOME_SAVE_BOTH, 1,
					    GNOME_INTERACT_NONE, 0, NULL);
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
	global_config.prompt_for_logout = GTK_TOGGLE_BUTTON(w)->active == TRUE;
}

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
	AppletInfo     *info = get_applet_info(PTOI(data));
	PanelWidget    *panel;

	g_return_if_fail(info != NULL);

	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);
	g_return_if_fail(panel!=NULL);

	panel_widget_applet_drag_start(panel,info->widget);
	panel_widget_applet_move_use_idle(panel);
}

void
panel_clean_applet(gint applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);
	AppletType type;
	PanelWidget *panel;

	g_return_if_fail(info != NULL);

	/*fixes reentrancy problem with this routine*/
	if(info->type == APPLET_EMPTY)
		return;

	type = info->type;
	info->type = APPLET_EMPTY;

	if(info->widget) {
		GtkWidget *w = info->widget;

		info->widget = NULL;

		gtk_signal_disconnect(GTK_OBJECT(w),info->destroy_callback);
		info->destroy_callback = -1;

		panel = gtk_object_get_data(GTK_OBJECT(w),
					    PANEL_APPLET_PARENT_KEY);

		/*FIXME: VERY BAD HACK*/
		if(info->widget && GTK_IS_SOCKET(info->applet_widget))
			((GdkWindowPrivate *)
				GTK_SOCKET(info->applet_widget)->
					plug_window)->colormap = NULL;

		if(panel)
			panel_widget_remove(panel,w);
	}
	info->applet_widget = NULL;
	if(type == APPLET_DRAWER && info->assoc) {
		panels = g_list_remove(panels,info->assoc);
		gtk_widget_destroy(info->assoc);
		info->assoc=NULL;
	}
	if(type == APPLET_MENU && (!info->params ||
				   strcmp(info->params,".")==0))
		main_menu_count--;
	info->assoc=NULL;
	if(info->menu)
		gtk_widget_unref(info->menu);
	info->menu = NULL;
	info->remove_item = NULL;

	if(info->id_str) g_free(info->id_str);
	info->id_str=NULL;
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
		if(umenu->text)
			g_free(umenu->text);
		g_free(umenu);
		info->user_menu = g_list_remove_link(info->user_menu,
						     info->user_menu);
	}
}

static void
remove_applet_callback(GtkWidget *widget, gpointer data)
{
	panel_clean_applet(PTOI(data));
}

static void
applet_callback_callback(GtkWidget *widget, gpointer data)
{
	AppletUserMenu *menu = data;
	AppletInfo *info = get_applet_info(menu->applet_id);

	g_return_if_fail(info != NULL);

	if(info->type == APPLET_EXTERN) {
		send_applet_do_callback(info->id_str,
					info->applet_id,
					menu->name);
	} else if(info->type == APPLET_LAUNCHER) {
		if(strcmp(menu->name,"properties")==0)
			launcher_properties(info->data);
	}
}

static void
create_applet_menu(AppletInfo *info)
{
	GtkWidget *menuitem;
	GtkWidget *applet_menu;
	GList *user_menu = info->user_menu;

	info->menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("Remove from panel"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) remove_applet_callback,
			   ITOP(info->applet_id));
	gtk_menu_append(GTK_MENU(info->menu), menuitem);
	gtk_widget_show(menuitem);
	info->remove_item = menuitem;

	menuitem = gtk_menu_item_new_with_label(_("Move applet"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) move_applet_callback,
			   ITOP(info->applet_id));
	gtk_menu_append(GTK_MENU(info->menu), menuitem);
	gtk_widget_show(menuitem);
	
	if(user_menu) {
		menuitem = gtk_menu_item_new();
		gtk_menu_append(GTK_MENU(info->menu), menuitem);
		gtk_widget_show(menuitem);
	}

	for(;user_menu!=NULL;user_menu = g_list_next(user_menu)) {
		AppletUserMenu *menu=user_menu->data;
		menuitem = gtk_menu_item_new_with_label(menu->text);
		gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
				   (GtkSignalFunc) applet_callback_callback,
				   menu);
		gtk_menu_append(GTK_MENU(info->menu), menuitem);
		gtk_widget_show(menuitem);
	}
}

static void
applet_menu_position (GtkMenu *menu, gint *x, gint *y, gpointer data)
{
	int wx, wy;
	AppletInfo *info = get_applet_info(PTOI(data));
	PanelWidget *panel;

	g_return_if_fail(info != NULL);
	g_return_if_fail(info->widget != NULL);

	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);

	g_return_if_fail(panel != NULL);

	gdk_window_get_origin (info->widget->window, &wx, &wy);

	switch(panel->snapped) {
		case PANEL_DRAWER:
		case PANEL_FREE:
			if(panel->orient==PANEL_VERTICAL) {
				*x = wx + info->widget->allocation.width;
				*y = wy;
				break;
			}
			/*fall through for horizontal*/
		case PANEL_BOTTOM:
			*x = wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
			break;
		case PANEL_TOP:
			*x = wx;
			*y = wy + info->widget->allocation.height;
			break;
		case PANEL_LEFT:
			*x = wx + info->widget->allocation.width;
			*y = wy;
			break;
		case PANEL_RIGHT:
			*x = wx - GTK_WIDGET (menu)->allocation.width;
			*y = wy;
			break;
	}

	if(*x + GTK_WIDGET (menu)->allocation.width > gdk_screen_width())
		*x=gdk_screen_width() - GTK_WIDGET (menu)->allocation.width;
	if(*x < 0) *x =0;

	if(*y + GTK_WIDGET (menu)->allocation.height > gdk_screen_height())
		*y=gdk_screen_height() - GTK_WIDGET (menu)->allocation.height;
	if(*y < 0) *y =0;
}




static void
show_applet_menu(gint applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);

	g_return_if_fail(info!=NULL);

	if (!info->menu)
		create_applet_menu(info);

	if((info->type == APPLET_DRAWER &&
	    panel_widget_get_applet_count(PANEL_WIDGET(info->assoc))>0) ||
	   (info->type == APPLET_MENU &&
	    main_menu_count <= 1 &&
	    (!info->params ||
	     strcmp(info->params,".")==0)))
	   	gtk_widget_set_sensitive(info->remove_item,FALSE);
	else
	   	gtk_widget_set_sensitive(info->remove_item,TRUE);

	gtk_menu_popup(GTK_MENU(info->menu), NULL, NULL, applet_menu_position,
		       ITOP(applet_id), 0/*3*/, time(NULL));
}



static gint
applet_button_press(GtkWidget *widget,GdkEventButton *event, gpointer data)
{
	if(event->button==3) {
		show_applet_menu(PTOI(data));
		return TRUE;
	}
	return FALSE;
}

static void
panel_properties_callback(GtkWidget *widget, gpointer data)
{
	panel_config(PANEL_WIDGET(data));
}

static void
panel_global_properties_callback(GtkWidget *widget, gpointer data)
{
	panel_config_global();
}

static void
panel_log_out_callback(GtkWidget *widget, gpointer data)
{
	panel_quit();
}

void
applet_show_menu(gint applet_id)
{
	static GdkCursor *arrow = NULL;
	AppletInfo *info = get_applet_info(applet_id);

	g_return_if_fail(info != NULL);

	if (!info->menu)
		create_applet_menu(info);

	if(!arrow)
		arrow = gdk_cursor_new(GDK_ARROW);

	gtk_menu_popup(GTK_MENU(info->menu), NULL, NULL, applet_menu_position,
		       ITOP(applet_id), 0/*3*/, time(NULL));
	gtk_grab_add(info->menu);
	gdk_pointer_grab(info->menu->window,
			 TRUE,
			 APPLET_EVENT_MASK,
			 NULL,
			 arrow,
			 GDK_CURRENT_TIME);
}


int
applet_get_panel(gint applet_id)
{
	int pos = -1;
	int panel;
	GList *list;
	AppletInfo *info = get_applet_info(applet_id);
	gpointer p;

	g_return_if_fail(info != NULL);

	p = gtk_object_get_data(GTK_OBJECT(info->widget),
				PANEL_APPLET_PARENT_KEY);

	for(panel=0,list=panels;list!=NULL;list=g_list_next(list),panel++)
		if(list->data == p)
			return panel;
	return -1;
}

void
applet_abort_id(gint applet_id)
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
applet_get_pos(gint applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);
	AppletData *ad;

	g_return_if_fail(info != NULL);

	ad = gtk_object_get_data(GTK_OBJECT(info->widget),
				 PANEL_APPLET_DATA);
	if(!ad)
		return -1;
	return ad->pos;
}

void
applet_drag_start(gint applet_id)
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
applet_drag_stop(gint applet_id)
{
	PanelWidget *panel;
	AppletInfo *info = get_applet_info(applet_id);

	g_return_if_fail(info != NULL);

	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);
	g_return_if_fail(panel!=NULL);

	panel_widget_applet_drag_end(panel);
}

void
applet_add_callback(gint applet_id, char *callback_name, char *menuitem_text)
{
	AppletUserMenu *menu = g_new(AppletUserMenu,1);
	AppletInfo *info = get_applet_info(applet_id);

	g_return_if_fail(info != NULL);

	menu->name = g_strdup(callback_name);
	menu->text = g_strdup(menuitem_text);
	menu->applet_id = applet_id;

	/*make sure the menu is rebuilt*/
	if(info->menu) {
		gtk_widget_unref(info->menu);
		info->menu=NULL;
	}

	info->user_menu = g_list_append(info->user_menu,menu);
}

int
applet_request_id (const char *path, char **cfgpath,
		   char **globcfgpath, guint32 * winid)
{
	GList *list;
	AppletInfo *info;
	int i;

	for(info=(AppletInfo *)applets->data,i=0;i<applet_count;i++,info++) {
		if(info && info->type == APPLET_EXTERN_PENDING &&
		   strcmp(info->params,path)==0) {
			/*we started this and already reserved a spot
			  for it, including the socket widget*/
			*cfgpath = info->cfg;
			info->cfg = NULL;
			*globcfgpath = g_strdup(old_panel_cfg_path);
			info->type = APPLET_EXTERN_RESERVED;
			*winid=GDK_WINDOW_XWINDOW(info->applet_widget->window);
			return i;
		}
	}

	*winid = reserve_applet_spot (EXTERN_ID, path, 0, 0, NULL,
				      APPLET_EXTERN_RESERVED);
	if(*winid == 0) {
		*globcfgpath = NULL;
		*cfgpath = NULL;
		return -1;
	}
	*cfgpath = NULL;
	*globcfgpath = g_strdup(old_panel_cfg_path);
	return i;
}

void
applet_request_glob_cfg (char **globcfgpath)
{
	*globcfgpath = g_strdup(old_panel_cfg_path);
}

void
applet_register (const char * ior, int applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);
	PanelWidget *panel;

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
}

/*note that type should be APPLET_EXTERN_RESERVED or APPLET_EXTERN_PENDING
  only*/
guint32
reserve_applet_spot (const char *id_str, const char *path, int panel,
		     int pos, char *cfgpath, AppletType type)
{
	GtkWidget *socket;

	socket = gtk_socket_new();

	g_return_val_if_fail(socket!=NULL,0);

	gtk_widget_show (socket);
	
	/*we save the ior in the id field of the appletinfo and the 
	  path in the params field*/
	if(!register_toy(socket,NULL,NULL,g_strdup(id_str),g_strdup(path),
		         pos,panel,cfgpath, type)) {
		g_warning("Couldn't add applet");
		return 0;
	}

	return GDK_WINDOW_XWINDOW(socket->window);
}

void
applet_remove_from_panel(gint applet_id)
{
	panel_clean_applet(applet_id);
}

static gint
panel_add_main_menu(GtkWidget *w, gpointer data)
{
	PanelWidget *panel = data;
	gint panel_num = find_panel(panel);

	load_applet(MENU_ID,NULL,PANEL_UNKNOWN_APPLET_POSITION,
		    panel_num!=-1?panel_num:0,NULL);

	return TRUE;
}	


GtkWidget *
create_panel_root_menu(PanelWidget *panel)
{
	GtkWidget *menuitem;
	GtkWidget *panel_menu;

	panel_menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("This panel properties..."));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) panel_properties_callback,
			   panel);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new_with_label(_("Global properties..."));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) panel_global_properties_callback,
			   panel);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new_with_label(_("Add main menu applet"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) panel_add_main_menu,
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
			   (GtkSignalFunc) panel_log_out_callback,
			   NULL);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	return panel_menu;
}


void
applet_set_tooltip(gint applet_id, const char *tooltip)
{
	AppletInfo *info = get_applet_info(applet_id);
	g_return_if_fail(info != NULL);

	gtk_tooltips_set_tip (panel_tooltips,info->widget,tooltip,NULL);
}

#if 0
static gint
panel_dnd_drag_request(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	/*FIXME use applet_id, pointers DO change*/
	AppletInfo *info = data;
	PanelWidget *panel;

	g_return_if_fail(info != NULL);
	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);
	g_return_if_fail(panel!=NULL);

	gtk_widget_dnd_data_set (widget, event, &info->widget,
				 sizeof(GtkWidget *));
	/*gtk_widget_ref(info->widget);
	panel_widget_remove(panel,info->widget);*/

	return TRUE;
}

static char *applet_drag_types[]={"internal/applet-widget-pointer"};
#endif

static gint
applet_destroy(GtkWidget *w, gpointer data)
{
	gint applet_id = PTOI(data);
	AppletInfo *info = get_applet_info(applet_id);

	g_return_if_fail(info!=NULL);

	info->widget = NULL;

	panel_clean_applet(applet_id);

	return FALSE;
}

gint
register_toy(GtkWidget *applet,
	     GtkWidget * assoc,
	     gpointer data,
	     char *id_str,
	     char *params,
	     int pos,
	     int panel,
	     char *cfgpath,
	     AppletType type)
{
	GtkWidget     *eventbox;
	AppletInfo    info;
	PanelWidget   *panelw;
	GList         *list;
	int            i;
	
	g_return_val_if_fail(applet != NULL, FALSE);
	g_return_val_if_fail(id_str != NULL, FALSE);

	list = g_list_nth(panels,panel);

	g_return_val_if_fail(list != NULL, FALSE);

	panelw = PANEL_WIDGET(list->data);

	g_return_val_if_fail(panelw != NULL, FALSE);
	
	/* We wrap the applet in a GtkEventBox so that we can capture
	   events over it */
	eventbox = gtk_event_box_new();
	gtk_widget_set_events(eventbox, gtk_widget_get_events(eventbox) |
			      APPLET_EVENT_MASK);
	gtk_container_add(GTK_CONTAINER(eventbox), applet);

	info.applet_id = applet_count;
	info.type = type;
	info.widget = eventbox;
	info.applet_widget = applet;
	info.assoc = assoc;
	info.menu = NULL;
	info.data = data;
	info.id_str = g_strdup(id_str);
	if(params)
		info.params = g_strdup(params);
	else
		info.params = NULL;
	if(cfgpath)
		info.cfg = g_strdup(cfgpath);
	else
		info.cfg = NULL;
	info.user_menu = NULL;

	gtk_object_set_user_data(GTK_OBJECT(eventbox),ITOP(applet_count));


	if(type == APPLET_DRAWER) {
		gtk_object_set_data(GTK_OBJECT(eventbox),
				    PANEL_APPLET_ASSOC_PANEL_KEY,assoc);
		PANEL_WIDGET(assoc)->master_widget = eventbox;
	} else
		gtk_object_set_data(GTK_OBJECT(eventbox),
				    PANEL_APPLET_ASSOC_PANEL_KEY,NULL);
	gtk_object_set_data(GTK_OBJECT(eventbox),
			    PANEL_APPLET_FORBIDDEN_PANELS,NULL);
		
	if(pos==PANEL_UNKNOWN_APPLET_POSITION)
		pos = 0;
	while(panel_widget_add(panelw, eventbox, pos)==-1) {
		list = g_list_next(list);
		if(!list) {
			gtk_widget_unref(eventbox);
			if(info.cfg)
				g_free(info.cfg);
			if(info.params)
				g_free(info.params);
			g_free(info.id_str);
			g_warning("Can't find an empty spot");
			return FALSE;
		}
		panelw = PANEL_WIDGET(list->data);
	}

	gtk_signal_connect(GTK_OBJECT(eventbox),
			   "button_press_event",
			   GTK_SIGNAL_FUNC(applet_button_press),
			   ITOP(applet_count));

	info.destroy_callback = gtk_signal_connect(GTK_OBJECT(eventbox),
			   			   "destroy",
			   			   GTK_SIGNAL_FUNC(
			   			   	applet_destroy),
			   			   ITOP(applet_count));

	gtk_widget_show(applet);
	gtk_widget_show(eventbox);

	applets = g_array_append_val(applets,AppletInfo,info);
	applet_count++;

	/*gtk_signal_connect (GTK_OBJECT (eventbox), 
			    "drag_request_event",
			    GTK_SIGNAL_FUNC(panel_dnd_drag_request),
			    info);

	gtk_widget_dnd_drag_set (GTK_WIDGET(eventbox), TRUE,
				 applet_drag_types, 1);*/

	orientation_change(applet_count-1,panelw);

	return TRUE;
}
