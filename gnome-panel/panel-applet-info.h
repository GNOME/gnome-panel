/*
 * panel-applet-info.h
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2010 Vincent Untz <vuntz@gnome.org>
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

#ifndef __PANEL_APPLET_INFO_H__
#define __PANEL_APPLET_INFO_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _PanelAppletInfo PanelAppletInfo;

PanelAppletInfo *panel_applet_info_new                             (const gchar  *iid,
								    const gchar  *name,
								    const gchar  *comment,
								    const gchar  *icon,
								    const gchar **old_ids);
void             panel_applet_info_free                            (PanelAppletInfo *info);

const gchar     *panel_applet_info_get_iid                         (PanelAppletInfo *info);
const gchar     *panel_applet_info_get_name                        (PanelAppletInfo *info);
const gchar     *panel_applet_info_get_description                 (PanelAppletInfo *info);
const gchar     *panel_applet_info_get_icon                        (PanelAppletInfo *info);
const gchar * const *panel_applet_info_get_old_ids                 (PanelAppletInfo *info);

G_END_DECLS

#endif /* __PANEL_APPLET_INFO_H__ */
