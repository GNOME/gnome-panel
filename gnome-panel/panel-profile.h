/*
 * panel-profile.h:
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_PROFILE_H__
#define __PANEL_PROFILE_H__

#include <glib/gmacros.h>
#include <gconf/gconf-client.h>

#include "panel-toplevel.h"
#include "panel-enums.h"

G_BEGIN_DECLS

void        panel_profile_load     (char *profile_name);
const char *panel_profile_get_name (void);


const char    *panel_profile_get_toplevel_id    (PanelToplevel     *toplevel);
PanelToplevel *panel_profile_get_toplevel_by_id (const char        *toplevel_id);
char          *panel_profile_find_new_id        (PanelGConfKeyType  type,
						 GSList            *existing_ids);


gboolean    panel_profile_get_show_program_list   (void);
void        panel_profile_set_show_program_list   (gboolean show_program_list);
gboolean    panel_profile_get_enable_program_list (void);


void        panel_profile_create_toplevel (void);
void        panel_profile_delete_toplevel (PanelToplevel *toplevel);

gboolean panel_profile_is_toplevel_list_writeable (void);

void panel_profile_set_toplevel_name              (PanelToplevel *toplevel,
						   const char    *name);
void panel_profile_set_toplevel_expand            (PanelToplevel *toplevel,
						   gboolean       expand);
void panel_profile_set_toplevel_auto_hide         (PanelToplevel *toplevel,
						   gboolean       autohide);
void panel_profile_set_toplevel_enable_buttons    (PanelToplevel *toplevel,
						   gboolean       enable_buttons);
void panel_profile_set_toplevel_enable_arrows     (PanelToplevel *toplevel,
						   gboolean       enable_arrows);
void panel_profile_set_toplevel_enable_animations (PanelToplevel *toplevel,
						   gboolean       enable_animations);

void panel_profile_toplevel_notify_add (PanelToplevel         *toplevel,
                                        const char            *key,
                                        GConfClientNotifyFunc  func,
                                        GObject               *object);

G_END_DECLS

#endif /* __PANEL_PROFILE_H__ */
