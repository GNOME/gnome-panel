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
#define HAVE_PANEL_SIZE 1

BEGIN_GNOME_DECLS

typedef GNOME_Panel_OrientType PanelOrientType;
#define ORIENT_UP GNOME_Panel_ORIENT_UP
#define ORIENT_DOWN GNOME_Panel_ORIENT_DOWN
#define ORIENT_LEFT GNOME_Panel_ORIENT_LEFT
#define ORIENT_RIGHT GNOME_Panel_ORIENT_RIGHT

typedef GNOME_Panel_BackType PanelBackType;
#define PANEL_BACK_NONE GNOME_Panel_BACK_NONE
#define PANEL_BACK_COLOR GNOME_Panel_BACK_COLOR
#define PANEL_BACK_PIXMAP GNOME_Panel_BACK_PIXMAP


#define APPLET_WIDGET(obj)          GTK_CHECK_CAST (obj, applet_widget_get_type (), AppletWidget)
#define APPLET_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, applet_widget_get_type (), AppletWidgetClass)
#define IS_APPLET_WIDGET(obj)       GTK_CHECK_TYPE (obj, applet_widget_get_type ())

typedef struct _AppletWidget		AppletWidget;
typedef struct _AppletWidgetClass	AppletWidgetClass;

typedef void (*AppletCallbackFunc)(AppletWidget *applet, gpointer data);

struct _AppletWidget
{
	GtkPlug			window;
	
	char			*privcfgpath;
	char			*globcfgpath;

        gpointer                corbadat; /* CORBA stuff */
};

struct _AppletWidgetClass
{
	GtkPlugClass parent_class;

	/* when the orientation of the parent panel changes, you should 
	   connect this signal before doing applet_widget_add so that
	   you get an initial change_orient signal during the add, so
	   that you can update your orientation properly */
	void (* change_orient) (AppletWidget *applet,
				GNOME_Panel_OrientType orient);
	/* when the panel size changes, semantics are the same as above */
	void (* change_size) (AppletWidget *applet,
			      GNOME_Panel_SizeType size);
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
	int (* save_session) (AppletWidget *applet,
			      char *cfgpath,
			      char *globcfgpath);
};

typedef GtkWidget *(*AppletFactoryActivator)(const char *goad_id, const char **params, int nparams);
/* Returns TRUE if the factory can activate this applet */
typedef gboolean (*AppletFactoryQuerier)(const char *goad_id);

guint		applet_widget_get_type		(void);

void            applet_factory_new(const char *goad_id,
				   AppletFactoryQuerier qfunc,
				   AppletFactoryActivator afunc);
GtkWidget*	applet_widget_new(const char *goad_id);

/*set tooltip over the applet, NULL to remove a tooltip*/
void		applet_widget_set_tooltip	(AppletWidget *applet,
						 char *text);

/*set tooltip on a specific widget inside the applet*/
void		applet_widget_set_widget_tooltip(AppletWidget *applet,
						 GtkWidget *widget,
						 char *text);

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
						 int bind_events);

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
						 char *name,
						 char *menutext,
						 AppletCallbackFunc func,
						 gpointer data);
void		applet_widget_register_stock_callback	(AppletWidget *applet,
							 char *name,
							 char *stock_type,
							 char *menutext,
							 AppletCallbackFunc func,
							 gpointer data);

/*remove a menuitem*/
void		applet_widget_unregister_callback (AppletWidget *applet,
						   char *name);

/*add a submenu*/
void		applet_widget_register_callback_dir (AppletWidget *applet,
						     char *name,
						     char *menutext);
void		applet_widget_register_stock_callback_dir (AppletWidget *applet,
							   char *name,
							   char *stock_type,
							   char *menutext);
/*remove a submenu*/
void		applet_widget_unregister_callback_dir (AppletWidget *applet,
						       char *name);

/*get thenumber of applets*/
int		applet_widget_get_applet_count	(void);

/*tell the panel to save our session here (just saves no shutdown),
  this should be done when you change some of your config and want
  the panel to save it's config, you should NOT call this in the
  session_save handler as it will result in a locked panel*/
void		applet_widget_sync_config	(AppletWidget *applet);

/* Get the oprientation the applet should use */
GNOME_Panel_OrientType	applet_widget_get_panel_orient	(AppletWidget *applet);

/*use this instead of gnome init, if you want multi applet, you also
  have to specify a "start new applet" function which will launch a new
  applet*/
int		applet_widget_init		(const char *app_id,
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


#define APPLET_ACTIVATE(func, goad_id, apldat) ({ CORBA_Environment ev; CORBA_exception_init(&ev); \
CORBA_Object_release(func(CORBA_ORB_resolve_initial_references(gnome_CORBA_ORB(), \
"RootPOA", &ev), goad_id, NULL, apldat, &ev), &ev); CORBA_exception_free(&ev); })

#define APPLET_DEACTIVATE(func, goad_id, apldat) ({ CORBA_Environment ev; CORBA_exception_init(&ev); \
func(CORBA_ORB_resolve_initial_references(gnome_CORBA_ORB(), "RootPOA", &ev), goad_id, apldat, &ev); CORBA_exception_free(&ev); })

END_GNOME_DECLS

#endif /* __APPLET_WIDGET_H__ */
