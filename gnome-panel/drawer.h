#ifndef DRAWER_H
#define DRAWER_H

#include "panel.h"
#include "panel-config.h"

G_BEGIN_DECLS

#define DRAWER_PANEL_KEY "drawer_panel"

typedef struct {
	char *pixmap;
	char *tooltip;
	GtkWidget *button;
	GtkWidget *drawer;
	GtkWidget *properties;
} Drawer;

Drawer *load_drawer_applet (gchar       *mypanel,
			    const char  *pixmap,
			    const char  *tooltip,
			    PanelWidget *panel,
			    int          pos,
			    gboolean     exactpos,
			    const char  *gconf_key);

void    drawer_save_to_gconf   (Drawer     *drawer,
				const char *gconf_key);

void    drawer_load_from_gconf (PanelWidget *panel_widget,
				gint         position,
				const char  *gconf_key,
				gboolean     use_default);

void set_drawer_applet_orient(Drawer *drawer, PanelOrient orient);

void add_drawer_properties_page(PerPanelConfig *ppc, GtkNotebook *prop_nbook, Drawer *drawer);

G_END_DECLS

#endif
