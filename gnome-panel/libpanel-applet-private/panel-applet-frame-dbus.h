/*
 * panel-applet-frame-dbus.h: panel side container for applets
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

#ifndef __PANEL_APPLET_FRAME_DBUS_H__
#define __PANEL_APPLET_FRAME_DBUS_H__

#include <panel-applet-frame.h>

G_BEGIN_DECLS

#define PANEL_TYPE_APPLET_FRAME_DBUS         (panel_applet_frame_dbus_get_type ())
#define PANEL_APPLET_FRAME_DBUS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_APPLET_FRAME_DBUS, PanelAppletFrameDBus))
#define PANEL_APPLET_FRAME_DBUS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_APPLET_FRAME_DBUS, PanelAppletFrameDBusClass))
#define PANEL_IS_APPLET_FRAME_DBUS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_APPLET_FRAME_DBUS))
#define PANEL_IS_APPLET_FRAME_DBUS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_APPLET_FRAME_DBUS))
#define PANEL_APPLET_FRAME_DBUS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_APPLET_FRAME_DBUS, PanelAppletFrameDBusClass))

typedef struct _PanelAppletFrameDBus        PanelAppletFrameDBus;
typedef struct _PanelAppletFrameDBusClass   PanelAppletFrameDBusClass;
typedef struct _PanelAppletFrameDBusPrivate PanelAppletFrameDBusPrivate;

struct _PanelAppletFrameDBusClass {
        PanelAppletFrameClass parent_class;
};

struct _PanelAppletFrameDBus{
	PanelAppletFrame parent;

        PanelAppletFrameDBusPrivate  *priv;
};

GType     panel_applet_frame_dbus_get_type           (void) G_GNUC_CONST;

gboolean  panel_applet_frame_dbus_load               (const gchar                 *iid,
						      PanelAppletFrameActivating  *frame_act);

G_END_DECLS

#endif /* __PANEL_APPLET_FRAME_DBUS_H__ */
