/*
 * Copyright (C) 2016-2020 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GP_APPLET_H
#define GP_APPLET_H

#include <gtk/gtk.h>
#include <libgnome-panel/gp-lockdown.h>

G_BEGIN_DECLS

/**
 * GP_TYPE_APPLET:
 *
 * The type for GpApplet.
 */
#define GP_TYPE_APPLET gp_applet_get_type ()
G_DECLARE_DERIVABLE_TYPE (GpApplet, gp_applet, GP, APPLET, GtkEventBox)

/**
 * GpAppletFlags:
 * @GP_APPLET_FLAGS_NONE: No flags set.
 * @GP_APPLET_FLAGS_EXPAND_MAJOR: On horizontal panels, the #GpApplet will
 *     be allocated as much width as possible. On vertical panels, it will
 *     be allocated as much height as possible.
 * @GP_APPLET_FLAGS_EXPAND_MINOR: On horizontal panels, the #GpApplet will
 *     be allocated the full height of the panel. On vertical panels, it
 *     will be allocated the full width. Most applets should set this flag
 *     in order to use full panel size and allow the applet to be Fitt's Law
 *     compliant.
 * @GP_APPLET_FLAGS_HAS_HANDLE: The panel will draw a handle for the
 *     #GpApplet. This handle will be usable to move the applet, and to open
 *     the context menu of the applet.
 *
 * Flags to be used with gp_applet_set_flags(), to indicate to the panel a
 * specific behavior requested by the #GpApplet.
 */
typedef enum
{
  GP_APPLET_FLAGS_NONE = 0,
  GP_APPLET_FLAGS_EXPAND_MAJOR = 1 << 0,
  GP_APPLET_FLAGS_EXPAND_MINOR = 1 << 1,
  GP_APPLET_FLAGS_HAS_HANDLE = 1 << 2
} GpAppletFlags;

/**
 * GpAppletClass:
 * @parent_class: The parent class.
 * @initial_setup: virtual method called when applet has initial settings
 * @placement_changed: Signal is emitted when the orientation or position
 *     properties of applet has changed.
 * @remove_from_panel: virtual method called when applet is removed from
 *     panel.
 *
 * The class structure for the #GpApplet class.
 */
struct _GpAppletClass
{
  GtkEventBoxClass parent_class;

  void (* initial_setup)       (GpApplet        *applet,
                                GVariant        *initial_settings);

  void (* placement_changed)   (GpApplet        *applet,
                                GtkOrientation   orientation,
                                GtkPositionType  position);

  void (* remove_from_panel)   (GpApplet        *self);

  /*< private >*/
  gpointer padding[9];
};

gboolean         gp_applet_get_locked_down           (GpApplet           *applet);

GpLockdownFlags  gp_applet_get_lockdowns             (GpApplet           *applet);

GtkOrientation   gp_applet_get_orientation           (GpApplet           *applet);

GtkPositionType  gp_applet_get_position              (GpApplet           *applet);

void             gp_applet_set_flags                 (GpApplet           *applet,
                                                      GpAppletFlags       flags);

void             gp_applet_set_size_hints            (GpApplet           *applet,
                                                      const gint         *size_hints,
                                                      guint               n_elements,
                                                      gint                base_size);

GSettings       *gp_applet_settings_new              (GpApplet           *applet,
                                                      const gchar        *schema);

void             gp_applet_request_focus             (GpApplet           *applet,
                                                      guint32             timestamp);

void             gp_applet_setup_menu                (GpApplet           *applet,
                                                      const gchar        *xml,
                                                      const GActionEntry *entries);

void             gp_applet_setup_menu_from_file      (GpApplet           *applet,
                                                      const gchar        *filename,
                                                      const GActionEntry *entries);

void             gp_applet_setup_menu_from_resource  (GpApplet           *applet,
                                                      const gchar        *resource_path,
                                                      const GActionEntry *entries);

GAction         *gp_applet_menu_lookup_action        (GpApplet           *applet,
                                                      const gchar        *action_name);

gboolean         gp_applet_get_prefer_symbolic_icons (GpApplet           *applet);

guint            gp_applet_get_panel_icon_size       (GpApplet           *applet);

guint            gp_applet_get_menu_icon_size        (GpApplet           *applet);

void             gp_applet_show_about                (GpApplet           *applet);

void             gp_applet_show_help                 (GpApplet           *applet,
                                                      const char         *page);

void             gp_applet_popup_menu_at_widget      (GpApplet           *applet,
                                                      GtkMenu            *menu,
                                                      GtkWidget          *widget,
                                                      GdkEvent           *event);

G_END_DECLS

#endif
