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

static GpAppletManager *manager = NULL;

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

	manager = gp_applet_manager_new ();
}

GpModuleManager *
panel_applets_manager_get_module_manager (void)
{
	_panel_applets_managers_ensure_loaded ();

	return gp_applet_manager_get_module_manager (manager);
}

gboolean
panel_applets_manager_factory_activate (const gchar *iid)
{
	_panel_applets_managers_ensure_loaded ();

	return gp_applet_manager_factory_activate (manager, iid);
}

void
panel_applets_manager_factory_deactivate (const gchar *iid)
{
	_panel_applets_managers_ensure_loaded ();

	gp_applet_manager_factory_deactivate (manager, iid);
}

GpAppletInfo *
panel_applets_manager_get_applet_info (const gchar *iid)
{
	_panel_applets_managers_ensure_loaded ();

	return gp_applet_manager_get_applet_info (manager, iid);
}

gboolean
panel_applets_manager_load_applet (const gchar                *iid,
				   PanelAppletFrameActivating *frame_act)
{
	_panel_applets_managers_ensure_loaded ();

	return gp_applet_manager_load_applet (manager, iid, frame_act);
}

gchar *
panel_applets_manager_get_new_iid (const gchar *old_iid)
{
	_panel_applets_managers_ensure_loaded ();

	return gp_applet_manager_get_new_iid (manager, old_iid);
}

gboolean
panel_applets_manager_open_initial_setup_dialog (const gchar            *iid,
                                                 GVariant               *settings,
                                                 GtkWindow              *parent,
                                                 GpInitialSetupCallback  callback,
                                                 gpointer                user_data,
                                                 GDestroyNotify          free_func)
{
	gboolean ret;

	_panel_applets_managers_ensure_loaded ();

	ret = gp_applet_manager_open_initial_setup_dialog (manager,
	                                                   iid,
	                                                   settings,
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

	return gp_applet_manager_get_standalone_menu (manager);
}

gboolean
panel_applets_manager_is_applet_disabled (const char  *iid,
                                          char       **reason)
{
	_panel_applets_managers_ensure_loaded ();

	return gp_applet_manager_is_applet_disabled (manager, iid, reason);
}
