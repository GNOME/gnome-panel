#ifndef __APPLET_OBJECT_H__
#define __APPLET_OBJECT_H__

#include <libbonobo.h>
#include <gtk/gtk.h>

#include "GNOME_Panel.h"

G_BEGIN_DECLS

#define APPLET_OBJECT_TYPE          (applet_object_get_type ())
#define APPLET_OBJECT(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), APPLET_OBJECT_TYPE, AppletObject))
#define APPLET_OBJECT_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST    ((k), APPLET_OBJECT_TYPE, AppletObjectClass))

#define APPLET_IS_OBJECT(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), APPLET_OBJECT_TYPE))
#define APPLET_IS_OBJECT_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE    ((k), APPLET_OBJECT_TYPE))
#define APPLET_OBJECT_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS  ((o), APPLET_OBJECT_TYPE, AppletObjectClass))

typedef struct _AppletObjectPrivate AppletObjectPrivate;

typedef struct {
	BonoboObject               base;

	AppletObjectPrivate       *priv;
} AppletObject;

typedef struct {
	BonoboObjectClass          parent_class;

	POA_GNOME_Applet2__epv     epv;
} AppletObjectClass;

typedef void (*AppletObjectCallbackFunc) (GtkWidget *widget, gpointer data);

GType applet_object_get_type               (void) G_GNUC_CONST;

AppletObject *applet_object_new             (GtkWidget    *widget,
					    const gchar  *iid,
					    guint32      *winid);

void  applet_object_register               (AppletObject *applet);

void  applet_object_abort_load             (AppletObject *applet);
void  applet_object_remove                 (AppletObject *applet);

void  applet_object_sync_config            (AppletObject *applet);

void  applet_object_register_callback      (AppletObject *applet,
					    const gchar  *name,
					    const gchar  *stock_item,
					    const gchar  *menutext,
					    AppletObjectCallbackFunc func,
					    gpointer      data);
void  applet_object_unregister_callback    (AppletObject *applet,
					    const char   *name);
void  applet_object_callback_set_sensitive (AppletObject *applet,
					    const gchar  *name,
					    gboolean      sensitive);

void  applet_object_set_tooltip            (AppletObject *applet,
					    const char   *text);
gint  applet_object_get_free_space         (AppletObject *applet);
void  applet_object_send_position          (AppletObject *applet,
					    gboolean      enable);
void  applet_object_send_draw              (AppletObject *applet,
					    gboolean      enable);
GNOME_Panel_RgbImage *
      applet_object_get_rgb_background     (AppletObject  *applet);

G_END_DECLS

#endif /* __APPLET_OBJECT_H__ */
