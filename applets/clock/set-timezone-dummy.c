/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include "set-timezone.h"

gboolean 
set_system_timezone (const char  *filename,
                     GError     **err)
{
	g_set_error (err, 
                     G_SPAWN_ERROR, 
                     G_SPAWN_ERROR_FAILED, 
                     "Timezone setting is not supported");

	return FALSE;
}

gint
can_set_system_timezone (void)
{
	return 0;
}


gint
can_set_system_time (void)
{
	return 0;
}

void
set_system_time_async (gint64         time,
                       GFunc          callback,
                       gpointer       data,
                       GDestroyNotify notify)
{
	GError *error;

	if (callback) {
		error = g_error_new_literal (G_SPAWN_ERROR,
					     G_SPAWN_ERROR_FAILED,
					     "Time setting is not supported");
		callback (data, error);

		g_error_free (error);
	}

	if (notify)
		notify (data);
}
