/*
 * panel-config-global.h: panel global configuration module
 *
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
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
 *      Mark McLoughlin <mark@skynet.ie>
 *	Glynn Foster <glynn.foster@sun.com>
 */

#ifndef __PANEL_CONFIG_GLOBAL_H__
#define __PANEL_CONFIG_GLOBAL_H__

#include <glib.h>

G_BEGIN_DECLS

void panel_global_config_load (void);

gboolean panel_global_config_get_highlight_when_over  (void);
gboolean panel_global_config_get_enable_animations    (void);
gboolean panel_global_config_get_drawer_auto_close    (void);
gboolean panel_global_config_get_tooltips_enabled     (void);
gboolean panel_global_config_get_confirm_panel_remove (void);

G_END_DECLS

#endif /* __PANEL_CONFIG_GLOBAL_H__ */
