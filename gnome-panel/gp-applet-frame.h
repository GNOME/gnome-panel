/*
 * Copyright (C) 2016 Alberts Muktupāvels
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

#ifndef GP_APPLET_FRAME_H
#define GP_APPLET_FRAME_H

#include <libgnome-panel/gp-applet-private.h>
#include <panel-applet-frame.h>

G_BEGIN_DECLS

#define GP_TYPE_APPLET_FRAME gp_applet_frame_get_type ()
G_DECLARE_FINAL_TYPE (GpAppletFrame, gp_applet_frame,
                      GP, APPLET_FRAME, PanelAppletFrame)

void gp_applet_frame_set_applet (GpAppletFrame *frame,
                                 GpApplet      *applet);

G_END_DECLS

#endif
