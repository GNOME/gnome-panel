/*
 * panel-addto.h:
 *
 * Copyright (C) 2004 Vincent Untz
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
 *	Vincent Untz <vincent@vuntz.net>
 */

#ifndef PANEL_ADDTO_DIALOG_H
#define PANEL_ADDTO_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PANEL_TYPE_ADDTO_DIALOG panel_addto_dialog_get_type()
G_DECLARE_FINAL_TYPE (PanelAddtoDialog, panel_addto_dialog,
                      PANEL, ADDTO_DIALOG, GtkWindow)

void panel_addto_present (GtkMenuItem *item,
                          PanelWidget *panel_widget);

G_END_DECLS

#endif
