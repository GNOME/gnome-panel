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

#include "gp-custom-launcher-applet.h"
#include "gp-launcher-applet.h"

static gboolean
custom_launcher_is_disabled (GpLockdownFlags   flags,
                             char            **reason)
{
  if ((flags & GP_LOCKDOWN_FLAGS_COMMAND_LINE) != GP_LOCKDOWN_FLAGS_COMMAND_LINE)
    return FALSE;

  *reason = g_strdup (_("Disabled because “disable-command-line” setting in "
                        "“org.gnome.desktop.lockdown” GSettings schema is "
                        "set to true."));

  return TRUE;
}

static GpAppletInfo *
launcher_get_applet_info (const gchar *id)
{
  GpGetAppletTypeFunc type_func;
  const gchar *name;
  const gchar *description;
  const gchar *icon;
  GpInitialSetupDialogFunc initial_setup_func;
  GpIsDisabledFunc is_disabled_func;
  GpAppletInfo *info;

  initial_setup_func = NULL;
  is_disabled_func = NULL;

  if (g_strcmp0 (id, "custom-launcher") == 0)
    {
      type_func = gp_custom_launcher_applet_get_type;
      name = _("Custom Application Launcher");
      description = _("Create a new launcher");
      icon = "gnome-panel-launcher";

      initial_setup_func = gp_custom_launcher_applet_initial_setup_dialog;
      is_disabled_func = custom_launcher_is_disabled;
    }
  else if (g_strcmp0 (id, "launcher") == 0)
    {
      type_func = gp_launcher_applet_get_type;
      name = _("Application Launcher...");
      description = _("Copy a launcher from the applications menu");
      icon = "gnome-panel-launcher";

      initial_setup_func = gp_launcher_applet_initial_setup_dialog;
    }
  else
    {
      g_assert_not_reached ();
      return NULL;
    }

  info = gp_applet_info_new (type_func, name, description, icon);

  if (initial_setup_func != NULL)
    gp_applet_info_set_initial_setup_dialog (info, initial_setup_func);

  if (is_disabled_func != NULL)
    gp_applet_info_set_is_disabled (info, is_disabled_func);

  return info;
}

static const gchar *
launcher_get_applet_id_from_iid (const gchar *iid)
{
  if (g_strcmp0 (iid, "PanelInternalFactory::Launcher") == 0)
    return "custom-launcher";

  return NULL;
}

void
gp_module_load (GpModule *module)
{
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  gp_module_set_gettext_domain (module, GETTEXT_PACKAGE);

  gp_module_set_abi_version (module, GP_MODULE_ABI_VERSION);

  gp_module_set_id (module, "org.gnome.gnome-panel.launcher");
  gp_module_set_version (module, PACKAGE_VERSION);

  gp_module_set_applet_ids (module, "custom-launcher", "launcher", NULL);

  gp_module_set_get_applet_info (module, launcher_get_applet_info);
  gp_module_set_compatibility (module, launcher_get_applet_id_from_iid);
}
