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

/*this is not actually used in this code, but is a constant one should
  use when calculating how many cells a panel should have*/
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
/*add an applet to the panel, preferably at position pos*/
gint		panel_widget_add		(PanelWidget *panel,
						 GtkWidget *applet,
						 gint pos);
/*remove an applet from the panel*/
gint		panel_widget_remove		(PanelWidget *panel,
						 GtkWidget *applet);
/*return position of an applet*/
gint		panel_widget_get_pos		(PanelWidget *panel,
						 GtkWidget *applet);
/*return a list of all applets*/
GList*		panel_widget_get_applets	(PanelWidget *panel);
/*run func for each applet*/
void		panel_widget_foreach		(PanelWidget *panel,
						 GFunc func,
						 gpointer user_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __PANEL_WIDGET_H__ */
