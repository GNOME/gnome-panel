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

G_BEGIN_DECLS
 
#define PANEL_TYPE_APPLET        (panel_applet_get_type ())
#define PANEL_APPLET(o)          (GTK_CHECK_CAST ((o), PANEL_TYPE_APPLET, PanelApplet))
#define PANEL_APPLET_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), PANEL_TYPE_APPLET, PanelAppletClass))
#define PANEL_IS_APPLET(o)       (GTK_CHECK_TYPE ((o), PANEL_TYPE_APPLET))
#define PANEL_IS_APPLET_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), PANEL_TYPE_APPLET))

typedef struct _PanelApplet        PanelApplet;
typedef struct _PanelAppletClass   PanelAppletClass;
typedef struct _PanelAppletPrivate PanelAppletPrivate;

struct _PanelApplet {
	GtkEventBox          event_box;

	PanelAppletPrivate  *priv;
};

struct _PanelAppletClass {
	GtkEventBoxClass     event_box_class;
};

GtkType            panel_applet_get_type  (void) G_GNUC_CONST;

PanelApplet       *panel_applet_new       (GtkWidget *widget);

void               panel_applet_construct (PanelApplet *applet,
					   GtkWidget   *widget);

BonoboControl     *panel_applet_get_control         (PanelApplet *applet);
BonoboUIComponent *panel_applet_get_popup_component (PanelApplet *applet);

G_END_DECLS

#endif /* __PANEL_APPLET_FRAME_H__ */
