/* GNOME panel: foobar widGET
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

#include <libgnome/libgnome.h>
#include <gdk/gdkkeysyms.h>

/* Yes, yes I know, now bugger off ... */
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

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
#include "panel-marshal.h"
#include "egg-screen-exec.h"
#include "panel-stock-icons.h"
#include "panel-action-button.h"
#include "panel-recent.h"

#define ICON_SIZE 20
#define FOOBAR_MENU_FLAGS (MAIN_MENU_SYSTEM | MAIN_MENU_KDE_SUB | MAIN_MENU_DISTRIBUTION_SUB)

extern GlobalConfig global_config;
extern GSList *panel_list;

extern GtkTooltips *panel_tooltips;

static void foobar_widget_class_init	(FoobarWidgetClass *klass);
static void foobar_widget_instance_init (FoobarWidget *foo);
static void foobar_widget_realize	(GtkWidget *w);
static void foobar_widget_destroy	(GtkObject *o);
static void foobar_widget_size_allocate	(GtkWidget *toplevel,
					 GtkAllocation *allocation);
static void foobar_widget_size_request	(GtkWidget *toplevel,
					 GtkRequisition *requisition);
static gboolean foobar_leave_notify	(GtkWidget *widget,
					 GdkEventCrossing *event);
static gboolean foobar_enter_notify	(GtkWidget *widget,
					 GdkEventCrossing *event);
static gboolean foobar_widget_popup_panel_menu (FoobarWidget *foobar);

static void append_task_menu (FoobarWidget *foo, GtkMenuShell *menu_bar);
static void setup_task_menu (FoobarWidget *foo);

static GList *foobars = NULL;

static GtkWindowClass *foobar_widget_parent_class = NULL;

enum {
	POPUP_PANEL_MENU_SIGNAL,
	WIDGET_LAST_SIGNAL
};

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

static guint foobar_widget_signals [WIDGET_LAST_SIGNAL] = { 0 };

static void
foobar_widget_class_init (FoobarWidgetClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
	GtkBindingSet  *binding_set;

	binding_set = gtk_binding_set_by_class (klass);

	klass->popup_panel_menu = foobar_widget_popup_panel_menu;

	object_class->destroy = foobar_widget_destroy;

	widget_class->realize            = foobar_widget_realize;
	widget_class->size_allocate      = foobar_widget_size_allocate;
	widget_class->size_request       = foobar_widget_size_request;
	widget_class->enter_notify_event = foobar_enter_notify;
	widget_class->leave_notify_event = foobar_leave_notify;

	gtk_rc_parse_string ("style \"panel-foobar-menubar-style\"\n"
			     "{\n"
			     "GtkMenuBar::shadow-type = none\n"
			     "GtkMenuBar::internal-padding = 0\n"
			     "}\n"
			     "widget \"*.panel-foobar-menubar\" style \"panel-foobar-menubar-style\"");

	foobar_widget_signals [POPUP_PANEL_MENU_SIGNAL] =
		g_signal_new ("popup_panel_menu",
			     G_TYPE_FROM_CLASS (object_class),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			     G_STRUCT_OFFSET (FoobarWidgetClass, popup_panel_menu),
			     NULL,
			     NULL,
			     panel_marshal_BOOLEAN__VOID,
			     G_TYPE_BOOLEAN,
			     0);

	gtk_binding_entry_add_signal (binding_set, GDK_F10, GDK_CONTROL_MASK,
				     "popup_panel_menu", 0);
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

	return FALSE;
}

static void
append_actions_menu (FoobarWidget *foo,
		     GtkWidget    *menu_bar)
{
	GtkWidget *menu, *item;

	menu = panel_menu_new ();

	item = stock_menu_item_new (_("Run Program..."),
				    PANEL_STOCK_RUN,
				    FALSE);
	gtk_tooltips_set_tip (panel_tooltips, item,
			      _("Run applications, if you know the "
				"correct command to type in"),
			      NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (item, "activate",
			  G_CALLBACK (panel_action_run_program), NULL);
	setup_internal_applet_drag (item, "ACTION:run:NEW");

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	if (panel_is_program_in_path  ("gnome-search-tool")) {
		item = stock_menu_item_new (
				_("Search for Files..."),
				PANEL_STOCK_SEARCHTOOL,
				FALSE);

		gtk_tooltips_set_tip (panel_tooltips, item,
				      _("Find files, folders, and documents "
				        "on your computer"),
				      NULL);

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_signal_connect (item, "activate",
				  G_CALLBACK (panel_action_search), NULL);
		setup_internal_applet_drag (item, "ACTION:search:NEW");
	}

	panel_recent_append_documents_menu (menu);
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	if (panel_is_program_in_path ("gnome-panel-screenshot")) {
		item = stock_menu_item_new (_("Screenshot..."),
					    PANEL_STOCK_SCREENSHOT,
					    FALSE);
		gtk_tooltips_set_tip (panel_tooltips, item,
			      	      _("Take a screenshot of your desktop"),
			              NULL);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        	g_signal_connect (item, "activate",
			  	  G_CALLBACK (panel_action_screenshot), NULL);	 
		setup_internal_applet_drag (item, "ACTION:screenshot:NEW");
	}

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	if (panel_is_program_in_path  ("xscreensaver")) {
		item = stock_menu_item_new (_("Lock Screen"), 
					    PANEL_STOCK_LOCKSCREEN, 
					    FALSE);
		gtk_tooltips_set_tip (panel_tooltips, item,
				      _("Protect your computer from "
					"unauthorized use"),
				      NULL);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_signal_connect (item, "activate",
				  G_CALLBACK (panel_action_lock_screen), NULL);
		setup_internal_applet_drag (item, "ACTION:lock:NEW");
	}

	item = stock_menu_item_new (_("Log Out"),
				    PANEL_STOCK_LOGOUT,
				    FALSE);
	gtk_tooltips_set_tip (panel_tooltips, item,
			      _("Quit from the GNOME desktop"),
			      NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (panel_action_logout), 0);
	setup_internal_applet_drag  (item, "ACTION:logout:NEW");

	/* FIXME: shutdown or reboot */

	item = gtk_menu_item_new_with_label (_("Actions"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), item);

	panel_stretch_events_to_toplevel (item, PANEL_STRETCH_TOP);
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
programs_menu_to_display (GtkWidget    *menu,
			  FoobarWidget *foo)
{
	if (menu_need_reread (menu)) {
		while (GTK_MENU_SHELL (menu)->children)
			gtk_widget_destroy (GTK_MENU_SHELL (menu)->children->data);

		create_root_menu (
			menu, PANEL_WIDGET (foo->panel),
			TRUE, FOOBAR_MENU_FLAGS, FALSE, FALSE);
	}
}

static void
set_the_task_submenu (FoobarWidget *foo, GtkWidget *item)
{
	foo->task_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), foo->task_menu);
	/*g_message ("setting...");*/
	g_object_set_data (G_OBJECT (foo->task_menu),
			   "menu_panel", foo->panel);
	g_signal_connect (G_OBJECT (foo->task_menu), "show",
			  G_CALLBACK (panel_make_sure_menu_within_screen),
			  NULL);
	g_signal_connect (G_OBJECT (foo->task_menu), "show",
			  G_CALLBACK (our_gtk_menu_position),
			  NULL);
}

static void
focus_window (GtkWidget *w, WnckWindow *window)
{
	WnckWorkspace* space;

	/* Make sure that the current workspace is the same as the app'
	 * Same behaviour as GNOME 1.4 */
	space = wnck_window_get_workspace (window);
	wnck_workspace_activate (space);

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

	if ( ! looked) {
		pixbuf = panel_make_menu_icon ("gnome-tasklist.png",
					       /* evil fallback huh? */
					       "gnome-gmenu.png",
					       20 /* size */,
					       NULL /* long_operation */);

		looked = TRUE;
	}

	return pixbuf;
}

static void
add_window (WnckWindow *window, FoobarWidget *foo)
{
	WnckScreen    *screen;
	WnckWorkspace *wspace;
	GtkWidget     *item, *label;
	GtkWidget     *image = NULL;
	GdkPixbuf     *pb;
	const char    *name;
	char          *title = NULL;
	int            slen;

	g_assert (foo->windows != NULL);

	screen = wnck_screen_get (foo->screen);
	wspace = wnck_screen_get_active_workspace (screen);

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
			g_object_unref (G_OBJECT (scaled));
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
	GtkWidget    *separator;
	WnckScreen   *screen;
	GList        *windows;
	GList        *list;

	screen  = wnck_screen_get (foo->screen);
	windows = wnck_screen_get_windows (screen);

	foo->windows = g_hash_table_new (g_direct_hash, g_direct_equal);

	separator = add_menu_separator (foo->task_menu);

	g_list_foreach (windows, (GFunc) add_window, foo);

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
set_das_pixmap (FoobarWidget *foo,
		WnckWindow   *window)
{
	GdkPixbuf *pixbuf = NULL;

	if (!GTK_WIDGET_REALIZED (foo))
		return;

	foo->icon_window = window;

	if (window)
		pixbuf = wnck_window_get_mini_icon (window);

	if (!pixbuf)
		pixbuf = get_default_image ();

	if (pixbuf) {
		double pix_x, pix_y;

		pix_x = gdk_pixbuf_get_width (pixbuf);
		pix_y = gdk_pixbuf_get_height (pixbuf);

		if (pix_x <= ICON_SIZE && pix_y <= ICON_SIZE) 
			gtk_image_set_from_pixbuf (
				GTK_IMAGE (foo->task_image), pixbuf);
		else {
			GdkPixbuf *scaled;
			double     greatest;

			greatest = pix_x > pix_y ? pix_x : pix_y;
			scaled = gdk_pixbuf_scale_simple (
					pixbuf,
					(ICON_SIZE / greatest) * pix_x,
					(ICON_SIZE / greatest) * pix_y,
					GDK_INTERP_BILINEAR);
			gtk_image_set_from_pixbuf (
				GTK_IMAGE (foo->task_image), scaled);
			g_object_unref (scaled);
		}
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

	foo->task_image = gtk_image_new ();
	gtk_widget_show (foo->task_image);
	gtk_container_add (GTK_CONTAINER (foo->task_bin), foo->task_image);

	gtk_menu_shell_append (menu_bar, foo->task_item);

	panel_stretch_events_to_toplevel (
		foo->task_item, PANEL_STRETCH_TOP | PANEL_STRETCH_RIGHT);
}

static void
icon_changed (WnckWindow *window, FoobarWidget *foo)
{
	if (foo->icon_window == window)
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

static void
active_window_changed (WnckScreen   *screen,
		       FoobarWidget *foo)
{
	WnckWindow *window = wnck_screen_get_active_window (screen);

	/* icon might have changed */
	if (foo->icon_window != window)
		set_das_pixmap (foo, window);
}

static void
window_opened (WnckScreen   *screen,
	       WnckWindow   *window,
	       FoobarWidget *foo)
{
	if (foo->windows != NULL)
		add_window (window, foo);

	bind_window_changes (window, foo);
}

static void
window_closed (WnckScreen   *screen,
	       WnckWindow   *window,
	       FoobarWidget *foo)
{
	if (window == foo->icon_window)
		set_das_pixmap (foo, NULL);

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
	WnckScreen *screen;
	GList      *windows, *l;

	g_assert (foo->task_item != NULL);

	g_signal_connect (G_OBJECT (foo->task_item), "select",
			  G_CALLBACK (create_task_menu), foo);
	g_signal_connect (G_OBJECT (foo->task_item), "deselect",
			  G_CALLBACK (destroy_task_menu), foo);

	set_the_task_submenu (foo, foo->task_item);

	screen = wnck_screen_get (foo->screen);

	/* setup the pixmap to the focused task */
	windows = wnck_screen_get_windows (screen);
	for (l = windows; l; l = l->next)
		if (wnck_window_is_active (l->data)) {
			set_das_pixmap  (foo, l->data);
			break;
		}

	/* if no focused task found, then just set it to default */
	if (!l)
		set_das_pixmap  (foo, NULL);

	g_list_foreach (windows, (GFunc) bind_window_changes, foo);

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
	GtkWindow *window;
	GtkWidget *bufmap;
	GtkWidget *menu_bar;
	GtkWidget *task_bar;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkWidget *align;
	GtkWidget *image;
	char      *path;

	window = GTK_WINDOW (foo);

	foo->screen  = 0;
	foo->monitor = 0;

	foo->windows    = NULL;
	foo->task_item  = NULL;
	foo->task_menu  = NULL;
	foo->task_image = NULL;
	foo->task_bin   = NULL;
	foo->icon_window = NULL;

	foo->compliant_wm = xstuff_is_compliant_wm ();
	if(foo->compliant_wm)
		GTK_WINDOW(foo)->type = GTK_WINDOW_TOPLEVEL;
	else
		GTK_WINDOW(foo)->type = GTK_WINDOW_POPUP;

	window->allow_shrink = TRUE;
	window->allow_grow   = TRUE;

	g_signal_connect (G_OBJECT (foo), "delete_event",
			  G_CALLBACK (gtk_true), NULL);

	/* panel widget */
	foo->panel = panel_widget_new (NULL, FALSE, GTK_ORIENTATION_HORIZONTAL,
				       PANEL_SIZE_X_SMALL, PANEL_BACK_NONE,
				       NULL, FALSE, FALSE, FALSE, NULL);
	PANEL_WIDGET (foo->panel)->panel_parent = GTK_WIDGET (foo);
	PANEL_WIDGET (foo->panel)->drop_widget = GTK_WIDGET (foo);
	panel_set_atk_name_desc (foo->panel, _("Menu Panel"), _("GNOME Menu Panel"));

	foo->ebox = gtk_event_box_new ();
	foo->hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_set_direction (foo->hbox, GTK_TEXT_DIR_LTR);
	gtk_container_add(GTK_CONTAINER(foo->ebox), foo->hbox);

	path = panel_pixmap_discovery ("panel-corner-left.png", FALSE /* fallback */);
	if (path != NULL) {
		bufmap = gtk_image_new_from_file (path);
		g_free (path);
		align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
		gtk_container_add (GTK_CONTAINER (align), bufmap);
		gtk_box_pack_start (GTK_BOX (foo->hbox), align, FALSE, FALSE, 0);
	}

	menu_bar = gtk_menu_bar_new ();	
	gtk_widget_set_name (menu_bar,
			     "panel-foobar-menubar");

	menuitem = gtk_image_menu_item_new_with_label (_("Applications"));
	image = gtk_image_new_from_stock (
			PANEL_STOCK_GNOME_LOGO,
			panel_foobar_icon_get_size ()),
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

	menu = create_root_menu (
			NULL, PANEL_WIDGET (foo->panel),
			TRUE, FOOBAR_MENU_FLAGS, FALSE, FALSE);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), menuitem);
	g_signal_connect (G_OBJECT (menu), "show",
			  G_CALLBACK (programs_menu_to_display),
			  foo);
	foo->programs = menu;

	/* Strech the applications menu to the corner */
	panel_stretch_events_to_toplevel (
		menuitem, PANEL_STRETCH_TOP | PANEL_STRETCH_LEFT);

	append_actions_menu (foo, menu_bar);

	gtk_box_pack_start (GTK_BOX (foo->hbox), menu_bar, FALSE, FALSE, 0);
	
	gtk_container_add (GTK_CONTAINER (foo->hbox), foo->panel);

	path = panel_pixmap_discovery ("panel-corner-right.png", FALSE /* fallback */);
	if (path != NULL) {
		bufmap = gtk_image_new_from_file (path);
		g_free (path);
		align = gtk_alignment_new (1.0, 0.0, 1.0, 0.0);
		gtk_container_add (GTK_CONTAINER (align), bufmap);
		gtk_box_pack_end (GTK_BOX (foo->hbox), align, FALSE, FALSE, 0);
	}

	task_bar = gtk_menu_bar_new ();
	gtk_widget_set_name (task_bar, "panel-foobar-menubar");

	append_task_menu (foo, GTK_MENU_SHELL (task_bar));

	gtk_box_pack_end (GTK_BOX (foo->hbox), task_bar, FALSE, FALSE, 0);
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

	g_slist_foreach (panel_list, queue_panel_resize, NULL);

	if (foo->windows != NULL)
		g_hash_table_destroy (foo->windows);
	foo->windows = NULL;

	if (GTK_OBJECT_CLASS (foobar_widget_parent_class)->destroy)
		GTK_OBJECT_CLASS (foobar_widget_parent_class)->destroy (o);
}

void
foobar_widget_screen_size_changed (FoobarWidget *foo,
				   GdkScreen    *screen)
{
	GtkWindow *window;
	int        w, h;

	window = GTK_WINDOW (foo);
	
	gtk_window_get_size (window, &w, &h);

	gtk_window_move (window,
			 multiscreen_x (foo->screen, foo->monitor),
			 multiscreen_y (foo->screen, foo->monitor));

	gtk_window_set_resizable (window, TRUE);
	gtk_window_resize (window,
			   multiscreen_width (foo->screen, foo->monitor),
			   h);
}

static void
foobar_widget_size_allocate (GtkWidget     *toplevel,
			     GtkAllocation *allocation)
{
	FoobarWidget *foo;

	GTK_WIDGET_CLASS (foobar_widget_parent_class)->size_allocate (toplevel, allocation);

	if (!GTK_WIDGET_REALIZED (toplevel))
		return;

	foo = FOOBAR_WIDGET (toplevel);

	g_slist_foreach (panel_list, queue_panel_resize, NULL);
	basep_border_queue_recalc (foo->screen, foo->monitor);

	xstuff_set_wmspec_strut (toplevel->window, 0, 0, allocation->height, 0);
}

static void
foobar_widget_size_request (GtkWidget      *toplevel,
			    GtkRequisition *requisition)
{
	FoobarWidget *foo;
	int           old_height;

	foo = FOOBAR_WIDGET (toplevel);

	old_height = toplevel->requisition.height;

	GTK_WIDGET_CLASS (foobar_widget_parent_class)->size_request (toplevel, requisition);

	if (!GTK_WIDGET_REALIZED (toplevel))
		return;

	requisition->width = multiscreen_width (foo->screen, foo->monitor);

	if (requisition->height != old_height)
		gtk_window_resize (GTK_WINDOW (toplevel),
				   requisition->width,
				   requisition->height);

	xstuff_set_pos_size (toplevel->window,
			     multiscreen_x (foo->screen, foo->monitor),
			     multiscreen_y (foo->screen, foo->monitor),
			     requisition->width,
			     requisition->height);
}

GtkWidget *
foobar_widget_new (const char *panel_id,
		   int         screen,
		   int         monitor)
{
	FoobarWidget *foo;

	g_return_val_if_fail (screen >= 0, NULL);
	g_return_val_if_fail (monitor >= 0, NULL);

	if (foobar_widget_exists (screen, monitor))
		return NULL;

	foo = g_object_new (FOOBAR_TYPE_WIDGET, NULL);

	if (panel_id)
		panel_widget_set_id (PANEL_WIDGET (foo->panel), panel_id);

	foo->screen  = screen;
	foo->monitor = monitor;

	gtk_window_set_screen (GTK_WINDOW (foo),
			       panel_screen_from_number (screen));

	gtk_window_move (GTK_WINDOW (foo),
			 multiscreen_x (screen, monitor),
			 multiscreen_y (screen, monitor));

	foobars = g_list_prepend (foobars, foo);

	return GTK_WIDGET (foo);
}

gboolean
foobar_widget_exists (int screen, int monitor)
{
	GList *l;

	g_return_val_if_fail (screen  >= 0, 0);
	g_return_val_if_fail (monitor >= 0, 0);

	for (l = foobars; l; l = l->next) {
		FoobarWidget *foo = l->data;

		if (foo->screen == screen &&
		    foo->monitor == monitor)
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

int
foobar_widget_get_height (int screen, int monitor)
{
	GList *l;

	g_return_val_if_fail (screen  >= 0, 0);
	g_return_val_if_fail (monitor >= 0, 0);

	for (l = foobars; l; l = l->next) {
		FoobarWidget *foo = FOOBAR_WIDGET (l->data);

		if (foo->screen  == screen &&
		    foo->monitor == monitor)
			return GTK_WIDGET (foo)->allocation.height;
	}

	return 0; 
}

static void
reparent_button_widgets(GtkWidget *w, gpointer data)
{
	GdkWindow *newwin = data;
	if (BUTTON_IS_WIDGET (w)) {
		GtkButton *button = GTK_BUTTON (w);
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

static gboolean
foobar_widget_popup_panel_menu (FoobarWidget *foobar)
{
 	gboolean retval;

	g_signal_emit_by_name (foobar->panel, "popup_menu", &retval);

	return retval;
}

