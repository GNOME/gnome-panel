/*
 * Copyright (C) 2016 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GP_MODULE_INFO_H
#define GP_MODULE_INFO_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * GpModuleInfo:
 *
 * The #GpModuleInfo struct is an opaque data structure.
 */
typedef struct _GpModuleInfo GpModuleInfo;

GpModuleInfo *gp_module_info_new                    (const gchar  *id,
                                                     const gchar  *version);

void          gp_module_info_set_applets            (GpModuleInfo *info,
                                                     ...);

void          gp_module_info_set_translation_domain (GpModuleInfo *info,
                                                     const gchar  *domain);

G_END_DECLS

#endif
