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
#include <libgnomeui/gnome-help.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

#include "inlinepixbufs.h"
#include "wncklet.h"

typedef struct {
	GtkWidget    *applet;

	GtkWidget    *image;
	GtkWidget    *menu;
	GtkWidget    *menu_bar;
	GtkWidget    *menu_item;
	GtkWidget    *no_windows_item;
	GtkWidget    *about_dialog;

	GdkPixbuf    *icon_pixbuf;
	WnckWindow   *icon_window;
	GHashTable   *window_hash;

	int           size;
} WindowMenu;

typedef struct {
	GtkWidget *item;
	GtkWidget *label;
} window_hash_item;

static void window_menu_connect_to_window (WindowMenu *window_menu,
					   WnckWindow *window);

static void
window_menu_help (BonoboUIComponent *uic,
		  WindowMenu        *window_menu,
		  const char        *verb) 
{
	GError *error = NULL;

	gnome_help_display_desktop_on_screen (
		NULL, "user-guide", "user-guide.xml", "gosmetacity-27",
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
	const char *translator_credits = _("translator-credits");

	if (window_menu->about_dialog) {
		gtk_window_set_screen (GTK_WINDOW (window_menu->about_dialog),
				       gtk_widget_get_screen (window_menu->applet));
		gtk_window_present (GTK_WINDOW (window_menu->about_dialog));
		return;
	}

	window_menu->about_dialog = gtk_about_dialog_new ();
	g_object_set (window_menu->about_dialog,
		      "name",  _("Window Selector"),
		      "version", VERSION,
		      "copyright", "Copyright \xc2\xa9 2003 Sun Microsystems, Inc.\n"
				   "Copyright \xc2\xa9 2001 Free Software Foundation, Inc.\n"
				   "Copyright \xc2\xa9 2000 Helix Code, Inc.",
		      "comments", _("The Window Selector shows a list of all windows and lets you browse them."),
		      "authors", authors,
		      "documenters", documenters,
		      "translator_credits", strcmp (translator_credits, "translator-credits") != 0 ? translator_credits : NULL,
		      "logo_icon_name", "panel-window-menu",
		      NULL);

	gtk_window_set_wmclass (GTK_WINDOW (window_menu->about_dialog), "window-menu", "WindowMenu");
	gtk_window_set_screen (GTK_WINDOW (window_menu->about_dialog),
			       gtk_widget_get_screen (window_menu->applet));

	gtk_window_set_icon_name (GTK_WINDOW (window_menu->about_dialog),
				  "panel-window-menu"); 

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
	window_menu->no_windows_item = NULL;

	if (window_menu->icon_pixbuf)
		g_object_unref (window_menu->icon_pixbuf);
	window_menu->icon_pixbuf = NULL;

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
window_menu_dimm_icon (GdkPixbuf *pixbuf)
{
	int x, y, pixel_stride, row_stride;
	guchar *row, *pixels;
	int w, h;

	w = gdk_pixbuf_get_width (pixbuf);
	h = gdk_pixbuf_get_height (pixbuf);
	
	g_assert (gdk_pixbuf_get_has_alpha (pixbuf));
	
	pixel_stride = 4;
	
	row = gdk_pixbuf_get_pixels (pixbuf);
	row_stride = gdk_pixbuf_get_rowstride (pixbuf);
	
	for (y = 0; y < h; y++) {
			pixels = row;			
			for (x = 0; x < w; x++) {
				pixels[3] /= 2;				
				pixels += pixel_stride;
			}			
			row += row_stride;
	}
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

	if (wnck_window_is_minimized (window))
		window_menu_dimm_icon (pixbuf);
	
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

/* The results of this function will need to be freed. */
static char *
window_menu_get_window_name (WnckWindow *window)
{
	const char *const_name;
	char *return_value;
	char *name;

	const_name = wnck_window_get_name (window);
	if (!const_name)
		name = g_strdup (_("Unknown Window"));
	else
		name = g_strdup (const_name);

	if (wnck_window_demands_attention (window)) {
		return_value = g_strdup_printf ("<b>%s</b>", name);
		g_free (name);
		name = return_value;
	}

	if (wnck_window_is_shaded (window)) {
		return_value = g_strdup_printf ("=%s=", name);
		g_free (name);
	} else if (wnck_window_is_minimized (window)) {
		return_value = g_strdup_printf ("[%s]", name);
		g_free (name);
	} else
		return_value = name;

	return return_value;
}

static void
window_menu_window_icon_changed (WnckWindow *window,
				 WindowMenu *window_menu)
{
	window_hash_item *item;
	GtkWidget        *image;

	if (window_menu->icon_window == window)
		window_menu_set_active_window (window_menu, window);

	item = NULL;

	item = g_hash_table_lookup (window_menu->window_hash, window);
	if (item != NULL) {
		image = gtk_image_new ();
		window_menu_set_window_icon (window_menu, image, window, TRUE);		    
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item->item),
					       GTK_WIDGET (image));
		gtk_widget_show (image);
	}
}

static void
window_menu_window_name_changed (WnckWindow *window,
				 WindowMenu *window_menu)
{
	window_hash_item *item;
	char *window_name;

	item = NULL;
	window_name = NULL;

	item = g_hash_table_lookup (window_menu->window_hash, window);
	if (item != NULL) {
		window_name = window_menu_get_window_name (window);
		gtk_label_set_text (GTK_LABEL (item->label), window_name);
		if (window_name != NULL)
			g_free (window_name);
	}
}

static void
window_menu_window_state_changed (WnckWindow *window,
				  WnckWindowState changed_mask,
				  WnckWindowState new_state,
				  WindowMenu *window_menu)
{
	window_hash_item *item;
	char *window_name;

	if (!(changed_mask & (WNCK_WINDOW_STATE_MINIMIZED|WNCK_WINDOW_STATE_SHADED|WNCK_WINDOW_STATE_SKIP_TASKLIST|WNCK_WINDOW_STATE_DEMANDS_ATTENTION)))
			return;

	item = NULL;
	window_name = NULL;

	item = g_hash_table_lookup (window_menu->window_hash, window);
	if (item == NULL)
		return;

	if (changed_mask & WNCK_WINDOW_STATE_SKIP_TASKLIST) {
		if (wnck_window_is_skip_tasklist (window)) {
			gtk_widget_hide (item->item);
		} else {
			gtk_widget_show (item->item);
		}
	}

	if (changed_mask & (WNCK_WINDOW_STATE_MINIMIZED|WNCK_WINDOW_STATE_SHADED|WNCK_WINDOW_STATE_DEMANDS_ATTENTION)) {
		window_name = window_menu_get_window_name (window);
		gtk_label_set_text (GTK_LABEL (item->label), window_name);
		if (window_name != NULL)
			g_free (window_name);
	}
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
		      const gchar *label,
		      WnckWindow  *window)
{
	GtkWidget *item;
	GtkWidget *ellipsizing_label;
	window_hash_item *hash_item;

	item = gtk_image_menu_item_new ();
	
	ellipsizing_label = gtk_label_new (label);
	gtk_misc_set_alignment (GTK_MISC (ellipsizing_label), 0.0, 0.5);
	gtk_label_set_ellipsize (GTK_LABEL (ellipsizing_label),
				 PANGO_ELLIPSIZE_END);
	/* if window demands attention, we need markup */
	if (window != NULL)
		gtk_label_set_use_markup (GTK_LABEL (ellipsizing_label), TRUE);

	if (window != NULL) {
		hash_item = g_new0 (window_hash_item, 1);
		hash_item->item = item;
		hash_item->label = ellipsizing_label;
		g_hash_table_insert (window_menu->window_hash, window, hash_item);
	}

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
	char          *name;

	if (wnck_window_is_skip_tasklist (window))
		return;

	name = window_menu_get_window_name (window);

	item = window_menu_item_new (window_menu,
				     name,
				     window);

	if (name != NULL)
		g_free (name);

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
	window_hash_item *item;

	if (window == window_menu->icon_window)
		window_menu_set_active_window (window_menu, NULL);

	if (!window_menu->menu || !GTK_WIDGET_VISIBLE (window_menu->menu))
		return;
	
	item = g_hash_table_lookup (window_menu->window_hash, window);
	if (!item)
		return;

	gtk_widget_hide (item->item);
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
	wncklet_connect_while_alive (window, "name_changed",
				     G_CALLBACK (window_menu_window_name_changed),
				     window_menu,
				     window_menu->applet);
	wncklet_connect_while_alive (window, "state_changed",
				     G_CALLBACK (window_menu_window_state_changed),
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
	window_menu->no_windows_item = NULL;
}

static void
window_menu_menu_hidden (GtkWidget  *menu,
			 WindowMenu *window_menu)
{
	gtk_widget_set_state (GTK_WIDGET (window_menu->applet), GTK_STATE_NORMAL);
}

static void
window_menu_on_show (GtkWidget  *widget,
		     WindowMenu *window_menu)
{
	GtkWidget  *separator;
	WnckScreen *screen;
	GList      *windows;
	GList      *l, *children;

	/* Remove existing items */
	children = gtk_container_get_children (GTK_CONTAINER (window_menu->menu));
	for (l = children; l; l = l->next)
		gtk_container_remove (GTK_CONTAINER (window_menu->menu), l->data);
	g_list_free (children);
	
	window_menu->no_windows_item = NULL;

	/* Add separator */
	separator = gtk_separator_menu_item_new ();
	gtk_widget_show (separator);
	gtk_menu_shell_append (GTK_MENU_SHELL (window_menu->menu), separator);


	/* Add windows */
	screen = window_menu_get_screen (window_menu);
	windows = wnck_screen_get_windows (screen);

	if (window_menu->window_hash)
		g_hash_table_destroy (window_menu->window_hash);
	window_menu->window_hash = g_hash_table_new_full (g_direct_hash,
							  g_direct_equal,
							  NULL,
							  g_free);

	for (l = windows; l; l = l->next)
		window_menu_add_window (window_menu, l->data);

	/* Remove separator if it is at the start or the end of the menu */
	l = GTK_MENU_SHELL (window_menu->menu)->children;

	if ((separator == l->data) || separator == g_list_last (l)->data)
		gtk_widget_destroy (separator);

	/* Check if a no-windows item is needed */
	if (!GTK_MENU_SHELL (window_menu->menu)->children) {
		window_menu->no_windows_item =
			window_menu_item_new (window_menu,
					      _("No Windows Open"),
					      NULL);

		gtk_widget_set_sensitive (window_menu->no_windows_item, FALSE);
		gtk_widget_show (window_menu->no_windows_item);	
		gtk_menu_shell_append (GTK_MENU_SHELL (window_menu->menu),
				       window_menu->no_windows_item);
	}
}

static gboolean
window_menu_key_press_event (GtkWidget   *widget,
			     GdkEventKey *event,
			     WindowMenu  *window_menu)
{
	GtkMenuShell *menu_shell;
	
	switch (event->keyval) {
	case GDK_KP_Enter:
	case GDK_ISO_Enter:
	case GDK_3270_Enter:
	case GDK_Return:
	case GDK_space:
	case GDK_KP_Space:
		/* 
		 * We need to call _gtk_menu_shell_activate() here as is done in 
		 * window_key_press_handler in gtkmenubar.c which pops up menu
		 * when F10 is pressed.
		 *
		 * As that function is private its code is replicated here.
		 */
		menu_shell = GTK_MENU_SHELL (window_menu->menu_item->parent);
		if (!menu_shell->active) {
			gtk_grab_add (GTK_WIDGET (menu_shell));
			menu_shell->have_grab = TRUE;
			menu_shell->active = TRUE;
		}
		gtk_menu_shell_select_item (menu_shell, window_menu->menu_item);
		return TRUE;
	default:
		break;
	}
	
	return FALSE;
}

static void
window_menu_change_background (PanelApplet               *applet,
			       PanelAppletBackgroundType  type,
			       GdkColor                  *color,
			       GdkPixmap                 *pixmap,
			       WindowMenu                *window_menu)
{
	GtkRcStyle *rc_style;
	GtkStyle   *style;

	/* reset style */
	gtk_widget_set_style (GTK_WIDGET (window_menu->menu_bar), NULL);
	rc_style = gtk_rc_style_new ();
	gtk_widget_modify_style (GTK_WIDGET (window_menu->menu_bar), rc_style);
	g_object_unref (rc_style);

	switch (type) {
	case PANEL_NO_BACKGROUND:
		break;
	case PANEL_COLOR_BACKGROUND:
		gtk_widget_modify_bg (GTK_WIDGET (window_menu->menu_bar),
				      GTK_STATE_NORMAL, color);
		break;
	case PANEL_PIXMAP_BACKGROUND:
		style = gtk_style_copy (GTK_WIDGET (window_menu->menu_bar)->style);
		if (style->bg_pixmap[GTK_STATE_NORMAL])
			g_object_unref (style->bg_pixmap[GTK_STATE_NORMAL]);
		style->bg_pixmap[GTK_STATE_NORMAL] = g_object_ref (pixmap);
		gtk_widget_set_style (GTK_WIDGET (window_menu->menu_bar), style);
		break;
	}
}

static gboolean
window_menu_on_menu_bar_expose (GtkWidget *widget,
				GdkEventExpose *event,
				gpointer data)
{
	WindowMenu *window_menu = data;

	if (GTK_WIDGET_HAS_FOCUS (window_menu->applet))
		gtk_paint_focus (widget->style,
				 widget->window, 
				 GTK_WIDGET_STATE (window_menu->menu_item),
				 NULL,
				 widget,
				 "menu-applet",
				 0, 0, -1, -1);
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

static void
set_tooltip (GtkWidget  *widget,
	     const char *tip)
{
	GtkTooltips *tooltips;

	tooltips = g_object_get_data (G_OBJECT (widget), "window-menu-tooltips");
	if (!tooltips) {
		tooltips = gtk_tooltips_new ();
		g_object_ref (tooltips);
		gtk_object_sink (GTK_OBJECT (tooltips));
		g_object_set_data_full (G_OBJECT (widget), "window-menu-tooltips", tooltips,
					(GDestroyNotify) g_object_unref);
	}

	gtk_tooltips_set_tip (tooltips, widget, tip, NULL);
}

static inline void
force_no_focus_padding (GtkWidget *widget)
{
        gboolean first_time = TRUE;

        if (first_time) {
                gtk_rc_parse_string ("\n"
                                     "   style \"window-menu-applet-button-style\"\n"
                                     "   {\n"
                                     "      GtkWidget::focus-line-width=0\n"
                                     "      GtkWidget::focus-padding=0\n"
                                     "   }\n"
                                     "\n"
                                     "    widget \"*.window-menu-applet-button\" style \"window-menu-applet-button-style\"\n"
                                     "\n");
                first_time = FALSE;
        }

        gtk_widget_set_name (widget, "window-menu-applet-button");
}

static gboolean
filter_button_press (GtkWidget *widget,
		     GdkEventButton *event,
		     gpointer data)
{
	if (event->button != 1)
		g_signal_stop_emission_by_name (widget, "button_press_event");
	
	return FALSE;
}

gboolean
window_menu_applet_fill (PanelApplet *applet)
{
	WindowMenu *window_menu;
	AtkObject  *atk_obj;

	window_menu = g_new0 (WindowMenu, 1);

	window_menu->applet = GTK_WIDGET (applet);
	force_no_focus_padding (window_menu->applet);

	atk_obj = gtk_widget_get_accessible (window_menu->applet);
	atk_object_set_name (atk_obj, _("Window Selector"));
	atk_object_set_description (atk_obj, _("Tool to switch between windows"));
	set_tooltip (window_menu->applet, _("Window Selector"));
 
	panel_applet_set_flags (applet, PANEL_APPLET_EXPAND_MINOR);

	g_signal_connect (window_menu->applet, "destroy",
			  G_CALLBACK (window_menu_destroy), window_menu);

	panel_applet_setup_menu_from_file (
		applet, NULL, "GNOME_WindowMenuApplet.xml",
		NULL, window_menu_verbs, window_menu);


	window_menu->menu_bar = gtk_menu_bar_new ();
	window_menu->menu_item = gtk_menu_item_new ();

	gtk_menu_shell_append (GTK_MENU_SHELL (window_menu->menu_bar), window_menu->menu_item);
	
	gtk_container_add (GTK_CONTAINER (window_menu->applet), window_menu->menu_bar);

	window_menu->image = gtk_image_new ();
	gtk_widget_show (window_menu->image);

	gtk_container_add (GTK_CONTAINER (window_menu->menu_item), window_menu->image);

	window_menu->size = 1;

	window_menu->menu = gtk_menu_new ();
	g_signal_connect (window_menu->menu, "hide",
			  G_CALLBACK (window_menu_menu_hidden), window_menu);
	g_signal_connect (window_menu->menu, "destroy",
			  G_CALLBACK (window_menu_destroy_menu), window_menu);

	g_signal_connect (window_menu->image, "size_allocate",
			  G_CALLBACK (window_menu_size_allocate), window_menu);

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (window_menu->menu_item), window_menu->menu);
	
	window_menu_setup_menu (window_menu);

	g_signal_connect (window_menu->menu, "show", G_CALLBACK (window_menu_on_show), window_menu);

	g_signal_connect (G_OBJECT (window_menu->applet), "change_background",
			  G_CALLBACK (window_menu_change_background), window_menu);

	g_signal_connect (G_OBJECT (window_menu->menu_bar), "button_press_event",
			  G_CALLBACK (filter_button_press), window_menu);
	g_signal_connect_after (G_OBJECT (window_menu->applet), "focus-in-event",
				G_CALLBACK (gtk_widget_queue_draw), window_menu);
	g_signal_connect_after (G_OBJECT (window_menu->applet), "focus-out-event",
				G_CALLBACK (gtk_widget_queue_draw), window_menu);
	g_signal_connect_after (G_OBJECT (window_menu->menu_bar), "expose-event",
				G_CALLBACK (window_menu_on_menu_bar_expose), window_menu);

	g_signal_connect (window_menu->applet, "key_press_event",
			  G_CALLBACK (window_menu_key_press_event), window_menu);
	
	gtk_widget_set_name (window_menu->menu_bar, "gnome-panel-window-menu-menu-bar-style");
	
	gtk_rc_parse_string (
		"style \"gnome-panel-window-menu-menu-bar-style\" {\n"
		"        GtkMenuBar::shadow-type = none\n"
		"        GtkMenuBar::internal-padding = 0\n"
		"}\n"
		"widget \"*gnome-panel-window-menu-menu-bar*\" style : highest \"gnome-panel-window-menu-menu-bar-style\"");

    	gtk_widget_show_all (GTK_WIDGET (window_menu->applet));

	return TRUE;
}
