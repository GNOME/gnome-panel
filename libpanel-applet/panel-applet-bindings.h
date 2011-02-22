/*
 * panel-applet-bindings.h: utility for key bindings.
 *
 * Copyright (c) 2011 Novell, Inc.
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
 *     Vincent Untz <vuntz@gnome.org>
 */

#ifndef __PANEL_APPLET_BINDINGS_H__
#define __PANEL_APPLET_BINDINGS_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gconf/gconf-client.h>

G_BEGIN_DECLS

void  panel_applet_bindings_init  (GConfClient *client);
void  panel_applet_bindings_clean (GConfClient *client);

guint panel_applet_bindings_get_mouse_button_modifier_keymask (void);

void panel_applet_bindings_key_event_is_popup (GdkEventKey *event,
					       gboolean    *is_popup,
					       gboolean    *is_popup_modifier);

G_END_DECLS

#endif /* __PANEL_APPLET_BINDINGS_H__ */
