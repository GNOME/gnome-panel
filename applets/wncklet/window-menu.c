/* window-menu.c: Window Selector applet
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2001 Free Software Foundation, Inc.
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 *      George Lebl <jirka@5z.com>
 *      Jacob Berkman <jacob@helixcode.com>
 */

#include <config.h>

#include "window-menu.h"

#include <string.h>

#include <panel-applet.h>
#include <gdk/gdkkeysyms.h>
#include <libgnomeui/libgnomeui.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

#include "inlinepixbufs.h"
#include "eel/eel-ellipsizing-label.h"
#include "wncklet.h"

typedef struct {
	GtkWidget    *applet;

	GtkWidget    *frame;
	GtkWidget    *image;
	GtkWidget    *menu;
	GtkWidget    *no_windows_item;
	GtkWidget    *about_dialog;

	GdkPixbuf    *icon_pixbuf;
	WnckWindow   *icon_window;
	GHashTable   *window_hash;

	int           size;

	GnomeIconTheme *icon_theme;
} WindowMenu;

static void window_menu_connect_to_window (WindowMenu *window_menu,
					   WnckWindow *window);

static void
window_menu_help (BonoboUIComponent *uic,
		  WindowMenu        *window_menu,
		  const char        *verb) 
{
	GError *error = NULL;

	gnome_help_display_desktop_on_screen (
		NULL, "user-guide", "wgosmetacity.xml", "gosmetacity-27",
		gtk_widget_get_screen (window_menu->applet),
		&error);

	if (error) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (
				NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("There was an error displaying help: %s"),
				error->message);

		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_widget_get_screen (window_menu->applet));
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

static void
window_menu_about (BonoboUIComponent *uic,
		   WindowMenu        *window_menu,
		   const char        *verb) 
{
	static const char *authors[] = {
		"Mark McLoughlin <mark@skynet.ie>",
		"George Lebl <jirka@5z.com>",
		"Jacob Berkman <jacob@helixcode.com>",
		NULL
	};
	const char *documenters [] = { 
                "Sun GNOME Documentation Team <gdocteam@sun.com>",
                NULL 
        };
	const char *translator_credits = _("translator_credits");

	GdkPixbuf        *pixbuf = NULL;
	char             *file;

	if (window_menu->about_dialog) {
		gtk_window_set_screen (GTK_WINDOW (window_menu->about_dialog),
				       gtk_widget_get_screen (window_menu->applet));
		gtk_window_present (GTK_WINDOW (window_menu->about_dialog));
		return;
	}

	file = gnome_icon_theme_lookup_icon (window_menu->icon_theme,
					     "panel-window-menu",
					     48, NULL, NULL);
	pixbuf = gdk_pixbuf_new_from_file (file, NULL);
	g_free (file);

	window_menu->about_dialog = gnome_about_new (_("Window Selector"), VERSION,
				 "Copyright \xc2\xa9 2003 Sun Microsystems, Inc.\n"
				 "Copyright \xc2\xa9 2001 Free Software Foundation, Inc.\n"
				 "Copyright \xc2\xa9 2000 Helix Code, Inc.",
				 _("The Window Selector shows a list of all windows and lets you browse them."),
				 authors,
				 documenters,
				 strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
				 pixbuf);

	gtk_window_set_wmclass (GTK_WINDOW (window_menu->about_dialog), "window-menu", "WindowMenu");
	gtk_window_set_screen (GTK_WINDOW (window_menu->about_dialog),
			       gtk_widget_get_screen (window_menu->applet));

	if (pixbuf) {
		gtk_window_set_icon (GTK_WINDOW (window_menu->about_dialog), pixbuf);
		g_object_unref (pixbuf);
	}

	g_signal_connect (window_menu->about_dialog, "destroy",
			  (GCallback) gtk_widget_destroyed, &window_menu->about_dialog);
	gtk_widget_show (window_menu->about_dialog);
}

static const BonoboUIVerb window_menu_verbs [] =
{   
	BONOBO_UI_UNSAFE_VERB ("WindowMenuHelp",  window_menu_help),
	BONOBO_UI_UNSAFE_VERB ("WindowMenuAbout", window_menu_about),
	BONOBO_UI_VERB_END
};

static void
window_menu_destroy (GtkWidget  *widget,
		     WindowMenu *window_menu)
{
	if (window_menu->menu)
		gtk_widget_destroy (window_menu->menu);
	window_menu->menu = NULL;

	if (window_menu->icon_pixbuf)
		g_object_unref (window_menu->icon_pixbuf);
	window_menu->icon_pixbuf = NULL;

	if (window_menu->icon_theme)
		g_object_unref (window_menu->icon_theme);
        window_menu->icon_theme = NULL;

	if (window_menu->no_windows_item)
		g_object_unref (window_menu->no_windows_item);
	window_menu->no_windows_item = NULL;

	if (window_menu->about_dialog) {
		gtk_widget_destroy (window_menu->about_dialog);
		window_menu->about_dialog = NULL;
	}

	g_free (window_menu);
}

static WnckScreen *
window_menu_get_screen (WindowMenu *window_menu)
{
	GdkScreen *screen;

	screen = gtk_widget_get_screen (window_menu->applet);

	return wnck_screen_get (gdk_screen_get_number (screen));
}

static GdkPixbuf *
window_menu_get_default_window_icon (void) 
{
	static GdkPixbuf *retval = NULL;

	if (retval)
		return retval;

	retval = gdk_pixbuf_new_from_inline (
			-1, default_icon_data, FALSE, NULL);
 
	g_assert (retval);

	return retval;
}

static void
window_menu_set_window_icon (WindowMenu *window_menu,
			     GtkWidget  *image,
			     WnckWindow *window,
			     gboolean    use_icon_size)
{
	GdkPixbuf *pixbuf, *freeme;
	int        width, height;
	int        icon_size = -1;

	pixbuf = NULL;
	freeme = NULL;
	
	if (window)
		pixbuf = wnck_window_get_icon (window);

	if (!pixbuf)
		pixbuf = window_menu_get_default_window_icon ();

	if (!use_icon_size && window_menu->size > 1)
		icon_size = window_menu->size;

	if (icon_size == -1)
		gtk_icon_size_lookup (
			GTK_ICON_SIZE_MENU, NULL, &icon_size);

	width  = gdk_pixbuf_get_width  (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	if (icon_size != -1 &&
	    (width > icon_size || height > icon_size)) {
		double scale;

		scale = ((double) icon_size) / MAX (width, height);

		pixbuf = gdk_pixbuf_scale_simple (
				pixbuf, width * scale, height * scale, GDK_INTERP_BILINEAR);
		freeme = pixbuf;
	}

	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);

	if (freeme)
		g_object_unref (freeme);
}

static void
window_menu_size_allocate (GtkWidget     *image,
			   GtkAllocation *allocation,
			   WindowMenu    *window_menu)
{
	PanelAppletOrient orientation;
	int               size = -1;

	if (image->allocation.width  == window_menu->size &&
	    image->allocation.height == window_menu->size)
		return;

	orientation = panel_applet_get_orient (PANEL_APPLET (window_menu->applet));

	switch (orientation) {
	case PANEL_APPLET_ORIENT_UP:
	case PANEL_APPLET_ORIENT_DOWN:
		size = image->allocation.height;
		break;
	case PANEL_APPLET_ORIENT_LEFT:
	case PANEL_APPLET_ORIENT_RIGHT:
		size = image->allocation.width;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	if (window_menu->size == size)
		return;

	window_menu->size = size;

	window_menu_set_window_icon (
		window_menu, image, window_menu->icon_window, FALSE);
}

static void
window_menu_set_active_window (WindowMenu *window_menu,
			       WnckWindow *window)
{
	window_menu_set_window_icon (
		window_menu, window_menu->image, window, FALSE);
	window_menu->icon_window = window;
}

static void
window_menu_window_icon_changed (WnckWindow *window,
				 WindowMenu *window_menu)
{
	if (window_menu->icon_window == window)
		window_menu_set_active_window (window_menu, window);
}

static void
window_menu_active_window_changed (WnckScreen *screen,
				   WindowMenu *window_menu)
{
	WnckWindow *window;
	
	window = wnck_screen_get_active_window (screen);

	if (window_menu->icon_window != window)
		window_menu_set_active_window (window_menu, window);
}

static void
window_menu_activate_window (WnckWindow *window)
{
	WnckWorkspace *workspace;

	workspace = wnck_window_get_workspace (window);
	wnck_workspace_activate (workspace);

	if (wnck_window_is_minimized (window))
		wnck_window_unminimize (window);
	
	wnck_window_activate (window);
}

#define WINDOW_MENU_MAX_WIDTH 50	/* maximum width in characters */

static gint
get_width (GtkWidget *widget, const char *text)
{
	PangoContext *context;
	PangoFontMetrics *metrics;
	gint char_width;
	PangoLayout *layout;
	PangoRectangle natural;
	gint max_width;
	gint screen_width;
	gint width;

	gtk_widget_ensure_style (widget);

	context = gtk_widget_get_pango_context (widget);
	metrics = pango_context_get_metrics (
		context, widget->style->font_desc,
		pango_context_get_language (context));
	char_width = pango_font_metrics_get_approximate_char_width (metrics);
	pango_font_metrics_unref (metrics);
	max_width = PANGO_PIXELS (WINDOW_MENU_MAX_WIDTH * char_width);

	layout = gtk_widget_create_pango_layout (widget, text);
	pango_layout_get_pixel_extents (layout, NULL, &natural);
	g_object_unref (G_OBJECT (layout));

	screen_width = gdk_screen_get_width (gtk_widget_get_screen (widget));

	width = MIN (natural.width, max_width);
	width = MIN (width, 3 * (screen_width / 4));
	
	return width;
}

static GtkWidget*
window_menu_item_new (WindowMenu  *window_menu,
		      const gchar *label)
{
	GtkWidget *item;
	GtkWidget *ellipsizing_label;

	item = gtk_image_menu_item_new ();
	
	ellipsizing_label = eel_ellipsizing_label_new (label);
	gtk_misc_set_alignment (GTK_MISC (ellipsizing_label), 0.0, 0.5);

	gtk_container_add (GTK_CONTAINER (item), ellipsizing_label);

	gtk_widget_show (ellipsizing_label);

	gtk_widget_set_size_request (
		ellipsizing_label, get_width (window_menu->applet, label), -1);
	return item;
}

static void
window_menu_add_window (WindowMenu *window_menu,
			WnckWindow *window)
{
	WnckWorkspace *workspace; 
	GtkWidget     *item;
	GtkWidget     *image;
	const char    *name;
	char          *label;
	char          *freeme = NULL;

	if (wnck_window_is_skip_tasklist (window))
		return;

	name = wnck_window_get_name (window);
	if (!name)
		name = _("Unknown Window");

	if (!wnck_window_is_minimized (window))
		label = (char *) name;
	else {
		label = g_strdup_printf ("[%s]", name);
		freeme = label;
	}

	item = window_menu_item_new (window_menu, label);
	if (freeme)
		g_free (freeme);

	image = gtk_image_new ();

	window_menu_set_window_icon (window_menu, image, window, TRUE);

	gtk_image_menu_item_set_image (
			GTK_IMAGE_MENU_ITEM (item), GTK_WIDGET (image));
	gtk_widget_show (image);

	workspace = wnck_screen_get_active_workspace (
				window_menu_get_screen (window_menu));

	if (wnck_window_get_workspace (window) == workspace)
		gtk_menu_shell_prepend (GTK_MENU_SHELL (window_menu->menu), item);
	else
		gtk_menu_shell_append (GTK_MENU_SHELL (window_menu->menu), item);

	g_hash_table_insert (window_menu->window_hash, window, item);
	g_signal_connect_swapped (item, "activate",
				  G_CALLBACK (window_menu_activate_window),
				  window);

	gtk_widget_show (item);
}

static void
window_menu_window_opened (WnckScreen *screen,
			   WnckWindow *window,
			   WindowMenu *window_menu)
{
	if (window_menu->menu && GTK_WIDGET_VISIBLE (window_menu->menu)) {
		if (window_menu->no_windows_item
		    && GTK_WIDGET_VISIBLE (window_menu->no_windows_item))
			gtk_widget_hide (window_menu->no_windows_item);
		window_menu_add_window (window_menu, window);
		gtk_menu_reposition (GTK_MENU (window_menu->menu));
	}

	window_menu_connect_to_window (window_menu, window);
}

static void
window_menu_window_closed (WnckScreen *screen,
			   WnckWindow *window,
			   WindowMenu *window_menu)
{
	GtkWidget *item;

	if (window == window_menu->icon_window)
		window_menu_set_active_window (window_menu, NULL);

	if (!window_menu->menu || !GTK_WIDGET_VISIBLE (window_menu->menu))
		return;
	
	item = g_hash_table_lookup (window_menu->window_hash, window);
	if (!item)
		return;

	gtk_widget_hide (item);
	gtk_menu_reposition (GTK_MENU (window_menu->menu));
}

static void
window_menu_connect_to_window (WindowMenu *window_menu,
			       WnckWindow *window)
{
	wncklet_connect_while_alive (window, "icon_changed",
				     G_CALLBACK (window_menu_window_icon_changed),
				     window_menu,
				     window_menu->applet);
}

static void
window_menu_connect_to_screen (WindowMenu *window_menu,
			       WnckScreen *screen)
{
	wncklet_connect_while_alive (screen, "active_window_changed",
				     G_CALLBACK (window_menu_active_window_changed),
				     window_menu,
				     window_menu->applet);

	wncklet_connect_while_alive (screen, "window_opened",
				     G_CALLBACK (window_menu_window_opened),
				     window_menu,
				     window_menu->applet);

	wncklet_connect_while_alive (screen, "window_closed",
				     G_CALLBACK (window_menu_window_closed),
				     window_menu,
				     window_menu->applet);
}

static void
window_menu_destroy_menu (GtkWidget  *widget,
			  WindowMenu *window_menu)
{
	window_menu->menu = NULL;

	if (window_menu->window_hash)
		g_hash_table_destroy (window_menu->window_hash);
	window_menu->window_hash = NULL;
}

static void
window_menu_menu_hidden (GtkWidget  *menu,
			 WindowMenu *window_menu)
{
	gtk_frame_set_shadow_type (GTK_FRAME (window_menu->frame), GTK_SHADOW_NONE);
}

static void
window_menu_position_menu (GtkMenu  *menu,
			   int      *x,
			   int      *y,
			   gboolean *push_in,
			   gpointer  user_data)
{
	GtkWidget *widget = GTK_WIDGET (user_data);
	GtkRequisition requisition;
	gint menu_xpos;
	gint menu_ypos;

	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	gdk_window_get_origin (widget->window, &menu_xpos, &menu_ypos);

	menu_xpos += widget->allocation.x;
	menu_ypos += widget->allocation.y;

	if (menu_ypos > gdk_screen_get_height (gtk_widget_get_screen (widget)) / 2)
		menu_ypos -= requisition.height;
	else
		menu_ypos += widget->allocation.height;

	*x = menu_xpos;
	*y = menu_ypos;
	*push_in = TRUE;
}

static void
window_menu_popup_menu (WindowMenu *window_menu,
			guint       button,
			guint32     activate_time)
			
{
	GtkWidget  *seperator;
	WnckScreen *screen;
	GList      *windows;
	GList      *l, *list;

	if (!window_menu->menu) {
		window_menu->menu = gtk_menu_new (); 
		g_signal_connect (window_menu->menu, "hide",
				  G_CALLBACK (window_menu_menu_hidden), window_menu);
	}

	list = gtk_container_get_children (GTK_CONTAINER (window_menu->menu));
	for (l = list; l; l = l->next)
		gtk_container_remove (GTK_CONTAINER (window_menu->menu), l->data);
	g_list_free (list);

	if (window_menu->no_windows_item)
		g_object_unref (window_menu->no_windows_item);
	window_menu->no_windows_item = NULL;

	g_signal_connect (window_menu->menu, "destroy",
			  G_CALLBACK (window_menu_destroy_menu), window_menu);

	seperator = gtk_separator_menu_item_new ();
	gtk_widget_show (seperator);
	gtk_menu_shell_append (GTK_MENU_SHELL (window_menu->menu), seperator);

	screen = window_menu_get_screen (window_menu);
	windows = wnck_screen_get_windows (screen);

	if (window_menu->window_hash)
		g_hash_table_destroy (window_menu->window_hash);
	window_menu->window_hash = g_hash_table_new (g_direct_hash, g_direct_equal);

	for (l = windows; l; l = l->next)
		window_menu_add_window (window_menu, l->data);

	l = GTK_MENU_SHELL (window_menu->menu)->children;
	if (l && seperator == l->data) {
		gtk_widget_destroy (seperator);
		l = GTK_MENU_SHELL (window_menu->menu)->children;
	}

	if (!l) {
		window_menu->no_windows_item = window_menu_item_new (
							window_menu,
							_("No Windows Open"));

		gtk_widget_set_sensitive (window_menu->no_windows_item, FALSE);
		gtk_widget_show (window_menu->no_windows_item);	
		gtk_menu_shell_append (GTK_MENU_SHELL (window_menu->menu),
				       window_menu->no_windows_item);
	}

	gtk_widget_show (window_menu->menu);

	gtk_menu_set_screen (GTK_MENU (window_menu->menu),
			     gtk_widget_get_screen (window_menu->applet));

	gtk_frame_set_shadow_type (GTK_FRAME (window_menu->frame), GTK_SHADOW_IN);

	gtk_menu_popup (GTK_MENU (window_menu->menu),
			NULL, NULL,
			window_menu_position_menu, window_menu->frame,
			button, activate_time);
}

static gboolean
window_menu_button_press_event (GtkWidget      *widget,
				GdkEventButton *event,
				WindowMenu     *window_menu)
{
	if (event->button != 1)
		return FALSE;

	window_menu_popup_menu (window_menu, event->button, event->time);

	return TRUE;
}

static gboolean
window_menu_key_press_event (GtkWidget   *widget,
			     GdkEventKey *event,
			     WindowMenu  *window_menu)
{
	switch (event->keyval) {
	case GDK_KP_Enter:
	case GDK_ISO_Enter:
	case GDK_3270_Enter:
	case GDK_Return:
	case GDK_space:
	case GDK_KP_Space:
		window_menu_popup_menu (window_menu, 0, event->time);
		return TRUE;
	default:
		break;
	}

	return FALSE;
}

static void
window_menu_setup_menu (WindowMenu *window_menu)
{
	WnckScreen *screen;
	GList      *windows, *l;

	screen = window_menu_get_screen (window_menu);
	windows = wnck_screen_get_windows (screen);

	for (l = windows; l; l = l->next)
		if (wnck_window_is_active (l->data))
			break;

	window_menu_set_active_window (window_menu, l ? l->data : NULL);

	for (l = windows; l; l = l->next)
		window_menu_connect_to_window (window_menu, l->data);

	window_menu_connect_to_screen (window_menu, screen);
}

gboolean
window_menu_applet_fill (PanelApplet *applet)
{
	WindowMenu *window_menu;
	GtkWidget  *alignment;
	AtkObject  *atk_obj;

	window_menu = g_new0 (WindowMenu, 1);

	window_menu->applet = GTK_WIDGET (applet);
	atk_obj = gtk_widget_get_accessible (window_menu->applet);
	atk_object_set_name (atk_obj, _("Window Selector"));
	atk_object_set_description (atk_obj, _("Tool to switch between windows"));

	panel_applet_set_flags (applet, PANEL_APPLET_EXPAND_MINOR);

        window_menu->icon_theme = gnome_icon_theme_new ();

	g_signal_connect (window_menu->applet, "destroy",
			  G_CALLBACK (window_menu_destroy), window_menu);

	panel_applet_setup_menu_from_file (
		applet, NULL, "GNOME_WindowMenuApplet.xml",
		NULL, window_menu_verbs, window_menu);

	window_menu->frame = gtk_frame_new (NULL);
	gtk_container_set_border_width (GTK_CONTAINER (window_menu->frame), 0);
	gtk_frame_set_shadow_type (GTK_FRAME (window_menu->frame), GTK_SHADOW_NONE);
	gtk_widget_show (window_menu->frame);

	alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_widget_show (alignment);
	gtk_container_add (GTK_CONTAINER (window_menu->frame), alignment);

	window_menu->image = gtk_image_new ();
	gtk_widget_show (window_menu->image);

	gtk_container_add (GTK_CONTAINER (alignment), window_menu->image);

	window_menu->size = 1;

	g_signal_connect (window_menu->image, "size_allocate",
			  G_CALLBACK (window_menu_size_allocate), window_menu);
 
	gtk_container_add (GTK_CONTAINER (window_menu->applet), window_menu->frame); 

	window_menu_setup_menu (window_menu);

	g_signal_connect (window_menu->applet, "button_press_event",
			  G_CALLBACK (window_menu_button_press_event), window_menu);
	g_signal_connect (window_menu->applet, "key_press_event",
			  G_CALLBACK (window_menu_key_press_event), window_menu);

    	gtk_widget_show (GTK_WIDGET (window_menu->applet));

	return TRUE;
}
