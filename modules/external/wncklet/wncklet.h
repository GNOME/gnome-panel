/*
 * Copyright (C) 2003  Wipro Technologies
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Arvind Samptur <arvind.samptur@wipro.com>
 */

#ifndef WNCKLET_H
#define WNCKLET_H

#include <glib.h>
#include <gtk/gtk.h>
#include <libwnck/libwnck.h>

#define WNCKLET_RESOURCE_PATH "/org/gnome/panel/applet/wncklet/"

G_BEGIN_DECLS

void        wncklet_connect_while_alive (gpointer    object,
					 const char *signal,
					 GCallback   func,
					 gpointer    func_data,
					 gpointer    alive_object);

G_END_DECLS

#endif
