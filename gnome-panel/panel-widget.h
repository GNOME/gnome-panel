#ifndef __PANEL_WIDGET_H__
#define __PANEL_WIDGET_H__


#include <gtk/gtk.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define PANEL_WIDGET(obj)          GTK_CHECK_CAST (obj, panel_widget_get_type (), PanelWidget)
#define PANEL_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, panel_widget_get_type (), PanelWidgetClass)
#define IS_PANEL_WIDGET(obj)       GTK_CHECK_TYPE (obj, panel_widget_get_type ())

/*this is not actually used in this code, but is a constant one should
  use when calculating how many cells a panel should have*/
#define PANEL_CELL_SIZE 48

#define PANEL_MAX 100

typedef struct _PanelWidget		PanelWidget;
typedef struct _PanelWidgetClass	PanelWidgetClass;

typedef struct _AppletRecord		AppletRecord;
typedef struct _DNDRecord		DNDRecord;
typedef enum {
	PANEL_HORIZONTAL,
	PANEL_VERTICAL
} PanelOrientation;
typedef enum {
	PANEL_FREE,
	PANEL_TOP,
	PANEL_BOTTOM,
	PANEL_LEFT,
	PANEL_RIGHT
} PanelSnapped;
typedef enum {
	PANEL_EXPLICIT_HIDE,
	PANEL_AUTO_HIDE
} PanelMode;
typedef enum {
	PANEL_SHOWN,
	PANEL_MOVING,
	PANEL_HIDDEN
} PanelState;

struct _AppletRecord
{
	GtkWidget		*applet;
	GtkWidget		*drawer;
	gint			cells;
};

struct _DNDRecord
{
	AppletRecord		*applet;
	GtkWidget		*parent;
};

struct _PanelWidget
{
	GtkWindow		window;

	GtkWidget		*fixed;
	GtkWidget		*table;
	GtkWidget		*hidebutton_n;
	GtkWidget		*hidebutton_e;
	GtkWidget		*hidebutton_w;
	GtkWidget		*hidebutton_s;

	AppletRecord		applets[PANEL_MAX];
	gint	 		applet_count;

	gint			size;
	PanelOrientation	orient;
	PanelSnapped		snapped;
	PanelMode		mode;
	PanelState		state;

	gint			step_size;
	gint			minimized_size;
	gint			minimize_delay;
	gint			tooltips_enabled;

	gint			leave_notify_timer_tag;

	GtkWidget		*currently_dragged_applet;
};

struct _PanelWidgetClass
{
	GtkWindowClass parent_class;
};

guint		panel_widget_get_type		(void);
GtkWidget*	panel_widget_new		(gint length,
						 PanelOrientation orient,
						 PanelSnapped snapped,
						 PanelMode mode,
						 PanelState state,
						 gint step_size,
						 gint minimized_size,
						 gint minimize_delay,
						 gint tooltips_enabled);
/*add an applet to the panel, preferably at position pos*/
gint		panel_widget_add		(PanelWidget *panel,
						 GtkWidget *applet,
						 gint pos);
/*add a drawer (you supply the button) to the panel, preferably at
  position pos*/
gint		panel_widget_add_with_drawer	(PanelWidget *panel,
						 GtkWidget *button,
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
