#ifndef __PANEL_WIDGET_H__
#define __PANEL_WIDGET_H__


#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define PANEL_WIDGET(obj)          GTK_CHECK_CAST (obj, panel_widget_get_type (), PanelWidget)
#define PANEL_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, panel_widget_get_type (), PanelWidgetClass)
#define IS_PANEL_WIDGET(obj)       GTK_CHECK_TYPE (obj, panel_widget_get_type ())

#define PANEL_CELL_SIZE 10
#define PANEL_MINIMUM_WIDTH 48

#define PANEL_MAX 1000 /*10,000 long screen, FIXME! make dynamic*/

#define PANEL_DRAWER_DROP_TARGET_SIZE 5

#define PANEL_APPLET_PARENT_KEY "panel_applet_parent_key"
#define PANEL_APPLET_ASSOC_PANEL_KEY "panel_applet_assoc_panel_key"

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
	PANEL_DRAWER,
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
	PANEL_HIDDEN,
	PANEL_HIDDEN_RIGHT,
	PANEL_HIDDEN_LEFT
} PanelState;
typedef enum {
	DROP_ZONE_LEFT,
	DROP_ZONE_RIGHT
} DrawerDropZonePos;

struct _AppletRecord
{
	GtkWidget		*applet;
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

	gint			leave_notify_timer_tag;

	GtkWidget		*currently_dragged_applet;
	gint			currently_dragged_applet_pos;

	GtkWidget		*drawer_drop_zone;
	DrawerDropZonePos	drawer_drop_zone_pos;

	gint			thick;
};

struct _PanelWidgetClass
{
	GtkWindowClass parent_class;

	void (* orient_change) (PanelWidget *panel,
				PanelOrientation orient,
				PanelSnapped snapped);
	void (* state_change) (PanelWidget *panel,
			       PanelState state);
	void (* applet_move) (PanelWidget *panel,
			      GtkWidget *applet);
	void (* applet_added) (PanelWidget *panel,
			       GtkWidget *applet);
	void (* applet_removed) (PanelWidget *panel);
};

guint		panel_widget_get_type		(void);
GtkWidget*	panel_widget_new		(gint size,
						 PanelOrientation orient,
						 PanelSnapped snapped,
						 PanelMode mode,
						 PanelState state,
						 gint pos_x,
						 gint pos_y,
						 DrawerDropZonePos
						 	drop_zone_pos);
/*add an applet to the panel, preferably at position pos*/
gint		panel_widget_add		(PanelWidget *panel,
						 GtkWidget *applet,
						 gint pos);
/*move oldpos to newpos*/
gint		panel_widget_move		(PanelWidget *panel,
						 gint oldpos,
						 gint pos);

/*remove an applet from the panel*/
gint		panel_widget_remove		(PanelWidget *panel,
						 GtkWidget *applet);

/*move applet to a different panel*/
gint		panel_widget_reparent		(PanelWidget *old_panel,
						 PanelWidget *new_panel,
						 GtkWidget *applet,
						 gint pos);
/*return position of an applet*/
gint		panel_widget_get_pos		(PanelWidget *panel,
						 GtkWidget *applet);
/*return a list of all applets*/
GList*		panel_widget_get_applets	(PanelWidget *panel);
/*run func for each applet*/
void		panel_widget_foreach		(PanelWidget *panel,
						 GFunc func,
						 gpointer user_data);

/*restore the current state, used after it was gtk_widget_hiden
  emits "restore_state" signal so you can propagate the restore
  state to drawers and such*/
void		panel_widget_restore_state	(PanelWidget *panel);

/*initiate drag*/
void		panel_widget_applet_drag_start	(PanelWidget *panel,
						 GtkWidget *applet);

/*needed for corba*/
void		panel_widget_applet_drag_start_no_grab(PanelWidget *panel,
						       GtkWidget *applet);
void		panel_widget_applet_drag_end_no_grab(PanelWidget *panel);
gint		panel_widget_applet_move_to_cursor(PanelWidget *panel);
void		panel_widget_applet_move_use_idle(PanelWidget *panel);

/*changing parameters*/
void		panel_widget_change_params	(PanelWidget *panel,
						 PanelOrientation orient,
						 PanelSnapped snapped,
						 PanelMode mode,
						 PanelState state,
						 DrawerDropZonePos
						 	drop_zone_pos);

/*changing parameters (orient only)*/
void		panel_widget_change_orient	(PanelWidget *panel,
						 PanelOrientation orient);

/*change global params*/
void		panel_widget_change_global	(gint explicit_step,
						 gint auto_step,
						 gint minimized_size,
						 gint minimize_delay);

extern GList *panels;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __PANEL_WIDGET_H__ */
