/*
 * panel-applet-frame.h: panel side container for applets
 *
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_APPLET_FRAME_H__
#define __PANEL_APPLET_FRAME_H__

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

GtkWidget *panel_applet_frame_new       (PanelWidget *panel,
					 const char  *iid,
					 const char  *id);

GtkWidget *panel_applet_frame_construct (PanelAppletFrame *frame,
					 PanelWidget      *panel,
					 const char       *iid,
					 const char       *id);

void       panel_applet_frame_set_info  (PanelAppletFrame *frame,
					 AppletInfo       *info);
					
void       panel_applet_frame_create    (PanelToplevel    *toplevel,
					 int               position,
					 const char       *iid);

void       panel_applet_frame_change_orientation (PanelAppletFrame *frame,
						  PanelOrientation  orientation);

void       panel_applet_frame_change_size   (PanelAppletFrame *frame,
					     PanelSize         size);

void       panel_applet_frame_change_background (PanelAppletFrame    *frame,
						 PanelBackgroundType  type);

void       panel_applet_frame_load_from_gconf   (PanelWidget *panel_widget,
						 gboolean     locked,
						 int          position,
						 const char  *id);

int        panel_applet_frame_get_size_hints    (PanelAppletFrame  *frame,
						 int              **size_hints);

void            panel_applet_frame_set_panel (PanelAppletFrame *frame,
					      PanelWidget      *panel);
PanelWidget    *panel_applet_frame_get_panel (PanelAppletFrame *frame);


G_END_DECLS

#endif /* __PANEL_APPLET_FRAME_H__ */
