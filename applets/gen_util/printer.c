/*
 * GNOME panel printer applet module.
 * (C) 1998 The Free Software Foundation
 *
 * Author: Miguel de Icaza <miguel@kernel.org>
 * Config dialog bits: Federico Mena   <quartic@gimp.org> 
 */

#include <config.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <gnome.h>
#include <applet-widget.h>
#include <gdk_imlib.h>
#include "print.xpm"

#include "printer.h"

typedef struct _Printer Printer;
struct _Printer {
	GtkWidget *applet;
	GtkWidget *printer;
	GtkWidget *prop_name, *prop_command;
	GtkWidget *ev;
	GtkWidget *pixmap;
	GtkWidget *label;
	GtkWidget *printer_prop;

	char *print_command;
	char *print_title;

	int panel_size;
};

enum {
  TARGET_URI_LIST
};

static void
position_label (GtkWidget *label, Printer *pr)
{
	int height;
	int x,y;

	height = (label->style->font->ascent +
		  label->style->font->descent + 2);
	x = (pr->panel_size - gdk_string_width (label->style->font,
						GTK_LABEL(label)->label)) / 2;
	y = pr->panel_size - height;
	gtk_fixed_move (GTK_FIXED (pr->printer), pr->ev, x, y);
}

static void
position_pixmap (Printer *pr)
{
	gtk_fixed_move (GTK_FIXED (pr->printer), pr->pixmap, 
			(pr->panel_size/2)-(48/2), 0);
}

static void
print_file (gchar *name, Printer *pr)
{
	char *argv[3];
	argv[0] = pr->print_command;
	argv[1] = name;
	argv[2] = NULL;
	if(gnome_execute_async(NULL, 2, argv)<0)
		gnome_error_dialog(_("Can't execute printer command"));
}

static void
drag_data_received (GtkWidget        *widget,
		    GdkDragContext   *context,
		    gint              x,
		    gint              y,
		    GtkSelectionData *selection_data,
		    guint             info,
		    guint             time,
		    gpointer          data)
{
	Printer *pr = data;
	GList *names;

	switch (info)
	  {
	  case TARGET_URI_LIST:
		  names = gnome_uri_list_extract_filenames ((char *)selection_data->data);
		  g_list_foreach (names, (GFunc)print_file, pr);
		  gnome_uri_list_free_strings (names);
		  break;
	  }
}

static GtkWidget *
printer_widget (Printer *pr)
{
	static GtkTargetEntry drop_types [] = { 
		{ "text/uri-list", 0, TARGET_URI_LIST }
	};
	static gint n_drop_types = sizeof (drop_types) / sizeof(drop_types[0]);
	
	pr->printer   = gtk_fixed_new ();
	pr->pixmap = gnome_pixmap_new_from_xpm_d (print_xpm);
	pr->label   = gtk_label_new (pr->print_title);
	/*this will make a grey background so that we can always read
	  the printer label*/
	pr->ev = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(pr->ev),pr->label);

	gtk_fixed_put (GTK_FIXED (pr->printer), pr->pixmap, 0, 0);
	gtk_fixed_put (GTK_FIXED (pr->printer), pr->ev, 0, 0);
	gtk_signal_connect (GTK_OBJECT (pr->label), "realize",
			    GTK_SIGNAL_FUNC (position_label), pr);

	gtk_drag_dest_set (GTK_WIDGET (pr->printer),
			   GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_HIGHLIGHT |
			   GTK_DEST_DEFAULT_DROP,
			   drop_types, n_drop_types,
			   GDK_ACTION_COPY);

	gtk_signal_connect (GTK_OBJECT (pr->printer), "drag_data_received",
			    GTK_SIGNAL_FUNC (drag_data_received), pr);
			    

	gtk_widget_set_usize (pr->printer, pr->panel_size, pr->panel_size);
	gtk_widget_show_all (pr->printer);
	return pr->printer;
}

static void
applet_set_default_back(Printer *pr)
{
	g_return_if_fail (pr != NULL);
	g_return_if_fail (pr->printer != NULL);

	gtk_widget_set_rc_style(pr->printer);
	gtk_widget_queue_draw(GTK_WIDGET(pr->printer));
}

static void
applet_set_back_color(Printer *pr, GdkColor *color)
{
	GtkStyle *ns;

	ns = gtk_style_copy(pr->printer->style);
	gtk_style_ref(ns);

	ns->bg[GTK_STATE_NORMAL] = *color;
	ns->bg[GTK_STATE_NORMAL].pixel = 1; /* bogus */

	if(ns->bg_pixmap[GTK_STATE_NORMAL]) {
		gdk_imlib_free_pixmap(ns->bg_pixmap[GTK_STATE_NORMAL]);
		ns->bg_pixmap[GTK_STATE_NORMAL] = NULL;
	}

	gtk_widget_set_style(pr->printer, ns);

	gtk_style_unref(ns);

	gtk_widget_queue_draw(GTK_WIDGET(pr->printer));
}

static void
applet_set_back_pixmap(Printer *pr, gchar *pixmap)
{
	GdkImlibImage *im;
	GdkPixmap *p;
	GtkStyle *ns;

	if(!pixmap || strcmp(pixmap,"")==0) {
		ns = gtk_style_copy(pr->printer->style);
		gtk_style_ref(ns);

		p = ns->bg_pixmap[GTK_STATE_NORMAL];
		if(p)
			gdk_imlib_free_pixmap (p);
		ns->bg_pixmap[GTK_STATE_NORMAL] = NULL;

		gtk_widget_set_style(pr->printer, ns);

		gtk_style_unref(ns);
		return;
	}

	if (!g_file_exists (pixmap))
		return;

	im = gdk_imlib_load_image (pixmap);
	if (!im)
		return;

	gdk_imlib_render (im, im->rgb_width, im->rgb_height);

	p = gdk_imlib_move_image (im);

	ns = gtk_style_copy(pr->printer->style);
	gtk_style_ref(ns);

	if(ns->bg_pixmap[GTK_STATE_NORMAL])
		gdk_imlib_free_pixmap (ns->bg_pixmap[GTK_STATE_NORMAL]);
	ns->bg_pixmap[GTK_STATE_NORMAL] = p;

	gtk_widget_set_style(pr->printer, ns);

	gtk_style_unref(ns);
	gdk_imlib_destroy_image (im);
}

static void
applet_back_change(GtkWidget *w,
		   PanelBackType type,
		   gchar *pixmap,
		   GdkColor *color,
		   gpointer data)
{
	Printer *pr = data;

	/* somewhat of a hack, this will actually wait until the widget
	   is fully realized and mapped before applying the changes */
	if(!GTK_WIDGET_REALIZED(w))
		gtk_widget_show_now(w);

	if(type == PANEL_BACK_PIXMAP)
		applet_set_back_pixmap(pr,pixmap);
	else if(type == PANEL_BACK_COLOR)
		applet_set_back_color(pr,color);
	else
		applet_set_default_back(pr);
}

static void
applet_change_pixel_size(GtkWidget *w,
			 int size,
			 gpointer data)
{
	Printer *pr = data;

	if(size>48)
		pr->panel_size = 48;
	else
		pr->panel_size = size;
	gtk_widget_set_usize (pr->printer, pr->panel_size, pr->panel_size);
	position_label (pr->label, pr);
	position_pixmap (pr);
	if(size<32)
		gtk_widget_hide(pr->ev);
	else
		gtk_widget_show(pr->ev);
}

static gint
applet_save_session(GtkWidget *w,
		    const char *privcfgpath,
		    const char *globcfgpath,
		    gpointer data)
{
	Printer *pr = data;

	gnome_config_push_prefix (privcfgpath);
	gnome_config_set_string ("print/print_command",
				 pr->print_command ? pr->print_command : "");
	gnome_config_set_string ("print/title",
				 pr->print_title ? pr->print_title : "");
	gnome_config_pop_prefix ();
	gnome_config_sync ();
	gnome_config_drop_all ();

	return FALSE;
}

static void
changed (GtkWidget *w, gpointer data)
{
	Printer *pr = data;
	gnome_property_box_changed (GNOME_PROPERTY_BOX (pr->printer_prop));
}

static void
apply_one (GtkWidget *widget, char **dest)
{
	g_free (*dest);

	*dest = gtk_entry_get_text (GTK_ENTRY (widget));
	*dest = g_strdup (*dest);
}

static void
apply_properties (GtkWidget *widget, gint button_num, gpointer data)
{
	Printer *pr = data;
	apply_one (pr->prop_name, &pr->print_title);
	apply_one (pr->prop_command, &pr->print_command);
	gtk_label_set_text (GTK_LABEL (pr->label), pr->print_title);
	applet_widget_set_tooltip (APPLET_WIDGET(pr->applet), pr->print_title);
	position_label (pr->label, pr);
}

static int
close_properties (GtkWidget *w, gpointer data)
{
	Printer *pr = data;
	pr->printer_prop = 0;
	return FALSE;
}

static void
build_label_and_entry (Printer *pr, GtkTable *table, int row, char *label,
		       char *gentry_id, GtkWidget **widget,
		       char *text, char *default_text)
{
	GtkWidget *w;

	w = gtk_label_new (label);
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), w,
			  0, 1, row, row + 1,
			  GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);

	*widget = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (*widget), text ? text : default_text);
	gtk_signal_connect (GTK_OBJECT (*widget), "changed",
			    GTK_SIGNAL_FUNC (changed), pr);
	gtk_table_attach (GTK_TABLE (table), *widget,
			  1, 2, row, row + 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
}

static void
printer_properties (AppletWidget *applet, gpointer data)
{
        static GnomeHelpMenuEntry help_entry = { NULL,
						 "properties-printer" };
	GtkWidget *table;
	Printer *pr = data;

	help_entry.name = gnome_app_id;

	if (pr->printer_prop) {
		gtk_widget_show_now(pr->printer_prop);
		gdk_window_raise(pr->printer_prop->window);
		return;
	}

	pr->printer_prop = gnome_property_box_new ();
	gtk_window_set_title (GTK_WINDOW (pr->printer_prop),
			      _("Printer properties"));
	
	table = gtk_table_new (2, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), GNOME_PAD);
	gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
	gtk_table_set_col_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);

	build_label_and_entry (pr,GTK_TABLE (table), 0, _("Printer name:"),
			       "printer_name", &pr->prop_name,
			       pr->print_title, "");

	build_label_and_entry (pr,GTK_TABLE (table), 1, _("Print command:"),
			       "printer_command", &pr->prop_command,
			       pr->print_command, "lpr");

	gnome_property_box_append_page (GNOME_PROPERTY_BOX (pr->printer_prop),
					table,
					gtk_label_new (_("Printer")));
	gtk_signal_connect (GTK_OBJECT (pr->printer_prop), "apply",
			    GTK_SIGNAL_FUNC(apply_properties), pr);
	gtk_signal_connect (GTK_OBJECT (pr->printer_prop), "delete_event",
			    GTK_SIGNAL_FUNC(close_properties), pr);
	gtk_signal_connect (GTK_OBJECT (pr->printer_prop), "destroy",
			    GTK_SIGNAL_FUNC(close_properties), pr);
	gtk_signal_connect (GTK_OBJECT (pr->printer_prop), "help",
			    GTK_SIGNAL_FUNC(gnome_help_pbox_display),
			    &help_entry);
			    
	gtk_widget_show_all (pr->printer_prop);
}

static void
printer_about (AppletWidget *applet, gpointer data)
{
	static GtkWidget   *about     = NULL;
	static const gchar *authors[] =
	{
		"Miguel de Icaza <miguel@kernel.org>",
		NULL
	};

	if (about != NULL)
	{
		gdk_window_show(about->window);
		gdk_window_raise(about->window);
		return;
	}
	
	about = gnome_about_new (_("Printer Applet"), "1.0",
				 _("(c) 1998 the Free Software Foundation"),
				 authors,
				 _("The printer applet lets you easily drag files to be printed via a print command"),
				 NULL);
	gtk_signal_connect (GTK_OBJECT(about), "destroy",
			    GTK_SIGNAL_FUNC(gtk_widget_destroyed), &about);
	gtk_widget_show (about);	
}

static void
applet_destroy(GtkWidget *applet, Printer *pr)
{
	g_free(pr->print_command);
	g_free(pr->print_title);
	g_free(pr);
}

GtkWidget *
make_printer_applet(const gchar *goad_id)
{
	Printer *pr;

	pr = g_new0(Printer,1);
	pr->applet = applet_widget_new(goad_id);
	if (!pr->applet)
		g_error(_("Can't create applet!\n"));

	pr->printer_prop = NULL;

	pr->panel_size =
		applet_widget_get_panel_pixel_size (APPLET_WIDGET(pr->applet));

	gnome_config_push_prefix (APPLET_WIDGET(pr->applet)->privcfgpath);
	pr->print_command =
		gnome_config_get_string ("print/print_command=lpr");
	pr->print_title   = gnome_config_get_string ("print/title=Print");
	gnome_config_pop_prefix ();

	applet_widget_set_tooltip (APPLET_WIDGET (pr->applet), pr->print_title);
	
	pr->printer = printer_widget (pr);
	gtk_widget_show (pr->printer);
	gtk_signal_connect(GTK_OBJECT(pr->applet),"back_change",
			   GTK_SIGNAL_FUNC(applet_back_change),
			   pr);
	gtk_signal_connect(GTK_OBJECT(pr->applet),"save_session",
			   GTK_SIGNAL_FUNC(applet_save_session),
			   pr);
	gtk_signal_connect(GTK_OBJECT(pr->applet),"change_pixel_size",
			   GTK_SIGNAL_FUNC(applet_change_pixel_size),
			   pr);
	gtk_signal_connect(GTK_OBJECT(pr->applet),"destroy",
			   GTK_SIGNAL_FUNC(applet_destroy),
			   pr);
	applet_widget_add (APPLET_WIDGET (pr->applet), pr->printer);
	gtk_widget_show (pr->applet);

	applet_widget_register_stock_callback(APPLET_WIDGET(pr->applet),
					      "about",
					      GNOME_STOCK_MENU_ABOUT,
					      _("About..."),
					      printer_about,
					      NULL);

	applet_widget_register_stock_callback(APPLET_WIDGET(pr->applet),
					      "properties",
					      GNOME_STOCK_MENU_PROP,
					      _("Properties..."),
					      printer_properties,
					      pr);

	return pr->applet;
}

/* as long as the dnd in shlib applets is broke, this must be a separate 
   extern applet */
#if 0

/*when we get a command to start a new widget*/
static GtkWidget *
applet_start_new_applet(const gchar *goad_id, const char **params, int nparams)
{
	/* return make_new_applet(goad_id);*/
	return make_printer_applet(goad_id);
}

int
main(int argc, char **argv)
{
	const gchar *goad_id;

	/*this is needed for printer applet*/
	struct sigaction sa;

	sa.sa_handler = SIG_IGN;
	sa.sa_flags   = SA_NOCLDSTOP;
	sigemptyset (&sa.sa_mask);
	sigaction (SIGCHLD, &sa, NULL);

	/* Initialize the i18n stuff */
        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
	
	applet_widget_init("gen_util_printer", VERSION, argc, argv,
			   NULL, 0, NULL);
	applet_factory_new("gen_util_printer_factory", NULL, applet_start_new_applet);

	goad_id = goad_server_activation_id();
	if(!goad_id)
	  goad_id = "gen_util_printer"; /* badhack */
	if(strcmp(goad_id, "gen_util_printer_factory")!=0) /* Only do if factory wasn't requested (odd) */
	  make_printer_applet(goad_id);

	applet_widget_gtk_main();

	return 0;
}

#endif
