#include <config.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libbonobo.h>

#include "applet-widget.h"
#include "applet-object.h"
#include "applet-private.h"

struct _AppletWidgetPrivate
{
	AppletObject           *object;

	/* something was added */
	gboolean		added_child;
	
	/*change freezing*/
	int			frozen_level;

	gboolean		frozen_got_orient;
	PanelOrientType		frozen_orient;			

	gboolean		frozen_got_size;
	int			frozen_size;			
	
	gboolean		frozen_got_back;
	PanelBackType		frozen_back_type;			
	char 			*frozen_back_pixmap;			
	GdkColor		frozen_back_color;			
	
	gboolean		frozen_got_position;
	gint			frozen_position_x;
	gint			frozen_position_y;

	/* for local case */
	GtkWidget		*ebox;
};

static void applet_widget_class_init    (AppletWidgetClass *klass);
static void applet_widget_instance_init	(AppletWidget      *widget);

typedef int (*SaveSignal)      (GtkObject *object,
				char      *cfgpath,
				char      *globcfgpath,
				gpointer   data);

typedef void (*BackSignal)     (GtkObject     *object,
				PanelBackType  type,
				char          *pixmap,
				GdkColor      *color,
				gpointer       data);

typedef void (*PositionSignal) (GtkObject *object,
				int        x,
				int        y,
				gpointer   data);

static int applet_count = 0;

static GtkPlugClass *parent_class;

GType
applet_widget_get_type (void)
{
	static GType applet_widget_type = 0;

	if (!applet_widget_type) {
		static const GtkTypeInfo applet_widget_info = {
			"AppletWidget",
			sizeof (AppletWidget),
			sizeof (AppletWidgetClass),
			(GtkClassInitFunc) applet_widget_class_init,
			(GtkObjectInitFunc) applet_widget_instance_init,
			NULL,
			NULL,
			NULL
		};

		applet_widget_type = gtk_type_unique (gtk_plug_get_type (), &applet_widget_info);
	}

	return applet_widget_type;
}

enum {
	CHANGE_ORIENT_SIGNAL,
	CHANGE_PIXEL_SIZE_SIGNAL,
	SAVE_SESSION_SIGNAL,
	BACK_CHANGE_SIGNAL,
	DO_DRAW_SIGNAL,
	TOOLTIP_STATE_SIGNAL,
	CHANGE_POSITION_SIGNAL,
	LAST_SIGNAL
};

static guint applet_widget_signals[LAST_SIGNAL] = {0};

static void
marshal_signal_save (GClosure *closure,
		     GValue *return_value,
		     guint n_param_values,
		     const GValue *param_values,
		     gpointer invocation_hint,
		     gpointer marshal_data)
{
  register SaveSignal callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  int v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }

  callback = (SaveSignal) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1, 
		       (char *) g_value_get_string (param_values + 1),
		       (char *) g_value_get_string (param_values + 2),
		       data2);

  g_value_set_int (return_value, v_return);

  /* make applets that forget to do this not fsckup */
  gnome_config_sync ();
  gnome_config_drop_all();
}


static void
marshal_signal_back  (GClosure *closure,
		     GValue *return_value,
		     guint n_param_values,
		     const GValue *param_values,
		     gpointer invocation_hint,
		     gpointer marshal_data)
{
  register BackSignal callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  
  g_return_if_fail (n_param_values == 4);
  
  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }

  callback = (BackSignal) (marshal_data ? marshal_data : cc->callback);

  callback (data1, 
	    g_value_get_enum (param_values + 1),
	    (char *) g_value_get_string (param_values + 2),
	    g_value_get_pointer (param_values + 3),
	    data2);
}

static void
marshal_signal_position   (GClosure *closure,
			   GValue *return_value,
			   guint n_param_values,
			   const GValue *param_values,
			   gpointer invocation_hint,
			   gpointer marshal_data)
{
  register PositionSignal callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (PositionSignal) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_value_get_int (param_values + 1),
            g_value_get_int (param_values + 2),
            data2);
}

static void
applet_widget_class_init (AppletWidgetClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) class;

	parent_class = gtk_type_class (gtk_plug_get_type ());

	applet_widget_signals[CHANGE_ORIENT_SIGNAL] =
		gtk_signal_new("change_orient",
			       GTK_RUN_FIRST,
			       GTK_CLASS_TYPE (object_class),
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 change_orient),
			       gtk_marshal_VOID__ENUM, /* FIXME: void isn't right here...should by gtk_marshal_NONE */
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_ENUM);
	applet_widget_signals[CHANGE_PIXEL_SIZE_SIGNAL] =
		gtk_signal_new("change_pixel_size",
			       GTK_RUN_FIRST,
			       GTK_CLASS_TYPE (object_class),
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 change_pixel_size),
			       gtk_marshal_NONE__INT,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_INT);
	applet_widget_signals[SAVE_SESSION_SIGNAL] =
		gtk_signal_new("save_session",
			       GTK_RUN_LAST,
			       GTK_CLASS_TYPE (object_class),
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 save_session),
			       marshal_signal_save,
			       GTK_TYPE_BOOL,
			       2,
			       GTK_TYPE_STRING,
			       GTK_TYPE_STRING);
	applet_widget_signals[BACK_CHANGE_SIGNAL] =
		gtk_signal_new("back_change",
			       GTK_RUN_LAST,
			       GTK_CLASS_TYPE (object_class),
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 back_change),
			       marshal_signal_back,
			       GTK_TYPE_NONE,
			       3,
			       GTK_TYPE_ENUM,
			       GTK_TYPE_POINTER,
			       GTK_TYPE_POINTER);
	applet_widget_signals[DO_DRAW_SIGNAL] =
		gtk_signal_new("do_draw",
			       GTK_RUN_LAST,
			       GTK_CLASS_TYPE (object_class),
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 do_draw),
			       gtk_signal_default_marshaller,
			       GTK_TYPE_NONE,
			       0);
	applet_widget_signals[TOOLTIP_STATE_SIGNAL] =
		gtk_signal_new("tooltip_state",
			       GTK_RUN_LAST,
			       GTK_CLASS_TYPE (object_class),
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 tooltip_state),
			       gtk_marshal_NONE__INT,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_INT);
	applet_widget_signals[CHANGE_POSITION_SIGNAL] =
		gtk_signal_new("change_position",
			       GTK_RUN_LAST,
			       GTK_CLASS_TYPE (object_class),
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 change_position),
			       marshal_signal_position,
			       GTK_TYPE_NONE,
			       2,
			       GTK_TYPE_INT,
			       GTK_TYPE_INT);

	class->change_orient = NULL;
	class->save_session = NULL;
	class->back_change = NULL;
	class->do_draw = NULL;
	class->tooltip_state = NULL;
	class->change_position = NULL;
}

static void
applet_widget_instance_init (AppletWidget *applet)
{
	g_return_if_fail (applet);
	g_return_if_fail (IS_APPLET_WIDGET (applet));

	applet->object = NULL;
	applet->orient = ORIENT_UP;
	applet->size   = PIXEL_SIZE_STANDARD;

	applet->priv = g_new0 (AppletWidgetPrivate, 1);

	applet->priv->added_child         = FALSE;
	applet->priv->frozen_level        = 0;
	applet->priv->frozen_got_orient   = FALSE;
	applet->priv->frozen_got_size     = FALSE;
	applet->priv->frozen_got_back     = FALSE;
	applet->priv->frozen_got_position = FALSE;
}

static void
applet_widget_destroy(GtkWidget *w, gpointer data)
{
	AppletWidget *applet;
	GtkPlug *plug;

	g_return_if_fail(w != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(w));

	applet = APPLET_WIDGET(w);
	plug = GTK_PLUG(w);

	/* XXX: hackaround to broken gtkplug/gtksocket, we kill the references
	   to ourselves on the socket and our references to the socket and
	   destroy the socket */
	if(plug->same_app && plug->socket_window) {
		GtkSocket *socket;
		gdk_window_get_user_data (plug->socket_window,
					  (gpointer *)&socket);
		if(socket) {
			GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET(socket));
			if (toplevel && GTK_IS_WINDOW (toplevel))
				gtk_window_remove_embedded_xid (GTK_WINDOW (toplevel), 
								GDK_WINDOW_XWINDOW (socket->plug_window));
			socket->plug_window = NULL;
			socket->same_app = FALSE;
			plug->socket_window = NULL;
			plug->same_app = FALSE;
			gtk_widget_destroy(GTK_WIDGET(socket));
		}
	}

	if(applet->priv->ebox && GTK_BIN(applet->priv->ebox)->child) {
		GtkWidget *child = GTK_BIN(applet->priv->ebox)->child;
		/* disconnect the destroy handler */
		gtk_signal_disconnect_by_data(GTK_OBJECT(child), applet);
		gtk_widget_destroy(child);
	}


	if (applet->privcfgpath) {
		g_free (applet->privcfgpath);
		g_free (applet->globcfgpath);

		applet->privcfgpath = NULL;
		applet->globcfgpath = NULL;

		/*
		 * if nothing has been added as our child, this 
		 * means we have not yet fully completed load, so 
		 * notify the panel that we are going to die
		 */
		if (!applet->priv->added_child)
			applet_object_abort_load (applet->object);
	}

	bonobo_object_unref (BONOBO_OBJECT (applet->object));
	applet->object = NULL;

	applet_count--;

	if (!applet_count)
		gtk_main_quit();

	g_free (applet->priv);
	applet->priv = NULL;
}

/**
 * applet_widget_new:
 * @iid: The implementation id of the applet we are starting
 *
 * Description: Make a new applet and register us with the panel, if you
 * decide to cancel the load before calling #applet_widget_add, you should
 * call #applet_widget_abort_load.  This widget is a simple container but you
 * should always use only #applet_widget_add to add a child and you should
 * only use it once.
 *
 * Returns: A pointer to a new widget of type #AppletWidget, or %NULL if 
 * something went wrong.
 **/
GtkWidget *
applet_widget_new (const char *iid)
{
	AppletWidget *applet;

	applet = APPLET_WIDGET (gtk_type_new (applet_widget_get_type()));
	applet_widget_construct (applet, iid);

	return GTK_WIDGET (applet);
}

/**
 * applet_widget_construct:
 * @applet: #AppletWidget to work on
 * @id: the implementation id of the applet to construct
 *
 * Description: For bindings and subclassing only
 **/
void
applet_widget_construct (AppletWidget *applet,
			 const char   *iid)
{
	GdkWindow           *win;
	guint32              winid;
	
	g_return_if_fail (iid);

	applet->object = applet_object_new (GTK_WIDGET (applet), iid, &winid);

	applet_debug_log ("applet_widget_construct: winid = %d\n", winid);

	win = gdk_window_lookup (winid);

	gtk_plug_construct (GTK_PLUG (applet), winid);

	/*
	 * after doing all that we just take the 
	 * socket and put it in limbo
	 */
	if (win) {
		GtkWidget *socket;

		gdk_window_get_user_data (win, (gpointer *)&socket);
		if (socket) {
			GtkWidget *temp_window;

			temp_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

			applet->priv->ebox = socket->parent;

			gtk_widget_set_uposition (GTK_WIDGET (temp_window),
						  gdk_screen_width ()  + 1,
						  gdk_screen_height () + 1);
			gtk_widget_realize (temp_window);
			gtk_widget_reparent (GTK_WIDGET (socket),
					     temp_window);
			gtk_signal_connect_object (GTK_OBJECT (applet->priv->ebox),
						   "destroy",
						   GTK_SIGNAL_FUNC (gtk_widget_destroy),
						   GTK_OBJECT (temp_window));
		}
	}
	
	gtk_signal_connect (GTK_OBJECT (applet), "destroy",
			    GTK_SIGNAL_FUNC (applet_widget_destroy),
			    NULL);

	applet_count++;
}

/**
 * applet_widget_get_applet_count:
 *
 * Description:  Gets the number of applets loaded in this this process.  If
 * this is a shared lib applet it will return the total number of shared lib
 * applets loaded.
 *
 * Returns:  The number of applets loaded.
 **/
int
applet_widget_get_applet_count(void)
{
	return applet_count;
}

static gboolean
applet_event(GtkWidget *w, GdkEvent *event, AppletWidget *aw)
{
	GdkEventButton *bevent = (GdkEventButton *)event;
	if(event->type == GDK_BUTTON_PRESS && 
	   (bevent->button == 3 || bevent->button == 2)) {
		XButtonEvent ev;
		GtkWidget *wi;
		GtkPlug *plug = GTK_PLUG(aw);

		/* on local case */
		if(aw->priv->ebox)
			return gtk_widget_event(aw->priv->ebox, event);

		if((wi = gtk_grab_get_current()))
			gtk_grab_remove(wi);
		gdk_pointer_ungrab(GDK_CURRENT_TIME);
		gdk_keyboard_ungrab(GDK_CURRENT_TIME);
		gdk_flush();
		gtk_signal_emit_stop_by_name(GTK_OBJECT(w),
					     "event");
		ev.type = ButtonPress;
		ev.send_event = True;
		ev.display = GDK_DISPLAY();
		ev.window = GDK_WINDOW_XWINDOW(plug->socket_window);
		ev.subwindow = None;
		ev.time = bevent->time;
		ev.x = bevent->x;
		ev.y = bevent->y;
		ev.x_root = bevent->x_root;
		ev.y_root = bevent->y_root;
		ev.state = bevent->state;
		ev.button = bevent->button;
		ev.same_screen = True;
		/* in the local case send it to our ebox */
		if(aw->priv->ebox && aw->priv->ebox->window) {
			XSendEvent(GDK_DISPLAY(),
				   GDK_WINDOW_XWINDOW(aw->priv->ebox->window),
				   True,NoEventMask,(XEvent *)&ev);
		} else if(plug->socket_window)
			XSendEvent(GDK_DISPLAY(),
				   GDK_WINDOW_XWINDOW(plug->socket_window),
				   True,NoEventMask,(XEvent *)&ev);
		
		return TRUE;
	}
	return FALSE;
}

static void
bind_applet_events(GtkWidget *widget, gpointer data)
{
	if (!GTK_WIDGET_NO_WINDOW(widget)) {
		gtk_signal_connect(GTK_OBJECT(widget), "event",
				   (GtkSignalFunc) applet_event,
				   data);
	}
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, data);
}

static void
destroy_the_applet(GtkWidget *w, AppletWidget *applet)
{
	applet->priv->ebox = NULL;
	gtk_widget_destroy(GTK_WIDGET(applet));
}

/**
 * applet_widget_add_full:
 * @applet: the #AppletWidget to work with
 * @widget: the child to add
 * @bind_events: bind 2nd and 3rd button events over the applet if %TRUE
 *
 * Description:  Add a child (@widget) to the @applet.  This finishes the
 * handshaking with the panel started in @applet_widget_new.  You should never
 * call this function twice for the same @applet and you should always use
 * this function rather then #gtk_container_add.  If you have already created
 * an applet widget with #applet_widget_new, but need to cancel the loading
 * of the applet, use #applet_widget_abort_load.  This function is only for
 * special applets and you should use #applet_widget_bind_events on some
 * internal widget if @bind_events was %FALSE.  Normally you'll just want to
 * use #applet_widget_add.
 **/
void
applet_widget_add_full (AppletWidget *applet,
			GtkWidget    *widget,
			gboolean      bind_events)
{

	g_return_if_fail (applet && IS_APPLET_WIDGET(applet));
	g_return_if_fail (widget && GTK_IS_WIDGET(widget));

	if (applet->priv->ebox) {
		gtk_container_add (GTK_CONTAINER (applet->priv->ebox), widget);

		gtk_signal_connect (GTK_OBJECT (widget),
				    "destroy",
				    GTK_SIGNAL_FUNC (destroy_the_applet),
				    applet);
	} else
		gtk_container_add (GTK_CONTAINER (applet), widget);

	applet_object_register (applet->object);

	if (bind_events) {
		if (applet->priv->ebox)
			bind_applet_events (widget, applet);
		else
			bind_applet_events (GTK_WIDGET (applet), applet);
	}

	applet->priv->added_child = TRUE;
}

/**
 * applet_widget_add:
 * @applet: the #AppletWidget to work with
 * @widget: the child to add
 *
 * Description:  Add a child (@widget) to the @applet.  This finishes the
 * handshaking with the panel started in @applet_widget_new.  You should never
 * call this function twice for the same @applet and you should always use
 * this function rather then #gtk_container_add.  If you have already created
 * an applet widget with #applet_widget_new, but need to cancel the loading
 * of the applet, use #applet_widget_abort_load.
 **/
void
applet_widget_add (AppletWidget *applet,
		   GtkWidget    *widget)
{
	applet_widget_add_full (applet, widget, TRUE);
}

/**
 * applet_widget_bind_events:
 * @applet: the #AppletWidget to work with
 * @widget: the widget over which to bind events
 *
 * Description:  Binds the 2nd and 3rd button clicks over this widget. 
 * Normally this is done during #applet_widget_add, but if you need to
 * bind events over a widget which you added later, use this function.
 **/
void
applet_widget_bind_events(AppletWidget *applet, GtkWidget *widget)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));
	g_return_if_fail(widget != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(widget));
	
	if(applet->priv->ebox && GTK_WIDGET(applet) == widget) {
		GtkWidget *child = GTK_BIN(applet->priv->ebox)->child;
		if(child) bind_applet_events(child, applet);
	} else
		bind_applet_events(GTK_WIDGET(widget), applet);
}

static GtkTooltips *
applet_widget_tooltips (void)
{
	static GtkTooltips *applet_tooltips = NULL;

	if (!applet_tooltips)
		applet_tooltips = gtk_tooltips_new();

	return applet_tooltips;
}

/**
 * applet_widget_set_tooltip:
 * @applet: the #AppletWidget to work with
 * @widget: the widget to set tooltip on
 * @text: the tooltip text
 *
 * Description:  Set a tooltip on the @widget that will follow the tooltip
 * setting from the panel configuration.
 **/
void
applet_widget_set_tooltip (AppletWidget *applet,
			   GtkWidget    *widget,
			   const char   *text)
{
	g_return_if_fail (applet && IS_APPLET_WIDGET (applet));
	g_return_if_fail (widget && GTK_IS_WIDGET (widget));

	gtk_tooltips_set_tip (applet_widget_tooltips (),
			      widget, text, NULL);
}

/**
 * applet_widget_get_panel_orient:
 * @applet: the #AppletWidget to work with
 *
 * Description:  Gets the orientation of the panel this widget is on.
 * it can be one of ORIENT_UP, ORIENT_DOWN, ORIENT_LEFT and ORIENT_RIGHT.
 * This is not the position of the panel, but rather the direction that the
 * applet should be "reaching out".  So any arrows should for example point
 * in this direction.  It will be ORIENT_UP or ORIENT_DOWN for horizontal
 * panels and ORIENT_LEFT or ORIENT_RIGHT for vertical panels
 *
 * Returns:  PanelOrientType enum of the orientation
 **/
PanelOrientType
applet_widget_get_panel_orient (AppletWidget *applet)
{
	g_return_val_if_fail (applet && IS_APPLET_WIDGET (applet), ORIENT_UP);

	return applet->orient;
}

/**
 * applet_widget_get_panel_pixel_size:
 * @applet: the #AppletWidget to work with
 *
 * Description:  Gets the width of the panel in pixels.  This is not the
 * actual size, but the recomended one.  The panel may be streched if the
 * applets use larger sizes then this.
 *
 * Returns:  Size of panel in pixels
 **/
int
applet_widget_get_panel_pixel_size(AppletWidget *applet)
{
	g_return_val_if_fail(applet != NULL, PIXEL_SIZE_STANDARD);
	g_return_val_if_fail(IS_APPLET_WIDGET(applet), PIXEL_SIZE_STANDARD);

	return applet->size;
}

/**
 * applet_widget_get_rgb_background:
 * @applet: the #AppletWidget to work with
 * @rgb: pointer to a pointer to which the rgb buffer will be returned
 * @w: pointer to an integer in which the width will be stored
 * @h: pointer to an integer in which the height will be stored
 * @rowstride: pointer to an integer in which the rowstride will be stored
 *
 * Description:  Gets an rgb buffer that you can draw your applet on.  Useful
 * in conjunction with the do_draw signal and the #applet_widget_send_draw
 * method.  The rgb should be freed after use with g_free.
 **/
void
applet_widget_get_rgb_background (AppletWidget  *applet,
				  guchar       **rgb,
				  int           *w,
				  int           *h,
				  int           *rowstride)
{
	GNOME_Panel_RgbImage *image;

	g_return_if_fail (applet && IS_APPLET_WIDGET (applet));
	g_return_if_fail (rgb && w && h && rowstride);

	image = applet_object_get_rgb_background (applet->object);

	*w = image->width;
	*h = image->height;
	if(!image->color_only)
		*rowstride = image->rowstride;
	else
		*rowstride = (*w)*3;

	if(image->data._buffer) {
		*rgb = g_new(guchar, (*h)*(*rowstride));
		if(!image->color_only) {
			int size = (*h)*(*rowstride);
			if(image->data._length<size)
				size = image->data._length;
			memcpy(*rgb,image->data._buffer,
			       sizeof(guchar)*size);
		} else {
			int i;
			int r;
			int g;
			int b;
			guchar *p;

			r = *(image->data._buffer);
			g = *(image->data._buffer+1);
			b = *(image->data._buffer+2);

			p = *rgb;
			for(i=0;i<(*w)*(*h);i++) {
				*(p++) = r;
				*(p++) = g;
				*(p++) = b;
			}
		}
	} else {
		/* this will make a black background */
		*rgb = g_new0(guchar, (*h)*(*rowstride));
	}
	
	CORBA_free (image);
}

/**
 * applet_widget_queue_resize:
 * @applet: #AppletWidget to work on
 *
 * Description:  For shared library applets this calls #gtk_widget_queue_resize
 * on the internal panel eventbox, for external applets this just calls this on
 * the #AppletWidget itself, but in both cases it forces a resize of the socket
 * on the panel
 **/
void
applet_widget_queue_resize (AppletWidget *applet)
{
	GtkPlug *plug;

	g_return_if_fail (applet);
	g_return_if_fail (IS_APPLET_WIDGET (applet));

	plug = GTK_PLUG (applet);

	if (applet->priv->ebox)
		gtk_widget_queue_resize (applet->priv->ebox);
	else
		gtk_widget_queue_resize (GTK_WIDGET (applet));
}

void
applet_widget_change_orient (AppletWidget          *widget,
			     const PanelOrientType  orient)
{
	widget->orient = orient;

        if (widget->priv->frozen_level > 0) {
                widget->priv->frozen_got_orient = TRUE;
                widget->priv->frozen_orient     = (PanelOrientType)orient;
        } else {
                gtk_signal_emit (GTK_OBJECT(widget),
                                 applet_widget_signals [CHANGE_ORIENT_SIGNAL],
                                 (PanelOrientType)orient);
        }
}

void
applet_widget_change_size (AppletWidget      *widget,
			   const CORBA_short  size)
{
	widget->size = size;

	if (widget->priv->frozen_level > 0) {
		widget->priv->frozen_got_size = TRUE;
		widget->priv->frozen_size     = size;
	} else {
		gtk_signal_emit (GTK_OBJECT (widget),
				applet_widget_signals [CHANGE_PIXEL_SIZE_SIGNAL],
				size);
	}
}

void
applet_widget_change_position (AppletWidget *widget,
			       const gint    x,
			       const gint    y)
{
	if (widget->priv->frozen_level > 0) {
		widget->priv->frozen_got_position = TRUE;
		widget->priv->frozen_position_x   = x;
		widget->priv->frozen_position_y   = y;
	}
	else {
		gtk_signal_emit (GTK_OBJECT (widget),
				 applet_widget_signals [CHANGE_POSITION_SIGNAL],
				 x, y);
	}
}

void
applet_widget_background_change (AppletWidget  *widget,
				 PanelBackType  type,
				 gchar         *pixmap,
				 GdkColor       color)
{
	if (widget->priv->frozen_level > 0) {
		widget->priv->frozen_got_back   = TRUE;
		widget->priv->frozen_back_type  = type;
		widget->priv->frozen_back_color = color;

		if (widget->priv->frozen_back_pixmap) {
			g_free (widget->priv->frozen_back_pixmap);
			widget->priv->frozen_back_pixmap = g_strdup (pixmap);
		}
	} 
	else {
		gtk_signal_emit (GTK_OBJECT (widget),
				 applet_widget_signals [BACK_CHANGE_SIGNAL],
				 type, pixmap, color);
	}
}

void
applet_widget_tooltips_enable (AppletWidget *widget)
{
	gtk_tooltips_enable (applet_widget_tooltips ());

	gtk_signal_emit (GTK_OBJECT (widget),
                         applet_widget_signals [TOOLTIP_STATE_SIGNAL],
                         TRUE);
}

void
applet_widget_tooltips_disable (AppletWidget *widget)
{
	gtk_tooltips_disable (applet_widget_tooltips ());

	gtk_signal_emit (GTK_OBJECT (widget),
                         applet_widget_signals [TOOLTIP_STATE_SIGNAL],
                         FALSE);
}

void
applet_widget_draw (AppletWidget *widget)
{
	gtk_signal_emit (GTK_OBJECT (widget),
			 applet_widget_signals [DO_DRAW_SIGNAL]);
}

gboolean
applet_widget_save_session (AppletWidget *widget,
			    const gchar  *config_path,
			    const gchar  *global_config_path)
{
	/*
	 * FIXME: is this 'done' or 'notdone'
	 */
	gboolean done;

	gtk_signal_emit (GTK_OBJECT (widget),
			 applet_widget_signals [SAVE_SESSION_SIGNAL],
			 config_path, 
			 global_config_path,
			 &done);

	/*
	 * @done == true would mean that the applet handled
	 * the session saving itself, therefore we pass the 
	 * reverse to the corba function
	 */
	return !done;
}

void
applet_widget_freeze_changes (AppletWidget *widget)
{
	widget->priv->frozen_level++;
}

void
applet_widget_thaw_changes (AppletWidget *widget)
{
	if (widget->priv->frozen_level > 0)
		widget->priv->frozen_level--;
	
	if (widget->priv->frozen_level > 0)
		return;

	if (widget->priv->frozen_got_orient) {
		widget->priv->frozen_got_orient = FALSE;

		gtk_signal_emit (GTK_OBJECT (widget),
				 applet_widget_signals [CHANGE_ORIENT_SIGNAL],
				 widget->priv->frozen_orient);
	}

	if (widget->priv->frozen_got_size) {
		widget->priv->frozen_got_size = FALSE;

		gtk_signal_emit (GTK_OBJECT (widget),
				 applet_widget_signals [CHANGE_PIXEL_SIZE_SIGNAL],
				 widget->priv->frozen_size);
	}

	if (widget->priv->frozen_got_back) {
		widget->priv->frozen_got_back = FALSE;

		gtk_signal_emit (GTK_OBJECT (widget),
				 applet_widget_signals [BACK_CHANGE_SIGNAL],
				 widget->priv->frozen_back_type,
				 widget->priv->frozen_back_pixmap,
				 &widget->priv->frozen_back_color);

		g_free (widget->priv->frozen_back_pixmap);
	}

	if (widget->priv->frozen_got_position) {
		widget->priv->frozen_got_position = FALSE;

		gtk_signal_emit (GTK_OBJECT (widget),
				 applet_widget_signals [CHANGE_POSITION_SIGNAL],
				 widget->priv->frozen_position_x,
				 widget->priv->frozen_position_y);
	}
}
