/* vim: set sw=8 et: */
/*
 * panel-lockdown.c: a lockdown tracker.
 *
 * Copyright (C) 2011 Novell, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Vincent Untz <vuntz@gnome.org>
 */

#ifndef __PANEL_LOCKDOWN_H__
#define __PANEL_LOCKDOWN_H__

#include <gio/gio.h>
#include "libgnome-panel/gp-lockdown.h"

G_BEGIN_DECLS

#define PANEL_TYPE_LOCKDOWN            (panel_lockdown_get_type ())
#define PANEL_LOCKDOWN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_LOCKDOWN, PanelLockdown))
#define PANEL_LOCKDOWN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_LOCKDOWN, PanelLockdownClass))
#define PANEL_IS_LOCKDOWN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_LOCKDOWN))
#define PANEL_IS_LOCKDOWN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_LOCKDOWN))
#define PANEL_LOCKDOWN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PANEL_TYPE_LOCKDOWN, PanelLockdownClass))

typedef struct _PanelLockdown          PanelLockdown;
typedef struct _PanelLockdownClass     PanelLockdownClass;
typedef struct _PanelLockdownPrivate   PanelLockdownPrivate;

struct _PanelLockdown {
        GObject parent;

        /*< private > */
        PanelLockdownPrivate *priv;
};

struct _PanelLockdownClass {
        GObjectClass parent_class;
};

GType panel_lockdown_get_type (void);

PanelLockdown *panel_lockdown_new (void);

gboolean panel_lockdown_get_panels_locked_down   (PanelLockdown *lockdown);
gboolean panel_lockdown_get_disable_command_line (PanelLockdown *lockdown);

gboolean panel_lockdown_is_applet_disabled       (PanelLockdown *lockdown,
                                                  const char    *module_id,
                                                  const char    *applet_id);

typedef void (*PanelLockdownNotify) (PanelLockdown *lockdown,
                                     gpointer       user_data);

void     panel_lockdown_on_notify                (PanelLockdown *      lockdown,
                                                  const char          *property,
                                                  GObject             *object_while_alive,
                                                  PanelLockdownNotify  callback,
                                                  gpointer             callback_data);

GpLockdownFlags panel_lockdown_get_flags   (PanelLockdown *lockdown,
                                            const char    *module_id,
                                            const char    *applet_id);

G_END_DECLS

#endif /* __PANEL_LOCKDOWN_H__ */
