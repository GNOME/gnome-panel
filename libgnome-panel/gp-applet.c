/*
 * Copyright (C) 2001 Sun Microsystems, Inc.
 * Copyright (c) 2010 Carlos Garcia Campos
 * Copyright (C) 2016-2021 Alberts Muktupāvels
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     Carlos Garcia Campos <carlosgc@gnome.org>
 *     Mark McLoughlin <mark@skynet.ie>
 */

/**
 * SECTION: gp-applet
 * @title: GpApplet
 * @short_description: a widget embedded in a panel
 * @include: libngome-panel/gp-applet.h
 *
 * Applets are small applications that are embedded in the GNOME Panel. They
 * can be used to give quick access to some features, or to display the state
 * of something specific.
 *
 * The #GpApplet API hides all of the embedding process as it handles all
 * the communication with the GNOME Panel. It is a subclass of #GtkEventBox,
 * so you can add any kind of widgets to it.
 */

/**
 * GpApplet:
 *
 * #GpApplet is an opaque data structure and can only be accessed using
 * the following functions.
 */

#include "config.h"
#include "gp-applet-private.h"

#include "gp-enum-types.h"
#include "gp-module-private.h"

typedef struct
 {
  gint  *size_hints;
  guint  n_elements;
} GpSizeHints;

typedef struct
{
  GtkBuilder         *builder;
  GSimpleActionGroup *action_group;

  GpModule           *module;

  gchar              *id;
  gchar              *settings_path;
  GVariant           *initial_settings;
  gchar              *gettext_domain;
  gboolean            locked_down;
  GpLockdownFlags     lockdowns;
  GtkOrientation      orientation;
  GtkPositionType     position;

  GpAppletFlags       flags;
  GpSizeHints        *size_hints;

  guint               size_hints_idle;

  GSettings          *general_settings;

  gboolean            enable_tooltips;

  gboolean            prefer_symbolic_icons;

  guint               panel_icon_size;
  guint               menu_icon_size;

  guint               icon_resize_id;

  GtkWidget          *about_dialog;
} GpAppletPrivate;

enum
{
  PROP_0,

  PROP_MODULE,

  PROP_ID,
  PROP_SETTINGS_PATH,
  PROP_INITIAL_SETTINGS,
  PROP_GETTEXT_DOMAIN,
  PROP_LOCKED_DOWN,
  PROP_LOCKDOWNS,
  PROP_ORIENTATION,
  PROP_POSITION,

  PROP_ENABLE_TOOLTIPS,

  PROP_PREFER_SYMBOLIC_ICONS,

  PROP_PANEL_ICON_SIZE,
  PROP_MENU_ICON_SIZE,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

enum
{
  PLACEMENT_CHANGED,

  FLAGS_CHANGED,
  SIZE_HINTS_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GpApplet, gp_applet, GTK_TYPE_EVENT_BOX)

static void
update_enable_tooltips (GpApplet *applet)
{
  GpAppletPrivate *priv;
  gboolean enable_tooltips;

  priv = gp_applet_get_instance_private (applet);
  enable_tooltips = g_settings_get_boolean (priv->general_settings,
                                            "enable-tooltips");

  gp_applet_set_enable_tooltips (applet, enable_tooltips);
}

static void
update_prefer_symbolic_icons (GpApplet *applet)
{
  GpAppletPrivate *priv;
  gboolean prefer_symbolic_icons;

  priv = gp_applet_get_instance_private (applet);
  prefer_symbolic_icons = g_settings_get_boolean (priv->general_settings,
                                                  "prefer-symbolic-icons");

  gp_applet_set_prefer_symbolic_icons (applet, prefer_symbolic_icons);
}

static void
update_menu_icon_size (GpApplet *applet)
{
  GpAppletPrivate *priv;
  guint menu_icon_size;

  priv = gp_applet_get_instance_private (applet);
  menu_icon_size = g_settings_get_enum (priv->general_settings,
                                        "menu-icon-size");

  gp_applet_set_menu_icon_size (applet, menu_icon_size);
}

static void
update_panel_icon_size (GpApplet *applet)
{
  GpAppletPrivate *priv;
  guint panel_max_icon_size;
  GtkAllocation allocation;
  guint panel_size;
  guint spacing;
  guint panel_icon_size;

  priv = gp_applet_get_instance_private (applet);
  panel_max_icon_size = g_settings_get_enum (priv->general_settings,
                                             "panel-max-icon-size");

  gtk_widget_get_allocation (GTK_WIDGET (applet), &allocation);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    panel_size = allocation.height;
  else if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    panel_size = allocation.width;
  else
    g_assert_not_reached ();

  spacing = 4;

  if (panel_size <= panel_max_icon_size + spacing)
    {
      if (panel_size < 22 + spacing)
        panel_icon_size = 16;
      else if (panel_size < 24 + spacing)
        panel_icon_size = 22;
      else if (panel_size < 32 + spacing)
        panel_icon_size = 24;
      else if (panel_size < 48 + spacing)
        panel_icon_size = 32;
      else if (panel_size < 64 + spacing)
        panel_icon_size = 48;
      else
        panel_icon_size = 64;
    }
  else
    {
      panel_icon_size = panel_max_icon_size;
    }

  gp_applet_set_panel_icon_size (applet, panel_icon_size);
}

static void
general_settings_changed_cb (GSettings   *settings,
                             const gchar *key,
                             GpApplet    *applet)
{
  if (key == NULL || g_strcmp0 (key, "enable-tooltips") == 0)
    update_enable_tooltips (applet);

  if (key == NULL || g_strcmp0 (key, "prefer-symbolic-icons") == 0)
    update_prefer_symbolic_icons (applet);

  if (key == NULL || g_strcmp0 (key, "menu-icon-size") == 0)
    update_menu_icon_size (applet);

  if (key == NULL || g_strcmp0 (key, "panel-max-icon-size") == 0)
    update_panel_icon_size (applet);
}

static gboolean
icon_resize_cb (gpointer user_data)
{
  GpApplet *self;
  GpAppletPrivate *priv;

  self = GP_APPLET (user_data);
  priv = gp_applet_get_instance_private (self);

  update_panel_icon_size (self);
  priv->icon_resize_id = 0;

  return G_SOURCE_REMOVE;
}

static gboolean
emit_size_hints_changed_cb (gpointer user_data)
{
  GpApplet *applet;
  GpAppletPrivate *priv;

  applet = GP_APPLET (user_data);
  priv = gp_applet_get_instance_private (applet);

  priv->size_hints_idle = 0;
  g_signal_emit (applet, signals[SIZE_HINTS_CHANGED], 0);

  return G_SOURCE_REMOVE;
}

static void
emit_size_hints_changed (GpApplet *applet)
{
  GpAppletPrivate *priv;
  const gchar *name;

  priv = gp_applet_get_instance_private (applet);
  if (priv->size_hints_idle != 0)
    return;

  priv->size_hints_idle = g_idle_add (emit_size_hints_changed_cb, applet);

  name = "[libgnome-panel] emit_size_hints_changed_cb";
  g_source_set_name_by_id (priv->size_hints_idle, name);
}

static void
gp_size_hints_free (gpointer data)
{
  GpSizeHints *size_hints;

  size_hints = (GpSizeHints *) data;

  g_free (size_hints->size_hints);
  g_free (size_hints);
}

static gboolean
size_hints_changed (GpAppletPrivate *priv,
                    const gint      *size_hints,
                    guint            n_elements,
                    gint             base_size)
{
  guint i;

  if (priv->size_hints == NULL && size_hints == NULL)
    return FALSE;

  if (priv->size_hints == NULL || size_hints == NULL)
    return TRUE;

  if (priv->size_hints->n_elements != n_elements)
    return TRUE;

  for (i = 0; i < n_elements; i++)
    {
      if (priv->size_hints->size_hints[i] != size_hints[i] + base_size)
        return TRUE;
    }

  return FALSE;
}

static void
gp_applet_constructed (GObject *object)
{
  GpApplet *applet;
  GpAppletClass *applet_class;
  GpAppletPrivate *priv;
  GActionGroup *group;
  GtkStyleContext *context;

  G_OBJECT_CLASS (gp_applet_parent_class)->constructed (object);

  applet = GP_APPLET (object);
  applet_class = GP_APPLET_GET_CLASS (applet);
  priv = gp_applet_get_instance_private (applet);

  if (applet_class->initial_setup != NULL && priv->initial_settings != NULL)
    applet_class->initial_setup (applet, priv->initial_settings);
  g_clear_pointer (&priv->initial_settings, g_variant_unref);

  gtk_builder_set_translation_domain (priv->builder, priv->gettext_domain);

  group = G_ACTION_GROUP (priv->action_group);
  gtk_widget_insert_action_group (GTK_WIDGET (applet), priv->id, group);

  context = gtk_widget_get_style_context (GTK_WIDGET (applet));
  gtk_style_context_add_class (context, priv->id);
}

static void
gp_applet_dispose (GObject *object)
{
  GpApplet *applet;
  GpAppletPrivate *priv;

  applet = GP_APPLET (object);
  priv = gp_applet_get_instance_private (applet);

  g_clear_object (&priv->builder);
  g_clear_object (&priv->action_group);

  g_clear_object (&priv->module);

  if (priv->size_hints_idle != 0)
    {
      g_source_remove (priv->size_hints_idle);
      priv->size_hints_idle = 0;
    }

  if (priv->icon_resize_id != 0)
    {
      g_source_remove (priv->icon_resize_id);
      priv->icon_resize_id = 0;
    }

  g_clear_pointer (&priv->initial_settings, g_variant_unref);
  g_clear_object (&priv->general_settings);

  g_clear_pointer (&priv->about_dialog, gtk_widget_destroy);

  G_OBJECT_CLASS (gp_applet_parent_class)->dispose (object);
}

static void
gp_applet_finalize (GObject *object)
{
  GpApplet *applet;
  GpAppletPrivate *priv;

  applet = GP_APPLET (object);
  priv = gp_applet_get_instance_private (applet);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->settings_path, g_free);
  g_clear_pointer (&priv->gettext_domain, g_free);
  g_clear_pointer (&priv->size_hints, gp_size_hints_free);

  G_OBJECT_CLASS (gp_applet_parent_class)->finalize (object);
}

static void
gp_applet_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GpApplet *applet;
  GpAppletPrivate *priv;

  applet = GP_APPLET (object);
  priv = gp_applet_get_instance_private (applet);

  switch (property_id)
    {
      case PROP_MODULE:
        break;

      case PROP_ID:
        g_value_set_string (value, priv->id);
        break;

      case PROP_SETTINGS_PATH:
        g_value_set_string (value, priv->settings_path);
        break;

      case PROP_INITIAL_SETTINGS:
        g_value_set_variant (value, priv->initial_settings);
        break;

      case PROP_GETTEXT_DOMAIN:
        g_value_set_string (value, priv->gettext_domain);
        break;

      case PROP_LOCKED_DOWN:
        g_value_set_boolean (value, priv->locked_down);
        break;

      case PROP_LOCKDOWNS:
        g_value_set_flags (value, priv->lockdowns);
        break;

      case PROP_ORIENTATION:
        g_value_set_enum (value, priv->orientation);
        break;

      case PROP_POSITION:
        g_value_set_enum (value, priv->position);
        break;

      case PROP_ENABLE_TOOLTIPS:
        g_value_set_boolean (value, priv->enable_tooltips);
        break;

      case PROP_PREFER_SYMBOLIC_ICONS:
        g_value_set_boolean (value, priv->prefer_symbolic_icons);
        break;

      case PROP_PANEL_ICON_SIZE:
        g_value_set_uint (value, priv->panel_icon_size);
        break;

      case PROP_MENU_ICON_SIZE:
        g_value_set_uint (value, priv->menu_icon_size);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gp_applet_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GpApplet *applet;
  GpAppletPrivate *priv;

  applet = GP_APPLET (object);
  priv = gp_applet_get_instance_private (applet);

  switch (property_id)
    {
      case PROP_MODULE:
        g_assert (priv->module == NULL);
        priv->module = g_value_dup_object (value);
        break;

      case PROP_ID:
        g_assert (priv->id == NULL);
        priv->id = g_value_dup_string (value);
        break;

      case PROP_SETTINGS_PATH:
        g_assert (priv->settings_path == NULL);
        priv->settings_path = g_value_dup_string (value);
        break;

      case PROP_INITIAL_SETTINGS:
        g_assert (priv->initial_settings == NULL);
        priv->initial_settings = g_value_dup_variant (value);
        break;

      case PROP_GETTEXT_DOMAIN:
        g_assert (priv->gettext_domain == NULL);
        priv->gettext_domain = g_value_dup_string (value);
        break;

      case PROP_LOCKED_DOWN:
        gp_applet_set_locked_down (applet, g_value_get_boolean (value));
        break;

      case PROP_LOCKDOWNS:
        gp_applet_set_lockdowns (applet, g_value_get_flags (value));
        break;

      case PROP_ORIENTATION:
        gp_applet_set_orientation (applet, g_value_get_enum (value));
        break;

      case PROP_POSITION:
        gp_applet_set_position (applet, g_value_get_enum (value));
        break;

      case PROP_ENABLE_TOOLTIPS:
        break;

      case PROP_PREFER_SYMBOLIC_ICONS:
        break;

      case PROP_PANEL_ICON_SIZE:
        break;

      case PROP_MENU_ICON_SIZE:
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static gboolean
gp_applet_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  gboolean ret;
  GtkStyleContext *context;
  gdouble width;
  gdouble height;

  ret = GTK_WIDGET_CLASS (gp_applet_parent_class)->draw (widget, cr);

  if (!gtk_widget_has_focus (widget))
    return ret;

  context = gtk_widget_get_style_context (widget);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_focus (context, cr, 0, 0, width, height);

  return ret;
}

static gboolean
gp_applet_focus (GtkWidget        *widget,
                 GtkDirectionType  dir)
{
  GtkWidget *focus_child;
  gboolean ret;

  focus_child = gtk_container_get_focus_child (GTK_CONTAINER (widget));

  if (!focus_child && !gtk_widget_has_focus (widget) &&
      gtk_widget_get_has_tooltip (widget))
    {
      gtk_widget_set_can_focus (widget, TRUE);
      gtk_widget_grab_focus (widget);
      gtk_widget_set_can_focus (widget, FALSE);

      return TRUE;
    }

  ret = GTK_WIDGET_CLASS (gp_applet_parent_class)->focus (widget, dir);

  if (!ret && !focus_child && !gtk_widget_has_focus (widget))
    {
      /* Applet does not have a widget which can focus so set the focus on
       * the applet unless it already had focus because it had a tooltip.
       */
      gtk_widget_set_can_focus (widget, TRUE);
      gtk_widget_grab_focus (widget);
      gtk_widget_set_can_focus (widget, FALSE);

      return TRUE;
    }

  return ret;
}

static GtkSizeRequestMode
gp_applet_get_request_mode (GtkWidget *widget)
{
  GpApplet *applet;
  GpAppletPrivate *priv;

  applet = GP_APPLET (widget);
  priv = gp_applet_get_instance_private (applet);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;

  return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
gp_applet_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GpApplet *self;
  GpAppletPrivate *priv;
  GtkAllocation old_allocation;

  self = GP_APPLET (widget);
  priv = gp_applet_get_instance_private (self);

  gtk_widget_get_allocation (widget, &old_allocation);

  GTK_WIDGET_CLASS (gp_applet_parent_class)->size_allocate (widget, allocation);

  if ((priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
       old_allocation.height != allocation->height) ||
      (priv->orientation == GTK_ORIENTATION_VERTICAL &&
       old_allocation.width != allocation->width))
    {
      if (priv->icon_resize_id == 0)
        {
          priv->icon_resize_id = g_idle_add (icon_resize_cb, self);
          g_source_set_name_by_id (priv->icon_resize_id,
                                   "[libgnome-panel] icon_resize_cb");
        }
    }
}

static void
install_properties (GObjectClass *object_class)
{
  /**
   * GpApplet:module:
   *
   * The applet module.
   */
  properties[PROP_MODULE] =
    g_param_spec_object ("module",
                         "Module",
                         "Module",
                         GP_TYPE_MODULE,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:id:
   *
   * The applet id.
   */
  properties[PROP_ID] =
    g_param_spec_string ("id", "Id", "Id", NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:settings-path:
   *
   * The GSettings path to the per-instance settings of the applet.
   */
  properties[PROP_SETTINGS_PATH] =
    g_param_spec_string ("settings-path", "Settings Path", "Settings Path",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:initial-settings:
   *
   * The GVariant with initial settings.
   */
  properties[PROP_INITIAL_SETTINGS] =
    g_param_spec_variant ("initial-settings", "Initial Settings", "Initial Settings",
                          G_VARIANT_TYPE ("a{sv}"), NULL,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                          G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:gettext-domain:
   *
   * The gettext domain.
   */
  properties[PROP_GETTEXT_DOMAIN] =
    g_param_spec_string ("gettext-domain", "Gettext Domain",
                         "Gettext Domain", NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:locked-down:
   *
   * Whether the applet is on locked down panel.
   *
   * Deprecated: 3.38: Use #GpApplet:lockdowns instead
   */
  properties[PROP_LOCKED_DOWN] =
    g_param_spec_boolean ("locked-down", "Locked Down", "Locked Down",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS | G_PARAM_DEPRECATED);

  /**
   * GpApplet:lockdowns:
   *
   * Active lockdowns.
   */
  properties[PROP_LOCKDOWNS] =
    g_param_spec_flags ("lockdowns",
                        "Lockdowns",
                        "Lockdowns",
                        GP_TYPE_LOCKDOWN_FLAGS,
                        GP_LOCKDOWN_FLAGS_NONE,
                        G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:orientation:
   *
   * The orientation of the panel.
   */
  properties[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation", "Orientation", "Orientation",
                       GTK_TYPE_ORIENTATION, GTK_ORIENTATION_HORIZONTAL,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:position:
   *
   * The position of the panel.
   */
  properties[PROP_POSITION] =
    g_param_spec_enum ("position", "Position", "Position",
                       GTK_TYPE_POSITION_TYPE, GTK_POS_TOP,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:enable-tooltips:
   *
   * Whether the applet should show tooltips.
   */
  properties[PROP_ENABLE_TOOLTIPS] =
    g_param_spec_boolean ("enable-tooltips", "Enable Tooltips", "Enable Tooltips",
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:prefer-symbolic-icons:
   *
   * Whether the applet should prefer symbolic icons in panels.
   */
  properties[PROP_PREFER_SYMBOLIC_ICONS] =
    g_param_spec_boolean ("prefer-symbolic-icons",
                          "Prefer symbolic icons",
                          "Prefer symbolic icons",
                          FALSE,
                          G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:panel-icon-size:
   *
   * The size of icons in panels.
   */
  properties[PROP_PANEL_ICON_SIZE] =
    g_param_spec_uint ("panel-icon-size", "Panel Icon Size", "Panel Icon Size",
                       16, 64, 16,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:menu-icon-size:
   *
   * The size of icons in menus.
   */
  properties[PROP_MENU_ICON_SIZE] =
    g_param_spec_uint ("menu-icon-size", "Menu Icon Size", "Menu Icon Size",
                       16, 24, 16,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
install_signals (void)
{
  /**
   * GpApplet::placement-changed:
   * @applet: the object on which the signal is emitted
   * @orientation: the new orientation
   * @position: the new position
   *
   * Signal is emitted when the orientation or position properties of
   * applet has changed.
   *
   * Note that gp_applet_get_orientation() or gp_applet_get_position()
   * functions will return old values in signal handler!
   */
  signals[PLACEMENT_CHANGED] =
    g_signal_new ("placement-changed", GP_TYPE_APPLET, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GpAppletClass, placement_changed),
                  NULL, NULL, NULL, G_TYPE_NONE, 2,
                  GTK_TYPE_ORIENTATION, GTK_TYPE_POSITION_TYPE);

  /**
   * GpApplet::flags-changed:
   * @applet: the object on which the signal is emitted
   *
   * Signal is emitted when flags has changed.
   */
  signals[FLAGS_CHANGED] =
    g_signal_new ("flags-changed", GP_TYPE_APPLET, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * GpApplet::size-hints-changed:
   * @applet: the object on which the signal is emitted
   *
   * Signal is emitted when size hints has changed.
   */
  signals[SIZE_HINTS_CHANGED] =
    g_signal_new ("size-hints-changed", GP_TYPE_APPLET, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gp_applet_class_init (GpAppletClass *applet_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (applet_class);
  widget_class = GTK_WIDGET_CLASS (applet_class);

  object_class->constructed = gp_applet_constructed;
  object_class->dispose = gp_applet_dispose;
  object_class->finalize = gp_applet_finalize;
  object_class->get_property = gp_applet_get_property;
  object_class->set_property = gp_applet_set_property;

  widget_class->draw = gp_applet_draw;
  widget_class->focus = gp_applet_focus;
  widget_class->get_request_mode = gp_applet_get_request_mode;
  widget_class->size_allocate = gp_applet_size_allocate;

  install_properties (object_class);
  install_signals ();

  gtk_widget_class_set_css_name (widget_class, "gp-applet");
}

static void
gp_applet_init (GpApplet *applet)
{
  GpAppletPrivate *priv;

  priv = gp_applet_get_instance_private (applet);

  priv->builder = gtk_builder_new ();
  priv->action_group = g_simple_action_group_new ();

  priv->general_settings = g_settings_new ("org.gnome.gnome-panel.general");

  g_signal_connect (priv->general_settings, "changed",
                    G_CALLBACK (general_settings_changed_cb), applet);

  general_settings_changed_cb (priv->general_settings, NULL, applet);
}

/**
 * gp_applet_get_locked_down:
 * @applet: a #GpApplet
 *
 * Gets whether @applet is on locked down panel. A locked down applet
 * should not allow any change to its configuration.
 *
 * Returns: %TRUE if @applet is on locked down panel, %FALSE otherwise.
 */
gboolean
gp_applet_get_locked_down (GpApplet *applet)
{
  GpAppletPrivate *priv;

  g_return_val_if_fail (GP_IS_APPLET (applet), FALSE);
  priv = gp_applet_get_instance_private (applet);

  return priv->locked_down;
}

void
gp_applet_set_locked_down (GpApplet *applet,
                           gboolean  locked_down)
{
  GpAppletPrivate *priv;

  priv = gp_applet_get_instance_private (applet);

  if (priv->locked_down == locked_down)
    return;

  priv->locked_down = locked_down;

  g_object_notify_by_pspec (G_OBJECT (applet), properties[PROP_LOCKED_DOWN]);
}

/**
 * gp_applet_get_lockdowns:
 * @applet: a #GpApplet
 *
 * Gets active lockdowns.
 *
 * Returns: the #GpLockdownFlags of @applet.
 */
GpLockdownFlags
gp_applet_get_lockdowns (GpApplet *applet)
{
  GpAppletPrivate *priv;

  g_return_val_if_fail (GP_IS_APPLET (applet), GP_LOCKDOWN_FLAGS_NONE);
  priv = gp_applet_get_instance_private (applet);

  return priv->lockdowns;
}

void
gp_applet_set_lockdowns (GpApplet        *applet,
                         GpLockdownFlags  lockdowns)
{
  GpAppletPrivate *priv;

  priv = gp_applet_get_instance_private (applet);

  if (priv->lockdowns == lockdowns)
    return;

  priv->lockdowns = lockdowns;

  g_object_notify_by_pspec (G_OBJECT (applet), properties[PROP_LOCKDOWNS]);
}

/**
 * gp_applet_get_orientation:
 * @applet: a #GpApplet
 *
 * Returns the orientation of the panel.
 *
 * Returns: the orientation of the panel.
 */
GtkOrientation
gp_applet_get_orientation (GpApplet *applet)
{
  GpAppletPrivate *priv;

  g_return_val_if_fail (GP_IS_APPLET (applet), GTK_ORIENTATION_HORIZONTAL);
  priv = gp_applet_get_instance_private (applet);

  return priv->orientation;
}

void
gp_applet_set_orientation (GpApplet       *applet,
                           GtkOrientation  orientation)
{
  GpAppletPrivate *priv;

  priv = gp_applet_get_instance_private (applet);

  if (priv->orientation == orientation)
    return;

  g_signal_emit (applet, signals[PLACEMENT_CHANGED], 0,
                 orientation, priv->position);

  priv->orientation = orientation;

  g_object_notify_by_pspec (G_OBJECT (applet), properties[PROP_ORIENTATION]);
}

/**
 * gp_applet_get_position:
 * @applet: a #GpApplet
 *
 * Returns the position of the panel.
 *
 * Returns: the position of the panel.
 */
GtkPositionType
gp_applet_get_position (GpApplet *applet)
{
  GpAppletPrivate *priv;

  g_return_val_if_fail (GP_IS_APPLET (applet), GTK_POS_TOP);
  priv = gp_applet_get_instance_private (applet);

  return priv->position;
}

void
gp_applet_set_position (GpApplet        *applet,
                        GtkPositionType  position)
{
  GpAppletPrivate *priv;

  priv = gp_applet_get_instance_private (applet);

  if (priv->position == position)
    return;

  g_signal_emit (applet, signals[PLACEMENT_CHANGED], 0,
                 priv->orientation, position);

  priv->position = position;

  g_object_notify_by_pspec (G_OBJECT (applet), properties[PROP_POSITION]);
}

/**
 * gp_applet_get_flags:
 * @applet: a #GpApplet
 *
 * Gets the #GpAppletFlags of @applet.
 *
 * Returns: the #GpAppletFlags of @applet.
 */
GpAppletFlags
gp_applet_get_flags (GpApplet *applet)
{
  GpAppletPrivate *priv;

  g_return_val_if_fail (GP_IS_APPLET (applet), GP_APPLET_FLAGS_NONE);
  priv = gp_applet_get_instance_private (applet);

  return priv->flags;
}

/**
 * gp_applet_set_flags:
 * @applet: a #GpApplet
 * @flags: a #GpAppletFlags to use for @applet
 *
 * Sets the #GpAppletFlags of @applet. Most of the time, at least
 * %GP_APPLET_FLAGS_EXPAND_MINOR should be used.
 */
void
gp_applet_set_flags (GpApplet      *applet,
                     GpAppletFlags  flags)
{
  GpAppletPrivate *priv;

  priv = gp_applet_get_instance_private (applet);

  if (priv->flags == flags)
    return;

  priv->flags = flags;

  g_signal_emit (applet, signals[FLAGS_CHANGED], 0);
}

/**
 * gp_applet_get_size_hints:
 * @applet: a #GpApplet
 * @n_elements: (out): return location for the length of the returned array
 *
 * Returns array with size hints.
 *
 * Returns: (transfer full) (array length=n_elements): a newly allocated
 *     array, or %NULL.
 */
gint *
gp_applet_get_size_hints (GpApplet *applet,
                          guint    *n_elements)
{
  GpAppletPrivate *priv;
  gint *size_hints;
  guint i;

  g_return_val_if_fail (GP_IS_APPLET (applet), NULL);
  g_return_val_if_fail (n_elements != NULL, NULL);

  priv = gp_applet_get_instance_private (applet);

  if (!priv->size_hints || priv->size_hints->n_elements == 0)
    {
      *n_elements = 0;
      return NULL;
    }

  *n_elements = priv->size_hints->n_elements;
  size_hints = g_new0 (gint, priv->size_hints->n_elements);

  for (i = 0; i < priv->size_hints->n_elements; i++)
    size_hints[i] = priv->size_hints->size_hints[i];

  return size_hints;
}

/**
 * gp_applet_set_size_hints:
 * @applet: a #GpApplet
 * @size_hints: (allow-none): array of sizes or %NULL
 * @n_elements: length of @size_hints
 * @base_size: base size of the applet
 *
 * Give hints to the panel about sizes @applet is comfortable with. This
 * is generally useful for applets that can take a lot of space, in case
 * the panel gets full and needs to restrict the size of some applets.
 *
 * @size_hints should have an even number of sizes. It is an array of
 * (max, min) pairs where min(i) > max(i + 1).
 *
 * @base_size will be added to all sizes in @size_hints, and is therefore
 * a way to guarantee a minimum size to @applet.
 *
 * The panel will try to allocate a size that is acceptable to @applet,
 * i.e. in one of the (@base_size + max, @base_size + min) ranges.
 *
 * %GP_APPLET_FLAGS_EXPAND_MAJOR must be set for @applet to use size hints.
 */
void
gp_applet_set_size_hints (GpApplet   *applet,
                          const gint *size_hints,
                          guint       n_elements,
                          gint        base_size)
{
  GpAppletPrivate *priv;
  guint i;

  g_return_if_fail (GP_IS_APPLET (applet));
  priv = gp_applet_get_instance_private (applet);

  if (!size_hints_changed (priv, size_hints, n_elements, base_size))
    return;

  if (!size_hints || n_elements == 0)
    {
      g_clear_pointer (&priv->size_hints, gp_size_hints_free);
      emit_size_hints_changed (applet);

      return;
    }

  if (!priv->size_hints)
    {
      priv->size_hints = g_new0 (GpSizeHints, 1);
      priv->size_hints->size_hints = g_new0 (gint, n_elements);
      priv->size_hints->n_elements = n_elements;
    }
  else
    {
      if (priv->size_hints->n_elements < n_elements)
        {
          g_free (priv->size_hints->size_hints);
          priv->size_hints->size_hints = g_new0 (gint, n_elements);
        }

      priv->size_hints->n_elements = n_elements;
    }

  for (i = 0; i < n_elements; i++)
    priv->size_hints->size_hints[i] = size_hints[i] + base_size;

  emit_size_hints_changed (applet);
}

/**
 * gp_applet_settings_new:
 * @applet: a #GpApplet
 * @schema: the name of the schema
 *
 * Creates a new #GSettings object for the per-instance settings of @applet,
 * with a given schema.
 *
 * Note that you cannot call gp_applet_settings_new() from init() class
 * method since required property settings-path is not set yet.
 *
 * Returns: (transfer full): a newly created #GSettings.
 */
GSettings *
gp_applet_settings_new (GpApplet    *applet,
                        const gchar *schema)
{
  GpAppletPrivate *priv;

  g_return_val_if_fail (GP_IS_APPLET (applet), NULL);
  g_return_val_if_fail (schema != NULL, NULL);

  priv = gp_applet_get_instance_private (applet);

  if (!priv->settings_path)
    {
      g_assert_not_reached ();
      return NULL;
    }

  return g_settings_new_with_path (schema, priv->settings_path);
}

/**
 * gp_applet_request_focus:
 * @applet: a #GpApplet
 * @timestamp: the timestamp of the user interaction (typically a button
 *     or key press event) which triggered this call
 *
 * Requests focus for @applet. There is no guarantee that @applet will
 * successfully get focus after that call.
 */
void
gp_applet_request_focus (GpApplet *applet,
                         guint32   timestamp)
{
  GtkWidget *widget;
  GtkWidget *toplevel;
  GdkWindow *window;

  g_return_if_fail (GP_IS_APPLET (applet));

  widget = GTK_WIDGET (applet);
  toplevel = gtk_widget_get_toplevel (widget);

  if (toplevel == NULL)
    return;

  window = gtk_widget_get_window (toplevel);

  if (window == NULL)
    return;

  gdk_window_focus (window, timestamp);
}

/**
 * gp_applet_setup_menu:
 * @applet: a #GpApplet
 * @xml: a menu XML string
 * @entries: a pointer to the first item in an %NULL-terminated array
 *     of #GActionEntry structs
 *
 * Sets up the context menu of @applet.
 */
void
gp_applet_setup_menu (GpApplet           *applet,
                      const gchar        *xml,
                      const GActionEntry *entries)
{
  GpAppletPrivate *priv;
  GError *error;
  GActionMap *action_map;

  g_return_if_fail (GP_IS_APPLET (applet));
  g_return_if_fail (xml != NULL);

  priv = gp_applet_get_instance_private (applet);

  error = NULL;
  gtk_builder_add_from_string (priv->builder, xml, -1, &error);

  if (error)
    {
      g_warning ("Error setting up menu: %s", error->message);
      g_error_free (error);
    }

  action_map = G_ACTION_MAP (priv->action_group);
  g_action_map_add_action_entries (action_map, entries, -1, applet);
}

/**
 * gp_applet_setup_menu_from_file:
 * @applet: a #GpApplet
 * @filename: path to a menu XML file
 * @entries: a pointer to the first item in an %NULL-terminated array
 *     of #GActionEntry structs
 *
 * Sets up the context menu of @applet.
 */
void
gp_applet_setup_menu_from_file (GpApplet           *applet,
                                const gchar        *filename,
                                const GActionEntry *entries)
{
  GpAppletPrivate *priv;
  GError *error;
  GActionMap *action_map;

  g_return_if_fail (GP_IS_APPLET (applet));
  g_return_if_fail (filename != NULL);

  priv = gp_applet_get_instance_private (applet);

  error = NULL;
  gtk_builder_add_from_file (priv->builder, filename, &error);

  if (error)
    {
      g_warning ("Error setting up menu: %s", error->message);
      g_error_free (error);
    }

  action_map = G_ACTION_MAP (priv->action_group);
  g_action_map_add_action_entries (action_map, entries, -1, applet);
}

/**
 * gp_applet_setup_menu_from_resource:
 * @applet: a #GpApplet
 * @resource_path: a resource path
 * @entries: a pointer to the first item in an %NULL-terminated array
 *     of #GActionEntry structs
 *
 * Sets up the context menu of @applet.
 */
void
gp_applet_setup_menu_from_resource (GpApplet           *applet,
                                    const gchar        *resource_path,
                                    const GActionEntry *entries)
{
  GpAppletPrivate *priv;
  GError *error;
  GActionMap *action_map;

  g_return_if_fail (GP_IS_APPLET (applet));
  g_return_if_fail (resource_path != NULL);

  priv = gp_applet_get_instance_private (applet);

  error = NULL;
  gtk_builder_add_from_resource (priv->builder, resource_path, &error);

  if (error)
    {
      g_warning ("Error setting up menu: %s", error->message);
      g_error_free (error);
    }

  action_map = G_ACTION_MAP (priv->action_group);
  g_action_map_add_action_entries (action_map, entries, -1, applet);
}

/**
 * gp_applet_menu_lookup_action:
 * @applet: a #GpApplet
 * @action_name: the name of an action
 *
 * Looks up the action with the name @action_name in action group.
 *
 * Returns: (transfer none): a #GAction, or %NULL.
 */
GAction *
gp_applet_menu_lookup_action (GpApplet    *applet,
                              const gchar *action_name)
{
  GpAppletPrivate *priv;
  GActionMap *action_map;

  g_return_val_if_fail (GP_IS_APPLET (applet), NULL);
  priv = gp_applet_get_instance_private (applet);

  action_map = G_ACTION_MAP (priv->action_group);

  return g_action_map_lookup_action (action_map, action_name);
}

GtkWidget *
gp_applet_get_menu (GpApplet *applet)
{
  GpAppletPrivate *priv;
  gchar *name;
  GObject *object;

  g_return_val_if_fail (GP_IS_APPLET (applet), NULL);
  priv = gp_applet_get_instance_private (applet);

  name = g_strdup_printf ("%s-menu", priv->id);
  object = gtk_builder_get_object (priv->builder, name);
  g_free (name);

  if (!object)
    return NULL;

  return gtk_menu_new_from_model (G_MENU_MODEL (object));
}

void
gp_applet_remove_from_panel (GpApplet *self)
{
  if (GP_APPLET_GET_CLASS (self)->remove_from_panel == NULL)
    return;

  GP_APPLET_GET_CLASS (self)->remove_from_panel (self);
}

/**
 * gp_applet_get_prefer_symbolic_icons:
 * @applet: a #GpApplet
 *
 * Returns whether the applet should prefer symbolic icons in panels.
 *
 * Returns: whether the applet should prefer symbolic icons in panels.
 */
gboolean
gp_applet_get_prefer_symbolic_icons (GpApplet *applet)
{
  GpAppletPrivate *priv;

  g_return_val_if_fail (GP_IS_APPLET (applet), FALSE);
  priv = gp_applet_get_instance_private (applet);

  return priv->prefer_symbolic_icons;
}

void
gp_applet_set_prefer_symbolic_icons (GpApplet *self,
                                     gboolean  prefer_symbolic_icons)
{
  GpAppletPrivate *priv;

  priv = gp_applet_get_instance_private (self);

  if (priv->prefer_symbolic_icons == prefer_symbolic_icons)
    return;

  priv->prefer_symbolic_icons = prefer_symbolic_icons;

  g_object_notify_by_pspec (G_OBJECT (self),
                            properties[PROP_PREFER_SYMBOLIC_ICONS]);
}

/**
 * gp_applet_get_panel_icon_size:
 * @applet: a #GpApplet
 *
 * Returns the panel icon size.
 *
 * Returns: the panel icon size.
 */
guint
gp_applet_get_panel_icon_size (GpApplet *applet)
{
  GpAppletPrivate *priv;

  g_return_val_if_fail (GP_IS_APPLET (applet), 16);
  priv = gp_applet_get_instance_private (applet);

  return priv->panel_icon_size;
}

void
gp_applet_set_panel_icon_size (GpApplet *self,
                               guint     panel_icon_size)
{
  GpAppletPrivate *priv;

  priv = gp_applet_get_instance_private (self);

  if (priv->panel_icon_size == panel_icon_size)
    return;

  priv->panel_icon_size = panel_icon_size;

  g_object_notify_by_pspec (G_OBJECT (self),
                            properties[PROP_PANEL_ICON_SIZE]);
}

/**
 * gp_applet_get_menu_icon_size:
 * @applet: a #GpApplet
 *
 * Returns the menu icon size.
 *
 * Returns: the menu icon size.
 */
guint
gp_applet_get_menu_icon_size (GpApplet *applet)
{
  GpAppletPrivate *priv;

  g_return_val_if_fail (GP_IS_APPLET (applet), 16);
  priv = gp_applet_get_instance_private (applet);

  return priv->menu_icon_size;
}

void
gp_applet_set_menu_icon_size (GpApplet *self,
                              guint     menu_icon_size)
{
  GpAppletPrivate *priv;

  priv = gp_applet_get_instance_private (self);

  if (priv->menu_icon_size == menu_icon_size)
    return;

  priv->menu_icon_size = menu_icon_size;

  g_object_notify_by_pspec (G_OBJECT (self),
                            properties[PROP_MENU_ICON_SIZE]);
}

/**
 * gp_applet_show_about:
 * @applet: a #GpApplet
 *
 * Show about dialog. #GpAboutDialogFunc must be set with
 * gp_applet_info_set_about_dialog().
 */
void
gp_applet_show_about (GpApplet *applet)
{
  GpAppletPrivate *priv;

  g_return_if_fail (GP_IS_APPLET (applet));
  priv = gp_applet_get_instance_private (applet);

  if (priv->about_dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (priv->about_dialog));
      return;
    }

  priv->about_dialog = gp_module_create_about_dialog (priv->module,
                                                      NULL,
                                                      priv->id);

  if (priv->about_dialog == NULL)
    return;

  g_object_add_weak_pointer (G_OBJECT (priv->about_dialog),
                             (gpointer *) &priv->about_dialog);

  gtk_window_present (GTK_WINDOW (priv->about_dialog));
}

/**
 * gp_applet_show_help:
 * @applet: a #GpApplet
 * @page: the optional page identifier
 *
 * Show help. Help URI must be set with gp_applet_info_set_help_uri().
 *
 * The optional @page indentifier may include options and anchor if needed.
 */
void
gp_applet_show_help (GpApplet   *applet,
                     const char *page)
{
  GpAppletPrivate *priv;

  g_return_if_fail (GP_IS_APPLET (applet));
  priv = gp_applet_get_instance_private (applet);

  gp_module_show_help (priv->module, NULL, priv->id, page);
}

/**
 * gp_applet_popup_menu_at_widget:
 * @applet: a #GpApplet
 * @menu: the #GtkMenu to pop up.
 * @widget: the #GtkWidget to align menu with.
 * @event: the #GdkEvent that initiated this request or NULL if it's the current event.
 *
 * Displays menu and makes it available for selection. This is convenience function
 * around gtk_menu_popup_at_widget() that automatically computes the widget_anchor and
 * menu_anchor parameters based on the current applet position.
 */
void
gp_applet_popup_menu_at_widget (GpApplet  *applet,
                                GtkMenu   *menu,
                                GtkWidget *widget,
                                GdkEvent  *event)
{
  GdkGravity widget_anchor;
  GdkGravity menu_anchor;

  switch (gp_applet_get_position (GP_APPLET (applet)))
    {
      case GTK_POS_TOP:
        widget_anchor = GDK_GRAVITY_SOUTH_WEST;
        menu_anchor = GDK_GRAVITY_NORTH_WEST;
        break;

      case GTK_POS_LEFT:
        widget_anchor = GDK_GRAVITY_NORTH_EAST;
        menu_anchor = GDK_GRAVITY_NORTH_WEST;
        break;

      case GTK_POS_RIGHT:
        widget_anchor = GDK_GRAVITY_NORTH_WEST;
        menu_anchor = GDK_GRAVITY_NORTH_EAST;
        break;

      case GTK_POS_BOTTOM:
        widget_anchor = GDK_GRAVITY_NORTH_WEST;
        menu_anchor = GDK_GRAVITY_SOUTH_WEST;
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  gtk_menu_popup_at_widget (menu, GTK_WIDGET (widget),
                            widget_anchor, menu_anchor,
                            event);
}

void
gp_applet_set_enable_tooltips (GpApplet *self,
                               gboolean  enable_tooltips)
{
  GpAppletPrivate *priv;

  priv = gp_applet_get_instance_private (self);

  if (priv->enable_tooltips == enable_tooltips)
    return;

  priv->enable_tooltips = enable_tooltips;

  g_object_notify_by_pspec (G_OBJECT (self),
                            properties[PROP_ENABLE_TOOLTIPS]);
}
