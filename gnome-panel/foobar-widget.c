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

#define SMALL_ICON_SIZE 20

extern GlobalConfig global_config;
extern GList *panel_list;

extern GtkTooltips *panel_tooltips;

static void foobar_widget_class_init (FoobarWidgetClass *klass);
static void foobar_widget_init (FoobarWidget *foo);
static void foobar_widget_realize (GtkWidget *w);

static GtkWidget *das_global_foobar = NULL;
static GtkWidget *clock_ebox = NULL;

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

        parent_class = gtk_type_class (gtk_window_get_type ());

	widget_class->realize = foobar_widget_realize;
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
	gnome_execute_async (NULL, 1, v);
}

static void
gmc_client (GtkWidget *w, gpointer data)
{
	char *v[3] = { "gmc-client" };
	v[1] = data;
	if(gnome_execute_async (NULL, 2, v) < 0)
		panel_error_dialog(_("Cannot execute the gmc-client program,\n"
				     "perhaps gmc is not installed"));
}

static void
gnomecal_client (GtkWidget *w, gpointer data)
{
	char *v[4] = { "gnomecal", "--view" };
	v[2] = data;
	if(gnome_execute_async (NULL, 3, v) < 0)
		panel_error_dialog(_("Cannot execute the gnome calendar,\n"
				     "perhaps it's not installed.\n"
				     "It is in the gnome-pim package."));
}

static GtkWidget *
append_gnome_menu (GtkWidget *menu_bar)
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
		{ N_("Bug Tracking System (www)"), N_("http://bugs.gnome.org/"),                         "bug-buddy.png" },
		{ NULL }
	};
	
	
	menu = gtk_menu_new ();
	
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

	add_tearoff (GTK_MENU (menu));

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

	add_tearoff (GTK_MENU (menu));

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

	gtk_signal_connect (GTK_OBJECT (menu), "show",
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
	static int day = 0;
	GtkWidget *label = GTK_WIDGET (data);
	struct tm *das_tm;
	time_t das_time;
	char hour[256];

	if (!IS_FOOBAR_WIDGET (das_global_foobar))
		return FALSE;

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

		gtk_tooltips_set_tip (panel_tooltips, clock_ebox, hour, NULL);

		day = das_tm->tm_mday;
	}

	if(strftime(hour, sizeof(hour), FOOBAR_WIDGET (das_global_foobar)->clock_format, das_tm) == 0) {
		/* according to docs, if the string does not fit, the
		 * contents of tmp2 are undefined, thus just use
		 * ??? */
		strcpy(hour, "???");
	}
	hour[sizeof(hour)-1] = '\0'; /* just for sanity */

	gtk_label_set_text (GTK_LABEL (label), hour);

	return TRUE;
}

static void
timeout_remove (GtkWidget *w, gpointer data)
{
	gtk_timeout_remove (GPOINTER_TO_INT (data));
}

static void
set_fooclock_format (GtkWidget *w, char *format)
{
	if (!IS_FOOBAR_WIDGET (das_global_foobar))
		return;

	g_free (FOOBAR_WIDGET (das_global_foobar)->clock_format);
	FOOBAR_WIDGET (das_global_foobar)->clock_format = g_strdup (_(format));
}

static void
append_format_item (GtkWidget *menu, const char *format)
{
	char hour[20];
	GtkWidget *item;
	struct tm *das_tm;
	time_t das_time = 0;

	das_tm = localtime (&das_time);
	if (strftime (hour, 20, _(format), das_tm) == 20)
		hour[19] = '\0';

	item = gtk_menu_item_new_with_label (hour);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (set_fooclock_format),
			    (gpointer)format);
}

static GtkWidget *
append_clock_menu (GtkWidget *menu_bar)
{
	GtkWidget *item, *menu, *label, *menu2;
	gint timeout;
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
		N_("%I:%M %p"),
		N_("%I:%M:%S %p"),
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

	add_menu_separator (menu);

	menu2 = gtk_menu_new ();
	for (i=0; formats[i]; i++)
		append_format_item (menu2, formats[i]);

	add_tearoff (GTK_MENU (menu2));

	item = gtk_menu_item_new_with_label (_("Format"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu2);
	gtk_menu_append (GTK_MENU (menu), item);

	add_tearoff (GTK_MENU (menu));

	item = gtk_menu_item_new ();
	label = gtk_label_new ("");
	timeout = gtk_timeout_add (1000, timeout_cb, label);
	gtk_signal_connect (GTK_OBJECT (label), "destroy",
			    GTK_SIGNAL_FUNC (timeout_remove),
			    GINT_TO_POINTER (timeout));
	clock_ebox = item;
	gtk_container_add (GTK_CONTAINER (item), label);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), item);

	return item;
}

void
foobar_widget_update_winhints (FoobarWidget *foo)
{
	GtkWidget *w = GTK_WIDGET (foo);

	if (!foo->compliant_wm)
		return;

	gdk_window_set_hints (w->window, 0, 0, 
			      0, 0, 0, 0, GDK_HINT_POS);

	gnome_win_hints_set_expanded_size (w, 0, 0, 0, 0);
	gdk_window_set_decorations (w->window, 0);
	gnome_win_hints_set_state (w, WIN_STATE_STICKY |
				   WIN_STATE_FIXED_POSITION);
	
	gnome_win_hints_set_hints (w, GNOME_PANEL_HINTS |
				   WIN_HINTS_DO_NOT_COVER);	
	gnome_win_hints_set_layer (w, global_config.keep_bottom
				   ? WIN_LAYER_BELOW
				   : WIN_LAYER_DOCK);
}

static void
foobar_widget_realize (GtkWidget *w)
{
	if(GTK_WIDGET_CLASS(parent_class)->realize)
		GTK_WIDGET_CLASS(parent_class)->realize(w);

	foobar_widget_update_winhints(FOOBAR_WIDGET(w));
	xstuff_set_no_group(w->window);
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
	if(foo->compliant_wm)
		GTK_WINDOW(foo)->type = GTK_WINDOW_TOPLEVEL;
	else
		GTK_WINDOW(foo)->type = GTK_WINDOW_POPUP;

	window->allow_shrink = TRUE;
	window->allow_grow   = TRUE;
	window->auto_shrink  = TRUE;

	gtk_signal_connect (GTK_OBJECT (foo), "delete_event",
			    GTK_SIGNAL_FUNC (gtk_true), NULL);

	gtk_widget_set_usize (GTK_WIDGET (foo),
			      gdk_screen_width (), -2);

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
	
	
	menuitem = pixmap_menu_item_new (_("Programs"), GNOME_STOCK_MENU_ABOUT);
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

	append_folder_menu  (menu_bar, _("Favorites"), NULL, FALSE, "apps/.");
	append_folder_menu  (menu_bar, _("Settings"),  NULL, TRUE,
			     "gnome/apps/Settings/.");
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

	menu_bar = gtk_menu_bar_new ();
	gtk_menu_bar_set_shadow_type (GTK_MENU_BAR (menu_bar),
				      GTK_SHADOW_NONE);
	append_clock_menu (menu_bar);
	append_gnome_menu (menu_bar);


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
queue_panel_resizes (GtkWidget *w, gpointer data)
{
	if (!GTK_WIDGET_REALIZED (w))
		return;
	g_list_foreach (panel_list, queue_panel_resize, data);
}

static void
foobar_destroyed(GtkWidget *w, gpointer data)
{
	das_global_foobar = NULL;
	g_list_foreach (panel_list, queue_panel_resize, data);
}

GtkWidget *
foobar_widget_new (void)
{
	g_return_val_if_fail (das_global_foobar == NULL, NULL);
	das_global_foobar =  gtk_type_new (FOOBAR_WIDGET_TYPE);
	gtk_signal_connect_after (GTK_OBJECT (das_global_foobar),
				  "size-allocate",
				  GTK_SIGNAL_FUNC (queue_panel_resizes), NULL);
	gtk_signal_connect (GTK_OBJECT (das_global_foobar), "destroy",
			    GTK_SIGNAL_FUNC (foobar_destroyed),
			    NULL);
	return das_global_foobar;
}

gboolean
foobar_widget_exists (void)
{
	return (das_global_foobar != NULL);
}

gint
foobar_widget_get_height (void)
{
	return (das_global_foobar && GTK_WIDGET_REALIZED (das_global_foobar)) 
		? das_global_foobar->allocation.height : 0; 
}

static void
reparent_button_widgets(GtkWidget *w, gpointer data)
{
	GdkWindow *newwin = data;
	if(IS_BUTTON_WIDGET(w)) {
		ButtonWidget *button = BUTTON_WIDGET(w);
		/* we can just reparent them all to 0,0 as the next thing
		 * that will happen is a queue_resize and on size allocate
		 * they will be put into their proper place */
		gdk_window_reparent(button->event_window, newwin, 0, 0);
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
	if(comp == foo->compliant_wm)
		return;

	window = GTK_WINDOW(foo);
	widget = GTK_WIDGET(foo);

	foo->compliant_wm = comp;
	if(foo->compliant_wm) {
		window->type = GTK_WINDOW_TOPLEVEL;
		attributes.window_type = GDK_WINDOW_TOPLEVEL;
	} else {
		window->type = GTK_WINDOW_POPUP;
		attributes.window_type = GDK_WINDOW_TEMP;
	}

	if(!widget->window)
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

	xstuff_set_no_group(newwin);

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
