/* applet-widget: the interface for the applets, these are the functions
 * that applets need
 * (C) 1998 the Free Software Foundation
 *
 * Author:  George Lebl
 */
#ifndef __APPLET_WIDGET_H__
#define __APPLET_WIDGET_H__

#include <gtk/gtk.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>

#include <gnome-panel.h>

#define HAVE_SAVE_SESSION_SIGNAL 1
#define HAVE_APPLET_BIND_EVENTS 1
#define HAVE_PANEL_PIXEL_SIZE 1
#define HAVE_PANEL_DRAW_SIGNAL 1
#define HAVE_APPLET_QUEUE_RESIZE 1
#define HAVE_APPLET_SIZE_ULTRA_TINY_AND_RIDICULOUS 1

BEGIN_GNOME_DECLS

typedef GNOME_Panel_OrientType PanelOrientType;
#define ORIENT_UP GNOME_Panel_ORIENT_UP
#define ORIENT_DOWN GNOME_Panel_ORIENT_DOWN
#define ORIENT_LEFT GNOME_Panel_ORIENT_LEFT
#define ORIENT_RIGHT GNOME_Panel_ORIENT_RIGHT

enum {
	PIXEL_SIZE_ULTRA_TINY = 12,
	PIXEL_SIZE_TINY = 24,
	PIXEL_SIZE_SMALL = 36,
	PIXEL_SIZE_STANDARD = 48,
	PIXEL_SIZE_LARGE = 64,
	PIXEL_SIZE_HUGE = 80,
	PIXEL_SIZE_RIDICULOUS = 128
};

typedef GNOME_Panel_BackType PanelBackType;
#define PANEL_BACK_NONE GNOME_Panel_BACK_NONE
#define PANEL_BACK_COLOR GNOME_Panel_BACK_COLOR
#define PANEL_BACK_PIXMAP GNOME_Panel_BACK_PIXMAP
#define PANEL_BACK_TRANSLUCENT GNOME_Panel_BACK_TRANSLUCENT


#define TYPE_APPLET_WIDGET          (applet_widget_get_type ())
#define APPLET_WIDGET(obj)          GTK_CHECK_CAST (obj, applet_widget_get_type (), AppletWidget)
#define APPLET_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, applet_widget_get_type (), AppletWidgetClass)
#define IS_APPLET_WIDGET(obj)       GTK_CHECK_TYPE (obj, applet_widget_get_type ())

typedef struct _AppletWidgetPrivate	AppletWidgetPrivate;

typedef struct _AppletWidget		AppletWidget;
typedef void (*AppletCallbackFunc)(AppletWidget *applet, gpointer data);

struct _AppletWidget
{
	GtkPlug			window;
	
	/*< public >*/
	char			*privcfgpath;
	char			*globcfgpath;
	
	/* you should really use the accessors for these anyway */
	PanelOrientType		orient;			
	int			size;			
	
	/*< private >*/
	AppletWidgetPrivate	*_priv;
};

typedef struct _AppletWidgetClass	AppletWidgetClass;
struct _AppletWidgetClass
{
	GtkPlugClass parent_class;

	/* when the orientation of the parent panel changes, you should 
	   connect this signal before doing applet_widget_add so that
	   you get an initial change_orient signal during the add, so
	   that you can update your orientation properly */
	void (* change_orient) (AppletWidget *applet,
				GNOME_Panel_OrientType orient);
	/* the panel background changes, the pixmap handeling is likely
	   to change */
	void (* back_change) (AppletWidget *applet,
			      GNOME_Panel_BackType type,
			      char *pixmap,
			      GdkColor *color);
	/*will send the current state of the tooltips, if they are enabled
	  or disabled, you should only need this if you are doing something
	  weird*/
	void (* tooltip_state) (AppletWidget *applet,
				int enabled);
	/*when the panel wants to save a session it will call this signal 
	  if you trap it make sure you do gnome_config_sync() and
	  gnome_config_drop_all() after your done otherwise the changes
	  might not be written to file, also make sure you return
	  FALSE from this signal or your position wil not get saved!*/
	gboolean (* save_session) (AppletWidget *applet,
				   char *cfgpath,
				   char *globcfgpath);
	/*when the position changes and we selected to get this signal,
	  it is sent so that you can move some external window along with
	  the applet, it is not normally sent, so you need to enable it
	  with the applet_widget_send_position*/
	void (* change_position) (AppletWidget *applet,
				  int x, int y);

	/* when the panel size changes, semantics are the same as above */
	void (* change_pixel_size) (AppletWidget *applet,
				    int size);
	
	/* done when we are requesting draws, only useful if you want
	   to get rgb data of the background to draw yourself on, this
	   signal is called when that data would be different and you
	   should reget it and redraw, you should use the
	   applet_widget_get_rgb_bg function to get rgb background for
	   you to render on, you need to use applet_widget_send_draw 
	   to enable this signal */
	void (* do_draw) (AppletWidget *applet);
};

typedef GtkWidget *(*AppletFactoryActivator)(const char *goad_id, const char **params, int nparams);
/* Returns TRUE if the factory can activate this applet */
typedef gboolean (*AppletFactoryQuerier)(const char *goad_id);

guint		applet_widget_get_type		(void) G_GNUC_CONST;

void            applet_factory_new(const char *goad_id,
				   AppletFactoryQuerier qfunc,
				   AppletFactoryActivator afunc);
GtkWidget*	applet_widget_new(const char *goad_id);

void		applet_widget_construct(AppletWidget* applet, const char *goad_id);

/*set tooltip over the applet, NULL to remove a tooltip*/
void		applet_widget_set_tooltip	(AppletWidget *applet,
						 const char *text);

/*set tooltip on a specific widget inside the applet*/
void		applet_widget_set_widget_tooltip(AppletWidget *applet,
						 GtkWidget *widget,
						 const char *text);

/* add a widget to the plug and register the applet, this has to
   be done after all the children had been added so that the applet-lib
   can bind the events over them so that peoplce can move them with
   the second button, get the menu, etc ...*/
void		applet_widget_add		(AppletWidget *applet,
						 GtkWidget *widget);
/* this function is the same as above, but you can select if the events
   are actually bound, most applet writers can use the above, this is
   just for very special cases*/
void		applet_widget_add_full		(AppletWidget *applet,
						 GtkWidget *widget,
						 gboolean bind_events);

/* bind the events for button2 and button3 on a widget, this is useful
   when you are added a new widget and want the right click menu and middle
   button move events to work on it*/
void		applet_widget_bind_events	(AppletWidget *applet,
						 GtkWidget *widget);

/* remove the plug from the panel, this will destroy the applet */
void		applet_widget_remove		(AppletWidget *applet);

/* The callback functions control the applet's right click menu, the name
   is just a string, which has to be unique and which controls the nesting,
   for example a name of "foo/bar" will add an item to the submenu
   identified by "/foo" (which you should have created before with
   register_callback_dir, use this for properies callback, help, about,
   etc... etc...
*/
/*add a callback onto the applet's right click menu*/
void		applet_widget_register_callback	(AppletWidget *applet,
						 const char *name,
						 const char *menutext,
						 AppletCallbackFunc func,
						 gpointer data);
void		applet_widget_register_stock_callback	(AppletWidget *applet,
							 const char *name,
							 const char *stock_type,
							 const char *menutext,
							 AppletCallbackFunc func,
							 gpointer data);

/*remove a menuitem*/
void		applet_widget_unregister_callback (AppletWidget *applet,
						   const char *name);

/*add a submenu*/
void		applet_widget_register_callback_dir (AppletWidget *applet,
						     const char *name,
						     const char *menutext);
void		applet_widget_register_stock_callback_dir (AppletWidget *applet,
							   const char *name,
							   const char *stock_type,
							   const char *menutext);
/*remove a submenu*/
void		applet_widget_unregister_callback_dir (AppletWidget *applet,
						       const char *name);

/*enable/disable a menu entry*/
void		applet_widget_callback_set_sensitive	(AppletWidget *applet,
							 const char *name,
							 gboolean sensitive);

/*get thenumber of applets*/
int		applet_widget_get_applet_count	(void);

/*tell the panel to save our session here (just saves no shutdown),
  this should be done when you change some of your config and want
  the panel to save it's config, you should NOT call this in the
  session_save handler as it will result in a locked panel*/
void		applet_widget_sync_config	(AppletWidget *applet);

/* Get the orientation the applet should use */
PanelOrientType	applet_widget_get_panel_orient	(AppletWidget *applet);

/* Get the pixel size the applet should use */
int		applet_widget_get_panel_pixel_size	(AppletWidget *applet);

/* Get the free space for the applet if it's on an edge panel or 0
   if on a packed panel or on error */
int		applet_widget_get_free_space	(AppletWidget *applet);

/* sets if the change_position signal is sent*/
void		applet_widget_send_position	(AppletWidget *applet,
						 gboolean enable);

/* sets if the do_draw signal is sent*/
void		applet_widget_send_draw		(AppletWidget *applet,
						 gboolean enable);

/* gets the rgb background, useful in conjunction with the do_draw signal */
void		applet_widget_get_rgb_bg	(AppletWidget *applet,
						 guchar **rgb,
						 int *w, int *h,
						 int *rowstride);

/* queue resize on the socket in the panel for shlib applets or
   just the applet for external applets */
void		applet_widget_queue_resize	(AppletWidget *applet);

/*use this instead of gnome init*/
gboolean	applet_widget_init		(const char *app_id,
						 const char *app_version,
						 int argc,
						 char **argv,
						 struct poptOption *options,
						 unsigned int flags,
						 poptContext *return_ctx);

/*abort the applet loading, once applet has been created, this is a way to
  tell the panel to forget about us if we decide we want to quit before
  we add the actual applet to the applet-widget*/
void		applet_widget_abort_load	(AppletWidget *applet);

/* use this as gtk_main in applets */
void		applet_widget_gtk_main		(void);

/*quit the applet*/
void		applet_widget_gtk_main_quit	(void);

/*quit the panel (this will log out the gnome session)*/
void		applet_widget_panel_quit	(void);

/* Used by shlib applets */
CORBA_Object applet_widget_corba_activate(GtkWidget *applet,
					  PortableServer_POA poa,
					  const char *goad_id,
					  const char **params,
					  gpointer *impl_ptr,
					  CORBA_Environment *ev);

void applet_widget_corba_deactivate(PortableServer_POA poa,
				    const char *goad_id,
				    gpointer impl_ptr,
				    CORBA_Environment *ev);


#define APPLET_ACTIVATE(func, goad_id, apldat) { CORBA_Environment ev; CORBA_exception_init(&ev); \
CORBA_Object_release(func(CORBA_ORB_resolve_initial_references(gnome_CORBA_ORB(), \
"RootPOA", &ev), goad_id, NULL, apldat, &ev), &ev); CORBA_exception_free(&ev); }

#define APPLET_DEACTIVATE(func, goad_id, apldat) { CORBA_Environment ev; CORBA_exception_init(&ev); \
func(CORBA_ORB_resolve_initial_references(gnome_CORBA_ORB(), "RootPOA", &ev), goad_id, apldat, &ev); CORBA_exception_free(&ev); }

END_GNOME_DECLS

#endif /* __APPLET_WIDGET_H__ */
