/*
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

 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_STRUTS_H__
#define __PANEL_STRUTS_H__

#include <glib.h>
#include "panel-toplevel.h"

G_BEGIN_DECLS

gboolean panel_struts_register_strut           (PanelToplevel    *toplevel,
						GdkScreen        *screen,
						int               monitor,
						PanelOrientation  orientation,
						int               strut_size,
						int               strut_start,
						int               strut_end);

void     panel_struts_unregister_strut         (PanelToplevel    *toplevel);

void     panel_struts_set_window_hint          (PanelToplevel    *toplevel);
void     panel_struts_unset_window_hint        (PanelToplevel    *toplevel);

gboolean panel_struts_update_toplevel_geometry (PanelToplevel    *toplevel,
						int              *x,
						int              *y,
						int              *w,
						int              *h);

G_END_DECLS

#endif /* __PANEL_STRUTS_H__ */
