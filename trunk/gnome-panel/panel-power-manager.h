/* panel-power-manager.h - functions for powering down, restarting, and
 *                         suspending the computer
 *
 * Copyright (C) 2006 Ray Strode <rstrode@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */
#ifndef PANEL_POWER_MANAGER_H
#define PANEL_POWER_MANAGER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define PANEL_TYPE_POWER_MANAGER		(panel_power_manager_get_type ())
#define PANEL_POWER_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_POWER_MANAGER, PanelPowerManager))
#define PANEL_POWER_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_POWER_MANAGER, PanelPowerManagerClass))
#define PANEL_IS_POWER_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_POWER_MANAGER))
#define PANEL_IS_POWER_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_POWER_MANAGER))
#define PANEL_POWER_MANAGER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PANEL_TYPE_POWER_MANAGER, PanelPowerManagerClass))
#define PANEL_POWER_MANAGER_ERROR		(panel_power_manager_error_quark ())

typedef struct _PanelPowerManager		PanelPowerManager;
typedef struct _PanelPowerManagerClass		PanelPowerManagerClass;
typedef struct _PanelPowerManagerPrivate	PanelPowerManagerPrivate;
typedef enum _PanelPowerManagerError		PanelPowerManagerError;


struct _PanelPowerManager {
	GObject parent;

	/*< private > */
	PanelPowerManagerPrivate *priv;
};

struct _PanelPowerManagerClass {
	GObjectClass parent_class;

	/* signals */
	void (* request_failed) (PanelPowerManager *manager, 
				 GError *error);
};

enum _PanelPowerManagerError {
  PANEL_POWER_MANAGER_ERROR_SUSPENDING = 0,
  PANEL_POWER_MANAGER_ERROR_HIBERNATING
};

GType panel_power_manager_get_type (void);
GQuark panel_power_manager_error_quark (void);

PanelPowerManager *panel_power_manager_new (void) G_GNUC_MALLOC;

gboolean panel_power_manager_can_suspend (PanelPowerManager *manager);
gboolean panel_power_manager_can_hibernate (PanelPowerManager *manager);
gboolean panel_power_manager_can_power_down (PanelPowerManager *manager);
gboolean panel_power_manager_can_restart (PanelPowerManager *manager);

void panel_power_manager_attempt_suspend (PanelPowerManager *manager);
void panel_power_manager_attempt_hibernate (PanelPowerManager *manager);

PanelPowerManager *panel_get_power_manager (void);

G_END_DECLS

#endif /* PANEL_POWER_MANAGER_H */
