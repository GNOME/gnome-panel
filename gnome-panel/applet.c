/* Gnome panel: general applet functionality
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

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

GArray *applets = NULL;
int applet_count = 0;

/*config sync stuff*/
extern int config_sync_timeout;
extern GList *applets_to_sync;
extern int panels_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

extern GtkTooltips *panel_tooltips;

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

		panel = gtk_object_get_data(GTK_OBJECT(w),
					    PANEL_APPLET_PARENT_KEY);

		if(panel) {
			gtk_widget_ref(w);
			gtk_container_remove(GTK_CONTAINER(panel),w);
			gtk_widget_destroy(w);
		}
	}
	info->applet_widget = NULL;
	if(type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		g_assert(drawer);
		if(drawer->drawer) {
			GtkWidget *dw = drawer->drawer;
			drawer->drawer = NULL;
			PANEL_WIDGET(DRAWER_WIDGET(dw)->panel)->master_widget = NULL;
			gtk_widget_destroy(dw);
		}
	}
	if(info->menu)
		gtk_widget_unref(info->menu);
	info->menu=NULL;

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
		info->user_menu = my_g_list_pop_first(info->user_menu);
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
		{
			Extern *ext = info->data;
			g_assert(ext);
			send_applet_do_callback(ext->ior,
						info->applet_id,
						menu->name);
			break;
		}
	case APPLET_LAUNCHER:
		if(strcmp(menu->name,"properties")==0)
			launcher_properties(info->data);
		break;
	case APPLET_DRAWER: 
		if(strcmp(menu->name,"properties")==0) {
			Drawer *drawer = info->data;
			g_assert(drawer);
			panel_config(drawer->drawer);
		}
		break;
	case APPLET_MENU:
		if(strcmp(menu->name,"properties")==0)
			menu_properties(info->data);
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
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu->menuitem),
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
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu->menuitem),
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

void
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

void
show_applet_menu(int applet_id, GdkEventButton *event)
{
	AppletInfo *info = get_applet_info(applet_id);
	GtkWidget *panel;

	g_return_if_fail(info!=NULL);

	panel = get_panel_parent(info->widget);

	if (!info->menu)
		create_applet_menu(info);

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
	     gpointer data,
	     PanelWidget *panel,
	     int pos,
	     AppletType type)
{
	GtkWidget     *eventbox;
	AppletInfo    info;
	int           bind_lower;
	
	g_return_val_if_fail(applet != NULL, FALSE);
	g_return_val_if_fail(panel != NULL, FALSE);

	if(/*GTK_WIDGET_NO_WINDOW(applet) || */GTK_IS_SOCKET(applet)) {
		/* We wrap the applet in a GtkEventBox so that we can capture
		   events over it */
		eventbox = gtk_event_box_new();
		gtk_widget_set_events(eventbox, (gtk_widget_get_events(eventbox) |
						 APPLET_EVENT_MASK) &
				      ~( GDK_POINTER_MOTION_MASK |
					 GDK_POINTER_MOTION_HINT_MASK));
		gtk_container_add(GTK_CONTAINER(eventbox), applet);
	} else {
		if(!GTK_WIDGET_NO_WINDOW(applet))
			gtk_widget_set_events(applet, (gtk_widget_get_events(applet) |
						       APPLET_EVENT_MASK) &
					      ~( GDK_POINTER_MOTION_MASK |
						 GDK_POINTER_MOTION_HINT_MASK));
		eventbox = applet;
	}

	info.applet_id = applet_count;
	info.type = type;
	info.widget = eventbox;
	info.applet_widget = applet;
	info.menu = NULL;
	info.data = data;
	info.user_menu = NULL;

	gtk_object_set_data(GTK_OBJECT(eventbox),"applet_id",
			    GINT_TO_POINTER(applet_count));

	if(type == APPLET_DRAWER) {
		Drawer *drawer = data;
		PanelWidget *assoc_panel = PANEL_WIDGET(DRAWER_WIDGET(drawer->drawer)->panel);

		gtk_object_set_data(GTK_OBJECT(eventbox),
				    PANEL_APPLET_ASSOC_PANEL_KEY,assoc_panel);
		assoc_panel->master_widget = eventbox;
	}
	gtk_object_set_data(GTK_OBJECT(eventbox),
			    PANEL_APPLET_FORBIDDEN_PANELS,NULL);
		
	/*add to the array of applets*/
	applets = g_array_append_val(applets,AppletInfo,info);
	applet_count++;

	/*we will need to save this applet's config now*/
	if(g_list_find(applets_to_sync, GINT_TO_POINTER(applet_count-1))==NULL)
		applets_to_sync =
			g_list_prepend(applets_to_sync,
				       GINT_TO_POINTER(applet_count-1));

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

	if(!GTK_WIDGET_NO_WINDOW(applet))
		gtk_signal_connect(GTK_OBJECT(eventbox),
				   "button_press_event",
				   GTK_SIGNAL_FUNC(applet_button_press),
				   GINT_TO_POINTER(applet_count-1));

	gtk_signal_connect(GTK_OBJECT(eventbox),
			   "destroy",
			   GTK_SIGNAL_FUNC(applet_destroy),
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
