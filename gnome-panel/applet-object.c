#include <config.h>
#include <string.h>

#include <libbonobo.h>
#include <bonobo-activation/bonobo-activation.h>

#include "applet-object.h"
#include "applet-widget.h"
#include "applet-private.h"

struct _AppletObjectPrivate {
	AppletWidget                   *widget;
	GNOME_PanelSpot                 panel_spot;
	gchar                          *iid;
	GSList                         *callbacks;
};

typedef struct {
        gchar                          *name;
        AppletObjectCallbackFunc        func;
        gpointer                        data;
} AppletObjectCallbackInfo;

static GObjectClass *applet_object_parent_class = NULL;

/*
 * applet_object_abort_load:
 * @applet: an #AppletObject.
 *
 * Abort the applet loading, once applet has been created, this is
 * a way to tell the panel to forget about us if we decide we want to quit
 * before we add the actual applet to the applet-widget.  This is only useful
 * to abort after #applet_widget_new was called but before #applet_widget_add
 * is called.
 */
void
applet_object_abort_load (AppletObject *applet)
{
	CORBA_Environment env;

        g_return_if_fail (applet && APPLET_IS_OBJECT (applet));

	CORBA_exception_init (&env);

	GNOME_PanelSpot_abort_load (applet->priv->panel_spot, &env);

	CORBA_exception_free (&env);

}

/*
 * applet_object_remove:
 * @applet: an #AppletObject.
 *
 * Description:  Remove the plug from the panel, this will destroy the applet.
 * You can only call this once for each applet.
 **/
void
applet_object_remove (AppletObject *applet)
{
        CORBA_Environment env;

        g_return_if_fail (applet && APPLET_IS_OBJECT (applet));

        CORBA_exception_init (&env);

        GNOME_PanelSpot_unregister_us (applet->priv->panel_spot, &env);

        CORBA_exception_free (&env);
}

/*
 * applet_object_sync_config:
 * @applet: an #AppletObject.
 *
 * Description:  Tell the panel to save our session here (just saves, no
 * shutdown).  This should be done when you change some of your config and want
 * the panel to save it's config, you should NOT call this in the session_save
 * handler as it will result in a locked panel, as it will actually trigger
 * another session_save signal for you.  However it also asks for a complete
 * panel save, so you should not do this too often, and only when the user
 * has changed some preferences and you want to sync them to disk.
 * Theoretically you don't even need to do that if you don't mind loosing
 * settings on a panel crash or when the user kills the session without
 * logging out properly, since the panel will always save your session when
 * it exists.
 */
void
applet_object_sync_config (AppletObject *applet)
{
        CORBA_Environment env;

        g_return_if_fail (applet && APPLET_IS_OBJECT (applet));

        CORBA_exception_init (&env);

        GNOME_PanelSpot_sync_config (applet->priv->panel_spot, &env);

        CORBA_exception_free (&env);
}

static AppletObjectCallbackInfo *
applet_object_lookup_callback (AppletObject *applet,
			       const gchar  *name)
{
	GSList *l;

	for (l = applet->priv->callbacks; l; l = l->next) {
		AppletObjectCallbackInfo *info;

		info = (AppletObjectCallbackInfo *)l->data;

		if (!strcmp (name, info->name))
			return info;
	}

	return NULL;
}

/*
 * applet_object_register_callback:
 * @applet: an #AppletWidget.
 * @name: path to the menu item.
 * @stock_type: GNOME_STOCK string to use for the pixmap
 * @menutext: text for the menu item.
 * @func: #AppletObjectCallbacFunc to call when the menu item is activated.
 * @data: data to be passed to @func.
 *
 * Description:  Adds a menu item to the applet's context menu.  The name
 * should be a path that is separated by '/' and ends in the name of this
 * item.  You need to add any submenus with
 * #applet_widget_register_callback_dir.
 */
void
applet_object_register_callback (AppletObject *applet,
				 const gchar  *name,
				 const gchar  *stock_item,
				 const gchar  *menutext,
				 AppletObjectCallbackFunc  func,
				 gpointer      data)
{
	AppletObjectCallbackInfo *info;

        g_return_if_fail (applet && APPLET_IS_OBJECT (applet));
	g_return_if_fail (name && menutext && func);

	if (!stock_item)
		stock_item = "";

	info = applet_object_lookup_callback (applet, name);

	if (!info) {
		info = g_new0 (AppletObjectCallbackInfo, 1);

		applet->priv->callbacks =
			g_slist_prepend (applet->priv->callbacks,
					 info);
	}
	else
		g_free (info->name);

        info->name = g_strdup (name);
        info->func = func;
        info->data = data;

        {
                CORBA_Environment env;

                CORBA_exception_init (&env);

                GNOME_PanelSpot_add_callback (applet->priv->panel_spot,
                                              name,
					      stock_item,
                                              menutext,
					      &env);

                CORBA_exception_free (&env);
        }
}

/*
 * applet_object_unregister_callback:
 * @applet: an #Appletobject.
 * @name: path to the menu item.
 *
 * Description:  Remove a menu item from the applet's context menu.  The
 * @name should be the full path to the menu item.  This will not remove
 * any submenus.
 */
void
applet_object_unregister_callback (AppletObject *applet,
				   const char   *name)
{
        AppletObjectCallbackInfo *info;

        g_return_if_fail (applet && APPLET_IS_OBJECT (applet) && name);

	info = applet_object_lookup_callback (applet, name);
	if (!info)
		return;

        applet->priv->callbacks = g_slist_remove (applet->priv->callbacks, info);

        {
                CORBA_Environment env;

                CORBA_exception_init (&env);

                GNOME_PanelSpot_remove_callback (applet->priv->panel_spot,
						 name,
						 &env);

                CORBA_exception_free (&env);
        }
}

/*
 * applet_object_callback_set_sensitive:
 * @applet: an #AppletObject.
 * @name: path to the menu item.
 * @sensitive: whether menu item should be sensitive.
 *
 * Description:  Sets the sensitivity of a menu item in the applet's
 * context menu.
 */
void
applet_object_callback_set_sensitive (AppletObject *applet,
				      const gchar  *name,
                                      gboolean      sensitive)
{
	CORBA_Environment env;

	g_return_if_fail (applet && APPLET_IS_OBJECT (applet) && name);

	CORBA_exception_init (&env);

	GNOME_PanelSpot_callback_set_sensitive (applet->priv->panel_spot,
						name,
						sensitive,
						&env);
	CORBA_exception_free (&env);
}

/*
 * applet_object_set_tooltip:
 * @applet: an #AppletObject.
 * @text: the tooltip text
 *
 * Description:  Set a tooltip on the entire applet that will follow the
 * tooltip setting from the panel configuration.
 */
void
applet_object_set_tooltip (AppletObject *applet,
			   const char   *text)
{
        CORBA_Environment env;

	g_return_if_fail (applet && APPLET_IS_OBJECT (applet));
	
	if (!text)
		text = "";

        CORBA_exception_init (&env);

        GNOME_PanelSpot__set_tooltip (applet->priv->panel_spot, text, &env);

        CORBA_exception_free (&env);
}

/*
 * applet_object_get_free_space:
 * @applet: an #AppletObject.
 *
 * Description:  Gets the free space left that you can use for your applet.
 * This is the number of pixels around your applet to both sides.  If you
 * strech by this amount you will not disturb any other applets.  If you
 * are on a packed panel 0 will be returned.
 *
 * Returns:  Free space left for your applet.
 */
gint
applet_object_get_free_space (AppletObject *applet)
{
        CORBA_Environment env;
        gint              retval;

	g_return_val_if_fail (applet && APPLET_IS_OBJECT (applet), 0);

        CORBA_exception_init (&env);

        retval = GNOME_PanelSpot__get_free_space (applet->priv->panel_spot, &env);

        CORBA_exception_free (&env);

        return retval;
}

/*
 * applet_object_send_position:
 * @applet: an #AppletObject.
 * @enable: whether to enable or disable change_position signal
 *
 * Description:  If you need to get a signal everytime this applet changes
 * position relative to the screen, you need to run this function with %TRUE
 * for @enable and bind the change_position signal on the applet.  This signal
 * can be quite CPU/bandwidth consuming so only applets which need it should
 * use it.  By default change_position is not sent.
 */
void
applet_object_send_position (AppletObject *applet,
			     gboolean      enable)
{
        CORBA_Environment env;

	g_return_if_fail (applet && APPLET_IS_OBJECT (applet));

        CORBA_exception_init (&env);

        GNOME_PanelSpot__set_send_position (applet->priv->panel_spot,
                                            enable, &env);

        CORBA_exception_free (&env);
}

/*
 * applet_object_send_draw:
 * @applet: the #AppletObject to work with
 * @enable: whether to enable or disable do_draw signal
 *
 * Description:  If you are using rgb background drawing, call this function
 * with %TRUE for @enable, and then bind the do_draw signal.  Inside that
 * signal you can get an RGB buffer to draw on with #applet_widget_get_rgb_bg.
 * The do_draw signal will only be sent when the RGB truly changed.
 */
void
applet_object_send_draw (AppletObject *applet,
                         gboolean      enable)
{
        CORBA_Environment env;

	g_return_if_fail (applet && APPLET_IS_OBJECT (applet));

        CORBA_exception_init (&env);

        GNOME_PanelSpot__set_send_draw (applet->priv->panel_spot,
                                        enable, &env);

        CORBA_exception_free (&env);
}

/*
 * applet_object_get_rgb_background:
 * @applet: an #AppletObject.
 *
 * get the #GNOME::Panel::RgbImage from the #GNOME::PanelSpot.
 *
 * Return value: an #GNOME::Panel::RgbImage.
 */
GNOME_Panel_RgbImage *
applet_object_get_rgb_background (AppletObject  *applet)
{
	GNOME_Panel_RgbImage *image;
	CORBA_Environment     env;

	g_return_val_if_fail (applet && APPLET_IS_OBJECT (applet), NULL);

	CORBA_exception_init (&env);

	image = GNOME_PanelSpot__get_rgb_background (applet->priv->panel_spot,
						     &env);

	CORBA_exception_free (&env);

	return image;
}

/*
 * applet_object_register:
 * @applet: an #AppletObject.
 *
 * register the applet with the panel.
 */
void
applet_object_register (AppletObject *applet)
{
	CORBA_Environment env;

	g_return_if_fail (applet && APPLET_IS_OBJECT (applet));

	CORBA_exception_init (&env);

	GNOME_PanelSpot_register_us (applet->priv->panel_spot, &env);

	CORBA_exception_free (&env);
}

static GNOME_Panel
applet_object_panel (void)
{
        static GNOME_Panel panel_client = CORBA_OBJECT_NIL;
	CORBA_Environment  env;

	CORBA_exception_init (&env);

        if (panel_client == CORBA_OBJECT_NIL)
                panel_client = bonobo_activation_activate_from_id (
                                                    "OAFIID:GNOME_Panel",
                                                    0, NULL, &env);

	CORBA_exception_free (&env);

	return panel_client;
}

/*
 * FIXME: is this needed?
 *
 * applet_object_panel_quit:
 * @applet: an #AppletObject.
 *
 * Description: Trigger 'Log out' on the panel.  This shouldn't be
 * used in normal applets, as it is not normal for applets to trigger
 * a logout.
 */
void
applet_object_panel_quit (AppletObject *applet)
{
	CORBA_Environment env;

	CORBA_exception_init (&env);

        GNOME_Panel_quit (applet_object_panel (), &env);
        if (BONOBO_EX (&env)) {
                CORBA_exception_free (&env);
                return;
        }

        CORBA_exception_free (&env);
}

static CORBA_string
impl_GNOME_Applet2__get_iid (PortableServer_Servant  servant,
			     CORBA_Environment      *ev)
{
	AppletObject *applet = APPLET_OBJECT (bonobo_object (servant));

	return CORBA_string_dup (applet->priv->iid);
}

static void
impl_GNOME_Applet2_change_orient (PortableServer_Servant        servant,
				  const GNOME_Panel_OrientType  orient,
				  CORBA_Environment            *ev)
{
	AppletObject *applet = APPLET_OBJECT (bonobo_object (servant));

	applet_widget_change_orient (applet->priv->widget, orient);
}

static void
impl_GNOME_Applet2_change_size (PortableServer_Servant  servant,
				const CORBA_short       size,
				CORBA_Environment      *ev)
{
	AppletObject *applet = APPLET_OBJECT (bonobo_object (servant));

	applet_widget_change_size (applet->priv->widget, size);
}

static void
impl_GNOME_Applet2_change_position (PortableServer_Servant  servant,
				    const CORBA_short       x,
				    const CORBA_short       y,
				    CORBA_Environment      *ev)
{
	AppletObject *applet = APPLET_OBJECT (bonobo_object (servant));

	applet_widget_change_position (applet->priv->widget, x, y);
}

static void
impl_GNOME_Applet2_do_callback (PortableServer_Servant  servant,
				const CORBA_char       *callback_name,
				CORBA_Environment      *ev)
{
	AppletObject             *applet;
	AppletObjectCallbackInfo *info;

	applet = APPLET_OBJECT (bonobo_object (servant));

	info = applet_object_lookup_callback (applet, callback_name);

	if (info && info->func)
		info->func (GTK_WIDGET (applet->priv->widget), info->data);
}

static void
impl_GNOME_Applet2_back_change (PortableServer_Servant          servant,
				const GNOME_Panel_BackInfoType *backing,
				CORBA_Environment              *ev)
{
	AppletObject *applet = APPLET_OBJECT (bonobo_object (servant));
	GdkColor      color;
	gchar        *pixmap = NULL;

	memset (&color, 0, sizeof (GdkColor));

	switch (backing->_d) {
	case GNOME_Panel_BACK_COLOR:
		color.red   = backing->_u.c.red;
		color.green = backing->_u.c.green;
		color.blue  = backing->_u.c.blue;
		break;
	case GNOME_Panel_BACK_PIXMAP:
		pixmap = backing->_u.pmap;
		break;
	case GNOME_Panel_BACK_NONE:
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	applet_widget_background_change (applet->priv->widget,
					 backing->_d,
					 pixmap,
					 color);
}

static void
impl_GNOME_Applet2_set_tooltips_state (PortableServer_Servant  servant,
				       const CORBA_boolean     enabled,
				       CORBA_Environment      *ev)
{
	AppletObject *applet = APPLET_OBJECT (bonobo_object (servant));

	if (enabled)
		applet_widget_tooltips_enable (applet->priv->widget);
	else
		applet_widget_tooltips_disable (applet->priv->widget);
}

static void
impl_GNOME_Applet2_draw (PortableServer_Servant  servant,
			 CORBA_Environment      *ev)
{
	AppletObject *applet = APPLET_OBJECT (bonobo_object (servant));

	applet_widget_draw (applet->priv->widget);
}

static void
impl_GNOME_Applet2_save_session (PortableServer_Servant     servant,
				 const CORBA_char          *config_path,
				 const CORBA_char          *global_config_path,
				 const CORBA_unsigned_long  cookie,
				 CORBA_Environment         *ev)
{
	AppletObject *applet = APPLET_OBJECT (bonobo_object (servant));
	gboolean      done;

	done = applet_widget_save_session (applet->priv->widget,
					   config_path,
					   global_config_path);


	GNOME_PanelSpot_done_session_save (applet->priv->panel_spot,
                                           done, cookie, ev);
}

static void
impl_GNOME_Applet2_freeze_changes (PortableServer_Servant  servant,
				   CORBA_Environment      *ev)
{
	AppletObject *applet = APPLET_OBJECT (bonobo_object (servant));

	applet_widget_freeze_changes (applet->priv->widget);
}

static void
impl_GNOME_Applet2_thaw_changes (PortableServer_Servant  servant,
				 CORBA_Environment      *ev)
{
	AppletObject *applet = APPLET_OBJECT (bonobo_object (servant));

	applet_widget_thaw_changes (applet->priv->widget);
}

static void
applet_object_finalize (GObject *object)
{
	AppletObject      *applet = APPLET_OBJECT (object);
	CORBA_Environment  env;
	GSList            *l;

	CORBA_exception_init (&env);

	gtk_widget_unref (GTK_WIDGET (applet->priv->widget));

	g_free (applet->priv->iid);

	CORBA_Object_release (applet->priv->panel_spot, &env);

	for (l = applet->priv->callbacks; l; l = l->next) {
		AppletObjectCallbackInfo *info;

		info = (AppletObjectCallbackInfo *)l->data;

		g_free (info->name);
		g_free (info);
	}
	g_slist_free (applet->priv->callbacks);
	applet->priv->callbacks = NULL;

	if (applet->priv) {
		g_free (applet->priv);
		applet->priv = NULL;
	}

	applet_object_parent_class->finalize (object);

	CORBA_exception_free (&env);
}

static void
applet_object_class_init (AppletObjectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	applet_object_parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = applet_object_finalize;

	klass->epv._get_iid           = impl_GNOME_Applet2__get_iid;
	klass->epv.change_orient      = impl_GNOME_Applet2_change_orient;
	klass->epv.do_callback        = impl_GNOME_Applet2_do_callback;
	klass->epv.back_change        = impl_GNOME_Applet2_back_change;
	klass->epv.set_tooltips_state = impl_GNOME_Applet2_set_tooltips_state;
	klass->epv.draw               = impl_GNOME_Applet2_draw;
	klass->epv.save_session       = impl_GNOME_Applet2_save_session;
	klass->epv.change_size        = impl_GNOME_Applet2_change_size;
	klass->epv.change_position    = impl_GNOME_Applet2_change_position;
	klass->epv.freeze_changes     = impl_GNOME_Applet2_freeze_changes;
	klass->epv.thaw_changes       = impl_GNOME_Applet2_thaw_changes;
}

static void
applet_object_init (AppletObject *applet)
{
	applet->priv = g_new0 (AppletObjectPrivate, 1);

	applet->priv->widget     = NULL;
	applet->priv->panel_spot = CORBA_OBJECT_NIL;
	applet->priv->iid        = NULL;
	applet->priv->callbacks  = NULL;
}

AppletObject *
applet_object_new (GtkWidget   *widget,
		   const gchar *iid,
		   guint32     *winid)
{
	AppletObject      *applet;
	GNOME_Panel        panel;
	GNOME_Applet2      applet_obj;
	CORBA_Environment  env;
	CORBA_char        *config_path;
	CORBA_char        *global_config_path;

	CORBA_exception_init (&env);

	applet = g_object_new (APPLET_OBJECT_TYPE, NULL);

	applet->priv->widget = APPLET_WIDGET (gtk_widget_ref (widget));

	applet->priv->iid    = g_strdup (iid);

	panel = applet_object_panel ();

	applet_obj = BONOBO_OBJREF (applet);

	applet->priv->panel_spot = GNOME_Panel_add_applet (panel,
							   applet_obj,
							   iid,
							   &config_path,
							   &global_config_path,
							   winid,
							   &env);

	if (config_path && *config_path)
		applet->priv->widget->privcfgpath = g_strdup (config_path);
	CORBA_free (config_path);

	if (global_config_path && *global_config_path)
		applet->priv->widget->globcfgpath = g_strdup (global_config_path);
	CORBA_free (global_config_path);

	applet->priv->widget->orient = GNOME_PanelSpot__get_parent_orient (
							applet->priv->panel_spot,
							&env);
	if (BONOBO_EX (&env)) {
		CORBA_exception_free (&env);
		CORBA_exception_init (&env);

		applet->priv->widget->orient = ORIENT_UP;
	}

	applet->priv->widget->size = GNOME_PanelSpot__get_parent_size (
							applet->priv->panel_spot,
							&env);
	if (BONOBO_EX (&env))
		applet->priv->widget->size = PIXEL_SIZE_STANDARD;

	CORBA_exception_free (&env);

	return applet;
}

BONOBO_TYPE_FUNC_FULL (AppletObject,
		       GNOME_Applet2,
		       BONOBO_OBJECT_TYPE,
		       applet_object);

