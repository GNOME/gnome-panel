/*
 * panel-applets-manmanger.h
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include <glib.h>

G_BEGIN_DECLS

typedef struct _PanelAppletInfo PanelAppletInfo;

gboolean         panel_applets_manager_init                        (void);
void             panel_applets_manager_shutdown                    (void);
GList           *panel_applets_manager_get_applets                 (void);

gboolean         panel_applets_manager_factory_activate            (const gchar     *iid);
void             panel_applets_manager_factory_deactivate          (const gchar     *iid);

gboolean         panel_applets_manager_is_factory_in_process       (const gchar     *iid);
PanelAppletInfo *panel_applets_manager_get_applet_info             (const gchar     *iid);
PanelAppletInfo *panel_applets_manager_get_applet_info_from_old_id (const gchar     *iid);

const gchar     *panel_applet_info_get_iid                         (PanelAppletInfo *info);
const gchar     *panel_applet_info_get_name                        (PanelAppletInfo *info);
const gchar     *panel_applet_info_get_description                 (PanelAppletInfo *info);
const gchar     *panel_applet_info_get_icon                        (PanelAppletInfo *info);

G_END_DECLS

#endif /* __PANEL_APPLETS_MANAGER_H__ */
