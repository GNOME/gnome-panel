#ifndef __PANEL_APPLET_CONTROL_H__
#define __PANEL_APPLET_CONTROL_H__

#include "panel-widget.h"

G_BEGIN_DECLS

void panel_applet_load (const gchar *iid,
			PanelWidget *panel,
			gint         pos);

G_END_DECLS

#endif /* __PANEL_APPLET_CONTROL_H__ */
