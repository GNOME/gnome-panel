/*
 * panel-applets-manager.h
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

#ifndef __PANEL_APPLETS_MANAGER_H__
#define __PANEL_APPLETS_MANAGER_H__

#include <glib-object.h>

#include "panel-applet-frame.h"
#include "panel-applet-info.h"

G_BEGIN_DECLS

#define PANEL_TYPE_APPLETS_MANAGER		(panel_applets_manager_get_type ())
#define PANEL_APPLETS_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_APPLETS_MANAGER, PanelAppletsManager))
#define PANEL_APPLETS_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_APPLETS_MANAGER, PanelAppletsManagerClass))
#define PANEL_IS_APPLETS_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_APPLETS_MANAGER))
#define PANEL_IS_APPLETS_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_APPLETS_MANAGER))
#define PANEL_APPLETS_MANAGER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PANEL_TYPE_APPLETS_MANAGER, PanelAppletsManagerClass))

/**
 * PANEL_APPLETS_MANAGER_EXTENSION_POINT_NAME:
 *
 * Extension point for #PanelAppletsManager functionality.
 **/
#define PANEL_APPLETS_MANAGER_EXTENSION_POINT_NAME "panel-applets-manager"

typedef struct _PanelAppletsManager		PanelAppletsManager;
typedef struct _PanelAppletsManagerClass	PanelAppletsManagerClass;

struct _PanelAppletsManagerClass {
	GObjectClass parent_class;

	GList *            (*get_applets)           (PanelAppletsManager  *manager);

	gboolean           (*factory_activate)      (PanelAppletsManager  *manager,
						     const gchar          *iid);
	gboolean           (*factory_deactivate)    (PanelAppletsManager  *manager,
						     const gchar          *iid);

	PanelAppletInfo  * (*get_applet_info)       (PanelAppletsManager  *manager,
						     const gchar          *iid);

	PanelAppletInfo  * (*get_applet_info_from_old_id) (PanelAppletsManager  *manager,
							   const gchar          *iid);

	gboolean           (*load_applet)           (PanelAppletsManager         *manager,
						     const gchar                 *iid,
						     PanelAppletFrameActivating  *frame_act);
};

struct _PanelAppletsManager {
	GObject parent;
};

GType             panel_applets_manager_get_type                    (void);

GList            *panel_applets_manager_get_applets                 (void);

gboolean          panel_applets_manager_factory_activate            (const gchar     *iid);
void              panel_applets_manager_factory_deactivate          (const gchar     *iid);

PanelAppletInfo  *panel_applets_manager_get_applet_info             (const gchar     *iid);
PanelAppletInfo  *panel_applets_manager_get_applet_info_from_old_id (const gchar     *iid);

gboolean          panel_applets_manager_load_applet                 (const gchar                *iid,
								     PanelAppletFrameActivating *frame_act);

G_END_DECLS

#endif /* __PANEL_APPLETS_MANAGER_H__ */
