/* Gnome panel: panel widget
 * (C) 1997-1998 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
/* This widget, although slightly written as a general purpose widget, it
   has MANY interdependencies, which makes it almost impossible to use in
   anything else but the panel, what it would need is some serious
   cleaning up*/
#ifndef PANEL_WIDGET_H
#define PANEL_WIDGET_H


#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gnome.h>
#include "button-widget.h"
#include "panel-types.h"

BEGIN_GNOME_DECLS

#define TYPE_PANEL_WIDGET          (panel_widget_get_type ())
#define PANEL_WIDGET(obj)          GTK_CHECK_CAST (obj, panel_widget_get_type (), PanelWidget)
#define PANEL_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, panel_widget_get_type (), PanelWidgetClass)
#define IS_PANEL_WIDGET(obj)       GTK_CHECK_TYPE (obj, panel_widget_get_type ())

#define PANEL_MINIMUM_WIDTH 12

#define PANEL_APPLET_ASSOC_PANEL_KEY "panel_applet_assoc_panel_key"
#define PANEL_APPLET_FORBIDDEN_PANELS "panel_applet_forbidden_panels"
#define PANEL_APPLET_DATA "panel_applet_data"

typedef struct _PanelWidget		PanelWidget;
typedef struct _PanelWidgetClass	PanelWidgetClass;

typedef struct _AppletRecord		AppletRecord;
typedef struct _AppletData		AppletData;
typedef struct _DNDRecord		DNDRecord;
typedef enum {
	PANEL_HORIZONTAL=0,
	PANEL_VERTICAL
} PanelOrientation;
typedef enum {
	PANEL_SWITCH_MOVE = 0,
	PANEL_FREE_MOVE,
	PANEL_PUSH_MOVE
} PanelMovementType;

struct _AppletData
{
	GtkWidget *	applet;
	int		pos;
	int		cells;
	gboolean	dirty;

	int		drag_off; /* offset on the applet where drag
				     was started */

	int		no_die; /* if >0 never send the about to die
				   signal, an int and not a bool for
				   nesting reasons */
};

struct _PanelWidget
{
	GtkFixed		fixed;

	guint32			unique_id;
	
	GList			*applet_list;
	GList			*no_window_applet_list;

	int			size;
	PanelOrientation	orient;
	int			sz;
	gboolean		fit_pixmap_bg;  /* fit pixmap while keeping
						   ratio*/
	gboolean		strech_pixmap_bg; /*strech pixmap to the size
						    of the panel */
	gboolean		rotate_pixmap_bg; /*rotate pixmap on vertical
						    panels */

	gboolean		packed;

	AppletData		*currently_dragged_applet;

	int			thick;

	PanelBackType		back_type;
	char                    *back_pixmap;
	GdkColor		back_color;
	gboolean		inhibit_draw;

	gboolean		no_padding_on_ends;
	
	GtkWidget		*master_widget;
	
	GtkWidget		*drop_widget; /*this is the widget that the
						panel checks for the cursor
						on drops usually the panel
					        widget itself*/
	
	GtkWidget		*panel_parent;
	
	GdkPixbuf		*backpix;	/* background pixmap unscaled */
	int			scale_w, scale_h;
	
	GdkPixmap		*backpixmap;	/* if a background pixmap
						   was set, this is used
						   for tiling onto the
						   background */
	int			x;
	int			y;
	int                     width;
	int                     height;
};

struct _PanelWidgetClass
{
	GtkFixedClass parent_class;

	void (* orient_change) (PanelWidget *panel,
				PanelOrientation orient);
	void (* size_change) (PanelWidget *panel,
			      int sz);
	void (* applet_move) (PanelWidget *panel,
			      GtkWidget *applet);
	void (* applet_added) (PanelWidget *panel,
			       GtkWidget *applet);
	void (* applet_removed) (PanelWidget *panel,
				 GtkWidget *applet);
	void (* back_change) (PanelWidget *panel,
			      PanelBackType type,
			      char *pixmap,
			      GdkColor *color);
	void (* applet_draw) (PanelWidget *panel,
			      GtkWidget *applet);
	void (* applet_about_to_die) (PanelWidget *panel,
				      GtkWidget *applet);
};

guint		panel_widget_get_type		(void) G_GNUC_CONST;
GtkWidget *	panel_widget_new		(gboolean packed,
						 PanelOrientation orient,
						 int sz,
						 PanelBackType back_type,
						 char *back_pixmap,
						 gboolean fit_pixmap_bg,
						 gboolean strech_pixmap_bg,
						 gboolean rotate_pixmap_bg,
						 gboolean no_padding_on_ends,
						 GdkColor *back_color);
/*add an applet to the panel, preferably at position pos, if insert_at_pos
  is on, we REALLY want to insert at the pos given by pos*/
int		panel_widget_add_full		(PanelWidget *panel,
						 GtkWidget *applet,
						 int pos,
						 gboolean bind_lower_events,
						 gboolean insert_at_pos);
#define panel_widget_add(panel,applet,pos) (panel_widget_add_full(panel,applet,pos,TRUE,FALSE))

PanelWidget *	panel_widget_get_by_id		(gint32 id);
void		panel_widget_set_id		(PanelWidget *panel,
						 gint32 id);

/*needs to be called for drawers after add*/
void		panel_widget_add_forbidden	(PanelWidget *panel);

/*move applet to newpos*/
int		panel_widget_move		(PanelWidget *panel,
						 GtkWidget *applet,
						 int pos);

/*move applet to a different panel*/
int		panel_widget_reparent		(PanelWidget *old_panel,
						 PanelWidget *new_panel,
						 GtkWidget *applet,
						 int pos);
/*return position of an applet*/
int		panel_widget_get_pos		(PanelWidget *panel,
						 GtkWidget *applet);

/*get amount of free space around the applet (including the applet size),
  or return 0 on error or if the panel is packed*/
int		panel_widget_get_free_space	(PanelWidget *panel,
						 GtkWidget *applet);

/* use these for drag_off for special cases */
#define PW_DRAG_OFF_CURSOR -1
#define PW_DRAG_OFF_CENTER -2

/*drag*/
void		panel_widget_applet_drag_start	(PanelWidget *panel,
						 GtkWidget *applet,
						 int drag_off);
void		panel_widget_applet_drag_end	(PanelWidget *panel);

/* needed for corba */
void		panel_widget_applet_drag_start_no_grab(PanelWidget *panel,
						       GtkWidget *applet,
						       int drag_off);
void		panel_widget_applet_drag_end_no_grab(PanelWidget *panel);

/* changing parameters */
void		panel_widget_change_params	(PanelWidget *panel,
						 PanelOrientation orient,
						 int sz,
						 PanelBackType back_type,
						 char *pixmap_name,
						 gboolean fit_pixmap_bg,
						 gboolean strech_pixmap_bg,
						 gboolean rotate_pixmap_bg,
						 gboolean no_padding_on_ends,
						 GdkColor *back_color);

/* changing parameters (orient only) */
void		panel_widget_change_orient	(PanelWidget *panel,
						 PanelOrientation orient);

/*change global params*/
void		panel_widget_change_global	(int explicit_step,
						 int auto_step,
						 int drawer_step,
						 int minimized_size,
						 int minimize_delay,
						 int maximize_delay,
						 PanelMovementType move_type,
						 gboolean disable_animations,
						 int applet_padding,
						 int applet_border_padding);

void		panel_widget_set_back_pixmap	(PanelWidget *panel,
						 char *file);
void		panel_widget_set_back_color	(PanelWidget *panel,
						 GdkColor *color);

/*draw EVERYTHING (meaning icons)*/
void		panel_widget_draw_all		(PanelWidget *panel,
						 GdkRectangle *area);
/*draw just one icon (applet has to be an icon of course)*/
void		panel_widget_draw_icon		(PanelWidget *panel,
						 ButtonWidget *applet);


/*get the number of applets*/
int		panel_widget_get_applet_count	(PanelWidget *panel);

/*tells us if an applet is "stuck" on the right side*/
int		panel_widget_is_applet_stuck	(PanelWidget *panel,
						 GtkWidget *applet);
/*get pos of the cursor location*/
int		panel_widget_get_cursorloc	(PanelWidget *panel);

/*needed for other panel types*/
gboolean	panel_widget_is_cursor		(PanelWidget *panel,
						 int overlap);

/* when we get color_only, we also optionally set r, g, b to the
   color and w, and h to the area if the background is one color
   only, otherwise normally return an rgb and set r, g, b to -1 */
void panel_widget_get_applet_rgb_bg(PanelWidget *panel,
				    GtkWidget *applet,
				    guchar **rgb,
				    int *w, int *h,
				    int *rowstride,
				    gboolean color_only,
				    int *r, int *g, int *b);

extern gboolean panel_applet_in_drag;

void 		panel_widget_force_repaint 	(PanelWidget *panel);
void		panel_widget_setup_translucent_background (PanelWidget	*panel);
END_GNOME_DECLS

#endif /* PANEL_WIDGET_H */
