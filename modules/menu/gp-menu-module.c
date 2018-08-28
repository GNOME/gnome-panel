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

#include <glib/gi18n-lib.h>
#include <libgnome-panel/gp-module.h>

#include "gp-main-menu-applet.h"
#include "gp-menu-bar-applet.h"
#include "gp-user-menu-applet.h"

static GpAppletInfo *
menu_get_applet_info (const gchar *id)
{
  GpGetAppletTypeFunc type_func;
  const gchar *name;
  const gchar *description;
  const gchar *icon;
  GpAppletInfo *info;

  if (g_strcmp0 (id, "main-menu") == 0)
    {
      type_func = gp_main_menu_applet_get_type;
      name = _("Main Menu");
      description = _("The main GNOME menu");
      icon = "start-here";
    }
  else if (g_strcmp0 (id, "menu-bar") == 0)
    {
      type_func = gp_menu_bar_applet_get_type;
      name = _("Menu Bar");
      description = _("A custom menu bar");
      icon = "start-here";
    }
  else if (g_strcmp0 (id, "user-menu") == 0)
    {
      type_func = gp_user_menu_applet_get_type;
      name = _("User menu");
      description = _("Menu to change your settings and log out");
      icon = "computer";
    }
  else
    {
      g_assert_not_reached ();
      return NULL;
    }

  info = gp_applet_info_new (type_func, name, description, icon);

  return info;
}

static const gchar *
menu_get_applet_id_from_iid (const gchar *iid)
{
  if (g_strcmp0 (iid, "PanelInternalFactory::MenuBar") == 0)
    return "menu-bar";

  return NULL;
}

void
gp_module_load (GpModule *module)
{
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  gp_module_set_gettext_domain (module, GETTEXT_PACKAGE);

  gp_module_set_abi_version (module, GP_MODULE_ABI_VERSION);

  gp_module_set_id (module, "org.gnome.gnome-panel.menu");
  gp_module_set_version (module, PACKAGE_VERSION);

  gp_module_set_applet_ids (module, "menu-bar", "user-menu", NULL);

  gp_module_set_get_applet_info (module, menu_get_applet_info);
  gp_module_set_compatibility (module, menu_get_applet_id_from_iid);
}
