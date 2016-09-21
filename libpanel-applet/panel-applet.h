/*
 * panel-applet.h: panel applet writing API.
 *
 * Copyright (C) 2001 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_APPLET_H__
#define __PANEL_APPLET_H__

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * PanelAppletOrient:
 * @PANEL_APPLET_ORIENT_UP: The #PanelApplet is on a horizontal panel, at the
 * bottom of the screen. It is oriented towards the top of the screen.
 * @PANEL_APPLET_ORIENT_DOWN: The #PanelApplet is on a horizontal panel, at the
 * top of the screen. It is oriented towards the bottom of the screen.
 * @PANEL_APPLET_ORIENT_LEFT: The #PanelApplet is on a vertical panel, at the
 * right of the screen. It is oriented towards the left of the screen.
 * @PANEL_APPLET_ORIENT_RIGHT: The #PanelApplet is on a vertical panel, at the
 * left of the screen. It is oriented towards the right of the screen.
 *
 * Type defining the orientation of the applet. The values may seem backward
 * (e.g. %PANEL_APPLET_ORIENT_RIGHT means the panel is on the left side), but
 * this represents the direction the applet is oriented to.
 **/
typedef enum {
	PANEL_APPLET_ORIENT_UP,
	PANEL_APPLET_ORIENT_DOWN,
	PANEL_APPLET_ORIENT_LEFT,
	PANEL_APPLET_ORIENT_RIGHT
} PanelAppletOrient;

/**
 * PANEL_APPLET_ORIENT_FIRST:
 *
 * This macro is an alias for #PANEL_APPLET_ORIENT_UP
 **/
#define PANEL_APPLET_ORIENT_FIRST PANEL_APPLET_ORIENT_UP

/**
 * PANEL_APPLET_ORIENT_LAST:
 *
 * This macro is an alias for #PANEL_APPLET_ORIENT_RIGHT
 **/
#define PANEL_APPLET_ORIENT_LAST  PANEL_APPLET_ORIENT_RIGHT

#define PANEL_TYPE_APPLET         (panel_applet_get_type ())
#define PANEL_APPLET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_APPLET, PanelApplet))
#define PANEL_APPLET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_APPLET, PanelAppletClass))
#define PANEL_IS_APPLET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_APPLET))
#define PANEL_IS_APPLET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_APPLET))
#define PANEL_APPLET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_APPLET, PanelAppletClass))

/**
 * PanelAppletFlags:
 * @PANEL_APPLET_FLAGS_NONE: No flags set.
 * @PANEL_APPLET_EXPAND_MAJOR: On horizontal panels, the #PanelApplet will be
 * allocated as much width as possible. On vertical panels, it will be
 * allocated as much height as possible.
 * @PANEL_APPLET_EXPAND_MINOR: On horizontal panels, the #PanelApplet will be
 * allocated the full height of the panel. On vertical panels, it will be
 * allocated the full width. Most applets should set this flag in order to use
 * the full panel size and allow the applet to be Fitt's Law compliant.
 * @PANEL_APPLET_HAS_HANDLE: The panel will draw a handle for the
 * #PanelApplet. This handle will be usable to move the applet, and to open the
 * context menu of the applet.
 *
 * Flags to be used with panel_applet_get_flags()/panel_applet_set_flags(), to
 * indicate to the panel a specific behavior requested by the #PanelApplet.
 **/
typedef enum {
	PANEL_APPLET_FLAGS_NONE   = 0,
	PANEL_APPLET_EXPAND_MAJOR = 1 << 0,
	PANEL_APPLET_EXPAND_MINOR = 1 << 1,
	PANEL_APPLET_HAS_HANDLE   = 1 << 2
} PanelAppletFlags;

/**
 * PANEL_APPLET_FLAGS_ALL:
 *
 * A convenience macro that combines #PANEL_APPLET_EXPAND_MAJOR, #PANEL_APPLET_EXPAND_MINOR and
 *     #PANEL_APPLET_HAS_HANDLE from #PanelAppletFlags
 **/
#define PANEL_APPLET_FLAGS_ALL (PANEL_APPLET_EXPAND_MAJOR | \
                                PANEL_APPLET_EXPAND_MINOR | \
                                PANEL_APPLET_HAS_HANDLE)

typedef struct _PanelApplet        PanelApplet;
typedef struct _PanelAppletClass   PanelAppletClass;
typedef struct _PanelAppletPrivate PanelAppletPrivate;

/**
 * PanelAppletFactoryCallback:
 * @applet: a newly-created #PanelApplet.
 * @iid: identifier of the requested applet type.
 * @user_data: user data.
 *
 * The type used for callbacks after. The callback will usually check that @iid
 * is a valid applet type identifier for the applet factory, and will then fill
 * @applet with widgets, connect to signals, etc.
 *
 * Returns: %TRUE if @iid is a valid applet type identifier for the applet
 * factory and if the creation of @applet was successfully completed, %FALSE
 * otherwise.
 **/
typedef gboolean (*PanelAppletFactoryCallback) (PanelApplet *applet,
						const gchar *iid,
						gpointer     user_data);

/**
 * PanelApplet:
 *
 * #PanelApplet is an opaque data structure and can only be accessed using
 * the following functions.
 */
struct _PanelApplet {
	GtkEventBox          event_box;

	/*< private >*/
	PanelAppletPrivate  *priv;
};

/**
 * PanelAppletClass:
 * @event_box_class: The parent class.
 * @change_orient: Signal is emitted when the orientation of applet
 *    has changed.
 * @move_focus_out_of_applet: Signal is emmited when the focus is moved
 *    out of applet. This is an implementation detail.
 *
 * The class structure for the #PanelApplet class.
 */
struct _PanelAppletClass {
	GtkEventBoxClass     event_box_class;

	void (*change_orient) (PanelApplet       *applet,
			       PanelAppletOrient  orient);
	void (*move_focus_out_of_applet) (PanelApplet        *frame,
					  GtkDirectionType    direction);
};

GType              panel_applet_get_type             (void) G_GNUC_CONST;
 
PanelAppletOrient  panel_applet_get_orient           (PanelApplet *applet);
GtkOrientation     panel_applet_get_gtk_orientation  (PanelApplet *applet);

GSettings         *panel_applet_settings_new         (PanelApplet *applet,
						      const char  *schema);

PanelAppletFlags   panel_applet_get_flags            (PanelApplet      *applet);
void      	   panel_applet_set_flags            (PanelApplet      *applet,
						      PanelAppletFlags  flags);

void      	   panel_applet_set_size_hints       (PanelApplet      *applet,
						      const int        *size_hints,
						      int               n_elements,
						      int               base_size);

gboolean           panel_applet_get_locked_down      (PanelApplet  *applet);

void               panel_applet_request_focus        (PanelApplet  *applet,
						      guint32       timestamp);

void               panel_applet_setup_menu           (PanelApplet        *applet,
						      const gchar        *xml,
						      GSimpleActionGroup *action_group,
						      const gchar        *translation_domain);
void               panel_applet_setup_menu_from_file (PanelApplet        *applet,
						      const gchar        *filename,
						      GSimpleActionGroup *action_group,
						      const gchar        *translation_domain);
void               panel_applet_setup_menu_from_resource (PanelApplet    *applet,
						          const gchar    *resource_path,
							  GSimpleActionGroup *action_group,
							  const gchar        *translation_domain);

int                panel_applet_factory_setup_in_process (const gchar               *factory_id,
							  GType                      applet_type,
							  PanelAppletFactoryCallback callback,
							  gpointer                   data);


/*
 * These macros are getting a bit unwieldy.
 *
 * Things to define for these:
 *	+ required if Native Language Support is enabled (ENABLE_NLS):
 *                   GETTEXT_PACKAGE and GNOMELOCALEDIR 
 */

#if !defined(ENABLE_NLS)
#define _PANEL_APPLET_SETUP_GETTEXT(call_textdomain)				\
	do { } while (0)
#else /* defined(ENABLE_NLS) */
#include <libintl.h>
#define _PANEL_APPLET_SETUP_GETTEXT(call_textdomain)				\
	do {									\
		bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);		\
		bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");		\
		if (call_textdomain)						\
			textdomain (GETTEXT_PACKAGE);				\
	} while (0)
#endif /* !defined(ENABLE_NLS) */

/**
 * PANEL_APPLET_IN_PROCESS_FACTORY:
 * @factory_id: identifier of an applet factory.
 * @type: GType of the applet this factory creates.
 * @callback: (scope call): a %PanelAppletFactoryCallback to be called
 *     when a new applet is created.
 * @data: (closure): callback data.
 *
 * Convenience macro providing the symbol needed to automatically register the
 * @factory_id applet factory for an in-process applet.
 *
 * Applet instances created by the applet factory will use @applet_type as
 * GType. Unless you subclass #PanelApplet, you should use %PANEL_TYPE_APPLET
 * as @applet_type.
 *
 * On creation of the applet instances, @callback is called to setup the
 * applet. If @callback returns %FALSE, the creation of the applet instance is
 * cancelled.
 */
#define PANEL_APPLET_IN_PROCESS_FACTORY(factory_id, type, callback, data)       \
gboolean _panel_applet_shlib_factory (void);					\
G_MODULE_EXPORT gint                                                            \
_panel_applet_shlib_factory (void)					        \
{										\
	_PANEL_APPLET_SETUP_GETTEXT (FALSE);					\
        return panel_applet_factory_setup_in_process (factory_id, type,         \
                                                      callback, data);          \
}

G_END_DECLS

#endif /* __PANEL_APPLET_H__ */
