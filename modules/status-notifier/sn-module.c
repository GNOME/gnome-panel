/*
 * Copyright (C) 2016-2017 Alberts Muktupāvels
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

#include "sn-applet.h"

static GpAppletInfo *
sn_get_applet_info (const gchar *applet)
{
  const gchar *description;
  GpAppletInfo *info;

  description = _("Display all Status Notifier Items");
  info = gp_applet_info_new (_("Status Notifier Host"), description,
                             "gnome-panel-notification-area");

  return info;
}

static GType
sn_get_applet_type (const gchar *applet)
{
  return SN_TYPE_APPLET;
}

static const gchar *
sn_get_applet_from_iid (const gchar *iid)
{
  if (g_strcmp0 (iid, "status-notifier::status-notifier") == 0)
    return "status-notifier";

  return NULL;
}

void
gp_module_load (GpModule *module)
{
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  gp_module_set_gettext_domain (module, GETTEXT_PACKAGE);

  gp_module_set_abi_version (module, GP_MODULE_ABI_VERSION);

  gp_module_set_id (module, "org.gnome.gnome-panel.status-notifier");
  gp_module_set_version (module, PACKAGE_VERSION);

  gp_module_set_applet_ids (module, "status-notifier", NULL);
}

void
gp_module_get_applet_vtable (GpAppletVTable *vtable)
{
  *vtable = (GpAppletVTable) {
    sn_get_applet_info,
    sn_get_applet_type,
    sn_get_applet_from_iid,
    NULL
  };
}
