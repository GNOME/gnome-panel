/*
 * Copyright (C) 2001 Sun Microsystems, Inc.
 * Copyright (c) 2010 Carlos Garcia Campos
 * Copyright (C) 2016 Alberts Muktupāvels
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

typedef struct
 {
  gint  *size_hints;
  guint  n_elements;
} GpSizeHints;

typedef struct
{
  GtkBuilder         *builder;
  GSimpleActionGroup *action_group;

  gchar              *id;
  gchar              *settings_path;
  gchar              *translation_domain;
  gboolean            locked_down;
  GtkOrientation      orientation;
  GtkPositionType     position;

  GpAppletFlags       flags;
  GpSizeHints        *size_hints;

  guint               size_hints_idle;
} GpAppletPrivate;

enum
{
  PROP_0,

  PROP_ID,
  PROP_SETTINGS_PATH,
  PROP_TRANSLATION_DOMAIN,
  PROP_LOCKED_DOWN,
  PROP_ORIENTATION,
  PROP_POSITION,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

enum
{
  LOCKED_DOWN_CHANGED,
  PLACEMENT_CHANGED,

  FLAGS_CHANGED,
  SIZE_HINTS_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GpApplet, gp_applet, GTK_TYPE_EVENT_BOX)

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

  if ((!priv->size_hints && size_hints) || (priv->size_hints && !size_hints))
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
  GpAppletPrivate *priv;
  GActionGroup *group;

  G_OBJECT_CLASS (gp_applet_parent_class)->constructed (object);

  applet = GP_APPLET (object);
  priv = gp_applet_get_instance_private (applet);

  gtk_builder_set_translation_domain (priv->builder, priv->translation_domain);

  group = G_ACTION_GROUP (priv->action_group);
  gtk_widget_insert_action_group (GTK_WIDGET (applet), priv->id, group);
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

  if (priv->size_hints_idle != 0)
    {
      g_source_remove (priv->size_hints_idle);
      priv->size_hints_idle = 0;
    }

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
  g_clear_pointer (&priv->translation_domain, g_free);
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
      case PROP_ID:
        g_value_set_string (value, priv->id);
        break;

      case PROP_SETTINGS_PATH:
        g_value_set_string (value, priv->settings_path);
        break;

      case PROP_TRANSLATION_DOMAIN:
        g_value_set_string (value, priv->translation_domain);
        break;

      case PROP_LOCKED_DOWN:
        g_value_set_boolean (value, priv->locked_down);
        break;

      case PROP_ORIENTATION:
        g_value_set_enum (value, priv->orientation);
        break;

      case PROP_POSITION:
        g_value_set_enum (value, priv->position);
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
      case PROP_ID:
        g_assert (priv->id == NULL);
        priv->id = g_value_dup_string (value);
        break;

      case PROP_SETTINGS_PATH:
        g_assert (priv->settings_path == NULL);
        priv->settings_path = g_value_dup_string (value);
        break;

      case PROP_TRANSLATION_DOMAIN:
        g_assert (priv->translation_domain == NULL);
        priv->translation_domain = g_value_dup_string (value);
        break;

      case PROP_LOCKED_DOWN:
        gp_applet_set_locked_down (applet, g_value_get_boolean (value));
        break;

      case PROP_ORIENTATION:
        gp_applet_set_orientation (applet, g_value_get_enum (value));
        break;

      case PROP_POSITION:
        gp_applet_set_position (applet, g_value_get_enum (value));
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

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;

  return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
install_properties (GObjectClass *object_class)
{
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
   * GpApplet:translation-domain:
   *
   * The translation domain.
   */
  properties[PROP_TRANSLATION_DOMAIN] =
    g_param_spec_string ("translation-domain", "Translation Domain",
                         "Translation Domain", NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:locked-down:
   *
   * Whether the applet is on locked down panel.
   */
  properties[PROP_LOCKED_DOWN] =
    g_param_spec_boolean ("locked-down", "Locked Down", "Locked Down",
                          FALSE,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:orientation:
   *
   * The orientation of the panel.
   */
  properties[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation", "Orientation", "Orientation",
                       GTK_TYPE_ORIENTATION, GTK_ORIENTATION_HORIZONTAL,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  /**
   * GpApplet:position:
   *
   * The position of the panel.
   */
  properties[PROP_POSITION] =
    g_param_spec_enum ("position", "Position", "Position",
                       GTK_TYPE_POSITION_TYPE, GTK_POS_TOP,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
install_signals (void)
{
  /**
   * GpApplet::locked-down-changed:
   * @applet: the object on which the signal is emitted
   * @locked_down: the new locked down value
   *
   * Signal is emmited when the locked down property of applet has
   * changed.
   */
  signals[LOCKED_DOWN_CHANGED] =
    g_signal_new ("locked-down-changed", GP_TYPE_APPLET, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GpAppletClass, locked_down_changed),
                  NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

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

  g_signal_emit (applet, signals[LOCKED_DOWN_CHANGED], 0, locked_down);
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

/**
 * gp_applet_add_text_class:
 * @applet: a #GpApplet
 * @widget: a #GtkWidget
 *
 * Use this function to add css class to widgets that are visible on panel
 * and shows text.
 */
void
gp_applet_add_text_class (GpApplet  *applet,
                          GtkWidget *widget)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_add_class (context, "gp-text-color");
}
