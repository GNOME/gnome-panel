/*
 * panel-applets-manager.c
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

#include <gio/gio.h>

#include <libpanel-util/panel-cleanup.h>

#include "panel-modules.h"

#include "panel-applets-manager.h"

G_DEFINE_ABSTRACT_TYPE (PanelAppletsManager, panel_applets_manager, G_TYPE_OBJECT)

static void
panel_applets_manager_init (PanelAppletsManager *manager)
{
}

static void
panel_applets_manager_class_init (PanelAppletsManagerClass *class)
{
}

/* Generic methods */

static GSList *panel_applets_managers = NULL;

static void
_panel_applets_manager_cleanup (gpointer data)
{
	g_slist_foreach (panel_applets_managers, (GFunc) g_object_unref, NULL);
	g_slist_free (panel_applets_managers);
	panel_applets_managers = NULL;
}

static void
_panel_applets_managers_ensure_loaded (void)
{
	GIOExtensionPoint *point;
	GList             *extensions, *l;

	if (panel_applets_managers != NULL)
		return;

	panel_cleanup_register (PANEL_CLEAN_FUNC (_panel_applets_manager_cleanup), NULL);

	panel_modules_ensure_loaded ();

	point = g_io_extension_point_lookup (PANEL_APPLETS_MANAGER_EXTENSION_POINT_NAME);

	extensions = g_io_extension_point_get_extensions (point);

	if (extensions == NULL)
		g_error ("No PanelAppletsManager implementations exist.");

	for (l = extensions; l != NULL; l = l->next) {
		GIOExtension *extension;
		GType         type;
		GObject      *object;

		extension = l->data;
		type = g_io_extension_get_type (extension);
		object = g_object_new (type, NULL);
		panel_applets_managers = g_slist_prepend (panel_applets_managers, object);
	}

	panel_applets_managers = g_slist_reverse (panel_applets_managers);
}

GList *
panel_applets_manager_get_applets (void)
{
	GSList *l;
	GList  *retval = NULL;

	_panel_applets_managers_ensure_loaded ();

	for (l = panel_applets_managers; l != NULL; l = l->next) {
		GList *applets;
		PanelAppletsManager *manager = PANEL_APPLETS_MANAGER (l->data);

		applets = PANEL_APPLETS_MANAGER_GET_CLASS (manager)->get_applets (manager);
		if (applets)
			retval = g_list_concat (retval, applets);
	}

	return retval;
}

gboolean
panel_applets_manager_factory_activate (const gchar *iid)
{
	GSList *l;

	_panel_applets_managers_ensure_loaded ();

	for (l = panel_applets_managers; l != NULL; l = l->next) {
		PanelAppletsManager *manager = PANEL_APPLETS_MANAGER (l->data);

		if (PANEL_APPLETS_MANAGER_GET_CLASS (manager)->factory_activate (manager, iid))
			return TRUE;
	}

	return FALSE;
}

void
panel_applets_manager_factory_deactivate (const gchar *iid)
{
	GSList *l;

	_panel_applets_managers_ensure_loaded ();

	for (l = panel_applets_managers; l != NULL; l = l->next) {
		PanelAppletsManager *manager = PANEL_APPLETS_MANAGER (l->data);

		if (PANEL_APPLETS_MANAGER_GET_CLASS (manager)->factory_deactivate (manager, iid))
			return;
	}
}

PanelAppletInfo *
panel_applets_manager_get_applet_info (const gchar *iid)
{
	GSList *l;
	PanelAppletInfo *retval = NULL;

	_panel_applets_managers_ensure_loaded ();

	for (l = panel_applets_managers; l != NULL; l = l->next) {
		PanelAppletsManager *manager = PANEL_APPLETS_MANAGER (l->data);

		retval = PANEL_APPLETS_MANAGER_GET_CLASS (manager)->get_applet_info (manager, iid);

		if (retval != NULL)
			return retval;
	}

	return NULL;
}

PanelAppletInfo *
panel_applets_manager_get_applet_info_from_old_id (const gchar *iid)
{
	GSList *l;
	PanelAppletInfo *retval = NULL;

	_panel_applets_managers_ensure_loaded ();

	for (l = panel_applets_managers; l != NULL; l = l->next) {
		PanelAppletsManager *manager = PANEL_APPLETS_MANAGER (l->data);

		retval = PANEL_APPLETS_MANAGER_GET_CLASS (manager)->get_applet_info_from_old_id (manager, iid);

		if (retval != NULL)
			return retval;
	}

	return NULL;
}

gboolean
panel_applets_manager_load_applet (const gchar                *iid,
				   PanelAppletFrameActivating *frame_act)
{
	GSList *l;

	_panel_applets_managers_ensure_loaded ();

	for (l = panel_applets_managers; l != NULL; l = l->next) {
		PanelAppletsManager *manager = PANEL_APPLETS_MANAGER (l->data);

		if (!PANEL_APPLETS_MANAGER_GET_CLASS (manager)->get_applet_info (manager, iid))
			continue;

		return PANEL_APPLETS_MANAGER_GET_CLASS (manager)->load_applet (manager, iid, frame_act);
	}

	return FALSE;
}
