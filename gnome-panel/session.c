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
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include <X11/keysym.h>
#include "panel-include.h"
#include "gnome-run.h"
#include "global-keys.h"
#include "xstuff.h"
#include "multiscreen-stuff.h"
#include "conditional.h"

/*#define PANEL_DEBUG 1*/

int config_sync_timeout = 0;
int applets_to_sync = FALSE;
int panels_to_sync = FALSE;
int need_complete_save = FALSE;

extern GSList *panels;
extern GSList *applets;
extern GSList *applets_last;
extern int applet_count;

extern GtkTooltips *panel_tooltips;

extern GnomeClient *client;

gboolean commie_mode = FALSE;
gboolean no_run_box = FALSE;

GlobalConfig global_config;

char *panel_cfg_path = NULL;
char *old_panel_cfg_path = NULL;

/*list of all panel widgets created*/
extern GSList *panel_list;

extern char *kde_menudir;
extern char *kde_icondir;
extern char *kde_mini_icondir;

int ss_cur_applet = 0;
gboolean ss_done_save = FALSE;
gushort ss_cookie = 0;
GtkWidget *ss_timeout_dlg = NULL;
static gboolean ss_interactive = FALSE;
static int ss_timeout = 500;

/*send the tooltips state to all external applets*/
static void
send_tooltips_state(gboolean enabled)
{
	GSList *li;

	for(li = applets; li != NULL; li = li->next) {
		AppletInfo *info = li->data;
		if (info->type == APPLET_EXTERN) {
			Extern *ext = info->data;
			g_assert(ext != NULL);
			/*if it's not set yet, don't send it, it will be sent
			  when the ior is discovered anyhow, so this would be
			  redundant anyway*/
			if (ext->applet != NULL) {
				CORBA_Environment ev;
				CORBA_exception_init(&ev);
				GNOME_Applet_set_tooltips_state(ext->applet,
								enabled, &ev);
				if(ev._major)
					panel_clean_applet(ext->info);
				CORBA_exception_free(&ev);
			}
		}
	}
}

void
apply_global_config (void)
{
	int i;
	static int dot_buttons_old = 0; /*doesn't matter first time this is
					  done there are no menu applets*/
	static int menu_titles_old = 0; /*doesn't matter first time this is
					  done there are no menu applets*/
	static int keep_bottom_old = -1;
	static int normal_layer_old = -1;
	static int autohide_size_old = -1;
	static int menu_flags_old = -1;
	static int old_use_large_icons = -1;
	static int old_merge_menus = -1;
	static int old_menu_check = -1;
	static int old_fast_button_scaling = -1;
	static int old_avoid_collisions = -1;
	GSList *li;

	panel_widget_change_global (global_config.explicit_hide_step_size,
				    global_config.auto_hide_step_size,
				    global_config.drawer_step_size,
				    global_config.minimized_size,
				    global_config.minimize_delay,
				    global_config.maximize_delay,
				    global_config.movement_type,
				    global_config.disable_animations,
				    global_config.applet_padding,
				    global_config.applet_border_padding);

	if (global_config.tooltips_enabled)
		gtk_tooltips_enable (panel_tooltips);
	else
		gtk_tooltips_disable (panel_tooltips);
	/* not incredibly efficent way to do this, we just make
	 * sure that all directories are reread */
	if (old_merge_menus != global_config.merge_menus ||
	    old_menu_check != global_config.menu_check) {
		fr_force_reread();
	}
	/*if we changed dot_buttons/small_icons mark all menus as dirty
	  for rereading, hopefullly the user doesn't do this too often
	  so that he doesn't have to reread his menus all the time:)*/
	if(menu_titles_old != global_config.show_menu_titles ||
	   dot_buttons_old != global_config.show_dot_buttons ||
	   old_use_large_icons != global_config.use_large_icons ||
	   old_merge_menus != global_config.merge_menus ||
	   old_menu_check != global_config.menu_check) {
		GSList *li;
		for(li = applets; li != NULL; li = g_slist_next(li)) {
			AppletInfo *info = li->data;
			if(info->menu != NULL) {
				gtk_widget_unref(info->menu);
				info->menu = NULL;
				info->menu_age = 0;
			}
			if(info->type == APPLET_MENU) {
				Menu *menu = info->data;
				if(menu->menu != NULL) {
					gtk_widget_unref(menu->menu);
					menu->menu = NULL;
					menu->age = 0;
				}
			}
		}
		for(li = panel_list; li != NULL; li = g_slist_next(li)) {
			PanelData *pd = li->data;
			if(pd->menu != NULL) {
				gtk_widget_unref(pd->menu);
				pd->menu = NULL;
				pd->menu_age = 0;
			}
		}
		foobar_widget_force_menu_remake();
	}
	menu_titles_old = global_config.show_menu_titles;
	dot_buttons_old = global_config.show_dot_buttons;
	old_use_large_icons = global_config.use_large_icons;
	old_merge_menus = global_config.merge_menus;
	old_menu_check = global_config.menu_check;
	send_tooltips_state(global_config.tooltips_enabled);

	/* if we changed global menu flags, cmark all main menus that use
	 * the global setting as dirty */
	if(menu_flags_old != global_config.menu_flags) {
		GSList *li;
		for(li = applets; li != NULL; li = g_slist_next(li)) {
			AppletInfo *info = li->data;
			if(info->type == APPLET_MENU) {
				Menu *menu = info->data;
				if(menu->menu != NULL &&
				   menu->global_main) {
					gtk_widget_unref(menu->menu);
					menu->menu = NULL;
					menu->age = 0;
				}
			}
		}
	}

	if (keep_bottom_old == -1 ||
	    keep_bottom_old != global_config.keep_bottom ||
	    normal_layer_old == -1 ||
	    normal_layer_old != global_config.normal_layer) {
		for (li = panel_list; li != NULL; li = li->next) {
			PanelData *pd = li->data;
			if ( ! GTK_WIDGET_REALIZED (pd->panel))
				continue;
			if (IS_BASEP_WIDGET (pd->panel))
				basep_widget_update_winhints (BASEP_WIDGET (pd->panel));
			else if (IS_FOOBAR_WIDGET (pd->panel))
				foobar_widget_update_winhints (FOOBAR_WIDGET (pd->panel));
		}
		panel_reset_dialog_layers ();
	}
	keep_bottom_old = global_config.keep_bottom;
	normal_layer_old = global_config.normal_layer;

	for (i = 0; i < LAST_TILE; i++) {
		button_widget_set_flags (i, global_config.tiles_enabled[i],
					 1, 0);
		button_widget_load_tile (i, global_config.tile_up[i],
					 global_config.tile_down[i],
					 global_config.tile_border[i],
					 global_config.tile_depth[i]);
	}

	if (old_fast_button_scaling != global_config.fast_button_scaling) {
		button_widget_redo_all ();
	}
	old_fast_button_scaling = global_config.fast_button_scaling;

	for (li = panel_list; li != NULL; li = li->next) {
		PanelData *pd = li->data;
		if (IS_BASEP_WIDGET (pd->panel)) {
			if ((autohide_size_old != global_config.minimized_size) &&
			    (BASEP_WIDGET (pd->panel)->state == 
			     BASEP_AUTO_HIDDEN)) {
				gtk_widget_queue_resize (GTK_WIDGET (pd->panel));
			}
			basep_update_frame (BASEP_WIDGET (pd->panel));

			if ((menu_flags_old != global_config.menu_flags) &&
			    pd->menu) {
				gtk_widget_unref (pd->menu);
				pd->menu = NULL;
				pd->menu_age = 0;
			}
				
		}
	}
	autohide_size_old = global_config.minimized_size;
	menu_flags_old = global_config.menu_flags;

	if (old_avoid_collisions != global_config.avoid_collisions) {
		int i;
		for (i = 0; i < multiscreen_screens (); i++)
			basep_border_queue_recalc (i);
	}
	old_avoid_collisions = global_config.avoid_collisions;

	panel_global_keys_setup();
}

static void
timeout_dlg_realized (GtkWidget *dialog)
{
	/* always make top layer */
	gnome_win_hints_set_layer (GTK_WIDGET(dialog),
				   WIN_LAYER_ABOVE_DOCK);
}

static gboolean
session_save_timeout (gpointer data)
{
	int cookie = GPOINTER_TO_INT (data);
	if (cookie != ss_cookie)
		return FALSE;

#ifdef PANEL_DEBUG	
	printf("SAVE TIMEOUT (%u)\n",ss_cookie);
#endif
	if ( ! ss_interactive) {
		ss_cookie ++;
		return FALSE;
	}

	ss_timeout_dlg =
		gnome_message_box_new(_("An applet is not "
					"responding to a "
					"save request.\n"
					"Remove the applet "
					"or continue waiting?"),
				      GNOME_MESSAGE_BOX_WARNING,
				      NULL);
	gtk_signal_connect(GTK_OBJECT(ss_timeout_dlg),"destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			   &ss_timeout_dlg);
	gnome_dialog_append_button_with_pixmap (GNOME_DIALOG (ss_timeout_dlg),
						_("Remove applet"),
						GNOME_STOCK_PIXMAP_TRASH);
	gnome_dialog_append_button_with_pixmap (GNOME_DIALOG (ss_timeout_dlg),
						_("Continue waiting"),
						GNOME_STOCK_PIXMAP_TIMER);

	gtk_signal_connect_after(GTK_OBJECT(ss_timeout_dlg), "realize",
				 GTK_SIGNAL_FUNC(timeout_dlg_realized),
				 NULL);
	
	if (0 == gnome_dialog_run_and_close (GNOME_DIALOG (ss_timeout_dlg))) {
		ss_cookie++;
		g_warning(_("Timed out on sending session save to an applet"));
		save_next_applet();
		return FALSE;
	}
	return TRUE;
}

static void
send_applet_session_save (AppletInfo *info,
			  CORBA_Object obj,
			  const char *cfgpath,
			  const char *globcfgpath)
{
	guint timeout;
	CORBA_Environment ev;
	
	/*new unique cookie*/
	ss_cookie++;
	
#ifdef PANEL_DEBUG	
	printf("SENDING_SESSION_SAVE (%u)\n", ss_cookie);
#endif

	timeout = gtk_timeout_add(ss_timeout, session_save_timeout,
				  GINT_TO_POINTER((int)ss_cookie));

	CORBA_exception_init(&ev);
	GNOME_Applet_save_session(obj,
				  (CORBA_char *)cfgpath,
				  (CORBA_char *)globcfgpath,
				  ss_cookie, &ev);
	if(ev._major == CORBA_SYSTEM_EXCEPTION) {
		CORBA_SystemException *exc =
			CORBA_exception_value(&ev);
		if(exc &&
		   (exc->minor == ex_CORBA_BAD_OPERATION ||
		    exc->minor == ex_CORBA_NO_IMPLEMENT)) {
			gboolean ret;
			gtk_timeout_remove(timeout);
			CORBA_exception_free(&ev);
			CORBA_exception_init(&ev);
			ret = GNOME_Applet_session_save(obj,
						(CORBA_char *)cfgpath,
						(CORBA_char *)globcfgpath,
						&ev);
			save_applet (info, ret);
		}
	} else if(ev._major) {
		gtk_timeout_remove(timeout);
		panel_clean_applet(info);
		CORBA_exception_free(&ev);
		save_next_applet();
		return;
	}

	CORBA_exception_free(&ev);
}




/*returns TRUE if the save was completed, FALSE if we need to wait
  for the applet to respond*/
static gboolean
save_applet_configuration(AppletInfo *info)
{
	GString       *buf;
	int            panel_num;
	int            panel_id;
	PanelWidget   *panel;
	AppletData    *ad;
	
	g_return_val_if_fail(info!=NULL,TRUE);

	buf = g_string_new(NULL);

	gnome_config_push_prefix("");
	g_string_sprintf(buf, "%sApplet_Config/Applet_%d", PANEL_CONFIG_PATH,
			 info->applet_id+1);
	gnome_config_clean_section(buf->str);
	gnome_config_pop_prefix();

	g_string_sprintf(buf, "%sApplet_Config/Applet_%d/", PANEL_CONFIG_PATH,
			 info->applet_id+1);
	gnome_config_push_prefix(buf->str);

	/*obviously no need for saving*/
	if (info->type == APPLET_EXTERN_PENDING ||
	    info->type == APPLET_EXTERN_RESERVED ||
	    info->type == APPLET_EMPTY) {
		gnome_config_set_string("id", EMPTY_ID);
		gnome_config_pop_prefix();
		g_string_free(buf,TRUE);
		return TRUE;
	}

	panel = PANEL_WIDGET(info->widget->parent);
	ad = gtk_object_get_data(GTK_OBJECT(info->widget),PANEL_APPLET_DATA);

	panel_num = g_slist_index (panels, panel);
	if (panel_num == -1) {
		gnome_config_set_string("id", EMPTY_ID);
		gnome_config_pop_prefix();
		g_string_free(buf,TRUE);
		return TRUE;
	}
	panel_id = panel->unique_id;

	switch(info->type) {
	case APPLET_EXTERN:
		{
			char *s;
			Extern *ext = info->data;

			/*just in case the applet times out*/
			gnome_config_set_string("id", EMPTY_ID);
			gnome_config_pop_prefix();

			/*this is the file path we must kill first */
			g_string_sprintf(buf, "%sApplet_%d_Extern",
					 PANEL_CONFIG_PATH, info->applet_id+1);
			
			/* this should really be: */
			/*gnome_config_push_prefix("");
			gnome_config_clean_file(buf->str);
			gnome_config_pop_prefix();
			gnome_config_sync();*/
			/* but gnome-config.[ch] is broken ! */
			s = gnome_config_get_real_path (buf->str);
			unlink(s);
			g_free(s);

			gnome_config_sync();

			/*this is the file path we pass to the applet for it's
			  own config, this is a separate file, so that we */
			g_string_sprintf(buf, "%sApplet_%d_Extern/",
					 PANEL_CONFIG_PATH, info->applet_id+1);
			/*have the applet do it's own session saving*/
			send_applet_session_save(info, ext->applet,
						 buf->str,
						 PANEL_CONFIG_PATH
						 "Applet_All_Extern/");
			/* update the configuration string */
			g_free (ext->cfg);
			ext->cfg = g_strdup (buf->str);
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
			gnome_config_set_int ("unique_drawer_panel_id", 
					      PANEL_WIDGET (BASEP_WIDGET(drawer->drawer)->panel)->unique_id);
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
			gnome_config_set_bool("global_main",
					      menu->global_main);
			gnome_config_set_bool("old_style_main", FALSE);
			gnome_config_set_bool("custom_icon",
					      menu->custom_icon);
			gnome_config_set_string("custom_icon_file",
						menu->custom_icon_file);
			break;
		}
	case APPLET_LAUNCHER:
		{
			Launcher *launcher = info->data;

			gnome_config_set_string ("id", LAUNCHER_ID);

			/* clean old launcher info */
			gnome_config_clean_key ("parameters");

			launcher_save (launcher);
			gnome_config_set_string ("base_location",
						 g_basename (launcher->dentry->location));
			break;
		}
	case APPLET_LOGOUT:
		gnome_config_set_string("id", LOGOUT_ID);
		break;
	case APPLET_LOCK:
		gnome_config_set_string("id", LOCK_ID);
		break;
	case APPLET_STATUS:
		gnome_config_set_string("id", STATUS_ID);
		break;
	case APPLET_RUN:
		gnome_config_set_string("id", RUN_ID);
		break;
	default:
		g_warning ("Unknown applet type encountered: %d; ignoring.",
			   info->type);
		break;
	}
	gnome_config_set_int("position", ad->pos);
	gnome_config_set_int("panel", panel_num);
	gnome_config_set_int("unique_panel_id", panel_id);
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
	BasePWidget   *basep = NULL; 
	PanelWidget   *panel = NULL;

	if (IS_BASEP_WIDGET (pd->panel)) {
		basep = BASEP_WIDGET (pd->panel);
		panel = PANEL_WIDGET (basep->panel);
	} else if (IS_FOOBAR_WIDGET (pd->panel)) {
		panel = PANEL_WIDGET (FOOBAR_WIDGET(pd->panel)->panel);
	}

	buf = g_string_new(NULL);

	gnome_config_push_prefix ("");
	g_string_sprintf(buf, "%spanel/Panel_%d", PANEL_CONFIG_PATH, *num);
	gnome_config_clean_section(buf->str);
	gnome_config_pop_prefix ();

	g_string_sprintf(buf, "%spanel/Panel_%d/", PANEL_CONFIG_PATH, *num);

	(*num)++;

	gnome_config_push_prefix (buf->str);

	gnome_config_set_int("type",pd->type);

	if (basep != NULL) {
		gnome_config_set_bool("hidebuttons_enabled",
				      basep->hidebuttons_enabled);
		gnome_config_set_bool("hidebutton_pixmaps_enabled",
				      basep->hidebutton_pixmaps_enabled);
	
		gnome_config_set_int ("mode", basep->mode);
		gnome_config_set_int ("state", basep->state);

		gnome_config_set_int ("level", basep->level);
		gnome_config_set_bool ("avoid_on_maximize",
				       basep->avoid_on_maximize);
		gnome_config_set_int ("screen",
				      basep->screen);
	}

	gnome_config_set_int("unique_id", panel->unique_id);

	gnome_config_set_int("sz", panel->sz);

	gnome_config_set_bool("fit_pixmap_bg", panel->fit_pixmap_bg);
	gnome_config_set_bool("strech_pixmap_bg", panel->strech_pixmap_bg);
	gnome_config_set_bool("rotate_pixmap_bg", panel->rotate_pixmap_bg);

	gnome_config_set_string ("backpixmap",
				 sure_string (panel->back_pixmap));

	g_string_sprintf(buf, "#%02x%02x%02x",
			 (guint)panel->back_color.red/256,
			 (guint)panel->back_color.green/256,
			 (guint)panel->back_color.blue/256);
	gnome_config_set_string("backcolor", buf->str);

	gnome_config_set_int("back_type", panel->back_type);
	
	g_string_free(buf,TRUE);

	/* now do different types */
	if (IS_BORDER_WIDGET(pd->panel))
		gnome_config_set_int("edge", BORDER_POS(basep->pos)->edge);

	switch (pd->type) {
	case ALIGNED_PANEL:
		gnome_config_set_int ("align", ALIGNED_POS (basep->pos)->align);
		break;
	case SLIDING_PANEL:
		gnome_config_set_int ("offset", SLIDING_POS (basep->pos)->offset);
		gnome_config_set_int ("anchor", SLIDING_POS (basep->pos)->anchor);
		break;
	case FLOATING_PANEL:
		gnome_config_set_int ("orient", PANEL_WIDGET (basep->panel)->orient);
		gnome_config_set_int ("x", FLOATING_POS (basep->pos)->x);
		gnome_config_set_int ("y", FLOATING_POS (basep->pos)->y);
		break;
	case DRAWER_PANEL:
		gnome_config_set_int ("orient", DRAWER_POS (basep->pos)->orient);
		/*gnome_config_set_int ("temp_hidden", DRAWER_POS (basep->pos)->temp_state);*/
		break;
	case FOOBAR_PANEL:
		gnome_config_set_string ("/panel/Config/clock_format", FOOBAR_WIDGET (pd->panel)->clock_format);
		gnome_config_set_int ("screen", FOOBAR_WIDGET (pd->panel)->screen);
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
		gboolean complete_sync,
		gboolean sync_applets,
		gboolean sync_panels)
{
	int num;
#if PER_SESSION_CONFIGURATION
	gchar *new_args[] = { "rm", "-r", NULL };
#endif /* PER_SESSION_CONFIGURATION */

	/* If in commie mode, then no saving is needed, the user
	 * could have changed anything anyway */
	if (commie_mode)
		return;

	if (panel_cfg_path)
		g_free (panel_cfg_path);

#ifdef PER_SESSION_CONFIGURATION
	if (gnome_client_get_flags(client) & GNOME_CLIENT_IS_CONNECTED &&
	    GNOME_CLIENT (client)->restart_style != GNOME_RESTART_NEVER)
		panel_cfg_path = g_strdup (gnome_client_get_config_prefix (client));
	else
#endif /* PER_SESSION_CONFIGURATION */		
		panel_cfg_path = g_strdup ("/panel.d/default/");

#ifdef PER_SESSION_CONFIGURATION
	new_args[2] = gnome_config_get_real_path (panel_cfg_path);
	gnome_client_set_discard_command (client, 3, new_args);
#endif /* PER_SESSION_CONFIGURATION */

#ifdef PANEL_DEBUG	
	printf("Saving to [%s]\n",PANEL_CONFIG_PATH);

	printf("Saving session: 1"); fflush(stdout);

	printf(" 2"); fflush(stdout);
#endif

	gnome_config_push_prefix (PANEL_CONFIG_PATH "panel/Config/");

	if(complete_sync || sync_applets)
		gnome_config_set_int ("applet_count", applet_count);
#ifdef PANEL_DEBUG
	printf(" 3"); fflush(stdout);
#endif
	if(complete_sync || sync_panels) {
		num = 1;
		g_slist_foreach(panel_list, save_panel_configuration, &num);
		gnome_config_set_int("panel_count",num-1);
	}
#ifdef PANEL_DEBUG
	printf(" 4\n"); fflush(stdout);
#endif
	gnome_config_pop_prefix ();

	if(complete_sync)
		save_tornoff();

	gnome_config_sync();
	
	if(complete_sync || sync_applets) {
		ss_cur_applet = -1;
		ss_done_save = FALSE;

		/* kill removed launcher files */
		remove_unused_launchers ();

		/* start saving applets */
		save_next_applet ();
	}


#if 0 /*PANEL_DEBUG*/
	puts("");
#endif
}

static guint sync_handler = 0;
static gboolean sync_handler_needed = FALSE;

void
panel_config_sync(void)
{
	int ncs = need_complete_save;
	int ats = applets_to_sync;
	int pts = panels_to_sync;

	if (sync_handler != 0) {
		gtk_timeout_remove (sync_handler);
		sync_handler = 0;
	}

	sync_handler_needed = FALSE;

	if(need_complete_save ||
	   applets_to_sync ||
	   panels_to_sync) {
#ifdef PER_SESSION_CONFIGURATION
		if (!(gnome_client_get_flags(client) & 
		      GNOME_CLIENT_IS_CONNECTED)) {
#endif
			need_complete_save = FALSE;
			applets_to_sync = FALSE;
			panels_to_sync = FALSE;
			do_session_save (client, ncs, ats, pts);
#ifdef PER_SESSION_CONFIGURATION
		} else {
			/*prevent possible races by doing this before requesting
			  save*/
			need_complete_save = FALSE;
			applets_to_sync = FALSE;
			panels_to_sync = FALSE;
			gnome_client_request_save (client, GNOME_SAVE_LOCAL, FALSE,
						   GNOME_INTERACT_NONE, FALSE, FALSE);
		}
#endif /* PER_SESSION_CONFIGURATION */
	}
}

static gboolean
sync_handler_timeout (gpointer data)
{
	sync_handler = 0;

	if (sync_handler_needed)
		panel_config_sync ();

	return FALSE;
}

void
panel_config_sync_schedule (void)
{
	if (sync_handler == 0) {
		/* don't sync for another 30 secs */
		sync_handler = gtk_timeout_add (30000, sync_handler_timeout, NULL);
		sync_handler_needed = FALSE;
		panel_config_sync ();
	} else {
		/* a timeout is running */
		sync_handler_needed = TRUE;
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
	if (is_shutdown) {
		ss_timeout = 1500;
		ss_interactive = TRUE;
	}
	do_session_save(client,TRUE,FALSE,FALSE);
	if (is_shutdown) {
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

	gtk_timeout_remove (config_sync_timeout);
	config_sync_timeout = 0;
  
	status_inhibit = TRUE;
	status_spot_remove_all ();

	for (li = applets; li != NULL; li = li->next) {
		AppletInfo *info = li->data;
		if(info->type == APPLET_EXTERN) {
			Extern *ext = info->data;
			/* save but don't sync, we do that after
			 * for everything */
			extern_save_last_position (ext, FALSE /* sync */);
			ext->clean_remove = TRUE;
			gtk_widget_destroy (info->widget);
		} else if(info->type == APPLET_SWALLOW) {
			Swallow *swallow = info->data;
			swallow->clean_remove = TRUE;
			if(GTK_SOCKET(swallow->socket)->plug_window)
				XKillClient(GDK_DISPLAY(),
					    GDK_WINDOW_XWINDOW(GTK_SOCKET(swallow->socket)->plug_window));
		}
	}

	gnome_config_sync ();

	xstuff_unsetup_desktop_area ();
			
	/*clean up corba stuff*/
	panel_corba_clean_up();
	
	gtk_main_quit();
	return TRUE;
}

/* the logout function */
void
panel_quit (void)
{
	gnome_client_request_save (client, GNOME_SAVE_BOTH, 1,
				   GNOME_INTERACT_ANY, 0, 1);
}

/* try evil hacks to rewrite panel config from old applets (gnomepager for
 * now.  This is very evil.  But that's the only way to do it, it seems */
static gboolean
try_evil_config_hacks (const char *goad_id, PanelWidget *panel, int pos)
{
	gboolean ret = FALSE;

	if (strcmp (goad_id, "gnomepager_applet") == 0) {
		static gboolean first_time = TRUE;
		static gboolean in_path = FALSE;

		if(first_time) {
			char *tmp;
			tmp = panel_is_program_in_path("gnomepager_applet");
			in_path = tmp != NULL;
			first_time = FALSE;
			g_free(tmp);
		}

		if(!in_path) {
			load_extern_applet("deskguide_applet", NULL,
					   panel, pos, TRUE, TRUE);

			load_extern_applet("tasklist_applet", NULL,
					   panel, pos+1, TRUE, TRUE);
			ret = TRUE;
		}
	}
	return ret;
}

char *
get_correct_prefix (char const **sep)
{
	int count;
	char *path;

	gnome_config_push_prefix ("");

	/* if we're being commies, we just use the global path */
	if (commie_mode) {
		gnome_config_pop_prefix ();
		*sep = "=";
		return g_strdup_printf ("=%s/panel.d/default/", GLOBAL_CONFDIR);
	}

	count = conditional_get_int (PANEL_CONFIG_PATH
				     "panel/Config/panel_count", 0, NULL);
	if (count > 0) {
		gnome_config_pop_prefix ();
		*sep = "";
		return g_strdup (PANEL_CONFIG_PATH);
	}

	path = g_strdup_printf ("=%s/panel.d/default/panel=/Config/panel_count",
				GLOBAL_CONFDIR);
	count = conditional_get_int (path, 0, NULL);
	g_free (path);

	if (count > 0) {
		gnome_config_pop_prefix ();
		*sep = "=";
		return g_strdup_printf ("=%s/panel.d/default/", GLOBAL_CONFDIR);
	}

	gnome_config_pop_prefix ();

	/* Eeeeek! return to standard home path and load some slightly sane
	 * setup */
	*sep = "";
	return g_strdup (PANEL_CONFIG_PATH);
}

void
push_correct_global_prefix (void)
{
	if ( ! commie_mode) {
		gboolean foo, def;

		gnome_config_push_prefix ("/panel/Config/");

		foo = conditional_get_bool ("tooltips_enabled", TRUE, &def);
		if ( ! def)
			return;
	}

	/* ahhh, this doesn't exist, but tooltips_enabled should be
	 * in every home, every kitchen and every panel configuration,
	 * so we will load up from the global location */
	gnome_config_pop_prefix ();
	gnome_config_push_prefix ("=" GLOBAL_CONFDIR "/panel=/Config/");
}

void
init_user_applets(void)
{
	GString *buf;
	int count, num;	
	DistributionType distribution = get_distribution_type ();
	char *prefix;
	const char *sep;

	prefix = get_correct_prefix (&sep);

	buf = g_string_new (NULL);

	g_string_sprintf (buf, "%spanel%s/Config/applet_count",
			  prefix, sep);
	count = conditional_get_int (buf->str, 0, NULL);
	for (num = 1; num <= count; num++) {
		char *applet_name;
		int   pos = 0, panel_num, panel_id;
		PanelWidget *panel;

		g_string_sprintf (buf, "%sApplet_Config%s/Applet_%d/",
				  prefix, sep, num);
		gnome_config_push_prefix (buf->str);

		if ( ! conditional_true ("Conditional")) {
			gnome_config_pop_prefix ();
			continue;
		}

		applet_name = conditional_get_string ("id", "Unknown", NULL);
		
		if (strcmp (applet_name, EMPTY_ID) == 0) {
			g_free (applet_name);
			gnome_config_pop_prefix ();
			continue;
		} else if (strcmp (applet_name, "Unknown") == 0) {
			g_warning ("Unknown applet type!");
			g_free (applet_name);
			gnome_config_pop_prefix ();
			continue;
		}

		pos = conditional_get_int ("position", 0, NULL);
		panel_id = conditional_get_int ("unique_panel_id", -1, NULL);
		if (panel_id < 0) {
			GSList *list;

			panel_num = conditional_get_int ("panel", 0, NULL);

			list = g_slist_nth (panels, panel_num);
			if (list == NULL) {
				g_warning ("Can't find panel, "
					   "putting applet on the first one");
				panel = panels->data;
			} else
				panel = list->data;
		} else {
			panel = panel_widget_get_by_id (panel_id);
			if (panel == NULL) {
				g_warning ("Can't find panel, "
					   "putting applet on the first one");
				panel = panels->data;
			}
		}

		/* if this isn't true, then we are smoking some serious crack */
		g_assert (panel != NULL);
		
		/*if we are to right stick this, make the number large, 
		 G_MAXINT/2 should allways be large enough */
		pos += (conditional_get_bool ("right_stick", FALSE, NULL)
			? G_MAXINT/2 : 0);
		
		if (strcmp (applet_name, EXTERN_ID) == 0) {
			char *goad_id = conditional_get_string ("goad_id",
								NULL, NULL);
			if ( ! string_empty (goad_id) &&
			    /* if we try an evil hack such as loading tasklist
			     * and deskguide instead of the desguide don't
			     * try to load this applet in the first place */
			    ! try_evil_config_hacks (goad_id, panel, pos)) {
				/*this is the config path to be passed to the
				  applet when it loads*/
				g_string_sprintf (buf, "%sApplet_%d_Extern/",
						  PANEL_CONFIG_PATH, num);
				load_extern_applet (goad_id, buf->str, 
						    panel, pos, TRUE, TRUE);
			}
			g_free (goad_id);

		} else if (strcmp (applet_name, LAUNCHER_ID) == 0) { 
			gboolean hoard = FALSE;
			Launcher *launcher;
			char *file;

			file = conditional_get_string ("base_location", NULL,
						       NULL);
			if (string_empty (file)) {
				g_free (file);
				file = conditional_get_string ("parameters",
							       NULL, NULL);
				hoard = TRUE;
			} else {
				char *tmp = launcher_file_name (file);
				g_free (file);
				file = tmp;
			}

			launcher = load_launcher_applet(file, panel, pos, TRUE);
			g_free(file);

			/* If this was an old style launcher, hoard it now */
			if (hoard && launcher != NULL)
				launcher_hoard (launcher);

		} else if (strcmp (applet_name, LOGOUT_ID) == 0) { 
			load_logout_applet (panel, pos, TRUE);

		} else if (strcmp (applet_name, LOCK_ID) == 0) {
			load_lock_applet (panel, pos, TRUE);

		} else if (strcmp (applet_name, STATUS_ID) == 0) {
			load_status_applet (panel, pos, TRUE);
			
		} else if (strcmp (applet_name, RUN_ID) == 0) {
			load_run_applet (panel, pos, TRUE);

		} else if (strcmp (applet_name, SWALLOW_ID) == 0) {
			char *path = conditional_get_string ("execpath",
							     NULL, NULL);
			char *params = conditional_get_string ("parameters",
							       NULL, NULL);
			int width = conditional_get_int ("width", 0, NULL);
			int height = conditional_get_int ("height", 0, NULL);
			load_swallow_applet (path, params, width, height,
					     panel, pos, TRUE);
			g_free (path);
			g_free (params);

		} else if (strcmp (applet_name, MENU_ID) == 0) {
			char *params = conditional_get_string ("parameters",
							       ".", NULL);
			int type = conditional_get_int ("main_menu_type", -1,
							NULL);
			/* this defaults to false, because we want old menus to
			 * work in the old style */
			gboolean global_main_was_default;
			gboolean global_main =
				conditional_get_bool ("global_main", FALSE,
						      &global_main_was_default);
			int flags;
			gboolean old_style = 
				conditional_get_bool ("old_style_main", TRUE,
						      NULL);
			gboolean custom_icon = 
				conditional_get_bool ("custom_icon", FALSE,
						      NULL);
			char *custom_icon_file =
				conditional_get_string ("custom_icon_file",
							NULL, NULL);

			flags = conditional_get_int ("main_menu_flags",
						     get_default_menu_flags (),
						     NULL);
			if (flags < 0)
				flags = get_default_menu_flags ();
			
			/* Hack to try to do the right conversion while trying
			 * to turn on the feature on as many setups as
			 * possible */
			if (global_main_was_default &&
			    flags == global_config.menu_flags) {
				global_main = TRUE;
			}

			if(type >= 0) {
				flags = 0;
				if(type == X_MAIN_MENU_BOTH) {
					flags |= MAIN_MENU_SYSTEM|MAIN_MENU_USER;
				} else if(type == X_MAIN_MENU_SYSTEM) {
					flags |= MAIN_MENU_SYSTEM|
						MAIN_MENU_USER_SUB;
				} else {
					flags |= MAIN_MENU_SYSTEM_SUB|
						MAIN_MENU_USER;
				}
				/*guess distribution menus*/
				if(distribution != DISTRIBUTION_UNKNOWN)
					flags |= MAIN_MENU_DISTRIBUTION_SUB;
				/*guess KDE menus */
				if(panel_file_exists(kde_menudir))
					flags |= MAIN_MENU_KDE_SUB;
			}
			if(old_style) {
				/*this is needed to make panel properly
				  read older style configs */
				if(flags&MAIN_MENU_SYSTEM &&
				   flags&MAIN_MENU_SYSTEM_SUB)
					flags &=~ MAIN_MENU_SYSTEM;
				if(flags&MAIN_MENU_USER &&
				   flags&MAIN_MENU_USER_SUB)
					flags &=~ MAIN_MENU_USER;
				if(flags&MAIN_MENU_DISTRIBUTION &&
				   flags&MAIN_MENU_DISTRIBUTION_SUB)
					flags &=~ MAIN_MENU_DISTRIBUTION;
				/*keep this for compatibility with older
				  config files */
				if(flags&MAIN_MENU_OBSOLETE_DEBIAN &&
				   flags&MAIN_MENU_OBSOLETE_DEBIAN_SUB)
					flags &=~ MAIN_MENU_DISTRIBUTION;
				if(flags&MAIN_MENU_KDE &&
				   flags&MAIN_MENU_KDE_SUB)
					flags &=~ MAIN_MENU_KDE;
				flags |= MAIN_MENU_APPLETS_SUB |
					MAIN_MENU_PANEL_SUB |
					MAIN_MENU_DESKTOP;
			}

			if (old_style)
				load_menu_applet(params, flags, FALSE,
						 custom_icon, custom_icon_file,
						 panel, pos, TRUE);
			else
				load_menu_applet(params, flags, global_main,
						 custom_icon, custom_icon_file,
						 panel, pos, TRUE);

			g_free (custom_icon_file);
			g_free (params);

		} else if (strcmp (applet_name, DRAWER_ID) == 0) {
			int mypanel = conditional_get_int ("parameters", -1,
							   NULL);
			int mypanel_id = conditional_get_int
				("unique_drawer_panel_id", -1, NULL);
			char *pixmap = conditional_get_string ("pixmap",
							       NULL, NULL);
			char *tooltip = conditional_get_string ("tooltip",
								NULL, NULL);
			if (mypanel_id < 0 && mypanel >= 0) {
				PanelWidget *pw = g_slist_nth_data (panels, mypanel);
				if (pw != NULL)
					mypanel_id = pw->unique_id;
			}
			load_drawer_applet (mypanel_id, pixmap, tooltip,
					    panel, pos, TRUE);
			g_free (pixmap);
			g_free (tooltip);
		} else {
			g_warning ("Unknown applet type!");
		}
		gnome_config_pop_prefix ();
		g_free (applet_name);
	}
	g_string_free (buf, TRUE);
	g_free (prefix);
}

void
init_user_panels(void)
{
	GString *buf;
	int count, num;	
	char *s;
	GtkWidget *panel = NULL;
	char *prefix;
	const char *sep;

	prefix = get_correct_prefix (&sep);

	buf = g_string_new (NULL);

	g_string_sprintf (buf, "%spanel%s/Config/panel_count",
			  prefix, sep);

	count = conditional_get_int (buf->str, 0, NULL);

	/*load a default snapped panel on the bottom of the screen,
	  it is required to have at least one panel for this all
	  to work, so this is the way we find out if there was no
	  config from last time*/
	if (count <= 0)  {
		/* Eeeeeeeek! no default config, no user config, this is
		 * bad bad bad, load a single panel */
		panel = edge_widget_new (0 /* screen */,
					 BORDER_BOTTOM,
					 BASEP_EXPLICIT_HIDE /* mode */,
					 BASEP_SHOWN /* state */,
					 BASEP_LEVEL_DEFAULT /* level */,
					 TRUE /* avoid_on_maximize */,
					 SIZE_STANDARD /* size */,
					 TRUE /* hidebuttons_enabled */,
					 TRUE /* hidebutton_pixmaps */,
					 PANEL_BACK_NONE /* back type */,
					 NULL,
					 TRUE,
					 FALSE,
					 TRUE,
					 NULL);
		panel_setup(panel);
		gtk_widget_show(panel);

		/* load up the foot menu */
		load_menu_applet(NULL, get_default_menu_flags (),
				 TRUE, FALSE, NULL,
				 PANEL_WIDGET(BASEP_WIDGET(panel)->panel),
				 0, TRUE);

		g_free (prefix);
		g_string_free (buf, TRUE);

		return;
	}

	for (num = 1; num <= count; num++) {
		PanelType type;
		PanelBackType back_type;
		int sz;
		BasePState state;
		BasePMode mode;
		BasePLevel level;
		gboolean avoid_on_maximize;
		BorderEdge edge;
		char *back_pixmap, *color;
		GdkColor back_color = {0,0,0,1};
		gboolean fit_pixmap_bg;
		gboolean strech_pixmap_bg;
		gboolean rotate_pixmap_bg;
		int hidebuttons_enabled;
		int hidebutton_pixmaps_enabled;
		int unique_id;
		int screen;

		g_string_sprintf (buf, "%spanel%s/Panel_%d/",
				  prefix, sep, num);
		gnome_config_push_prefix (buf->str);

		if ( ! conditional_true ("Conditional")) {
			gnome_config_pop_prefix ();
			continue;
		}

		unique_id = conditional_get_int ("unique_id", -1, NULL);
		
		back_pixmap = conditional_get_string ("backpixmap", NULL, NULL);
		if (string_empty (back_pixmap)) {
			g_free (back_pixmap);
			back_pixmap = NULL;
		}

		color = conditional_get_string ("backcolor", "#ffffff", NULL);
		if ( ! string_empty (color))
			gdk_color_parse (color, &back_color);

		back_type=conditional_get_int ("back_type",
					       PANEL_BACK_NONE, NULL);
		fit_pixmap_bg = conditional_get_bool ("fit_pixmap_bg",
						      TRUE, NULL);
		strech_pixmap_bg = conditional_get_bool ("strech_pixmap_bg",
							 FALSE, NULL);
		rotate_pixmap_bg = conditional_get_bool ("rotate_pixmap_bg",
							 FALSE, NULL);

		sz=conditional_get_int ("sz", SIZE_STANDARD, NULL);
		if(sz < 0)
			sz = 0;

		/*a hack to allow for old config files to be read correctly*/
		/*don't update this if new sizes (SIZE_SMALL) are added*/
		if(sz < 4) {
			switch(sz) {
			case 0: sz = SIZE_TINY; break;
			case 1: sz = SIZE_STANDARD; break;
			case 2: sz = SIZE_LARGE; break;
			case 3: sz = SIZE_HUGE; break;
			default: break;
			}
		}
		
		/*now for type specific config*/

		type = conditional_get_int ("type", EDGE_PANEL, NULL);

		hidebuttons_enabled =
			conditional_get_bool ("hidebuttons_enabled", TRUE, NULL);
		hidebutton_pixmaps_enabled =
			conditional_get_bool ("hidebutton_pixmaps_enabled", TRUE, NULL);

		state = conditional_get_int ("state", 0, NULL);
		mode = conditional_get_int ("mode", 0, NULL);
		level = conditional_get_int ("level", 0, NULL);
		screen = conditional_get_int ("screen", 0, NULL);
		if (screen < 0)
			screen = 0;
#if 0 /* i guess we can't easily do this for now */
		pos = basep_widget_load_pos_settings();
#endif
		switch (type) {
			
		case EDGE_PANEL:
			edge = conditional_get_int ("edge", BORDER_BOTTOM,
						    NULL);

			avoid_on_maximize = conditional_get_bool
				("avoid_on_maximize", TRUE, NULL);

			panel = edge_widget_new (screen,
						 edge, 
						 mode, state,
						 level, avoid_on_maximize,
						 sz,
						 hidebuttons_enabled,
						 hidebutton_pixmaps_enabled,
						 back_type, back_pixmap,
						 fit_pixmap_bg,
						 strech_pixmap_bg,
						 rotate_pixmap_bg,
						 &back_color);
			break;
		case ALIGNED_PANEL: {
			AlignedAlignment align;
			edge = conditional_get_int ("edge", BORDER_BOTTOM,
						    NULL);
			
			align = conditional_get_int ("align", ALIGNED_LEFT,
						     NULL);

			avoid_on_maximize = conditional_get_bool
				("avoid_on_maximize", TRUE, NULL);

			panel = aligned_widget_new (screen,
						    align,
						    edge,
						    mode,
						    state,
						    level,
						    avoid_on_maximize,
						    sz,
						    hidebuttons_enabled,
						    hidebutton_pixmaps_enabled,
						    back_type,
						    back_pixmap,
						    fit_pixmap_bg,
						    strech_pixmap_bg,
						    rotate_pixmap_bg,
						    &back_color);
			break;
		}
		case SLIDING_PANEL: {
			gint16 offset;
			SlidingAnchor anchor;
			edge = conditional_get_int ("edge", BORDER_BOTTOM,
						    NULL);
			
			anchor = conditional_get_int ("anchor",
						      SLIDING_ANCHOR_LEFT,
						      NULL);

			offset = conditional_get_int ("offset", 0, NULL);

			avoid_on_maximize = conditional_get_bool
				("avoid_on_maximize", TRUE, NULL);

			panel = sliding_widget_new (screen,
						    anchor,
						    offset,
						    edge,
						    mode,
						    state,
						    level,
						    avoid_on_maximize,
						    sz,
						    hidebuttons_enabled,
						    hidebutton_pixmaps_enabled,
						    back_type,
						    back_pixmap,
						    fit_pixmap_bg,
						    strech_pixmap_bg,
						    rotate_pixmap_bg,
						    &back_color);
			break;
		}
		case DRAWER_PANEL: {
			PanelOrientType orient;
			/*BasePState temp_state;*/

			orient = conditional_get_int ("orient", ORIENT_UP,
						      NULL);

			/* FIXME: there are some issues with auto hiding drawers */

			avoid_on_maximize = conditional_get_bool
				("avoid_on_maximize", FALSE, NULL);

			panel = drawer_widget_new (orient,
						   BASEP_EXPLICIT_HIDE, 
						   state,
						   level,
						   avoid_on_maximize,
						   sz,
						   hidebuttons_enabled,
						   hidebutton_pixmaps_enabled,
						   back_type,
						   back_pixmap,
						   fit_pixmap_bg,
						   strech_pixmap_bg,
						   rotate_pixmap_bg,
						   &back_color);
#if 0
			g_string_sprintf (buf, "temp_state=%d", BASEP_SHOWN);
			temp_state = conditional_get_int (buf->str);
			DRAWER_POS (BASEP_WIDGET (panel)->pos)->temp_state = temp_state;
#endif
			break;
		}
		case FLOATING_PANEL: {
			PanelOrientation orient;
			int x, y;
			
			orient = conditional_get_int ("orient",
						      PANEL_HORIZONTAL,
						      NULL);

			x = conditional_get_int ("x", 0, NULL);
			y = conditional_get_int ("y", 0, NULL);

			avoid_on_maximize = conditional_get_bool
				("avoid_on_maximize", FALSE, NULL);

			panel = floating_widget_new (screen,
						     x,
						     y,
						     orient,
						     mode,
						     state,
						     level,
						     avoid_on_maximize,
						     sz,
						     hidebuttons_enabled,
						     hidebutton_pixmaps_enabled,
						     back_type,
						     back_pixmap,
						     fit_pixmap_bg,
						     strech_pixmap_bg,
						     rotate_pixmap_bg,
						     &back_color);
			break;
		}
		case FOOBAR_PANEL:
			panel = foobar_widget_new (screen);

			push_correct_global_prefix ();
			s = conditional_get_string ("clock_format",
						    _("%I:%M:%S %p"),
						    NULL);
			gnome_config_pop_prefix ();

			if (s != NULL)
				foobar_widget_set_clock_format (FOOBAR_WIDGET (panel), s);
			g_free (s);

			break;
		default:
			panel = NULL;
			g_warning ("Unkown panel type: %d; ignoring.", type);
			break;
		}

		gnome_config_pop_prefix ();
		
		g_free (color);
		g_free (back_pixmap);

		if (panel != NULL) {
			if (unique_id > 0)
				panel_set_id (panel, unique_id);
			panel_setup (panel);
			gtk_widget_show (panel);
		}
	}
	g_string_free (buf, TRUE);

	g_free (prefix);
}

static gboolean
is_among_users (const char *username, const char *users)
{
	char *copy;
	char *p;

	if (users == NULL)
		return FALSE;

	copy = g_strchug (g_strdup (users));

	if (strcmp (copy, "*") == 0) {
		g_free (copy);
		return TRUE;
	}

	p = strtok (copy, ", \t;:");
	while (p != NULL) {
		if (strcasecmp_no_locale (username, p) == 0) {
			g_free (copy);
			return TRUE;
		}
		p = strtok (NULL, ", \t;:");
	}

	g_free (copy);
	return FALSE;
}

void
load_system_wide (void)
{
	char *users;
	const char *username = g_get_user_name ();

	gnome_config_push_prefix ("=" GLOBAL_CONFDIR "/System=/Restrictions/");

	commie_mode = gnome_config_get_bool ("LockDown=FALSE");
	no_run_box = gnome_config_get_bool ("NoRunBox=FALSE");

	users = gnome_config_get_string ("RestrictedUsers=*");
	if (is_among_users (username, users)) {
		g_free (users);
		users = gnome_config_get_string ("UnrestrictedUsers=");
		if (is_among_users (username, users)) {
			commie_mode = FALSE;
			no_run_box = FALSE;
		}
	} else {
		commie_mode = FALSE;
		no_run_box = FALSE;
	}
	g_free (users);

	/* Root shall always be allowed to do whatever */
	if (getuid () == 0) {
		commie_mode = FALSE;
		no_run_box = FALSE;
	}

	gnome_config_pop_prefix ();
}


void
load_up_globals (void)
{
	/* NOTE: !!!!!!!
	 * Keep in sync with loadup_vals in gnome-panel-properties.c,
	 * the function is the same there, but missing the apply_global_config
	 * and the default menu flags are hardcoded in loadup_vals
	 * FIXME: make this code common!!!!!
	 */
	char *tile_def[] = {
		"normal",
		"purple",
		"green",
		"blue"
	};
	int i;
	gboolean def;
	GString *keybuf;
	GString *tilebuf;

	/*set up global options*/
	push_correct_global_prefix ();

	global_config.tooltips_enabled =
		conditional_get_bool ("tooltips_enabled", TRUE, NULL);

	global_config.tooltips_enabled =
		conditional_get_bool ("tooltips_enabled", TRUE, NULL);

	global_config.show_dot_buttons =
		conditional_get_bool ("show_dot_buttons", FALSE, NULL);

	global_config.show_menu_titles =
		conditional_get_bool ("show_menu_titles", FALSE, NULL);

	global_config.hungry_menus =
		conditional_get_bool ("memory_hungry_menus", FALSE, NULL);

	global_config.use_large_icons =
		conditional_get_bool ("use_large_icons", FALSE, NULL);

	global_config.merge_menus =
		conditional_get_bool ("merge_menus", TRUE, NULL);

	global_config.menu_check =
		conditional_get_bool ("menu_check", TRUE, NULL);

	global_config.off_panel_popups =
		conditional_get_bool ("off_panel_popups", TRUE, NULL);
		
	global_config.disable_animations =
		conditional_get_bool ("disable_animations", FALSE, NULL);
		
	global_config.auto_hide_step_size =
		conditional_get_int ("auto_hide_step_size",
				     DEFAULT_AUTO_HIDE_STEP_SIZE, NULL);

	global_config.explicit_hide_step_size =
		conditional_get_int ("explicit_hide_step_size", 
				     DEFAULT_EXPLICIT_HIDE_STEP_SIZE, NULL);
		
	global_config.drawer_step_size =
		conditional_get_int ("drawer_step_size",
				     DEFAULT_DRAWER_STEP_SIZE, NULL);
		
	global_config.minimize_delay =
		conditional_get_int ("minimize_delay",
				     DEFAULT_MINIMIZE_DELAY, NULL);

	global_config.maximize_delay =
		conditional_get_int ("maximize_delay",
				     DEFAULT_MAXIMIZE_DELAY, NULL);
		
	global_config.minimized_size =
		conditional_get_int("minimized_size",
				    DEFAULT_MINIMIZED_SIZE, NULL);
		
	global_config.movement_type =
		conditional_get_int("movement_type", 
				    PANEL_SWITCH_MOVE, NULL);

	global_config.keys_enabled = conditional_get_bool ("keys_enabled",
							   TRUE, NULL);

	g_free(global_config.menu_key);
	global_config.menu_key = conditional_get_string ("menu_key",
							 "Mod1-F1", NULL);
	convert_string_to_keysym_state(global_config.menu_key,
				       &global_config.menu_keysym,
				       &global_config.menu_state);

	g_free(global_config.run_key);
	global_config.run_key = conditional_get_string ("run_key", "Mod1-F2",
							NULL);
	convert_string_to_keysym_state(global_config.run_key,
				       &global_config.run_keysym,
				       &global_config.run_state);

	g_free(global_config.screenshot_key);
	global_config.screenshot_key =
		conditional_get_string ("screenshot_key", "Print",
					NULL);
	convert_string_to_keysym_state (global_config.screenshot_key,
					&global_config.screenshot_keysym,
					&global_config.screenshot_state);

	g_free(global_config.window_screenshot_key);
	global_config.window_screenshot_key =
		conditional_get_string ("window_screenshot_key",
					"Shift-Print", NULL);
	convert_string_to_keysym_state (global_config.window_screenshot_key,
					&global_config.window_screenshot_keysym,
					&global_config.window_screenshot_state);

	global_config.applet_padding =
		conditional_get_int ("applet_padding", 3, NULL);

	global_config.applet_border_padding =
		conditional_get_int ("applet_border_padding", 0, NULL);

	global_config.autoraise = conditional_get_bool ("autoraise", TRUE, NULL);

	global_config.keep_bottom =
		conditional_get_bool ("keep_bottom", FALSE, &def);
	/* if keep bottom was the default, then we want to do a nicer
	 * saner default which is normal layer.  If it was not the
	 * default then we don't want to change the layerness as it was
	 * selected by the user and thus we default to FALSE */
	if (def)
		global_config.normal_layer =
			conditional_get_bool ("normal_layer", TRUE, NULL);
	else
		global_config.normal_layer =
			conditional_get_bool ("normal_layer", FALSE, NULL);

	global_config.drawer_auto_close =
		conditional_get_bool ("drawer_auto_close", FALSE, NULL);
	global_config.simple_movement =
		conditional_get_bool ("simple_movement", FALSE, NULL);
	global_config.hide_panel_frame =
		conditional_get_bool ("hide_panel_frame", FALSE, NULL);
	global_config.tile_when_over =
		conditional_get_bool ("tile_when_over", FALSE, NULL);
	global_config.saturate_when_over =
		conditional_get_bool ("saturate_when_over", TRUE, NULL);
	global_config.confirm_panel_remove =
		conditional_get_bool ("confirm_panel_remove", TRUE, NULL);
	global_config.fast_button_scaling =
		conditional_get_bool ("fast_button_scaling", FALSE, NULL);
	global_config.avoid_collisions =
		conditional_get_bool ("avoid_collisions", TRUE, NULL);
	
	global_config.menu_flags = conditional_get_int
		("menu_flags", get_default_menu_flags (), NULL);
	if (global_config.menu_flags < 0) {
		global_config.menu_flags = get_default_menu_flags ();
	}

	keybuf = g_string_new(NULL);
	tilebuf = g_string_new(NULL);
	for (i = 0; i < LAST_TILE; i++) {
		GString *keybuf = g_string_new(NULL);
		GString *tilebuf = g_string_new(NULL);

		g_string_sprintf (keybuf, "new_tiles_enabled_%d",i);
		global_config.tiles_enabled[i] =
			conditional_get_bool (keybuf->str, FALSE, NULL);

		g_free (global_config.tile_up[i]);
		g_string_sprintf (keybuf, "tile_up_%d", i);
		g_string_sprintf (tilebuf, "tiles/tile-%s-up.png", tile_def[i]);
		global_config.tile_up[i] = conditional_get_string (keybuf->str,
								   tilebuf->str,
								   NULL);

		g_free(global_config.tile_down[i]);
		g_string_sprintf (keybuf, "tile_down_%d", i);
		g_string_sprintf (tilebuf, "tiles/tile-%s-down.png",
				  tile_def[i]);
		global_config.tile_down[i] =
			conditional_get_string (keybuf->str, tilebuf->str,
						NULL);

		g_string_sprintf (keybuf, "tile_border_%d", i);
		global_config.tile_border[i] =
			conditional_get_int (keybuf->str, 2, NULL);
		g_string_sprintf (keybuf, "tile_depth_%d", i);
		global_config.tile_depth[i] =
			conditional_get_int (keybuf->str, 2, NULL);
	}
	g_string_free (tilebuf, TRUE);
	g_string_free (keybuf, TRUE);

	gnome_config_sync ();

	gnome_config_pop_prefix ();

	apply_global_config ();
}

void
write_global_config (void)
{
	int i;
	GString *buf;

	gnome_config_push_prefix ("/panel/Config/");

	gnome_config_set_int ("auto_hide_step_size",
			      global_config.auto_hide_step_size);
	gnome_config_set_int ("explicit_hide_step_size",
			      global_config.explicit_hide_step_size);
	gnome_config_set_int ("drawer_step_size",
			      global_config.drawer_step_size);
	gnome_config_set_int ("minimized_size",
			      global_config.minimized_size);
	gnome_config_set_int ("minimize_delay",
			      global_config.minimize_delay);
	gnome_config_set_int ("maximize_delay",
			      global_config.maximize_delay);
	gnome_config_set_int ("movement_type",
			      (int)global_config.movement_type);
	gnome_config_set_bool ("tooltips_enabled",
			       global_config.tooltips_enabled);
	gnome_config_set_bool ("show_dot_buttons",
			       global_config.show_dot_buttons);
	gnome_config_set_bool ("show_menu_titles",
			       global_config.show_menu_titles);
	gnome_config_set_bool ("memory_hungry_menus",
			       global_config.hungry_menus);
	gnome_config_set_bool ("use_large_icons",
			       global_config.use_large_icons);
	gnome_config_set_bool ("merge_menus",
			       global_config.merge_menus);
	gnome_config_set_bool ("menu_check",
			       global_config.menu_check);
	gnome_config_set_bool ("off_panel_popups",
			       global_config.off_panel_popups);
	gnome_config_set_bool ("disable_animations",
			       global_config.disable_animations);
	gnome_config_set_int ("applet_padding",
			      global_config.applet_padding);
	gnome_config_set_int ("applet_border_padding",
			      global_config.applet_border_padding);
	gnome_config_set_bool ("autoraise",
			       global_config.autoraise);
	gnome_config_set_bool ("keep_bottom",
			       global_config.keep_bottom);
	gnome_config_set_bool ("normal_layer",
			       global_config.normal_layer);
	gnome_config_set_bool ("drawer_auto_close",
			       global_config.drawer_auto_close);
	gnome_config_set_bool ("simple_movement",
			       global_config.simple_movement);
	gnome_config_set_bool ("hide_panel_frame",
			       global_config.hide_panel_frame);
	gnome_config_set_bool ("tile_when_over",
			       global_config.tile_when_over);
	gnome_config_set_bool ("saturate_when_over",
			       global_config.saturate_when_over);
	gnome_config_set_bool ("confirm_panel_remove",
			       global_config.confirm_panel_remove);
	gnome_config_set_int ("menu_flags", global_config.menu_flags);
	gnome_config_set_bool ("keys_enabled", global_config.keys_enabled);
	gnome_config_set_string ("menu_key", global_config.menu_key);
	gnome_config_set_string ("run_key", global_config.run_key);
	gnome_config_set_string ("screenshot_key",
				 global_config.screenshot_key);
	gnome_config_set_string ("window_screenshot_key",
				 global_config.window_screenshot_key);
	gnome_config_set_bool ("fast_button_scaling",
			       global_config.fast_button_scaling);
	gnome_config_set_bool ("avoid_collisions",
			       global_config.avoid_collisions);
			     
	buf = g_string_new (NULL);
	for (i = 0; i < LAST_TILE; i++) {
		g_string_sprintf (buf, "new_tiles_enabled_%d", i);
		gnome_config_set_bool (buf->str,
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
	gnome_config_pop_prefix();
	gnome_config_sync();
	
}

/* used for conversion to new config */
static void
convert_write_config(void)
{
	int i;
	int is_def;
	GString *buf;
	gnome_config_push_prefix("/panel/Config/");
	
	/* is there any new config written here */
	gnome_config_get_int_with_default("auto_hide_step_size", &is_def);
	if(!is_def) {
		/* we don't want to overwrite a good config */
		gnome_config_pop_prefix();
		return;
	}

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
	gnome_config_set_bool("show_dot_buttons",
			      global_config.show_dot_buttons);
	gnome_config_set_bool("memory_hungry_menus",
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
	buf = g_string_new(NULL);
	for(i=0;i<LAST_TILE;i++) {
		g_string_sprintf(buf,"new_tiles_enabled_%d",i);
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
	gnome_config_pop_prefix();
	gnome_config_sync();
}

static gboolean
convert_read_old_config(void)
{
	char *tile_def[] = {
		"normal",
		"purple",
		"green",
		"blue"
	};
	GString *buf;
	int i,is_def;
	int applet_count; /*store this so that we can clean*/
	int panel_count; /*store this so that we can clean*/

	gnome_config_push_prefix(PANEL_CONFIG_PATH "panel/Config/");

	gnome_config_get_bool_with_default("tooltips_enabled=TRUE",&is_def);
	if(is_def) {
		gnome_config_pop_prefix();
		return FALSE;
	}
	
	buf = g_string_new(NULL);

	global_config.tooltips_enabled =
		gnome_config_get_bool("tooltips_enabled=TRUE");

	global_config.show_dot_buttons =
		gnome_config_get_bool("show_dot_buttons=FALSE");

	/*
	global_config.hungry_menus =
		gnome_config_get_bool("hungry_menus=TRUE");
		*/
	/* we default to FALSE now and want everything to go to false */
	global_config.hungry_menus = FALSE;

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

	global_config.applet_padding = gnome_config_get_int("applet_padding=3");

	global_config.autoraise = gnome_config_get_bool("autoraise=TRUE");

	global_config.keep_bottom = gnome_config_get_bool("keep_bottom=FALSE");

	global_config.drawer_auto_close = gnome_config_get_bool("drawer_auto_close=FALSE");
	global_config.simple_movement = gnome_config_get_bool("simple_movement=FALSE");
	global_config.hide_panel_frame = gnome_config_get_bool("hide_panel_frame=FALSE");
	global_config.tile_when_over = gnome_config_get_bool("tile_when_over=FALSE");
	for(i=0;i<LAST_TILE;i++) {
		g_string_sprintf(buf,"new_tiles_enabled_%d=TRUE",i);
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

	/* preserve applet count */
	applet_count = gnome_config_get_int("applet_count=0");
	panel_count = gnome_config_get_int("panel_count=0");

	gnome_config_pop_prefix();

	gnome_config_push_prefix("");

	gnome_config_clean_section(PANEL_CONFIG_PATH "panel/Config");

	gnome_config_set_int(PANEL_CONFIG_PATH "panel/Config/applet_count",
			     applet_count);
	gnome_config_set_int(PANEL_CONFIG_PATH "panel/Config/panel_count",
			     panel_count);

	gnome_config_pop_prefix();

	gnome_config_sync();
	g_string_free(buf,TRUE);
	return TRUE;
}

void
convert_old_config(void)
{
	if(convert_read_old_config())
		convert_write_config();
}
