/* Gnome panel: basep widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Jacob Berkman
 */
#ifndef __BASEP_WIDGET_H__
#define __BASEP_WIDGET_H__

#include "panel-widget.h"

BEGIN_GNOME_DECLS

#define BASEP_WIDGET_TYPE        (basep_widget_get_type ())
#define BASEP_WIDGET(o)          (GTK_CHECK_CAST((o), BASEP_WIDGET_TYPE, BasePWidget))
#define BASEP_WIDGET_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), BASEP_WIDGET_TYPE, BasePWidgetClass))
#define IS_BASEP_WIDGET(o)       (GTK_CHECK_TYPE((o), BASEP_WIDGET_TYPE))
#define IS_BASEP_WIDGET_CLASS(k) (GTK_CHECK_CLASS_TYPE((k), BASEP_WIDGET_TYPE))

#define BASEP_POS_TYPE        (basep_pos_get_type ())
#define BASEP_POS(o)          (GTK_CHECK_CAST((o), BASEP_POS_TYPE, BasePPos))
#define BASEP_POS_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), BASEP_POS_TYPE, BasePPosClass))
#define IS_BASEP_POS(o)       (GTK_CHECK_TYPE((o), BASEP_POS_TYPE))
#define IS_BASEP_POS_CLASS(k) (GTK_CHECK_CLASS_TYPE((k), BASEP_POS_TYPE))

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

struct _BasePWidget
{
	GtkWindow		window;
	GtkAllocation		shown_alloc;

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

	int			hidebuttons_enabled;
	int			hidebutton_pixmaps_enabled;

	int                     leave_notify_timer_tag;
	int                     autohide_inhibit;
	int                     drawers_open;

	gboolean                request_cube;
	gboolean                keep_in_screen;

	guint32                 autohide_complete;

	gboolean		compliant_wm;
};

struct _BasePWidgetClass
{
	GtkWindowClass parent_class;

	/* signals */
	void (*type_change)  (BasePWidget *basep,
			      PanelType type);

	void (*mode_change)  (BasePWidget *basep,
			      BasePMode mode);

	void (*state_change) (BasePWidget *basep,
			      BasePState state);

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
					   gint16 x, gint16 y,
					   guint16 w, guint16 h);

	void (*get_pos)                   (BasePWidget *basep,
					   gint16 *x, gint16 *y,
					   guint16 w, guint16 h);

	void (*get_size)                  (BasePWidget *basep,
					   guint16 *w, guint16 *h);

	PanelOrientType (*get_hide_orient) (BasePWidget *basep);

	void (*get_hide_size)              (BasePWidget *basep,
					    PanelOrientType hide_orient,
					    guint16 *w, guint16 *h);

	void (*get_hide_pos)               (BasePWidget *basep,
					    PanelOrientType hide_orient,
					    gint16 *x, gint16 *y,
					    guint16 w, guint16 h);

	void (*get_menu_pos)              (BasePWidget *basep,
					   GtkWidget *widget,
					   GtkRequisition *mreq,
					   gint *x, gint *y,
					   gint16 wx, gint16 wy,
					   guint16 ww, guint16 wh);

	void (*realize)                   (GtkWidget *widget);

	int (*north_clicked)              (BasePWidget *basep);
	int (*south_clicked)              (BasePWidget *basep);
	int (*east_clicked)               (BasePWidget *basep);
	int (*west_clicked)               (BasePWidget *basep);

	void (*pre_convert_hook)          (BasePWidget *basep);
};

GtkType         basep_pos_get_type              (void);
GtkType		basep_widget_get_type		(void);
GtkWidget*	basep_widget_construct		(BasePWidget *basep,
						 int packed,
						 int reverse_arrows,
						 PanelOrientation orient,
						 int sz,
						 BasePMode mode,
						 BasePState state,
						 int hidebuttons_enabled,
						 int hidebutton_pixmaps_enabled,
						 PanelBackType back_type,
						 char *back_pixmap,
						 gboolean fit_pixmap_bg,
						 gboolean strech_pixmap_bg,
						 gboolean rotate_pixmap_bg,
						 GdkColor *back_color);

/* changing parameters */
void		basep_widget_change_params	(BasePWidget *basep,
						 PanelOrientation orient,
						 int sz,
						 BasePMode mode,
						 BasePState state,
						 int hidebuttons_enabled,
						 int hidebutton_pixmaps_enabled,
						 PanelBackType back_type,
						 char *pixmap_name,
						 gboolean fit_pixmap_bg,
						 gboolean strech_pixmap_bg,
						 gboolean rotate_pixmap_bg,
						 GdkColor *back_color);

/*gboolean       basep_widget_convert_to         (BasePWidget *basep,
						 PanelType type);*/

void            _basep_widget_enable_buttons (BasePWidget *basep,
					      gboolean enabled);

#define         basep_widget_enable_buttons(basep) \
                (_basep_widget_enable_buttons ((basep),TRUE))

#define         basep_widget_disable_buttons(basep) \
                (_basep_widget_enable_buttons ((basep),FALSE))

void		basep_widget_set_hidebuttons	(BasePWidget *basep);
void            basep_widget_update_winhints    (BasePWidget *basep);

/*autohide*/
void            basep_widget_autoshow           (BasePWidget *basep);
int             basep_widget_autohide           (gpointer data);

/*queue an autohide*/
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
						 gint16 *x, gint16 *y,
						 guint16 w, guint16 h);

void            basep_widget_get_menu_pos  (BasePWidget *basep,
					    GtkWidget *menu,
					    gint *x, gint *y,
					    gint16 wx, gint16 wy,
					    guint16 ww, guint16 wh);

PanelOrientType basep_widget_get_applet_orient (BasePWidget *basep);

void            basep_widget_set_pos           (BasePWidget *basep,
						gint16 x, gint16 y);

void            basep_widget_get_pos           (BasePWidget *basep,
						gint16 *x, gint16 *y);

void            basep_widget_get_size          (BasePWidget *basep,
						guint16 *w, guint16 *h);

PanelOrientType basep_widget_get_hide_orient   (BasePWidget *basep);

void            basep_widget_get_hide_size      (BasePWidget *basep,
						 PanelOrientType hide_orient,
						 guint16 *w, guint16 *h);

void            basep_widget_get_hide_pos      (BasePWidget *basep,
						PanelOrientType hide_orient,
						guint16 *w, guint16 *h);

void            basep_widget_pre_convert_hook (BasePWidget *basep);

void            basep_widget_set_state         (BasePWidget *basep,
						BasePState state,
						gboolean emit);

void            basep_update_frame             (BasePWidget *basep);

/* redo the widget->window based on new compliancy setting */
void		basep_widget_redo_window	(BasePWidget *basep);

#define GNOME_PANEL_HINTS (WIN_HINTS_SKIP_FOCUS|WIN_HINTS_SKIP_WINLIST|WIN_HINTS_SKIP_TASKBAR)

END_GNOME_DECLS

#endif /* __BASEP_WIDGET_H__ */
