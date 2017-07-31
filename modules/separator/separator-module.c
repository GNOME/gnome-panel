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

#include "separator-applet.h"

static GpAppletInfo *
separator_get_applet_info (const gchar *applet)
{
  return gp_applet_info_new (_("Separator"),
                             _("A separator to organize the panel items"),
                             "gnome-panel-separator");
}

static GType
separator_get_applet_type (const gchar *applet)
{
  return SEPARATOR_TYPE_APPLET;
}

static const gchar *
separator_get_applet_from_iid (const gchar *iid)
{
  if (g_strcmp0 (iid, "PanelInternalFactory::Separator") == 0 ||
      g_strcmp0 (iid, "separator::separator") == 0)
    return "separator";

  return NULL;
}

guint32
gp_module_get_abi_version (void)
{
  return GP_MODULE_ABI_VERSION;
}

GpModuleInfo *
gp_module_get_module_info (void)
{
  GpModuleInfo *info;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  info = gp_module_info_new ("org.gnome.gnome-panel.separator",
                             PACKAGE_VERSION, GETTEXT_PACKAGE);

  gp_module_info_set_applets (info, "separator", NULL);

  return info;
}

void
gp_module_get_applet_vtable (GpAppletVTable *vtable)
{
  *vtable = (GpAppletVTable) {
    separator_get_applet_info,
    separator_get_applet_type,
    separator_get_applet_from_iid,
    NULL
  };
}
