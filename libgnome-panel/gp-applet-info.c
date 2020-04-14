/*
 * Copyright (C) 2016-2020 Alberts Muktupāvels
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
 * @func: the function to call to get #GType of this applet
 * @name: the name of this applet
 * @description: the description of this applet
 * @icon_name: the icon name for this applet
 *
 * Creates a new #GpAppletInfo.
 *
 * Returns: (transfer full): a newly created #GpAppletInfo.
 */
GpAppletInfo *
gp_applet_info_new (GpGetAppletTypeFunc  func,
                    const gchar         *name,
                    const gchar         *description,
                    const gchar         *icon_name)
{
  GpAppletInfo *info;

  info = g_new0 (GpAppletInfo, 1);

  info->get_applet_type_func = func;

  info->name = g_strdup (name);
  info->description = g_strdup (description);
  info->icon_name = g_strdup (icon_name);

  info->initial_setup_dialog_func = NULL;

  info->help_uri = NULL;
  info->about_dialog_func = NULL;

  info->backends = NULL;

  info->is_disabled_func = NULL;

  return info;
}

/**
 * gp_applet_info_set_initial_setup_dialog:
 * @info: a #GpAppletInfo
 * @func: the function to call to setup initial setup dialog
 *
 * Specifies a function to be used to setup initial setup dialog.
 */
void
gp_applet_info_set_initial_setup_dialog (GpAppletInfo             *info,
                                         GpInitialSetupDialogFunc  func)
{
  info->initial_setup_dialog_func = func;
}

/**
 * gp_applet_info_set_help_uri:
 * @info: a #GpAppletInfo
 * @help_uri: the help uri
 *
 * Sets the help uri. Must be in `help:<document>` format. Optional page
 * identifier with options and anchor can be passed to gp_applet_show_help().
 */
void
gp_applet_info_set_help_uri (GpAppletInfo *info,
                             const gchar  *help_uri)
{
  g_free (info->help_uri);
  info->help_uri = g_strdup (help_uri);
}

/**
 * gp_applet_info_set_about_dialog:
 * @info: a #GpAppletInfo
 * @func: the function to call to setup about dialog
 *
 * Specifies a function to be used to setup about dialog.
 */
void
gp_applet_info_set_about_dialog (GpAppletInfo      *info,
                                 GpAboutDialogFunc  func)
{
  info->about_dialog_func = func;
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

/**
 * gp_applet_info_set_is_disabled:
 * @info: a #GpAppletInfo
 * @func: the function to call to check if applet should be disabled
 *
 * Specifies a function to be used to check if applet should be disabled.
 */
void
gp_applet_info_set_is_disabled (GpAppletInfo     *info,
                                GpIsDisabledFunc  func)
{
  info->is_disabled_func = func;
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
