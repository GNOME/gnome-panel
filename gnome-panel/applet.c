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
#include "gnome-panel.h"

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

extern GSList *panels;

GSList *applets = NULL;
GSList *applets_last = NULL;
int applet_count = 0;

/*config sync stuff*/
extern int config_sync_timeout;
extern GSList *applets_to_sync;
extern int panels_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

extern GtkTooltips *panel_tooltips;

static void
move_applet_callback(GtkWidget *widget, AppletInfo *info)
{
	PanelWidget    *panel;

	g_return_if_fail(info != NULL);

	panel = PANEL_WIDGET(info->widget->parent);
	g_return_if_fail(panel!=NULL);

	panel_widget_applet_drag_start(panel,info->widget);
}

/*destroy widgets and call the above cleanup function*/
void
panel_clean_applet(AppletInfo *info)
{
	g_return_if_fail(info != NULL);

	if(info->widget)
		gtk_widget_destroy(info->widget);
}

static void
remove_applet_callback(GtkWidget *widget, AppletInfo *info)
{
	panel_clean_applet(info);
}

static void
applet_callback_callback(GtkWidget *widget, gpointer data)
{
	AppletUserMenu *menu = data;

	g_return_if_fail(menu->info != NULL);

	switch(menu->info->type) {
	case APPLET_EXTERN:
		{
			CORBA_Environment ev;
			Extern *ext = menu->info->data;
			g_assert(ext);
			g_assert(ext->applet);

			CORBA_exception_init(&ev);
			GNOME_Applet_do_callback(ext->applet, menu->name, &ev);
			if(ev._major)
				panel_clean_applet(ext->info);
			CORBA_exception_free(&ev);
			break;
		}
	case APPLET_LAUNCHER:
		if(strcmp(menu->name,"properties")==0)
			launcher_properties(menu->info->data);
		break;
	case APPLET_DRAWER: 
		if(strcmp(menu->name,"properties")==0) {
			Drawer *drawer = menu->info->data;
			g_assert(drawer);
			panel_config(drawer->drawer);
		}
		break;
      /*
	case APPLET_SWALLOW: 
		if(strcmp(menu->name,"properties")==0) {
			Swallow *swallow = info->data;
			g_assert(swallow);
			swallow_properties(swallow); // doesn't exist yet
		} 
		break; 
      */
	case APPLET_MENU:
		if(strcmp(menu->name,"properties")==0)
			menu_properties(menu->info->data);
		break;
	default: break;
	}
}

static void
applet_menu_deactivate(GtkWidget *w, AppletInfo *info)
{
	GtkWidget *panel = get_panel_parent(info->widget);
	info->menu_age = 0;
	
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
applet_add_callback(AppletInfo *info,
		    char *callback_name,
		    char *stock_item,
		    char *menuitem_text)
{
	AppletUserMenu *menu;

	g_return_if_fail(info != NULL);
	
	if((menu=applet_get_callback(info->user_menu,callback_name))==NULL) {
		menu = g_new(AppletUserMenu,1);
		menu->name = g_strdup(callback_name);
		menu->stock_item = g_strdup(stock_item);
		menu->text = g_strdup(menuitem_text);
		menu->info = info;
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
		gtk_widget_destroy(info->menu);
		info->menu=NULL;
		info->menu_age = 0;
	}
}

void
applet_remove_callback(AppletInfo *info, char *callback_name)
{
	AppletUserMenu *menu;

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
		gtk_widget_destroy(info->menu);
		info->menu=NULL;
		info->menu_age = 0;
	}
}

static void
add_to_submenus(AppletInfo *info,
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
		s_menu = g_new(AppletUserMenu,1);
		s_menu->name = g_strdup(t);
		s_menu->text = g_strdup(_("???"));
		s_menu->stock_item = NULL;
		s_menu->info = info;
		s_menu->menuitem = NULL;
		s_menu->submenu = NULL;
		info->user_menu = g_list_append(info->user_menu,s_menu);
		user_menu = info->user_menu;
	}
	
	if(!s_menu->submenu)
		s_menu->submenu = gtk_menu_new();
	
	add_to_submenus(info,t,p,menu,s_menu->submenu,user_menu);
	
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
			   info);
	gtk_menu_append(GTK_MENU(info->menu), menuitem);
	
	menuitem = gtk_menu_item_new();
	setup_menuitem(menuitem,NULL,_("Move applet"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) move_applet_callback,
			   info);
	gtk_menu_append(GTK_MENU(info->menu), menuitem);
	
	if(user_menu) {
		menuitem = gtk_menu_item_new();
		gtk_menu_append(GTK_MENU(info->menu), menuitem);
		gtk_widget_show(menuitem);
	}

	for(;user_menu!=NULL;user_menu = g_list_next(user_menu)) {
		AppletUserMenu *menu=user_menu->data;
		add_to_submenus(info,"",menu->name,menu,info->menu,
				info->user_menu);
	}

	/*connect the deactivate signal, so that we can "re-allow" autohide
	  when the menu is deactivated*/
	gtk_signal_connect(GTK_OBJECT(info->menu),"deactivate",
			   GTK_SIGNAL_FUNC(applet_menu_deactivate),
			   info);
}

void
show_applet_menu(AppletInfo *info, GdkEventButton *event)
{
	GtkWidget *panel;

	g_return_if_fail(info!=NULL);

	panel = get_panel_parent(info->widget);

	if (!info->menu)
		create_applet_menu(info);

	if(IS_SNAPPED_WIDGET(panel)) {
		SNAPPED_WIDGET(panel)->autohide_inhibit = TRUE;
		snapped_widget_queue_pop_down(SNAPPED_WIDGET(panel));
	}
	info->menu_age = 0;
	gtk_menu_popup(GTK_MENU(info->menu), NULL, NULL, applet_menu_position,
		       info, event->button, event->time);
}



static int
applet_button_press (GtkWidget *widget,GdkEventButton *event, AppletInfo *info)
{
	if(event->button==3) {
		if(!panel_applet_in_drag) {
			if(info->type == APPLET_SWALLOW) {
				Swallow *swallow = info->data;
				GtkWidget *handle_box = swallow->handle_box;
				if(!GTK_HANDLE_BOX(handle_box)->child_detached)
					show_applet_menu(info, event);
			} else
				show_applet_menu(info, event);
		}
	}
	/*stop all button press events here so that they don't get to the
	  panel*/
	return TRUE;
}

static void
applet_destroy(GtkWidget *w, AppletInfo *info)
{
	AppletType type;
	
	g_return_if_fail(info != NULL);

	info->widget = NULL;

	if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		g_assert(drawer);
		if(drawer->drawer) {
			GtkWidget *dw = drawer->drawer;
			drawer->drawer = NULL;
			PANEL_WIDGET(DRAWER_WIDGET(dw)->panel)->master_widget = NULL;
			gtk_widget_destroy(dw);
		}
	}
	if(info->menu) {
		gtk_widget_destroy(info->menu);
		info->menu = NULL;
		info->menu_age = 0;
	}

	info->type = APPLET_EMPTY;

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
}

int
register_toy(GtkWidget *applet,
	     gpointer data,
	     PanelWidget *panel,
	     int pos,
	     AppletType type)
{
	AppletInfo *info;
	
	g_return_val_if_fail(applet != NULL, FALSE);
	g_return_val_if_fail(panel != NULL, FALSE);

	if(!GTK_WIDGET_NO_WINDOW(applet))
		gtk_widget_set_events(applet, (gtk_widget_get_events(applet) |
					       APPLET_EVENT_MASK) &
				      ~( GDK_POINTER_MOTION_MASK |
					 GDK_POINTER_MOTION_HINT_MASK));

	info = g_new(AppletInfo,1);
	info->applet_id = applet_count;
	info->type = type;
	info->widget = applet;
	info->menu = NULL;
	info->menu_age = 0;
	info->data = data;
	info->user_menu = NULL;

	/* Since we will be taking care of refcounting by ourselves,
	 * sink the object.
	 */

	gtk_widget_ref (info->widget);
	gtk_object_sink (GTK_OBJECT (info->widget));

	gtk_object_set_data(GTK_OBJECT(applet),"applet_info",info);

	if(type == APPLET_DRAWER) {
		Drawer *drawer = data;
		PanelWidget *assoc_panel = PANEL_WIDGET(DRAWER_WIDGET(drawer->drawer)->panel);

		gtk_object_set_data(GTK_OBJECT(applet),
				    PANEL_APPLET_ASSOC_PANEL_KEY,assoc_panel);
		assoc_panel->master_widget = applet;
	}
	gtk_object_set_data(GTK_OBJECT(applet),
			    PANEL_APPLET_FORBIDDEN_PANELS,NULL);
		
	if(!applets) {
		applets_last = applets = g_slist_append(NULL,info);
	} else {
		applets_last = g_slist_append(applets_last,info);
		applets_last = applets_last->next;
	}
	applet_count++;

	/*we will need to save this applet's config now*/
	if(g_slist_find(applets_to_sync, info)==NULL)
		applets_to_sync = g_slist_prepend(applets_to_sync,info);

	if(panel_widget_add_full(panel, applet, pos, TRUE)==-1) {
		GSList *list;
		for(list = panels; list != NULL; list = g_slist_next(list))
			if(panel_widget_add_full(panel, applet, pos, TRUE)!=-1)
				break;
		if(!list) {
			/*can't put it anywhere, clean up*/
			gtk_widget_destroy(applet);
			info->widget = NULL;
			panel_clean_applet(info);
			g_warning("Can't find an empty spot");
			return FALSE;
		}
		panel = PANEL_WIDGET(list->data);
	}

	if(IS_BUTTON_WIDGET (applet) || !GTK_WIDGET_NO_WINDOW(applet))
		gtk_signal_connect(GTK_OBJECT(applet),
				   "button_press_event",
				   GTK_SIGNAL_FUNC(applet_button_press),
				   info);

	gtk_signal_connect(GTK_OBJECT(applet), "destroy",
			   GTK_SIGNAL_FUNC(applet_destroy),
			   info);

	gtk_widget_show_all(applet);

	/*gtk_signal_connect (GTK_OBJECT (applet), 
			    "drag_request_event",
			    GTK_SIGNAL_FUNC(panel_dnd_drag_request),
			    info);

	gtk_widget_dnd_drag_set (GTK_WIDGET(applet), TRUE,
				 applet_drag_types, 1);*/

	orientation_change(info,panel);
	back_change(info,panel);

	return TRUE;
}
