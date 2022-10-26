/*
 * panel-action-protocol.h: _GNOME_PANEL_ACTION protocol impl.
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_ACTION_PROTOCOL_H__
#define __PANEL_ACTION_PROTOCOL_H__

#include <glib.h>

#include "gp-application.h"

G_BEGIN_DECLS

#define GP_TYPE_ACTION_PROTOCOL (gp_action_protocol_get_type ())
G_DECLARE_FINAL_TYPE (GpActionProtocol, gp_action_protocol,
                      GP, ACTION_PROTOCOL, GObject)

GpActionProtocol *gp_action_protocol_new (GpApplication *application);

G_END_DECLS

#endif /* __PANEL_ACTION_PROTOCOL_H__ */
