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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#ifndef __PANEL_OBJECT_LOADER_H__
#define __PANEL_OBJECT_LOADER_H__

#include "gp-application.h"

G_BEGIN_DECLS

void     panel_object_loader_queue         (const char *id,
                                            const char *settings_path);

void     panel_object_loader_do_load       (GpApplication *application,
                                            gboolean       initial_load);

gboolean panel_object_loader_is_queued     (const char *id);

void     panel_object_loader_stop_loading  (GpApplication *application,
                                            const char    *id);

G_END_DECLS

#endif /* __PANEL_OBJECT_LOADER_H__ */
