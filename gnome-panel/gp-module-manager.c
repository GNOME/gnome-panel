/*
 * Copyright (C) 2018-2020 Alberts Muktupāvels
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

#include <libgnome-panel/gp-image-menu-item.h>
#include <libgnome-panel/gp-module-private.h>

#include "gp-module-manager.h"

struct _GpModuleManager
{
  GObject     parent;

  GHashTable *modules;
};

G_DEFINE_TYPE (GpModuleManager, gp_module_manager, G_TYPE_OBJECT)

static gint
sort_modules (gconstpointer a,
              gconstpointer b)
{
  GpModule *a_module;
  GpModule *b_module;

  a_module = (GpModule *) a;
  b_module = (GpModule *) b;

  return g_strcmp0 (gp_module_get_id (a_module), gp_module_get_id (b_module));
}

static void
load_modules (GpModuleManager *self)
{
  GDir *dir;
  const gchar *name;

  dir = g_dir_open (MODULESDIR, 0, NULL);
  if (!dir)
    return;

  while ((name = g_dir_read_name (dir)) != NULL)
    {
      gchar *path;
      GpModule *module;
      const gchar *id;

      if (!g_str_has_suffix (name, ".so"))
        continue;

      path = g_build_filename (MODULESDIR, name, NULL);
      module = gp_module_new_from_path (path);
      g_free (path);

      if (module == NULL)
        continue;

      id = gp_module_get_id (module);
      g_hash_table_insert (self->modules, g_strdup (id), module);
    }

  g_dir_close (dir);
}

static void
gp_module_manager_finalize (GObject *object)
{
  GpModuleManager *self;

  self = GP_MODULE_MANAGER (object);

  g_clear_pointer (&self->modules, g_hash_table_destroy);

  G_OBJECT_CLASS (gp_module_manager_parent_class)->finalize (object);
}

static void
gp_module_manager_class_init (GpModuleManagerClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->finalize = gp_module_manager_finalize;

  g_type_ensure (GP_TYPE_IMAGE_MENU_ITEM);
}

static void
gp_module_manager_init (GpModuleManager *self)
{
  self->modules = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         g_object_unref);

  load_modules (self);
}

GpModuleManager *
gp_module_manager_new (void)
{
  return g_object_new (GP_TYPE_MODULE_MANAGER, NULL);
}

GList *
gp_module_manager_get_modules (GpModuleManager *self)
{
  GList *modules;

  modules = g_hash_table_get_values (self->modules);
  modules = g_list_sort (modules, sort_modules);

  return modules;
}

GpModule *
gp_module_manager_get_module (GpModuleManager *self,
                              const char      *id)
{
  return g_hash_table_lookup (self->modules, id);
}
