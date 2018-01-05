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

#include "fish-applet.h"

static GpAppletInfo *
fish_get_applet_info (const gchar *applet)
{
  const gchar *description;
  GpAppletInfo *info;

  description = _("Display a swimming fish or another animated creature");
  info = gp_applet_info_new (_("Fish"), description, "gnome-panel-fish");

  gp_applet_info_set_backends (info, "x11");

  return info;
}

static GType
fish_get_applet_type (const gchar *applet)
{
  return FISH_TYPE_APPLET;
}

static const gchar *
fish_get_applet_id_from_iid (const gchar *iid)
{
  if (g_strcmp0 (iid, "FishAppletFactory::FishApplet") == 0 ||
      g_strcmp0 (iid, "fish::fish") == 0)
    return "fish";

  return NULL;
}

void
gp_module_load (GpModule *module)
{
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  gp_module_set_gettext_domain (module, GETTEXT_PACKAGE);

  gp_module_set_abi_version (module, GP_MODULE_ABI_VERSION);

  gp_module_set_id (module, "org.gnome.gnome-panel.fish");
  gp_module_set_version (module, PACKAGE_VERSION);

  gp_module_set_applet_ids (module, "fish", NULL);

  gp_module_set_compatibility (module, fish_get_applet_id_from_iid);
}

void
gp_module_get_applet_vtable (GpAppletVTable *vtable)
{
  *vtable = (GpAppletVTable) {
    fish_get_applet_info,
    fish_get_applet_type
  };
}
