/* Gnome panel: Initialization routines
 * (C) 1997,1998,1999,2000 the Free Software Foundation
 * (C) 2000 Eazel, Inc.
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 *          George Lebl
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <libgnomeui/gnome-window-icon.h>

#include "panel-include.h"

#include "gwmh.h"

#include "xstuff.h"
#include "multiscreen-stuff.h"
#include "conditional.h"

extern int config_sync_timeout;
extern int applets_to_sync;
extern int panels_to_sync;
extern int need_complete_save;

extern GSList *panels;

extern GSList *applets;
extern GSList *applets_last;
extern int applet_count;

extern gboolean commie_mode;
extern GlobalConfig global_config;
extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

extern GdkPixmap *desktop_pixmap;

/*list of all panel widgets created*/
extern GSList *panel_list;

GtkTooltips *panel_tooltips = NULL;

GnomeClient *client = NULL;

char *kde_menudir = NULL;
char *kde_icondir = NULL;
char *kde_mini_icondir = NULL;

char *merge_merge_dir = NULL;
int merge_main_dir_len = 0;
char *merge_main_dir = NULL;

gboolean panel_in_startup = TRUE;

static gboolean
menu_age_timeout(gpointer data)
{
	GSList *li;
	for(li=applets;li!=NULL;li=g_slist_next(li)) {
		AppletInfo *info = li->data;
		if(info->menu && info->menu_age++>=6 &&
		   !GTK_WIDGET_VISIBLE(info->menu)) {
			gtk_widget_unref(info->menu);
			info->menu = NULL;
			info->menu_age = 0;
		}
		/*if we are allowed to, don't destroy applet menus*/
		if(!global_config.hungry_menus &&
		   info->type == APPLET_MENU) {
			Menu *menu = info->data;
			if(menu->menu && menu->age++>=6 &&
			   !GTK_WIDGET_VISIBLE(menu->menu)) {
				gtk_widget_unref(menu->menu);
				menu->menu = NULL;
				menu->age = 0;
			}
		}
	}
	
	/*skip panel menus if we are memory hungry*/
	if(global_config.hungry_menus)
		return TRUE;
	
	for(li = panel_list; li != NULL; li = g_slist_next(li)) {
		PanelData *pd = li->data;
		if(pd->menu && pd->menu_age++>=6 &&
		   !GTK_WIDGET_VISIBLE(pd->menu)) {
			gtk_widget_unref(pd->menu);
			pd->menu = NULL;
			pd->menu_age = 0;
		}
	}

	return TRUE;
}

/* Some important code copied from PonG */
typedef struct _AppletContainer AppletContainer;
struct _AppletContainer {
	GdkWindow *win;
	gboolean hide_mode;
	int state;
	int x, y, xs, ys;
	int handler;
	GdkPixmap *phsh[4];
	GdkBitmap *phsh_mask[4];
};
AppletContainer phsh = {0};

static void
phsh_kill (void)
{
	int i;
	for (i = 0; i < 4; i++) {
		gdk_pixmap_unref (phsh.phsh[i]);
		gdk_bitmap_unref (phsh.phsh_mask[i]);
	}
	gdk_window_destroy (phsh.win);
	gtk_timeout_remove (phsh.handler);
	memset (&phsh, 0, sizeof (AppletContainer));
}

static gboolean
phsh_move (gpointer data)
{
	int orient, state;
	gboolean change = TRUE;

	phsh.x += phsh.xs;
	phsh.y += phsh.ys;
	if (phsh.x <= -60 ||
	    phsh.x >= gdk_screen_width ()) {
		phsh_kill ();
		return FALSE;
	}
	if (phsh.y <= 0 ||
	    phsh.y >= gdk_screen_height () - 40 ||
	    rand() % (phsh.hide_mode?10:50) == 0)
		phsh.ys = -phsh.ys;

	phsh.state ++;
	if (phsh.state % (phsh.hide_mode?2:4) == 0)
		change = TRUE;
	if (phsh.state >= (phsh.hide_mode?4:8))
		phsh.state = 0;

	state = phsh.state >= (phsh.hide_mode?2:4) ? 1 : 0;
	orient = phsh.xs >= 0 ? 0 : 2;

	if (change) {
		gdk_window_set_back_pixmap (phsh.win, phsh.phsh[orient + state], FALSE);
		gdk_window_shape_combine_mask (phsh.win, phsh.phsh_mask[orient + state], 0, 0);
		gdk_window_clear (phsh.win);
	}

	gdk_window_move (phsh.win, phsh.x, phsh.y);
	gdk_window_raise (phsh.win);

	return TRUE;
}

static void
phsh_reverse (GdkPixbuf *gp)
{
	guchar *pixels = gdk_pixbuf_get_pixels (gp);
	int x, y;
	int rs = gdk_pixbuf_get_rowstride (gp);
#define DOSWAP(x,y) tmp = x; x = y; y = tmp;
	for (y = 0; y < 40; y++, pixels += rs) {
		guchar *p = pixels;
		guchar *p2 = pixels + 60*4 - 4;
		for (x = 0; x < 30; x++, p+=4, p2-=4) {
			guchar tmp;
			DOSWAP (p[0], p2[0]);
			DOSWAP (p[1], p2[1]);
			DOSWAP (p[2], p2[2]);
			DOSWAP (p[3], p2[3]);
		}
	}
#undef DOSWAP
}

/* This dered's the phsh */
static void
phsh_dered(GdkPixbuf *gp)
{
	guchar *pixels = gdk_pixbuf_get_pixels (gp);
	int x, y;
	int rs = gdk_pixbuf_get_rowstride (gp);
	for (y = 0; y < 40; y++, pixels += rs) {
		guchar *p = pixels;
		for (x = 0; x < 60; x++, p+=4) {
			if (p[0] < 55 && p[1] > 100)
			       p[3] = 0;	
		}
	}
}

static GdkFilterReturn
phsh_filter (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	XEvent *xevent = (XEvent *)gdk_xevent;

	if (xevent->type == ButtonPress &&
	    ! phsh.hide_mode) {
		gtk_timeout_remove (phsh.handler);
		phsh.handler = gtk_timeout_add (90, phsh_move, NULL);
		phsh.xs *= 2.0;
		phsh.ys *= 2.5;
		phsh.hide_mode = TRUE;
		if (phsh.x < (gdk_screen_width () / 2))
			phsh.xs *= -1;
	}
	return GDK_FILTER_CONTINUE;
}

/* this checks the screen */
static void
check_screen (void)
{
	GdkWindowAttr attributes;
	char *phsh_file;
	char *name;
	GdkPixbuf *gp, *tmp;

	if (phsh.win != NULL)
		return;

	name = g_strdup_printf ("%cish/%cishanim.png",
				'f', 'f');
	phsh_file = gnome_pixmap_file (name);
	g_free (name);
	if (phsh_file == NULL)
		return;

	tmp = gdk_pixbuf_new_from_file (phsh_file);
	if (tmp == NULL)
		return;

	g_free (phsh_file);

	if (gdk_pixbuf_get_width (tmp) != 180 ||
	    gdk_pixbuf_get_height (tmp) != 40) {
		gdk_pixbuf_unref (tmp);
		return;
	}

	phsh.state = 0;
	phsh.hide_mode = FALSE;

	gp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 60, 40);
	gdk_pixbuf_copy_area (tmp, 60, 0, 60, 40, gp, 0, 0);

	phsh_dered (gp);
	gdk_pixbuf_render_pixmap_and_mask (gp, &phsh.phsh[2], &phsh.phsh_mask[2], 128);
	phsh_reverse (gp);
	gdk_pixbuf_render_pixmap_and_mask (gp, &phsh.phsh[0], &phsh.phsh_mask[0], 128);

	gdk_pixbuf_copy_area (tmp, 120, 0, 60, 40, gp, 0, 0);

	phsh_dered (gp);
	gdk_pixbuf_render_pixmap_and_mask (gp, &phsh.phsh[3], &phsh.phsh_mask[3], 128);
	phsh_reverse (gp);
	gdk_pixbuf_render_pixmap_and_mask (gp, &phsh.phsh[1], &phsh.phsh_mask[1], 128);
	gdk_pixbuf_unref (gp);

	gdk_pixbuf_unref (tmp);
	
	phsh.x = -60;
	phsh.y = (rand() % (gdk_screen_height () - 40 - 2)) + 1;
	phsh.xs = 8;
	phsh.ys = (rand() % 2) + 1;

	attributes.window_type = GDK_WINDOW_TEMP;
	attributes.x = phsh.x;
	attributes.y = phsh.y;
	attributes.width = 60;
	attributes.height = 40;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gdk_rgb_get_visual();
	attributes.colormap = gdk_rgb_get_cmap();
	attributes.event_mask = GDK_BUTTON_PRESS_MASK;

	phsh.win = gdk_window_new (NULL, &attributes,
				   GDK_WA_X | GDK_WA_Y |
				   GDK_WA_VISUAL | GDK_WA_COLORMAP);
	gdk_window_set_back_pixmap (phsh.win, phsh.phsh[0], FALSE);
	gdk_window_shape_combine_mask (phsh.win, phsh.phsh_mask[0], 0, 0);

	/* setup the keys filter */
	gdk_window_add_filter (phsh.win,
			       phsh_filter,
			       NULL);

	gdk_window_show (phsh.win);
	phsh.handler = gtk_timeout_add (150, phsh_move, NULL);
}

static guint screen_check_id = 0;

static gboolean
check_screen_timeout (gpointer data)
{
	screen_check_id = 0;

	check_screen ();

	screen_check_id = gtk_timeout_add (rand()%120*1000,
					   check_screen_timeout, NULL);
	return FALSE;
}

void
start_screen_check (void)
{
	if (screen_check_id > 0)
		gtk_timeout_remove (screen_check_id);

	screen_check_id = 0;
	check_screen ();

	screen_check_id = gtk_timeout_add (rand()%120*1000, check_screen_timeout, NULL);
}

static int
try_config_sync(gpointer data)
{
	panel_config_sync();
	return TRUE;
}

static void
find_kde_directory(void)
{
	int i;
	char *kdedir = g_getenv ("KDEDIR");
	char *try_prefixes[] = {
		"/usr",
		"/opt/kde",
		"/usr/local",
		"/kde",
		NULL
	};
	if(kdedir) {
		kde_menudir = g_concat_dir_and_file(kdedir,"share/applnk");
		kde_icondir = g_concat_dir_and_file(kdedir,"share/icons");
		kde_mini_icondir = g_concat_dir_and_file(kdedir,"share/icons/mini");
		return;
	}

	/* if what configure gave us works use that */
	if(g_file_test(KDE_MENUDIR,G_FILE_TEST_ISDIR)) {
		kde_menudir = g_strdup(KDE_MENUDIR);
		kde_icondir = g_strdup(KDE_ICONDIR);
		kde_mini_icondir = g_strdup(KDE_MINI_ICONDIR);
		return;
	}

	for(i=0;try_prefixes[i];i++) {
		char *try;
		try = g_concat_dir_and_file(try_prefixes[i],"share/applnk");
		if(g_file_test(try,G_FILE_TEST_ISDIR)) {
			kde_menudir = try;
			kde_icondir = g_concat_dir_and_file(try_prefixes[i],"share/icons");
			kde_mini_icondir = g_concat_dir_and_file(try_prefixes[i],"share/icons/mini");
			return;
		}
		g_free(try);
	}

	/* absolute fallback, these don't exist, but we're out of options
	   here */
	kde_menudir = g_strdup(KDE_MENUDIR);
	kde_icondir = g_strdup(KDE_ICONDIR);
	kde_mini_icondir = g_strdup(KDE_MINI_ICONDIR);
}

static void
setup_merge_directory(void)
{
	int len;

	merge_main_dir = gnome_datadir_file("gnome/apps/");
	merge_main_dir_len = merge_main_dir != NULL ? strlen (merge_main_dir) : 0;
	merge_merge_dir = conditional_get_string ("/panel/Merge/Directory",
						  "/etc/X11/applnk/",
						  NULL);

	if (string_empty (merge_merge_dir) ||
	    ! g_file_test(merge_merge_dir, G_FILE_TEST_ISDIR)) {
		g_free(merge_merge_dir);
		merge_merge_dir = NULL;
		return;
	}

	len = strlen(merge_merge_dir);
	if (merge_merge_dir[len-1] != '/') {
		char *tmp = g_strconcat(merge_merge_dir, "/", NULL);
		g_free(merge_merge_dir);
		merge_merge_dir = tmp;
	}
}

static void
setup_visuals (void)
{
	gdk_rgb_init ();
	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
}

static void
kill_free_drawers (void)
{
	GSList *li;
	for (li = panel_list; li != NULL; li = li->next) {
		PanelData *pd = li->data;
		if (IS_DRAWER_WIDGET (pd->panel) &&
		    PANEL_WIDGET (BASEP_WIDGET (pd->panel)->panel)->master_widget == NULL) {
			status_unparent (pd->panel);
			gtk_widget_destroy (pd->panel);
		}
	}
}

/* gets a GdkPixmap for a given X atom name whose value is an X Pixmap */
static GdkPixmap *
get_pixmap_prop (char *prop_id)
{
	Atom prop, type;
	int format;
	unsigned long length, after;
	unsigned char *data;
	Pixmap p;

	prop = XInternAtom(GDK_DISPLAY(), prop_id, True);

	if (prop == None) {
		return NULL;
	}

	XGetWindowProperty(GDK_DISPLAY(), GDK_ROOT_WINDOW(), prop, 0L, 1L, 
			False, AnyPropertyType, &type, &format, &length, 
			&after, &data);

	if (type != XA_PIXMAP) {
		return NULL;
	}

	p = *((Pixmap *)data);

	/* remember not to unref this pixmap */
	return gdk_pixmap_foreign_new(p);
}


/* gets a GdkPixmap that refers to the GNOME desktop pixmap and stores it
 * in desktop_pixmap.
 */
static void
get_desktop_pixmap()
{
	desktop_pixmap = get_pixmap_prop ("_XROOTPMAP_ID");
}

/* an event handler for when the GNOME desktop pixmap changes */
static GdkFilterReturn
desktop_event_filter (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	XEvent *xevent;
	GSList *item;
	PanelWidget *panel;

	xevent = (XEvent *) gdk_xevent;

	if (xevent->type == PropertyNotify && xevent->xproperty.atom == 
			gdk_atom_intern("ESETROOT_PMAP_ID", TRUE)) {

		get_desktop_pixmap ();

		for (item = panels; item; item = g_slist_next (item)) {
			panel = PANEL_WIDGET (item->data);

			if (panel->back_type == PANEL_BACK_TRANSLUCENT) {
				panel_widget_setup_translucent_background (panel);
				panel_widget_force_repaint (panel);
			}
		}


	}
	return GDK_FILTER_CONTINUE;
}

/* set up the event handler */
static void
set_up_desktop_event_handler () 
{
	/* we need to watch for desktop image changes */
	gdk_window_add_filter (GDK_ROOT_PARENT(), desktop_event_filter, NULL);
}	


int
main(int argc, char **argv)
{
	CORBA_ORB orb;
	CORBA_Environment ev;
	gboolean duplicate;
	gchar *real_global_path;
	
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	CORBA_exception_init(&ev);
	orb = gnome_CORBA_init("panel", VERSION,
			       &argc, argv,
			       GNORBA_INIT_SERVER_FUNC, &ev);
	CORBA_exception_free(&ev);
	gnome_window_icon_set_default_from_file (GNOME_ICONDIR"/gnome-panel.png");
	setup_visuals ();

	switch (panel_corba_gtk_init (orb)) {
	case 0: 
		duplicate = FALSE;
		break; /* success */
	case -4: {
		GtkWidget* box = gnome_question_dialog
			(_("I've detected a panel already running.\n"
			   "Start another panel as well?\n" 
			   "(The new panel will not be restarted.)"), NULL, NULL);
		panel_set_dialog_layer (box);
		if (gnome_dialog_run_and_close (GNOME_DIALOG (box)))
			return 0;
		duplicate = TRUE;
		break;
	}
	default: {
		GtkWidget *box = panel_error_dialog
			(_("There was a problem registering the panel "
			   "with the GOAD server.\n"
			   "The panel will now exit."));
		gnome_dialog_run_and_close (GNOME_DIALOG (box));
		return 0;
		break;
	}
	}

	setup_merge_directory();

	find_kde_directory();

	client = gnome_master_client ();

	gnome_client_set_restart_style (client, duplicate 
					? GNOME_RESTART_NEVER 
					: GNOME_RESTART_IMMEDIATELY);

	gnome_client_set_priority (client, 40);


	if (gnome_client_get_flags (client) & GNOME_CLIENT_RESTORED)
		old_panel_cfg_path = g_strdup (gnome_client_get_config_prefix (client));
	else
		old_panel_cfg_path = g_strdup ("/panel.d/default/");

#ifndef PER_SESSION_CONFIGURATION
	real_global_path = gnome_config_get_real_path (old_panel_cfg_path);
	if ( ! panel_file_exists (real_global_path)) {
		g_free (old_panel_cfg_path);
		old_panel_cfg_path = g_strdup ("/panel.d/default/");
	}
	g_free (real_global_path);
#endif /* !PER_SESSION_CONFIGURATION */

	gnome_client_set_global_config_prefix (client, PANEL_CONFIG_PATH);
	
	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
			    GTK_SIGNAL_FUNC (panel_session_save), NULL);
	gtk_signal_connect (GTK_OBJECT (client), "die",
			    GTK_SIGNAL_FUNC (panel_session_die), NULL);

	panel_tooltips = gtk_tooltips_new ();

	xstuff_init ();
	multiscreen_init ();

	gnome_win_hints_init ();

	load_system_wide ();

	/* read, convert and remove old config */
	if ( ! commie_mode)
		convert_old_config ();

	/* set the globals, it is important this is before
	 * init_user_applets */
	load_up_globals ();
	/* this is so the capplet gets the right defaults */
	if ( ! commie_mode)
		write_global_config ();

	gwmh_init ();

	init_fr_chunks ();
	
	init_menus ();

	/* get the GNOME desktop image stuff */
	get_desktop_pixmap ();
	set_up_desktop_event_handler ();
	
	init_user_panels ();
	init_user_applets ();

	kill_free_drawers ();

	load_tornoff ();

	gnome_triggers_do ("Session startup", NULL, "gnome", "login", NULL);

	/*add forbidden lists to ALL panels*/
	g_slist_foreach (panels,
			 (GFunc)panel_widget_add_forbidden,
			 NULL);

	/*this will make the drawers be hidden for closed panels etc ...*/
	send_state_change ();

	/*attempt to sync the config every 10 seconds, only if a change was
	  indicated though*/
	config_sync_timeout = gtk_timeout_add (10*1000, try_config_sync, NULL);

	/* add some timeouts */
	gtk_timeout_add (10*1000, menu_age_timeout, NULL);

	panel_in_startup = FALSE;
	
	/*load these as the last thing to prevent some races any races from
	  starting multiple goad_id's at once are libgnorba's problem*/
	load_queued_externs ();

	status_applet_create_offscreen ();

	gtk_main ();

	return 0;
}
