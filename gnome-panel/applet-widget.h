#ifndef __APPLET_WIDGET_H__
#define __APPLET_WIDGET_H__

#include <gtk/gtk.h>
#include <gnome.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef PANEL_H
/*from panel.h*/
typedef enum {
	ORIENT_UP,
	ORIENT_DOWN,
	ORIENT_LEFT,
	ORIENT_RIGHT
} PanelOrientType;
#endif

#ifndef __PANEL_WIDGET_H__
/*from panel-widget.h*/
typedef enum {
	PANEL_BACK_NONE,
	PANEL_BACK_COLOR,
	PANEL_BACK_PIXMAP
} PanelBackType;
#endif

#define APPLET_WIDGET(obj)          GTK_CHECK_CAST (obj, applet_widget_get_type (), AppletWidget)
#define APPLET_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, applet_widget_get_type (), AppletWidgetClass)
#define IS_APPLET_WIDGET(obj)       GTK_CHECK_TYPE (obj, applet_widget_get_type ())

typedef struct _AppletWidget		AppletWidget;
typedef struct _AppletWidgetClass	AppletWidgetClass;

typedef void (*AppletCallbackFunc)(AppletWidget *applet, gpointer data);
typedef void (*AppletStartNewFunc)(const gchar *param, gpointer data);

struct _AppletWidget
{
	GtkPlug			window;

	gint			applet_id;

	/* use these as prefixes when loading saving data */
	gchar			*cfgpath;
	gchar			*globcfgpath;
};

struct _AppletWidgetClass
{
	GtkPlugClass parent_class;

	/* when the orientation of the parent panel changes, you should 
	   connect this signal before doing applet_widget_add so that
	   you get an initial change_orient signal during the add, so
	   that you can update your orientation properly */
	void (* change_orient) (AppletWidget *applet,
				PanelOrientType orient);
	/* the panel background changes, the pixmap handeling is likely
	   to change */
	void (* back_change) (AppletWidget *applet,
			      PanelBackType type,
			      gchar *pixmap,
			      GdkColor *color);
	/*will send the current state of the tooltips, if they are enabled
	  or disabled, you should only need this if you are doing something
	  weird*/
	void (* tooltip_state) (AppletWidget *applet,
				gint enabled);
	/*when the panel wants to save a session it will call this signal 
	  if you trap it make sure you do gnome_config_sync() and
	  gnome_config_drop_all() after your done otherwise the changes
	  might not be written to file, also make sure you return
	  FALSE from this signal or your position wil not get saved!*/
	gint (* session_save) (AppletWidget *applet,
			       gchar *cfgpath,
			       gchar *globcfgpath);
};

guint		applet_widget_get_type		(void);

/*start one but add a parameter that the panel should use next time
  to start us*/
GtkWidget*	applet_widget_new_with_param	(const gchar *param);


/*start a normal applet*/
#define applet_widget_new()	\
	applet_widget_new_with_param("")

/*set tooltip over the applet, NULL to remove a tooltip*/
void		applet_widget_set_tooltip	(AppletWidget *applet,
						 gchar *text);

/*set tooltip on a specific widget inside the applet*/
void		applet_widget_set_widget_tooltip(AppletWidget *applet,
						 GtkWidget *widget,
						 gchar *text);

/* add a widget to the plug and register the applet, this has to
   be done after all the children had been added so that the applet-lib
   can bind the events over them so that peoplce can move them with
   the second button, get the menu, etc ...*/
void		applet_widget_add		(AppletWidget *applet,
						 GtkWidget *widget);

/* remove the plug from the panel, this will destroy the applet */
void		applet_widget_remove_from_panel (AppletWidget *applet);

/* The callback functions control the applet's right click menu, the name
   is just a string, which has to be unique and which controls the nesting,
   for example a name of "foo/bar" will add an item to the submenu
   identified by "/foo" (which you should have created before with
   register_callback_dir, use this for properies callback, help, about,
   etc... etc...
*/
/*add a callback onto the applet's right click menu*/
void		applet_widget_register_callback	(AppletWidget *applet,
						 gchar *name,
						 gchar *menutext,
						 AppletCallbackFunc func,
						 gpointer data);
/*remove a menuitem*/
void		applet_widget_unregister_callback (AppletWidget *applet,
						   gchar *name);
/*add a submenu*/
void		applet_widget_register_callback_dir (AppletWidget *applet,
						     char *name,
						     char *menutext);
/*remove a submenu*/
void		applet_widget_unregister_callback_dir (AppletWidget *applet,
						       char *name);


/* get the applet widget with the id of applet_id */
AppletWidget*	applet_widget_get_by_id		(gint applet_id);

/*get thenumber of applets*/
gint		applet_widget_get_applet_count	(void);

/*tell the panel to save the session here (just saves no shutdown),
  this should be done when you change some of your config and want
  the panel to save it's config, you should NOT call this in the
  session_save handler as it will result in a locked panel*/
void		applet_widget_sync_config	(void);

/* Get the oprientation the applet should use */
PanelOrientType	applet_widget_get_panel_orient	(AppletWidget *applet);


/*use this instead of gnome init, if you want multi applet, you also
  have to specify a "start new applet" function which will launch a new
  applet*/
error_t		applet_widget_init		(char *app_id,
						 struct argp *app_parser,
						 int argc,
						 char **argv,
						 unsigned int flags,
						 int *arg_index,
						 gchar *argv0,
						 gint last_die,
						 gint multi_applet,
						 AppletStartNewFunc new_func,
						 gpointer new_func_data);

/*defaults init for use with "normal" non-multi applets*/
#define \
applet_widget_init_defaults(app_id,app_parser,argc,argv,flags,arg_index,argv0) \
applet_widget_init(app_id,app_parser,argc,argv,flags,arg_index, \
		   argv0,TRUE,FALSE,NULL,NULL)


/* use this as gtk_main in applets */
void		applet_widget_gtk_main		(void);

/* convenience function for multi applets */
gchar *		make_param_string		(gint argc, char *argv[]);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __APPLET_WIDGET_H__ */
