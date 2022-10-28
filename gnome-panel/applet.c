/* Gnome panel: general applet functionality
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Federico Mena
 *           Miguel de Icaza
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <gdk/gdkx.h>

#include <libgnome-panel/gp-action-private.h>
#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-show.h>

#include "panel.h"
#include "panel-bindings.h"
#include "panel-applet-frame.h"
#include "panel-toplevel.h"
#include "panel-util.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-schemas.h"

static GSList *registered_applets = NULL;
static GSList *queued_position_saves = NULL;
static guint   queued_position_source = 0;

/* permanently remove an applet - all non-permanent
 * cleanups should go in panel_applet_destroy()
 */
void
panel_applet_clean (AppletInfo *info)
{
	g_return_if_fail (info != NULL);

	if (info->widget) {
		GtkWidget *widget = info->widget;

		info->widget = NULL;
		gtk_widget_destroy (widget);
	}
}

static void
panel_applet_destroy (GtkWidget  *widget,
		      AppletInfo *info)
{
	g_return_if_fail (info != NULL);

	info->widget = NULL;

	registered_applets = g_slist_remove (registered_applets, info);

	queued_position_saves =
		g_slist_remove (queued_position_saves, info);

	if (info->settings)
		g_object_unref (info->settings);
	info->settings = NULL;

	g_free (info->id);
	info->id = NULL;

	g_free (info);
}

const char *
panel_applet_get_toplevel_id (AppletInfo *applet)
{
	PanelWidget *panel_widget;

	g_return_val_if_fail (applet != NULL, NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (applet->widget), NULL);

	panel_widget = panel_applet_get_panel_widget (applet);
	if (!panel_widget)
		return NULL;

	return panel_toplevel_get_id (panel_widget->toplevel);
}

static gboolean
panel_applet_position_save_timeout (gpointer dummy)
{
	GSList *l;

	queued_position_source = 0;

	for (l = queued_position_saves; l; l = l->next) {
		AppletInfo *info = l->data;

		panel_applet_save_position (info, info->id, TRUE);
	}

	g_slist_free (queued_position_saves);
	queued_position_saves = NULL;

	return FALSE;
}

void
panel_applet_save_position (AppletInfo *applet_info,
			    const char *id,
			    gboolean    immediate)
{
	const char  *toplevel_id;
	AppletData  *applet_data;

	g_return_if_fail (applet_info != NULL);
	g_return_if_fail (G_IS_OBJECT (applet_info->widget));

	if (!immediate) {
		if (!queued_position_source)
			queued_position_source =
				g_timeout_add_seconds (1,
						       (GSourceFunc) panel_applet_position_save_timeout,
						       NULL);

		if (!g_slist_find (queued_position_saves, applet_info))
			queued_position_saves =
				g_slist_prepend (queued_position_saves, applet_info);

		return;
	}

	if (!(toplevel_id = panel_applet_get_toplevel_id (applet_info)))
		return;

	applet_data = g_object_get_data (G_OBJECT (applet_info->widget),
					 PANEL_APPLET_DATA);

	g_settings_set_string (applet_info->settings,
			       PANEL_OBJECT_TOPLEVEL_ID_KEY,
			       toplevel_id);
	g_settings_set_enum (applet_info->settings,
			     PANEL_OBJECT_PACK_TYPE_KEY,
			     applet_data->pack_type);
	g_settings_set_int (applet_info->settings,
			    PANEL_OBJECT_PACK_INDEX_KEY,
			    applet_data->pack_index);
}

const char *
panel_applet_get_id (AppletInfo *info)
{
	if (!info)
		return NULL;

	return info->id;
}

GSList *
panel_applet_list_applets (void)
{
	return registered_applets;
}

void
panel_applet_foreach (PanelWidget            *panel,
                      PanelAppletForeachFunc  func,
                      gpointer                user_data)
{
  GSList *applets;
  GSList *l;

  applets = panel_applet_list_applets ();

  for (l = applets; l != NULL; l = l->next)
    {
      AppletInfo *info;

      info = l->data;

      if (panel != NULL && panel != panel_applet_get_panel_widget (info))
        continue;

      func (info, user_data);
    }
}

gboolean
panel_applet_activate_main_menu (guint32 activate_time)
{
  GSList *l;

  for (l = registered_applets; l != NULL; l = l->next)
    {
      AppletInfo *info;
      GtkWidget *applet;

      info = l->data;

      applet = gtk_bin_get_child (GTK_BIN (info->widget));
      if (applet == NULL)
        continue;

      if (!g_type_is_a (G_TYPE_FROM_INSTANCE (applet), GP_TYPE_ACTION))
        continue;

      if (gp_action_handle_action (GP_ACTION (applet),
                                   GP_ACTION_MAIN_MENU,
                                   activate_time))
        return TRUE;
    }

  return FALSE;
}

AppletInfo *
panel_applet_register (GtkWidget       *applet,
		       PanelWidget     *panel,
		       const char      *id,
		       GSettings       *settings)
{
	AppletInfo          *info;
	PanelObjectPackType  pack_type;
	int                  pack_index;
	
	g_return_val_if_fail (applet != NULL && panel != NULL, NULL);

	if (gtk_widget_get_has_window (applet))
		gtk_widget_set_events (applet, (gtk_widget_get_events (applet) |
						APPLET_EVENT_MASK) &
				       ~( GDK_POINTER_MOTION_MASK |
					  GDK_POINTER_MOTION_HINT_MASK));

	info = g_new0 (AppletInfo, 1);
	info->widget       = applet;
	info->settings     = g_object_ref (settings);
	info->id           = g_strdup (id);

	g_object_set_data (G_OBJECT (applet), "applet_info", info);

	registered_applets = g_slist_append (registered_applets, info);

	/* Find where to insert the applet */
        pack_type = g_settings_get_enum (info->settings, PANEL_OBJECT_PACK_TYPE_KEY);
        pack_index = g_settings_get_int (info->settings, PANEL_OBJECT_PACK_INDEX_KEY);

	/* Insert it */
	panel_widget_add (panel, applet, pack_type, pack_index, TRUE);

	g_signal_connect (applet, "destroy",
			  G_CALLBACK (panel_applet_destroy),
			  info);

	gtk_widget_show (applet);

	orientation_change (info, panel);

	gtk_widget_child_focus (applet, GTK_DIR_TAB_FORWARD);

	return info;
}

PanelWidget *
panel_applet_get_panel_widget (AppletInfo *info)
{
  return PANEL_WIDGET (gtk_widget_get_parent (info->widget));
}

GpApplet *
panel_applet_get_applet (AppletInfo *info)
{
  return GP_APPLET (gtk_bin_get_child (GTK_BIN (info->widget)));
}

gboolean
panel_applet_can_freely_move (AppletInfo *applet)
{
	PanelWidget *panel;
	GpApplication *application;
	PanelLockdown *lockdown;

	panel = panel_applet_get_panel_widget (applet);
	application = panel_toplevel_get_application (panel->toplevel);
	lockdown = gp_application_get_lockdown (application);

	/* if we check for more lockdown than this, then we'll need to update
	 * callers that use panel_lockdown_on_notify() */
	if (panel_lockdown_get_panels_locked_down (lockdown))
		return FALSE;

	return (g_settings_is_writable (applet->settings,
					PANEL_OBJECT_TOPLEVEL_ID_KEY) &&
	        g_settings_is_writable (applet->settings,
					PANEL_OBJECT_PACK_TYPE_KEY) &&
	        g_settings_is_writable (applet->settings,
					PANEL_OBJECT_PACK_INDEX_KEY));
}
