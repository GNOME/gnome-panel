#ifndef DRAWER_H
#define DRAWER_H

#include "panel.h"

G_BEGIN_DECLS

typedef struct {
	char          *pixmap;
	char          *tooltip;

	PanelToplevel *toplevel;
	GtkWidget     *button;
	GtkWidget     *properties;

	gboolean       opened_for_drag;
	guint          close_timeout_id;

	AppletInfo    *info;
} Drawer;

void    panel_drawer_create (PanelToplevel *toplevel,
			     int            position,
			     const char    *custom_icon,
			     gboolean       use_custom_icon,
			     const char    *tooltip);

void    panel_drawer_set_dnd_enabled (Drawer   *drawer,
				      gboolean  dnd_enabled);

void    drawer_load_from_gconf (PanelWidget *panel_widget,
				gboolean     locked,
				gint         position,
				const char  *id);

void set_drawer_applet_orientation (Drawer           *drawer,
				    PanelOrientation  orientation);

#ifdef FIXME_FOR_NEW_TOPLEVEL
void add_drawer_properties_page(PerPanelConfig *ppc, GtkNotebook *prop_nbook, Drawer *drawer);
#endif

G_END_DECLS

#endif
