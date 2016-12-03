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

/**
 * SECTION: gp-applet-info
 * @title: GpAppletInfo
 * @short_description: the applet info
 * @include: libngome-panel/gp-module.h
 *
 * #GpAppletInfo is used to provide full info about applet.
 */

#include "config.h"

#include "gp-applet-info-private.h"

/**
 * gp_applet_info_new:
 * @name: the name of this applet
 * @description: the description of this applet
 * @icon_name: the icon name for this applet
 *
 * Creates a new #GpAppletInfo.
 *
 * Returns: (transfer full): a newly created #GpAppletInfo.
 */
GpAppletInfo *
gp_applet_info_new (const gchar *name,
                    const gchar *description,
                    const gchar *icon_name)
{
  GpAppletInfo *info;

  info = g_new0 (GpAppletInfo, 1);

  info->name = g_strdup (name);
  info->description = g_strdup (description);
  info->icon_name = g_strdup (icon_name);

  info->help_uri = NULL;
  info->has_about_dialog = FALSE;

  info->backends = NULL;

  return info;
}

/**
 * gp_applet_info_set_help_uri:
 * @info: a #GpAppletInfo
 * @help_uri: the help uri
 *
 * Sets the help uri.
 */
void
gp_applet_info_set_help_uri (GpAppletInfo *info,
                             const gchar  *help_uri)
{
  g_free (info->help_uri);
  info->help_uri = g_strdup (help_uri);
}

/**
 * gp_applet_info_set_has_about_dialog:
 * @info: a #GpAppletInfo
 * @has_about_dialog: whether applet has an about dialog
 *
 * Sets whether applet has an about dialog.
 */
void
gp_applet_info_set_has_about_dialog (GpAppletInfo *info,
                                     gboolean      has_about_dialog)
{
  info->has_about_dialog = has_about_dialog;
}

/**
 * gp_applet_info_set_backends:
 * @info: a #GpAppletInfo
 * @backends: (nullable): a comma-separated list of backends
 *
 * Sets a list of backends that this applet supports.
 */
void
gp_applet_info_set_backends (GpAppletInfo *info,
                             const gchar  *backends)
{
  g_free (info->backends);
  info->backends = g_strdup (backends);
}

void
gp_applet_info_free (GpAppletInfo *info)
{
  if (info == NULL)
    return;

  g_free (info->name);
  g_free (info->description);
  g_free (info->icon_name);

  g_free (info->help_uri);

  g_free (info->backends);

  g_free (info);
}
