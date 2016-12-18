#ifndef PANEL_H
#define PANEL_H

#include <gtk/gtk.h>
#include "panel-toplevel.h"
#include "panel-widget.h"
#include "applet.h"

G_BEGIN_DECLS

void orientation_change(AppletInfo *info, PanelWidget *panel);

void panel_setup (PanelToplevel *toplevel);

GdkScreen *panel_screen_from_panel_widget  (PanelWidget *panel);

void panel_delete (PanelToplevel *toplevel);

GtkWidget  *panel_deletion_dialog  (PanelToplevel *toplevel);

G_END_DECLS

#endif
