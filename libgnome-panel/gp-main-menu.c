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
#include "gp-main-menu-private.h"

G_DEFINE_INTERFACE (GpMainMenu, gp_main_menu, G_TYPE_OBJECT)

static void
gp_main_menu_default_init (GpMainMenuInterface *iface)
{
}

gboolean
gp_main_menu_activate (GpMainMenu *main_menu,
                       guint32     time)
{
  GpMainMenuInterface *iface;

  iface = GP_MAIN_MENU_GET_IFACE (main_menu);

  if (iface->activate == NULL)
    return FALSE;

  iface->activate (main_menu, time);

  return TRUE;
}
