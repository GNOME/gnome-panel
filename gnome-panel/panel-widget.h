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

#define PANEL_APPLET_PARENT_KEY "panel_applet_parent_key"
#define PANEL_APPLET_ASSOC_PANEL_KEY "panel_applet_assoc_panel_key"
#define PANEL_APPLET_FORBIDDEN_PANELS "panel_applet_forbidden_panels"
#define PANEL_APPLET_DATA "panel_applet_data"

typedef struct _PanelWidget		PanelWidget;
typedef struct _PanelWidgetClass	PanelWidgetClass;

typedef struct _AppletRecord		AppletRecord;
typedef struct _AppletData		AppletData;
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
typedef enum {
	PANEL_SWITCH_MOVE,
	PANEL_FREE_MOVE
} PanelMovementType;

struct _AppletData
{
	GtkWidget *applet;
	gint pos;
	gint cells;

	gint prevx;
	gint prevy;
	gint prevwidth;
	gint prevheight;
};

struct _DNDRecord
{
	AppletData		*ad;
	GtkWidget		*parent;
};

struct _PanelWidget
{
	GtkWindow		window;

	GtkWidget		*frame;
	GtkWidget		*fixed;
	GtkWidget		*table;
	GtkWidget		*hidebutton_n;
	GtkWidget		*hidebutton_e;
	GtkWidget		*hidebutton_w;
	GtkWidget		*hidebutton_s;

	GList			*applet_list;

	gint			size;
	PanelOrientation	orient;
	PanelSnapped		snapped;
	PanelMode		mode;
	PanelState		state;

	gint			leave_notify_timer_tag;

	AppletData		*currently_dragged_applet;

	DrawerDropZonePos	drawer_drop_zone_pos;
	gint			x;
	gint			y;

	gint			thick;

	gint			drawers_open; /* a count which can be used
						 to block the autohide, until
						 it is 0 .. it's set by the
						 app not the widget*/
	GtkWidget		*master_widget; /*the drawer button that is
						  holding this panel*/

	char                    *back_pixmap;
	gint			pixmap_enabled;
	GdkColor		back_color;

	gint			autohide_inhibit;
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
						 	drop_zone_pos,
						 char *back_pixmap,
						 GdkColor *back_color);

/*add an applet to the panel, preferably at position pos*/
gint		panel_widget_add		(PanelWidget *panel,
						 GtkWidget *applet,
						 gint pos);
/*needs to be called for drawers after add*/
void		panel_widget_add_forbidden	(PanelWidget *panel);

/*move applet to newpos*/
gint		panel_widget_move		(PanelWidget *panel,
						 GtkWidget *applet,
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



/*open and close drawers*/
void		panel_widget_open_drawer	(PanelWidget *panel);
void		panel_widget_close_drawer	(PanelWidget *panel);

/*drag*/
void		panel_widget_applet_drag_start	(PanelWidget *panel,
						 GtkWidget *applet);
void		panel_widget_applet_drag_end	(PanelWidget *panel);

/* needed for corba */
void		panel_widget_applet_drag_start_no_grab(PanelWidget *panel,
						       GtkWidget *applet);
void		panel_widget_applet_drag_end_no_grab(PanelWidget *panel);
gint		panel_widget_applet_move_to_cursor(PanelWidget *panel);
void		panel_widget_applet_move_use_idle(PanelWidget *panel);

/* changing parameters */
void		panel_widget_change_params	(PanelWidget *panel,
						 PanelOrientation orient,
						 PanelSnapped snapped,
						 PanelMode mode,
						 PanelState state,
						 DrawerDropZonePos
						 	drop_zone_pos,
						 char *pixmap_name);

/* changing parameters (orient only) */
void		panel_widget_change_orient	(PanelWidget *panel,
						 PanelOrientation orient);
/*changing parameters (dropzonepos only)*/
void		panel_widget_change_drop_zone_pos(PanelWidget *panel,
						  DrawerDropZonePos
						 	drop_zone_pos);

/*change global params*/
void		panel_widget_change_global	(gint explicit_step,
						 gint auto_step,
						 gint drawer_step,
						 gint minimized_size,
						 gint minimize_delay,
						 PanelMovementType move_type,
						 gint disable_animations);

/*popup the widget if it's popped down (autohide)*/
void		panel_widget_pop_up		(PanelWidget *panel);

/*queue a pop_down in autohide mode*/
void		panel_widget_queue_pop_down	(PanelWidget *panel);

void		panel_widget_enable_buttons	(PanelWidget *panel);
void		panel_widget_disable_buttons	(PanelWidget *panel);

void		panel_widget_set_drawer_pos	(PanelWidget *panel,
						 gint x,
						 gint y);

/*get the number of applets*/
gint		panel_widget_get_applet_count	(PanelWidget *panel);

extern GList *panels;

extern gint panel_applet_in_drag;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __PANEL_WIDGET_H__ */
