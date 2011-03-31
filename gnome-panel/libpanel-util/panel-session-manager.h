/*
 * panel-session.h:
 *
 * Copyright (C) 2008 Novell, Inc.
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
 *	Vincent Untz <vuntz@gnome.org>
 */

#ifndef PANEL_SESSION_MANAGER_H
#define PANEL_SESSION_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PANEL_TYPE_SESSION_MANAGER		(panel_session_manager_get_type ())
#define PANEL_SESSION_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_SESSION_MANAGER, PanelSessionManager))
#define PANEL_SESSION_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_SESSION_MANAGER, PanelSessionManagerClass))
#define PANEL_IS_SESSION_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_SESSION_MANAGER))
#define PANEL_IS_SESSION_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_SESSION_MANAGER))
#define PANEL_SESSION_MANAGER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PANEL_TYPE_SESSION_MANAGER, PanelSessionManagerClass))

typedef struct _PanelSessionManager		PanelSessionManager;
typedef struct _PanelSessionManagerClass	PanelSessionManagerClass;
typedef struct _PanelSessionManagerPrivate	PanelSessionManagerPrivate;

/* Keep in sync with the values defined in gnome-session/session.h */
typedef enum {
        PANEL_SESSION_MANAGER_LOGOUT_MODE_NORMAL = 0,
        PANEL_SESSION_MANAGER_LOGOUT_MODE_NO_CONFIRMATION,
        PANEL_SESSION_MANAGER_LOGOUT_MODE_FORCE
} PanelSessionManagerLogoutType;

/* Keep in sync with the values defined in gnome-session/gsm-presence.h */
typedef enum {
        PANEL_SESSION_MANAGER_PRESENCE_AVAILABLE = 0,
        PANEL_SESSION_MANAGER_PRESENCE_INVISIBLE,
        PANEL_SESSION_MANAGER_PRESENCE_BUSY,
        PANEL_SESSION_MANAGER_PRESENCE_IDLE
} PanelSessionManagerPresenceType;

struct _PanelSessionManager {
	GObject parent;

	/*< private > */
	PanelSessionManagerPrivate *priv;
};

struct _PanelSessionManagerClass {
	GObjectClass parent_class;

        void (* presence_changed)  (PanelSessionManager             *manager,
                                    PanelSessionManagerPresenceType  presence);
};

GType panel_session_manager_get_type (void);

PanelSessionManager *panel_session_manager_get (void);

void panel_session_manager_set_presence     (PanelSessionManager             *session,
                                             PanelSessionManagerPresenceType  presence);
PanelSessionManagerPresenceType panel_session_manager_get_presence (PanelSessionManager *session);


void panel_session_manager_request_logout   (PanelSessionManager           *session,
					     PanelSessionManagerLogoutType  mode);
void panel_session_manager_request_shutdown (PanelSessionManager *session);

gboolean panel_session_manager_is_shutdown_available (PanelSessionManager *session);

G_END_DECLS

#endif /* PANEL_SESSION_MANAGER_H */
