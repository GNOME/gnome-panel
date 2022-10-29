/*
 * Copyright (C) 2022 Alberts Muktupāvels
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

#ifndef GP_ERROR_APPLET_H
#define GP_ERROR_APPLET_H

#include "gp-application.h"
#include "libgnome-panel/gp-applet.h"

G_BEGIN_DECLS

#define GP_TYPE_ERROR_APPLET (gp_error_applet_get_type ())
G_DECLARE_FINAL_TYPE (GpErrorApplet, gp_error_applet, GP, ERROR_APPLET, GpApplet)

GpErrorApplet *gp_error_applet_new (const char    *module_id,
                                    const char    *applet_id,
                                    GError        *error,
                                    GpApplication *application);

G_END_DECLS

#endif
