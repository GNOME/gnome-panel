/*
 * panel-applets-manager-dbus.h
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
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
 */

#ifndef __PANEL_APPLETS_MANAGER_DBUS_H__
#define __PANEL_APPLETS_MANAGER_DBUS_H__

#include <panel-applets-manager.h>

G_BEGIN_DECLS

#define PANEL_TYPE_APPLETS_MANAGER_DBUS			(panel_applets_manager_dbus_get_type ())
#define PANEL_APPLETS_MANAGER_DBUS(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_APPLETS_MANAGER_DBUS, PanelAppletsManagerDBus))
#define PANEL_APPLETS_MANAGER_DBUS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_APPLETS_MANAGER_DBUS, PanelAppletsManagerDBusClass))
#define PANEL_IS_APPLETS_MANAGER_DBUS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_APPLETS_MANAGER_DBUS))
#define PANEL_IS_APPLETS_MANAGER_DBUS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_APPLETS_MANAGER_DBUS))
#define PANEL_APPLETS_MANAGER_DBUS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PANEL_TYPE_APPLETS_MANAGER_DBUS, PanelAppletsManagerDBusClass))

typedef struct _PanelAppletsManagerDBus		PanelAppletsManagerDBus;
typedef struct _PanelAppletsManagerDBusClass	PanelAppletsManagerDBusClass;
typedef struct _PanelAppletsManagerDBusPrivate	PanelAppletsManagerDBusPrivate;

struct _PanelAppletsManagerDBusClass {
	PanelAppletsManagerClass parent_class;
};

struct _PanelAppletsManagerDBus {
	PanelAppletsManager parent;

	/*< private > */
	PanelAppletsManagerDBusPrivate *priv;
};

GType panel_applets_manager_dbus_get_type (void);

G_END_DECLS

#endif /* __PANEL_APPLETS_MANAGER_DBUS_H__ */
