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

#include <gdk/gdkx.h>
#include <X11/keysym.h>

#include <libgnome/libgnome.h>
#include <libbonobo.h>

#include "gnome-desktop-item.h"

#include "session.h"

#include "aligned-widget.h"
#include "button-widget.h"
#include "distribution.h"
#include "drawer-widget.h"
#include "edge-widget.h"
#include "floating-widget.h"
#include "foobar-widget.h"
#include "launcher.h"
#include "logout.h"
#include "menu-fentry.h"
#include "menu-util.h"
#include "menu.h"
#include "panel-util.h"
#include "panel-gconf.h"
#include "panel-config-global.h"
#include "session.h"
#include "sliding-widget.h"
#include "status.h"
#include "swallow.h"
#include "gnome-run.h"
#include "global-keys.h"
#include "xstuff.h"
#include "multiscreen-stuff.h"
#include "conditional.h"
#include "panel-applet-frame.h"
#include "panel-shell.h"

#undef SESSION_DEBUG 

int config_sync_timeout = 0;
int applets_to_sync = FALSE;
int panels_to_sync = FALSE;
int need_complete_save = FALSE;

extern GSList *panels;
extern GSList *applets;
extern int applet_count;

extern gchar *panel_profile_name;

extern GtkTooltips *panel_tooltips;

extern GnomeClient *client;

gboolean commie_mode = FALSE;
gboolean no_run_box = FALSE;

GlobalConfig global_config;

/*list of all panel widgets created*/
extern GSList *panel_list;

extern char *kde_menudir;

int ss_cur_applet = 0;
gboolean ss_done_save = FALSE;
gushort ss_cookie = 0;
GtkWidget *ss_timeout_dlg = NULL;
static gboolean ss_interactive = FALSE;
static int ss_timeout = 500;

void
apply_global_config (void)
{
	static int layer_old = -1;
	static int menu_flags_old = -1;
	static int old_menu_check = -1;
	GSList *li;

	if (global_config.tooltips_enabled)
		gtk_tooltips_enable (panel_tooltips);
	else
		gtk_tooltips_disable (panel_tooltips);
	/* not incredibly efficent way to do this, we just make
	 * sure that all directories are reread */
	if (old_menu_check != global_config.menu_check) {
		fr_force_reread ();
	}
	if(old_menu_check != global_config.menu_check) {
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
	old_menu_check = global_config.menu_check;

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

	if (layer_old != global_config.layer) {
		 for (li = panel_list; li != NULL; li = li->next) {
			PanelData *pd = li->data;
			if ( ! GTK_WIDGET_REALIZED (pd->panel))
				continue;
			if (BASEP_IS_WIDGET (pd->panel))
				basep_widget_update_winhints (BASEP_WIDGET (pd->panel));
			else if (FOOBAR_IS_WIDGET (pd->panel))
				foobar_widget_update_winhints (FOOBAR_WIDGET (pd->panel));
		}
	}
	layer_old = global_config.layer;
	
	for (li = panel_list; li != NULL; li = li->next) {
		PanelData *pd = li->data;
		if (BASEP_IS_WIDGET (pd->panel)) {
			basep_update_frame (BASEP_WIDGET (pd->panel));

			if ((menu_flags_old != global_config.menu_flags) &&
			    pd->menu) {
				gtk_widget_unref (pd->menu);
				pd->menu = NULL;
				pd->menu_age = 0;
			}
				
		}
	}
	menu_flags_old = global_config.menu_flags;

	panel_global_keys_setup();
}

#ifdef FIXME /* Keep this code until a new one is done for applets */
static gboolean
session_save_timeout (gpointer data)
{
	int cookie = GPOINTER_TO_INT (data);
	if (cookie != ss_cookie)
		return FALSE;

#ifdef SESSION_DEBUG	
	printf("SAVE TIMEOUT (%u)\n",ss_cookie);
#endif
	if ( ! ss_interactive) {
		ss_cookie ++;
		return FALSE;
	}

	ss_timeout_dlg =
		gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
				       _("An applet is not "
					 "responding to a "
					 "save request.\n"
					 "Remove the applet "
					 "or continue waiting?"));
	g_signal_connect (G_OBJECT(ss_timeout_dlg),"destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &ss_timeout_dlg);
	gtk_dialog_add_button (GTK_DIALOG (ss_timeout_dlg),
			       _("Remove applet"),
			       1); /* FIXME: GNOME_STOCK_PIXMAP_TRASH */
	gtk_dialog_add_button (GTK_DIALOG (ss_timeout_dlg),
			       _("Continue waiting"),
			       2); /* FIXME: GNOME_STOCK_PIXMAP_TIMER */

	if (1 == gtk_dialog_run (GTK_DIALOG (ss_timeout_dlg))) {
		ss_cookie++;
		g_warning(_("Timed out on sending session save to an applet"));
		save_next_applet();
		gtk_widget_destroy (ss_timeout_dlg);
		return FALSE;
	}
	gtk_widget_destroy (ss_timeout_dlg);
	return TRUE;
}
#endif

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

	if (info->type == APPLET_EMPTY) {
		gnome_config_set_string ("id", EMPTY_ID);
		gnome_config_pop_prefix ();
		g_string_free (buf,TRUE);
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
	case APPLET_BONOBO:
		/*
		 * No session saving for applets.
		 */
		break;
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
			gnome_config_set_bool("custom_icon",
					      menu->custom_icon);
			gnome_config_set_string("custom_icon_file",
						menu->custom_icon_file);
			break;
		}
	case APPLET_LAUNCHER:
		{
			Launcher *launcher = info->data;
			const char *location;

			gnome_config_set_string ("id", LAUNCHER_ID);

			/* clean old launcher info */
			gnome_config_clean_key ("parameters");

			launcher_save (launcher);
			location = gnome_desktop_item_get_location (launcher->ditem);
			gnome_config_set_string ("base_location", location);
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
	gchar *panel_profile_key;

	if (BASEP_IS_WIDGET (pd->panel)) {
		basep = BASEP_WIDGET (pd->panel);
		panel = PANEL_WIDGET (basep->panel);
	} else if (FOOBAR_IS_WIDGET (pd->panel)) {
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
	
	/* now do different types */
	if (BORDER_IS_WIDGET(pd->panel))
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
	g_string_sprintf(buf, "%spanel/Panel_%d", PANEL_CONFIG_PATH, *num);
		panel_profile_key = panel_gconf_global_config_get_full_key ("clock-format");
		panel_gconf_set_string (panel_profile_key, FOOBAR_WIDGET (pd->panel)->clock_format);
		g_free (panel_profile_key);
		gnome_config_set_int ("screen", FOOBAR_WIDGET (pd->panel)->screen);
		break;
	default:
		break;
	}
	
	g_string_free(buf,TRUE);

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

/*
 * We queue the location of .desktop files belonging
 * to dead launchers here, to be removed when saving
 * the session.
 */
static GList *session_dead_launcher_locations = NULL;

void
session_add_dead_launcher (const gchar *location)
{
	g_return_if_fail (location);

	session_dead_launcher_locations =
			g_list_prepend (session_dead_launcher_locations, 
					g_strdup (location));
}

static void
session_unlink_dead_launchers (void)
{
	GList *l;

	for (l = session_dead_launcher_locations; l; l = l->next) {
		gchar *file = l->data;

		unlink (file);

		g_free (file);

		l->data = NULL;
	}

	g_list_free (session_dead_launcher_locations);

	session_dead_launcher_locations = NULL;
}

static void
do_session_save(GnomeClient *client,
		gboolean complete_sync,
		gboolean sync_applets,
		gboolean sync_panels)
{
	int num;

	/* If in commie mode, then no saving is needed, the user
	 * could have changed anything anyway */
	if (commie_mode)
		return;

#ifdef SESSION_DEBUG	
	printf("Saving to [%s]\n",PANEL_CONFIG_PATH);

	printf("Saving session: 1"); fflush(stdout);

	printf(" 2"); fflush(stdout);
#endif

	gnome_config_push_prefix (PANEL_CONFIG_PATH "panel/Config/");

	if(complete_sync || sync_applets)
		gnome_config_set_int ("applet_count", applet_count);
#ifdef SESSION_DEBUG
	printf(" 3"); fflush(stdout);
#endif
	if(complete_sync || sync_panels) {
		num = 1;
		g_slist_foreach(panel_list, save_panel_configuration, &num);
		gnome_config_set_int("panel_count",num-1);
	}
#ifdef SESSION_DEBUG
	printf(" 4\n"); fflush(stdout);
#endif
	gnome_config_pop_prefix ();

	if(complete_sync)
		save_tornoff();

	gnome_config_sync();
	
	if(complete_sync || sync_applets) {
		ss_cur_applet = -1;
		ss_done_save = FALSE;

		session_unlink_dead_launchers ();

		/* start saving applets */
		save_next_applet ();
	}
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
			need_complete_save = FALSE;
			applets_to_sync = FALSE;
			panels_to_sync = FALSE;
			do_session_save (client, ncs, ats, pts);
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
	GSList *l;

	gtk_timeout_remove (config_sync_timeout);
	config_sync_timeout = 0;
  
	status_inhibit = TRUE;
	status_spot_remove_all ();

	for (l = applets; l; l = l->next) {
		AppletInfo *info = l->data;

		switch (info->type) {
		case APPLET_BONOBO:
			panel_applet_frame_save_position (
				PANEL_APPLET_FRAME (info->data));
			break;
		case APPLET_SWALLOW: {
			Swallow   *swallow = info->data;
			GtkSocket *socket;

			swallow->clean_remove = TRUE;

			socket = GTK_SOCKET (swallow->socket);

			if (socket->plug_window)
                                XKillClient (GDK_DISPLAY (),
					     GDK_WINDOW_XWINDOW(socket->plug_window));

			}
			break;
		default:
			break;
		}
	}

	gnome_config_sync ();

	xstuff_unsetup_desktop_area ();
			
	panel_shell_unregister ();
	
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
		return g_strdup_printf ("=" GLOBAL_CONFDIR PANEL_CONFIG_PATH);
	}

	count = conditional_get_int (PANEL_CONFIG_PATH
				     "panel/Config/panel_count", 0, NULL);
	if (count > 0) {
		gnome_config_pop_prefix ();
		*sep = "";
		return g_strdup (PANEL_CONFIG_PATH);
	}

	path = "=" GLOBAL_CONFDIR PANEL_CONFIG_PATH "panel=/Config/panel_count";
	count = conditional_get_int (path, 0, NULL);

	if (count > 0) {
		gnome_config_pop_prefix ();
		*sep = "=";
		return g_strdup_printf ("=" GLOBAL_CONFDIR PANEL_CONFIG_PATH);
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

static void
session_init_user_applets (void)
{
	GString *buf;
	int count, num;	
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

		if (!strcmp (applet_name, BONOBO_ID)) {
			char *iid;

			iid = conditional_get_string ("iid", NULL, NULL);

			if (iid && iid [0] )
				panel_applet_frame_load (iid, panel, pos);

			g_free (iid);

		} else if (!strcmp (applet_name, LAUNCHER_ID)) { 
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
			char *path = conditional_get_string ("path",
							     "applications:/", NULL);
			gboolean main_menu =
				conditional_get_bool ("main_menu", TRUE, NULL);
			gboolean global_main =
				conditional_get_bool ("global_main", TRUE, NULL);
			gboolean custom_icon = 
				conditional_get_bool ("custom_icon", FALSE,
						      NULL);
			char *custom_icon_file =
				conditional_get_string ("custom_icon_file",
							NULL, NULL);
			int flags = conditional_get_int ("main_menu_flags",
							 get_default_menu_flags (),
							 NULL);
			if (flags < 0)
				flags = get_default_menu_flags ();
			
			load_menu_applet (path, main_menu,
					  flags, global_main,
					  custom_icon, custom_icon_file,
					  panel, pos, TRUE);

			g_free (custom_icon_file);
			g_free (path);

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

static void
session_init_panels(void)
{
	gchar *panel_profile_key;
	gchar *panel_default_profile;
	gboolean use_default = FALSE;

	GtkWidget *panel = NULL;
	GSList *panel_ids;
	GSList *temp;
	char *s;
	
	panel_profile_key = panel_gconf_general_profile_get_full_key (panel_profile_name, "panel-id-list");

	if (panel_gconf_dir_exists (panel_profile_key) == FALSE) {
		/* FIXME: We need to resort to another fallback default panel config 
		          and do some sort of check for screen sizes */
		use_default = TRUE;
		panel_default_profile = "medium";
		g_free (panel_profile_key);
		panel_profile_key = panel_gconf_general_default_profile_get_full_key (panel_default_profile, "panel-id-list");
	}

	panel_ids = gconf_client_get_list (panel_gconf_get_client (),
					   panel_profile_key,
					   GCONF_VALUE_STRING,
					   NULL);
	g_free (panel_profile_key);
					  
	/* Eeeeeeeek! no default config, no user config, this is
	   bad bad bad, load a single panel
	if (panel_ids == NULL)  {
		panel = edge_widget_new (0,
					 BORDER_BOTTOM,
					 BASEP_EXPLICIT_HIDE, 
					 BASEP_SHOWN,
					 PANEL_SIZE_MEDIUM,
					 TRUE,
					 TRUE,
					 PANEL_BACK_NONE,
					 NULL,
					 TRUE,
					 FALSE,
					 TRUE,
					 NULL);
		panel_setup(panel);
		gtk_widget_show(panel);
	*/
		/* FIXME : load up the foot menu 
		load_menu_applet ("applications:/",
				  TRUE, 
				  get_default_menu_flags (),
				  TRUE, FALSE, NULL,
				  PANEL_WIDGET(BASEP_WIDGET(panel)->panel),
				  0, TRUE);
		
		return;
	} */

	for (temp = panel_ids; temp; temp = temp->next) {
		PanelType type;
		PanelBackType back_type;
		int sz;
		BasePState state;
		BasePMode mode;
		BorderEdge edge;
		char *back_pixmap, *color;
		GdkColor back_color = {0,0,0,1};
		gboolean fit_pixmap_bg;
		gboolean strech_pixmap_bg;
		gboolean rotate_pixmap_bg;
		int hidebuttons_enabled;
		int hidebutton_pixmaps_enabled;
		int screen;

		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name, 
										   (const gchar *) temp->data,
										   "panel-background-pixmap",
										   use_default);
		back_pixmap = panel_gconf_get_string (panel_profile_key);
		if (string_empty (back_pixmap)) {
			g_free (back_pixmap);
			back_pixmap = NULL;
		}
		g_free (panel_profile_key);


		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
										   (const gchar *) temp->data,
										   "panel-background-color",
										   use_default);
		color = panel_gconf_get_string (panel_profile_key);
		if ( ! string_empty (color))
			gdk_color_parse (color, &back_color);
		g_free (panel_profile_key);

		
		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
										   (const gchar *) temp->data,
										   "panel-background-type",
										   use_default);
		back_type = panel_gconf_get_int (panel_profile_key);
		g_free (panel_profile_key);

		
		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
										   (const gchar *) temp->data,
										   "panel-background-pixmap-fit",
										   use_default);
		fit_pixmap_bg = panel_gconf_get_bool (panel_profile_key);
		g_free (panel_profile_key);


		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
										   (const gchar *) temp->data,
										   "panel-background-pixmap-stretch",
										   use_default);

		strech_pixmap_bg = panel_gconf_get_bool (panel_profile_key);
		g_free (panel_profile_key);


		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
										   (const gchar *) temp->data,
										   "panel-background-pixmap-rotate",
										   use_default);
		rotate_pixmap_bg = panel_gconf_get_bool (panel_profile_key);
		g_free (panel_profile_key);

		
		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
										   (const gchar *) temp->data,
										   "panel-size",
										   use_default);
		
		sz = panel_gconf_get_int (panel_profile_key);
		g_free (panel_profile_key);


		/* Now for type specific config */

		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
										   (const gchar *) temp->data,
										   "panel-type",
										   use_default);
		type = panel_gconf_get_int (panel_profile_key);
		g_free (panel_profile_key);

		
		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
										   (const gchar *) temp->data,
										   "hide-buttons-enabled",
										   use_default);
		
		hidebuttons_enabled = panel_gconf_get_bool (panel_profile_key);
		g_free (panel_profile_key);

		
		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
										   (const gchar *) temp->data,
										   "hide-button-pixmaps-enabled",
										   use_default);

		hidebutton_pixmaps_enabled = panel_gconf_get_bool (panel_profile_key);
		g_free (panel_profile_key);

		
		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
										   (const gchar *) temp->data,
										   "panel-hide-state",
										   use_default);
		state = panel_gconf_get_int (panel_profile_key);
		g_free (panel_profile_key);

		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
										   (const gchar *) temp->data,
										   "panel-hide-mode",
										   use_default);
		mode = panel_gconf_get_int (panel_profile_key);
		g_free (panel_profile_key);


		panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
										   (const gchar *) temp->data,
										   "screen-id",
										   use_default);
		screen = panel_gconf_get_int (panel_profile_key);
		g_free (panel_profile_key);

#if 0 /* i guess we can't easily do this for now */
		pos = basep_widget_load_pos_settings();
#endif
		switch (type) {
			
		case EDGE_PANEL:
			panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
											   (const gchar *) temp->data,
											   "screen-edge",
											   use_default);
			
			edge = panel_gconf_get_int (panel_profile_key);
			g_free (panel_profile_key);

			panel = edge_widget_new (screen,
						 edge, 
						 mode, state,
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
			
			panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
											   (const gchar *) temp->data,
											   "screen-edge",
											   use_default);
			edge = panel_gconf_get_int (panel_profile_key);
			g_free (panel_profile_key);
			
			align = conditional_get_int ("align", ALIGNED_LEFT,
						     NULL);

			panel = aligned_widget_new (screen,
						    align,
						    edge,
						    mode,
						    state,
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
			
			panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
											   (const gchar *) temp->data,
											   "screen-edge",
											   use_default);
			edge = panel_gconf_get_int (panel_profile_key);
			g_free (panel_profile_key);
			

			panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
											   (const gchar *) temp->data,
											   "panel-anchor",
											   use_default);
			anchor = panel_gconf_get_int (panel_profile_key);
			g_free (panel_profile_key);
			
			
			panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
											   (const gchar *) temp->data,
											   "panel-offset",
											   use_default);
			offset = panel_gconf_get_int (panel_profile_key);
			g_free (panel_profile_key);
	
			/* FIXME : clean up after schema has been written, kept so we can remember default
			anchor = conditional_get_int ("anchor",	
						      SLIDING_ANCHOR_LEFT,
						      NULL);
			offset = conditional_get_int ("offset", 0, NULL);
			*/

			panel = sliding_widget_new (screen,
						    anchor,
						    offset,
						    edge,
						    mode,
						    state,
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
			PanelOrient orient;
			
			panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
											   (const gchar *) temp->data,
											   "panel-orient",
											   use_default);
			orient = panel_gconf_get_int (panel_profile_key);
			g_free (panel_profile_key);

			/* FIXME : clean up after schema has been written, kept so we can remember default
			orient = conditional_get_int ("orient", PANEL_ORIENT_UP,
						      NULL);
			*/

			/* FIXME: there are some issues with auto hiding drawers */

			panel = drawer_widget_new (orient,
						   BASEP_EXPLICIT_HIDE, 
						   state,
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
		case FLOATING_PANEL: {
			GtkOrientation orient;
			int x, y;
			
			panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
											   (const gchar *) temp->data,
											   "panel-orient",
											   use_default);
			orient = panel_gconf_get_int (panel_profile_key);
			g_free (panel_profile_key);


			panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
											   (const gchar *) temp->data,
											   "panel-x-position",
											   use_default);
			x = panel_gconf_get_int (panel_profile_key);
			g_free (panel_profile_key);


			panel_profile_key = panel_gconf_panel_profile_get_conditional_key (panel_profile_name,
											   (const gchar *) temp->data,
											   "panel-y-position",
											   use_default);
			y = panel_gconf_get_int (panel_profile_key);
			g_free (panel_profile_key);
			
			/* FIXME : clean up after schema has been written, kept so we can remember default
			orient = conditional_get_int ("orient",
						      GTK_ORIENTATION_HORIZONTAL,
						      NULL);
			x = conditional_get_int ("x", 0, NULL);
			y = conditional_get_int ("y", 0, NULL);
			*/


			panel = floating_widget_new (screen,
						     x,
						     y,
						     orient,
						     mode,
						     state,
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

			panel_profile_key = panel_gconf_global_config_get_full_key ("clock-format");
			s = panel_gconf_get_string (panel_profile_key);
			g_free (panel_profile_key);

			if (s != NULL)
				foobar_widget_set_clock_format (FOOBAR_WIDGET (panel), s);
			g_free (s);

			break;
		default:
			panel = NULL;
			g_warning ("Unkown panel type: %d; ignoring.", type);
			break;
		}

		g_free (color);
		g_free (back_pixmap);

		if (panel != NULL) {
			panel_setup (panel);
			gtk_widget_show (panel);
		}
	}
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
		if (g_ascii_strcasecmp (username, p) == 0) {
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
session_read_global_config (void)
{
	GSList *li, *list;

	list = panel_gconf_all_global_entries ();

	for (li = list; li != NULL ; li = li->next) {
		GConfEntry *entry;
		GConfValue *value;
		gchar      *key;

		entry = (GConfEntry *)li->data;

		value = gconf_entry_get_value (entry);

		key = g_path_get_basename (gconf_entry_get_key (entry));

		if (!strcmp (key, "panel-animation-speed")) {
			switch (gconf_value_get_int (value)) {
				case 0:
					global_config.animation_speed = 80;
					break;
				case 1:
					global_config.animation_speed = 60;
					break;
				case 2:
					global_config.animation_speed = 10;
					break;
				default:
					global_config.animation_speed = 60;
			}
		}

		else if (!strcmp (key, "panel-minimized-speed"))
			global_config.minimized_size =
				gconf_value_get_int (value);

		else if (!strcmp (key, "panel-minimized-size"))
			global_config.minimized_size =
				gconf_value_get_int (value);

		else if (!strcmp (key, "panel-hide-delay"))
			global_config.hide_delay =
				gconf_value_get_int (value);

		else if (!strcmp (key, "panel-show-delay"))
			global_config.show_delay =
				gconf_value_get_int (value);

		else if (!strcmp (key, "tooltips-enabled"))
			global_config.tooltips_enabled =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "keep-menus-in-memory"))
			global_config.keep_menus_in_memory =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "enable-animations"))
			global_config.enable_animations =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "autoraise-panel"))
			global_config.autoraise =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "panel-window-layer"))
			global_config.layer =
				gconf_value_get_int (value);

		else if (!strcmp (key, "drawer-autoclose"))
			global_config.drawer_auto_close =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "auto-update-menus"))
			g_warning ("\nGman: what's this '%s' ?\nmarkmc: something that's probably not needed\nGman: then why is in the schema? :P\n_vicious_: It's needed so that say NFS setups don't recheck menus all the time, cuz it can slow things down quite a bit.\nandersca: you guys are very lucky to have _v_.", key);

		else if (!strcmp (key, "highlight-launchers-on-mouseover"))
			global_config.highlight_when_over =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "confirm-panel-remove"))
			global_config.confirm_panel_remove =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "enable-key-bindings"))
			global_config.keys_enabled =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "menu-key")) { 
			if (global_config.menu_key )
				g_free (global_config.menu_key );

			global_config.menu_key =
				g_strdup (gconf_value_get_string (value));

			convert_string_to_keysym_state (global_config.menu_key,
							&global_config.menu_keysym,
							&global_config.menu_state);
		} else if (!strcmp (key, "run-key")) {
			if (global_config.run_key )
				g_free (global_config.run_key );

			global_config.run_key =
				 g_strdup (gconf_value_get_string (value));

			convert_string_to_keysym_state (global_config.run_key,
							&global_config.run_keysym,
							&global_config.run_state);
		} else if (!strcmp (key, "screenshot-key")) {
			if (global_config.screenshot_key )
				g_free (global_config.screenshot_key );

			global_config.screenshot_key =
				g_strdup (gconf_value_get_string (value));

			convert_string_to_keysym_state (global_config.screenshot_key,
							&global_config.screenshot_keysym,
							&global_config.screenshot_state);

		} else if (!strcmp (key, "window-screenshot-key")) {
			if (global_config.window_screenshot_key )
				g_free (global_config.window_screenshot_key );

			global_config.window_screenshot_key =
				g_strdup (gconf_value_get_string (value));

			convert_string_to_keysym_state (global_config.window_screenshot_key,
							&global_config.window_screenshot_keysym,
							&global_config.window_screenshot_state);

		} else  {
			g_warning ("%s not handled", key);
		}

		g_free (key);
		gconf_entry_free (entry);
	}

	g_slist_free (list);

	/* FIXME STUFF THAT IS BORKED */
	global_config.menu_check = TRUE;
	global_config.menu_flags = get_default_menu_flags();

	apply_global_config ();
}

void
session_write_global_config (void)
{
	GConfChangeSet *global_config_cs;
	gchar *full_key;

	global_config_cs = gconf_change_set_new ();

	/* FIXME STUFF THAT IS BORKED 
	panel_gconf_global_config_set_int ("menu-flags", global_config.menu_flags);
	panel_gconf_global_config_set_bool ("menu-check", global_config.menu_check);
	*/

	/* FIXME: Make this more generic - this currently sucks */

	full_key = panel_gconf_global_config_get_full_key ("panel-animation-speed");
	gconf_change_set_set_int (global_config_cs, full_key , global_config.animation_speed);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("panel-minimized-speed");
	gconf_change_set_set_int (global_config_cs, full_key, global_config.minimized_size);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("panel-hide-delay");
	gconf_change_set_set_int (global_config_cs, full_key, global_config.hide_delay);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("panel-show-delay");
	gconf_change_set_set_int (global_config_cs, full_key, global_config.show_delay);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("panel-window-layer");
	gconf_change_set_set_int (global_config_cs, full_key, global_config.layer);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("tooltips-enabled");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.tooltips_enabled);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("enable-animations");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.enable_animations);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("autoraise-panel");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.autoraise);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("drawer-autoclose");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.drawer_auto_close);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("highlight-launchers-on-mouseover");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.highlight_when_over);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("confirm-panel-remove");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.confirm_panel_remove);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("keep-menus-in-memory");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.keep_menus_in_memory);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("menu-key");
	gconf_change_set_set_string (global_config_cs, full_key, global_config.menu_key);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("run-key");
	gconf_change_set_set_string (global_config_cs, full_key, global_config.run_key);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("screenshot-key");
	gconf_change_set_set_string (global_config_cs, full_key, global_config.screenshot_key);
	g_free (full_key);
	
	full_key = panel_gconf_global_config_get_full_key ("window-screenshot-key");
	gconf_change_set_set_string (global_config_cs, full_key, global_config.window_screenshot_key);
	g_free (full_key);

	gconf_client_commit_change_set (panel_gconf_get_client (), global_config_cs, FALSE, NULL);

	gconf_change_set_unref (global_config_cs);
			     
}

void session_load (void) {
  session_init_panels ();
/*  session_init_user_applets (); */
}
