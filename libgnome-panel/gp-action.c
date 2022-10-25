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

#include "config.h"
#include "gp-action-private.h"

G_DEFINE_INTERFACE (GpAction, gp_action, G_TYPE_OBJECT)

static void
gp_action_default_init (GpActionInterface *iface)
{
}

gboolean
gp_action_handle_action (GpAction      *self,
                         GpActionFlags  action,
                         uint32_t       time)
{
  GpActionInterface *iface;

  iface = GP_ACTION_GET_IFACE (self);

  if (iface->handle_action == NULL)
    return FALSE;

  return iface->handle_action (self, action, time);
}
