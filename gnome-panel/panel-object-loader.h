/*
 * panel-object-loader.h: object loader
 * vim: set et:
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

#ifndef __PANEL_OBJECT_LOADER_H__
#define __PANEL_OBJECT_LOADER_H__

#include <glib.h>

G_BEGIN_DECLS

void     panel_object_loader_queue         (const char *id,
                                            const char *settings_path);
void     panel_object_loader_do_load       (gboolean initial_load);

gboolean panel_object_loader_is_queued     (const char *id);
void     panel_object_loader_stop_loading  (const char *id);

/*******************************\
 * iid <=> object type mapping *
\*******************************/

char     *panel_object_type_to_iid (PanelObjectType  type,
                                    const char      *detail);

gboolean  panel_object_iid_to_type (const char       *iid,
                                    PanelObjectType  *type,
                                    char            **detail);

G_END_DECLS

#endif /* __PANEL_OBJECT_LOADER_H__ */
