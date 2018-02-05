/*
 * Copyright (C) 2005 Vincent Untz
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     Vincent Untz <vincent@vuntz.net>
 */

#include "config.h"
#include "gp-volumes.h"

struct _GpVolumes
{
  GObject         parent;

  GVolumeMonitor *monitor;

  GHashTable     *local_drives;
  GHashTable     *local_volumes;
  GHashTable     *local_mounts;

  GHashTable     *remote_mounts;

  gulong          drive_changed_id;
  gulong          drive_connected_id;
  gulong          drive_disconnected_id;

  gulong          mount_added_id;
  gulong          mount_changed_id;
  gulong          mount_removed_id;

  gulong          volume_added_id;
  gulong          volume_changed_id;
  gulong          volume_removed_id;
};

enum
{
  CHANGED,

  LAST_SIGNAL
};

static guint volumes_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpVolumes, gp_volumes, G_TYPE_OBJECT)

static void
add_drive (GpVolumes *volumes,
           GDrive    *drive)
{
  GList *list;

  list = g_drive_get_volumes (drive);

  if (list != NULL)
    {
      GList *l;

      for (l = list; l != NULL; l = l->next)
        {
          GVolume *volume;
          GMount *mount;

          volume = G_VOLUME (l->data);
          mount = g_volume_get_mount (volume);

          if (mount != NULL)
            {
              g_hash_table_replace (volumes->local_mounts, mount,
                                    g_object_ref (mount));

              g_object_unref (mount);
            }
          else
            {
              /* Do show the unmounted volumes; this is so the user can
               * mount it (in case automounting is off).
               *
               * Also, even if automounting is enabled, this gives a
               * visual cue that the user should remember to yank out
               * the media if he just unmounted it.
               */
              g_hash_table_replace (volumes->local_volumes, volume,
                                    g_object_ref (volume));
            }
        }

      g_list_free_full (list, g_object_unref);
    }
  else
    {
      /* If the drive has no mountable volumes and we cannot detect
       * media change.. we display the drive so the user can manually
       * poll the drive by clicking on it...
       *
       * This is mainly for drives like floppies where media detection
       * doesn't work.. but it's also for human beings who like to turn
       * off media detection in the OS to save battery juice.
       */
      if (g_drive_is_media_removable (drive) &&
          !g_drive_is_media_check_automatic (drive))
        {
          g_hash_table_replace (volumes->local_drives, drive,
                                g_object_ref (drive));
        }
    }
}

static void
add_volume (GpVolumes *volumes,
            GVolume   *volume)
{
  GDrive *drive;
  GMount *mount;

  drive = g_volume_get_drive (volume);

  if (drive != NULL)
    {
      g_object_unref (drive);
      return;
    }

  mount = g_volume_get_mount (volume);

  if (mount != NULL)
    {
      g_hash_table_replace (volumes->local_mounts, mount,
                            g_object_ref (mount));

      g_object_unref (mount);
    }
  else
    {
      /* See comment in add_drive() why we add an icon for an
       * unmounted mountable volume.
       */
      g_hash_table_replace (volumes->local_volumes, volume,
                            g_object_ref (volume));
    }
}

static void
add_mount (GpVolumes *volumes,
           GMount    *mount)
{
  GVolume *volume;
  GFile *root;

  if (g_mount_is_shadowed (mount))
    return;

  volume = g_mount_get_volume (mount);

  if (volume != NULL)
    {
      g_object_unref (volume);
      return;
    }

  root = g_mount_get_root (mount);

  if (g_file_is_native (root))
    {
      g_hash_table_replace (volumes->local_mounts, mount,
                            g_object_ref (mount));
    }
  else
    {
      g_hash_table_replace (volumes->remote_mounts, mount,
                            g_object_ref (mount));
    }

  g_object_unref (root);
}

static void
get_all_drives (GpVolumes *volumes)
{
  GList *drives;
  GList *l;

  drives = g_volume_monitor_get_connected_drives (volumes->monitor);

  for (l = drives; l != NULL; l = l->next)
    add_drive (volumes, l->data);

  g_list_free_full (drives, g_object_unref);
}

static void
get_all_volumes (GpVolumes *volumes)
{
  GList *list;
  GList *l;

  list = g_volume_monitor_get_volumes (volumes->monitor);

  for (l = list; l != NULL; l = l->next)
    add_volume (volumes, l->data);

  g_list_free_full (list, g_object_unref);
}

static void
get_all_mounts (GpVolumes *volumes)
{
  GList *mounts;
  GList *l;

  mounts = g_volume_monitor_get_mounts (volumes->monitor);

  for (l = mounts; l != NULL; l = l->next)
    add_mount (volumes, l->data);

  g_list_free_full (mounts, g_object_unref);
}

static void
get_all (GpVolumes *volumes)
{
  g_hash_table_remove_all (volumes->local_drives);
  g_hash_table_remove_all (volumes->local_volumes);
  g_hash_table_remove_all (volumes->local_mounts);
  g_hash_table_remove_all (volumes->remote_mounts);

  get_all_drives (volumes);
  get_all_volumes (volumes);
  get_all_mounts (volumes);
}

static void
emit_changed (GpVolumes *volumes)
{
  g_signal_emit (volumes, volumes_signals[CHANGED], 0);
}

static void
drive_changed_cb (GVolumeMonitor *monitor,
                  GDrive         *drive,
                  GpVolumes      *volumes)
{
  get_all (volumes);
  emit_changed (volumes);
}

static void
drive_connected_cb (GVolumeMonitor *monitor,
                    GDrive         *drive,
                    GpVolumes      *volumes)
{
  get_all (volumes);
  emit_changed (volumes);
}

static void
drive_disconnected_cb (GVolumeMonitor *monitor,
                       GDrive         *drive,
                       GpVolumes      *volumes)
{
  get_all (volumes);
  emit_changed (volumes);
}

static void
mount_added_cb (GVolumeMonitor *monitor,
                GMount         *mount,
                GpVolumes      *volumes)
{
  get_all (volumes);
  emit_changed (volumes);
}

static void
mount_changed_cb (GVolumeMonitor *monitor,
                  GMount         *mount,
                  GpVolumes      *volumes)
{
  get_all (volumes);
  emit_changed (volumes);
}

static void
mount_removed_cb (GVolumeMonitor *monitor,
                  GMount         *mount,
                  GpVolumes      *volumes)
{
  get_all (volumes);
  emit_changed (volumes);
}

static void
volume_added_cb (GVolumeMonitor *monitor,
                 GVolume        *volume,
                 GpVolumes      *volumes)
{
  get_all (volumes);
  emit_changed (volumes);
}

static void
volume_changed_cb (GVolumeMonitor *monitor,
                   GVolume        *volume,
                   GpVolumes      *volumes)
{
  get_all (volumes);
  emit_changed (volumes);
}

static void
volume_removed_cb (GVolumeMonitor *monitor,
                   GVolume        *volume,
                   GpVolumes      *volumes)
{
  get_all (volumes);
  emit_changed (volumes);
}

static GHashTable *
create_hash_table (void)
{
  return g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
                                (GDestroyNotify) g_object_unref);
}

static void
gp_volumes_dispose (GObject *object)
{
  GpVolumes *volumes;

  volumes = GP_VOLUMES (object);

  if (volumes->drive_changed_id != 0)
    {
      g_signal_handler_disconnect (volumes->monitor, volumes->drive_changed_id);
      volumes->drive_changed_id = 0;
    }

  if (volumes->drive_connected_id != 0)
    {
      g_signal_handler_disconnect (volumes->monitor, volumes->drive_connected_id);
      volumes->drive_connected_id = 0;
    }

  if (volumes->drive_disconnected_id != 0)
    {
      g_signal_handler_disconnect (volumes->monitor, volumes->drive_disconnected_id);
      volumes->drive_disconnected_id = 0;
    }

  if (volumes->mount_added_id != 0)
    {
      g_signal_handler_disconnect (volumes->monitor, volumes->mount_added_id);
      volumes->mount_added_id = 0;
    }

  if (volumes->mount_changed_id != 0)
    {
      g_signal_handler_disconnect (volumes->monitor, volumes->mount_changed_id);
      volumes->mount_changed_id = 0;
    }

  if (volumes->mount_removed_id != 0)
    {
      g_signal_handler_disconnect (volumes->monitor, volumes->mount_removed_id);
      volumes->mount_removed_id = 0;
    }

  if (volumes->volume_added_id != 0)
    {
      g_signal_handler_disconnect (volumes->monitor, volumes->volume_added_id);
      volumes->volume_added_id = 0;
    }

  if (volumes->volume_changed_id != 0)
    {
      g_signal_handler_disconnect (volumes->monitor, volumes->volume_changed_id);
      volumes->volume_changed_id = 0;
    }

  if (volumes->volume_removed_id != 0)
    {
      g_signal_handler_disconnect (volumes->monitor, volumes->volume_removed_id);
      volumes->volume_removed_id = 0;
    }

  g_clear_pointer (&volumes->local_drives, g_hash_table_destroy);
  g_clear_pointer (&volumes->local_volumes, g_hash_table_destroy);
  g_clear_pointer (&volumes->local_mounts, g_hash_table_destroy);
  g_clear_pointer (&volumes->remote_mounts, g_hash_table_destroy);

  g_clear_object (&volumes->monitor);

  G_OBJECT_CLASS (gp_volumes_parent_class)->dispose (object);
}

static void
gp_volumes_class_init (GpVolumesClass *volumes_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (volumes_class);

  object_class->dispose = gp_volumes_dispose;

  volumes_signals[CHANGED] =
    g_signal_new ("changed", GP_TYPE_VOLUMES, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gp_volumes_init (GpVolumes *volumes)
{
  volumes->monitor = g_volume_monitor_get ();

  volumes->local_drives = create_hash_table ();
  volumes->local_volumes = create_hash_table ();
  volumes->local_mounts = create_hash_table ();
  volumes->remote_mounts = create_hash_table ();

  volumes->drive_changed_id = g_signal_connect (volumes->monitor,
                                                "drive-changed",
                                                G_CALLBACK (drive_changed_cb),
                                                volumes);

  volumes->drive_connected_id = g_signal_connect (volumes->monitor,
                                                  "drive-connected",
                                                  G_CALLBACK (drive_connected_cb),
                                                  volumes);

  volumes->drive_disconnected_id = g_signal_connect (volumes->monitor,
                                                     "drive-disconnected",
                                                     G_CALLBACK (drive_disconnected_cb),
                                                     volumes);

  volumes->mount_added_id = g_signal_connect (volumes->monitor,
                                              "mount-added",
                                              G_CALLBACK (mount_added_cb),
                                              volumes);

  volumes->mount_changed_id = g_signal_connect (volumes->monitor,
                                                "mount-changed",
                                                G_CALLBACK (mount_changed_cb),
                                                volumes);

  volumes->mount_removed_id = g_signal_connect (volumes->monitor,
                                                "mount-removed",
                                                G_CALLBACK (mount_removed_cb),
                                                volumes);

  volumes->volume_added_id = g_signal_connect (volumes->monitor,
                                               "volume-added",
                                               G_CALLBACK (volume_added_cb),
                                               volumes);

  volumes->volume_changed_id = g_signal_connect (volumes->monitor,
                                                 "volume-changed",
                                                 G_CALLBACK (volume_changed_cb),
                                                 volumes);

  volumes->volume_removed_id = g_signal_connect (volumes->monitor,
                                                 "volume-removed",
                                                 G_CALLBACK (volume_removed_cb),
                                                 volumes);

  get_all (volumes);
}

GpVolumes *
gp_volumes_new (void)
{
  return g_object_new (GP_TYPE_VOLUMES, NULL);
}

guint
gp_volumes_get_local_count (GpVolumes *volumes)
{
  guint count;

  count = g_hash_table_size (volumes->local_drives);
  count += g_hash_table_size (volumes->local_volumes);
  count += g_hash_table_size (volumes->local_mounts);

  return count;
}

guint
gp_volumes_get_remote_count (GpVolumes *volumes)
{
  return g_hash_table_size (volumes->remote_mounts);
}

void
gp_volumes_foreach_local_drives (GpVolumes                  *volumes,
                                 GpVolumesForeachDrivesFunc  foreach_func,
                                 gpointer                    user_data)
{
  GList *values;
  GList *l;

  values = g_hash_table_get_values (volumes->local_drives);

  for (l = values; l != NULL; l = l->next)
    foreach_func (volumes, l->data, user_data);

  g_list_free (values);
}

void
gp_volumes_foreach_local_volumes (GpVolumes                   *volumes,
                                  GpVolumesForeachVolumesFunc  foreach_func,
                                  gpointer                     user_data)
{
  GList *values;
  GList *l;

  values = g_hash_table_get_values (volumes->local_volumes);

  for (l = values; l != NULL; l = l->next)
    foreach_func (volumes, l->data, user_data);

  g_list_free (values);
}

void
gp_volumes_foreach_local_mounts (GpVolumes                  *volumes,
                                 GpVolumesForeachMountsFunc  foreach_func,
                                 gpointer                    user_data)
{
  GList *values;
  GList *l;

  values = g_hash_table_get_values (volumes->local_mounts);

  for (l = values; l != NULL; l = l->next)
    foreach_func (volumes, l->data, user_data);

  g_list_free (values);
}

void
gp_volumes_foreach_remote_mounts (GpVolumes                  *volumes,
                                  GpVolumesForeachMountsFunc  foreach_func,
                                  gpointer                    user_data)
{
  GList *values;
  GList *l;

  values = g_hash_table_get_values (volumes->remote_mounts);

  for (l = values; l != NULL; l = l->next)
    foreach_func (volumes, l->data, user_data);

  g_list_free (values);
}
