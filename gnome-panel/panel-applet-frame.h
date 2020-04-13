/*
 * panel-applet-frame.h: panel side container for applets
 *
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
 * Copyright (C) 2016 - 2020 Alberts Muktupāvels
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_APPLET_FRAME_H__
#define __PANEL_APPLET_FRAME_H__

#include <gtk/gtk.h>

#include "libgnome-panel/gp-applet-private.h"
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

G_DEFINE_AUTOPTR_CLEANUP_FUNC (PanelAppletFrame, g_object_unref)

struct _PanelAppletFrameClass {
        GtkEventBoxClass parent_class;

	void (*move_focus_out_of_applet) (PanelAppletFrame *frame,
	                                  GtkDirectionType  direction);
};

struct _PanelAppletFrame {
	GtkEventBox parent;

        PanelAppletFramePrivate  *priv;
};

GType panel_applet_frame_get_type           (void) G_GNUC_CONST;

void  panel_applet_frame_create             (PanelToplevel       *toplevel,
					     PanelObjectPackType  pack_type,
					     int                  pack_index,
					     const char          *iid,
					     GVariant            *initial_settings);

void  panel_applet_frame_load               (PanelWidget         *panel_widget,
					     const char          *id,
					     GSettings           *settings);

void  panel_applet_frame_change_orientation (PanelAppletFrame    *frame,
					     PanelOrientation     orientation);

void  panel_applet_frame_set_panel          (PanelAppletFrame    *frame,
					     PanelWidget         *panel);


/* For module implementations only */

typedef struct _PanelAppletFrameActivating        PanelAppletFrameActivating;

PanelOrientation  panel_applet_frame_activating_get_orientation (PanelAppletFrameActivating *frame_act);
gboolean          panel_applet_frame_activating_get_locked_down (PanelAppletFrameActivating *frame_act);
gchar            *panel_applet_frame_activating_get_settings_path (PanelAppletFrameActivating *frame_act);
gchar            *panel_applet_frame_activating_get_initial_settings_path (PanelAppletFrameActivating *frame_act);

void  _panel_applet_frame_set_applet            (PanelAppletFrame           *self,
                                                 GpApplet                   *applet);

void  _panel_applet_frame_set_iid               (PanelAppletFrame           *frame,
						 const gchar                *iid);

void  _panel_applet_frame_activated             (PanelAppletFrame           *frame,
						 PanelAppletFrameActivating *frame_act,
						 GError                     *error);

G_END_DECLS

#endif /* __PANEL_APPLET_FRAME_H__ */
