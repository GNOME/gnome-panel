/* wncklet.h
 *
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Arvind Samptur <arvind.samptur@wipro.com>
 *
 */

#ifndef __WNCKLET_H__
#define __WNCKLET_H__

#include <glib.h>

G_BEGIN_DECLS

void wncklet_connect_while_alive (gpointer    object,
			          const char *signal,
			          GCallback   func,
			          gpointer    func_data,
			          gpointer    alive_object);
G_END_DECLS

#endif
