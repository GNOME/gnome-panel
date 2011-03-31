/*
 * panel-menu-bar-object.h: a base class for menu bar objects
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_MENU_BAR_OBJECT_H__
#define __PANEL_MENU_BAR_OBJECT_H__

#include <gtk/gtk.h>

#include "panel-widget.h"

G_BEGIN_DECLS

#define PANEL_TYPE_MENU_BAR_OBJECT         (panel_menu_bar_object_get_type ())
#define PANEL_MENU_BAR_OBJECT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_MENU_BAR_OBJECT, PanelMenuBarObject))
#define PANEL_MENU_BAR_OBJECT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_MENU_BAR_OBJECT, PanelMenuBarObjectClass))
#define PANEL_IS_MENU_BAR_OBJECT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_MENU_BAR_OBJECT))
#define PANEL_IS_MENU_BAR_OBJECT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_MENU_BAR_OBJECT))
#define PANEL_MENU_BAR_OBJECT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_MENU_BAR_OBJECT, PanelMenuBarObjectClass))

typedef struct _PanelMenuBarObject        PanelMenuBarObject;
typedef struct _PanelMenuBarObjectClass   PanelMenuBarObjectClass;
typedef struct _PanelMenuBarObjectPrivate PanelMenuBarObjectPrivate;

struct _PanelMenuBarObject{
	GtkMenuBar                  menubar;

	PanelMenuBarObjectPrivate  *priv;
};

struct _PanelMenuBarObjectClass {
	GtkMenuBarClass             menubar_class;
};

GType      panel_menu_bar_object_get_type  (void) G_GNUC_CONST;

void panel_menu_bar_object_object_load_finish (PanelMenuBarObject *menubar,
					       PanelWidget        *panel);

void       panel_menu_bar_object_change_background (PanelMenuBarObject *menubar);

void             panel_menu_bar_object_set_orientation (PanelMenuBarObject     *menubar,
							PanelOrientation        orientation);
PanelOrientation panel_menu_bar_object_get_orientation (PanelMenuBarObject     *menubar);

G_END_DECLS

#endif /* __PANEL_MENU_BAR_OBJECT_H__ */
