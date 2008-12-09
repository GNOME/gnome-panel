/* -*- mode: C; c-file-style: "linux" -*- */
/* "Show desktop" panel applet */

/* 
 * Copyright (C) 2002 Red Hat, Inc.
 * Developed by Havoc Pennington
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
 */

#ifndef _SHOWDESKTOP_H_
#define _SHOWDESKTOP_H_

#include <glib.h>
#include <panel-applet.h>

G_BEGIN_DECLS

gboolean show_desktop_applet_fill (PanelApplet *applet);

G_END_DECLS

#endif


