/*
 * panel-applet-private.h:
 *
 * Copyright (C) 2001 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *     Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_APPLET_PRIVATE_H__
#define __PANEL_APPLET_PRIVATE_H__

#include "panel-applet.h"

G_BEGIN_DECLS

gboolean _panel_applet_popup_menu (PanelApplet *applet, guint button, guint32 time);

G_END_DECLS

#endif /* __PANEL_APPLET_PRIVATE_H__ */
