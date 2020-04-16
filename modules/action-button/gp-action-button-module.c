/*
 * Copyright (C) 2020 Alberts Muktupāvels
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

#include "gp-lock-screen-applet.h"

static GpAppletInfo *
action_button_get_applet_info (const char *id)
{
  GpGetAppletTypeFunc type_func;
  const char *name;
  const char *description;
  const char *icon;
  GpIsDisabledFunc is_disabled_func;
  GpAppletInfo *info;

  is_disabled_func = NULL;

  if (g_strcmp0 (id, "lock-screen") == 0)
    {
      type_func = gp_lock_screen_applet_get_type;
      name = _("Lock Screen");
      description = _("Protect your computer from unauthorized use");
      icon = "system-lock-screen";

      is_disabled_func = gp_lock_screen_applet_is_disabled;
    }
  else
    {
      g_assert_not_reached ();
      return NULL;
    }

  info = gp_applet_info_new (type_func, name, description, icon);

  if (is_disabled_func != NULL)
    gp_applet_info_set_is_disabled (info, is_disabled_func);

  return info;
}

static const char *
action_button_get_applet_id_from_iid (const char *iid)
{
  if (g_strcmp0 (iid, "PanelInternalFactory::ActionButton:lock") == 0)
    return "lock-screen";

  return NULL;
}

void
gp_module_load (GpModule *module)
{
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  gp_module_set_gettext_domain (module, GETTEXT_PACKAGE);

  gp_module_set_abi_version (module, GP_MODULE_ABI_VERSION);

  gp_module_set_id (module, "org.gnome.gnome-panel.action-button");
  gp_module_set_version (module, PACKAGE_VERSION);

  gp_module_set_applet_ids (module,
                            "lock-screen",
                            NULL);

  gp_module_set_get_applet_info (module, action_button_get_applet_info);
  gp_module_set_compatibility (module, action_button_get_applet_id_from_iid);
}
