/*
 * Copyright 2002 Sun Microsystems Inc.
 *
 * Gnome Panel Accessibility support module
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 */

#ifndef __PANEL_A11Y_H__
#define __PANEL_A11Y_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

void panel_a11y_set_atk_name_desc (GtkWidget  *widget,
				   const char *name, 
				   const char *desc);
void panel_a11y_set_atk_relation  (GtkWidget *widget,
				   GtkLabel  *label);

G_END_DECLS

#endif /* __PANEL_A11Y_H__ */
