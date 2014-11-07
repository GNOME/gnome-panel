/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2008 Novell, Inc.
 * Copyright (C) 2010 Carlos Garcia Campos
 * Copyright (C) 2014 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *    Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *    Carlos Garcia Campos <carlosgc@gnome.org>
 *    Colin Walters <walters@verbum.org>
 *    Jacob Berkman <jacob@ximian.com>
 *    Mark McLoughlin <mark@skynet.ie>
 *    Vincent Untz <vuntz@gnome.org>
 */

#ifndef PANEL_SESSION_H
#define PANEL_SESSION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PANEL_TYPE_SESSION         (panel_session_get_type ())
#define PANEL_SESSION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_SESSION, PanelSession))
#define PANEL_SESSION_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    PANEL_TYPE_SESSION, PanelSessionClass))
#define PANEL_IS_SESSION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_SESSION))
#define PANEL_IS_SESSION_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    PANEL_TYPE_SESSION))
#define PANEL_SESSION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  PANEL_TYPE_SESSION, PanelSessionClass))

typedef struct _PanelSession        PanelSession;
typedef struct _PanelSessionClass   PanelSessionClass;
typedef struct _PanelSessionPrivate PanelSessionPrivate;

struct _PanelSession {
	GObject              parent;
	PanelSessionPrivate *priv;
};

struct _PanelSessionClass {
	GObjectClass parent_class;
};

GType         panel_session_get_type        (void);

PanelSession *panel_session_new             (gboolean      replace);

gboolean      panel_session_register_client (PanelSession *session);

G_END_DECLS

#endif
