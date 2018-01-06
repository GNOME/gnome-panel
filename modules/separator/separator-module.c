/*
 * Copyright (C) 2016-2018 Alberts Muktupāvels
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

#include "separator-applet.h"

static GpAppletInfo *
separator_get_applet_info (const gchar *id)
{
  const gchar *name;
  const gchar *description;
  const gchar *icon;
  GpAppletInfo *info;

  name = _("Separator");
  description = _("A separator to organize the panel items");
  icon = "gnome-panel-separator";

  info = gp_applet_info_new (separator_applet_get_type, name, description, icon);

  return info;
}

static const gchar *
separator_get_applet_id_from_iid (const gchar *iid)
{
  if (g_strcmp0 (iid, "PanelInternalFactory::Separator") == 0 ||
      g_strcmp0 (iid, "separator::separator") == 0)
    return "separator";

  return NULL;
}

void
gp_module_load (GpModule *module)
{
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  gp_module_set_gettext_domain (module, GETTEXT_PACKAGE);

  gp_module_set_abi_version (module, GP_MODULE_ABI_VERSION);

  gp_module_set_id (module, "org.gnome.gnome-panel.separator");
  gp_module_set_version (module, PACKAGE_VERSION);

  gp_module_set_applet_ids (module, "separator", NULL);

  gp_module_set_get_applet_info (module, separator_get_applet_info);
  gp_module_set_compatibility (module, separator_get_applet_id_from_iid);
}
