/*
 * panel-applet.h:
 *
 * Authors:
 *   Mark McLoughlin <mark@skynet.ie>
 *
 * Copyright 2001 Sun Microsystems, Inc.
 */

#ifndef __PANEL_FRAME_H__
#define __PANEL_FRAME_H__

#include <glib/gmacros.h>
#include <gtk/gtkeventbox.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-generic-factory.h>

#include "GNOME_Panel.h"

G_BEGIN_DECLS

typedef GNOME_PanelOrient PanelAppletOrient;

#define PANEL_APPLET_ORIENT_UP    GNOME_ORIENT_UP
#define PANEL_APPLET_ORIENT_DOWN  GNOME_ORIENT_UP
#define PANEL_APPLET_ORIENT_LEFT  GNOME_ORIENT_UP
#define PANEL_APPLET_ORIENT_RIGHT GNOME_ORIENT_UP

 
#define PANEL_TYPE_APPLET         (panel_applet_get_type ())
#define PANEL_APPLET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_APPLET, PanelApplet))
#define PANEL_APPLET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_APPLET, PanelAppletClass))
#define PANEL_IS_APPLET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_APPLET))
#define PANEL_IS_APPLET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_APPLET))
#define PANEL_APPLET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_APPLET, PanelAppletClass))

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
			       gint               size);
};

GType              panel_applet_get_type  (void) G_GNUC_CONST;

PanelApplet       *panel_applet_new       (GtkWidget *widget);

void               panel_applet_construct (PanelApplet *applet,
					   GtkWidget   *widget);


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


G_END_DECLS

#endif /* __PANEL_APPLET_FRAME_H__ */
