/*
 * panel-toplevel.h: The panel's toplevel window object.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.

 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_TOPLEVEL_H__
#define __PANEL_TOPLEVEL_H__

#include <gtk/gtk.h>

#include "panel-enums-gsettings.h"

G_BEGIN_DECLS

/* We need PanelWidget type but don't want to include
   the panel-widget.h */
#ifndef TYPEDEF_PANEL_WIDGET
typedef struct _PanelWidget		PanelWidget;
#define TYPEDEF_PANEL_WIDGET
#endif /* TYPEDEF_PANEL_WIDGET */

#define PANEL_TYPE_TOPLEVEL         (panel_toplevel_get_type ())
#define PANEL_TOPLEVEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_TOPLEVEL, PanelToplevel))
#define PANEL_TOPLEVEL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_TOPLEVEL, PanelToplevelClass))
#define PANEL_IS_TOPLEVEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_TOPLEVEL))
#define PANEL_IS_TOPLEVEL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_TOPLEVEL))
#define PANEL_TOPLEVEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_TOPLEVEL, PanelToplevelClass))

typedef struct _PanelToplevel        PanelToplevel;
typedef struct _PanelToplevelClass   PanelToplevelClass;
typedef struct _PanelToplevelPrivate PanelToplevelPrivate;

struct _PanelToplevel {
	GtkWindow              window_instance;

	PanelToplevelPrivate  *priv;
};

struct _PanelToplevelClass {
	GtkWindowClass         window_class;

	/* key bindings */
	gboolean  (*popup_panel_menu) (PanelToplevel *toplevel);
	gboolean  (*toggle_expand)    (PanelToplevel *toplevel);
	gboolean  (*expand)           (PanelToplevel *toplevel);
	gboolean  (*unexpand)         (PanelToplevel *toplevel);
	gboolean  (*toggle_hidden)    (PanelToplevel *toplevel);
	gboolean  (*begin_move)       (PanelToplevel *toplevel);
	gboolean  (*begin_resize)     (PanelToplevel *toplevel);
};

GType                panel_toplevel_get_type               (void) G_GNUC_CONST;

PanelWidget         *panel_toplevel_get_panel_widget       (PanelToplevel       *toplevel);

const char          *panel_toplevel_get_id                 (PanelToplevel       *toplevel);
void                 panel_toplevel_set_name               (PanelToplevel       *toplevel,	
							    const char          *name);
const char          *panel_toplevel_get_name               (PanelToplevel       *toplevel);
const char          *panel_toplevel_get_description        (PanelToplevel       *toplevel);
void                 panel_toplevel_set_expand             (PanelToplevel       *toplevel,
							    gboolean             expand);
gboolean             panel_toplevel_get_expand             (PanelToplevel       *toplevel);
void                 panel_toplevel_set_orientation        (PanelToplevel       *toplevel,
							    PanelOrientation     orientation);
PanelOrientation     panel_toplevel_get_orientation        (PanelToplevel       *toplevel);
void                 panel_toplevel_set_size               (PanelToplevel       *toplevel,
							    int                  size);
int                  panel_toplevel_get_size               (PanelToplevel       *toplevel);
void                 panel_toplevel_set_monitor            (PanelToplevel       *toplevel,
							    int                  monitor);
int                  panel_toplevel_get_monitor            (PanelToplevel       *toplevel);
void                 panel_toplevel_set_auto_hide_size     (PanelToplevel       *toplevel,
							    int                  autohide_size);

gboolean             panel_toplevel_get_is_hidden          (PanelToplevel       *toplevel);

void                 panel_toplevel_hide                   (PanelToplevel       *toplevel,
							    gboolean             auto_hide,
							    GtkDirectionType     direction);
void                 panel_toplevel_unhide                 (PanelToplevel       *toplevel);
void                 panel_toplevel_queue_auto_hide        (PanelToplevel       *toplevel);
void                 panel_toplevel_queue_auto_unhide      (PanelToplevel       *toplevel);
void                 panel_toplevel_queue_initial_unhide   (PanelToplevel       *toplevel);
void                 panel_toplevel_push_autohide_disabler (PanelToplevel       *toplevel);
void                 panel_toplevel_pop_autohide_disabler  (PanelToplevel       *toplevel);

void                 panel_toplevel_set_auto_hide          (PanelToplevel       *toplevel,
							    gboolean             autohide);
void                 panel_toplevel_set_hide_delay         (PanelToplevel       *toplevel,
							    int                  hide_delay);
void                 panel_toplevel_set_unhide_delay       (PanelToplevel       *toplevel,
							    int                  unhide_delay);

void                 panel_toplevel_set_animation_speed    (PanelToplevel       *toplevel,
							    PanelAnimationSpeed  animation_speed);

void                 panel_toplevel_set_enable_buttons     (PanelToplevel       *toplevel,
							    gboolean             enable_buttons);

gboolean             panel_toplevel_is_last                (PanelToplevel       *toplevel);
int                  panel_toplevel_get_maximum_size       (PanelToplevel *toplevel);

GSList              *panel_toplevel_list_toplevels         (void);
PanelToplevel       *panel_toplevel_get_by_id              (const char    *toplevel_id);
gboolean             panel_toplevel_find_empty_spot        (GdkScreen        *screen,
							    PanelOrientation *orientation,
							    int              *monitor);

G_END_DECLS

#endif /* __PANEL_TOPLEVEL_H__ */
