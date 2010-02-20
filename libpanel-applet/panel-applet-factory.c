/*
 * panel-applet-factory.c: panel applet writing API.
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "panel-applet-factory.h"
#include "_panelappletfactory.h"
#include "panel-applet.h"
#include "_panelapplet.h"

struct _PanelAppletFactory {
	GObject            base;

	gchar             *factory_id;
	EggDBusConnection *connection;

	guint              n_applets;
	GType              applet_type;
	GClosure          *closure;
};

struct _PanelAppletFactoryClass {
	GObjectClass base_class;
};

#define PANEL_APPLET_FACTORY_OBJECT_PATH  "/org/gnome/panel/applet/%s"
#define PANEL_APPLET_FACTORY_SERVICE_NAME "org.gnome.panel.applet.%s"
#define PANEL_APPLET_OBJECT_PATH          "/org/gnome/panel/applet/%s/%d"


static void panel_applet_factory_factory_iface_init (_PanelAppletFactoryIface *iface);

G_DEFINE_TYPE_WITH_CODE (PanelAppletFactory, panel_applet_factory, G_TYPE_OBJECT,
	 G_IMPLEMENT_INTERFACE (_PANEL_TYPE_APPLET_FACTORY, panel_applet_factory_factory_iface_init));

static void
panel_applet_factory_finalize (GObject *object)
{
	PanelAppletFactory *factory = PANEL_APPLET_FACTORY (object);

	if (factory->connection) {
		g_object_unref (factory->connection);
		factory->connection = NULL;
	}

	if (factory->factory_id) {
		g_free (factory->factory_id);
		factory->factory_id = NULL;
	}

	if (factory->closure) {
		g_closure_unref (factory->closure);
		factory->closure = NULL;
	}

	G_OBJECT_CLASS (panel_applet_factory_parent_class)->finalize (object);
}

static void
panel_applet_factory_init (PanelAppletFactory *factory)
{
}

static void
panel_applet_factory_class_init (PanelAppletFactoryClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->finalize = panel_applet_factory_finalize;
}

static void
panel_applet_factory_applet_removed (PanelAppletFactory *factory,
				     GObject            *applet)
{
	if (--factory->n_applets == 0)
		g_object_unref (factory);
}

/* PanelAppletFactory Interface */
static gboolean
set_property (EggDBusHashMap *hash_map,
	      gchar          *name,
	      EggDBusVariant *variant,
	      GObject        *applet)
{
	g_object_set_property (applet, name, egg_dbus_variant_get_gvalue (variant));

	return FALSE;
}

static void
panel_applet_factory_get_applet (_PanelAppletFactory     *_factory,
				 const gchar             *applet_id,
				 gint                     screen_num,
				 EggDBusHashMap          *props,
				 EggDBusMethodInvocation *method_invocation)
{
	PanelAppletFactory *factory = PANEL_APPLET_FACTORY (_factory);
	GObject            *applet;
	gchar              *object_path;
	GdkScreen          *screen;
	guint32             xid;
	static gint         id = 0;

	applet = g_object_new (factory->applet_type,
			       "id", applet_id,
			       "closure", factory->closure,
			       NULL);
	factory->n_applets++;

	object_path = g_strdup_printf (PANEL_APPLET_OBJECT_PATH, applet_id, id++);
	egg_dbus_connection_register_interface (factory->connection,
						object_path,
						_PANEL_TYPE_APPLET,
						G_OBJECT (applet),
						G_TYPE_INVALID);
	g_object_weak_ref (applet, (GWeakNotify)panel_applet_factory_applet_removed, factory);

	egg_dbus_hash_map_foreach (props,
				   (EggDBusHashMapForeachFunc)set_property,
				   applet);

	screen = screen_num != -1 ?
		gdk_display_get_screen (gdk_display_get_default (), screen_num) :
		gdk_screen_get_default ();
	xid = panel_applet_get_xid (PANEL_APPLET (applet), screen);

	_panel_applet_factory_handle_get_applet_finish (method_invocation, object_path, xid);
	g_free (object_path);
}

static void
panel_applet_factory_factory_iface_init (_PanelAppletFactoryIface *factory_iface)
{
	factory_iface->handle_get_applet = panel_applet_factory_get_applet;
}

PanelAppletFactory *
panel_applet_factory_new (const gchar *factory_id,
			  GType        applet_type,
			  GClosure    *closure)
{
	PanelAppletFactory *factory;

	factory = PANEL_APPLET_FACTORY (g_object_new (PANEL_TYPE_APPLET_FACTORY, NULL));
	factory->factory_id = g_strdup (factory_id);
	factory->applet_type = applet_type;
	factory->closure = g_closure_ref (closure);

	return factory;
}

gboolean
panel_applet_factory_register_service (PanelAppletFactory *factory)
{
	guint   request_name_result;
	gchar  *service_name;
	gchar  *object_path;
	GError *error = NULL;

	if (!factory->connection) {
		factory->connection = egg_dbus_connection_get_for_bus (EGG_DBUS_BUS_TYPE_SESSION);
		if (!factory->connection) {
			g_printerr ("Failed to connect to the D-BUS daemon\n");

			return FALSE;
		}
	}

	service_name = g_strdup_printf (PANEL_APPLET_FACTORY_SERVICE_NAME, factory->factory_id);
	if (!egg_dbus_bus_request_name_sync (egg_dbus_connection_get_bus (factory->connection),
					     EGG_DBUS_CALL_FLAGS_NONE,
					     service_name,
					     EGG_DBUS_REQUEST_NAME_FLAGS_DO_NOT_QUEUE,
					     &request_name_result,
					     NULL,
					     &error)) {
		g_printerr ("Failed to acquire daemon name: %s", error->message);
		g_error_free (error);
		g_free (service_name);

		return FALSE;
	}
	g_free (service_name);

	switch (request_name_result) {
	case EGG_DBUS_REQUEST_NAME_REPLY_EXISTS:
		g_printerr ("Panel applet already running, exiting.\n");

		return FALSE;
	case EGG_DBUS_REQUEST_NAME_REPLY_ALREADY_OWNED:
	case EGG_DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
		object_path = g_strdup_printf (PANEL_APPLET_FACTORY_OBJECT_PATH, factory->factory_id);
		egg_dbus_connection_register_interface (factory->connection,
							object_path,
							_PANEL_TYPE_APPLET_FACTORY,
							G_OBJECT (factory),
							G_TYPE_INVALID);
		g_free (object_path);

		break;
	default:
		g_printerr ("Not primary owner of the service, exiting.\n");

		return FALSE;
	}

	return TRUE;
}


