/* GNOME panel: foobar widget
 * Copyright 1999,2000 Helix Code, Inc.
 * Copyright 2000 Eazel, Inc.
 *
 * Author: Jacob Berkman
 *
 */

/* since IS_BASEP_WIDGET() is used throughout, it makes life easier if we
 * have a GtkWindow of our own.
 */

#include <config.h>
#include <unistd.h>

#include "foobar-widget.h"

#include "menu.h"
#include "menu-util.h"
#include "session.h"
#include "panel-widget.h"
#include "xstuff.h"
#include "basep-widget.h"
#include "panel_config_global.h"
#include "panel-util.h"
#include "drawer-widget.h"
#include "gnome-run.h"
#include "scroll-menu.h"
#include "gwmh.h"
#include "tasklist_icon.h"
#include "multiscreen-stuff.h"

#define SMALL_ICON_SIZE 20

extern GlobalConfig global_config;
extern GSList *panel_list;

extern GtkTooltips *panel_tooltips;

static void foobar_widget_class_init	(FoobarWidgetClass	*klass);
static void foobar_widget_init		(FoobarWidget		*foo);
static void foobar_widget_realize	(GtkWidget		*w);
static void foobar_widget_destroy	(GtkObject		*o);
static void foobar_widget_size_allocate	(GtkWidget		*w,
					 GtkAllocation		*alloc);
static gboolean foobar_leave_notify	(GtkWidget *widget,
					 GdkEventCrossing *event);
static gboolean foobar_enter_notify	(GtkWidget *widget,
					 GdkEventCrossing *event);
static void append_task_menu (FoobarWidget *foo, GtkMenuBar *menu_bar);
static void setup_task_menu (FoobarWidget *foo);

static GList *foobars = NULL;

static GtkWindowClass *parent_class = NULL;



GtkType
foobar_widget_get_type (void)
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
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

        parent_class = gtk_type_class (gtk_window_get_type ());

	object_class->destroy = foobar_widget_destroy;

	widget_class->realize = foobar_widget_realize;
	widget_class->size_allocate = foobar_widget_size_allocate;
	widget_class->enter_notify_event = foobar_enter_notify;
	widget_class->leave_notify_event = foobar_leave_notify;
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
add_tearoff (GtkMenu *menu)
{
	GtkWidget *item;

	if (!gnome_preferences_get_menus_have_tearoff ())
		return;
	
	item = gtk_tearoff_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_prepend (menu, item);
}

static gboolean
foobar_leave_notify (GtkWidget *widget,
		     GdkEventCrossing *event)
{
	if (GTK_WIDGET_CLASS (parent_class)->leave_notify_event)
		GTK_WIDGET_CLASS (parent_class)->leave_notify_event (widget,
								     event);

	return FALSE;
}

static gboolean
foobar_enter_notify (GtkWidget *widget,
		     GdkEventCrossing *event)
{
	if (GTK_WIDGET_CLASS (parent_class)->enter_notify_event)
		GTK_WIDGET_CLASS (parent_class)->enter_notify_event (widget,
								     event);

	if (global_config.autoraise)
		gdk_window_raise (widget->window);

	return FALSE;
}

#if 0
static void
url_show (GtkWidget *w, const char *url)
{
	gnome_url_show (_(url));
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

static void
about_cb (GtkWidget *w, gpointer data)
{
	char *v[2] = { "gnome-about" };
	gnome_execute_async (g_get_home_dir (), 1, v);
}
#endif

static void
gmc_client (GtkWidget *w, gpointer data)
{
	char *v[3] = { "gmc-client" };
	v[1] = data;
	if(gnome_execute_async (g_get_home_dir (), 2, v) < 0)
		panel_error_dialog(_("Cannot execute the gmc-client program,\n"
				     "perhaps gmc is not installed"));
}

static void
gnomecal_client (GtkWidget *w, gpointer data)
{
	char *v[4] = { "gnomecal", "--view" };
	v[2] = data;
	if(gnome_execute_async (g_get_home_dir (), 3, v) < 0)
		panel_error_dialog(_("Cannot execute the gnome calendar,\n"
				     "perhaps it's not installed.\n"
				     "It is in the gnome-pim package."));
}

#if 0
static GtkWidget *
append_gnome_menu (FoobarWidget *foo, GtkWidget *menu_bar)
{
	GtkWidget *item;
	GtkWidget *menu;
	int i;
	char *url[][3] = {
		{ N_("News (www)"),                N_("http://gnotices.gnome.org/gnome-news/"),          "gnome-news.png" },
		{ N_("FAQ (www)"),                 N_("http://www.gnome.org/gnomefaq/html/"),            GNOME_STOCK_PIXMAP_HELP },
		{ N_("Mailing Lists (www)"),       N_("http://mail.gnome.org/mailman/listinfo/"),        GNOME_STOCK_PIXMAP_MAIL },
		{ NULL, "" },
		{ N_("Software (www)"),            N_("http://www.gnome.org/applist/list-martin.phtml"), GNOME_STOCK_PIXMAP_SAVE },
		{ N_("Development (www)"),         N_("http://developer.gnome.org/"),                    "gnome-devel.png" },
		{ N_("Bug Tracking System (www)"), N_("http://bugzilla.gnome.org/"),                         "bug-buddy.png" },
		{ NULL }
	};
	
	
	menu = hack_scroll_menu_new ();
	
	for (i=0; url[i][1]; i++)
		gtk_menu_append (GTK_MENU (menu),
				 url_menu_item (_(url[i][0]), url[i][1],
						url[i][2]));
		
	add_menu_separator (menu);

	item = pixmap_menu_item_new (_("About GNOME"), GNOME_STOCK_MENU_ABOUT);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (about_cb), NULL);

	add_tearoff (GTK_MENU (menu));

	item = pixmap_menu_item_new ("", "gnome-spider.png");

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), item);
	return item;
}
#endif

static GtkWidget *
append_gmc_item (GtkWidget *menu, const char *label, char *flag)
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_label (label);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (gmc_client), flag);

	return item;
}

static gboolean
display_gmc_menu (void)
{
	static gboolean checked_path = FALSE;
	static gboolean got_gmc = FALSE;

	if ( ! checked_path) {
		char *gmc_client;
		gmc_client = panel_is_program_in_path ("gmc-client");

		if (gmc_client == NULL)
			got_gmc = FALSE;
		else
			got_gmc = TRUE;

		g_free (gmc_client);
		checked_path = TRUE;
	}

	if ( ! got_gmc)
		return FALSE;

	if (xstuff_nautilus_desktop_present ())
		return FALSE;

	return TRUE;
}

static void
desktop_selected (GtkWidget *widget, gpointer data)
{
	GList *gmc_menu_items = data;
	GList *li;
	gboolean gmc_menu = display_gmc_menu ();

	for (li = gmc_menu_items; li != NULL; li = li->next) {
		GtkWidget *item = li->data;

		if (gmc_menu)
			gtk_widget_show (item);
		else
			gtk_widget_hide (item);
	}
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
	GList *gmc_menu_items = NULL;

	menu = hack_scroll_menu_new ();

	for (i=0; arrange[i]; i+=2)
		append_gmc_item (menu, _(arrange[i]), arrange[i+1]);

	item = gtk_menu_item_new_with_label (_("Arrange Icons"));
	gmc_menu_items = g_list_prepend (gmc_menu_items, item);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

	add_tearoff (GTK_MENU (menu));

	menu = hack_scroll_menu_new ();

	gtk_menu_append (GTK_MENU (menu), item);

	item = add_menu_separator (menu);
	gmc_menu_items = g_list_prepend (gmc_menu_items, item);

	item = append_gmc_item (menu, _("Rescan Desktop Directory"),
				"--rescan-desktop");
	gmc_menu_items = g_list_prepend (gmc_menu_items, item);
	item = append_gmc_item (menu, _("Rescan Desktop Devices"),
				"--rescan-desktop-devices");
	gmc_menu_items = g_list_prepend (gmc_menu_items, item);

	item = add_menu_separator (menu);
	gmc_menu_items = g_list_prepend (gmc_menu_items, item);
	
	char_tmp = panel_is_program_in_path ("xscreensaver");
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

	add_tearoff (GTK_MENU (menu));

	item = gtk_menu_item_new_with_label (_(" Desktop "));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), item);

	gtk_signal_connect_full (GTK_OBJECT (menu), "show",
				 GTK_SIGNAL_FUNC (desktop_selected),
				 NULL,
				 gmc_menu_items,
				 (GtkDestroyNotify) g_list_free,
				 FALSE,
				 FALSE);
}

static GtkWidget *
append_folder_menu (GtkWidget *menu_bar, const char *label,
		    const char *pixmap, gboolean system, const char *path)
{
	GtkWidget *item, *menu;
	char *real_path;

	real_path = system 
		? gnome_unconditional_datadir_file (path)
		: gnome_util_home_file (path);

	if (real_path == NULL) {
		g_warning (_("can't find real path"));
		return NULL;
	}

	menu = create_fake_menu_at (real_path,
				    FALSE /* applets */,
				    FALSE /* launcher_add */,
				    FALSE /* favourites_add */,
				    label /* dir_name */,
				    NULL /* pixmap_name */,
				    FALSE /* title */);
	g_free (real_path);
	if (path != NULL && strcmp (path, "apps") == 0)
		/* This will add the add submenu thingie */
		start_favourites_menu (menu, TRUE /* fake_submenus */);

	if (menu == NULL) {
		g_warning (_("menu wasn't created"));
		return NULL;
	}

	if (pixmap)
		item = pixmap_menu_item_new (label, pixmap);
	else
		item = gtk_menu_item_new_with_label (label);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), item);

	gtk_signal_connect (GTK_OBJECT (menu), "show",
			    GTK_SIGNAL_FUNC (submenu_to_display),
			    NULL);

	return menu;
}

static void
append_gnomecal_item (GtkWidget *menu, const char *label, const char *flag)
{
	GtkWidget *item = gtk_menu_item_new_with_label (label);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (gnomecal_client), (gpointer)flag);
}

static void
update_clock (FoobarWidget *foo)
{
	static int day = 0;
	struct tm *das_tm;
	time_t das_time;
	char hour[256];

	if (foo->clock_label == NULL)
		return;

	time (&das_time);
	das_tm = localtime (&das_time);

	if (das_tm->tm_mday != day) {
		if(strftime(hour, sizeof(hour), _("%A %B %d"), das_tm) == 0) {
			/* according to docs, if the string does not fit, the
			 * contents of tmp2 are undefined, thus just use
			 * ??? */
			strcpy(hour, "???");
		}
		hour[sizeof(hour)-1] = '\0'; /* just for sanity */

		gtk_tooltips_set_tip (panel_tooltips, foo->clock_ebox,
				      hour, NULL);

		day = das_tm->tm_mday;
	}

	if(strftime(hour, sizeof(hour), foo->clock_format, das_tm) == 0) {
		/* according to docs, if the string does not fit, the
		 * contents of tmp2 are undefined, thus just use
		 * ??? */
		strcpy(hour, "???");
	}
	hour[sizeof(hour)-1] = '\0'; /* just for sanity */

	gtk_label_set_text (GTK_LABEL (foo->clock_label), hour);
}

static int
timeout_cb (gpointer data)
{
	FoobarWidget *foo = FOOBAR_WIDGET (data);

	if (foo->clock_label == NULL) {
		foo->clock_timeout = 0;
		return FALSE;
	}

	update_clock (foo);

	return TRUE;
}

static void
set_fooclock_format (GtkWidget *w, char *format)
{
	GList *li;

	for (li = foobars; li != NULL; li = li->next) {
		foobar_widget_set_clock_format (FOOBAR_WIDGET (li->data),
						_(format));
	}
}

static void
append_format_item (GtkWidget *menu, const char *format)
{
	char hour[256];
	GtkWidget *item;
	struct tm *das_tm;
	time_t das_time = 43200;

	das_tm = localtime (&das_time);
	if (strftime (hour, sizeof(hour), _(format), das_tm) == 0) {
		/* according to docs, if the string does not fit, the
		 * contents of tmp2 are undefined, thus just use
		 * ??? */
		strcpy(hour, "???");
	}
	hour[sizeof(hour)-1] = '\0'; /* just for sanity */

	item = gtk_menu_item_new_with_label (hour);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (set_fooclock_format),
			    (gpointer)format);
}

static void
set_time_cb (GtkWidget *menu_item, char *path)
{
	char *v[2] = { path };
	
	if (gnome_execute_async (g_get_home_dir (), 1, v) < 0)
		panel_error_dialog (_("Could not call time-admin\n"
						  "Perhaps time-admin is not installed"));
}

static GtkWidget *
append_clock_menu (FoobarWidget *foo, GtkWidget *menu_bar)
{
	GtkWidget *item, *menu, *menu2;
	gchar *time_admin_path;
	int i;
	const char *cals[] = { 
		N_("Today"),      "dayview",
		N_("This Week"),  "weekview",
		N_("This Month"), "monthview",
		NULL
	};

	const char *formats[] = {
		N_("%H:%M"),
		N_("%H:%M:%S"),
		N_("%l:%M %p"),
		N_("%l:%M:%S %p"),
		NULL
	};

	menu = hack_scroll_menu_new ();
	
#if 0 /* put back when evolution can do this */
	item = gtk_menu_item_new_with_label (_("Add appointement..."));
	gtk_menu_append (GTK_MENU (menu), item);

	add_menu_separator (menu);
#endif

	time_admin_path = gnome_is_program_in_path ("time-admin");
	if (time_admin_path) {
		item = gtk_menu_item_new_with_label (_("Set Time"));
		gtk_signal_connect (GTK_OBJECT (item), "activate",
						GTK_SIGNAL_FUNC (set_time_cb), time_admin_path);
		gtk_menu_append (GTK_MENU (menu), item);
		add_menu_separator (menu);
	}

	for (i=0; cals[i]; i+=2)
		append_gnomecal_item (menu, _(cals[i]), cals[i+1]);

	add_menu_separator (menu);

	menu2 = hack_scroll_menu_new ();
	for (i=0; formats[i]; i++)
		append_format_item (menu2, formats[i]);

	add_tearoff (GTK_MENU (menu2));

	item = gtk_menu_item_new_with_label (_("Format"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu2);
	gtk_menu_append (GTK_MENU (menu), item);

	add_tearoff (GTK_MENU (menu));

	item = gtk_menu_item_new ();

	foo->clock_label = gtk_label_new ("");
	foo->clock_timeout = gtk_timeout_add (1000, timeout_cb, foo);

	foo->clock_ebox = item;
	gtk_container_add (GTK_CONTAINER (item), foo->clock_label);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), item);

	return item;
}

void
foobar_widget_global_set_clock_format (const char *format)
{
	GList *li;

	for (li = foobars; li != NULL; li = li->next) {
		foobar_widget_set_clock_format (FOOBAR_WIDGET (li->data),
						format);
	}
}

void
foobar_widget_set_clock_format (FoobarWidget *foo, const char *clock_format)
{
	g_free (foo->clock_format);
	foo->clock_format = g_strdup (clock_format);

	update_clock (foo);
}

void
foobar_widget_update_winhints (FoobarWidget *foo)
{
	GtkWidget *w = GTK_WIDGET (foo);
	GnomeWinLayer layer;

	if ( ! foo->compliant_wm)
		return;

	xstuff_set_pos_size (w->window,
			     multiscreen_x (foo->screen),
			     multiscreen_y (foo->screen),
			     w->allocation.width,
			     w->allocation.height);

	gnome_win_hints_set_expanded_size (w, 0, 0, 0, 0);
	gdk_window_set_decorations (w->window, 0);
	gnome_win_hints_set_state (w, WIN_STATE_STICKY |
				   WIN_STATE_FIXED_POSITION);
	
	gnome_win_hints_set_hints (w, GNOME_PANEL_HINTS |
				   WIN_HINTS_DO_NOT_COVER);	
	if (global_config.normal_layer) {
		layer = WIN_LAYER_NORMAL;
	} else if (global_config.keep_bottom) {
		layer = WIN_LAYER_BELOW;
	} else {
		layer = WIN_LAYER_DOCK;
	}
	gnome_win_hints_set_layer (w, layer);
}

static void
foobar_widget_realize (GtkWidget *w)
{
	gtk_window_set_wmclass (GTK_WINDOW (w),
				"panel_window", "Panel");

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		GTK_WIDGET_CLASS (parent_class)->realize (w);

	foobar_widget_update_winhints (FOOBAR_WIDGET (w));
	xstuff_set_no_group_and_no_input (w->window);

	setup_task_menu (FOOBAR_WIDGET (w));
}

static void
programs_menu_to_display(GtkWidget *menu)
{
	if(menu_need_reread(menu)) {
		int flags;

		while(GTK_MENU_SHELL(menu)->children)
			gtk_widget_destroy(GTK_MENU_SHELL(menu)->children->data);
		flags = (get_default_menu_flags() & 
			 ~(MAIN_MENU_SYSTEM_SUB | MAIN_MENU_USER |
			   MAIN_MENU_USER_SUB | MAIN_MENU_PANEL |
			   MAIN_MENU_PANEL_SUB | MAIN_MENU_DESKTOP |
			   MAIN_MENU_DESKTOP_SUB)) |
			MAIN_MENU_SYSTEM;
		create_root_menu (menu, TRUE, flags, TRUE, FALSE, FALSE);
	}
}

static void
set_the_task_submenu (FoobarWidget *foo, GtkWidget *item)
{
	foo->task_menu = hack_scroll_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), foo->task_menu);
	/*g_message ("setting...");*/
}

static void
focus_task (GtkWidget *w, GwmhTask *task)
{
	gwmh_desk_set_current_area (task->desktop, task->harea, task->varea);
	if (GWMH_TASK_ICONIFIED (task)) 
		gwmh_task_deiconify (task);
	gwmh_task_show  (task);
	gwmh_task_raise (task);
	gwmh_task_focus (task);
}

static void
add_task (GwmhTask *task, FoobarWidget *foo)
{
	GtkWidget *item, *label;
	char *title = NULL;
	int slen;
	GtkWidget *pixmap  = NULL;

	static GwmhDesk *desk = NULL;

	g_assert (foo->tasks);

	if (GWMH_TASK_SKIP_WINLIST (task))
		return;
	if (task->name != NULL) {
		slen = strlen (task->name);
		if (slen > 443)
			title = g_strdup_printf ("%.420s...%s", task->name, task->name+slen-20);
		else
			title = g_strdup (task->name);
	} else {
		/* Translators: Task with no name, should not really happen, so
		 * this should signal that the panel is confused by this task
		 * (thus question marks) */
		title = g_strdup (_("???"));
	}

	if (GWMH_TASK_ICONIFIED (task)) {
		char *tmp = title;
		title = g_strdup_printf ("[%s]", title);
		g_free (tmp);
	}
	
	item = gtk_pixmap_menu_item_new ();
	pixmap = get_task_icon (task, GTK_WIDGET (foo));
	if (pixmap != NULL) {
		gtk_widget_show (pixmap);
		gtk_pixmap_menu_item_set_pixmap (GTK_PIXMAP_MENU_ITEM (item),
						 pixmap);
	}

	label = gtk_label_new (title);
	g_free (title);

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_container_add (GTK_CONTAINER (item), label);
	g_hash_table_insert (foo->tasks, task, item);
	gtk_signal_connect (GTK_OBJECT (item), "activate", 
			    GTK_SIGNAL_FUNC (focus_task),
			    task);
	gtk_widget_show_all (item);
	
	if (!desk)
		desk = gwmh_desk_get_config ();

	if (task->desktop == desk->current_desktop &&
	    task->harea   == desk->current_harea   &&
	    task->varea   == desk->current_varea)
		gtk_menu_prepend (GTK_MENU (foo->task_menu), item);
	else
		gtk_menu_append (GTK_MENU (foo->task_menu), item);
}

static void
create_task_menu (GtkWidget *w, gpointer data)
{
	FoobarWidget *foo = FOOBAR_WIDGET (data);
	GList *tasks = gwmh_task_list_get ();
	GList *list;
	GtkWidget *separator;

	/*g_message ("creating...");*/
	foo->tasks = g_hash_table_new (g_direct_hash, g_direct_equal);

	separator = add_menu_separator (foo->task_menu);

	g_list_foreach (tasks, (GFunc)add_task, foo);

	list = g_list_last (GTK_MENU_SHELL (foo->task_menu)->children);

	if (list != NULL &&
	    separator == list->data) {
		/* if the separator is the last item wipe it.
		 * We leave it as the first though */
		gtk_widget_destroy (separator);
	}

	/* Owen: don't read the next line */
	GTK_MENU_SHELL (GTK_MENU_ITEM (w)->submenu)->active = 1;
	our_gtk_menu_position (GTK_MENU (GTK_MENU_ITEM (w)->submenu));
}

static void
destroy_task_menu (GtkWidget *w, gpointer data)
{
	FoobarWidget *foo = FOOBAR_WIDGET (data);
	/*g_message ("removing...");*/
	gtk_menu_item_remove_submenu (GTK_MENU_ITEM (w));
	g_hash_table_destroy (foo->tasks);
	foo->tasks = NULL;
	set_the_task_submenu (foo, w);
}

static GtkWidget *
get_default_pixmap (void)
{
	GtkWidget *widget;
	static GdkPixmap *pixmap = NULL;
	static GdkBitmap *mask   = NULL;
	static gboolean looked   = FALSE;

	if ( ! looked) {
		GdkPixbuf *pb = NULL, *scaled = NULL;
		pb = gdk_pixbuf_new_from_file (GNOME_ICONDIR"/gnome-tasklist.png");
		
		if (pb != NULL) {
			scaled = gdk_pixbuf_scale_simple (pb, 20, 20, 
							  GDK_INTERP_BILINEAR);
			gdk_pixbuf_unref (pb);
		}

		if (scaled != NULL) {
			gdk_pixbuf_render_pixmap_and_mask (scaled,
							   &pixmap, &mask, 128);
			gdk_pixbuf_unref (scaled);
			
			if (pixmap != NULL)
				gdk_pixmap_ref (pixmap);

			if (mask != NULL)
				gdk_bitmap_ref (mask);
		}

		looked = TRUE;
	}
	if (pixmap != NULL)
		widget = gtk_pixmap_new (pixmap, mask);
	else
		widget = gtk_label_new ("*");
	gtk_widget_show (widget);

	return widget;
}

static void
set_das_pixmap (FoobarWidget *foo, GwmhTask *task)
{
	if (!GTK_WIDGET_REALIZED (foo))
		return;

	foo->icon_task = NULL;

	if (foo->task_pixmap != NULL)
		gtk_widget_destroy (foo->task_pixmap);
	foo->task_pixmap = NULL;

	if (task != NULL) {
		foo->task_pixmap = get_task_icon (task, GTK_WIDGET (foo));
		foo->icon_task = task;
	}

	if (foo->task_pixmap == NULL) {
		foo->task_pixmap = get_default_pixmap ();
	}

	if (foo->task_pixmap != NULL) {
		gtk_container_add (GTK_CONTAINER (foo->task_bin),
				   foo->task_pixmap);
	}
}

static gboolean
task_notify (gpointer data,
	     GwmhTask *task,
	     GwmhTaskNotifyType ntype,
	     GwmhTaskInfoMask imask)
{
	FoobarWidget *foo = FOOBAR_WIDGET (data);
	GtkWidget *item;

	switch (ntype) {
	case GWMH_NOTIFY_INFO_CHANGED:
		if (imask & GWMH_TASK_INFO_WM_HINTS &&
		    GWMH_TASK_FOCUSED (task)) {
			/* icon might have changed */
			set_das_pixmap (foo, task);
		} else if (imask & GWMH_TASK_INFO_FOCUSED) {
			if (GWMH_TASK_FOCUSED (task) &&
			    foo->icon_task != task) {
				/* Focused and not set in the top thingie,
				 * so setup */
				set_das_pixmap (foo, task);
			} else if ( ! GWMH_TASK_FOCUSED (task) &&
				   task == foo->icon_task) {
				/* Just un-focused and currently the
				 * icon_task, so set the pixmap to
				 * the default (nothing) */
				set_das_pixmap (foo, NULL);
			}
		}
		break;
	case GWMH_NOTIFY_NEW:
		if (foo->tasks != NULL)
			add_task (task, foo);
		break;
	case GWMH_NOTIFY_DESTROY:
		if (task == foo->icon_task)
			set_das_pixmap (foo, NULL);
		/* FIXME: Whoa; leak? */
		if (foo->tasks != NULL) {
			item = g_hash_table_lookup (foo->tasks, task);
			if (item) {
				g_hash_table_remove (foo->tasks, task);
				gtk_widget_hide (item);
			} else {
				g_warning ("Could not find item for task '%s'",
					   sure_string (task->name));
			}
		}
		break;
	default:
		break;
	}
	return TRUE;
}

static void
append_task_menu (FoobarWidget *foo, GtkMenuBar *menu_bar)
{
	foo->task_item = gtk_menu_item_new ();

	foo->task_bin = gtk_alignment_new (0.3, 0.5, 0.0, 0.0);
	gtk_widget_set_usize (foo->task_bin, 25, 20);
	gtk_widget_show (foo->task_bin);
	gtk_container_add (GTK_CONTAINER (foo->task_item), foo->task_bin);

	gtk_menu_bar_append (menu_bar, foo->task_item);
}

static void
setup_task_menu (FoobarWidget *foo)
{
	GList *tasks;
	g_assert (foo->task_item != NULL);

	gtk_signal_connect (GTK_OBJECT (foo->task_item), "select",
			    GTK_SIGNAL_FUNC (create_task_menu), foo);
	gtk_signal_connect (GTK_OBJECT (foo->task_item), "deselect",
			    GTK_SIGNAL_FUNC (destroy_task_menu), foo);

	set_the_task_submenu (foo, foo->task_item);

	/* setup the pixmap to the focused task */
	tasks = gwmh_task_list_get ();
	while (tasks != NULL) {
		if (GWMH_TASK_FOCUSED (tasks->data)) {
			set_das_pixmap  (foo, tasks->data);
			break;
		}
		tasks = tasks->next;
	}

	/* if no focused task found, then just set it to default */
	if (tasks == NULL)
		set_das_pixmap  (foo, NULL);

	foo->notify = gwmh_task_notifier_add (task_notify, foo);
}

static void
foobar_widget_init (FoobarWidget *foo)
{
	/*gchar *path;*/
	GtkWindow *window = GTK_WINDOW (foo);
	/*GtkWidget *bufmap;*/
	GtkWidget *menu_bar, *bar;
	GtkWidget *menu, *menuitem;
	/*GtkWidget *align;*/
	gint flags;

	foo->screen = 0;

	foo->task_item = NULL;
	foo->task_menu = NULL;
	foo->task_pixmap = NULL;
	foo->task_bin = NULL;
	foo->icon_task = NULL;

	foo->clock_format = g_strdup (_("%H:%M"));
	foo->clock_timeout = 0;
	foo->clock_label = NULL;

	foo->compliant_wm = xstuff_is_compliant_wm ();
	if(foo->compliant_wm)
		GTK_WINDOW(foo)->type = GTK_WINDOW_TOPLEVEL;
	else
		GTK_WINDOW(foo)->type = GTK_WINDOW_POPUP;

	window->allow_shrink = TRUE;
	window->allow_grow   = TRUE;
	window->auto_shrink  = TRUE;

	gtk_signal_connect (GTK_OBJECT (foo), "delete_event",
			    GTK_SIGNAL_FUNC (gtk_true), NULL);

	gtk_widget_set_uposition (GTK_WIDGET (foo),
				  multiscreen_x (foo->screen),
				  multiscreen_y (foo->screen));
	gtk_widget_set_usize (GTK_WIDGET (foo),
			      multiscreen_width (foo->screen), -2);

	foo->ebox = gtk_event_box_new ();
	foo->hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add(GTK_CONTAINER(foo->ebox), foo->hbox);

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
	
	
	menuitem = pixmap_menu_item_new (_("Programs"),
					 "gnome-logo-icon-transparent.png");
	flags = (get_default_menu_flags() & 
		 ~(MAIN_MENU_SYSTEM_SUB | MAIN_MENU_USER | MAIN_MENU_USER_SUB |
		   MAIN_MENU_PANEL | MAIN_MENU_PANEL_SUB | MAIN_MENU_DESKTOP |
		   MAIN_MENU_DESKTOP_SUB)) | MAIN_MENU_SYSTEM;
	menu = create_root_menu (NULL, TRUE, flags, TRUE, FALSE, FALSE);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), menuitem);
	gtk_signal_connect (GTK_OBJECT (menu), "show",
			    GTK_SIGNAL_FUNC (programs_menu_to_display),
			    NULL);
	foo->programs = menu;

	foo->favorites =
		append_folder_menu(menu_bar, _("Favorites"), NULL,
				   FALSE, "apps");
	foo->settings =
		append_folder_menu(menu_bar, _("Settings"),  NULL, TRUE,
			           "gnome/apps/Settings");
	append_desktop_menu (menu_bar);

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

	bar = menu_bar = gtk_menu_bar_new ();
	gtk_menu_bar_set_shadow_type (GTK_MENU_BAR (menu_bar),
				      GTK_SHADOW_NONE);
	append_clock_menu (foo, menu_bar);
#if 0
	/* TODO: use the gnome menu if no gnome compliant WM or tasklist disabled */
	append_gnome_menu (foo, menu_bar);
#endif
	append_task_menu (foo, GTK_MENU_BAR (bar));


	gtk_box_pack_end (GTK_BOX (foo->hbox), menu_bar, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (foo), foo->ebox);
	gtk_widget_show_all (foo->ebox);
}

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
foobar_widget_destroy (GtkObject *o)
{
	FoobarWidget *foo = FOOBAR_WIDGET (o);

	foobars = g_list_remove (foobars, foo);

	if (foo->clock_timeout != 0)
		gtk_timeout_remove (foo->clock_timeout);
	foo->clock_timeout = 0;
	
	foo->clock_label = NULL;

	g_free (foo->clock_format);
	foo->clock_format = NULL;

	g_slist_foreach (panel_list, queue_panel_resize, NULL);

	if (foo->tasks != NULL)
		g_hash_table_destroy (foo->tasks);
	foo->tasks = NULL;
	if (foo->notify > 0)
		gwmh_task_notifier_remove (foo->notify);
	foo->notify = 0;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (o);
}

static void
foobar_widget_size_allocate (GtkWidget *w, GtkAllocation *alloc)
{
	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		GTK_WIDGET_CLASS (parent_class)->size_allocate (w, alloc);

	if (GTK_WIDGET_REALIZED (w)) {
		FoobarWidget *foo = FOOBAR_WIDGET (w);
		xstuff_set_pos_size (w->window,
				     multiscreen_x (foo->screen),
				     multiscreen_y (foo->screen),
				     alloc->width,
				     alloc->height);

		g_slist_foreach (panel_list, queue_panel_resize, NULL);
		basep_border_queue_recalc (foo->screen);
	}
}

GtkWidget *
foobar_widget_new (int screen)
{
	FoobarWidget *foo;

	g_return_val_if_fail (screen >= 0, NULL);

	if (foobar_widget_exists (screen))
		return NULL;

	foo = gtk_type_new (TYPE_FOOBAR_WIDGET);

	foo->screen = screen;
	gtk_widget_set_uposition (GTK_WIDGET (foo),
				  multiscreen_x (foo->screen),
				  multiscreen_y (foo->screen));
	gtk_widget_set_usize (GTK_WIDGET (foo),
			      multiscreen_width (foo->screen), -2);

	foobars = g_list_prepend (foobars, foo);

	return GTK_WIDGET (foo);
}

gboolean
foobar_widget_exists (int screen)
{
	GList *li;

	for (li = foobars; li != NULL; li = li->next) {
		FoobarWidget *foo = li->data;

		if (foo->screen == screen)
			return TRUE;
	}
	return FALSE;
}

void
foobar_widget_force_menu_remake (void)
{
	FoobarWidget *foo;
	GList *li;

	for (li = foobars; li != NULL; li = li->next) {
		foo = FOOBAR_WIDGET(li->data);

		if (foo->programs != NULL)
			gtk_object_set_data (GTK_OBJECT(foo->programs),
					     "need_reread", GINT_TO_POINTER(1));
		if (foo->settings != NULL)
			gtk_object_set_data (GTK_OBJECT(foo->settings),
					     "need_reread", GINT_TO_POINTER(1));
		if (foo->favorites != NULL)
			gtk_object_set_data (GTK_OBJECT(foo->favorites),
					     "need_reread", GINT_TO_POINTER(1));
	}
}

gint
foobar_widget_get_height (int screen)
{
	GList *li;

	g_return_val_if_fail (screen >= 0, 0);

	for (li = foobars; li != NULL; li = li->next) {
		FoobarWidget *foo = FOOBAR_WIDGET(li->data);

		if (foo->screen == screen)
			return GTK_WIDGET (foo)->allocation.height;
	}
	return 0; 
}

static void
reparent_button_widgets(GtkWidget *w, gpointer data)
{
	GdkWindow *newwin = data;
	if (IS_BUTTON_WIDGET (w)) {
		ButtonWidget *button = BUTTON_WIDGET(w);
		/* we can just reparent them all to 0,0 as the next thing
		 * that will happen is a queue_resize and on size allocate
		 * they will be put into their proper place */
		gdk_window_reparent (button->event_window, newwin, 0, 0);
	}
}

void
foobar_widget_redo_window(FoobarWidget *foo)
{
	GtkWindow *window;
	GtkWidget *widget;
	GdkWindowAttr attributes;
	gint attributes_mask;
	GdkWindow *oldwin;
	GdkWindow *newwin;
	gboolean comp;

	comp = xstuff_is_compliant_wm();
	if (comp == foo->compliant_wm)
		return;

	window = GTK_WINDOW(foo);
	widget = GTK_WIDGET(foo);

	foo->compliant_wm = comp;
	if (foo->compliant_wm) {
		window->type = GTK_WINDOW_TOPLEVEL;
		attributes.window_type = GDK_WINDOW_TOPLEVEL;
	} else {
		window->type = GTK_WINDOW_POPUP;
		attributes.window_type = GDK_WINDOW_TEMP;
	}

	if (widget->window == NULL)
		return;

	/* this is mostly copied from gtkwindow.c realize method */
	attributes.title = window->title;
	attributes.wmclass_name = window->wmclass_name;
	attributes.wmclass_class = window->wmclass_class;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= (GDK_EXPOSURE_MASK |
				  GDK_KEY_PRESS_MASK |
				  GDK_ENTER_NOTIFY_MASK |
				  GDK_LEAVE_NOTIFY_MASK |
				  GDK_FOCUS_CHANGE_MASK |
				  GDK_STRUCTURE_MASK);

	attributes_mask = GDK_WA_VISUAL | GDK_WA_COLORMAP;
	attributes_mask |= (window->title ? GDK_WA_TITLE : 0);
	attributes_mask |= (window->wmclass_name ? GDK_WA_WMCLASS : 0);
   
	oldwin = widget->window;

	newwin = gdk_window_new(NULL, &attributes, attributes_mask);
	gdk_window_set_user_data(newwin, window);

	xstuff_set_no_group_and_no_input (newwin);

	/* reparent our main panel window */
	gdk_window_reparent(foo->ebox->window, newwin, 0, 0);
	/* reparent all the base event windows as they are also children of
	 * the foobar */
	gtk_container_foreach(GTK_CONTAINER(foo->panel),
			      reparent_button_widgets,
			      newwin);


	widget->window = newwin;

	gdk_window_set_user_data(oldwin, NULL);
	gdk_window_destroy(oldwin);

	widget->style = gtk_style_attach(widget->style, widget->window);
	gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	gtk_widget_queue_resize(widget);

	foobar_widget_update_winhints (foo);

	gtk_drag_dest_set (widget, 0, NULL, 0, 0);

	gtk_widget_map(widget);
}
