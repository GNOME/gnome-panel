#ifndef MAIN_H
#define MAIN_H

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

G_BEGIN_DECLS

GConfClient *panel_main_gconf_client (void);

void         start_screen_check      (void);

G_END_DECLS

#endif
