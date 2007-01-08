/*
 * panel-recent.h
 *
 * Copyright (C) 2002 James Willcox <jwillcox@gnome.org>
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
 * Authors:
 * 	James Willcox <jwillcox@gnome.org>
 */

#ifndef __PANEL_RECENT_H__
#define __PANEL_RECENT_H__

#include "gtk/gtk.h"

G_BEGIN_DECLS

void panel_recent_append_documents_menu (GtkWidget        *menu,
					 GtkRecentManager *manager);

G_END_DECLS

#endif /* __PANEL_RECENT_H__ */
