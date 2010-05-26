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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef PANEL_APPLET_FACTORY_H
#define PANEL_APPLET_FACTORY_H

#include <glib-object.h>

#include "panel-applet.h"

G_BEGIN_DECLS

#define PANEL_TYPE_APPLET_FACTORY            (panel_applet_factory_get_type ())
#define PANEL_APPLET_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_APPLET_FACTORY, PanelAppletFactory))
#define PANEL_APPLET_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PANEL_TYPE_APPLET_FACTORY, PanelAppletFactoryClass))
#define PANEL_IS_APPLET_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_APPLET_FACTORY))

typedef struct _PanelAppletFactory        PanelAppletFactory;
typedef struct _PanelAppletFactoryClass   PanelAppletFactoryClass;

GType               panel_applet_factory_get_type         (void) G_GNUC_CONST;
PanelAppletFactory *panel_applet_factory_new              (const gchar        *applet_id,
							   GType               applet_type,
							   GClosure           *closure);
gboolean            panel_applet_factory_register_service (PanelAppletFactory *factory);

/* This technically belongs to panel-applet.h, but it's not something that
 * should be public API. */
guint32            panel_applet_get_xid              (PanelApplet *applet,
						      GdkScreen   *screen);
const gchar       *panel_applet_get_object_path      (PanelApplet *applet);

G_END_DECLS

#endif /* PANEL_APPLET_FACTORY_H */
