/*
 * Copyright (C) 2015 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SN_APPLET_H
#define SN_APPLET_H

#include <panel-applet.h>

G_BEGIN_DECLS

#define SN_TYPE_APPLET            (sn_applet_get_type())
#define SN_APPLET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SN_TYPE_APPLET, SnApplet))
#define SN_APPLET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  SN_TYPE_APPLET, SnAppletClass))
#define SN_IS_APPLET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SN_TYPE_APPLET))
#define SN_IS_APPLET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  SN_TYPE_APPLET))
#define SN_APPLET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),   SN_TYPE_APPLET, SnAppletClass))

typedef struct _SnApplet      SnApplet;
typedef struct _SnAppletClass SnAppletClass;

struct _SnAppletClass
{
  PanelAppletClass parent_class;
};

GType sn_applet_get_type (void);

G_END_DECLS

#endif
