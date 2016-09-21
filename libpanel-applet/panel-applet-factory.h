/*
 * panel-applet-factory.h: panel applet writing API.
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PANEL_APPLET_FACTORY_H
#define PANEL_APPLET_FACTORY_H

#include <glib-object.h>

#include "panel-applet.h"

G_BEGIN_DECLS

#define PANEL_TYPE_APPLET_FACTORY     (panel_applet_factory_get_type ())
#define PANEL_APPLET_FACTORY(o)       (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_APPLET_FACTORY, PanelAppletFactory))
#define PANEL_APPLET_FACTORY_CLASS(c) (G_TYPE_CHECK_CLASS_CAST((c),     PANEL_TYPE_APPLET_FACTORY, PanelAppletFactoryClass))
#define PANEL_IS_APPLET_FACTORY(o)    (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_APPLET_FACTORY))

typedef struct _PanelAppletFactory      PanelAppletFactory;
typedef struct _PanelAppletFactoryClass PanelAppletFactoryClass;

GType               panel_applet_factory_get_type          (void) G_GNUC_CONST;
PanelAppletFactory *panel_applet_factory_new               (const gchar        *applet_id,
                                                            GType               applet_type,
                                                            GClosure           *closure);
gboolean            panel_applet_factory_register_service  (PanelAppletFactory *factory);
GtkWidget          *panel_applet_factory_get_applet_widget (const gchar        *id,
                                                            guint               uid);
G_END_DECLS

#endif /* PANEL_APPLET_FACTORY_H */
