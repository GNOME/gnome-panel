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
#include <libgnomeui/gnome-window-icon.h>
#include <applet-widget.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "print.xpm"

#include "printer.h"

typedef struct _Printer Printer;
struct _Printer {
	GtkWidget *applet;

	GtkWidget *darea;
	guchar *back_rgb;
	int back_w, back_h, back_rs;

	GdkPixmap *printer;
	GdkBitmap *printer_mask;
	int printer_w, printer_h;

	char *print_command;
	char *print_title;

	GtkWidget *prop_name, *prop_command;
	GtkWidget *printer_prop;

	GtkWidget *about;

	int panel_size;
};

enum {
	TARGET_URI_LIST
};

static void
darea_draw(GtkWidget *darea, gpointer must_be_ignored, Printer *pr)
{
	int fontheight, textx, texty, fontwidth;
	GdkGC *gc;

	if(!GTK_WIDGET_REALIZED(darea) ||
	   !GTK_WIDGET_DRAWABLE(darea))
		return;

	if(!pr->back_rgb)
		applet_widget_get_rgb_bg(APPLET_WIDGET(pr->applet),
					 &pr->back_rgb, &pr->back_w,
					 &pr->back_h, &pr->back_rs);
	if(pr->back_rgb) {
		gdk_draw_rgb_image(darea->window, darea->style->white_gc,
				   0, 0, pr->back_w, pr->back_h,
				   GDK_RGB_DITHER_NORMAL,
				   pr->back_rgb, pr->back_rs);
	}

	if(pr->printer) {
		int originx = (pr->panel_size/2)-(pr->printer_w/2);
		gc = gdk_gc_new(darea->window);
		if(pr->printer_mask) {
			gdk_gc_set_clip_mask(gc, pr->printer_mask);
			gdk_gc_set_clip_origin(gc, originx, 0);
		}
		gdk_draw_pixmap(darea->window,
				gc,
				pr->printer,
				0, 0,
				originx, 0,
				pr->printer_w,
				pr->printer_h);
		gdk_gc_destroy(gc); 
	}

	fontheight = gdk_string_height (darea->style->font,
					pr->print_title) + 2;
	fontwidth = gdk_string_width (darea->style->font, pr->print_title) + 2;

	textx = (pr->panel_size - fontwidth) / 2;
	texty = pr->panel_size - fontheight;

	if(pr->printer_h + fontheight > pr->panel_size)
		return;

	gdk_draw_rectangle(darea->window,
			   darea->style->bg_gc[GTK_STATE_NORMAL],
			   TRUE,
			   textx, texty,
			   fontwidth, fontheight);

	gdk_draw_string(darea->window,
			darea->style->font,
			darea->style->fg_gc[GTK_STATE_NORMAL],
			textx+1, texty+fontheight-1,
			pr->print_title);
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

static void
printer_widget (Printer *pr)
{
	static GtkTargetEntry drop_types [] = { 
		{ "text/uri-list", 0, TARGET_URI_LIST }
	};
	static gint n_drop_types = sizeof (drop_types) / sizeof(drop_types[0]);

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
	pr->darea = gtk_drawing_area_new();
	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_signal_connect(GTK_OBJECT(pr->darea),"draw",
			   GTK_SIGNAL_FUNC(darea_draw),
			   pr);
	gtk_signal_connect(GTK_OBJECT(pr->darea),"expose_event",
			   GTK_SIGNAL_FUNC(darea_draw),
			   pr);

	gtk_drag_dest_set (GTK_WIDGET (pr->darea),
			   GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_HIGHLIGHT |
			   GTK_DEST_DEFAULT_DROP,
			   drop_types, n_drop_types,
			   GDK_ACTION_COPY);

	gtk_signal_connect (GTK_OBJECT (pr->darea), "drag_data_received",
			    GTK_SIGNAL_FUNC (drag_data_received), pr);

	gtk_widget_set_usize (pr->darea, pr->panel_size, pr->panel_size);
	gtk_widget_show_all (pr->darea);

	gtk_widget_queue_draw(pr->darea);
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

	gtk_widget_set_usize (pr->darea, pr->panel_size, pr->panel_size);
	gtk_widget_queue_draw(pr->darea);
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
	applet_widget_set_tooltip (APPLET_WIDGET(pr->applet), pr->print_title);
	gtk_widget_queue_draw(pr->darea);
}

static void
close_properties (GtkWidget *w, gpointer data)
{
	Printer *pr = data;
	pr->printer_prop = NULL;
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
help_cb (AppletWidget *w, gpointer data)
{
	GnomeHelpMenuEntry help_entry = { "printer_applet" };/*emb*/
	help_entry.path = data;
	gnome_help_display(NULL, &help_entry);
}

static void
phelp_cb (GtkWidget *w, gint tab, gpointer data)
{
	help_cb (NULL, data);
}

static void
printer_properties (AppletWidget *applet, gpointer data)
{
	GtkWidget *table;
	Printer *pr = data;

	if (pr->printer_prop) {
		gtk_widget_show_now(pr->printer_prop);
		gdk_window_raise(pr->printer_prop->window);
		return;
	}

	pr->printer_prop = gnome_property_box_new ();
	gtk_window_set_wmclass (GTK_WINDOW (pr->printer_prop),
				"printer", "Printer");
	gtk_window_set_title (GTK_WINDOW (pr->printer_prop),
			      _("Printer properties"));
	gnome_window_icon_set_from_file (GTK_WINDOW (pr->printer_prop),
					 GNOME_ICONDIR"/mc/i-printer.png");
	
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
	gtk_signal_connect (GTK_OBJECT (pr->printer_prop), "destroy",
			    GTK_SIGNAL_FUNC(close_properties), pr);
	gtk_signal_connect (GTK_OBJECT (pr->printer_prop), "help",
			    GTK_SIGNAL_FUNC(phelp_cb),
			    "index.html#PRINTER-PROPERTIES");/*emb*/
			    
	gtk_widget_show_all (pr->printer_prop);
}

static void
printer_about (AppletWidget *applet, gpointer data)
{
	Printer *pr = data;
	static const gchar *authors[] =
	{
		"Miguel de Icaza <miguel@kernel.org>",
		NULL
	};

	if (pr->about != NULL) {
		gtk_widget_show_now(pr->about);
		gdk_window_raise(pr->about->window);
		return;
	}
	
	pr->about = gnome_about_new (_("Printer Applet"), "1.0",
				     _("(c) 1998 the Free Software Foundation"),
				     authors,
				     _("The printer applet lets you easily drag files to be printed via a print command"),
				     NULL);
	gtk_window_set_wmclass (GTK_WINDOW (pr->about),
				"printer", "Printer");
	gnome_window_icon_set_from_file (GTK_WINDOW (pr->about),
					 GNOME_ICONDIR"/mc/i-printer.png");
	gtk_signal_connect (GTK_OBJECT(pr->about), "destroy",
			    GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			    &pr->about);
	gtk_widget_show (pr->about);	
}

static void
applet_destroy(GtkWidget *applet, Printer *pr)
{
	if(pr->printer_prop)
		gtk_widget_destroy(pr->printer_prop);
	if(pr->about)
		gtk_widget_destroy(pr->about);
	g_free(pr->back_rgb);
	if(pr->printer)
		gdk_pixmap_unref(pr->printer);
	if(pr->printer_mask)
		gdk_bitmap_unref(pr->printer_mask);
	g_free(pr->print_command);
	g_free(pr->print_title);
	g_free(pr);
}

static void
applet_do_draw(GtkWidget *w, Printer *pr)
{
	g_free(pr->back_rgb);
	pr->back_rgb = NULL;

	gtk_widget_queue_draw(pr->darea);
}

static void
load_printer_xpm(Printer *pr)
{
	GdkPixbuf *pb;

	pb = gdk_pixbuf_new_from_xpm_data ((const char **)print_xpm);

	if(!pb) {
		g_warning("print.xpm is on drugs");
		return;
	}

	pr->printer_w = gdk_pixbuf_get_width(pb);
	pr->printer_h = gdk_pixbuf_get_height(pb);

	gdk_pixbuf_render_pixmap_and_mask(pb, &pr->printer,
					  &pr->printer_mask, 128);

	gdk_pixbuf_unref(pb);
}

GtkWidget *
make_printer_applet(const gchar *goad_id)
{
	Printer *pr;

	pr = g_new0(Printer, 1);
	pr->applet = applet_widget_new(goad_id);
	if (!pr->applet) {
		g_warning(_("Can't create applet!\n"));
		g_free(pr);
		return NULL;
	}

	pr->printer_prop = NULL;

	pr->panel_size =
		applet_widget_get_panel_pixel_size (APPLET_WIDGET(pr->applet));

	gnome_config_push_prefix (APPLET_WIDGET(pr->applet)->privcfgpath);
	pr->print_command =
		gnome_config_get_string ("print/print_command=lpr");
	pr->print_title   = gnome_config_get_string ("print/title=Print");
	gnome_config_pop_prefix ();

	applet_widget_set_tooltip (APPLET_WIDGET (pr->applet), pr->print_title);

	load_printer_xpm(pr);
	
	printer_widget (pr);
	gtk_widget_show (pr->darea);

	gtk_signal_connect(GTK_OBJECT(pr->applet),"do_draw",
			   GTK_SIGNAL_FUNC(applet_do_draw),
			   pr);
	applet_widget_send_draw(APPLET_WIDGET(pr->applet), TRUE);

	gtk_signal_connect(GTK_OBJECT(pr->applet), "save_session",
			   GTK_SIGNAL_FUNC(applet_save_session),
			   pr);
	gtk_signal_connect(GTK_OBJECT(pr->applet), "change_pixel_size",
			   GTK_SIGNAL_FUNC(applet_change_pixel_size),
			   pr);
	gtk_signal_connect(GTK_OBJECT(pr->applet), "destroy",
			   GTK_SIGNAL_FUNC(applet_destroy),
			   pr);
	applet_widget_add (APPLET_WIDGET (pr->applet), pr->darea);
	gtk_widget_show (pr->applet);

	applet_widget_register_stock_callback(APPLET_WIDGET(pr->applet),
					      "properties",
					      GNOME_STOCK_MENU_PROP,
					      _("Properties..."),
					      printer_properties,
					      pr);

	applet_widget_register_stock_callback(APPLET_WIDGET(pr->applet),
					      "help",
					      GNOME_STOCK_PIXMAP_HELP,
					      _("Help"),
					      help_cb, "index.html");/*emb*/

	applet_widget_register_stock_callback(APPLET_WIDGET(pr->applet),
					      "about",
					      GNOME_STOCK_MENU_ABOUT,
					      _("About..."),
					      printer_about,
					      pr);


	return pr->applet;
}

/* as long as the dnd in shlib applets is broke, this must be a separate 
   extern applet, this is now fixed but in case things change this code
   is still here */
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
