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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PANEL_APPLETS_MANAGER_H__
#define __PANEL_APPLETS_MANAGER_H__

#include <glib-object.h>

#include "libgnome-panel/gp-initial-setup-dialog-private.h"
#include "panel-applet-frame.h"
#include "panel-applet-info.h"

G_BEGIN_DECLS

#define PANEL_TYPE_APPLETS_MANAGER		(panel_applets_manager_get_type ())
#define PANEL_APPLETS_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_APPLETS_MANAGER, PanelAppletsManager))
#define PANEL_APPLETS_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_APPLETS_MANAGER, PanelAppletsManagerClass))
#define PANEL_IS_APPLETS_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_APPLETS_MANAGER))
#define PANEL_IS_APPLETS_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_APPLETS_MANAGER))
#define PANEL_APPLETS_MANAGER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PANEL_TYPE_APPLETS_MANAGER, PanelAppletsManagerClass))

typedef struct _PanelAppletsManager		PanelAppletsManager;
typedef struct _PanelAppletsManagerClass	PanelAppletsManagerClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (PanelAppletsManager, g_object_unref)

struct _PanelAppletsManagerClass {
	GObjectClass parent_class;

	GList *            (*get_applets)           (PanelAppletsManager  *manager);

	gboolean           (*factory_activate)      (PanelAppletsManager  *manager,
						     const gchar          *iid);
	gboolean           (*factory_deactivate)    (PanelAppletsManager  *manager,
						     const gchar          *iid);

	PanelAppletInfo  * (*get_applet_info)       (PanelAppletsManager  *manager,
						     const gchar          *iid);

	gboolean           (*load_applet)           (PanelAppletsManager         *manager,
						     const gchar                 *iid,
						     PanelAppletFrameActivating  *frame_act);

	GtkWidget        * (*get_applet_widget)     (PanelAppletsManager         *manager,
	                                             const gchar                 *iid,
	                                             guint                        uid);

	gchar            * (*get_new_iid)           (PanelAppletsManager         *manager,
	                                             const gchar                 *old_iid);

	gboolean           (*open_initial_setup_dialog) (PanelAppletsManager         *manager,
	                                                 const gchar                 *iid,
	                                                 GtkWindow                   *parent,
	                                                 GpInitialSetupCallback       callback,
	                                                 gpointer                     user_data,
	                                                 GDestroyNotify               free_func);
};

struct _PanelAppletsManager {
	GObject parent;
};

GType             panel_applets_manager_get_type                    (void);

GList            *panel_applets_manager_get_applets                 (void);

gboolean          panel_applets_manager_factory_activate            (const gchar     *iid);
void              panel_applets_manager_factory_deactivate          (const gchar     *iid);

PanelAppletInfo  *panel_applets_manager_get_applet_info             (const gchar     *iid);

gboolean          panel_applets_manager_load_applet                 (const gchar                *iid,
								     PanelAppletFrameActivating *frame_act);

GtkWidget        *panel_applets_manager_get_applet_widget           (const gchar     *iid,
                                                                     guint            uid);

gchar            *panel_applets_manager_get_new_iid                 (const gchar     *old_iid);

gboolean          panel_applets_manager_open_initial_setup_dialog   (const gchar            *iid,
                                                                     GtkWindow              *parent,
                                                                     GpInitialSetupCallback  callback,
                                                                     gpointer                user_data,
                                                                     GDestroyNotify          free_func);

G_END_DECLS

#endif /* __PANEL_APPLETS_MANAGER_H__ */
