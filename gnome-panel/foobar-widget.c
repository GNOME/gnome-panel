/* GNOME panel: foobar widget
 * Copyright 1999 Helix Code, Inc.
 *
 * Author: Jacob Berkman
 *
 */

/* since IS_BASEP_WIDGET() is used throughout, it makes life easier if we
 * have a GtkWindow of our own.
 */

#include <config.h>

#include "foobar-widget.h"

#include "gnome-run.h"
#include "menu.h"
#include "menu-util.h"
#include "session.h"
/*#include "gdk-bufmap.h"*/
#include "panel-widget.h"
#include "xstuff.h"
#include "basep-widget.h"
#include "panel_config_global.h"
#include "drawer-widget.h"

#define SMALL_ICON_SIZE 20

extern GlobalConfig global_config;
extern GList *panel_list;

static GtkWindowClass *foobar_widget_parent_class = NULL;

static void foobar_widget_class_init (FoobarWidgetClass *klass);
static void foobar_widget_init (FoobarWidget *foo);

static GtkWidget *das_global_foobar = NULL;

GtkType
foobar_widget_get_type ()
{
	static GtkType foobar_widget_type = 0;

	if (!foobar_widget_type) {
		GtkTypeInfo foobar_widget_info = {
			"FoobarWidget",
			sizeof (FoobarWidget),
			sizeof (FoobarWidgetClass),
			(GtkClassInitFunc) foobar_widget_class_init,
			(GtkObjectInitFunc) foobar_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		foobar_widget_type = gtk_type_unique (gtk_window_get_type (),
						      &foobar_widget_info);

	}

	return foobar_widget_type;
}

static void
foobar_widget_class_init (FoobarWidgetClass *klass)
{
	foobar_widget_parent_class = gtk_type_class (FOOBAR_WIDGET_TYPE);
}

static GtkWidget *
pixmap_menu_item_new (const char *text, const char *try_file)
{
	GtkWidget *item;
	GtkWidget *label;

	item = gtk_pixmap_menu_item_new ();

	if (try_file && gnome_preferences_get_menus_have_icons ()) {
		GtkWidget *pixmap;
		pixmap = gnome_stock_pixmap_widget_at_size (
			NULL, try_file, SMALL_ICON_SIZE, SMALL_ICON_SIZE);

		if(pixmap) {
			gtk_widget_show (pixmap);
			gtk_pixmap_menu_item_set_pixmap
				(GTK_PIXMAP_MENU_ITEM (item), pixmap);
		}
	}

	if (text) {
		label = gtk_label_new (text);
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_container_add (GTK_CONTAINER (item), label);
	}

	return item;
}

static void
url_show (GtkWidget *w, const char *url)
{
	gnome_url_show (url);
}

static GtkWidget *
url_menu_item (const char *label, const char *url, const char *pixmap)
{
	GtkWidget *item;
	item = pixmap_menu_item_new (label, pixmap);
	if (!label) gtk_widget_set_sensitive (item, FALSE);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (url_show), 
			    (gpointer *)url);
	return item;
}

#if 0
static void
run_cb (GtkWidget *w, gpointer data)
{
	show_run_dialog ();
}
#endif

static void
about_cb (GtkWidget *w, gpointer data)
{
	char *v[2] = { "gnome-about" };
	gnome_execute_async (NULL, 1, v);
}

static void
gmc_client (GtkWidget *w, gpointer data)
{
	char *v[3] = { "gmc-client" };
	v[1] = data;
	gnome_execute_async (NULL, 2, v);
}

static void
gnomecal_client (GtkWidget *w, gpointer data)
{
	char *v[4] = { "gnomecal", "--view" };
	v[2] = data;
	gnome_execute_async (NULL, 3, v);
}

static GtkWidget *
append_gnome_menu (GtkWidget *menu_bar)
{
	GtkWidget *item;
	GtkWidget *menu;
	int i;
	char *url[][3] = {
		{ N_("News"),
		  "http://gnotices.gnome.org/gnome-news/",
		  "gnome-news.png" },
		{ N_("FAQ"),
		  "http://www.gnome.org/gnomefaq/html/",
		  GNOME_STOCK_PIXMAP_HELP },
		{ N_("Mailing Lists"),
		  "http://www.gnome.org/mailing-lists/archives/",
		  GNOME_STOCK_PIXMAP_MAIL },
		{ NULL, "" },
		{ N_("Software"),
		  "http://www.gnome.org/applist/list-martin.phtml",
		  GNOME_STOCK_PIXMAP_SAVE },
		{ N_("Development"),
		  "http://developer.gnome.org/",
		  "gnome-devel.png" },
		{ N_("Bug Tracking System"),
		  "http://bugs.gnome.org/",
		  "bug-buddy.png" },
		{ NULL }
	};
	
	
	menu = gtk_menu_new ();
	
	for (i=0; url[i][1]; i++)
		gtk_menu_append (GTK_MENU (menu),
				 url_menu_item (_(url[i][0]), url[i][1],
						url[i][2]));
		
	add_menu_separator (menu);
#if 0		
	item = pixmap_menu_item_new (_("Run..."), GNOME_STOCK_MENU_EXEC);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (run_cb), NULL);

	add_menu_separator (menu);
#endif
	item = pixmap_menu_item_new (_("About GNOME"), GNOME_STOCK_MENU_ABOUT);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (about_cb), NULL);

#if 0
	item = pixmap_menu_item_new (_("Log Out"), "gnome-term-night.png");
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (panel_quit), 0);
	setup_internal_applet_drag (item, "LOGOUT:NEW");
#endif

	/*item = gtk_menu_item_new_with_label ("G N O M E");*/
	item = pixmap_menu_item_new ("", "gnome-spider.png");

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), item);
	return item;
}

static void
append_gmc_item (GtkWidget *menu, const char *label, char *flag)
{
	GtkWidget *item = gtk_menu_item_new_with_label (label);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (gmc_client), flag);
}

static void
append_desktop_menu (GtkWidget *menu_bar)
{
	GtkWidget *menu, *item;
	char *char_tmp;
	int i;
	static char *arrange[] = { 
		N_("By Name"), "--arrange-desktop-icons=name",
		N_("By Type"), "--arrange-desktop-icons=type",
		N_("By Size"), "--arrange-desktop-icons=size",
		N_("By Time Last Accessed"), "--arrange-desktop-icons=atime",
		N_("By Time Last Modified"), "--arrange-desktop-icons=mtime",
		N_("By Time Last Changed"),  "--arrange-desktop-icons=ctime",
		NULL
	};

	menu = gtk_menu_new ();

	for (i=0; arrange[i]; i+=2)
		append_gmc_item (menu, _(arrange[i]), arrange[i+1]);

	item = gtk_menu_item_new_with_label (_("Arrange Icons"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

	menu = gtk_menu_new ();

	gtk_menu_append (GTK_MENU (menu), item);
	add_menu_separator (menu);

	append_gmc_item (menu, _("Rescan Desktop Directory"), "--rescan-desktop");
	append_gmc_item (menu, _("Rescan Desktop Devices"), "--rescan-desktop-devices");



	add_menu_separator (menu);
	


	char_tmp = gnome_is_program_in_path ("xscreensaver");
	if (char_tmp) {	
		item = pixmap_menu_item_new (_("Lock Screen"), 
					       "gnome-lockscreen.png");
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC (panel_lock), 0);
		setup_internal_applet_drag(item, "LOCK:NEW");

		g_free (char_tmp);
	}

	item = pixmap_menu_item_new (_("Log Out"), "gnome-term-night.png");
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC(panel_quit), 0);
	setup_internal_applet_drag (item, "LOGOUT:NEW");

	item = gtk_menu_item_new_with_label (_(" Desktop "));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), item);
}

static void
append_folder_menu (GtkWidget *menu_bar, const char *label,
		    const char *pixmap, gboolean system, const char *path)
{
	GtkWidget *item, *menu;
	char *real_path;

	real_path = system 
		? gnome_unconditional_datadir_file (path)
		: gnome_util_home_file (path);

	if (!real_path) {
		g_warning (_("can't fine real path"));
		return;
	}

	menu = create_fake_menu_at (real_path, FALSE, label, NULL, FALSE);
	g_free (real_path);

	if (!menu) {
		g_warning (_("menu wasn't created"));
		return;
	}

	if (pixmap)
		item = pixmap_menu_item_new (label, pixmap);
	else
		item = gtk_menu_item_new_with_label (label);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), item);

	gtk_signal_connect (GTK_OBJECT (menu),"show",
			    GTK_SIGNAL_FUNC (submenu_to_display),
			    NULL);
}

static void
append_gnomecal_item (GtkWidget *menu, const char *label, const char *flag)
{
	GtkWidget *item = gtk_menu_item_new_with_label (label);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (gnomecal_client), (gpointer)flag);
}

static int
timeout_cb (gpointer data)
{
	GtkWidget *label = GTK_WIDGET (data);
	struct tm *das_tm;
	time_t das_time;
	char hour[20];

	time (&das_time);
	das_tm = localtime (&das_time);

	if (strftime (hour, 20, _("%I:%M:%S %p"), das_tm) == 20)
		hour[19] = '\0';

	gtk_label_set_text (GTK_LABEL (label), hour);

	return TRUE;
}

static void
timeout_remove (GtkWidget *w, gpointer data)
{
	gtk_timeout_remove (GPOINTER_TO_INT (data));
}

static GtkWidget *
append_clock_menu (GtkWidget *menu_bar)
{
	GtkWidget *item, *menu, *label;
	gint timeout;
	int i;
	const char *cals[] = { 
		N_("Today"),      "dayview",
		N_("This Week"),  "weekview",
		N_("This Month"), "monthview",
		NULL
	};

	menu = gtk_menu_new ();
	
#if 0 /* put back when evolution can do this */
	item = gtk_menu_item_new_with_label (_("Add appointement..."));
	gtk_menu_append (GTK_MENU (menu), item);

	add_menu_separator (menu);
#endif

	for (i=0; cals[i]; i+=2)
		append_gnomecal_item (menu, _(cals[i]), cals[i+1]);

	item = gtk_menu_item_new ();
	label = gtk_label_new (_(""));
	timeout = gtk_timeout_add (1000, timeout_cb, label);
	gtk_signal_connect (GTK_OBJECT (label), "destroy",
			    GTK_SIGNAL_FUNC (timeout_remove),
			    GINT_TO_POINTER (timeout));
	gtk_container_add (GTK_CONTAINER (item), label);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), item);

	return item;
}

void
foobar_widget_update_winhints (GtkWidget *foo, gpointer ignored)
{
	if (!FOOBAR_WIDGET (foo)->compliant_wm)
		return;

	gdk_window_set_hints (foo->window, 0, 0, 
			      0, 0, 0, 0, GDK_HINT_POS);

	gnome_win_hints_set_expanded_size (foo, 0, 0, 0, 0);
	gdk_window_set_decorations (foo->window, 0);
	gnome_win_hints_set_state (foo, WIN_STATE_STICKY |
				   WIN_STATE_FIXED_POSITION);
	
	gnome_win_hints_set_hints (foo, GNOME_PANEL_HINTS | WIN_HINTS_DO_NOT_COVER);	
	gnome_win_hints_set_layer (foo, WIN_LAYER_DOCK);
}


static void
foobar_widget_init (FoobarWidget *foo)
{
	/*gchar *path;*/
	GtkWindow *window = GTK_WINDOW (foo);
	/*GtkWidget *bufmap;*/
	GtkWidget *menu_bar;
	GtkWidget *menu, *menuitem;
	/*GtkWidget *align;*/
	gint flags;

	foo->compliant_wm = xstuff_is_compliant_wm ();

	window->allow_shrink = TRUE;
	window->allow_grow   = TRUE;
	window->auto_shrink  = TRUE;

	gtk_signal_connect (GTK_OBJECT (foo), "delete_event",
			    GTK_SIGNAL_FUNC (gtk_true), NULL);
	gtk_signal_connect (GTK_OBJECT (foo), "realize",
			    GTK_SIGNAL_FUNC (foobar_widget_update_winhints),
			    NULL);

	gtk_widget_set_usize (GTK_WIDGET (foo),
			      gdk_screen_width (), -2);

	foo->hbox = gtk_hbox_new (FALSE, 0);

#if 0	
	path = gnome_pixmap_file ("panel/corner1.png");
	bufmap = gnome_pixmap_new_from_file (path);
	g_free (path);
	align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
	gtk_container_add (GTK_CONTAINER (align), bufmap);
	gtk_box_pack_start (GTK_BOX (foo->hbox), align, FALSE, FALSE, 0);
#endif

	menu_bar = gtk_menu_bar_new ();	
	gtk_menu_bar_set_shadow_type (GTK_MENU_BAR (menu_bar),
				      GTK_SHADOW_NONE);
	
	
	menuitem = pixmap_menu_item_new (_("Programs"), GNOME_STOCK_MENU_ABOUT);
	flags = (get_default_menu_flags() & 
		 ~(MAIN_MENU_SYSTEM_SUB | MAIN_MENU_USER | MAIN_MENU_USER_SUB |
		   MAIN_MENU_PANEL | MAIN_MENU_PANEL_SUB | MAIN_MENU_DESKTOP |
		   MAIN_MENU_DESKTOP_SUB)) | MAIN_MENU_SYSTEM;
	menu = create_root_menu (TRUE, flags, TRUE, FALSE, FALSE);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), menuitem);

	append_folder_menu  (menu_bar, _("Settings"),  NULL, TRUE,
			     "gnome/apps/Settings/.");
	append_desktop_menu (menu_bar);
	append_folder_menu  (menu_bar, _("Favorites"), NULL, FALSE, "apps/.");
	gtk_box_pack_start (GTK_BOX (foo->hbox), menu_bar, FALSE, FALSE, 0);
	
	
	/* panel widget */
	foo->panel = panel_widget_new (FALSE, PANEL_HORIZONTAL,
				       SIZE_TINY, PANEL_BACK_NONE,
				       NULL, FALSE, FALSE, FALSE, TRUE, NULL);
	PANEL_WIDGET (foo->panel)->panel_parent = GTK_WIDGET (foo);
	PANEL_WIDGET (foo->panel)->drop_widget = GTK_WIDGET (foo);

	gtk_container_add (GTK_CONTAINER (foo->hbox), foo->panel);

	gtk_object_set_data (GTK_OBJECT (menu_bar), "menu_panel", foo->panel);

#if 0
	path = gnome_pixmap_file ("panel/corner2.png");
	bufmap = gnome_pixmap_new_from_file (path);
	g_free (path);
	align = gtk_alignment_new (1.0, 0.0, 1.0, 0.0);
	gtk_container_add (GTK_CONTAINER (align), bufmap);
	gtk_box_pack_end (GTK_BOX (foo->hbox), align, FALSE, FALSE, 0);
#endif

	menu_bar = gtk_menu_bar_new ();
	gtk_menu_bar_set_shadow_type (GTK_MENU_BAR (menu_bar),
				      GTK_SHADOW_NONE);
	append_clock_menu (menu_bar);
	append_gnome_menu (menu_bar);


	gtk_box_pack_end (GTK_BOX (foo->hbox), menu_bar, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (foo), foo->hbox);
	gtk_widget_show_all (foo->hbox);
}

#warning This is probably hackish
static void
queue_panel_resize (gpointer data, gpointer user_data)
{
	PanelData *pd = data;
	GtkWidget *panel;

	g_assert (pd);

	panel = pd->panel;

	g_return_if_fail (GTK_IS_WIDGET (panel));

	if (!IS_DRAWER_WIDGET (panel) && !IS_FOOBAR_WIDGET (panel))
		gtk_widget_queue_resize (panel);
}

static void
queue_panel_resizes (GtkWidget *w, gpointer data)
{
	if (!GTK_WIDGET_REALIZED (w))
		return;
	g_list_foreach (panel_list, queue_panel_resize, data);
}

GtkWidget *
foobar_widget_new (void)
{
	g_return_val_if_fail (das_global_foobar == NULL, NULL);
	das_global_foobar =  gtk_type_new (FOOBAR_WIDGET_TYPE);
	gtk_signal_connect_after (GTK_OBJECT (das_global_foobar), "size-allocate",
				  GTK_SIGNAL_FUNC (queue_panel_resizes), NULL);
	gtk_signal_connect (GTK_OBJECT (das_global_foobar), "destroy",
			    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			    &das_global_foobar);
	return das_global_foobar;
}

gboolean
foobar_widget_exists ()
{
	return (das_global_foobar != NULL);
}

gint
foobar_widget_get_height ()
{
	return (das_global_foobar && GTK_WIDGET_REALIZED (das_global_foobar)) 
		? das_global_foobar->allocation.height : 0; 
}
