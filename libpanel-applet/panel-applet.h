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
#include <gtk/gtkeventbox.h>
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
	PANEL_PIXMAP_BACKGOUND
} PanelAppletBackgroundType;

typedef struct _PanelApplet        PanelApplet;
typedef struct _PanelAppletClass   PanelAppletClass;
typedef struct _PanelAppletPrivate PanelAppletPrivate;

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
				   const gchar               *pixmap);
};

GType              panel_applet_get_type  (void) G_GNUC_CONST;

GtkWidget         *panel_applet_new       (GtkWidget *widget);

void               panel_applet_construct (PanelApplet *applet,
					   GtkWidget   *widget);

PanelAppletOrient panel_applet_get_orient (PanelApplet *applet);

guint              panel_applet_get_size  (PanelApplet *applet);

gchar             *panel_applet_get_global_key  (PanelApplet *applet);
gchar             *panel_applet_get_private_key (PanelApplet *applet);

void      	   panel_applet_get_expand_flags (PanelApplet *applet,
						  gboolean    *expand_major,
						  gboolean    *expand_minor);
void      	   panel_applet_set_expand_flags (PanelApplet *applet,
						  gboolean     expand_major,
						  gboolean     expand_minor);

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


int                panel_applet_factory_main         (int                     argc,
						      char                  **argv,
						      const gchar            *iid,
						      const gchar            *name,
						      const gchar            *version,
						      BonoboFactoryCallback   callback,
						      gpointer                data);

#define PANEL_APPLET_BONOBO_FACTORY(iid, name, version, callback, data)		\
int main (int argc, char *argv [])						\
{										\
        return panel_applet_factory_main (argc, argv, iid, name,		\
					  version, callback, data);		\
}

#define PANEL_APPLET_BONOBO_SHLIB_FACTORY(iid, descr, callback, data)		\
		BONOBO_ACTIVATION_SHLIB_FACTORY(iid, descr, callback, data)


G_END_DECLS

#endif /* __PANEL_APPLET_H__ */
