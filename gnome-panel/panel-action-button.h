/*
 * panel-action-button.h:
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
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
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_ACTION_BUTTON_H__
#define __PANEL_ACTION_BUTTON_H__

#include <gtk/gtkbutton.h>
#include "button-widget.h"
#include "panel-widget.h"

G_BEGIN_DECLS

#define PANEL_TYPE_ACTION_BUTTON         (panel_action_button_get_type ())
#define PANEL_ACTION_BUTTON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_ACTION_BUTTON, PanelActionButton))
#define PANEL_ACTION_BUTTON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_ACTION_BUTTON, PanelActionButtonClass))
#define PANEL_IS_ACTION_BUTTON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_ACTION_BUTTON))
#define PANEL_IS_ACTION_BUTTON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_ACTION_BUTTON))
#define PANEL_ACTION_BUTTON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_ACTION_BUTTON, PanelActionButtonClass))

typedef struct _PanelActionButton        PanelActionButton;
typedef struct _PanelActionButtonClass   PanelActionButtonClass;
typedef struct _PanelActionButtonPrivate PanelActionButtonPrivate;

typedef enum {
	PANEL_ACTION_NONE = 0,
	PANEL_ACTION_LOCK,
	PANEL_ACTION_LOGOUT,
	PANEL_ACTION_RUN,
	PANEL_ACTION_SEARCH,
	PANEL_ACTION_SCREENSHOT,
	PANEL_ACTION_SHOW_DESKTOP,
	PANEL_ACTION_LAST
} PanelActionButtonType;

struct _PanelActionButton{
	ButtonWidget               button;

	PanelActionButtonPrivate  *priv;
};

struct _PanelActionButtonClass {
	ButtonWidgetClass          button_class;
};

GType      panel_action_button_get_type  (void) G_GNUC_CONST;

GtkWidget *panel_action_button_load             (PanelActionButtonType   type,
						 PanelWidget            *panel,
						 int                     position,
						 gboolean                exactpos,
						 const char             *gconf_key,
						 gboolean                compatibility);

void       panel_action_button_set_type         (PanelActionButton     *button,
						 PanelActionButtonType  type);

GtkWidget *panel_action_button_load_from_gconf  (PanelWidget            *panel,
						 int                     position,
						 gboolean                exactpos,
						 const char             *gconf_key);
void       panel_action_button_save_to_gconf    (PanelActionButton      *button,
						 const char             *gconf_key);


void       panel_action_button_invoke_menu      (PanelActionButton      *button,
						 const char             *callback_name);

gboolean   panel_action_button_load_from_drag   (const char             *drag_string,
						 PanelWidget            *panel,
						 int                     position,
						 gboolean                exactpos,
						 const char             *gconf_key,
						 int                    *old_applet);

void       panel_action_lock_screen (GtkWidget *widget);
void       panel_action_logout      (GtkWidget *widget);
void       panel_action_run_program (GtkWidget *widget);
void       panel_action_search      (GtkWidget *widget);
void       panel_action_screenshot  (GtkWidget *widget);

G_END_DECLS

#endif /* __PANEL_ACTION_BUTTON_H__ */
