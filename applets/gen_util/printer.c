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
#include "applet-lib.h"
#include "applet-widget.h"
#include <gdk_imlib.h>
#include "print.xpm"

typedef struct _Printer Printer;
struct _Printer {
	GtkWidget *printer;
	GtkWidget *prop_name, *prop_command;
	GtkWidget *ev;
	GtkWidget *label;
	GtkWidget *printer_prop;

	char *print_command;
	char *print_title;
};

enum {
  TARGET_URI_LIST,
};

void
position_label (GtkWidget *label, gpointer data)
{
	int height;
	GtkWidget *ev = data;
	GtkWidget *parent = ev->parent;
	
	height = (label->style->font->ascent +
		  label->style->font->descent + 2);
	gtk_fixed_move (GTK_FIXED (parent), ev,
		       (48 - gdk_string_width (label->style->font,
					       GTK_LABEL(label)->label)) / 2,
		        48 - height);
}

static void
execute (char *command)
{
	pid_t pid;

	pid = fork ();
        if (pid == 0){
		int i;

		for (i = 0; i < 255; i++)
			close (i);
		execl ("/bin/sh", "/bin/sh", "-c", command, NULL);
		
		_exit (127);
	}
}

static void
print_file (gchar *name, Printer *pr)
{
	gchar *str = g_copy_strings (pr->print_command, " ", name, NULL);
	execute (str);
	g_free (str);
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
	gchar *str;
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

GtkWidget *
printer_widget (Printer *pr)
{
	GtkWidget *fixed;
	GtkWidget *printer;
	int height;
	static GtkTargetEntry drop_types [] = { 
		{ "text/uri-list", 0, TARGET_URI_LIST },
	};
	static gint n_drop_types = sizeof (drop_types) / sizeof(drop_types[0]);
	
	fixed   = gtk_fixed_new ();
	printer = gnome_pixmap_new_from_xpm_d (print_xpm);
	pr->label   = gtk_label_new (pr->print_title);
	/*this will make a grey background so that we can always read
	  the printer label*/
	pr->ev = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(pr->ev),pr->label);

	gtk_fixed_put (GTK_FIXED (fixed), printer, 0, 0);
	gtk_fixed_put (GTK_FIXED (fixed), pr->ev, 0, 0);
	gtk_signal_connect (GTK_OBJECT (pr->label), "realize",
			    GTK_SIGNAL_FUNC (position_label), pr->ev);

	gtk_drag_dest_set (GTK_WIDGET (fixed),
			   GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_HIGHLIGHT |
			   GTK_DEST_DEFAULT_DROP,
			   drop_types, n_drop_types,
			   GDK_ACTION_COPY);

	gtk_signal_connect (GTK_OBJECT (fixed), "drag_data_received",
			    GTK_SIGNAL_FUNC (drag_data_received), pr);
			    

	gtk_widget_set_usize (fixed, 48, 48);
	gtk_widget_show_all (fixed);
	return fixed;
}

static void
applet_set_default_back(Printer *pr)
{
	GtkStyle *ns;

	ns = gtk_style_new();

	gtk_style_ref(ns);
	gtk_widget_set_style(pr->printer, ns);
	gtk_style_unref(ns);

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
	if(type == PANEL_BACK_PIXMAP)
		applet_set_back_pixmap(pr,pixmap);
	else if(type == PANEL_BACK_COLOR)
		applet_set_back_color(pr,color);
	else
		applet_set_default_back(pr);
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
	if (*dest)
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
	gtk_label_set (GTK_LABEL (pr->label), pr->print_title);
	position_label (pr->label, pr->ev);
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
	gtk_widget_show (w);

	*widget = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (*widget), text ? text : default_text);
	gtk_signal_connect (GTK_OBJECT (*widget), "changed",
			    GTK_SIGNAL_FUNC (changed), pr);
	gtk_table_attach (GTK_TABLE (table), *widget,
			  1, 2, row, row + 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (*widget);
}

static void
printer_properties (AppletWidget *applet, gpointer data)
{
	GtkWidget *table;
	Printer *pr = data;

	if (pr->printer_prop) {
		gdk_window_raise(pr->printer_prop->window);
		return;
	}

	pr->printer_prop = gnome_property_box_new ();
	gtk_window_set_title (GTK_WINDOW (pr->printer_prop),
			      _("Printer properties"));
	
	table = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (table);
	gtk_container_border_width (GTK_CONTAINER (table), GNOME_PAD);
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
			    
	gtk_widget_show (pr->printer_prop);
}

GtkWidget *
make_printer_applet(const gchar *param)
{
	GtkWidget *applet;
	Printer *pr;

	applet = applet_widget_new_with_param (param);
	if (!applet)
		g_error(_("Can't create applet!\n"));

	pr = g_new(Printer,1);
	pr->printer_prop = NULL;

	gnome_config_push_prefix (APPLET_WIDGET(applet)->privcfgpath);
	pr->print_command =
		gnome_config_get_string ("print/print_command=lpr");
	pr->print_title   = gnome_config_get_string ("print/title=Print");
	gnome_config_pop_prefix ();
	
	pr->printer = printer_widget (pr);
	gtk_widget_show (pr->printer);
	gtk_signal_connect(GTK_OBJECT(applet),"back_change",
			   GTK_SIGNAL_FUNC(applet_back_change),
			   pr);
	gtk_signal_connect(GTK_OBJECT(applet),"save_session",
			   GTK_SIGNAL_FUNC(applet_save_session),
			   pr);
	applet_widget_add (APPLET_WIDGET (applet), pr->printer);
	gtk_widget_show (applet);

	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "properties",
					      GNOME_STOCK_MENU_PROP,
					      _("Properties..."),
					      printer_properties,
					      pr);
	return applet;
}
