/*
 * Copyright (C) 2020 Alberts Muktupāvels
 * Copyright (C) 2021 Sebastian Geiger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GP_APPLET_ROW_H
#define GP_APPLET_ROW_H

#include "libgnome-panel/gp-applet-info-private.h"
#include "libgnome-panel/gp-module-private.h"

#include "applet.h"

G_BEGIN_DECLS

#define GP_TYPE_APPLET_LIST_ROW (gp_applet_list_row_get_type ())
G_DECLARE_FINAL_TYPE (GpAppletListRow, gp_applet_list_row, GP, APPLET_LIST_ROW, GtkListBoxRow)

GtkWidget    *gp_applet_list_row_new             (GpModule        *module,
                                                  const char      *applet_id,
                                                  AppletInfo      *info);

GpAppletInfo *gp_applet_list_row_get_info        (GpAppletListRow *self);

const char   *gp_applet_list_row_get_iid         (GpAppletListRow *self);

AppletInfo   *gp_applet_list_row_get_applet_info (GpAppletListRow *self);

G_END_DECLS

#endif
