/*
 * panel-applet-frame-bonobo.h: panel side container for applets
 *
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
 * Copyright (C) 2010 Vincent Untz <vuntz@gnome.org>
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

#ifndef __PANEL_APPLET_FRAME_BONOBO_H__
#define __PANEL_APPLET_FRAME_BONOBO_H__

#include <panel-applet-frame.h>

G_BEGIN_DECLS

#define PANEL_TYPE_APPLET_FRAME_BONOBO         (panel_applet_frame_bonobo_get_type ())
#define PANEL_APPLET_FRAME_BONOBO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_APPLET_FRAME_BONOBO, PanelAppletFrameBonobo))
#define PANEL_APPLET_FRAME_BONOBO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_APPLET_FRAME_BONOBO, PanelAppletFrameBonoboClass))
#define PANEL_IS_APPLET_FRAME_BONOBO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_APPLET_FRAME_BONOBO))
#define PANEL_IS_APPLET_FRAME_BONOBO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_APPLET_FRAME_BONOBO))
#define PANEL_APPLET_FRAME_BONOBO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_APPLET_FRAME_BONOBO, PanelAppletFrameBonoboClass))

typedef struct _PanelAppletFrameBonobo        PanelAppletFrameBonobo;
typedef struct _PanelAppletFrameBonoboClass   PanelAppletFrameBonoboClass;
typedef struct _PanelAppletFrameBonoboPrivate PanelAppletFrameBonoboPrivate;

struct _PanelAppletFrameBonoboClass {
        PanelAppletFrameClass parent_class;
};

struct _PanelAppletFrameBonobo{
	PanelAppletFrame parent;

        PanelAppletFrameBonoboPrivate  *priv;
};

GType     panel_applet_frame_bonobo_get_type           (void) G_GNUC_CONST;

gboolean  panel_applet_frame_bonobo_load               (const gchar                 *iid,
							PanelAppletFrameActivating  *frame_act);

G_END_DECLS

#endif /* __PANEL_APPLET_FRAME_BONOBO_H__ */
