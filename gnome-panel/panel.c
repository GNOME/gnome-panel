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

#ifdef HAVE_LIBINTL
#define _(String) gettext(String)
#else
#define _(String) (String)
#endif


#define APPLET_CMD_FUNC "panel_applet_cmd_func"
#define APPLET_FLAGS    "panel_applet_flags"

#define DEFAULT_STEP_SIZE 6
#define DEFAULT_DELAY     0
#define DEFAULT_HEIGHT    48

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
static GtkWidget *applet_menu_prop_separator;
static GtkWidget *applet_menu_prop_item;

static GdkCursor *fleur_cursor;

Panel *the_panel;


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
pop_down(void)
{
	int width, height;
	int swidth, sheight;

	if ((the_panel->state == PANEL_MOVING) ||
	    (the_panel->state == PANEL_HIDDEN))
		return;

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
pop_show(void)
{
	int width, height;
	int swidth, sheight;

	if ((the_panel->state == PANEL_MOVING) ||
	    (the_panel->state == PANEL_SHOWN))
		return;

	gtk_widget_show(the_panel->hidebutton);
	gtk_widget_hide(the_panel->showbutton);

	the_panel->state = PANEL_MOVING;

	width   = the_panel->window->allocation.width;
	height  = the_panel->window->allocation.height;
	swidth  = gdk_screen_width();
	sheight = gdk_screen_height();

	switch (the_panel->pos) {
		case PANEL_POS_TOP:
		case PANEL_POS_BOTTOM:
			move_horiz(-width +
				the_panel->hidebutton->allocation.width, 0);
			break;
		case PANEL_POS_LEFT:
		case PANEL_POS_RIGHT:
			move_vert(-height +
				the_panel->hidebutton->allocation.height, 0);
			break;
	}

	the_panel->state = PANEL_SHOWN;
}

static void
pop_hide(void)
{
	int width, height;
	int swidth, sheight;

	if ((the_panel->state == PANEL_MOVING) ||
	    (the_panel->state == PANEL_HIDDEN))
		return;

	gtk_widget_hide(the_panel->hidebutton);
	gtk_widget_show(the_panel->showbutton);

	the_panel->state = PANEL_MOVING;

	width   = the_panel->window->allocation.width;
	height  = the_panel->window->allocation.height;
	swidth  = gdk_screen_width();
	sheight = gdk_screen_height();

	switch (the_panel->pos) {
		case PANEL_POS_TOP:
		case PANEL_POS_BOTTOM:
			move_horiz(0, -width +
				the_panel->hidebutton->allocation.width);
			break;
		case PANEL_POS_LEFT:
		case PANEL_POS_RIGHT:
			move_vert(0, -height +
				the_panel->hidebutton->allocation.height);
			break;
	}

	the_panel->state = PANEL_HIDDEN;
}

static void
panel_show_hide(GtkWidget *widget, gpointer data)
{
	if(the_panel->state == PANEL_MOVING) 
		return;
	else if(the_panel->state == PANEL_HIDDEN)
		pop_show();
	else
		pop_hide();
}

static gint
panel_enter_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	gdk_window_raise(widget->window);
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
	the_panel->leave_notify_timer_tag = gtk_timeout_add (500, pop_down, NULL);
	
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
save_applet_configuration(GtkWidget *widget, gpointer data)
{
	int           *num;
	int            xpos, ypos;
	char          *id;
	char          *params;
	char          *path;
	char           buf[256];
	AppletCommand  cmd;

	num = data;

	cmd.cmd = APPLET_CMD_QUERY;
	id      = call_applet(widget, &cmd);

	cmd.cmd = APPLET_CMD_GET_INSTANCE_PARAMS;
	params  = call_applet(widget, &cmd);
	
	get_applet_geometry(widget, &xpos, &ypos, NULL, NULL);

	/* XXX: The increasing number is sort of a hack to guarantee unique keys */

	sprintf(buf, "%d,", (*num)++);
	path = g_copy_strings("/panel/Applets/", buf, id, ",", params, NULL);
	sprintf(buf, "%d %d", xpos, ypos);

	gnome_config_set_string(path, buf);

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

	num = 0;
	gnome_config_clean_section("/panel/Applets");
	gtk_container_foreach(GTK_CONTAINER(the_panel->fixed), save_applet_configuration, &num);
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
applet_drag_end(GtkWidget *applet)
{
	the_panel->applet_being_dragged = NULL;

	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gtk_grab_remove(applet);
}


static void
move_applet_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget      *applet;

	/* FIXME: when the mouse moves outside of the applet, no
	 * motion events are sent to it, even when the applet has a
	 * gtk_grab to it.  However, when the drag was started by the
	 * mouse instead of the menu, it works.  I don't know why this
	 * happens.
	 */
	
	applet = gtk_object_get_user_data(GTK_OBJECT(applet_menu));
	applet_drag_start(applet, TRUE);
}


static void
remove_applet_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget *applet;

	applet = gtk_object_get_user_data(GTK_OBJECT(applet_menu));
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

	flags = applet_flags(applet);

	if (flags & APPLET_HAS_PROPERTIES) {
		gtk_widget_show(applet_menu_prop_separator);
		gtk_widget_show(applet_menu_prop_item);
	} else {
		gtk_widget_hide(applet_menu_prop_separator);
		gtk_widget_hide(applet_menu_prop_item);
	}

	gtk_object_set_user_data(GTK_OBJECT(applet_menu), applet);

	gtk_menu_popup(GTK_MENU(applet_menu), NULL, NULL, NULL, NULL, 3, time(NULL));
}

typedef struct _applet_pos {
	gint x,y,width,height;
	gint ok;
	GtkWidget *w;
} applet_pos;

static void
is_applet_pos_ok(GtkWidget *applet, gpointer data)
{
	applet_pos *newpos;
	int x, y, width, height;

	newpos=(applet_pos *)data;

	if(!(newpos->ok) || (applet==newpos->w && newpos->w!=NULL))
		return;

	get_applet_geometry(applet, &x, &y, &width, &height);

	if((	newpos->x<(x+width) &&
		(newpos->x+newpos->width)>x) &&
		(newpos->y<(y+height) &&
		(newpos->y+newpos->height)>y))
		/*it's overlapping some other applet so scratch that*/
		newpos->ok=FALSE;
	else
		return;
}

static gint
panel_is_spot_clean(GtkWidget *widget, gint x, gint y, gint width,
	gint height)
{
	/*set up the applet_pos structure*/
	applet_pos newpos={x,y,width,height,TRUE,widget};

	gtk_container_foreach(GTK_CONTAINER(the_panel->fixed),
		      is_applet_pos_ok,
		      &newpos);
	if(newpos.ok)
		return TRUE;
	else
		return FALSE;
}

/*find a free spot for the applet*/
static void
fixup_applet_position(GtkWidget *widget, gint *x, gint *y, gint width,
	gint height)
{
	gint moveright,movedown; /*used for moving the applet
						into acceptable position*/
	gint x2,y2,xpos,ypos;


	if(!widget)
		return;

	xpos=widget->allocation.x;
	ypos=widget->allocation.y;

	x2 = *x + width;
	y2 = *y + height;

	if (x2 > the_panel->fixed->allocation.width)
		*x = the_panel->fixed->allocation.width - width;

	if (y2 > the_panel->fixed->allocation.height)
		*y = the_panel->fixed->allocation.height - height;

	if (*x < 0)
		*x = 0;

	if (*y < 0)
		*y = 0;

	/*where did we move?*/
	moveright=(xpos<*x || xpos<=0 ||
		xpos>=the_panel->fixed->allocation.width)?TRUE:FALSE;
	movedown=(ypos<*y || ypos<=0 ||
		ypos>=the_panel->fixed->allocation.height)?TRUE:FALSE;

	/*move the applet as close as we can to the
	  one blocking it*/
	while(!panel_is_spot_clean(widget,*x,*y,
		width,height)) {

		if(moveright) {
			if(--(*x)<0) {
				moveright=FALSE;
				*x=xpos;
			}
		} else
			(*x)++;
		/* FIXME: should be done for
		   vertical bar as well */
	}

	/*ajust positions again!, in the worst case
	  we don't have enough room and we will have
	  to overlap applets*/
	x2 = *x + width;
	y2 = *y + height;

	if (x2 > the_panel->fixed->allocation.width)
		*x = the_panel->fixed->allocation.width - width;

	if (y2 > the_panel->fixed->allocation.height)
		*y = the_panel->fixed->allocation.height - height;

	if (*x < 0)
		*x = 0;

	if (*y < 0)
		*y = 0;
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

	get_applet_geometry(widget, &xpos, &ypos, &width, &height);

	switch (event->type) {
		case GDK_BUTTON_PRESS:
			bevent = (GdkEventButton *) event;

			if (the_panel->applet_being_dragged)
				return TRUE;

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
			
			if (the_panel->applet_being_dragged == widget) {
				applet_drag_end(widget);
				return TRUE;
			}

			break;

		case GDK_MOTION_NOTIFY:
			if (the_panel->applet_being_dragged == widget) {
				gtk_widget_get_pointer(the_panel->window, &x, &y);

				dx = x - the_panel->applet_drag_click_x;
				dy = y - the_panel->applet_drag_click_y;

				x1 = the_panel->applet_drag_orig_x + dx;
				y1 = the_panel->applet_drag_orig_y + dy;

				fixup_applet_position(widget,&x1,&y1,
					width,height);

				if ((x1 != xpos) || (y1 != ypos))
					gtk_fixed_move(GTK_FIXED(the_panel->fixed), widget, x1, y1);

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


void
panel_init(void)
{
	the_panel = g_new(Panel, 1);

	the_panel->window = gtk_window_new(GTK_WINDOW_POPUP);

	the_panel->box = gtk_hbox_new(FALSE,0); /*FIXME: this will have to be
							vbox for a vertical
							one!*/

	the_panel->pos   = PANEL_POS_BOTTOM;
	the_panel->state = PANEL_SHOWN;
	the_panel->mode  = PANEL_GETS_HIDDEN;
	/*the_panel->mode  = PANEL_STAYS_PUT;*/

	/*FIXME: the hide button should only be shown if the panel
	  is staying put*/
	
	/* I'll leave you to fix it, cause it does kinda screw up in
	 * PANEL_GETS_HIDDEN mode */
	the_panel->hidebutton=gtk_button_new_with_label("<");
	gtk_signal_connect(GTK_OBJECT(the_panel->hidebutton), "clicked",
			   GTK_SIGNAL_FUNC(panel_show_hide),NULL);
	gtk_box_pack_start(GTK_BOX(the_panel->box),the_panel->hidebutton,
			   FALSE,FALSE,0);
	gtk_widget_show(the_panel->hidebutton);
	
	
	the_panel->fixed = gtk_fixed_new();

	gtk_box_pack_start(GTK_BOX(the_panel->box),the_panel->fixed,
		TRUE,TRUE,0);
	gtk_widget_show(the_panel->fixed);

	the_panel->showbutton=gtk_button_new_with_label(">");
	gtk_signal_connect(GTK_OBJECT(the_panel->showbutton), "clicked",
				  GTK_SIGNAL_FUNC(panel_show_hide),NULL);
	gtk_box_pack_start(GTK_BOX(the_panel->box),the_panel->showbutton,
		FALSE,FALSE,0);

	gtk_container_add(GTK_CONTAINER(the_panel->window), the_panel->box);
	gtk_widget_show(the_panel->box);

	gtk_widget_set_usize(the_panel->window, gdk_screen_width(), DEFAULT_HEIGHT);
	gtk_widget_set_uposition(the_panel->window, 0, gdk_screen_height() - DEFAULT_HEIGHT);
#if 0
	the_panel->pos   = PANEL_POS_TOP;
	the_panel->state = PANEL_SHOWN;
	the_panel->mode  = PANEL_GETS_HIDDEN;
	gtk_widget_set_usize(the_panel->window, gdk_screen_width(), DEFAULT_HEIGHT);
	gtk_widget_set_uposition(the_panel->window, 0, 0);
#endif
#if 0
	the_panel->pos   = PANEL_POS_RIGHT;
	the_panel->state = PANEL_SHOWN;
	the_panel->mode  = PANEL_GETS_HIDDEN;
	gtk_widget_set_usize(the_panel->window, DEFAULT_HEIGHT, gdk_screen_height());
	gtk_widget_set_uposition(the_panel->window, gdk_screen_width() - DEFAULT_HEIGHT, 0);
#endif
#if 0
	the_panel->pos   = PANEL_POS_LEFT;
	the_panel->state = PANEL_SHOWN;
	the_panel->mode  = PANEL_GETS_HIDDEN;
	gtk_widget_set_usize(the_panel->window, DEFAULT_HEIGHT, gdk_screen_height());
	gtk_widget_set_uposition(the_panel->window, 0, 0);
#endif
	the_panel->step_size            = DEFAULT_STEP_SIZE;
	the_panel->delay                = DEFAULT_DELAY;
	the_panel->minimize_delay       = DEFAULT_MINIMIZE_DELAY;
	the_panel->minimized_size       = DEFAULT_MINIMIZED_SIZE;
	the_panel->applet_being_dragged = NULL;

	the_panel->enter_notify_id = gtk_signal_connect(GTK_OBJECT(the_panel->window), "enter_notify_event",
							(GtkSignalFunc) panel_enter_notify,
							the_panel);
	the_panel->leave_notify_id = gtk_signal_connect(GTK_OBJECT(the_panel->window), "leave_notify_event",
							(GtkSignalFunc) panel_leave_notify,
							the_panel);
	gtk_signal_connect_after(GTK_OBJECT(the_panel->window), "realize",
				 (GtkSignalFunc) realize_change_cursor,
				 NULL);

	fleur_cursor = gdk_cursor_new(GDK_FLEUR);

	create_applet_menu();
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


static gint
fix_an_applet_idle_func(gpointer data)
{
	int xpos;
	int ypos;
	GtkWidget *applet;
	static GtkWidget *occupied;

	if(!data)
		return FALSE;

	applet=(GtkWidget *)data;

	if(occupied) {
		/*this time it's occupied, wait for next time*/
		if(occupied!=applet)
			return TRUE;
		/*free up the function*/
		occupied=NULL;
		return FALSE;
	}
	occupied=applet;

	if(GTK_WIDGET(applet)->allocation.width<=1) {
		occupied=NULL;
		return TRUE;
	}

	xpos=GTK_WIDGET(applet)->allocation.x;
	ypos=GTK_WIDGET(applet)->allocation.y;

	/*we probably want to do something like this but we need to
	  refine that function*/
	/*fix_applet_position(applet, &xpos, &ypos);*/

	fixup_applet_position(applet,&xpos,&ypos,
		GTK_WIDGET(applet)->allocation.width,
		GTK_WIDGET(applet)->allocation.height);
	gtk_fixed_move(GTK_FIXED(the_panel->fixed), applet, xpos, ypos);
	return TRUE;
}


static void
register_toy(GtkWidget *applet, char *id, int xpos, int ypos, long flags)
{
	GtkWidget *eventbox;
	
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

	/*fix_applet_position(eventbox, &xpos, &ypos);*/

	gtk_idle_add((void *)fix_an_applet_idle_func, eventbox);

	gtk_fixed_put(GTK_FIXED(the_panel->fixed), eventbox, xpos, ypos);

	gtk_widget_show(eventbox);
	gtk_widget_show(applet);
}


static void
properties(void)
{
	fprintf(stderr, "Panel properties not yet implemented\n");
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
