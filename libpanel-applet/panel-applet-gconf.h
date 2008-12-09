/*
 * panel-applet-gconf.h: panel applet preferences handling.
 *
 * Copyright (C) 2001-2003 Sun Microsystems, Inc.
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

#include <glib.h>
#include <gconf/gconf-value.h>

#include <panel-applet.h>

G_BEGIN_DECLS

gchar       *panel_applet_gconf_get_full_key (PanelApplet     *applet,
					      const gchar     *key);

void         panel_applet_gconf_set_bool     (PanelApplet     *applet,
					      const gchar     *key,
					      gboolean         the_bool,
					      GError         **opt_error);
void         panel_applet_gconf_set_int      (PanelApplet     *applet,
					      const gchar     *key,
					      gint             the_int,
					      GError         **opt_error);
void         panel_applet_gconf_set_string   (PanelApplet     *applet,
					      const gchar     *key,
					      const gchar     *the_string,
					      GError         **opt_error);
void         panel_applet_gconf_set_float    (PanelApplet     *applet,
					      const gchar     *key,
					      gdouble          the_float,
					      GError         **opt_error);
void         panel_applet_gconf_set_list     (PanelApplet     *applet,
					      const gchar     *key,
					      GConfValueType   list_type,
					      GSList          *list,
					      GError         **opt_error);
void         panel_applet_gconf_set_value    (PanelApplet     *applet,
					      const gchar     *key,
					      GConfValue      *value,
					      GError         **opt_error);

gboolean     panel_applet_gconf_get_bool     (PanelApplet     *applet,
					      const gchar     *key,
					      GError         **opt_error);
gint         panel_applet_gconf_get_int      (PanelApplet     *applet,
					      const gchar     *key,
					      GError         **opt_error);
gchar       *panel_applet_gconf_get_string   (PanelApplet     *applet,
					      const gchar     *key,
					      GError         **opt_error);
gdouble      panel_applet_gconf_get_float    (PanelApplet     *applet,
					      const gchar     *key,
					      GError         **opt_error);
GSList      *panel_applet_gconf_get_list     (PanelApplet     *applet,
					      const gchar     *key,
					      GConfValueType   list_type,
					      GError         **opt_error);
GConfValue  *panel_applet_gconf_get_value    (PanelApplet     *applet,
					      const gchar     *key,
					      GError         **opt_error);

G_END_DECLS

#endif /* __PANEL_APPLET_GCONF_H__ */
