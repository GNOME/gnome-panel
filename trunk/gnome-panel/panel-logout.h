/*
 * panel-logout.h:
 *
 * Copyright (C) 2006 Vincent Untz
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

#ifndef PANEL_LOGOUT_H
#define PANEL_LOGOUT_H

#include "gtk/gtkmessagedialog.h"

G_BEGIN_DECLS

typedef enum {
	PANEL_LOGOUT_DIALOG_LOGOUT,
	PANEL_LOGOUT_DIALOG_SHUTDOWN
} PanelLogoutDialogType;

#define PANEL_TYPE_LOGOUT_DIALOG         (panel_logout_get_type ())
#define PANEL_LOGOUT_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_LOGOUT_DIALOG, PanelLogoutDialog))
#define PANEL_LOGOUT_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_LOGOUT_DIALOG, PanelLogoutDialogClass))
#define PANEL_IS_LOGOUT_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_LOGOUT_DIALOG))
#define PANEL_IS_LOGOUT_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_LOGOUT_DIALOG))
#define PANEL_LOGOUT_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_LOGOUT_DIALOG, PanelLogoutDialogClass))

typedef struct _PanelLogoutDialog        PanelLogoutDialog;
typedef struct _PanelLogoutDialogClass   PanelLogoutDialogClass;
typedef struct _PanelLogoutDialogPrivate PanelLogoutDialogPrivate;

struct _PanelLogoutDialog {
	GtkMessageDialog          parent;

	PanelLogoutDialogPrivate *priv;
};

struct _PanelLogoutDialogClass {
	GtkMessageDialogClass   parent_class;
};

GType  panel_logout_get_type          (void) G_GNUC_CONST;	

void panel_logout_new (PanelLogoutDialogType  type,
		       GdkScreen             *screen,
		       guint32                activate_time);

G_END_DECLS

#endif /* PANEL_LOGOUT_H */
