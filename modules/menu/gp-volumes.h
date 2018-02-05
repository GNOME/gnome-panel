/*
 * Copyright (C) 2018 Alberts Muktupāvels
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

#ifndef GP_VOLUMES_H
#define GP_VOLUMES_H

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GP_TYPE_VOLUMES (gp_volumes_get_type ())
G_DECLARE_FINAL_TYPE (GpVolumes, gp_volumes, GP, VOLUMES, GObject)

typedef void (* GpVolumesForeachDrivesFunc)  (GpVolumes *volumes,
                                              GDrive    *drive,
                                              gpointer   user_data);


typedef void (* GpVolumesForeachVolumesFunc) (GpVolumes *volumes,
                                              GVolume   *volume,
                                              gpointer   user_data);

typedef void (* GpVolumesForeachMountsFunc)  (GpVolumes *volumes,
                                              GMount    *mount,
                                              gpointer   user_data);

GpVolumes *gp_volumes_new                   (void);

guint      gp_volumes_get_local_count       (GpVolumes                   *volumes);

guint      gp_volumes_get_remote_count      (GpVolumes                   *volumes);

void       gp_volumes_foreach_local_drives  (GpVolumes                   *volumes,
                                             GpVolumesForeachDrivesFunc   foreach_func,
                                             gpointer                     user_data);

void       gp_volumes_foreach_local_volumes (GpVolumes                   *volumes,
                                             GpVolumesForeachVolumesFunc  foreach_func,
                                             gpointer                     user_data);

void       gp_volumes_foreach_local_mounts  (GpVolumes                   *volumes,
                                             GpVolumesForeachMountsFunc   foreach_func,
                                             gpointer                     user_data);

void       gp_volumes_foreach_remote_mounts (GpVolumes                   *volumes,
                                             GpVolumesForeachMountsFunc   foreach_func,
                                             gpointer                     user_data);

G_END_DECLS

#endif
