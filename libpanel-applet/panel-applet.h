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

#include <glib/gmacros.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkeventbox.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-client.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-generic-factory.h>

#include <GNOME_Panel.h>

G_BEGIN_DECLS

typedef GNOME_Vertigo_PanelOrient PanelAppletOrient;

#define PANEL_APPLET_ORIENT_UP    GNOME_Vertigo_PANEL_ORIENT_UP
#define PANEL_APPLET_ORIENT_DOWN  GNOME_Vertigo_PANEL_ORIENT_DOWN
#define PANEL_APPLET_ORIENT_LEFT  GNOME_Vertigo_PANEL_ORIENT_LEFT
#define PANEL_APPLET_ORIENT_RIGHT GNOME_Vertigo_PANEL_ORIENT_RIGHT


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

BonoboControl     *panel_applet_get_control          (PanelApplet  *applet);
BonoboUIComponent *panel_applet_get_popup_component  (PanelApplet  *applet);

void               panel_applet_setup_menu           (PanelApplet        *applet,
						      const gchar        *xml,
						      const BonoboUIVerb *verb_list,
						      gpointer            user_data);

void               panel_applet_setup_menu_from_file (PanelApplet        *applet,
						      const gchar        *opt_datadir,
						      const gchar        *file,
						      const gchar        *opt_app_name,
						      const BonoboUIVerb *verb_list,
						      gpointer            user_data);


int                panel_applet_factory_main          (const gchar		  *iid,
						       GType                       applet_type,
						       PanelAppletFactoryCallback  callback,
						       gpointer			   data);

int                panel_applet_factory_main_closure  (const gchar		  *iid,
						       GType                       applet_type,
						       GClosure                   *closure);

Bonobo_Unknown     panel_applet_shlib_factory         (const char                 *iid,
						       GType                       applet_type,
						       PortableServer_POA          poa,
						       gpointer                    impl_ptr,
						       PanelAppletFactoryCallback  callback,
						       gpointer                    user_data,
						       CORBA_Environment          *ev);

Bonobo_Unknown	   panel_applet_shlib_factory_closure (const char                 *iid,
						       GType                       applet_type,
						       PortableServer_POA          poa,
						       gpointer                    impl_ptr,
						       GClosure                   *closure,
						       CORBA_Environment          *ev);

/*
 * These macros are getting a bit unwieldy.
 *
 * Things to define for these:
 *	+ required if Native Language Support is enabled (ENABLE_NLS):
 *                   GETTEXT_PACKAGE and GNOMELOCALEDIR 
 *      + optional : PREFIX, SYSCONFDIR, DATADIR and LIBDIR.
 */

#if !defined(ENABLE_NLS)
#if defined(PREFIX) && defined(SYSCONFDIR) && defined(DATADIR) && defined(LIBDIR)
#define PANEL_APPLET_BONOBO_FACTORY(iid, type, name, version, callback, data)	\
int main (int argc, char *argv [])						\
{										\
	gnome_program_init (name, version,					\
			    LIBGNOMEUI_MODULE,					\
			    argc, argv,						\
			    GNOME_CLIENT_PARAM_SM_CONNECT, FALSE,               \
			    GNOME_PROGRAM_STANDARD_PROPERTIES,			\
			    NULL);						\
        return panel_applet_factory_main (iid, type, callback, data);		\
}
#else /* !defined(PREFIX) ... */
#define PANEL_APPLET_BONOBO_FACTORY(iid, type, name, version, callback, data)	\
int main (int argc, char *argv [])						\
{										\
	gnome_program_init (name, version,					\
			    LIBGNOMEUI_MODULE,					\
			    argc, argv,						\
			    GNOME_CLIENT_PARAM_SM_CONNECT, FALSE,               \
			    GNOME_PARAM_NONE);					\
        return panel_applet_factory_main (iid, type, callback, data);		\
}
#endif /* defined(PREFIX) ... */
#else /* defined(ENABLE_NLS) */
#include <libintl.h>
#if defined(PREFIX) && defined(SYSCONFDIR) && defined(DATADIR) && defined(LIBDIR)
#define PANEL_APPLET_BONOBO_FACTORY(iid, type, name, version, callback, data)	\
int main (int argc, char *argv [])						\
{										\
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);			\
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");			\
	textdomain (GETTEXT_PACKAGE);						\
	gnome_program_init (name, version,					\
			    LIBGNOMEUI_MODULE,					\
			    argc, argv,						\
			    GNOME_CLIENT_PARAM_SM_CONNECT, FALSE,               \
			    GNOME_PROGRAM_STANDARD_PROPERTIES,			\
			    NULL);						\
        return panel_applet_factory_main (iid, type, callback, data);		\
}
#else /* !defined(PREFIX) ... */
#define PANEL_APPLET_BONOBO_FACTORY(iid, type, name, version, callback, data)	\
int main (int argc, char *argv [])						\
{										\
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);			\
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");			\
	textdomain (GETTEXT_PACKAGE);						\
	gnome_program_init (name, version,					\
			    LIBGNOMEUI_MODULE,					\
			    argc, argv,						\
			    GNOME_CLIENT_PARAM_SM_CONNECT, FALSE,               \
			    GNOME_PARAM_NONE);					\
        return panel_applet_factory_main (iid, type, callback, data);		\
}
#endif /* defined(PREFIX) ... */
#endif /* !defined(ENABLE_NLS) */

#if !defined(ENABLE_NLS)
#define PANEL_APPLET_BONOBO_SHLIB_FACTORY(iid, type, descr, callback, data)	\
static Bonobo_Unknown								\
__panel_applet_shlib_factory (PortableServer_POA  poa,				\
			      const char         *oafiid,			\
			      gpointer            impl_ptr,			\
			      CORBA_Environment  *ev)				\
{										\
        return panel_applet_shlib_factory ((iid), (type), poa, impl_ptr,	\
					   (callback), (data), ev);		\
}										\
static BonoboActivationPluginObject plugin_list[] = {				\
	{ (iid), __panel_applet_shlib_factory },				\
	{ NULL }								\
};										\
const  BonoboActivationPlugin Bonobo_Plugin_info = { plugin_list, (descr) };
#else /* defined(ENABLE_NLS) */
#include <libintl.h>
#define PANEL_APPLET_BONOBO_SHLIB_FACTORY(iid, type, descr, callback, data)	\
static Bonobo_Unknown								\
__panel_applet_shlib_factory (PortableServer_POA  poa,				\
			      const char         *oafiid,			\
			      gpointer            impl_ptr,			\
			      CORBA_Environment  *ev)				\
{										\
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);			\
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");			\
	textdomain (GETTEXT_PACKAGE);						\
        return panel_applet_shlib_factory ((iid), (type), poa, impl_ptr,	\
					   (callback), (data), ev);		\
}										\
static BonoboActivationPluginObject plugin_list[] = {				\
	{ (iid), __panel_applet_shlib_factory },				\
	{ NULL }								\
};										\
const  BonoboActivationPlugin Bonobo_Plugin_info = { plugin_list, (descr) };
#endif /* !defined(ENABLE_NLS) */

G_END_DECLS

#endif /* __PANEL_APPLET_H__ */
