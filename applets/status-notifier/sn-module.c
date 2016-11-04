/*
 * Copyright (C) 2016 Alberts Muktupāvels
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

static GpModuleInfo *
sn_get_module_info (void)
{
  GpModuleInfo *info;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  info = gp_module_info_new ("status-notifier", PACKAGE_VERSION);

  gp_module_info_set_applets (info, "status-notifier", NULL);
  gp_module_info_set_translation_domain (info, GETTEXT_PACKAGE);

  return info;
}

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

guint32
gp_module_get_abi_version (void)
{
  return GP_MODULE_ABI_VERSION;
}

void
gp_module_get_vtable (GpModuleVTable *vtable)
{
  *vtable = (GpModuleVTable) {
    sn_get_module_info,
    sn_get_applet_info,
    sn_get_applet_type,
    NULL
  };
}
