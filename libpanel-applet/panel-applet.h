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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *     Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_APPLET_H__
#define __PANEL_APPLET_H__

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
	PANEL_APPLET_ORIENT_UP,
	PANEL_APPLET_ORIENT_DOWN,
	PANEL_APPLET_ORIENT_LEFT,
	PANEL_APPLET_ORIENT_RIGHT
} PanelAppletOrient;

#define PANEL_TYPE_APPLET         (panel_applet_get_type ())
#define PANEL_APPLET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_APPLET, PanelApplet))
#define PANEL_APPLET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_APPLET, PanelAppletClass))
#define PANEL_IS_APPLET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_APPLET))
#define PANEL_IS_APPLET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_APPLET))
#define PANEL_APPLET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_APPLET, PanelAppletClass))

typedef enum {
	PANEL_NO_BACKGROUND,
	PANEL_COLOR_BACKGROUND,
	PANEL_PIXMAP_BACKGROUND
} PanelAppletBackgroundType;

typedef enum {
	PANEL_APPLET_FLAGS_NONE   = 0,
	PANEL_APPLET_EXPAND_MAJOR = 1 << 0,
	PANEL_APPLET_EXPAND_MINOR = 1 << 1,
	PANEL_APPLET_HAS_HANDLE   = 1 << 2
} PanelAppletFlags;

typedef struct _PanelApplet        PanelApplet;
typedef struct _PanelAppletClass   PanelAppletClass;
typedef struct _PanelAppletPrivate PanelAppletPrivate;

typedef gboolean (*PanelAppletFactoryCallback) (PanelApplet *applet,
						const gchar *iid,
						gpointer     user_data);

struct _PanelApplet {
	GtkEventBox          event_box;

	PanelAppletPrivate  *priv;
};

struct _PanelAppletClass {
	GtkEventBoxClass     event_box_class;

	void (*change_orient) (PanelApplet       *applet,
			       PanelAppletOrient  orient);

	void (*change_size)   (PanelApplet       *applet,
			       guint              size);

	void (*change_background) (PanelApplet               *applet,
				   PanelAppletBackgroundType  type,
				   GdkColor                  *color,
				   GdkPixmap                 *pixmap);
	void (*move_focus_out_of_applet) (PanelApplet        *frame,
					  GtkDirectionType    direction);
};

GType              panel_applet_get_type             (void) G_GNUC_CONST;
 
GtkWidget         *panel_applet_new                  (void);

PanelAppletOrient  panel_applet_get_orient           (PanelApplet *applet);
guint              panel_applet_get_size             (PanelApplet *applet);
PanelAppletBackgroundType
                   panel_applet_get_background       (PanelApplet *applet,
						      /* return values */
						      GdkColor    *color,
						      GdkPixmap  **pixmap);

void               panel_applet_set_background_widget (PanelApplet *applet,
						       GtkWidget   *widget);

gchar             *panel_applet_get_preferences_key  (PanelApplet *applet);

void               panel_applet_add_preferences      (PanelApplet  *applet,
						      const gchar  *schema_dir,
						      GError      **opt_error);

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
						      GtkActionGroup     *action_group);
void               panel_applet_setup_menu_from_file (PanelApplet        *applet,
						      const gchar        *filename,
						      GtkActionGroup     *action_group);

int                panel_applet_factory_main          (const gchar		  *factory_id,
						       gboolean                    out_process,
						       GType                       applet_type,
						       PanelAppletFactoryCallback  callback,
						       gpointer			   data);
gboolean           _panel_applet_shlib_factory        (void);


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

#define PANEL_APPLET_OUT_PROCESS_FACTORY(id, type, name, callback, data)	\
int main (int argc, char *argv [])						\
{										\
	GOptionContext *context;						\
	GError         *error;							\
	int             retval;							\
										\
	_PANEL_APPLET_SETUP_GETTEXT (TRUE);					\
										\
	context = g_option_context_new ("");					\
	g_option_context_add_group (context, gtk_get_option_group (TRUE));	\
										\
	error = NULL;								\
	if (!g_option_context_parse (context, &argc, &argv, &error)) {		\
		if (error) {							\
			g_printerr ("Cannot parse arguments: %s.\n",		\
				    error->message);				\
			g_error_free (error);					\
		} else								\
			g_printerr ("Cannot parse arguments.\n");		\
		g_option_context_free (context);				\
		return 1;							\
	}									\
										\
	gtk_init (&argc, &argv);						\
                                                                                \
        retval = panel_applet_factory_main (id, TRUE, type, callback, data);    \
	g_option_context_free (context);					\
										\
	return retval;								\
}

#define PANEL_APPLET_IN_PROCESS_FACTORY(id, type, descr, callback, data)	\
G_MODULE_EXPORT gint                                                            \
_panel_applet_shlib_factory (void)					        \
{										\
	_PANEL_APPLET_SETUP_GETTEXT (FALSE);					\
        return panel_applet_factory_main (id, FALSE, type, callback, data);     \
}

G_END_DECLS

#endif /* __PANEL_APPLET_H__ */
