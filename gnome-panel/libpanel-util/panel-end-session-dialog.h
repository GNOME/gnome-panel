/*
 * panel-end-session-dialog.h:
 *
 * Copyright (C) Alberts Muktupāvels
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
 *	Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 */

#ifndef PANEL_END_SESSION_DIALOG_H
#define PANEL_END_SESSION_DIALOG_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PANEL_TYPE_END_SESSION_DIALOG         (panel_end_session_dialog_get_type ())
#define PANEL_END_SESSION_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_END_SESSION_DIALOG, PanelEndSessionDialog))
#define PANEL_END_SESSION_DIALOG_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    PANEL_TYPE_END_SESSION_DIALOG, PanelEndSessionDialogClass))
#define PANEL_IS_END_SESSION_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_END_SESSION_DIALOG))
#define PANEL_IS_END_SESSION_DIALOG_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    PANEL_TYPE_END_SESSION_DIALOG))
#define PANEL_END_SESSION_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o),   PANEL_TYPE_END_SESSION_DIALOG, PanelEndSessionDialogClass))

typedef struct _PanelEndSessionDialog        PanelEndSessionDialog;
typedef struct _PanelEndSessionDialogClass   PanelEndSessionDialogClass;
typedef struct _PanelEndSessionDialogPrivate PanelEndSessionDialogPrivate;

struct _PanelEndSessionDialog {
	GObject                       parent;
	PanelEndSessionDialogPrivate *priv;
};

struct _PanelEndSessionDialogClass {
	GObjectClass parent_class;
};

GType                  panel_end_session_dialog_get_type                  (void);

PanelEndSessionDialog *panel_end_session_dialog_get                       (void);

gboolean               panel_end_session_dialog_is_hibernate_available    (PanelEndSessionDialog *dialog);
void                   panel_end_session_dialog_request_hibernate         (PanelEndSessionDialog *dialog);

gboolean               panel_end_session_dialog_is_suspend_available      (PanelEndSessionDialog *dialog);
void                   panel_end_session_dialog_request_suspend           (PanelEndSessionDialog *dialog);

gboolean               panel_end_session_dialog_is_hybrid_sleep_available (PanelEndSessionDialog *dialog);
void                   panel_end_session_dialog_request_hybrid_sleep      (PanelEndSessionDialog *dialog);

G_END_DECLS

#endif /* PANEL_END_SESSION_DIALOG_H */
