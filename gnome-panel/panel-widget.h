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
#include "button-widget.h"
#include "panel-types.h"

G_BEGIN_DECLS

#define PANEL_TYPE_WIDGET          	(panel_widget_get_type ())
#define PANEL_WIDGET(object)          	(G_TYPE_CHECK_INSTANCE_CAST ((object), PANEL_TYPE_WIDGET, PanelWidget))
#define PANEL_WIDGET_CLASS(klass)  	(G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_WIDGET, PanelWidgetClass))
#define PANEL_IS_WIDGET(object)       	(G_TYPE_CHECK_INSTANCE_TYPE ((object), PANEL_TYPE_WIDGET)) 

#define PANEL_MINIMUM_WIDTH 12

#define PANEL_APPLET_ASSOC_PANEL_KEY "panel_applet_assoc_panel_key"
#define PANEL_APPLET_FORBIDDEN_PANELS "panel_applet_forbidden_panels"
#define PANEL_APPLET_DATA "panel_applet_data"

typedef struct _PanelWidget		PanelWidget;
typedef struct _PanelWidgetClass	PanelWidgetClass;

typedef struct _AppletRecord		AppletRecord;
typedef struct _AppletData		AppletData;
typedef struct _DNDRecord		DNDRecord;

struct _AppletData
{
	GtkWidget *	applet;
	int		pos;
	int		cells;
	int             min_cells;

	gboolean        expand_major;
	gboolean        expand_minor;
	
	gboolean	dirty;

	int		drag_off; /* offset on the applet where drag
				     was started */

	int		no_die; /* if >0 never send the about to die
				   signal, an int and not a bool for
				   nesting reasons */

	/* Valid size ranges for expanded applets */
	int *           size_hints; 
	int             size_hints_len;
  
};

struct _PanelWidget
{
	GtkFixed		fixed;

	gchar 			*unique_id;
	
	GList			*applet_list;

	int			size;
	GtkOrientation		orient;
	int			sz;
	gboolean		fit_pixmap_bg;  /* fit pixmap while keeping
						   ratio*/
	gboolean		stretch_pixmap_bg; /*stretch pixmap to the size
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
};

struct _PanelWidgetClass
{
	GtkFixedClass parent_class;

	void (* orient_change) (PanelWidget *panel);
	void (* size_change) (PanelWidget *panel);
	void (* applet_move) (PanelWidget *panel,
			      GtkWidget *applet);
	void (* applet_added) (PanelWidget *panel,
			       GtkWidget *applet);
	void (* applet_removed) (PanelWidget *panel,
				 GtkWidget *applet);
	void (* back_change) (PanelWidget *panel);
	void (* push_move) (PanelWidget		*panel,
                            GtkDirectionType	 dir);
	void (* switch_move) (PanelWidget	*panel,
                              GtkDirectionType	 dir);
	void (* free_move) (PanelWidget		*panel,
                            GtkDirectionType	 dir);
	void (* tab_move) (PanelWidget	*panel,
                           gboolean	 next);
	void (* end_move) (PanelWidget	*panel);
};

GType		panel_widget_get_type		(void) G_GNUC_CONST;

GtkWidget *	panel_widget_new		(gchar *panel_id,
						 gboolean packed,
						 GtkOrientation orient,
						 int sz,
						 PanelBackType back_type,
						 char *back_pixmap,
						 gboolean fit_pixmap_bg,
						 gboolean stretch_pixmap_bg,
						 gboolean rotate_pixmap_bg,
						 GdkColor *back_color);
/*add an applet to the panel, preferably at position pos, if insert_at_pos
  is on, we REALLY want to insert at the pos given by pos*/
int		panel_widget_add		(PanelWidget *panel,
						 GtkWidget *applet,
						 int pos,
						 gboolean insert_at_pos,
						 gboolean expand_major,
						 gboolean expand_minor);

PanelWidget *	panel_widget_get_by_id		(gchar *id);
void		panel_widget_set_id		(PanelWidget *panel,
						 const char *id);
void		panel_widget_set_new_id		(PanelWidget *panel);

/*needs to be called for drawers after add*/
void		panel_widget_add_forbidden	(PanelWidget *panel);

/*move applet to a different panel*/
int		panel_widget_reparent		(PanelWidget *old_panel,
						 PanelWidget *new_panel,
						 GtkWidget *applet,
						 int pos);

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
						 GtkOrientation orient,
						 int sz,
						 PanelBackType back_type,
						 char *pixmap_name,
						 gboolean fit_pixmap_bg,
						 gboolean stretch_pixmap_bg,
						 gboolean rotate_pixmap_bg,
						 GdkColor *back_color);

void		panel_widget_set_back_pixmap	(PanelWidget *panel,
						 const char *file);
void		panel_widget_set_back_color	(PanelWidget *panel,
						 GdkColor *color);

/*draw EVERYTHING (meaning icons)*/
void		panel_widget_draw_all		(PanelWidget *panel,
						 GdkRectangle *area);
/*draw just one icon (applet has to be an icon of course)*/
void		panel_widget_draw_icon		(PanelWidget *panel,
						 ButtonWidget *applet);


/*tells us if an applet is "stuck" on the right side*/
int		panel_widget_is_applet_stuck	(PanelWidget *panel,
						 GtkWidget *applet);
/*get pos of the cursor location*/
int		panel_widget_get_cursorloc	(PanelWidget *panel);

/*needed for other panel types*/
gboolean	panel_widget_is_cursor		(PanelWidget *panel,
						 int overlap);
/* set the focus on the panel */
void            panel_widget_focus              (PanelWidget *panel);

PanelOrient     panel_widget_get_applet_orient  (PanelWidget *panel);

extern gboolean panel_applet_in_drag;

G_END_DECLS

#endif /* PANEL_WIDGET_H */
