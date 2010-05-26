/*
 * panel-applets-manager-bonobo.h
 *
 * Copyright (C) 2010 Vincent Untz <vuntz@gnome.org>
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
 */

#ifndef __PANEL_APPLETS_MANAGER_BONOBO_H__
#define __PANEL_APPLETS_MANAGER_BONOBO_H__

#include <panel-applets-manager.h>

G_BEGIN_DECLS

#define PANEL_TYPE_APPLETS_MANAGER_BONOBO		(panel_applets_manager_bonobo_get_type ())
#define PANEL_APPLETS_MANAGER_BONOBO(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_APPLETS_MANAGER_BONOBO, PanelAppletsManagerBonobo))
#define PANEL_APPLETS_MANAGER_BONOBO_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_APPLETS_MANAGER_BONOBO, PanelAppletsManagerBonoboClass))
#define PANEL_IS_APPLETS_MANAGER_BONOBO(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_APPLETS_MANAGER_BONOBO))
#define PANEL_IS_APPLETS_MANAGER_BONOBO_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_APPLETS_MANAGER_BONOBO))
#define PANEL_APPLETS_MANAGER_BONOBO_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PANEL_TYPE_APPLETS_MANAGER_BONOBO, PanelAppletsManagerBonoboClass))

typedef struct _PanelAppletsManagerBonobo		PanelAppletsManagerBonobo;
typedef struct _PanelAppletsManagerBonoboClass		PanelAppletsManagerBonoboClass;
typedef struct _PanelAppletsManagerBonoboPrivate	PanelAppletsManagerBonoboPrivate;

struct _PanelAppletsManagerBonoboClass {
	PanelAppletsManagerClass parent_class;
};

struct _PanelAppletsManagerBonobo {
	PanelAppletsManager parent;

	/*< private > */
	PanelAppletsManagerBonoboPrivate *priv;
};

GType panel_applets_manager_bonobo_get_type (void);

G_END_DECLS

#endif /* __PANEL_APPLETS_MANAGER_BONOBO_H__ */
