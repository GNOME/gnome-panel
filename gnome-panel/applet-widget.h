#ifndef __APPLET_WIDGET_H__
#define __APPLET_WIDGET_H__

#include <gtk/gtk.h>
#include <gnome.h>

#include <applet-lib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define APPLET_WIDGET(obj)          GTK_CHECK_CAST (obj, applet_widget_get_type (), AppletWidget)
#define APPLET_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, applet_widget_get_type (), AppletWidgetClass)
#define IS_APPLET_WIDGET(obj)       GTK_CHECK_TYPE (obj, applet_widget_get_type ())

typedef struct _AppletWidget		AppletWidget;
typedef struct _AppletWidgetClass	AppletWidgetClass;

struct _AppletWidget
{
	GtkPlug			window;

	gint			applet_id;

	gchar			*cfgpath;
	gchar			*globcfgpath;
};

struct _AppletWidgetClass
{
	GtkPlugClass parent_class;

	void (* change_orient) (AppletWidget *applet,
				PanelOrientType orient);
	void (* session_save) (AppletWidget *applet,
			       char *cfgpath,
			       char *globcfgpath);
};

guint		applet_widget_get_type		(void);
GtkWidget*	applet_widget_new		(gchar *argv0);
void		applet_widget_set_tooltip	(AppletWidget *applet,
						 gchar *text);

void		applet_widget_add		(AppletWidget *applet,
						 GtkWidget *widget);

void		applet_widget_gtk_main		(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __APPLET_WIDGET_H__ */
