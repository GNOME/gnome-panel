/*
 * panel-applet-frame.h:
 *
 * Authors:
 *    Mark McLoughlin <mark@skynet.ie>
 *
 * Copyright 2001 Sun Microsystems, Inc.
 */

#ifndef __PANEL_APPLET_CONTROL_H__
#define __PANEL_APPLET_CONTROL_H__

#include <gtk/gtkeventbox.h>

#include "panel-widget.h"
#include "applet.h"

G_BEGIN_DECLS

#define PANEL_TYPE_APPLET_FRAME         (panel_applet_frame_get_type ())
#define PANEL_APPLET_FRAME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_APPLET_FRAME, PanelAppletFrame))
#define PANEL_APPLET_FRAME_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_APPLET_FRAME, PanelAppletFrameClass))
#define PANEL_IS_APPLET_FRAME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_APPLET_FRAME))
#define PANEL_IS_APPLET_FRAME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_APPLET_FRAME))
#define PANEL_APPLET_FRAME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_APPLET_FRAME, PanelAppletFrameClass))

typedef struct _PanelAppletFrame        PanelAppletFrame;
typedef struct _PanelAppletFrameClass   PanelAppletFrameClass;
typedef struct _PanelAppletFramePrivate PanelAppletFramePrivate;

struct _PanelAppletFrame{
        GtkEventBox               event_box;

        PanelAppletFramePrivate  *priv;
};

struct _PanelAppletFrameClass {
        GtkEventBoxClass          event_box_class;
};

GType      panel_applet_frame_get_type  (void) G_GNUC_CONST;

GtkWidget *panel_applet_frame_new       (const gchar *iid);

void       panel_applet_frame_construct (PanelAppletFrame *frame,
					 const gchar      *iid);

void       panel_applet_frame_set_info  (PanelAppletFrame *frame,
					 AppletInfo       *info);
					
void       panel_applet_frame_load      (const gchar *iid,
					 PanelWidget *panel,
					 gint         pos);

void       panel_applet_frame_change_orient (PanelAppletFrame *frame,
					     PanelOrient       orient);

void       panel_applet_frame_change_size   (PanelAppletFrame *frame,
					     PanelSize         size);

void       panel_applet_frame_change_background_pixmap (PanelAppletFrame *frame,
							gchar            *pixmap);

void       panel_applet_frame_change_background_color  (PanelAppletFrame *frame,
							guint16           red,
							guint16           green,
							guint16           blue);

void       panel_applet_frame_clear_background         (PanelAppletFrame *frame);

void       panel_applet_frame_save_position            (PanelAppletFrame *frame,
							const gchar      *base_key);

void       panel_applet_frame_save_session             (PanelAppletFrame *frame,
							const gchar      *base_key);

G_END_DECLS

#endif /* __PANEL_APPLET_CONTROL_H__ */
