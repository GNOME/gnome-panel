/* GNOME panel: foobar widget
 * Copyright 1999,2000 Helix Code, Inc.
 * Copyright 2000 Eazel, Inc.
 *
 * Author: Jacob Berkman
 *
 */

/* since BASEP_IS_WIDGET() is used throughout, it makes life easier if we
 * have a GtkWindow of our own.
 */

#include <config.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <libgnome/libgnome.h>
/* Yes, yes I know, now bugger off ... */
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>
#include <gconf/gconf-client.h>

#include "foobar-widget.h"

#include "menu.h"
#include "menu-util.h"
#include "session.h"
#include "panel-widget.h"
#include "xstuff.h"
#include "basep-widget.h"
#include "panel-config-global.h"
#include "panel-util.h"
#include "drawer-widget.h"
#include "gnome-run.h"
#include "multiscreen-stuff.h"
#include "panel-gconf.h"

#define ICON_SIZE 20

extern GlobalConfig global_config;
extern GSList *panel_list;

extern GtkTooltips *panel_tooltips;

static void foobar_widget_class_init	(FoobarWidgetClass *klass);
static void foobar_widget_instance_init (FoobarWidget *foo);
static void foobar_widget_realize	(GtkWidget *w);
static void foobar_widget_destroy	(GtkObject *o);
static void foobar_widget_size_allocate	(GtkWidget *w,
					 GtkAllocation *alloc);
static gboolean foobar_leave_notify	(GtkWidget *widget,
					 GdkEventCrossing *event);
static gboolean foobar_enter_notify	(GtkWidget *widget,
					 GdkEventCrossing *event);
static void append_task_menu (FoobarWidget *foo, GtkMenuShell *menu_bar);
static void setup_task_menu (FoobarWidget *foo);

static GList *foobars = NULL;

static GtkWindowClass *foobar_widget_parent_class = NULL;

GType
foobar_widget_get_type (void)
{
	static GType object_type = 0;

	if (object_type == 0) {
		static const GTypeInfo object_info = {
                    sizeof (FoobarWidgetClass),
                    (GBaseInitFunc)         NULL,
                    (GBaseFinalizeFunc)     NULL,
                    (GClassInitFunc)        foobar_widget_class_init,
                    NULL,                   /* class_finalize */
                    NULL,                   /* class_data */
                    sizeof (FoobarWidget),
                    0,                      /* n_preallocs */
                    (GInstanceInitFunc)     foobar_widget_instance_init
		};

		object_type = g_type_register_static (GTK_TYPE_WINDOW, "FoobarWidget", &object_info, 0);
        	foobar_widget_parent_class = g_type_class_ref (GTK_TYPE_WINDOW);

	}

	return object_type;
}

static void
foobar_widget_class_init (FoobarWidgetClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = foobar_widget_destroy;

	widget_class->realize = foobar_widget_realize;
	widget_class->size_allocate = foobar_widget_size_allocate;
	widget_class->enter_notify_event = foobar_enter_notify;
	widget_class->leave_notify_event = foobar_leave_notify;

	gtk_rc_parse_string ("style \"panel-foobar-menubar-style\"\n"
			     "{\n"
			     "GtkMenuBar::shadow-type = none\n"
			     "GtkMenuBar::internal-padding = 0\n"
			     "}\n"
			     "widget \"*.panel-foobar-menubar\" style \"panel-foobar-menubar-style\"");

}

static GtkWidget *
pixmap_menu_item_new (const char *text, const char *try_file)
{
	GtkWidget *item;
	GtkWidget *label;

	item = gtk_image_menu_item_new ();

	/* FIXME: listen to this gconf client key */
	if (try_file != NULL && panel_menu_have_icons ()) {
		GtkWidget *image;
		GdkPixbuf *pixbuf;
		char *file;

		if (g_path_is_absolute (try_file))
			file = g_strdup (try_file);
		else
			file = panel_pixmap_discovery (try_file,
						       TRUE /* fallback */);

		pixbuf = gdk_pixbuf_new_from_file (file, NULL);
		if (pixbuf != NULL &&
		    (gdk_pixbuf_get_width (pixbuf) != ICON_SIZE ||
		     gdk_pixbuf_get_height (pixbuf) != ICON_SIZE)) {
			GdkPixbuf *scaled;

			scaled = gdk_pixbuf_scale_simple (pixbuf,
							  ICON_SIZE,
							  ICON_SIZE,
							  GDK_INTERP_BILINEAR);

			gdk_pixbuf_unref (pixbuf);

			pixbuf = scaled;
		}

		if (pixbuf != NULL) {
			image = gtk_image_new_from_pixbuf (pixbuf);
			gtk_widget_show (image);

			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
						       image);

			gdk_pixbuf_unref (pixbuf);
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
add_tearoff (GtkMenuShell *menu)
{
	GtkWidget *item;

	if (!panel_menu_have_tearoff ())
		return;
	
	item = gtk_tearoff_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_shell_prepend (menu, item);
}

static gboolean
foobar_leave_notify (GtkWidget *widget,
		     GdkEventCrossing *event)
{
	if (GTK_WIDGET_CLASS (foobar_widget_parent_class)->leave_notify_event)
		GTK_WIDGET_CLASS (foobar_widget_parent_class)->leave_notify_event (widget,
								     event);

	return FALSE;
}

static gboolean
foobar_enter_notify (GtkWidget *widget,
		     GdkEventCrossing *event)
{
	if (GTK_WIDGET_CLASS (foobar_widget_parent_class)->enter_notify_event)
		GTK_WIDGET_CLASS (foobar_widget_parent_class)->enter_notify_event (widget,
								     event);

	if (global_config.autoraise)
		gdk_window_raise (widget->window);

	return FALSE;
}

static void
gnomecal_client (GtkWidget *w, gpointer data)
{
	char *v[4] = { "gnomecal", "--view", NULL, NULL };
	v[2] = data;
	if(gnome_execute_async (g_get_home_dir (), 3, v) < 0)
		panel_error_dialog("cannot_execute_gnome_calendar",
				   _("Cannot execute the gnome calendar,\n"
				     "perhaps it's not installed.\n"
				     "It is in the gnome-pim package."));
}

static void
append_actions_menu (GtkWidget *menu_bar)
{
	GtkWidget *menu, *item;

	menu = gtk_menu_new ();


	add_tearoff (GTK_MENU_SHELL (menu));

	menu = gtk_menu_new ();

	item = pixmap_menu_item_new (_("Run..."), "gnome-run.png");
	gtk_tooltips_set_tip (panel_tooltips, item,
			      _("Run applications, if you know the "
				"correct command to type in"),
			      NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (show_run_dialog), 0);
	setup_internal_applet_drag (item, "RUN:NEW");

	/* FIXME: search */
	/* FIXME: shutdown or reboot */

	if (panel_is_program_in_path  ("xscreensaver")) {
		item = pixmap_menu_item_new (_("Lock Display"), 
					       "gnome-lockscreen.png");
		gtk_tooltips_set_tip (panel_tooltips, item,
				      _("Protect your computer from "
					"unauthorized use"),
				      NULL);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (panel_lock), 0);
		setup_internal_applet_drag(item, "LOCK:NEW");
	}

	item = pixmap_menu_item_new (_("Log Out"), "gnome-term-night.png");
	gtk_tooltips_set_tip (panel_tooltips, item,
			      _("Quit from the GNOME desktop"),
			      NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (panel_quit), 0);
	setup_internal_applet_drag (item, "LOGOUT:NEW");

	add_tearoff (GTK_MENU_SHELL (menu));

	item = gtk_menu_item_new_with_label (_("Actions"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), item);

	panel_stretch_events_to_toplevel (item,
					 TRUE /* top */,
					 FALSE /* right */,
					 FALSE /* bottom */,
					 FALSE /* left */);
}

#if 0
/* sure we won't need this????
 * -George */
static GtkWidget *
append_folder_menu (GtkWidget *menu_bar, const char *label,
		    const char *pixmap, const char *path,
		    gboolean stretch_left,
		    gboolean stretch_top,
		    gboolean stretch_right)
{
	GtkWidget *item, *menu;

	menu = create_fake_menu_at (path,
				    FALSE /* applets */,
				    FALSE /* launcher_add */,
				    label /* dir_name */,
				    NULL /* pixmap_name */,
				    FALSE /* title */);

	if (menu == NULL) {
		g_warning (_("menu wasn't created"));
		return NULL;
	}

	if (pixmap != NULL)
		item = pixmap_menu_item_new (label, pixmap);
	else
		item = gtk_menu_item_new_with_label (label);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), item);

	g_signal_connect (G_OBJECT (menu), "show",
			  G_CALLBACK (submenu_to_display),
			  NULL);

	if (stretch_left || stretch_top || stretch_right)
		panel_stretch_events_to_toplevel (item,
						 stretch_top,
						 stretch_right,
						 FALSE,
						 stretch_left);
		

	return menu;
}
#endif

static void
append_gnomecal_items (GtkWidget *menu)
{
	GtkWidget *item;
	int i;
	
	const char *cals[] = { 
		N_("Today"),      N_("View the calendar for today."),      "gnome-day.png",   "dayview",
		N_("This Week"),  N_("View the calendar for this week."),  "gnome-week.png",  "weekview",
		N_("This Month"), N_("View the calendar for this month."), "gnome-month.png", "monthview",
		NULL
	};
	
	for (i=0; cals[i]; i+=4) {
		item = pixmap_menu_item_new (cals[i], cals[i+2]);
		gtk_tooltips_set_tip (panel_tooltips, item,
			      	      cals[i+1],
			      	      NULL);
	
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (gnomecal_client),
				  (char *)cals[i+3]);
	}
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
append_format_items (GtkWidget *menu)
{
	char hour[256];
	GtkWidget *item;
	GSList *group = NULL;
	struct tm *das_tm;
	time_t das_time = 0;
	char *key;
	char *s;
	int i;
	const char *formats[] = {
		N_("%H:%M"),
		N_("%H:%M:%S"),
		N_("%l:%M %p"),
		N_("%l:%M:%S %p"),
		NULL
	};

	key = panel_gconf_global_config_get_full_key ("clock-format");
	s = panel_gconf_get_string (key, _("%I:%M:%S %p"));
	g_free (key);
	
	for (i = 0; formats[i]; i++)
	{
		das_tm = localtime (&das_time);
		if (strftime (hour, sizeof(hour), _(formats[i]), das_tm) == 0) {
 			/* according to docs, if the string does not fit, the
 		 	 * contents of tmp2 are undefined, thus just use
 		 	 * ??? */
			strcpy(hour, "???");
		}
		hour[sizeof(hour)-1] = '\0'; /* just for sanity */

		item = gtk_radio_menu_item_new_with_label (group, hour);
		group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (item));
	
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);	
		g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (set_fooclock_format),
			  (char *)formats[i]);

		if (s && !strcmp (s, formats[i])) {
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
		}
	}
	g_free (s);
}

static void
set_time_cb (GtkWidget *menu_item, char *path)
{
	char *v[2] = { path };
	
	if (gnome_execute_async (g_get_home_dir (), 1, v) < 0)
		panel_error_dialog ("could_not_call_time_admin",
				    _("Could not call time-admin\n"
				      "Perhaps time-admin is not installed"));
}

static GtkWidget *
append_clock_menu (FoobarWidget *foo, GtkWidget *menu_bar)
{
	GtkWidget *item, *menu, *menu2;
	gchar *time_admin_path;

	menu = gtk_menu_new ();
	append_gnomecal_items (menu);

#if 0 /* put back when evolution can do this */
	item = gtk_image_menu_item_new_with_label (_("Add Appointment..."));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
#endif

	add_menu_separator (menu);

	/* check for time-admin (part of ximian-setup-tools) */
	time_admin_path = g_find_program_in_path  ("time-admin");
	if (time_admin_path != NULL) {
		item = pixmap_menu_item_new (_("Set Time..."), "gnome-set-time.png");
		gtk_tooltips_set_tip (panel_tooltips, item,
			      	      _("Adjust the date and time."),
			      	      NULL);	
		
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (set_time_cb),
				  time_admin_path);			
	}

	menu2 = gtk_menu_new ();
	append_format_items (menu2); 

	add_tearoff (GTK_MENU_SHELL (menu2));

	item = gtk_image_menu_item_new_with_label (_("Format"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu2);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	add_tearoff (GTK_MENU_SHELL (menu));

	item = gtk_menu_item_new ();

	foo->clock_label = gtk_label_new ("");
	foo->clock_timeout = gtk_timeout_add (1000, timeout_cb, foo);

	foo->clock_ebox = item;
	gtk_container_add (GTK_CONTAINER (item), foo->clock_label);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), item);

	panel_stretch_events_to_toplevel (item,
					 TRUE /* top */,
					 FALSE /* right */,
					 FALSE /* bottom */,
					 FALSE /* left */);

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

	if(foo->clock_label) {
		time_t das_time;
		struct tm *das_tm;
		char hour[256];
		gchar *str_utf8;
		int width;
		PangoLayout *layout;

		das_time = 0;
		das_tm = localtime (&das_time);
		strftime(hour, sizeof(hour), _(clock_format), das_tm);
		str_utf8 = g_locale_to_utf8((const gchar *)hour, strlen(hour), NULL, NULL, NULL);
		if(str_utf8) {
			layout = gtk_widget_create_pango_layout (foo->clock_label, str_utf8);
			pango_layout_get_pixel_size (layout, &width, NULL);
			width += 8; /* Padding */
			gtk_widget_set_size_request (foo->clock_label, width, -1);
			g_object_unref (G_OBJECT(layout));
		}
	}
	update_clock (foo);
}

void
foobar_widget_update_winhints (FoobarWidget *foo)
{
	GtkWidget *w = GTK_WIDGET (foo);

	gtk_window_set_decorated (GTK_WINDOW (w), FALSE);
	gtk_window_stick (GTK_WINDOW (w));

	xstuff_set_wmspec_dock_hints (w->window, FALSE /* autohide */);
}

static void
foobar_widget_realize (GtkWidget *w)
{
	gtk_window_set_wmclass (GTK_WINDOW (w),
				"panel_window", "Panel");

	if (GTK_WIDGET_CLASS (foobar_widget_parent_class)->realize)
		GTK_WIDGET_CLASS (foobar_widget_parent_class)->realize (w);

	foobar_widget_update_winhints (FOOBAR_WIDGET (w));
	xstuff_set_no_group_and_no_input (w->window);

	setup_task_menu (FOOBAR_WIDGET (w));

	xstuff_set_wmspec_strut (w->window,
				 0 /* left */,
				 0 /* right */,
				 w->allocation.height /* top */,
				 0 /* bottom */);
}

static void
programs_menu_to_display (GtkWidget *menu)
{
	if (menu_need_reread (menu)) {
		int flags;

		while (GTK_MENU_SHELL (menu)->children)
			gtk_widget_destroy (GTK_MENU_SHELL (menu)->children->data);
		flags = MAIN_MENU_SYSTEM;
		if (got_kde_menus ())
			flags |= MAIN_MENU_KDE_SUB;
		if (got_distro_menus ())
			flags |= MAIN_MENU_DISTRIBUTION_SUB;
		create_root_menu (menu, TRUE, flags, TRUE, FALSE, FALSE /* run_item */);
	}
}

static void
set_the_task_submenu (FoobarWidget *foo, GtkWidget *item)
{
	foo->task_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), foo->task_menu);
	/*g_message ("setting...");*/
	g_signal_connect (G_OBJECT (foo->task_menu), "show",
			  G_CALLBACK (our_gtk_menu_position),
			  NULL);
}

static void
focus_window (GtkWidget *w, WnckWindow *window)
{
	WnckScreen *screen = wnck_screen_get (0 /* FIXME screen number */);
	WnckWorkspace *wspace = wnck_screen_get_active_workspace (screen);

	if (wspace != NULL)
		wnck_window_move_to_workspace (window, wspace);
	if (wnck_window_is_minimized (window)) 
		wnck_window_unminimize (window);
	wnck_window_activate (window);
}

/* No need to unref, in fact do NOT unref the return */
static GdkPixbuf *
get_default_image (void)
{
	static GdkPixbuf *pixbuf = NULL;
	static gboolean looked   = FALSE;

	if (! looked) {
		GdkPixbuf *pb = NULL, *scaled = NULL;
		char *name = panel_pixmap_discovery ("gnome-tasklist.png", FALSE /* fallback */);
		if (name == NULL)
			/* evil fallback huh? */
			name = panel_pixmap_discovery ("apple-red.png", FALSE /* fallback */);

		if (name != NULL) {
			pb = gdk_pixbuf_new_from_file (name, NULL);
			g_free (name);
		}
		
		if (pb != NULL) {
			scaled = gdk_pixbuf_scale_simple (pb, 20, 20, 
							  GDK_INTERP_BILINEAR);
			gdk_pixbuf_unref (pb);

			pixbuf = scaled;
		}

		looked = TRUE;
	}

	return pixbuf;
}

static void
add_window (WnckWindow *window, FoobarWidget *foo)
{
	GtkWidget *item, *label;
	char *title = NULL;
	int slen;
	GtkWidget *image = NULL;
	GdkPixbuf *pb;
	const char *name;
	WnckScreen *screen = wnck_screen_get (0 /* FIXME screen number */);
	WnckWorkspace *wspace = wnck_screen_get_active_workspace (screen);

	g_assert (foo->windows != NULL);

	if (wnck_window_is_skip_tasklist (window))
		return;

	name = wnck_window_get_name (window);

	if (name != NULL) {
		slen = strlen (name);
		if (slen > 443)
			title = g_strdup_printf ("%.420s...%s", name, name + slen - 20);
		else
			title = g_strdup (name);
	} else {
		/* Translators: Task with no name, should not really happen, so
		 * this should signal that the panel is confused by this task
		 * (thus question marks) */
		title = g_strdup (_("???"));
	}

	if (wnck_window_is_minimized (window)) {
		char *tmp = title;
		title = g_strdup_printf ("[%s]", title);
		g_free (tmp);
	}
	
	item = gtk_image_menu_item_new ();
	pb = wnck_window_get_mini_icon (window);
	if (pb == NULL)
		pb = get_default_image ();
	if (pb != NULL) {
		double pix_x, pix_y;
		pix_x = gdk_pixbuf_get_width (pb);
		pix_y = gdk_pixbuf_get_height (pb);
		if (pix_x > ICON_SIZE || pix_y > ICON_SIZE) {
			double greatest;
			GdkPixbuf *scaled;

			greatest = pix_x > pix_y ? pix_x : pix_y;
			scaled = gdk_pixbuf_scale_simple (pb,
							  (ICON_SIZE / greatest) * pix_x,
							  (ICON_SIZE / greatest) * pix_y,
							  GDK_INTERP_BILINEAR);
			image = gtk_image_new_from_pixbuf (scaled);
			gdk_pixbuf_unref (scaled);
		} else {
			image = gtk_image_new_from_pixbuf (pb);
		}
		gtk_widget_show (image);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
					       GTK_WIDGET (image));
	}

	label = gtk_label_new (title);
	g_free (title);

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_container_add (GTK_CONTAINER (item), label);
	g_hash_table_insert (foo->windows, window, item);
	g_signal_connect (G_OBJECT (item), "activate", 
			  G_CALLBACK (focus_window),
			  window);
	gtk_widget_show_all (item);
	
	if (wspace == wnck_window_get_workspace (window)) {
		gtk_menu_shell_prepend (GTK_MENU_SHELL (foo->task_menu), item);
	} else {
		gtk_menu_shell_append (GTK_MENU_SHELL (foo->task_menu), item);
	}
}

static void
create_task_menu (GtkWidget *w, gpointer data)
{
	FoobarWidget *foo = FOOBAR_WIDGET (data);
	GList *list;
	GtkWidget *separator;
	WnckScreen *screen = wnck_screen_get (0 /* FIXME screen number */);
	GList *windows = wnck_screen_get_windows (screen);

	/* g_message ("creating..."); */
	foo->windows = g_hash_table_new (g_direct_hash, g_direct_equal);

	separator = add_menu_separator (foo->task_menu);

	g_list_foreach (windows, (GFunc)add_window, foo);

	list = g_list_last (GTK_MENU_SHELL (foo->task_menu)->children);

	if (list != NULL &&
	    separator == list->data) {
		/* if the separator is the last item wipe it.
		 * We leave it as the first though */
		gtk_widget_destroy (separator);
	}

	list = g_list_last (GTK_MENU_SHELL (foo->task_menu)->children);
	if (list == NULL) {
		GtkWidget *item;
		item = gtk_image_menu_item_new_with_label (_("No windows open"));
		gtk_widget_set_sensitive (item, FALSE);
		gtk_widget_show_all (item);
	
		gtk_menu_shell_append (GTK_MENU_SHELL (foo->task_menu), item);
	}

	if (GTK_WIDGET_VISIBLE (foo->task_menu))
		our_gtk_menu_position (GTK_MENU (foo->task_menu));

	/* Owen: don't read the next line */
#if 0
	GTK_MENU_SHELL (foo->task_menu)->active = 1;
	our_gtk_menu_position (GTK_MENU (foo->task_menu));
#endif
}

static void
destroy_task_menu (GtkWidget *w, gpointer data)
{
	FoobarWidget *foo = FOOBAR_WIDGET (data);
	/*g_message ("removing...");*/
	gtk_menu_item_remove_submenu (GTK_MENU_ITEM (w));
	g_hash_table_destroy (foo->windows);
	foo->windows = NULL;
	set_the_task_submenu (foo, w);
}

static void
set_das_pixmap (FoobarWidget *foo, WnckWindow *window)
{
	GdkPixbuf *pb;
	if ( ! GTK_WIDGET_REALIZED (foo))
		return;

	if (foo->task_image != NULL)
		gtk_widget_destroy (GTK_WIDGET (foo->task_image));
	foo->task_image = NULL;

	foo->icon_window = window;

	pb = NULL;
	if (window != NULL)
		pb = wnck_window_get_mini_icon (window);
	if (pb == NULL)
		pb = get_default_image ();

	if (pb != NULL) {
		double pix_x, pix_y;
		pix_x = gdk_pixbuf_get_width (pb);
		pix_y = gdk_pixbuf_get_height (pb);
		if (pix_x > ICON_SIZE || pix_y > ICON_SIZE) {
			double greatest;
			GdkPixbuf *scaled;

			greatest = pix_x > pix_y ? pix_x : pix_y;
			scaled = gdk_pixbuf_scale_simple (pb,
							  (ICON_SIZE / greatest) * pix_x,
							  (ICON_SIZE / greatest) * pix_y,
							  GDK_INTERP_BILINEAR);
			foo->task_image = gtk_image_new_from_pixbuf (scaled);
			gdk_pixbuf_unref (scaled);
		} else {
			foo->task_image = gtk_image_new_from_pixbuf (pb);
		}
		gtk_widget_show (foo->task_image);

		gtk_container_add (GTK_CONTAINER (foo->task_bin),
				   GTK_WIDGET (foo->task_image));
	}
}

static void
append_task_menu (FoobarWidget *foo, GtkMenuShell *menu_bar)
{
	foo->task_item = gtk_menu_item_new ();
	gtk_widget_show (foo->task_item);

	foo->task_bin = gtk_alignment_new (0.3, 0.5, 0.0, 0.0);
	gtk_widget_set_size_request (foo->task_bin, 25, 20);
	gtk_widget_show (foo->task_bin);
	gtk_container_add (GTK_CONTAINER (foo->task_item), foo->task_bin);

	gtk_menu_shell_append (menu_bar, foo->task_item);

	panel_stretch_events_to_toplevel (foo->task_item,
					 TRUE /* top */,
					 TRUE /* right */,
					 FALSE /* bottom */,
					 FALSE /* left */);
}

static void
icon_changed (WnckWindow *window, FoobarWidget *foo)
{
	set_das_pixmap (foo, window);
}

static void
bind_window_changes (WnckWindow *window, FoobarWidget *foo)
{
	panel_signal_connect_while_alive (G_OBJECT (window), "icon_changed",
					  G_CALLBACK (icon_changed),
					  foo,
					  G_OBJECT (foo));
	/* XXX: do we care about names changing? */
}

/* focused window changed */
static void
active_window_changed (WnckScreen *screen,
		       FoobarWidget *foo)
{
	WnckWindow *window = wnck_screen_get_active_window (screen);

	/* icon might have changed */
	if (foo->icon_window != window)
		set_das_pixmap (foo, window);
}

/* window added */
static void
window_opened (WnckScreen *screen,
	       WnckWindow *window,
	       FoobarWidget *foo)
{
	if (foo->windows != NULL)
		add_window (window, foo);
	bind_window_changes (window, foo);
}
/* window removed */
static void
window_closed (WnckScreen *screen,
	       WnckWindow *window,
	       FoobarWidget *foo)
{
	if (window == foo->icon_window)
		set_das_pixmap (foo, NULL);
	/* FIXME: Whoa; leak? */
	if (foo->windows != NULL) {
		GtkWidget *item;
		item = g_hash_table_lookup (foo->windows, window);
		if (item != NULL) {
			g_hash_table_remove (foo->windows, window);
			gtk_widget_hide (item);
			gtk_menu_reposition (GTK_MENU (item->parent));
		} else {
			g_warning ("Could not find item for task '%s'",
				   sure_string (wnck_window_get_name (window)));
		}
	}
}

static void
setup_task_menu (FoobarWidget *foo)
{
	GList *windows;
	WnckScreen *screen;
	g_assert (foo->task_item != NULL);

	g_signal_connect (G_OBJECT (foo->task_item), "select",
			  G_CALLBACK (create_task_menu), foo);
	g_signal_connect (G_OBJECT (foo->task_item), "deselect",
			  G_CALLBACK (destroy_task_menu), foo);

	set_the_task_submenu (foo, foo->task_item);

	screen = wnck_screen_get (0 /* FIXME screen number */);

	/* setup the pixmap to the focused task */
	windows = wnck_screen_get_windows (screen);
	while (windows != NULL) {
		if (wnck_window_is_active (windows->data)) {
			set_das_pixmap  (foo, windows->data);
			break;
		}
		windows = windows->next;
	}

	/* if no focused task found, then just set it to default */
	if (windows == NULL)
		set_das_pixmap  (foo, NULL);

	g_list_foreach (windows, (GFunc)bind_window_changes, foo);

	panel_signal_connect_while_alive (G_OBJECT (screen),
					  "active_window_changed",
					  G_CALLBACK (active_window_changed),
					  foo,
					  G_OBJECT (foo));
	panel_signal_connect_while_alive (G_OBJECT (screen),
					  "window_opened",
					  G_CALLBACK (window_opened),
					  foo,
					  G_OBJECT (foo));
	panel_signal_connect_while_alive (G_OBJECT (screen),
					  "window_closed",
					  G_CALLBACK (window_closed),
					  foo,
					  G_OBJECT (foo));

}

static void
foobar_widget_instance_init (FoobarWidget *foo)
{
	/*gchar *path;*/
	GtkWindow *window = GTK_WINDOW (foo);
	/*GtkWidget *bufmap;*/
	GtkWidget *menu_bar, *bar;
	GtkWidget *menu, *menuitem;
	/*GtkWidget *align;*/
	gint flags;

	foo->screen = 0;

	foo->windows    = NULL;
	foo->task_item  = NULL;
	foo->task_menu  = NULL;
	foo->task_image = NULL;
	foo->task_bin   = NULL;
	foo->icon_window = NULL;

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

	g_signal_connect (G_OBJECT (foo), "delete_event",
			  G_CALLBACK (gtk_true), NULL);

	gtk_window_move (GTK_WINDOW (foo),
			 multiscreen_x (foo->screen),
			 multiscreen_y (foo->screen));
	g_object_set (G_OBJECT (foo),
		      "width_request", (int)multiscreen_width (foo->screen),
		      NULL);

	foo->ebox = gtk_event_box_new ();
	foo->hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add(GTK_CONTAINER(foo->ebox), foo->hbox);

#if 0	
	path = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
					  "panel/corner1.png", TRUE, NULL);
	bufmap = gnome_pixmap_new_from_file (path);
	g_free (path);
	align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
	gtk_container_add (GTK_CONTAINER (align), bufmap);
	gtk_box_pack_start (GTK_BOX (foo->hbox), align, FALSE, FALSE, 0);
#endif

	menu_bar = gtk_menu_bar_new ();	
	gtk_widget_set_name (menu_bar,
			     "panel-foobar-menubar");
	
	menuitem = pixmap_menu_item_new (_("Applications"),
					 "gnome-logo-icon-transparent.png");
	flags = MAIN_MENU_SYSTEM;
	if (got_kde_menus ())
		flags |= MAIN_MENU_KDE_SUB;
	if (got_distro_menus ())
		flags |= MAIN_MENU_DISTRIBUTION_SUB;

	menu = create_root_menu (NULL, TRUE, flags, TRUE, FALSE, FALSE /* run_item */);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), menuitem);
	g_signal_connect (G_OBJECT (menu), "show",
			  G_CALLBACK (programs_menu_to_display),
			  NULL);
	foo->programs = menu;

	/* Strech the applications menu to the corner */
	panel_stretch_events_to_toplevel (menuitem,
					 TRUE /* top */,
					 FALSE /* right */,
					 FALSE /* bottom */,
					 TRUE /* left */);

	append_actions_menu (menu_bar);

	gtk_box_pack_start (GTK_BOX (foo->hbox), menu_bar, FALSE, FALSE, 0);
	
	
	/* panel widget */
	foo->panel = panel_widget_new (NULL, FALSE, GTK_ORIENTATION_HORIZONTAL,
				       PANEL_SIZE_X_SMALL, PANEL_BACK_NONE,
				       NULL, FALSE, FALSE, FALSE, NULL);
	PANEL_WIDGET (foo->panel)->panel_parent = GTK_WIDGET (foo);
	PANEL_WIDGET (foo->panel)->drop_widget = GTK_WIDGET (foo);

	gtk_container_add (GTK_CONTAINER (foo->hbox), foo->panel);

	g_object_set_data (G_OBJECT (menu_bar), "menu_panel", foo->panel);

#if 0
	path = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
					  "panel/corner2.png", TRUE, NULL);
	bufmap = gnome_pixmap_new_from_file (path);
	g_free (path);
	align = gtk_alignment_new (1.0, 0.0, 1.0, 0.0);
	gtk_container_add (GTK_CONTAINER (align), bufmap);
	gtk_box_pack_end (GTK_BOX (foo->hbox), align, FALSE, FALSE, 0);
#endif

	bar = menu_bar = gtk_menu_bar_new ();
	gtk_widget_set_name (menu_bar,
			     "panel-foobar-menubar");

	append_clock_menu (foo, menu_bar);

	append_task_menu (foo, GTK_MENU_SHELL (bar));


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

	if (!DRAWER_IS_WIDGET (panel) && !FOOBAR_IS_WIDGET (panel))
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

	if (foo->windows != NULL)
		g_hash_table_destroy (foo->windows);
	foo->windows = NULL;

	if (GTK_OBJECT_CLASS (foobar_widget_parent_class)->destroy)
		GTK_OBJECT_CLASS (foobar_widget_parent_class)->destroy (o);
}

static void
foobar_widget_size_allocate (GtkWidget *w, GtkAllocation *alloc)
{
	if (GTK_WIDGET_CLASS (foobar_widget_parent_class)->size_allocate)
		GTK_WIDGET_CLASS (foobar_widget_parent_class)->size_allocate (w, alloc);

	if (GTK_WIDGET_REALIZED (w)) {
		FoobarWidget *foo = FOOBAR_WIDGET (w);
		xstuff_set_pos_size (w->window,
				     multiscreen_x (foo->screen),
				     multiscreen_y (foo->screen),
				     alloc->width,
				     alloc->height);

		g_slist_foreach (panel_list, queue_panel_resize, NULL);
		basep_border_queue_recalc (foo->screen);

		xstuff_set_wmspec_strut (w->window,
					 0 /* left */,
					 0 /* right */,
					 alloc->height /* top */,
					 0 /* bottom */);
	}
}

GtkWidget *
foobar_widget_new (const char *panel_id, int screen)
{
	FoobarWidget *foo;

	g_return_val_if_fail (screen >= 0, NULL);

	if (foobar_widget_exists (screen))
		return NULL;

	foo = g_object_new (FOOBAR_TYPE_WIDGET, NULL);

	/* Ugly hack to reset the unique id back to the original one */	
	if (panel_id != NULL) 
		panel_widget_set_id (PANEL_WIDGET (foo->panel), panel_id);

	foo->screen = screen;
	gtk_window_move (GTK_WINDOW (foo),
			 multiscreen_x (foo->screen),
			 multiscreen_y (foo->screen));
	g_object_set (G_OBJECT (foo),
		      "width_request", (int)multiscreen_width (foo->screen),
		      NULL);

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
			g_object_set_data (G_OBJECT (foo->programs),
					   "need_reread",
					   GINT_TO_POINTER (TRUE));
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
	if (BUTTON_IS_WIDGET (w)) {
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

	xstuff_set_wmspec_strut (widget->window,
				 0 /* left */,
				 0 /* right */,
				 widget->allocation.height /* top */,
				 0 /* bottom */);

	gtk_drag_dest_set (widget, 0, NULL, 0, 0);

	gtk_widget_map(widget);
}
