/* applet-widget: the interface for the applets, these are the functions
 * that applets need
 * (C) 1998 the Free Software Foundation
 *
 * Author:  George Lebl
 */
#ifndef __APPLET_WIDGET_H__
#define __APPLET_WIDGET_H__

#include <gtk/gtk.h>

#include "applet-object.h"
#include "GNOME_Panel.h"

G_BEGIN_DECLS

typedef GNOME_Panel_OrientType PanelOrientType;
#define ORIENT_UP GNOME_Panel_ORIENT_UP
#define ORIENT_DOWN GNOME_Panel_ORIENT_DOWN
#define ORIENT_LEFT GNOME_Panel_ORIENT_LEFT
#define ORIENT_RIGHT GNOME_Panel_ORIENT_RIGHT

enum {
	PIXEL_SIZE_ULTRA_TINY = 12,
	PIXEL_SIZE_TINY       = 24,
	PIXEL_SIZE_SMALL      = 36,
	PIXEL_SIZE_STANDARD   = 48,
	PIXEL_SIZE_LARGE      = 64,
	PIXEL_SIZE_HUGE       = 80,
	PIXEL_SIZE_RIDICULOUS = 128
};

typedef GNOME_Panel_BackType PanelBackType;
#define PANEL_BACK_NONE GNOME_Panel_BACK_NONE
#define PANEL_BACK_COLOR GNOME_Panel_BACK_COLOR
#define PANEL_BACK_PIXMAP GNOME_Panel_BACK_PIXMAP

#define TYPE_APPLET_WIDGET          (applet_widget_get_type ())
#define APPLET_WIDGET(obj)          GTK_CHECK_CAST (obj, applet_widget_get_type (), AppletWidget)
#define APPLET_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, applet_widget_get_type (), AppletWidgetClass)
#define IS_APPLET_WIDGET(obj)       GTK_CHECK_TYPE (obj, applet_widget_get_type ())

typedef struct _AppletWidgetPrivate	AppletWidgetPrivate;
typedef struct _AppletWidget		AppletWidget;

struct _AppletWidget
{
	GtkPlug                  window;
	
	/*< public >*/
	AppletObject            *object;

	gchar                   *privcfgpath;
	gchar                   *globcfgpath;
	
	PanelOrientType          orient;			
	gint                     size;			
	
	/*< private >*/
	AppletWidgetPrivate     *priv;
};

typedef struct _AppletWidgetClass	AppletWidgetClass;
struct _AppletWidgetClass
{
	GtkPlugClass parent_class;

	/*
	 * when the orientation of the parent panel changes, you should 
	 * connect this signal before doing applet_widget_add so that
	 * you get an initial change_orient signal during the add, so
	 * that you can update your orientation properly
	 */
	void (*change_orient)     (AppletWidget    *applet,
				   PanelOrientType  orient);

	/*
	 * the panel background changes, the pixmap handeling is likely
	 * to change
	 */
	void (*back_change)       (AppletWidget  *applet,
				   PanelBackType  type,
				   char          *pixmap,
				   GdkColor      *color);

	/*
	 * will send the current state of the tooltips, if they are enabled
	 * or disabled, you should only need this if you are doing something
	 * weird
	 */
	void (*tooltip_state)     (AppletWidget *applet,
				   int           enabled);

	/*
	 * when the panel wants to save a session it will call this signal 
	 * if you trap it make sure you do gnome_config_sync() and
	 * gnome_config_drop_all() after your done otherwise the changes
	 * might not be written to file, also make sure you return
	 * FALSE from this signal or your position wil not get saved!
	 */
	gboolean (*save_session)  (AppletWidget *applet,
				   char         *config_path,
				   char         *global_config_path);

	/*
	 * when the position changes and we selected to get this signal,
	 * it is sent so that you can move some external window along with
	 * the applet, it is not normally sent, so you need to enable it
	 * with the applet_widget_send_position
	 */
	void (*change_position)   (AppletWidget *applet,
				   int           x,
				   int           y);

	/*
	 * when the panel size changes, semantics are the same as above
	 */
	void (*change_pixel_size) (AppletWidget *applet,
				   int           size);
	
	/*
	 * done when we are requesting draws, only useful if you want
	 * to get rgb data of the background to draw yourself on, this
	 * signal is called when that data would be different and you
	 * should reget it and redraw, you should use the
	 * applet_widget_get_rgb_bg function to get rgb background for
	 * you to render on, you need to use applet_widget_send_draw 
	 * to enable this signal
	 */
	void (*do_draw)           (AppletWidget *applet);
};

GType		applet_widget_get_type             (void) G_GNUC_CONST;


GtkWidget*	applet_widget_new                  (const char *iid);

void		applet_widget_construct            (AppletWidget *applet,
						    const char   *iid);

void		applet_widget_set_tooltip          (AppletWidget *applet,
						    GtkWidget    *widget,
						    const char   *text);

void		applet_widget_add                  (AppletWidget *applet,
						    GtkWidget    *widget);

void		applet_widget_add_full             (AppletWidget *applet,
						    GtkWidget    *widget,
						    gboolean      bind_events);

void		applet_widget_bind_events          (AppletWidget *applet,
						    GtkWidget    *widget);

int		applet_widget_get_applet_count     (void);

PanelOrientType	applet_widget_get_panel_orient     (AppletWidget *applet);

int		applet_widget_get_panel_pixel_size (AppletWidget *applet);

void		applet_widget_get_rgb_background   (AppletWidget  *applet,
						    guchar       **rgb,
						    int           *w,
						    int           *h,
						    int           *rowstride);

void		applet_widget_queue_resize         (AppletWidget *applet);

G_END_DECLS

#endif /* __APPLET_WIDGET_H__ */
