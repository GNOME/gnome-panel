/*
 * panel-applet-info.c
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2010 Vincent Untz <vuntz@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include "panel-applet-info.h"

struct _PanelAppletInfo {
	gchar  *iid;

	gchar  *name;
	gchar  *comment;
	gchar  *icon;

	gchar **old_ids;
};

PanelAppletInfo *
panel_applet_info_new (const gchar  *iid,
		       const gchar  *name,
		       const gchar  *comment,
		       const gchar  *icon,
		       const gchar **old_ids)
{
	PanelAppletInfo *info;
	int len;

	info = g_slice_new0 (PanelAppletInfo);

	info->iid = g_strdup (iid);
	info->name = g_strdup (name);
	info->comment = g_strdup (comment);
	info->icon = g_strdup (icon);

	/* Bonobo compatibility */
	if (old_ids != NULL) {
		len = g_strv_length ((gchar **) old_ids);
		if (len > 0) {
			int i;

			info->old_ids = g_new0 (gchar *, len + 1);

			for (i = 0; i < len; i++)
				info->old_ids[i] = g_strdup (old_ids[i]);
		}
	}

	return info;
}

void
panel_applet_info_free (PanelAppletInfo *info)
{
	if (!info)
		return;

	g_free (info->iid);
	g_free (info->name);
	g_free (info->comment);
	g_free (info->icon);
	g_strfreev (info->old_ids);

	g_slice_free (PanelAppletInfo, info);
}

const gchar *
panel_applet_info_get_iid (PanelAppletInfo *info)
{
	return info->iid;
}

const gchar *
panel_applet_info_get_name (PanelAppletInfo *info)
{
	return info->name;
}

const gchar *
panel_applet_info_get_description (PanelAppletInfo *info)
{
	return info->comment;
}

const gchar *
panel_applet_info_get_icon (PanelAppletInfo *info)
{
	return info->icon;
}

const gchar * const *
panel_applet_info_get_old_ids (PanelAppletInfo *info)
{
	return (const gchar * const *) info->old_ids;
}
