/*
 * panel-applet-gconf.h: panel applet preferences handling.
 *
 * Copyright (C) 2001 Sun Microsystems, Inc.
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
 *     Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_APPLET_GCONF_H__
#define __PANEL_APPLET_GCONF_H__

#include <glib/gmacros.h>
#include <glib/gerror.h>
#include <gconf/gconf-value.h>

#include <panel-applet.h>

G_BEGIN_DECLS

void         panel_applet_gconf_set_bool        (PanelApplet  *applet,
						 const gchar  *key,
						 gboolean      the_bool,
						 GError      **error);

void         panel_applet_gconf_set_int         (PanelApplet  *applet,
						 const gchar  *key,
						 gint          the_int,
						 GError      **error);

void         panel_applet_gconf_set_string      (PanelApplet  *applet,
						 const gchar  *key,
						 const gchar  *the_string,
						 GError      **error);

void         panel_applet_gconf_set_float       (PanelApplet  *applet,
						 const gchar  *key,
						 gdouble       the_float,
						 GError      **error);

void         panel_applet_gconf_set_value       (PanelApplet  *applet,
						 const gchar  *key,
						 GConfValue   *value,
						 GError      **error);


gboolean     panel_applet_gconf_get_bool        (PanelApplet  *applet,
						 const gchar  *key,
						 GError      **error);

gint         panel_applet_gconf_get_int         (PanelApplet  *applet,
						 const gchar  *key,
						 GError      **error);

gchar       *panel_applet_gconf_get_string      (PanelApplet  *applet,
						 const gchar  *key,
						 GError      **error);

gdouble      panel_applet_gconf_get_float       (PanelApplet  *applet,
						 const gchar  *key,
						 GError      **error);

GConfValue  *panel_applet_gconf_get_value       (PanelApplet  *applet,
						 const gchar  *key,
						 GError      **error);


G_END_DECLS

#endif /* __PANEL_APPLET_GCONF_H__ */
