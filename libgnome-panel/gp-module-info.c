/*
 * Copyright (C) 2016 Alberts Muktupāvels
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

#include <stdarg.h>

#include "gp-module-info-private.h"

static gchar **
get_applets (va_list args)
{
  GPtrArray *array;
  const gchar *applet;

  array = g_ptr_array_new ();
  while ((applet = va_arg (args, const gchar *)) != NULL)
    g_ptr_array_add (array, g_strdup (applet));

  g_ptr_array_add (array, NULL);

  return (gchar **) g_ptr_array_free (array, FALSE);
}

/**
 * gp_module_info_new:
 * @id: the id of this module
 * @version: the version of this module
 * @translation_domain: the translation domain or NULL
 * @...: a %NULL-terminated list of applet ids in this module
 *
 * Creates a new #GpModuleInfo.
 *
 * Returns: (transfer full): a newly created #GpModuleInfo.
 */
GpModuleInfo *
gp_module_info_new (const gchar *id,
                    const gchar *version,
                    const gchar *translation_domain,
                    ...)
{
  GpModuleInfo *info;
  va_list args;

  info = g_new0 (GpModuleInfo, 1);

  info->id = g_strdup (id);
  info->version = g_strdup (version);
  info->translation_domain = g_strdup (translation_domain);

  va_start (args, translation_domain);
  info->applets = get_applets (args);
  va_end (args);

  return info;
}

void
gp_module_info_free (GpModuleInfo *info)
{
  if (info == NULL)
    return;

  g_free (info->id);
  g_free (info->version);
  g_free (info->translation_domain);
  g_strfreev (info->applets);

  g_free (info);
}
