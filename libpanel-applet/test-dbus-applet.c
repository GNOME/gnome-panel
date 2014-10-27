#include <config.h>
#include <string.h>

#include "panel-applet.h"

static void
test_applet_on_do (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
	g_message ("%s called\n", g_action_get_name (G_ACTION (action)));
}

static const GActionEntry test_applet_menu_actions [] = {
	{ "test-applet-do-1", test_applet_on_do, NULL, NULL, NULL },
	{ "test-applet-do-2", test_applet_on_do, NULL, NULL, NULL },
	{ "test-applet-do-3", test_applet_on_do, NULL, NULL, NULL }
};

static const gchar test_applet_menu_xml[] =
	"<section>"
	"  <item>"
	"    <attribute name=\"label\" translatable=\"yes\">Test Item 1</attribute>"
	"    <attribute name=\"action\">test.test-applet-do-1</attribute>"
	"  </item>"
	"  <item>"
	"     <attribute name=\"label\" translatable=\"yes\">Test Item 2</attribute>"
	"     <attribute name=\"action\">test.test-applet-do-2</attribute>"
	"  </item>"
	"  <item>"
	"     <attribute name=\"label\" translatable=\"yes\">Test Item 3</attribute>"
	"     <attribute name=\"action\">test.test-applet-do-3</attribute>"
	"  </item>"
	"</section>";

typedef struct _TestApplet      TestApplet;
typedef struct _TestAppletClass TestAppletClass;

struct _TestApplet {
	PanelApplet   base;
	GtkWidget    *label;
};

struct _TestAppletClass {
	PanelAppletClass   base_class;
};

static GType test_applet_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (TestApplet, test_applet, PANEL_TYPE_APPLET);

static void
test_applet_init (TestApplet *applet)
{
}

static void
test_applet_class_init (TestAppletClass *klass)
{
}

static void
test_applet_handle_orient_change (TestApplet       *applet,
				  PanelAppletOrient orient,
				  gpointer          dummy)
{
        gchar *text;

        text = g_strdup (gtk_label_get_text (GTK_LABEL (applet->label)));

        g_strreverse (text);

        gtk_label_set_text (GTK_LABEL (applet->label), text);

        g_free (text);
}

static void
test_applet_handle_size_change (TestApplet *applet,
				gint        size,
				gpointer    dummy)
{
	switch (size) {
	case 12:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"xx-small\">Hello</span>");
		break;
	case 24:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"x-small\">Hello</span>");
		break;
	case 36:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"small\">Hello</span>");
		break;
	case 48:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"medium\">Hello</span>");
		break;
	case 64:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"large\">Hello</span>");
		break;
	case 80:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"x-large\">Hello</span>");
		break;
	case 128:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"xx-large\">Hello</span>");
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
test_applet_handle_background_change (TestApplet                *applet,
				      cairo_pattern_t           *pattern,
				      gpointer                   dummy)
{
	GdkWindow *window = gtk_widget_get_window (applet->label);

        gdk_window_set_background_pattern (window, pattern);
}

static gboolean
test_applet_fill (TestApplet *applet)
{
	GSimpleActionGroup *action_group;

	applet->label = gtk_label_new (NULL);

	gtk_container_add (GTK_CONTAINER (applet), applet->label);

	gtk_widget_show_all (GTK_WIDGET (applet));

	test_applet_handle_size_change (applet,
					panel_applet_get_size (PANEL_APPLET (applet)),
					NULL);
	test_applet_handle_orient_change (applet,
					  panel_applet_get_orient (PANEL_APPLET (applet)),
					  NULL);

	action_group = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP (action_group),
	                                 test_applet_menu_actions,
	                                 G_N_ELEMENTS (test_applet_menu_actions),
	                                 applet);

	gtk_widget_insert_action_group (GTK_WIDGET (applet), "test",
	                                G_ACTION_GROUP (action_group));

	panel_applet_setup_menu (PANEL_APPLET (applet),
				 test_applet_menu_xml,
				 action_group,
				 GETTEXT_PACKAGE);
	g_object_unref (action_group);

	gtk_widget_set_tooltip_text (GTK_WIDGET (applet), "Hello Tip");

	panel_applet_set_flags (PANEL_APPLET (applet), PANEL_APPLET_HAS_HANDLE);

	g_signal_connect (G_OBJECT (applet),
			  "change_orient",
			  G_CALLBACK (test_applet_handle_orient_change),
			  NULL);

	g_signal_connect (G_OBJECT (applet),
			  "change_size",
			  G_CALLBACK (test_applet_handle_size_change),
			  NULL);

	g_signal_connect (G_OBJECT (applet),
			  "change_background",
			  G_CALLBACK (test_applet_handle_background_change),
			  NULL);

	return TRUE;
}

static gboolean
test_applet_factory (TestApplet  *applet,
		     const gchar *iid,
		     gpointer     data)
{
	gboolean retval = FALSE;

	if (!strcmp (iid, "TestApplet"))
		retval = test_applet_fill (applet);

	return retval;
}


PANEL_APPLET_OUT_PROCESS_FACTORY ("TestAppletFactory",
				  test_applet_get_type (),
				  (PanelAppletFactoryCallback) test_applet_factory,
				  NULL)

