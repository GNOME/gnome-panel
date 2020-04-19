/*
 * panel-layout.h:
 *
 * Copyright (C) 2011 Novell, Inc.
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
 *	Vincent Untz <vuntz@gnome.org>
 */

#ifndef __PANEL_LAYOUT_H__
#define __PANEL_LAYOUT_H__

#include <glib.h>
#include <gdk/gdk.h>

#include "panel-enums-gsettings.h"

G_BEGIN_DECLS

gboolean panel_layout_load         (void);

gboolean panel_layout_is_writable  (void);

void  panel_layout_toplevel_create      (GdkScreen            *screen);
void  panel_layout_object_create        (const char           *iid,
                                         const char           *toplevel_id,
                                         PanelObjectPackType   pack_type,
                                         int                   pack_index,
                                         GVariant             *initial_settings);

char *panel_layout_object_create_start  (const char           *iid,
                                         const char           *toplevel_id,
                                         PanelObjectPackType   pack_type,
                                         int                   pack_index,
                                         GVariant             *initial_settings);
void  panel_layout_object_create_finish (const char           *object_id);

void panel_layout_delete_toplevel (const char *toplevel_id);
void panel_layout_delete_object   (const char *object_id);

G_END_DECLS

#endif /* __PANEL_LAYOUT_H__ */
