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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <gio/gio.h>

#include <libpanel-util/panel-cleanup.h>

#include "gp-applet-manager.h"
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

static PanelAppletsManager *manager = NULL;

static void
_panel_applets_manager_cleanup (gpointer data)
{
	g_clear_object (&manager);
}

static void
_panel_applets_managers_ensure_loaded (void)
{
	if (manager != NULL)
		return;

	panel_cleanup_register (PANEL_CLEAN_FUNC (_panel_applets_manager_cleanup), NULL);

	manager = g_object_new (GP_TYPE_APPLET_MANAGER, NULL);
}

GList *
panel_applets_manager_get_applets (void)
{
	_panel_applets_managers_ensure_loaded ();

	return PANEL_APPLETS_MANAGER_GET_CLASS (manager)->get_applets (manager);
}

gboolean
panel_applets_manager_factory_activate (const gchar *iid)
{
	_panel_applets_managers_ensure_loaded ();

	return PANEL_APPLETS_MANAGER_GET_CLASS (manager)->factory_activate (manager, iid);
}

void
panel_applets_manager_factory_deactivate (const gchar *iid)
{
	_panel_applets_managers_ensure_loaded ();

	PANEL_APPLETS_MANAGER_GET_CLASS (manager)->factory_deactivate (manager, iid);
}

PanelAppletInfo *
panel_applets_manager_get_applet_info (const gchar *iid)
{
	_panel_applets_managers_ensure_loaded ();

	return PANEL_APPLETS_MANAGER_GET_CLASS (manager)->get_applet_info (manager, iid);
}

gboolean
panel_applets_manager_load_applet (const gchar                *iid,
				   PanelAppletFrameActivating *frame_act)
{
	_panel_applets_managers_ensure_loaded ();

	return PANEL_APPLETS_MANAGER_GET_CLASS (manager)->load_applet (manager, iid, frame_act);
}

gchar *
panel_applets_manager_get_new_iid (const gchar *old_iid)
{
	_panel_applets_managers_ensure_loaded ();

	return PANEL_APPLETS_MANAGER_GET_CLASS (manager)->get_new_iid (manager, old_iid);
}

gboolean
panel_applets_manager_open_initial_setup_dialog (const gchar            *iid,
                                                 GtkWindow              *parent,
                                                 GpInitialSetupCallback  callback,
                                                 gpointer                user_data,
                                                 GDestroyNotify          free_func)
{
	gboolean ret;

	_panel_applets_managers_ensure_loaded ();

	ret = PANEL_APPLETS_MANAGER_GET_CLASS (manager)->open_initial_setup_dialog (manager,
	                                                                            iid,
	                                                                            parent,
	                                                                            callback,
	                                                                            user_data,
	                                                                            free_func);

	if (!ret && user_data != NULL && free_func != NULL)
		free_func (user_data);

	return ret;
}

GtkWidget *
panel_applets_manager_get_standalone_menu (void)
{
	_panel_applets_managers_ensure_loaded ();

	return PANEL_APPLETS_MANAGER_GET_CLASS (manager)->get_standalone_menu (manager);
}
