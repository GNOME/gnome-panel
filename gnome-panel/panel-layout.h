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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#ifndef __PANEL_LAYOUT_H__
#define __PANEL_LAYOUT_H__

#include <glib.h>
#include <gdk/gdk.h>

#include "panel-enums.h"

G_BEGIN_DECLS

void panel_layout_append_from_file (const char *layout_file);

gboolean panel_layout_load         (void);

gboolean panel_layout_is_writable  (void);

void  panel_layout_toplevel_create      (GdkScreen            *screen);
void  panel_layout_object_create        (PanelObjectType       type,
                                         const char           *type_detail,
                                         const char           *toplevel_id,
                                         PanelObjectPackType   pack_type,
                                         int                   pack_index);

char      *panel_layout_object_get_gconf_path (const char *object_id);
GSettings *panel_layout_get_instance_settings (GSettings  *settings_object,
                                               const char *schema);

char *panel_layout_object_create_start  (PanelObjectType       type,
                                         const char           *type_detail,
                                         const char           *toplevel_id,
                                         PanelObjectPackType   pack_type,
                                         int                   pack_index,
                                         GSettings           **settings);
void  panel_layout_object_create_finish (const char           *object_id);

void panel_layout_delete_toplevel (const char *toplevel_id);
void panel_layout_delete_object   (const char *object_id);

G_END_DECLS

#endif /* __PANEL_LAYOUT_H__ */
