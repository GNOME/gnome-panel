/* Gnome panel: panel functionality
 * (C) 1997 the Free Software Foundation
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 */

#ifdef HAVE_LIBINTL
#include <libintl.h>
#endif
#include <string.h>
#include "gnome.h"
#include "applet_files.h"
#include "gdkextra.h"
#include "panel_cmds.h"
#include "applet_cmds.h"
#include "panel.h"
#include "panel_config.h"

#ifdef HAVE_LIBINTL
#define _(String) gettext(String)
#else
#define _(String) (String)
#endif


#define APPLET_CMD_FUNC "panel_applet_cmd_func"
#define APPLET_FLAGS    "panel_applet_flags"

#define DEFAULT_STEP_SIZE 6
#define DEFAULT_DELAY     0
#define DEFAULT_HEIGHT    DEFAULT_APPLET_HEIGHT

/* amount of time in ms. to wait before lowering panel */
#define DEFAULT_MINIMIZE_DELAY 300

/* number of pixels it'll stick up from the bottom when using
 * PANEL_GETS_HIDDEN */
#define DEFAULT_MINIMIZED_SIZE 6

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)


static GtkWidget *applet_menu;
static GtkWidget *applet_menu_remove_item;
static GtkWidget *applet_menu_prop_separator;
static GtkWidget *applet_menu_prop_item;

static GtkWidget *panel_menu;


static GdkCursor *fleur_cursor;

static menu_count=0; /*how many "menu" applets we have ....*/
			/*FIXME: this should only count "main" menus!*/

Panel *the_panel;

static GtkTooltips *panel_tooltips;

/* some prototypes */
static void properties(void);



static void
move_window(GtkWindow *window, int x, int y)
{
	GtkWidget *widget;

	widget = GTK_WIDGET(window);
	
	gdk_window_set_hints(widget->window, x, y, 0, 0, 0, 0, GDK_HINT_POS);
	gdk_window_move(widget->window, x, y);
	gtk_widget_draw(widget, NULL); /* FIXME: this should draw only the newly exposed area! */
}


static void
move_horiz(int src_x, int dest_x)
{
	int x;

	if (the_panel->step_size != 0)
		if (src_x < dest_x)
			for (x = src_x; x < dest_x; x += the_panel->step_size) {
				move_window(GTK_WINDOW(the_panel->window), x,
					the_panel->window->allocation.y); 
				/* FIXME: do delay */
			}
		else
			for (x = src_x; x > dest_x; x -= the_panel->step_size) {
				move_window(GTK_WINDOW(the_panel->window), x,
					the_panel->window->allocation.y); 
				/* FIXME: do delay */
			}
	
	move_window(GTK_WINDOW(the_panel->window), dest_x,
		the_panel->window->allocation.y);
}

static void
move_vert(int src_y, int dest_y)
{
	int y;

	if (the_panel->step_size != 0)
		if (src_y < dest_y)
			for (y = src_y; y < dest_y; y += the_panel->step_size) {
				move_window(GTK_WINDOW(the_panel->window),
					the_panel->window->allocation.x, y);
				/* FIXME: do delay */
			}
		else
			for (y = src_y; y > dest_y; y -= the_panel->step_size) {
				move_window(GTK_WINDOW(the_panel->window),
					the_panel->window->allocation.x, y);
				/* FIXME: do delay */
			}

	move_window(GTK_WINDOW(the_panel->window),
		the_panel->window->allocation.x, dest_y);
}


static void
pop_up(void)
{
	int width, height;
	int swidth, sheight;

	if ((the_panel->state == PANEL_MOVING) ||
	    (the_panel->state == PANEL_SHOWN))
		return;

	the_panel->state = PANEL_MOVING;

	width   = the_panel->window->allocation.width;
	height  = the_panel->window->allocation.height;
	swidth  = gdk_screen_width();
	sheight = gdk_screen_height();

	switch (the_panel->pos) {
		case PANEL_POS_TOP:
		        move_vert(-height + the_panel->minimized_size, 0);
			break;

		case PANEL_POS_BOTTOM:
			move_vert(sheight - the_panel->minimized_size, 
				  sheight - height);
			break;

		case PANEL_POS_LEFT:
			move_horiz(-width + the_panel->minimized_size, 0);
			break;

		case PANEL_POS_RIGHT:
			move_horiz(swidth - the_panel->minimized_size, 
				   swidth - width);
			break;
	}

	the_panel->state = PANEL_SHOWN;
}

static gint
pop_down(gpointer data)
{
	int width, height;
	int swidth, sheight;

	if ((the_panel->state == PANEL_MOVING) ||
	    (the_panel->state == PANEL_HIDDEN))
		return FALSE;

	the_panel->state = PANEL_MOVING;

	width   = the_panel->window->allocation.width;
	height  = the_panel->window->allocation.height;
	swidth  = gdk_screen_width();
	sheight = gdk_screen_height();

	switch (the_panel->pos) {
		case PANEL_POS_TOP:
			move_vert(0, -height + the_panel->minimized_size);
			break;

		case PANEL_POS_BOTTOM:
			move_vert(sheight - height, 
				  sheight - the_panel->minimized_size);
			break;

		case PANEL_POS_LEFT:
			move_horiz(0, -width + the_panel->minimized_size);
			break;

		case PANEL_POS_RIGHT:
			move_horiz(swidth - width, 
				   swidth - the_panel->minimized_size);
			break;
	}

	the_panel->state = PANEL_HIDDEN;

	the_panel->leave_notify_timer_tag = 0;
  
	return (FALSE);
}

static void
set_show_hide_buttons_visibility(void)
{
	if(the_panel->mode!=PANEL_STAYS_PUT) {
		gtk_widget_hide(the_panel->hidebutton_l_h);
		gtk_widget_hide(the_panel->hidebutton_r_h);
		gtk_widget_hide(the_panel->hidebutton_l_v);
		gtk_widget_hide(the_panel->hidebutton_r_v);
		return;
	}
	if(the_panel->state==PANEL_SHOWN) {
		switch (the_panel->pos) {
			case PANEL_POS_TOP:
			case PANEL_POS_BOTTOM:
				gtk_widget_show(the_panel->hidebutton_l_h);
				gtk_widget_show(the_panel->hidebutton_r_h);
				gtk_widget_hide(the_panel->hidebutton_l_v);
				gtk_widget_hide(the_panel->hidebutton_r_v);
				break;
			case PANEL_POS_LEFT:
			case PANEL_POS_RIGHT:
				gtk_widget_hide(the_panel->hidebutton_l_h);
				gtk_widget_hide(the_panel->hidebutton_r_h);
				gtk_widget_show(the_panel->hidebutton_l_v);
				gtk_widget_show(the_panel->hidebutton_r_v);
				break;
		}
	} else {
		switch (the_panel->pos) {
			case PANEL_POS_TOP:
			case PANEL_POS_BOTTOM:
				gtk_widget_show(the_panel->hidebutton_l_h);
				gtk_widget_show(the_panel->hidebutton_r_h);
				gtk_widget_hide(the_panel->hidebutton_l_v);
				gtk_widget_hide(the_panel->hidebutton_r_v);
				break;
			case PANEL_POS_LEFT:
			case PANEL_POS_RIGHT:
				gtk_widget_hide(the_panel->hidebutton_l_h);
				gtk_widget_hide(the_panel->hidebutton_r_h);
				gtk_widget_show(the_panel->hidebutton_l_v);
				gtk_widget_show(the_panel->hidebutton_r_v);
				break;
		}
	}
}

static void
pop_show(int fromright)
{
	int width, height;

	if ((the_panel->state == PANEL_MOVING) ||
	    (the_panel->state == PANEL_SHOWN))
		return;

	the_panel->state = PANEL_MOVING;

	width   = the_panel->window->allocation.width;
	height  = the_panel->window->allocation.height;

	switch (the_panel->pos) {
		case PANEL_POS_TOP:
		case PANEL_POS_BOTTOM:
			if(fromright)
				move_horiz(-width +
					   the_panel->hidebutton_r_h->
					   allocation.width, 0);
			else
				move_horiz(width -
					   the_panel->hidebutton_l_h->
					   allocation.width, 0);
			break;
		case PANEL_POS_LEFT:
		case PANEL_POS_RIGHT:
			if(fromright)
				move_vert(-height +
					  the_panel->hidebutton_r_v->
					  allocation.height, 0);
			else
				move_vert(height -
					  the_panel->hidebutton_l_v->
					  allocation.height, 0);
			break;
	}

	the_panel->state = PANEL_SHOWN;

	set_show_hide_buttons_visibility();
}

static void
pop_hide(int fromright)
{
	int width, height;

	if ((the_panel->state == PANEL_MOVING) ||
	    (the_panel->state == PANEL_HIDDEN))
		return;

	the_panel->state = PANEL_MOVING;

	width   = the_panel->window->allocation.width;
	height  = the_panel->window->allocation.height;

	switch (the_panel->pos) {
		case PANEL_POS_TOP:
		case PANEL_POS_BOTTOM:
			if(fromright)
				move_horiz(0, -width +
					   the_panel->hidebutton_r_h->
					   allocation.width);
			else
				move_horiz(0, width -
					   the_panel->hidebutton_l_h->
					   allocation.width);
			break;
		case PANEL_POS_LEFT:
		case PANEL_POS_RIGHT:
			if(fromright)
				move_vert(0, -height +
					  the_panel->hidebutton_r_v->
					  allocation.height);
			else
				move_vert(0, height -
					  the_panel->hidebutton_l_v->
					  allocation.height);
			break;
	}

	the_panel->state = PANEL_HIDDEN;

	set_show_hide_buttons_visibility();
}

static void
panel_show_hide_right(GtkWidget *widget, gpointer data)
{
	if(the_panel->state == PANEL_MOVING) 
		return;
	else if(the_panel->state == PANEL_HIDDEN)
		pop_show(FALSE);
	else
		pop_hide(TRUE);
}

static void
panel_show_hide_left(GtkWidget *widget, gpointer data)
{
	if(the_panel->state == PANEL_MOVING) 
		return;
	else if(the_panel->state == PANEL_HIDDEN)
		pop_show(TRUE);
	else
		pop_hide(FALSE);
}

static gint
panel_enter_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	gdk_window_raise(the_panel->window->window);

	if ((the_panel->mode == PANEL_STAYS_PUT) ||
	    (event->detail == GDK_NOTIFY_INFERIOR))
		return FALSE;

	if (the_panel->leave_notify_timer_tag != 0) {
		gtk_timeout_remove (the_panel->leave_notify_timer_tag);
		the_panel->leave_notify_timer_tag = 0;
	}
 
	pop_up();
	
	return FALSE;
}


static gint
panel_leave_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	if ((the_panel->mode == PANEL_STAYS_PUT) ||
	    (event->detail == GDK_NOTIFY_INFERIOR))
		return FALSE;
	
	/* check if there's already a timeout set, and delete it if 
	 * there was */
	if (the_panel->leave_notify_timer_tag != 0) {
		gtk_timeout_remove (the_panel->leave_notify_timer_tag);
	}
	
	/* set up our delay for popup. */
	the_panel->leave_notify_timer_tag = gtk_timeout_add (the_panel->minimize_delay, pop_down, NULL);
	
	return FALSE;
}

static gint
visibility_notify(GtkWidget *widget, GdkEventVisibility *event, gpointer data)
{
	if(event->state==GDK_VISIBILITY_FULLY_OBSCURED)
		gdk_window_raise(the_panel->window->window);
	
	return FALSE;
}

static void
change_window_cursor(GdkWindow *window, GdkCursorType cursor_type)
{
	GdkCursor *cursor;

	cursor = gdk_cursor_new(cursor_type);
	gdk_window_set_cursor(window, cursor);
	gdk_cursor_destroy(cursor);
}


static void
realize_change_cursor(GtkWidget *widget, gpointer data)
{
	change_window_cursor(widget->window, GDK_ARROW);
}


static AppletCmdFunc
applet_cmd_func(GtkWidget *applet)
{
	return gtk_object_get_data(GTK_OBJECT(applet), APPLET_CMD_FUNC);
}


static long
applet_flags(GtkWidget *applet)
{
	return (long) gtk_object_get_data(GTK_OBJECT(applet), APPLET_FLAGS);
}


static void
get_applet_geometry(GtkWidget *applet, int *x, int *y, int *width, int *height)
{
	if (x)
		*x = applet->allocation.x;

	if (y)
		*y = applet->allocation.y;

	if (width)
		*width = applet->allocation.width;

	if (height)
		*height = applet->allocation.height;
}


static gpointer
call_applet(GtkWidget *applet, AppletCommand *cmd)
{
	AppletCmdFunc cmd_func;
	
	cmd->panel  = the_panel;
	cmd->applet = GTK_BIN(applet)->child;

	cmd_func = applet_cmd_func(applet);

	return (*cmd_func) (cmd);
}

static void
applet_orientation_notify(GtkWidget *widget, gpointer data)
{
	AppletCommand  cmd;

	cmd.cmd = APPLET_CMD_ORIENTATION_CHANGE_NOTIFY;
	call_applet(widget, &cmd);
}

static void
save_applet_configuration(GtkWidget *widget, gpointer data)
{
	int           *num;
	int            xpos, ypos;
	char          *id;
	char          *params;
	char          *path;
	char          *fullpath;
	char           buf[256];
	AppletCommand  cmd;

	num = data;

	cmd.cmd = APPLET_CMD_QUERY;
	id      = call_applet(widget, &cmd);

	cmd.cmd = APPLET_CMD_GET_INSTANCE_PARAMS;
	params  = call_applet(widget, &cmd);
	
	get_applet_geometry(widget, &xpos, &ypos, NULL, NULL);

	/* XXX: The increasing number is sort of a hack to guarantee unique keys */

	sprintf(buf, "_%d/", (*num)++);
	path = g_copy_strings("/panel/Applet", buf, NULL);

	fullpath = g_copy_strings(path,"id",NULL);
	gnome_config_set_string(fullpath, id);
	g_free(fullpath);

	switch (the_panel->pos) {
		case PANEL_POS_TOP:
		case PANEL_POS_BOTTOM:
			sprintf(buf, "%d %d", xpos, ypos);
			break;
		case PANEL_POS_LEFT:
		case PANEL_POS_RIGHT:
			/*x and y are reversed!*/
			/*this is done for loading the app position as well!!!*/
			sprintf(buf, "%d %d", ypos, xpos);
			break;
	}

	fullpath = g_copy_strings(path,"geometry",NULL);
	gnome_config_set_string(fullpath, buf);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"parameters",NULL);
	gnome_config_set_string(fullpath, params);
	g_free(fullpath);


	g_free(params);
	g_free(path);
}


static void
destroy_applet_module(gpointer key, gpointer value, gpointer user_data)
{
	AppletCommand  cmd;
	AppletFile    *af;

	cmd.cmd    = APPLET_CMD_DESTROY_MODULE;
	cmd.panel  = the_panel;
	cmd.applet = NULL;

	af = value;

	(*af->cmd_func) (&cmd);
}


/* This is called when the session manager requests a shutdown.  It
   can also be run directly when we don't detect a session manager.
   We assume no interaction is done by the applets.  And we ignore the
   other arguments for now.  Yes, this is lame.  */
int
panel_session_save (gpointer client_data,
		    GnomeSaveStyle save_style,
		    int is_shutdown,
		    GnomeInteractStyle interact_style,
		    int is_fast)
{
	int num;
	char buf[256];

	for(num=gnome_config_get_int("/panel/Applets/count=0");
		num>0;num--) {
		sprintf(buf,"/panel/Applet_%d",num);
		gnome_config_clean_section(buf);
	}

	num = 1;
	gnome_config_clean_section("/panel/Applets");
	gtk_container_foreach(GTK_CONTAINER(the_panel->fixed),
			      save_applet_configuration, &num);

	gnome_config_set_int("/panel/Applets/count",num-1);

	gnome_config_set_int("/panel/Config/position",the_panel->pos);
	gnome_config_set_int("/panel/Config/mode",the_panel->mode);
	gnome_config_set_int("/panel/Config/step_size",the_panel->step_size);
	gnome_config_set_int("/panel/Config/delay",the_panel->delay);
	gnome_config_set_int("/panel/Config/minimize_delay",
			     the_panel->minimize_delay);
	gnome_config_set_int("/panel/Config/minimized_size",
			     the_panel->minimized_size);
	gnome_config_set_bool("/panel/Config/tooltips_enabled",
			      the_panel->tooltips_enabled);
	gnome_config_sync();

	gdk_cursor_destroy(fleur_cursor);

	gtk_widget_destroy(the_panel->window);

	g_hash_table_foreach(applet_files_ht, destroy_applet_module, NULL);

	applet_files_destroy();

	/* Always successful.  */
	return 1;
}

static void
panel_quit(void)
{
  if (! gnome_session_connected_p ())
    {
      panel_session_save (NULL, GNOME_SAVE_BOTH, 1, GNOME_INTERACT_NONE, 0);
      gtk_main_quit ();
      /* We don't want to return, because we've probably been called from an
       * applet which has since been dlclose()'d, and we'd end up with a SEGV
       * when we tried to return to the now-nonexistent code page. */
      exit(0);
    }
  else
    {
      /* We request a completely interactive, full, slow shutdown.  */
      gnome_session_request_save (GNOME_SAVE_BOTH, 1, GNOME_INTERACT_ANY,
				  0, 1);
    }
}


static void
applet_drag_start(GtkWidget *applet, int warp)
{
	the_panel->applet_being_dragged = applet;

	if (warp)
		gdk_pointer_warp(NULL, applet->window,
				 0, 0, 0, 0,
				 applet->allocation.width / 2,
				 applet->allocation.height / 2);
	
	gtk_widget_get_pointer(the_panel->window,
			       &the_panel->applet_drag_click_x,
			       &the_panel->applet_drag_click_y);
	get_applet_geometry(applet,
			    &the_panel->applet_drag_orig_x,
			    &the_panel->applet_drag_orig_y,
			    NULL,
			    NULL);

	gtk_grab_add(applet);
	gdk_pointer_grab(applet->window,
			 TRUE,
			 APPLET_EVENT_MASK,
			 NULL,
			 fleur_cursor,
			 GDK_CURRENT_TIME);
}


static void
applet_drag_end(void)
{
	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gtk_grab_remove(the_panel->applet_being_dragged);
	the_panel->applet_being_dragged = NULL;
}


static void
move_applet_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget      *applet;

	applet = gtk_object_get_user_data(GTK_OBJECT(applet_menu));
	applet_drag_start(applet, TRUE);
}


static void
remove_applet_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget *applet;
	AppletCommand  cmd;
	gchar *id;

	applet = gtk_object_get_user_data(GTK_OBJECT(applet_menu));

	cmd.cmd = APPLET_CMD_QUERY;
	id      = call_applet(applet, &cmd);

	if(strcmp(id,"Menu")==0)
		if(menu_count<=1)
			return;
		/*FIXME: do something to make the user aware that this was
		  wrong ... a message box maybe ... or a beep*/
	menu_count--;

	gtk_widget_destroy(applet);
}


static void
applet_properties_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget     *applet;
	AppletCommand  cmd;

	applet = gtk_object_get_user_data(GTK_OBJECT(applet_menu));

	cmd.cmd = APPLET_CMD_PROPERTIES;

	call_applet(applet, &cmd);
}


static void
create_applet_menu(void)
{
	GtkWidget *menuitem;

	applet_menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("Move applet"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) move_applet_callback,
			   NULL);
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new_with_label(_("Remove from panel"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) remove_applet_callback,
			   NULL);
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);
	applet_menu_remove_item = menuitem;

	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);
	applet_menu_prop_separator = menuitem;
	
	menuitem = gtk_menu_item_new_with_label(_("Applet properties..."));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) applet_properties_callback,
			   NULL);
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);
	applet_menu_prop_item = menuitem;
}


static void
show_applet_menu(GtkWidget *applet)
{
	long flags;
	AppletCommand  cmd;
	gchar *id;

	flags = applet_flags(applet);

	if (flags & APPLET_HAS_PROPERTIES) {
		gtk_widget_show(applet_menu_prop_separator);
		gtk_widget_show(applet_menu_prop_item);
	} else {
		gtk_widget_hide(applet_menu_prop_separator);
		gtk_widget_hide(applet_menu_prop_item);
	}

	cmd.cmd = APPLET_CMD_QUERY;
	id      = call_applet(applet, &cmd);

	if(strcmp(id,"Menu")!=0 || menu_count>1)
		gtk_widget_show(applet_menu_remove_item);
	else
		gtk_widget_hide(applet_menu_remove_item);
	gtk_object_set_user_data(GTK_OBJECT(applet_menu), applet);

	gtk_menu_popup(GTK_MENU(applet_menu), NULL, NULL, NULL, NULL, 3, time(NULL));
}

static void
panel_properties_callback(GtkWidget *widget, gpointer data)
{
	properties();
}

static void
panel_log_out_callback(GtkWidget *widget, gpointer data)
{
	panel_quit();
}


static void
create_panel_menu(void)
{
	GtkWidget *menuitem;

	panel_menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("Panel properties..."));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) panel_properties_callback,
			   NULL);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);
	
	menuitem = gtk_menu_item_new_with_label(_("Log out"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) panel_log_out_callback,
			   NULL);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);
}


static int
panel_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if(event->button==3) {
		gtk_menu_popup(GTK_MENU(panel_menu), NULL, NULL, NULL,
			NULL, event->button, time(NULL));
		return TRUE;
	}
	return FALSE;
}

typedef struct _applet_pos {
	gint x,y,width,height;
	gint xlimit,ylimit;
	gint repaired;
	GtkWidget *w;
} applet_pos;

static gint
is_applet_pos_ok(GList *list, applet_pos *newpos)
{
	int x, y, width, height;
	int diffx, diffy;
	int nx, ny;

	GtkWidget *applet;

	for(;list;list=list->next) {
		applet = list->data;
		if(applet==newpos->w)
			continue;

		get_applet_geometry(applet, &x, &y, &width, &height);

		if((	newpos->x<(x+width) &&
			(newpos->x+newpos->width)>x) &&
			(newpos->y<(y+height) &&
			(newpos->y+newpos->height)>y)) {
			/*it's overlapping some other applet so scratch that*/
			if(newpos->repaired)
				return FALSE;
			diffx=(newpos->x+(newpos->width/2))-(x+(width/2));
			diffy=(newpos->y+(newpos->height/2))-(y+(height/2));
			nx=newpos->x+x-(newpos->x+newpos->width);
			ny=newpos->y+y-(newpos->y+newpos->height);
			if(diffx>0) nx+=width+newpos->width;
			if(diffy>0) ny+=height+newpos->height;
			if( (abs(diffy)<abs(diffx)
				|| (ny<0 && (ny+newpos->height)>newpos->ylimit))
				&& nx>=0 && (nx+newpos->width)<=newpos->xlimit) {
				newpos->x=nx;
				newpos->repaired=TRUE;
				return FALSE;
			} else if(ny>=0 && (ny+newpos->height)<=newpos->ylimit)  {
				newpos->y=ny;
				newpos->repaired=TRUE;
				return FALSE;
			}
			if(diffx>0) nx-=width+newpos->width;
			if(diffy>0) ny-=height+newpos->height;
			if( (abs(diffy)<abs(diffx)
				|| (ny<0 && (ny+newpos->height)>newpos->ylimit))
				&& nx>=0 && (nx+newpos->width)<=newpos->xlimit) {
				newpos->x=nx;
				newpos->repaired=TRUE;
				return FALSE;
			} else if(ny>=0 && (ny+newpos->height)<=newpos->ylimit)  {
				newpos->y=ny;
				newpos->repaired=TRUE;
				return FALSE;
			}
			nx+=width+newpos->width;
			ny+=height+newpos->height;
			if( (abs(diffy)<abs(diffx)
				|| (ny<0 && (ny+newpos->height)>newpos->ylimit))
				&& nx>=0 && (nx+newpos->width)<=newpos->xlimit) {
				newpos->x=nx;
				newpos->repaired=TRUE;
				return FALSE;
			} else if(ny>=0 && (ny+newpos->height)<=newpos->ylimit)  {
				newpos->y=ny;
				newpos->repaired=TRUE;
				return FALSE;
			}
			newpos->repaired=FALSE;
			return FALSE;
		}
	}
	return TRUE;
}

static gint
panel_find_clean_spot(GtkWidget *widget, GList *applets, gint *x, gint *y,
	gint width, gint height)
{
	/*set up the applet_pos structure*/
	applet_pos newpos={*x,*y,width,height,0,0,FALSE,widget};
	switch (the_panel->pos) {
		case PANEL_POS_TOP:
		case PANEL_POS_BOTTOM:
			newpos.xlimit=the_panel->fixed->allocation.width;
			newpos.ylimit=the_panel->window->allocation.height;
			break;
		case PANEL_POS_LEFT:
		case PANEL_POS_RIGHT:
			newpos.xlimit=the_panel->window->allocation.width;
			newpos.ylimit=the_panel->fixed->allocation.height;
			break;
	}

	if(!is_applet_pos_ok(applets,&newpos)) {
		if(newpos.repaired) {
			if(!is_applet_pos_ok(applets,&newpos))
				return FALSE;
		} else {
			return FALSE;
		}
	}

	*x=newpos.x;
	*y=newpos.y;
	return TRUE;
}

static void
fix_coordinates_to_limits(int *x,int *y, int width, int height)
{
	switch (the_panel->pos) {
		case PANEL_POS_TOP:
		case PANEL_POS_BOTTOM:
			if ((*x+width) > the_panel->fixed->allocation.width)
				*x=the_panel->fixed->allocation.width-width;

			if ((*y+height) > the_panel->window->allocation.height)
				*y=the_panel->window->allocation.height-height;
			break;
		case PANEL_POS_LEFT:
		case PANEL_POS_RIGHT:
			if ((*x+width) > the_panel->window->allocation.width)
				*x=the_panel->window->allocation.width-width;

			if ((*y+height) > the_panel->fixed->allocation.height)
				*y=the_panel->fixed->allocation.height-height;
			break;
	}

	if (*x < 0)
		*x = 0;

	if (*y < 0)
		*y = 0;
}

/*find a free spot for the applet*/
static void
fixup_applet_position(GtkWidget *widget, gint *x, gint *y, gint width,
	gint height,gint useoldlist)
{
	gint movedown; /*used for moving the applet (it would be move right
			 for horizontal but we use only one .. i.e. move
			 down the panel (down or right)!)
			 into acceptable position*/
	gint noturn;    /*set once a turn around has been made on one end,
			     it will prevent a hang when tehpanel is full*/
	gint *adjvar;	/*this points to the variable that we are adjusting*/
	gint adjdef;	/*default for the above*/
	gint adjlim;	/*limit to the above*/
	gint xpos,ypos;

	static GList *applets=NULL;


	if(!widget)
		return;

	xpos=widget->allocation.x;
	ypos=widget->allocation.y;

	fix_coordinates_to_limits(x,y,width,height);
	
	/*change the adjustment variables for horizontal/vertical
	  position*/
	switch (the_panel->pos) {
		case PANEL_POS_TOP:
		case PANEL_POS_BOTTOM:
			/*where did we move?*/
			movedown=(xpos<*x || xpos<=0 ||
				xpos>=the_panel->fixed->allocation.width)?
				TRUE:FALSE;
			adjvar=x;
			adjdef=*x;
			adjlim=the_panel->fixed->allocation.width-width;
			break;
		case PANEL_POS_LEFT:
		case PANEL_POS_RIGHT:
			/*where did we move?*/
			movedown=(ypos<*y || ypos<=0 ||
				ypos>=the_panel->fixed->allocation.height)?
				TRUE:FALSE;
			adjvar=y;
			adjdef=*y;
			adjlim=the_panel->fixed->allocation.height-height;
			break;
	}

	if(!useoldlist && applets) {
		g_list_free(applets);
		applets=NULL;
	}
		

	if(!applets)
		applets=gtk_container_children(GTK_CONTAINER(the_panel->fixed));

	/*move the applet as close as we can to the
	  one blocking it*/
	noturn=FALSE;
	while(!panel_find_clean_spot(widget,applets,x,y,width,height)) {
		if(movedown) {
			if(--(*adjvar)<0) {
				movedown=FALSE;
				*adjvar=adjdef;
				if(noturn)
					break;
				noturn=TRUE;
			}
		} else {
			if(++(*adjvar)>adjlim) {
				movedown=TRUE;
				*adjvar=adjdef;
				if(noturn)
					break;
				noturn=TRUE;
			}
		}
	}

	/*ajust positions to limits again!, in the worst case
	  we don't have enough room and we will have
	  to overlap applets*/
	fix_coordinates_to_limits(x,y,width,height);
}

static gint
panel_applet_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GdkEventButton *bevent;
	gint            x, y;
	int             x1, y1;
	int             dx, dy;
	int             xpos, ypos;
	int             width, height;

	GdkGC		*gc;
	GdkColor	color={1,100,200,150};

	get_applet_geometry(widget, &xpos, &ypos, &width, &height);

	switch (event->type) {
		case GDK_BUTTON_PRESS:
			bevent = (GdkEventButton *) event;

			if (the_panel->applet_being_dragged) {
				applet_drag_end();
				return TRUE;
			}

			switch (bevent->button) {
				case 2: /* Start drag */
					if (the_panel->applet_being_dragged)
						break;

					applet_drag_start(widget, FALSE);
					return TRUE;

				case 3: /* Applet menu */
					show_applet_menu(widget);
					return TRUE;
			}

			break;

		case GDK_BUTTON_RELEASE:
			bevent = (GdkEventButton *) event;
			
			if (the_panel->applet_being_dragged) {
				applet_drag_end();
				return TRUE;
			}

			break;

		case GDK_MOTION_NOTIFY:
			if (the_panel->applet_being_dragged) {
				gtk_widget_get_pointer(the_panel->window,
						       &x, &y);

				dx = x - the_panel->applet_drag_click_x;
				dy = y - the_panel->applet_drag_click_y;

				x1 = the_panel->applet_drag_orig_x + dx;
				y1 = the_panel->applet_drag_orig_y + dy;

				fixup_applet_position(the_panel->
						      applet_being_dragged,
						      &x1,&y1,width,height,
						      TRUE);

				if ((x1 != xpos) || (y1 != ypos))
					gtk_fixed_move(GTK_FIXED(the_panel->
						       fixed), the_panel->
						       applet_being_dragged,
						       x1, y1);

				return TRUE;
			}

			break;

		default:
			break;
	}

	return FALSE;
}


static GtkWidget *
listening_parent(GtkWidget *widget)
{
	if (GTK_WIDGET_NO_WINDOW(widget))
		return listening_parent(widget->parent);

	return widget;
}


static gint
panel_sub_event_handler(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GdkEventButton *bevent;

	switch (event->type) {
		case GDK_BUTTON_PRESS:
			bevent = (GdkEventButton *) event;

			switch (bevent->button) {
				case 1:
				case 2:
				case 3:
					return gtk_widget_event(listening_parent(widget->parent), event);
			}

			break;

		case GDK_BUTTON_RELEASE:
			bevent = (GdkEventButton *) event;
			
			if (the_panel->applet_being_dragged)
				return gtk_widget_event(listening_parent(widget->parent), event);
			break;

		case GDK_MOTION_NOTIFY:
			if (the_panel->applet_being_dragged)
				return gtk_widget_event(listening_parent(widget->parent), event);
			break;

		default:
			break;
	}

	return FALSE;
}


static void
get_applet_type(gpointer key, gpointer value, gpointer user_data)
{
	GList **list = user_data;

	*list = g_list_prepend(*list, g_strdup(key));
}


GList *
get_applet_types(void)
{
	GList *list = NULL;

	g_hash_table_foreach(applet_files_ht, get_applet_type, &list);
	return list;
}

static void
get_max_applet_width(GtkWidget *applet, gpointer data)
{
	if(applet->allocation.width>*((int *)data))
		*((int *)data)=applet->allocation.width;
}

static void
get_max_applet_height(GtkWidget *applet, gpointer data)
{
	if(applet->allocation.height>*((int *)data))
		*((int *)data)=applet->allocation.height;
}

static void
set_panel_position(void)
{
	int width=DEFAULT_HEIGHT,height=DEFAULT_HEIGHT;
	int buttonswidth=0;
	int buttonsheight=0;
	GtkAllocation newalloc;

	gtk_container_foreach(GTK_CONTAINER(the_panel->fixed),
		      get_max_applet_width,
		      &width);
	gtk_container_foreach(GTK_CONTAINER(the_panel->fixed),
		      get_max_applet_height,
		      &height);

	switch (the_panel->pos) {
		case PANEL_POS_TOP:
			gtk_widget_set_usize(the_panel->window,
					     gdk_screen_width(),
					     height);
			gtk_widget_set_uposition(the_panel->window, 0, 0);
			newalloc.x=0;
			newalloc.y=0;
			newalloc.width=gdk_screen_width();
			newalloc.height=height;
			if(the_panel->mode==PANEL_STAYS_PUT)
				buttonswidth=the_panel->hidebutton_l_h->
					     allocation.width+
					     the_panel->hidebutton_r_h->
					     allocation.width;
			break;
		case PANEL_POS_BOTTOM:
			gtk_widget_set_usize(the_panel->window,
					     gdk_screen_width(),
					     height);
			gtk_widget_set_uposition(the_panel->window, 0,
						 gdk_screen_height() -
						 height);
			newalloc.x=0;
			newalloc.y=gdk_screen_height() - height;
			newalloc.width=gdk_screen_width();
			newalloc.height=height;
			if(the_panel->mode==PANEL_STAYS_PUT)
				buttonswidth=the_panel->hidebutton_l_h->
					     allocation.width+
					     the_panel->hidebutton_r_h->
					     allocation.width;
			break;
		case PANEL_POS_LEFT:
			gtk_widget_set_usize(the_panel->window, width,
					     gdk_screen_height());
			gtk_widget_set_uposition(the_panel->window, 0, 0);
			newalloc.x=0;
			newalloc.y=0;
			newalloc.width=width;
			newalloc.height=gdk_screen_height();
			if(the_panel->mode==PANEL_STAYS_PUT)
				buttonsheight=the_panel->hidebutton_l_v->
					      allocation.height+
					      the_panel->hidebutton_r_v->
					      allocation.height;
			break;
		case PANEL_POS_RIGHT:
			gtk_widget_set_usize(the_panel->window, width,
					     gdk_screen_height());
			gtk_widget_set_uposition(the_panel->window,
						 gdk_screen_width() -
						 width, 0);
			newalloc.x=gdk_screen_width() - width;
			newalloc.y=0;
			newalloc.width=width;
			newalloc.height=gdk_screen_height();
			if(the_panel->mode==PANEL_STAYS_PUT)
				buttonsheight=the_panel->hidebutton_l_v->
					      allocation.height+
					      the_panel->hidebutton_r_v->
					      allocation.height;
			break;
	}
	gtk_widget_size_allocate(the_panel->window,&newalloc);
	newalloc.width-=buttonswidth;
	newalloc.height-=buttonsheight;
	gtk_widget_size_allocate(the_panel->fixed,&newalloc);
}





void
panel_init(void)
{
	GtkWidget *pixmap;
	char *pixmap_name;
	char buf[256];

	the_panel = g_new(Panel, 1);

	the_panel->window = gtk_window_new(GTK_WINDOW_POPUP);


	the_panel->state = PANEL_SHOWN;
	the_panel->applet_being_dragged = NULL;

	sprintf(buf,"/panel/Config/position=%d",PANEL_POS_BOTTOM);
	the_panel->pos=gnome_config_get_int(buf);
	sprintf(buf,"/panel/Config/mode=%d",PANEL_STAYS_PUT);
	the_panel->mode=gnome_config_get_int(buf);
	sprintf(buf,"/panel/Config/step_size=%d",DEFAULT_STEP_SIZE);
	the_panel->step_size=gnome_config_get_int(buf);
	sprintf(buf,"/panel/Config/delay=%d",DEFAULT_DELAY);
	the_panel->delay=gnome_config_get_int(buf);
	sprintf(buf,"/panel/Config/minimize_delay=%d",DEFAULT_MINIMIZE_DELAY);
	the_panel->minimize_delay=gnome_config_get_int(buf);
	sprintf(buf,"/panel/Config/minimized_size=%d",DEFAULT_MINIMIZED_SIZE);
	the_panel->minimized_size=gnome_config_get_int(buf);
	the_panel->tooltips_enabled=gnome_config_get_bool(
		"/panel/Config/tooltips_enabled=true");


	the_panel->table = gtk_table_new(3,3,FALSE);


	/*hide buttons (one for vertical one for horizontal)*/
	the_panel->hidebutton_l_h=gtk_button_new();
	pixmap_name=gnome_unconditional_pixmap_file("panel-arrow-left.xpm");
	pixmap = gnome_create_pixmap_widget(the_panel->window,
					    the_panel->hidebutton_l_h,
					    pixmap_name);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(the_panel->hidebutton_l_h),pixmap);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(the_panel->hidebutton_l_h), "clicked",
			   GTK_SIGNAL_FUNC(panel_show_hide_right),NULL);
	gtk_table_attach(GTK_TABLE(the_panel->table),the_panel->hidebutton_l_h,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	the_panel->hidebutton_l_v=gtk_button_new();
	pixmap_name=gnome_unconditional_pixmap_file("panel-arrow-up.xpm");
	pixmap = gnome_create_pixmap_widget(the_panel->window,
					    the_panel->hidebutton_l_v,
					    pixmap_name);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(the_panel->hidebutton_l_v),pixmap);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(the_panel->hidebutton_l_v), "clicked",
			   GTK_SIGNAL_FUNC(panel_show_hide_right),NULL);
	gtk_table_attach(GTK_TABLE(the_panel->table),the_panel->hidebutton_l_v,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);
	
	/*fixed*/
	the_panel->fixed = gtk_fixed_new();

	gtk_table_attach(GTK_TABLE(the_panel->table),the_panel->fixed,
			 1,2,1,2,GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,0,0);
	gtk_widget_show(the_panel->fixed);

	/*show buttons (one for vertical one for horizontal)*/
	the_panel->hidebutton_r_h=gtk_button_new();
	pixmap_name=gnome_unconditional_pixmap_file("panel-arrow-right.xpm");
	pixmap = gnome_create_pixmap_widget(the_panel->window,
					    the_panel->hidebutton_r_h,
					    pixmap_name);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(the_panel->hidebutton_r_h),pixmap);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(the_panel->hidebutton_r_h), "clicked",
			   GTK_SIGNAL_FUNC(panel_show_hide_left),NULL);
	gtk_table_attach(GTK_TABLE(the_panel->table),the_panel->hidebutton_r_h,
			 2,3,1,2,GTK_FILL,GTK_FILL,0,0);
	the_panel->hidebutton_r_v=gtk_button_new();
	pixmap_name=gnome_unconditional_pixmap_file("panel-arrow-down.xpm");
	pixmap = gnome_create_pixmap_widget(the_panel->window,
					    the_panel->hidebutton_r_v,
					    pixmap_name);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(the_panel->hidebutton_r_v),pixmap);
	gtk_widget_show(pixmap);
	gtk_signal_connect(GTK_OBJECT(the_panel->hidebutton_r_v), "clicked",
			   GTK_SIGNAL_FUNC(panel_show_hide_left),NULL);
	gtk_table_attach(GTK_TABLE(the_panel->table),the_panel->hidebutton_r_v,
			 1,2,2,3,GTK_FILL,GTK_FILL,0,0);
	
	set_show_hide_buttons_visibility();

	/*add this whole thing to the window*/
	gtk_container_add(GTK_CONTAINER(the_panel->window), the_panel->table);
	gtk_widget_show(the_panel->table);

	/*set the position and size of the panel*/
	set_panel_position();

	the_panel->enter_notify_id = gtk_signal_connect(GTK_OBJECT(the_panel->window), "enter_notify_event",
							(GtkSignalFunc) panel_enter_notify,
							the_panel);
	the_panel->leave_notify_id = gtk_signal_connect(GTK_OBJECT(the_panel->window), "leave_notify_event",
							(GtkSignalFunc) panel_leave_notify,
							the_panel);
	the_panel->visibility_notify_id = gtk_signal_connect(GTK_OBJECT(the_panel->window), "visibility_notify_event",
							(GtkSignalFunc) visibility_notify,
							NULL);

	the_panel->button_press_id = gtk_signal_connect(GTK_OBJECT(the_panel->window), "button_press_event",
							(GtkSignalFunc) panel_button_press,
							NULL);
	/*gtk_signal_connect_after(GTK_OBJECT(the_panel->window), "realize",
				 (GtkSignalFunc) realize_change_cursor,
				 NULL);*/

	change_window_cursor(the_panel->window->window, GDK_ARROW);
	
	gdk_window_set_events(the_panel->window->window,
			      GDK_VISIBILITY_NOTIFY_MASK |
			      GDK_ENTER_NOTIFY_MASK |
			      GDK_LEAVE_NOTIFY_MASK |
			      GDK_BUTTON_RELEASE_MASK);

	fleur_cursor = gdk_cursor_new(GDK_FLEUR);

	create_applet_menu();
	create_panel_menu();

	/*set up the tooltips*/
	panel_tooltips=gtk_tooltips_new();
	if(the_panel->tooltips_enabled)
		gtk_tooltips_enable(panel_tooltips);
	else
		gtk_tooltips_disable(panel_tooltips);
}


static void
init_applet_module(gpointer key, gpointer value, gpointer user_data)
{
	AppletCommand  cmd;
	AppletFile    *af;

	cmd.cmd    = APPLET_CMD_INIT_MODULE;
	cmd.panel  = the_panel;
	cmd.applet = NULL;
	cmd.params.init_module.cmd_func = panel_command;

	af = value;

	(*af->cmd_func) (&cmd);
}


void
panel_init_applet_modules(void)
{
	g_hash_table_foreach(applet_files_ht, init_applet_module, NULL);
}


static void
create_applet(char *id, char *params, int xpos, int ypos)
{
	AppletCommand cmd;
	AppletCmdFunc cmd_func;
	int           requested;
	
	g_assert(id != NULL);
	
	cmd_func = get_applet_cmd_func(id);
	if (!cmd_func) {
		fprintf(stderr, "create_applet: could not find applet \"%s\"\n", id);
		return;
	}

	requested = FALSE;
	
	if (!params) {
		cmd.cmd    = APPLET_CMD_GET_DEFAULT_PARAMS;
		cmd.panel  = the_panel;
		cmd.applet = NULL;
		
		params = (*cmd_func) (&cmd);

		if (!params) {
			fprintf(stderr,
				"create_applet: warning: applet \"%s\" returned NULL default parameters\n"
				"               using empty parameter string \"\"\n",
				id);
			params = g_strdup("");
		}

		requested = TRUE;
	}

	cmd.cmd    = APPLET_CMD_CREATE_INSTANCE;
	cmd.panel  = the_panel;
	cmd.applet = NULL;
	cmd.params.create_instance.params = params;
	cmd.params.create_instance.xpos   = xpos;
	cmd.params.create_instance.ypos   = ypos;

	(*cmd_func) (&cmd);

	if (requested)
		g_free(params);
}


static void
bind_applet_events(GtkWidget *widget, void *data)
{
	/* XXX: This is more or less a hack.  We need to be able to
	 * capture events over applets so that we can drag them with
	 * the mouse and such.  So we need to force the applet's
	 * widgets to recursively send the events back to their parent
	 * until the event gets to the applet wrapper (the
	 * GtkEventBox) for processing by us.
	 */
	
	if (!GTK_WIDGET_NO_WINDOW(widget)) {
		gtk_widget_set_events(widget, gtk_widget_get_events(widget) | APPLET_EVENT_MASK);
		gtk_signal_connect(GTK_OBJECT(widget), "event",
				   (GtkSignalFunc) panel_sub_event_handler,
				   NULL);
	}
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget), bind_applet_events, 0);
}

static void
sort_applet_by_pos(GtkWidget *applet, gpointer data)
{
	GList     **list;
	GtkWidget *w;
	int        pos;

	list = data;
	pos  = 0;

	while (*list) {
		w = (*list)->data;
		
		if (applet->allocation.x < w->allocation.x)
			break;

		pos++;
		*list = (*list)->next;
	}

	*list = g_list_insert(*list, applet, pos);
}



static void
fix_applet_position(GtkWidget *applet, int *xpos, int *ypos)
{
	GList         *applets;
	GtkWidget     *w;
	GtkRequisition requisition;
	int            req_width;
	int            found;
	int            x;
	int            size;
	int            largest;
	int            x_for_largest;
	
	/* FIXME!!!  This is not working at all --- the widgets have
         * no allocation when I need them to :-(
	 */

	/* FIXME: This routine currently only handles the horizontal case of the panel */
	
	if ((*xpos != PANEL_UNKNOWN_APPLET_POSITION) &&
	    (*ypos != PANEL_UNKNOWN_APPLET_POSITION))
		return;

	*ypos = 0;

	applets = NULL;
	gtk_container_foreach(GTK_CONTAINER(the_panel->fixed),
			      sort_applet_by_pos,
			      &applets);

	gtk_widget_size_request(applet, &requisition);
	req_width = requisition.width;

	/* Find first fit */

	found         = FALSE;
	x             = 0;
	largest       = 0;
	x_for_largest = 0;

	for (; applets; applets = applets->next) {
		w = applets->data;

		size = w->allocation.x - x;

		if (size >= req_width) {
			found = TRUE;
			break;
		}

		if (size > largest) {
			largest = size;
			x_for_largest = x;
		}

		x = w->allocation.x + w->allocation.width;
	}

	if (found)
		*xpos = x;
	else {
		size = gdk_screen_width() - x;

		if (size >= req_width)
			*xpos = x;
		else
			*xpos = x_for_largest;
	}

	g_list_free(applets);
}

static applet_pos applet_stack[100];
static gint applet_stacktop=-1;
static gint have_idle_func=FALSE;

static gint
fix_an_applet_idle_func(gpointer data)
{
	int xpos;
	int ypos;
	GtkWidget *applet;

	if(applet_stacktop<0) {
		have_idle_func=FALSE;
		return FALSE;
	}

	applet=applet_stack[applet_stacktop].w;
	xpos=applet_stack[applet_stacktop].x;
	ypos=applet_stack[applet_stacktop].y;

	if(the_panel->window->allocation.width<=1 ||
		the_panel->fixed->allocation.width<=1 ||
		GTK_WIDGET(applet)->allocation.width<=1)
		return TRUE;

	/*we probably want to do something like this but we need to
	  refine that function*/
	/*fix_applet_position(applet, &xpos, &ypos);*/

	fixup_applet_position(applet,&xpos,&ypos,
		GTK_WIDGET(applet)->allocation.width,
		GTK_WIDGET(applet)->allocation.height,FALSE);
	gtk_fixed_move(GTK_FIXED(the_panel->fixed), applet, xpos, ypos);
	if((--applet_stacktop)<0) {
		set_panel_position();
		have_idle_func=FALSE;
		return FALSE;
	}
	return TRUE;
}

/*add an applet to the stack of applets to fix up and create an
  idle function if we haven't made it yet*/
static gint
fix_an_applet(GtkWidget *applet,gint x, gint y)
{
	if((++applet_stacktop)>=100) {
		applet_stacktop--;
		return FALSE;
	}
	applet_stack[applet_stacktop].w=applet;
	applet_stack[applet_stacktop].x=x;
	applet_stack[applet_stacktop].y=y;
	/*we don't use the rest of applet_pos structure*/
	if(have_idle_func)
		return TRUE;

	gtk_idle_add((void *)fix_an_applet_idle_func, NULL);
	have_idle_func=TRUE;
	return TRUE;
}

static void
set_tooltip(GtkWidget *applet, char *tooltip)
{
	if(!applet)
		return;
	gtk_tooltips_set_tips(panel_tooltips,applet,tooltip);
}


static void
register_toy(GtkWidget *applet, char *id, int xpos, int ypos, long flags)
{
	GtkWidget     *eventbox;
	
	g_assert(applet != NULL);
	g_assert(id != NULL);

	/* We wrap the applet in a GtkEventBox so that we can capture events over it */

	eventbox = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(eventbox), applet);

	gtk_widget_set_events(eventbox, gtk_widget_get_events(eventbox) | APPLET_EVENT_MASK);
	gtk_signal_connect(GTK_OBJECT(eventbox), "event", (GtkSignalFunc) panel_applet_event, NULL);
	bind_applet_events(applet, NULL);
	
	/* Attach our private data to the applet */

	gtk_object_set_data(GTK_OBJECT(eventbox), APPLET_CMD_FUNC, get_applet_cmd_func(id));
	gtk_object_set_data(GTK_OBJECT(eventbox), APPLET_FLAGS, (gpointer) flags);

	switch (the_panel->pos) {
		case PANEL_POS_TOP:
		case PANEL_POS_BOTTOM:
			fix_an_applet(eventbox,xpos,ypos);
			gtk_fixed_put(GTK_FIXED(the_panel->fixed), eventbox,
				      0, 0);
			break;
		case PANEL_POS_LEFT:
		case PANEL_POS_RIGHT:
			/*x and y are reversed!*/
			/*this is done for saving the app position as well!!!*/
			fix_an_applet(eventbox,ypos,xpos);
			gtk_fixed_put(GTK_FIXED(the_panel->fixed), eventbox,
				      0, 0);
			break;
	}

	gtk_widget_show(eventbox);
	gtk_widget_show(applet);

	/*notify the applet of the orientation of the panel!*/
	applet_orientation_notify(eventbox,NULL);

	if(strcmp(id,"Menu")==0)
		menu_count++;
}

static void
change_size_notify(GtkWidget *applet)
{
	fix_an_applet(applet,applet->allocation.x,applet->allocation.y);
}


static void
swap_applet_coords(GtkWidget *applet, gpointer data)
{
	gint x,y,width,height;

	get_applet_geometry(applet, &x, &y, &width, &height);

	/*swap the coordinates around*/
	fix_coordinates_to_limits(&y,&x,width,height);
	gtk_fixed_move(GTK_FIXED(the_panel->fixed), applet, y, x);
	fix_an_applet(applet,y,x);
}

static void
panel_change_orient(void)
{
	gtk_container_foreach(GTK_CONTAINER(the_panel->fixed),
		      swap_applet_coords,NULL);
}

static void
fix_an_applet_foreach(GtkWidget *applet, gpointer data)
{
	gint x,y,width,height;

	get_applet_geometry(applet, &x, &y, &width, &height);

	/*swap the coordinates around*/
	fix_coordinates_to_limits(&x,&y,width,height);
	gtk_fixed_move(GTK_FIXED(the_panel->fixed), applet, x, y);
	fix_an_applet(applet,x,y);
}

static void
panel_fix_all_applets(void)
{
	gtk_container_foreach(GTK_CONTAINER(the_panel->fixed),
		              fix_an_applet_foreach,NULL);
}



void
panel_reconfigure(Panel *newconfig)
{
	gint oldpos;
	gint oldmode;

	if(the_panel->mode == PANEL_GETS_HIDDEN) {
		the_panel->step_size=0;
		pop_up();
	}
	/*the set panel position will make it shown, this may change!
	  it would require more work to keep the state to be persistent
	  accross sessions or even reconfigurations*/
	the_panel->state = PANEL_SHOWN;
	oldmode = the_panel->mode;
	the_panel->mode = newconfig->mode;
	oldpos=the_panel->pos;
	the_panel->pos = newconfig->pos;
	if(newconfig->pos != oldpos) {
		set_show_hide_buttons_visibility();
		set_panel_position();
		switch (oldpos) {
			case PANEL_POS_TOP:
				if(newconfig->pos != PANEL_POS_BOTTOM)
					panel_change_orient();
				else
					panel_fix_all_applets();
				break;
			case PANEL_POS_BOTTOM:
				if(newconfig->pos != PANEL_POS_TOP)
					panel_change_orient();
				else
					panel_fix_all_applets();
				break;
			case PANEL_POS_LEFT:
				if(newconfig->pos != PANEL_POS_RIGHT)
					panel_change_orient();
				else
					panel_fix_all_applets();
				break;
			case PANEL_POS_RIGHT:
				if(newconfig->pos != PANEL_POS_LEFT)
					panel_change_orient();
				else
					panel_fix_all_applets();
				break;
		}
		/*notify each applet that we're changing orientation!*/
		gtk_container_foreach(GTK_CONTAINER(the_panel->fixed),
				      applet_orientation_notify,NULL);
	} else if(oldmode != newconfig->mode) {
		set_show_hide_buttons_visibility();
		set_panel_position();
		panel_fix_all_applets();
	}

	the_panel->step_size = newconfig->step_size;
	the_panel->delay = newconfig->delay;
	the_panel->minimize_delay = newconfig->minimize_delay;
	the_panel->minimized_size = newconfig->minimized_size;
	the_panel->tooltips_enabled = newconfig->tooltips_enabled;
	if(the_panel->tooltips_enabled)
		gtk_tooltips_enable(panel_tooltips);
	else
		gtk_tooltips_disable(panel_tooltips);
}

static void
properties(void)
{
	panel_config();
}

gpointer
panel_command(PanelCommand *cmd)
{
	g_assert(cmd != NULL);
	
	switch (cmd->cmd) {
		case PANEL_CMD_QUIT:
			panel_quit();
			return NULL;

		case PANEL_CMD_GET_APPLET_TYPES:
			return get_applet_types();

		case PANEL_CMD_GET_APPLET_CMD_FUNC:
			return get_applet_cmd_func(cmd->params.get_applet_cmd_func.id);

		case PANEL_CMD_CREATE_APPLET:
			create_applet(cmd->params.create_applet.id,
				      cmd->params.create_applet.params,
				      cmd->params.create_applet.xpos,
				      cmd->params.create_applet.ypos);
			break;
			
		case PANEL_CMD_REGISTER_TOY:
			register_toy(cmd->params.register_toy.applet,
				     cmd->params.register_toy.id,
				     cmd->params.register_toy.xpos,
				     cmd->params.register_toy.ypos,
				     cmd->params.register_toy.flags);
			break;

		case PANEL_CMD_SET_TOOLTIP:
			set_tooltip(cmd->params.set_tooltip.applet,
				    cmd->params.set_tooltip.tooltip);
			break;

		case PANEL_CMD_CHANGE_SIZE_NOTIFY:
			change_size_notify(
				cmd->params.change_size_notify.applet);
			break;

		case PANEL_CMD_PROPERTIES:
			properties();
			break;

		default:
			fprintf(stderr, "panel_command: Oops, unknown command type %d\n",
				(int) cmd->cmd);
			break;
	}

	return NULL;
}
