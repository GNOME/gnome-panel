/*
 * tz-sel-dialog.h: timezone selection dialog
 *
 * Copyright (C) 2007 Vincent Untz <vuntz@gnome.org>
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
 *      Vincent Untz <vuntz@gnome.org>
 */

#ifndef TZ_SEL_DIALOG_H
#define TZ_SEL_DIALOG_H

#include <gtk/gtk.h>

#include "tz-list.h"

G_BEGIN_DECLS

GtkWidget *tz_sel_dialog_new     (TzList    *tz_list);
void       tz_sel_dialog_present (GtkWindow *window);

G_END_DECLS

#endif /* TZ_SEL_DIALOG_H */
