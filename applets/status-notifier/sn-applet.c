/*
 * Copyright (C) 2015 Alberts Muktupāvels
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

#include <libstatus-notifier/sn.h>

#include "sn-applet.h"

struct _SnApplet
{
  PanelApplet parent;
};

G_DEFINE_TYPE (SnApplet, sn_applet, PANEL_TYPE_APPLET)

static gboolean
sn_applet_fill (SnApplet *applet)
{
  GtkWidget *label;

  label = gtk_label_new ("Status Notifier Host");

  gtk_container_add (GTK_CONTAINER (applet), label);
  gtk_widget_show (label);

  gtk_widget_show (GTK_WIDGET (applet));

  return TRUE;
}

static gboolean
sn_applet_factory (PanelApplet *applet,
                   const gchar *iid,
                   gpointer     user_data)
{
  if (g_strcmp0 (iid, "SnApplet") == 0)
    return sn_applet_fill (SN_APPLET (applet));

  return FALSE;
}

static void
sn_applet_class_init (SnAppletClass *applet_class)
{
}

static void
sn_applet_init (SnApplet *applet)
{
  PanelApplet *panel_applet;

  panel_applet = PANEL_APPLET (applet);

  panel_applet_set_flags (panel_applet, PANEL_APPLET_HAS_HANDLE |
                          PANEL_APPLET_EXPAND_MINOR);
}

PANEL_APPLET_IN_PROCESS_FACTORY ("SnAppletFactory", SN_TYPE_APPLET,
                                 sn_applet_factory, NULL);
