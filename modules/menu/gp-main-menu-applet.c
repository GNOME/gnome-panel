/*
 * Copyright (C) 2018 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gp-main-menu-applet.h"

#include <libgnome-panel/gp-action.h>

struct _GpMainMenuApplet
{
  GpMenuButtonApplet parent;
};

static void gp_action_interface_init (GpActionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GpMainMenuApplet, gp_main_menu_applet, GP_MENU_BUTTON_TYPE_APPLET,
                         G_IMPLEMENT_INTERFACE (GP_TYPE_ACTION, gp_action_interface_init))

static gboolean
gp_menu_button_applet_handle_action (GpAction     *action,
                                     GpActionFlags flags,
                                     uint32_t      time)
{
  GpMenuButtonApplet *menu_button;

  if ((flags & GP_ACTION_MAIN_MENU) != GP_ACTION_MAIN_MENU)
    return FALSE;

  menu_button = GP_MENU_BUTTON_APPLET (action);

  return gp_menu_button_applet_popup_menu (menu_button, NULL);
}

static void
gp_action_interface_init (GpActionInterface *iface)
{
  iface->handle_action = gp_menu_button_applet_handle_action;
}

static void
gp_main_menu_applet_class_init (GpMainMenuAppletClass *menu_button_class)
{
}

static void
gp_main_menu_applet_init (GpMainMenuApplet *menu_button)
{
}
