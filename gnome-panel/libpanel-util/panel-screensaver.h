/*
 * panel-screensaver.h:
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#ifndef PANEL_SCREENSAVER_H
#define PANEL_SCREENSAVER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PANEL_TYPE_SCREENSAVER			(panel_screensaver_get_type ())
#define PANEL_SCREENSAVER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_SCREENSAVER, PanelScreensaver))
#define PANEL_SCREENSAVER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_SCREENSAVER, PanelScreensaverClass))
#define PANEL_IS_SCREENSAVER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_SCREENSAVER))
#define PANEL_IS_SCREENSAVER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_SCREENSAVER))
#define PANEL_SCREENSAVER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PANEL_TYPE_SCREENSAVER, PanelScreensaverClass))

typedef struct _PanelScreensaver		PanelScreensaver;
typedef struct _PanelScreensaverClass		PanelScreensaverClass;
typedef struct _PanelScreensaverPrivate		PanelScreensaverPrivate;

struct _PanelScreensaver {
	GObject parent;

	/*< private > */
	PanelScreensaverPrivate *priv;
};

struct _PanelScreensaverClass {
	GObjectClass parent_class;
};

GType panel_screensaver_get_type (void);

PanelScreensaver *panel_screensaver_get (void);

void panel_screensaver_lock     (PanelScreensaver *screensaver);
void panel_screensaver_activate (PanelScreensaver *screensaver);

G_END_DECLS

#endif /* PANEL_SCREENSAVER_H */
