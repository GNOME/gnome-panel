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

#ifndef GP_APPLET_INFO_PRIVATE_H
#define GP_APPLET_INFO_PRIVATE_H

#include "libgnome-panel/gp-applet-info.h"

struct _GpAppletInfo
{
  gchar    *name;
  gchar    *description;
  gchar    *icon;

  gchar    *help_uri;
  gboolean  has_about_dialog;

  gchar    *backends;
};

void gp_applet_info_free (GpAppletInfo *info);

G_END_DECLS

#endif
