#ifndef __APPLET_WIDGET_H__
#define __APPLET_WIDGET_H__


#include <gtk/gtk.h>

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
	GtkWindow		window;

	GtkWidget		*eb;
};

struct _AppletWidgetClass
{
	GtkWindowClass parent_class;
};

guint		applet_widget_get_type		(void);
GtkWidget*	applet_widget_new		(void);
/*add a widget to the applet*/
void		applet_widget_add		(AppletWidget *applet,
						 GtkWidget *widget);
/*remove a widget from the applet*/
void		applet_widget_remove		(AppletWidget *applet,
						 GtkWidget *widget);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __APPLET_WIDGET_H__ */
