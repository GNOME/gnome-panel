/*
 * Copyright (C) 2019 Alberts Muktupāvels
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
#include "gp-application.h"

struct _GpApplication
{
  GObject parent;
};

G_DEFINE_TYPE (GpApplication, gp_application, G_TYPE_OBJECT)

static void
gp_application_class_init (GpApplicationClass *self_class)
{
}

static void
gp_application_init (GpApplication *self)
{
}

GpApplication *
gp_application_new (void)
{
  return g_object_new (GP_TYPE_APPLICATION,
                       NULL);
}
