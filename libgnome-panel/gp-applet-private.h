/*
 * Copyright (C) 2001 Sun Microsystems, Inc.
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     Mark McLoughlin <mark@skynet.ie>
 */

#ifndef GP_APPLET_PRIVATE_H
#define GP_APPLET_PRIVATE_H

#include <libgnome-panel/gp-applet.h>

G_BEGIN_DECLS

void           gp_applet_set_locked_down (GpApplet        *applet,
                                          gboolean         locked_down);

void           gp_applet_set_orientation (GpApplet        *applet,
                                          GtkOrientation   orientation);

void           gp_applet_set_position    (GpApplet        *applet,
                                          GtkPositionType  position);

GpAppletFlags  gp_applet_get_flags       (GpApplet        *applet);

gint          *gp_applet_get_size_hints  (GpApplet        *applet,
                                          guint           *n_elements);

GtkWidget     *gp_applet_get_menu        (GpApplet        *applet);

G_END_DECLS

#endif
