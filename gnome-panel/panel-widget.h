#ifndef __PANEL_WIDGET_H__
#define __PANEL_WIDGET_H__


#include <gdk/gdk.h>
#include <gtk/gtkeventbox.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define PANEL_WIDGET(obj)          GTK_CHECK_CAST (obj, panel_widget_get_type (), PanelWidget)
#define PANEL_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, panel_widget_get_type (), PanelWidgetClass)
#define IS_PANEL_WIDGET(obj)       GTK_CHECK_TYPE (obj, panel_widget_get_type ())

#define PANEL_CELL_SIZE 48

typedef struct _PanelWidget		PanelWidget;
typedef struct _PanelWidgetClass	PanelWidgetClass;

typedef struct _AppletRecord		AppletRecord;
typedef enum {
	PANEL_HORIZONTAL,
	PANEL_VERTICAL
} PanelOrientation;

struct _AppletRecord
{
	GtkWidget		*widget;
	gboolean		is_applet;
};

struct _PanelWidget
{
	GtkEventBox		event_box;

	AppletRecord		**applets;
	gint	 		applet_count;
	GtkTable		*table;

	gint			size;
	PanelOrientation	orientation;
};

struct _PanelWidgetClass
{
	GtkEventBoxClass parent_class;
};

guint		panel_widget_get_type		(void);
GtkWidget*	panel_widget_new		(gint size,
						 PanelOrientation orient);
gint		panel_widget_add		(PanelWidget *panel,
						 GtkWidget *applet,
						 gint pos);
gint		panel_widget_remove		(PanelWidget *panel,
						 GtkWidget *applet);
gint		panel_widget_getpos		(PanelWidget *panel,
						 GtkWidget *applet);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __PANEL_WIDGET_H__ */
