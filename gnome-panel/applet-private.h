#ifndef __APPLET_PRIATE_H__
#define __APPLET_PRIATE_H__

#include "applet-object.h"
#include "applet-widget.h"

#include "GNOME_Panel.h"

G_BEGIN_DECLS

void  applet_object_register               (AppletObject *applet);

GNOME_Panel_RgbImage *
      applet_object_get_rgb_background     (AppletObject  *applet);

void  applet_widget_change_orient          (AppletWidget          *widget,
					    const PanelOrientType  orient);

void  applet_widget_change_size            (AppletWidget      *widget,
					    const CORBA_short  size);

void  applet_widget_change_position        (AppletWidget *widget,
					    const gint    x,
					    const gint    y);

void  applet_widget_background_change      (AppletWidget  *widget,
					    PanelBackType  type,
					    gchar         *pixmap,
					    GdkColor       color);

void  applet_widget_tooltips_enable        (AppletWidget *widget);
void  applet_widget_tooltips_disable       (AppletWidget *widget);
void  applet_widget_draw                   (AppletWidget *widget);

gboolean applet_widget_save_session        (AppletWidget *widget,
					    const gchar  *config_path,
					    const gchar  *global_config_path);

void  applet_widget_freeze_changes         (AppletWidget *widget);
void  applet_widget_thaw_changes           (AppletWidget *widget);

G_END_DECLS

#endif /* __APPLET_PRIATE_H__ */
