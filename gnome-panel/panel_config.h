#ifndef PANEL_CONFIG_H
#define PANEL_CONFIG_H

#include "panel-widget.h"

void panel_config(GtkWidget *panel);
void update_config_orient(GtkWidget *panel);
void update_config_back(PanelWidget *panel);
void kill_config_dialog(GtkWidget *panel);

#endif /* PANEL_CONFIG_H */
