/*
 * panel-cleanup.h: utility to clean up things on exit
 *
 * Copyright (C) 2008 Novell, Inc.
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

#ifndef PANEL_CLEANUP_H
#define PANEL_CLEANUP_H

#include "glib.h"

G_BEGIN_DECLS

#define PANEL_CLEAN_FUNC(f)     ((PanelCleanFunc) (f))

typedef void  (*PanelCleanFunc) (gpointer data);

void panel_cleanup_unref_and_nullify (gpointer data);

void panel_cleanup_do         (void);

void panel_cleanup_register   (PanelCleanFunc func,
			       gpointer       data);

G_END_DECLS

#endif /* PANEL_CLEANUP_H */
