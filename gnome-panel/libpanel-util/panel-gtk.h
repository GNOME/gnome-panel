/*
 * panel-gtk.h: various small extensions to gtk+
 *
 * Copyright (C) 2009-2010 Novell, Inc.
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

#ifndef PANEL_GTK_H
#define PANEL_GTK_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PANEL_GTK_BUILDER_GET(builder, name) GTK_WIDGET (gtk_builder_get_object (builder, name))

void  panel_gtk_file_chooser_add_image_preview (GtkFileChooser *chooser);
char *panel_gtk_get_bookmark_file (void);

G_END_DECLS

#endif /* PANEL_GTK_H */
