/*
 * panel-applets-manager-bonobo.c
 *
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

#include <string.h>
#include <libbonobo.h>

#include <panel-applets-manager.h>

#include "panel-applet-frame-bonobo.h"

#include "panel-applets-manager-bonobo.h"

G_DEFINE_DYNAMIC_TYPE (PanelAppletsManagerBonobo, panel_applets_manager_bonobo, PANEL_TYPE_APPLETS_MANAGER);

struct _PanelAppletsManagerBonoboPrivate
{
	GHashTable *applets;
};

static const char applet_requirements [] =
	"has_all (repo_ids, ['IDL:Bonobo/Control:1.0',"
	"		     'IDL:GNOME/Vertigo/PanelAppletShell:1.0']) && "
	"defined (panel:icon)";

static char *applet_sort_criteria [] = {
	"name",
	NULL
	};

static void
panel_applets_manager_bonobo_load_applet_infos (PanelAppletsManagerBonobo *manager)
{
	Bonobo_ServerInfoList *applet_list;
	CORBA_Environment      env;
	const char * const    *langs;
	GSList                *langs_gslist;
	int                    i;

	CORBA_exception_init (&env);

	applet_list = bonobo_activation_query (applet_requirements,
					       applet_sort_criteria,
					       &env);
	if (BONOBO_EX (&env)) {
		g_warning ("Bonobo query returned exception %s\n",
			   BONOBO_EX_REPOID (&env));

		CORBA_exception_free (&env);
		CORBA_free (applet_list);

		return;
	}

	CORBA_exception_free (&env);

	langs = g_get_language_names ();

	langs_gslist = NULL;
	for (i = 0; langs[i]; i++)
		langs_gslist = g_slist_prepend (langs_gslist, (char *) langs[i]);

	langs_gslist = g_slist_reverse (langs_gslist);

	for (i = 0; i < applet_list->_length; i++) {
		Bonobo_ServerInfo *info;
		const char *name, *description, *icon;
		PanelAppletInfo *applet_info;

		info = &applet_list->_buffer[i];

		name = bonobo_server_info_prop_lookup (info,
						       "name",
						       langs_gslist);
		description = bonobo_server_info_prop_lookup (info,
							      "description",
							      langs_gslist);
		icon = bonobo_server_info_prop_lookup (info,
						       "panel:icon",
						       NULL);

		applet_info = panel_applet_info_new (info->iid, name, description, icon, NULL);

		g_hash_table_insert (manager->priv->applets, g_strdup (info->iid), applet_info);
	}

	g_slist_free (langs_gslist);
	CORBA_free (applet_list);
}

static GList *
panel_applets_manager_bonobo_get_applets (PanelAppletsManager *manager)
{
	PanelAppletsManagerBonobo *bonobo_manager = PANEL_APPLETS_MANAGER_BONOBO (manager);

	GHashTableIter iter;
	gpointer       key, value;
	GList         *retval = NULL;

	g_hash_table_iter_init (&iter, bonobo_manager->priv->applets);
	while (g_hash_table_iter_next (&iter, &key, &value))
		retval = g_list_prepend (retval, value);

	return g_list_reverse (retval);;
}

static gboolean
panel_applets_manager_bonobo_factory_activate (PanelAppletsManager *manager,
					       const gchar         *iid)
{
	PanelAppletsManagerBonobo *bonobo_manager = PANEL_APPLETS_MANAGER_BONOBO (manager);
	PanelAppletInfo *info;

	/* we let bonobo deal with that, but we need to return the right value */

	info = g_hash_table_lookup (bonobo_manager->priv->applets, iid);

	return (info != NULL);
}

static gboolean
panel_applets_manager_bonobo_factory_deactivate (PanelAppletsManager *manager,
						 const gchar         *iid)
{
	PanelAppletsManagerBonobo *bonobo_manager = PANEL_APPLETS_MANAGER_BONOBO (manager);
	PanelAppletInfo *info;

	/* we let bonobo deal with that, but we need to return the right value */

	info = g_hash_table_lookup (bonobo_manager->priv->applets, iid);

	return (info != NULL);
}

static PanelAppletInfo *
panel_applets_manager_bonobo_get_applet_info (PanelAppletsManager *manager,
					      const gchar         *iid)
{
	PanelAppletsManagerBonobo *bonobo_manager = PANEL_APPLETS_MANAGER_BONOBO (manager);

	return g_hash_table_lookup (bonobo_manager->priv->applets, iid);
}

static PanelAppletInfo *
panel_applets_manager_bonobo_get_applet_info_from_old_id (PanelAppletsManager *manager,
							  const gchar         *iid)
{
	PanelAppletsManagerBonobo *bonobo_manager = PANEL_APPLETS_MANAGER_BONOBO (manager);

	return g_hash_table_lookup (bonobo_manager->priv->applets, iid);
}

static gboolean
panel_applets_manager_bonobo_load_applet (PanelAppletsManager         *manager,
					const gchar                 *iid,
					PanelAppletFrameActivating  *frame_act)
{
	return panel_applet_frame_bonobo_load (iid, frame_act);
}

static void
panel_applets_manager_bonobo_finalize (GObject *object)
{
	PanelAppletsManagerBonobo *manager = PANEL_APPLETS_MANAGER_BONOBO (object);

	if (manager->priv->applets) {
		g_hash_table_destroy (manager->priv->applets);
		manager->priv->applets = NULL;
	}

	G_OBJECT_CLASS (panel_applets_manager_bonobo_parent_class)->finalize (object);
}

static void
panel_applets_manager_bonobo_init (PanelAppletsManagerBonobo *manager)
{
	manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
						     PANEL_TYPE_APPLETS_MANAGER_BONOBO,
						     PanelAppletsManagerBonoboPrivate);

	manager->priv->applets = g_hash_table_new_full (g_str_hash,
								 g_str_equal,
								 (GDestroyNotify) g_free,
								 (GDestroyNotify) panel_applet_info_free);

	panel_applets_manager_bonobo_load_applet_infos (manager);
}

static void
panel_applets_manager_bonobo_class_finalize (PanelAppletsManagerBonoboClass *class)
{
}

static void
panel_applets_manager_bonobo_class_init (PanelAppletsManagerBonoboClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);
	PanelAppletsManagerClass *manager_class = PANEL_APPLETS_MANAGER_CLASS (class);

	/* This is a horrible hack: we shouldn't call bonobo_init() here, but
	 * in g_io_module_load() or even
	 * panel_applets_manager_bonobo_register(). However, it looks like if
	 * there's no giomodule.cache file, the module gets unloaded, and
	 * bonobo_init() gets called twice, which makes everythings goes wrong:
	 * bonobo has been unloaded so believes it has to get initialized, but
	 * the types are already registered in the GType system. And bam. */

	bonobo_init (NULL, NULL);

	gobject_class->finalize = panel_applets_manager_bonobo_finalize;

	manager_class->get_applets = panel_applets_manager_bonobo_get_applets;
	manager_class->factory_activate = panel_applets_manager_bonobo_factory_activate;
	manager_class->factory_deactivate = panel_applets_manager_bonobo_factory_deactivate;
	manager_class->get_applet_info = panel_applets_manager_bonobo_get_applet_info;
	manager_class->get_applet_info_from_old_id = panel_applets_manager_bonobo_get_applet_info_from_old_id;
	manager_class->load_applet = panel_applets_manager_bonobo_load_applet;

	g_type_class_add_private (class, sizeof (PanelAppletsManagerBonoboPrivate));
}


void
panel_applets_manager_bonobo_register (GIOModule *module)
{
	panel_applets_manager_bonobo_register_type (G_TYPE_MODULE (module));
	g_io_extension_point_implement (PANEL_APPLETS_MANAGER_EXTENSION_POINT_NAME,
					PANEL_TYPE_APPLETS_MANAGER_BONOBO,
					"bonobo",
					10);
}
