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
	GtkWidget *label;
	GtkWidget *printer_prop;

	char *print_command;
	char *print_title;
};

void
position_label (GtkWidget *label, gpointer data)
{
	int height;
	GtkWidget *parent = label->parent;
	
	height = (label->style->font->ascent +
		  label->style->font->descent + 2);
	gtk_fixed_move (GTK_FIXED (parent), label,
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
drop_data_available (GtkWidget *widget,
		     GdkEventDropDataAvailable *event,
		     gpointer data)
{
	Printer *pr = data;
	char *p = event->data;
	int count = event->data_numbytes;
	int len, items;
	char *str;

	p = event->data;
	count = event->data_numbytes;
	do {
		len = 1 + strlen (p);
		count -= len;

		str = g_copy_strings (pr->print_command, " ", p, NULL);
		execute (str);
		g_free (str);
		
		p += len;
		items++;
	} while (count > 0);
}

static void
configure_dnd (GtkWidget *w, gpointer data)
{
	Printer *pr = data;
	char *drop_types [] = { "url:ALL" };

	gtk_widget_dnd_drop_set (w, 1, drop_types, 1, FALSE);
	gtk_signal_connect (GTK_OBJECT (w), "drop_data_available_event",
			    GTK_SIGNAL_FUNC (drop_data_available), pr);
}

GtkWidget *
printer_widget (Printer *pr)
{
	GtkWidget *fixed;
	GtkWidget *printer;
	int height;
	
	fixed   = gtk_fixed_new ();
	printer = gnome_pixmap_new_from_xpm_d (print_xpm);
	pr->label   = gtk_label_new (pr->print_title);

	gtk_fixed_put (GTK_FIXED (fixed), printer, 0, 0);
	gtk_fixed_put (GTK_FIXED (fixed), pr->label, 0, 0);
	gtk_signal_connect (GTK_OBJECT (pr->label), "realize",
			    GTK_SIGNAL_FUNC (position_label), NULL);
	gtk_signal_connect (GTK_OBJECT (printer), "realize",
			    GTK_SIGNAL_FUNC (configure_dnd), pr);
	gtk_signal_connect (GTK_OBJECT (fixed), "realize",
			    GTK_SIGNAL_FUNC (configure_dnd), pr);
	gtk_widget_set_usize (fixed, 48, 48);
	gtk_widget_show_all (fixed);
	return fixed;
}

static gint
applet_session_save(GtkWidget *w,
		    const char *cfgpath,
		    const char *globcfgpath,
		    gpointer data)
{
	Printer *pr = data;

	gnome_config_push_prefix (cfgpath);
	gnome_config_set_string ("print_command", pr->print_command ? pr->print_command : "");
	gnome_config_set_string ("title", pr->print_title ? pr->print_title : "");
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
	position_label (pr->label, pr->label);
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

	gnome_property_box_append_page (GNOME_PROPERTY_BOX (pr->printer_prop), table,
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
		g_error("Can't create applet!\n");

	pr = g_new(Printer,1);
	pr->printer_prop = NULL;

	if(APPLET_WIDGET(applet)->cfgpath && *(APPLET_WIDGET(applet)->cfgpath)) {
		gnome_config_push_prefix (APPLET_WIDGET(applet)->cfgpath);
		pr->print_command = gnome_config_get_string ("print_command=lpr");
		pr->print_title   = gnome_config_get_string ("title=Print");
		gnome_config_pop_prefix ();
	} else {
		pr->print_title   = g_strdup ("Print");
		pr->print_command = g_strdup ("lpr");
	}
	
	pr->printer = printer_widget (pr);
	gtk_widget_show (pr->printer);
	applet_widget_add (APPLET_WIDGET (applet), pr->printer);
	gtk_widget_show (applet);
	gtk_signal_connect(GTK_OBJECT(applet),"session_save",
			   GTK_SIGNAL_FUNC(applet_session_save),
			   pr);

	applet_widget_register_callback(APPLET_WIDGET(applet),
					"properties",
					_("Properties"),
					printer_properties,
					pr);
	return applet;
}
