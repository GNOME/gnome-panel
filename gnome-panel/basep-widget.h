/* Gnome panel: basep widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Jacob Berkman
 */
#ifndef BASEP_WIDGET_H
#define BASEP_WIDGET_H

#include "panel-widget.h"

BEGIN_GNOME_DECLS

#define TYPE_BASEP_WIDGET        (basep_widget_get_type ())
#define BASEP_WIDGET(o)          (GTK_CHECK_CAST((o), TYPE_BASEP_WIDGET, BasePWidget))
#define BASEP_WIDGET_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), TYPE_BASEP_WIDGET, BasePWidgetClass))
#define IS_BASEP_WIDGET(o)       (GTK_CHECK_TYPE((o), TYPE_BASEP_WIDGET))
#define IS_BASEP_WIDGET_CLASS(k) (GTK_CHECK_CLASS_TYPE((k), TYPE_BASEP_WIDGET))

#define TYPE_BASEP_POS        (basep_pos_get_type ())
#define BASEP_POS(o)          (GTK_CHECK_CAST((o), TYPE_BASEP_POS, BasePPos))
#define BASEP_POS_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), TYPE_BASEP_POS, BasePPosClass))
#define IS_BASEP_POS(o)       (GTK_CHECK_TYPE((o), TYPE_BASEP_POS))
#define IS_BASEP_POS_CLASS(k) (GTK_CHECK_CLASS_TYPE((k), TYPE_BASEP_POS))

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
	BASEP_MOVING,
	BASEP_AUTO_HIDDEN,
	BASEP_HIDDEN_LEFT,
	BASEP_HIDDEN_RIGHT
} BasePState;

typedef enum {
	BASEP_LEVEL_DEFAULT,
	BASEP_LEVEL_ABOVE,
	BASEP_LEVEL_NORMAL,
	BASEP_LEVEL_BELOW
} BasePLevel;

struct _BasePWidget
{
	GtkWindow		window;
	GtkAllocation		shown_alloc;

	int			screen;

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
	BasePLevel		level;
	gboolean		avoid_on_maximize;

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
			       BasePMode mode);

	void (* state_change) (BasePWidget *basep,
			       BasePState state);

	void (* screen_change) (BasePWidget *basep,
				int screen);

};

/* we mostly want the class for this */
struct _BasePPos {
	GtkObject object;
	
	BasePWidget *basep;
};

struct _BasePPosClass {
	GtkObjectClass parent_class;

	/* virtual functions */
	void (*set_hidebuttons) (BasePWidget *basep);

	void (*update_winhints) (BasePWidget *basep);

	PanelOrientType
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

	PanelOrientType (*get_hide_orient) (BasePWidget *basep);

	void (*get_hide_size)              (BasePWidget *basep,
					    PanelOrientType hide_orient,
					    int *w, int *h);

	void (*get_hide_pos)               (BasePWidget *basep,
					    PanelOrientType hide_orient,
					    int *x, int *y,
					    int w, int h);

	void (*get_menu_pos)              (BasePWidget *basep,
					   GtkWidget *widget,
					   GtkRequisition *mreq,
					   int *x, int *y,
					   int wx, int wy,
					   int ww, int wh);

	void (*realize)                   (GtkWidget *widget);

	void (*north_clicked)             (BasePWidget *basep);
	void (*south_clicked)             (BasePWidget *basep);
	void (*east_clicked)              (BasePWidget *basep);
	void (*west_clicked)              (BasePWidget *basep);

	void (*pre_convert_hook)          (BasePWidget *basep);
};

GtkType         basep_pos_get_type              (void) G_GNUC_CONST;
GtkType		basep_widget_get_type		(void) G_GNUC_CONST;
GtkWidget*	basep_widget_construct		(BasePWidget *basep,
						 gboolean packed,
						 gboolean reverse_arrows,
						 int screen,
						 PanelOrientation orient,
						 int sz,
						 BasePMode mode,
						 BasePState state,
						 BasePLevel level,
						 gboolean avoid_on_maximize,
						 gboolean hidebuttons_enabled,
						 gboolean hidebutton_pixmaps_enabled,
						 PanelBackType back_type,
						 char *back_pixmap,
						 gboolean fit_pixmap_bg,
						 gboolean strech_pixmap_bg,
						 gboolean rotate_pixmap_bg,
						 GdkColor *back_color);

/* changing parameters */
void		basep_widget_change_params	(BasePWidget *basep,
						 int screen,
						 PanelOrientation orient,
						 int sz,
						 BasePMode mode,
						 BasePState state,
						 BasePLevel level,
						 gboolean avoid_on_maximize,
						 gboolean hidebuttons_enabled,
						 gboolean hidebutton_pixmaps_enabled,
						 PanelBackType back_type,
						 char *pixmap_name,
						 gboolean fit_pixmap_bg,
						 gboolean strech_pixmap_bg,
						 gboolean rotate_pixmap_bg,
						 GdkColor *back_color);

/*gboolean       basep_widget_convert_to         (BasePWidget *basep,
						 PanelType type);*/

void            basep_widget_enable_buttons_ (BasePWidget *basep,
					      gboolean enabled);

#define         basep_widget_enable_buttons(basep) \
                (basep_widget_enable_buttons_ ((basep),TRUE))

#define         basep_widget_disable_buttons(basep) \
                (basep_widget_enable_buttons_ ((basep),FALSE))

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
						 PanelOrientType hide_orient,
						 int leftover,
						 int step);

void		basep_widget_do_showing		(BasePWidget *basep,
						 PanelOrientType hide_orient,
						 int leftover,
						 int step);

void		basep_widget_get_position	(BasePWidget *basep,
						 PanelOrientType hide_orient,
						 int *x, int *y,
						 int w, int h);

void            basep_widget_get_menu_pos  (BasePWidget *basep,
					    GtkWidget *menu,
					    int *x, int *y,
					    int wx, int wy,
					    int ww, int wh);

PanelOrientType basep_widget_get_applet_orient (BasePWidget *basep);

/* initialize drag offsets according to cursor */
void            basep_widget_init_offsets      (BasePWidget *basep);

void            basep_widget_set_pos           (BasePWidget *basep,
						int x, int y);

void            basep_widget_get_pos           (BasePWidget *basep,
						int *x, int *y);

void            basep_widget_get_size          (BasePWidget *basep,
						int *w, int *h);

PanelOrientType basep_widget_get_hide_orient   (BasePWidget *basep);

/*
void            basep_widget_get_hide_size      (BasePWidget *basep,
						 PanelOrientType hide_orient,
						 int *w, int *h);

void            basep_widget_get_hide_pos      (BasePWidget *basep,
						PanelOrientType hide_orient,
						int *w, int *h);
						*/

void            basep_widget_pre_convert_hook (BasePWidget *basep);

void            basep_widget_set_state         (BasePWidget *basep,
						BasePState state,
						gboolean emit);
void		basep_widget_screen_change	(BasePWidget *basep,
						 int screen);

void            basep_update_frame             (BasePWidget *basep);

/* redo the widget->window based on new compliancy setting */
void		basep_widget_redo_window	(BasePWidget *basep);

/* -1 means don't set, caller will not get queue resized as optimization */

void		basep_border_recalc		(int screen);
void		basep_border_queue_recalc	(int screen);
void		basep_border_get		(int screen,
						 BorderEdge edge,
						 int *left,
						 int *center,
						 int *right);

#define GNOME_PANEL_HINTS (WIN_HINTS_SKIP_FOCUS|WIN_HINTS_SKIP_WINLIST|WIN_HINTS_SKIP_TASKBAR)

END_GNOME_DECLS

#endif /* __BASEP_WIDGET_H__ */
