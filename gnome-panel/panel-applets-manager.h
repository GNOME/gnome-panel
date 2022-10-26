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

#include "gp-module-manager.h"
#include "libgnome-panel/gp-initial-setup-dialog-private.h"
#include "panel-applet-frame.h"

G_BEGIN_DECLS

GpAppletManager *panel_applets_manager_get                       (void);

gboolean         panel_applets_manager_open_initial_setup_dialog (const gchar                 *iid,
                                                                  GVariant                    *settings,
                                                                  GtkWindow                   *parent,
                                                                  GpInitialSetupCallback       callback,
                                                                  gpointer                     user_data,
                                                                  GDestroyNotify               free_func);

G_END_DECLS

#endif /* __PANEL_APPLETS_MANAGER_H__ */
