/* Gnome panel: basep widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
#ifndef __BASEP_WIDGET_H__
#define __BASEP_WIDGET_H__

#include <gtk/gtk.h>
#include "basep-widget.h"

BEGIN_GNOME_DECLS

#define BASEP_WIDGET(obj)          GTK_CHECK_CAST (obj, basep_widget_get_type (), BasePWidget)
#define BASEP_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, basep_widget_get_type (), BasePWidgetClass)
#define IS_BASEP_WIDGET(obj)       GTK_CHECK_TYPE (obj, basep_widget_get_type ())

typedef struct _BasePWidget		BasePWidget;
typedef struct _BasePWidgetClass	BasePWidgetClass;

struct _BasePWidget
{
	GtkWindow		window;
	GtkWidget		*ebox;
	
	GtkWidget		*panel;
	
	GtkWidget		*table;
	GtkWidget		*hidebutton_n;
	GtkWidget		*hidebutton_e;
	GtkWidget		*hidebutton_w;
	GtkWidget		*hidebutton_s;
	
	GtkWidget		*frame;
	GtkWidget		*innerebox;

	GtkAllocation		shown_alloc;

	int			hidebuttons_enabled;
	int			hidebutton_pixmaps_enabled;
};

struct _BasePWidgetClass
{
	GtkWindowClass parent_class;

	/*virtual function, not a signal*/
	void (*set_hidebuttons) (BasePWidget *basep);
};

guint		basep_widget_get_type		(void);
GtkWidget*	basep_widget_construct		(BasePWidget *basep,
						 int packed,
						 int reverse_arrows,
						 PanelOrientation orient,
						 int hidebuttons_enabled,
						 int hidebutton_pixmaps_enabled,
						 PanelBackType back_type,
						 char *back_pixmap,
						 int fit_pixmap_bg,
						 GdkColor *back_color);

/* changing parameters */
void		basep_widget_change_params	(BasePWidget *basep,
						 PanelOrientation orient,
						 int hidebuttons_enabled,
						 int hidebutton_pixmaps_enabled,
						 PanelBackType back_type,
						 char *pixmap_name,
						 int fit_pixmap_bg,
						 GdkColor *back_color);

void		basep_widget_enable_buttons	(BasePWidget *basep);
void		basep_widget_disable_buttons	(BasePWidget *basep);

void		basep_widget_set_hidebuttons	(BasePWidget *basep);

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
						 gint16 *x,
						 gint16 *y,
						 gint16 w,
						 gint16 h);

#define GNOME_PANEL_HINTS (WIN_HINTS_SKIP_FOCUS|WIN_HINTS_SKIP_WINLIST|WIN_HINTS_SKIP_TASKBAR)

END_GNOME_DECLS

#endif /* __BASEP_WIDGET_H__ */
