/*
 * panel-menu-button.c: panel menu button
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-menu-button.h"

#include <string.h>
#include <libgnome/gnome-i18n.h>

#include "applet.h"
#include "panel-widget.h"
#include "panel-stock-icons.h"
#include "panel-util.h"
#include "panel-profile.h"
#include "panel-globals.h"
#include "menu-util.h"
#include "menu.h"
#include "quick-desktop-reader.h"

enum {
	PROP_0,
	PROP_MENU_PATH,
	PROP_CUSTOM_ICON,
	PROP_USE_MENU_PATH,
	PROP_USE_CUSTOM_ICON,
	PROP_DND_ENABLED
};

struct _PanelMenuButtonPrivate {
	AppletInfo            *info;
	PanelToplevel         *toplevel;
	guint                  gconf_notify;

	GtkMenu               *menu;

	char                  *menu_path;
	char                  *custom_icon;

	guint                  use_menu_path : 1;
	guint                  use_custom_icon : 1;
	guint                  dnd_enabled : 1;
};

static GObjectClass *parent_class;

static void
panel_menu_button_instance_init (PanelMenuButton      *button,
				 PanelMenuButtonClass *klass)
{
	button->priv = g_new0 (PanelMenuButtonPrivate, 1);

	button->priv->info         = NULL;
	button->priv->toplevel     = NULL;
	button->priv->gconf_notify = 0;

	button->priv->menu_path   = NULL;
	button->priv->custom_icon = NULL;

	button->priv->use_menu_path   = FALSE;
	button->priv->use_custom_icon = FALSE;

	button_widget_set_stock_id (BUTTON_WIDGET (button), PANEL_STOCK_MAIN_MENU);

	/* FIXME: set a tooltip */
}

static void
panel_menu_button_finalize (GObject *object)
{
	PanelMenuButton *button = PANEL_MENU_BUTTON (object);

	button->priv->info = NULL;

	gconf_client_notify_remove (panel_gconf_get_client (), button->priv->gconf_notify);
	button->priv->gconf_notify = 0;

	if (button->priv->menu) {
		/* detaching the menu will kill our reference */
		gtk_menu_detach (button->priv->menu);
		button->priv->menu = NULL;
	}

	g_free (button->priv->menu_path);
	button->priv->menu_path = NULL;

	g_free (button->priv->custom_icon);
	button->priv->custom_icon = NULL;

	g_free (button->priv);
	button->priv = NULL;

	parent_class->finalize (object);
}

static void
panel_menu_button_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
	PanelMenuButton *button;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (object));

	button = PANEL_MENU_BUTTON (object);

	switch (prop_id) {
	case PROP_MENU_PATH:
		g_value_set_string (value, button->priv->menu_path);
		break;
	case PROP_CUSTOM_ICON:
		g_value_set_string (value, button->priv->custom_icon);
		break;
	case PROP_USE_MENU_PATH:
		g_value_set_boolean (value, button->priv->use_menu_path);
		break;
	case PROP_USE_CUSTOM_ICON:
		g_value_set_boolean (value, button->priv->use_custom_icon);
		break;
	case PROP_DND_ENABLED:
		g_value_set_boolean (value, button->priv->dnd_enabled);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_menu_button_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	PanelMenuButton *button;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (object));

	button = PANEL_MENU_BUTTON (object);

	switch (prop_id) {
	case PROP_MENU_PATH:
                panel_menu_button_set_menu_path (button, g_value_get_string (value));
                break;
	case PROP_CUSTOM_ICON:
                panel_menu_button_set_custom_icon (button, g_value_get_string (value));
                break;
	case PROP_USE_MENU_PATH:
		panel_menu_button_set_use_menu_path (button, g_value_get_boolean (value));
		break;
	case PROP_USE_CUSTOM_ICON:
		panel_menu_button_set_use_custom_icon (button, g_value_get_boolean (value));
		break;
	case PROP_DND_ENABLED:
		panel_menu_button_set_dnd_enabled (button, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_menu_button_associate_panel (PanelMenuButton *button)
{
	PanelWidget *panel_widget = NULL;

	if (!button->priv->menu)
		return;

	if (button->priv->toplevel)
		panel_widget = panel_toplevel_get_panel_widget (button->priv->toplevel);

	panel_applet_menu_set_recurse (button->priv->menu, "menu_panel", panel_widget);
}

static void
panel_menu_button_parent_set (GtkWidget *widget,
			      GtkWidget *previous_parent)
{
	PanelMenuButton *button = PANEL_MENU_BUTTON (widget);

	g_return_if_fail (!widget->parent || PANEL_IS_WIDGET (widget->parent));

	if (widget->parent)
		button->priv->toplevel = PANEL_WIDGET (widget->parent)->toplevel;
	else
		button->priv->toplevel = NULL;

	panel_menu_button_associate_panel (button);

	if (GTK_WIDGET_CLASS (parent_class)->parent_set)
		GTK_WIDGET_CLASS (parent_class)->parent_set (widget, previous_parent);
}

static void
panel_menu_button_drag_data_get (GtkWidget        *widget,
				 GdkDragContext   *context,
				 GtkSelectionData *selection_data,
				 guint             info,
				 guint             time)
{
	PanelMenuButton *button;
	char            *drag_data;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (widget));

	button = PANEL_MENU_BUTTON (widget);

	drag_data = g_strdup_printf ("MENU:%d", panel_find_applet_index (widget));

	gtk_selection_data_set (
		selection_data, selection_data->target,
		8, (guchar *) drag_data, strlen (drag_data));

	g_free (drag_data);
}

static void
panel_menu_button_menu_deactivated (PanelMenuButton *button)
{
	panel_toplevel_unblock_auto_hide (button->priv->toplevel);

	GTK_BUTTON (button)->in_button = FALSE;
	BUTTON_WIDGET (button)->ignore_leave = FALSE;
	gtk_button_released (GTK_BUTTON (button));
}

static GtkMenu *
panel_menu_button_create_menu (PanelMenuButton *button)
{
	PanelWidget *panel_widget;
	GtkWidget   *menu = NULL;

	panel_widget = panel_toplevel_get_panel_widget (button->priv->toplevel);

	if (button->priv->use_menu_path && button->priv->menu_path)
		menu = create_menu_at (NULL,
				       button->priv->menu_path,
				       FALSE,
				       NULL,
				       TRUE,
				       FALSE);

	if (!menu)
		menu = create_panel_root_menu (panel_widget);

	return GTK_MENU (menu);
}

static void 
panel_menu_button_menu_detacher	(GtkWidget *widget,
				 GtkMenu   *menu)
{
 	PanelMenuButton *button;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (widget));

	button = PANEL_MENU_BUTTON (widget);

	g_return_if_fail (button->priv->menu == menu);

	/* just in case someone still owns a reference to the
	   menu (the menu may be up or some such other nonsense) */
	g_signal_handlers_disconnect_by_func (button->priv->menu,
					      G_CALLBACK (panel_menu_button_menu_deactivated),
					      button);

	/* This is a workaround pending fixing bug #113112 */
	g_object_set_data (G_OBJECT (button), "gtk-attached-menu", NULL);
	button->priv->menu = NULL;
}

void
panel_menu_button_popup_menu (PanelMenuButton *button,
			      guint            n_button,
			      guint32          activate_time)
{
	GdkScreen *screen;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));

	if (button->priv->menu && menu_need_reread (GTK_WIDGET (button->priv->menu))) {
		gtk_menu_detach (button->priv->menu);
		button->priv->menu = NULL;
	}

	if (!button->priv->menu) {
		button->priv->menu = panel_menu_button_create_menu (button);

		gtk_menu_attach_to_widget (button->priv->menu, 
					   GTK_WIDGET (button),
					   panel_menu_button_menu_detacher);
		/* This is a workaround pending fixing bug #113112 */
		g_object_set_data (G_OBJECT (button), "gtk-attached-menu", button->priv->menu);

		panel_menu_button_associate_panel (button);

		g_signal_connect_swapped (button->priv->menu, "deactivate",
					  G_CALLBACK (panel_menu_button_menu_deactivated),
					  button);
	}

	panel_toplevel_block_auto_hide (button->priv->toplevel);

	BUTTON_WIDGET (button)->ignore_leave = TRUE;

	screen = gtk_window_get_screen (GTK_WINDOW (button->priv->toplevel));
	gtk_menu_set_screen (button->priv->menu, screen);

	gtk_menu_popup (button->priv->menu,
			NULL,
			NULL,
			(GtkMenuPositionFunc) panel_position_applet_menu,
			GTK_WIDGET (button),
			n_button,
			activate_time);
}

static void
panel_menu_button_handle_event (PanelMenuButton *button)
{
	GdkEvent *event;
	guint     n_button;
	guint32   activate_time;

	event = gtk_get_current_event ();

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		activate_time = event->button.time;
		n_button = event->button.button;
		break;
	case GDK_KEY_PRESS:
	case GDK_KEY_RELEASE:
		activate_time = event->key.time;
		n_button = 1;
		break;
	default:
		activate_time = n_button = 0;
		g_assert_not_reached ();
		break;
	}
	
	gtk_grab_remove (GTK_WIDGET (button));

	panel_menu_button_popup_menu (button, n_button, activate_time);

	gdk_event_free (event);
}

static void
panel_menu_button_pressed (GtkButton *gtk_button)
{
	PanelMenuButton *button;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (gtk_button));

	button = PANEL_MENU_BUTTON (gtk_button);

	if (GTK_BUTTON_CLASS (parent_class)->pressed)
		GTK_BUTTON_CLASS (parent_class)->pressed (gtk_button);

	panel_menu_button_handle_event (button);
}

static void
panel_menu_button_clicked (GtkButton *gtk_button)
{
	PanelMenuButton *button;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (gtk_button));

	button = PANEL_MENU_BUTTON (gtk_button);

	if (GTK_BUTTON_CLASS (parent_class)->clicked)
		GTK_BUTTON_CLASS (parent_class)->clicked (gtk_button);

	panel_menu_button_handle_event (button);
}

static void
panel_menu_button_class_init (PanelMenuButtonClass *klass,
			   gpointer           dummy)
{
	GObjectClass   *gobject_class = (GObjectClass   *) klass;
	GtkWidgetClass *widget_class  = (GtkWidgetClass *) klass;
	GtkButtonClass *button_class  = (GtkButtonClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize     = panel_menu_button_finalize;
	gobject_class->get_property = panel_menu_button_get_property;
	gobject_class->set_property = panel_menu_button_set_property;

	widget_class->parent_set    = panel_menu_button_parent_set;
        widget_class->drag_data_get = panel_menu_button_drag_data_get;
                                                                                                             
	button_class->clicked = panel_menu_button_clicked;
	button_class->pressed = panel_menu_button_pressed;

	g_object_class_install_property (
			gobject_class,
			PROP_MENU_PATH,
                        g_param_spec_string ("menu-path",
					     _("Menu Path"),
					     _("The path from which to construct the menu"),
					     NULL,
					     G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_CUSTOM_ICON,
                        g_param_spec_string ("custom-icon",
					     _("Custom Icon"),
					     _("The custom icon for the menu"),
					     NULL,
					     G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_USE_MENU_PATH,
                        g_param_spec_boolean ("use-menu-path",
					      _("Use Menu Path"),
					     _("Use the path specified by the menu-path property"),
					     FALSE,
					     G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_USE_CUSTOM_ICON,
                        g_param_spec_boolean ("use-custom-icon",
					      _("Use Custom Icon"),
					      _("Use the icon specified by the custom-icon property"),
					      FALSE,
					      G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_DND_ENABLED,
                        g_param_spec_boolean ("dnd-enabled",
					      _("Drag N' Drop enabled"),
					      _("Whether or not drag and drop is enabled on the widget"),
					      FALSE,
					      G_PARAM_READWRITE));
}

GType
panel_menu_button_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelMenuButtonClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_menu_button_class_init,
			NULL,
			NULL,
			sizeof (PanelMenuButton),
			0,
			(GInstanceInitFunc) panel_menu_button_instance_init,
			NULL
		};

		type = g_type_register_static (
				BUTTON_TYPE_WIDGET, "PanelMenuButton", &info, 0);
	}

	return type;
}

static void
panel_menu_button_gconf_notify (GConfClient     *client,
				guint            cnxn_id,
				GConfEntry      *entry,
				PanelMenuButton *button)
{
	GConfValue *value;
	const char *key;

	key = panel_gconf_basename (gconf_entry_get_key (entry));

	value = entry->value;

	if (!strcmp (key, "menu_path")) {
		if (value && value->type == GCONF_VALUE_STRING)
			panel_menu_button_set_menu_path (button,
							 gconf_value_get_string (value));
	} else if (!strcmp (key, "custom_icon")) {
		if (value && value->type == GCONF_VALUE_STRING)
			panel_menu_button_set_custom_icon (button,
							   gconf_value_get_string (value));
	} else if (!strcmp (key, "use_menu_path")) {
		if (value && value->type == GCONF_VALUE_BOOL)
			panel_menu_button_set_use_menu_path (button,
							     gconf_value_get_bool (value));
	} else if (!strcmp (key, "use_custom_icon")) {
		if (value && value->type == GCONF_VALUE_BOOL)
			panel_menu_button_set_use_custom_icon (button,
							       gconf_value_get_bool (value));
	}
}

static void
panel_menu_button_connect_to_gconf (PanelMenuButton *button)
{
	GConfClient *client;
	const char  *key;
	const char  *profile;

	client  = panel_gconf_get_client ();
	profile = panel_profile_get_name ();

	key = panel_gconf_sprintf (PANEL_CONFIG_DIR "/%s/objects/%s",
				   profile,
				   button->priv->info->id);
	gconf_client_add_dir (client, key, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	button->priv->gconf_notify =
		gconf_client_notify_add (client, key,
					 (GConfClientNotifyFunc) panel_menu_button_gconf_notify,
					 button, NULL, NULL);
}

static void
panel_menu_button_load (const char  *menu_path,
			gboolean     use_menu_path,
			const char  *custom_icon,
			gboolean     use_custom_icon,
			PanelWidget *panel,
			gboolean     locked,
			int          position,
			gboolean     exactpos,
			const char  *id)
{
	PanelMenuButton *button;

	g_return_if_fail (panel != NULL);

	button = g_object_new (PANEL_TYPE_MENU_BUTTON,
			       "menu-path", menu_path,
			       "custom-icon", custom_icon,
			       "use-menu-path", use_menu_path,
			       "use-custom-icon", use_custom_icon,
			       NULL);

	button->priv->info = panel_applet_register (
					GTK_WIDGET (button), NULL, NULL,
					panel, locked, position, exactpos,
					PANEL_OBJECT_MENU, id);
	if (!button->priv->info) {
		gtk_widget_destroy (GTK_WIDGET (button));
		return;
	}

	panel_applet_add_callback (
		button->priv->info, "help", GTK_STOCK_HELP, _("_Help"));

	panel_widget_set_applet_expandable (panel, GTK_WIDGET (button), FALSE, TRUE);
	panel_widget_set_applet_size_constrained (panel, GTK_WIDGET (button), TRUE);

	panel_menu_button_connect_to_gconf (button);
}

static char *
panel_menu_button_get_icon_for_menu_path (PanelMenuButton *button)
{
	QuickDesktopItem *item;
        char             *retval;
	char             *dentry_path;

	if (!button->priv->use_menu_path || !button->priv->menu_path)
		return NULL;
                                                                                                             
	dentry_path = g_build_path (G_DIR_SEPARATOR_S,
				    button->priv->menu_path,
				    ".directory",
				    NULL);
	item = quick_desktop_item_load_uri (dentry_path, NULL, FALSE);
	g_free (dentry_path);

	if (!item)
		return NULL;
                                                                                                             
	retval = gnome_desktop_item_find_icon (panel_icon_theme, item->icon, 20, 0);
                                                                                                             
	quick_desktop_item_destroy (item);

	return retval;
}

static void
panel_menu_button_set_icon (PanelMenuButton *button)
{
	char *icon_path = NULL;

	if (button->priv->use_custom_icon)
		icon_path = button->priv->custom_icon;

	if (!icon_path)
		icon_path = panel_menu_button_get_icon_for_menu_path (button);

	if (icon_path)
		button_widget_set_icon_name (BUTTON_WIDGET (button), icon_path);
	else
		button_widget_set_stock_id (BUTTON_WIDGET (button), PANEL_STOCK_MAIN_MENU);
}
                                                                                                             
void
panel_menu_button_set_menu_path (PanelMenuButton *button,
				 const char      *menu_path)
{
	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));

	if (!button->priv->menu_path && (!menu_path || !menu_path [0]))
		return;

	if (button->priv->menu_path && menu_path &&
	    !strcmp (button->priv->menu_path, menu_path))
		return;

	g_free (button->priv->menu_path);
	button->priv->menu_path = NULL;

	if (menu_path && menu_path [0])
		button->priv->menu_path = g_strdup (menu_path);

	if (button->priv->menu)
		gtk_menu_detach (button->priv->menu);
	button->priv->menu = NULL;

	panel_menu_button_set_icon (button);
}

void
panel_menu_button_set_custom_icon (PanelMenuButton *button,
				   const char      *custom_icon)
{
	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));

	g_free (button->priv->custom_icon);
	button->priv->custom_icon = NULL;

	if (custom_icon && custom_icon [0])
		button->priv->custom_icon = g_strdup (custom_icon);

	panel_menu_button_set_icon (button);
}

void
panel_menu_button_set_use_menu_path (PanelMenuButton *button,
				     gboolean         use_menu_path)
{
	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));

	use_menu_path = use_menu_path != FALSE;

	if (button->priv->use_menu_path == use_menu_path)
		return;

	button->priv->use_menu_path = use_menu_path;

	if (button->priv->menu)
		gtk_menu_detach (button->priv->menu);
	button->priv->menu = NULL;

	panel_menu_button_set_icon (button);
}

gboolean
panel_menu_button_get_use_menu_path (PanelMenuButton *button)
{
	g_return_val_if_fail (PANEL_IS_MENU_BUTTON (button), FALSE);

	return button->priv->use_menu_path;
}

void
panel_menu_button_set_use_custom_icon (PanelMenuButton *button,
				       gboolean         use_custom_icon)
{
	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));

	button->priv->use_custom_icon = use_custom_icon != FALSE;

	panel_menu_button_set_icon (button);
}

void
panel_menu_button_load_from_gconf (PanelWidget *panel,
				   gboolean     locked,
				   int          position,
				   gboolean     exactpos,
				   const char  *id)
{
	GConfClient *client;
	const char  *profile;
	const char  *key;
	char        *menu_path;
	char        *custom_icon;
	gboolean     use_menu_path;
	gboolean     use_custom_icon;

	client  = panel_gconf_get_client ();
	profile = panel_profile_get_name ();

	key = panel_gconf_sprintf (PANEL_CONFIG_DIR "/%s/objects/%s", profile, id);
	gconf_client_add_dir (client, key, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "menu_path");
	menu_path = gconf_client_get_string (client, key, NULL);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "custom_icon");
	custom_icon = gconf_client_get_string (client, key, NULL);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "use_menu_path");
	use_menu_path = gconf_client_get_bool (client, key, NULL);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "use_custom_icon");
	use_custom_icon = gconf_client_get_bool (client, key, NULL);

	panel_menu_button_load (menu_path,
				use_menu_path,
				custom_icon,
				use_custom_icon,
				panel,
				locked,
				position,
				exactpos,
				id);

	g_free (menu_path);
	g_free (custom_icon);
}

void
panel_menu_button_create (PanelToplevel *toplevel,
			  int            position,
			  const char    *menu_path,
			  gboolean       use_menu_path)
{
	GConfClient *client;
	const char  *profile;
	const char  *key;
	char        *id;

	client  = panel_gconf_get_client ();
	profile = panel_profile_get_name ();

	id = panel_profile_prepare_object (PANEL_OBJECT_MENU, toplevel, position, FALSE);

	key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "use_menu_path");
	gconf_client_set_bool (client, key, use_menu_path, NULL);

	if (use_menu_path && menu_path && menu_path [0]) {
		key = panel_gconf_full_key (PANEL_GCONF_OBJECTS, profile, id, "menu_path");
		gconf_client_set_string (client, key, menu_path, NULL);
	}

	/* frees id */
	panel_profile_add_to_list (PANEL_GCONF_OBJECTS, id);
}
			  
void
panel_menu_button_invoke_menu (PanelMenuButton *button,
			       const char   *callback_name)
{
	GdkScreen *screen;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));
	g_return_if_fail (callback_name != NULL);

	if (strcmp (callback_name, "help"))
		return;

	screen = gtk_widget_get_screen (GTK_WIDGET (button));

	panel_show_help (screen, "wgospanel.xml", "gospanel-37");
}

void
panel_menu_button_set_dnd_enabled (PanelMenuButton *button,
				   gboolean         dnd_enabled)
{
	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));

	dnd_enabled = dnd_enabled != FALSE;

	if (button->priv->dnd_enabled == dnd_enabled)
		return;

	if (dnd_enabled) {
		static GtkTargetEntry dnd_targets [] = {
			{ "application/x-panel-applet-internal", 0, 0 }
		};

		GTK_WIDGET_UNSET_FLAGS (button, GTK_NO_WINDOW);
		gtk_drag_source_set (GTK_WIDGET (button), GDK_BUTTON1_MASK,
				     dnd_targets, 1,
				     GDK_ACTION_COPY | GDK_ACTION_MOVE);
		gtk_drag_source_set_icon_stock (GTK_WIDGET (button),
						PANEL_STOCK_MAIN_MENU);
		GTK_WIDGET_SET_FLAGS (button, GTK_NO_WINDOW);
	} else
		gtk_drag_source_unset (GTK_WIDGET (button));
}
