/* Gnome panel: basep widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Jacob Berkman
 */
#ifndef BASEP_WIDGET_H
#define BASEP_WIDGET_H

#include "panel-widget.h"

G_BEGIN_DECLS

#define BASEP_TYPE_WIDGET        	(basep_widget_get_type ())
#define BASEP_WIDGET(object)          	(G_TYPE_CHECK_INSTANCE_CAST ((object), BASEP_TYPE_WIDGET, BasePWidget))
#define BASEP_WIDGET_CLASS(klass)    	(G_TYPE_CHECK_CLASS_CAST ((klass), BASEP_TYPE_WIDGET, BasePWidgetClass))
#define BASEP_IS_WIDGET(object)       	(G_TYPE_CHECK_INSTANCE_TYPE ((object), BASEP_TYPE_WIDGET))
#define BASEP_IS_WIDGET_CLASS(klass) 	(G_TYPE_CHECK_CLASS_TYPE ((klass), BASEP_TYPE_WIDGET))
#define BASEP_WIDGET_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS ((object), BASEP_TYPE_WIDGET, BasePWidgetClass))

#define BASEP_TYPE_POS		        (basep_pos_get_type ())
#define BASEP_POS(object)      		(G_TYPE_CHECK_INSTANCE_CAST((object), BASEP_TYPE_POS, BasePPos))
#define BASEP_POS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), BASEP_TYPE_POS, BasePPosClass))
#define BASEP_IS_POS(object)            (G_TYPE_CHECK_INSTANCE_TYPE ((object), BASEP_TYPE_POS))
#define BASEP_IS_POS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), BASEP_TYPE_POS))
#define BASEP_POS_GET_CLASS(k)		(BASEP_POS_CLASS(G_OBJECT_GET_CLASS(BASEP_WIDGET(k)->pos)))


typedef struct _BasePWidget		BasePWidget;
typedef struct _BasePWidgetClass	BasePWidgetClass;

typedef struct _BasePPos                BasePPos;
typedef struct _BasePPosClass           BasePPosClass;

typedef enum {
	BASEP_EXPLICIT_HIDE,
	BASEP_AUTO_HIDE
} BasePMode;

typedef enum {
        BASEP_SHOWN,
	BASEP_AUTO_HIDDEN,
	BASEP_HIDDEN_LEFT,
	BASEP_HIDDEN_RIGHT
} BasePState;

struct _BasePWidget
{
	GtkWindow		window;
	GtkAllocation		shown_alloc;

	int			screen;
	int                     monitor;

	GtkWidget		*ebox;
	
	GtkWidget		*panel;
	
	GtkWidget		*table;
	GtkWidget		*hidebutton_n;
	GtkWidget		*hidebutton_e;
	GtkWidget		*hidebutton_w;
	GtkWidget		*hidebutton_s;
	
	GtkWidget		*frame;
	GtkWidget		*innerebox;

	BasePPos                *pos;

	BasePMode               mode;
	BasePState              state;
	gboolean		moving;

	gboolean		hidebuttons_enabled;
	gboolean		hidebutton_pixmaps_enabled;

	int                     enter_notify_timer_tag;
	int                     leave_notify_timer_tag;
	gboolean                autohide_inhibit;
	int                     drawers_open;

	gboolean                request_cube;
	gboolean                keep_in_screen;

	gboolean		compliant_wm;

	/* drag offsets, where the panel was clicked when started
	 * to be moved */
	int			offset_x;
	int			offset_y;

	/* wm strut */
	int			strut_left;
	int			strut_right;
	int			strut_top;
	int			strut_bottom;
};

struct _BasePWidgetClass
{
	GtkWindowClass parent_class;

	/* signals */
	/*FIXME: perhaps the type_change signal should be implemented
	  so that we don't have to handle the update_config_type in menu.c
	  but in panel.c along with the other update stuff */
	/*void (*type_change)  (BasePWidget *basep,
			      PanelType type);*/

	void (* mode_change)  (BasePWidget *basep,
			       BasePMode old_mode);

	void (* state_change) (BasePWidget *basep,
			       BasePState old_state);

	void (* screen_change) (BasePWidget *basep,
				int          old_screen,
				int          old_monitor);

	gboolean (* popup_panel_menu) (BasePWidget *basep);
};

/* we mostly want the class for this */
struct _BasePPos {
	GObject object;
	
	BasePWidget *basep;
};

struct _BasePPosClass {
	GObjectClass parent_class;

	/* virtual functions */
	void (*set_hidebuttons) (BasePWidget *basep);

	void (*update_winhints) (BasePWidget *basep);

	PanelOrient
	(*get_applet_orient) (BasePWidget *basep);

	void (*set_pos)                   (BasePWidget *basep,
					   int x, int y,
					   int w, int h,
					   gboolean force);

	void (*get_pos)                   (BasePWidget *basep,
					   int *x, int *y,
					   int w, int h);

	void (*get_size)                  (BasePWidget *basep,
					   int *w, int *h);

	PanelOrient (*get_hide_orient) (BasePWidget *basep);

	void (*get_hide_size)              (BasePWidget *basep,
					    PanelOrient hide_orient,
					    int *w, int *h);

	void (*get_hide_pos)               (BasePWidget *basep,
					    PanelOrient hide_orient,
					    int *x, int *y,
					    int w, int h);

	void (*realize)                   (GtkWidget *widget);

	void (*north_clicked)             (BasePWidget *basep);
	void (*south_clicked)             (BasePWidget *basep);
	void (*east_clicked)              (BasePWidget *basep);
	void (*west_clicked)              (BasePWidget *basep);

	void (*pre_convert_hook)          (BasePWidget *basep);
};

GType           basep_pos_get_type              (void) G_GNUC_CONST;
GType		basep_widget_get_type		(void) G_GNUC_CONST;

GtkWidget*	basep_widget_construct		(const char *panel_id,
						 BasePWidget *basep,
						 gboolean packed,
						 gboolean reverse_arrows,
						 int screen,
						 int monitor,
						 GtkOrientation orient,
						 int sz,
						 BasePMode mode,
						 BasePState state,
						 gboolean hidebuttons_enabled,
						 gboolean hidebutton_pixmaps_enabled,
						 PanelBackgroundType back_type,
						 const char *back_pixmap,
						 gboolean fit_pixmap_bg,
						 gboolean stretch_pixmap_bg,
						 gboolean rotate_pixmap_bg,
						 PanelColor *back_color);

/* changing parameters */
void		basep_widget_change_params	(BasePWidget *basep,
						 int screen,
						 int monitor,
						 GtkOrientation orient,
						 int sz,
						 BasePMode mode,
						 BasePState state,
						 gboolean hidebuttons_enabled,
						 gboolean hidebutton_pixmaps_enabled,
						 PanelBackgroundType back_type,
						 const char *pixmap_name,
						 gboolean fit_pixmap_bg,
						 gboolean stretch_pixmap_bg,
						 gboolean rotate_pixmap_bg,
						 PanelColor *back_color);

void            basep_widget_screen_size_changed (BasePWidget *basep,
						  GdkScreen   *screen);

/*gboolean       basep_widget_convert_to         (BasePWidget *basep,
						 PanelType type);*/

void            basep_widget_enable_buttons     (BasePWidget *basep,
						 gboolean     enabled);

void		basep_widget_set_hidebuttons	(BasePWidget *basep);
void            basep_widget_update_winhints    (BasePWidget *basep);

/*autohide*/
gboolean        basep_widget_autoshow           (gpointer data);
gboolean        basep_widget_autohide           (gpointer data);

/*queue an autohide*/
void            basep_widget_queue_autoshow     (BasePWidget *basep);
void            basep_widget_queue_autohide     (BasePWidget *basep);

/*explicit hiding*/
void            basep_widget_explicit_hide      (BasePWidget *basep,
						 BasePState hidestate);
void            basep_widget_explicit_show      (BasePWidget *basep);

void		basep_widget_do_hiding		(BasePWidget *basep,
						 PanelOrient hide_orient,
						 int leftover,
						 PanelSpeed animation_step);

void		basep_widget_do_showing		(BasePWidget *basep,
						 PanelOrient hide_orient,
						 int leftover,
						 PanelSpeed animation_step);

void		basep_widget_get_position	(BasePWidget *basep,
						 PanelOrient hide_orient,
						 int *x, int *y,
						 int w, int h);

PanelOrient basep_widget_get_applet_orient (BasePWidget *basep);

/* initialize drag offsets according to cursor */
void            basep_widget_init_offsets      (BasePWidget *basep);

void            basep_widget_set_pos           (BasePWidget *basep,
						int x, int y);

void            basep_widget_get_pos           (BasePWidget *basep,
						int *x, int *y);

void            basep_widget_get_size          (BasePWidget *basep,
						int *w, int *h);

void            basep_widget_pre_convert_hook (BasePWidget *basep);

void		basep_widget_screen_change	(BasePWidget *basep,
						 int          screen,
						 int          monitor);

void            basep_update_frame             (BasePWidget *basep);

/* redo the widget->window based on new compliancy setting */
void		basep_widget_redo_window	(BasePWidget *basep);

/* -1 means don't set, caller will not get queue resized as optimization */

void		basep_border_recalc		(int          screen,
						 int          monitor);
void		basep_border_queue_recalc	(int          screen,
						 int          monitor);
void		basep_border_get		(BasePWidget *basep, 
						 BorderEdge   edge,
						 int         *left,
						 int         *center,
						 int         *right);

G_END_DECLS

#endif /* __BASEP_WIDGET_H__ */
