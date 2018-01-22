/*
 * Copyright (C) 2018 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GP_ACTION_H
#define GP_ACTION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GP_TYPE_ACTION (gp_action_get_type ())
G_DECLARE_INTERFACE (GpAction, gp_action, GP, ACTION, GObject)

struct _GpActionInterface
{
  GTypeInterface parent;

  gboolean (* main_menu) (GpAction *action,
                          guint32   time);

  /*< private >*/
  gpointer padding[10];
};

G_END_DECLS

#endif
