#include <config.h>
#include <string.h>
#include <glib.h>
#include <fcntl.h>
#include <dirent.h>

#include "panel-include.h"

/*this is used to do an immediate move instead of set_uposition, which
queues one*/
void
move_window(GtkWidget *widget, int x, int y)
{
	gdk_window_set_hints(widget->window, x, y, 0, 0, 0, 0, GDK_HINT_POS);
	gdk_window_move(widget->window, x, y);
	/* FIXME: this should draw only the newly exposed area! */
	gtk_widget_draw(widget, NULL);
}



int
string_is_in_list(GSList *list,char *text)
{
	for(;list!=NULL;list=g_slist_next(list))
		if(strcmp(text,list->data)==0)
			return TRUE;
	return FALSE;
}


/*FIXME: use gnome_entry*/
GtkWidget *
create_text_entry(GtkWidget *table,
		  char *history_id,
		  int row,
		  char *label,
		  char *text,
		  GtkWidget *w)
{
	GtkWidget *wlabel;
	GtkWidget *entry;

	wlabel = gtk_label_new(label);
	gtk_misc_set_alignment(GTK_MISC(wlabel), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), wlabel,
			 0, 1, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);
	gtk_widget_show(wlabel);

	entry = gtk_entry_new();
	if (text)
		gtk_entry_set_text(GTK_ENTRY(entry), text);
	gtk_table_attach(GTK_TABLE(table), entry,
			 1, 2, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);

	gtk_signal_connect_object_while_alive (GTK_OBJECT (entry), "changed",
					       GTK_SIGNAL_FUNC(gnome_property_box_changed), 
					       GTK_OBJECT(w));
	return entry;
}

GtkWidget *
create_pixmap_entry(GtkWidget *table,
		    char *history_id,
		    int row,
		    char *label,
		    char *text,
		    GtkWidget *w,
		    int pw, int ph /*preview size*/)
{
	GtkWidget *wlabel;
	GtkWidget *entry;
	GtkWidget *t;

	wlabel = gtk_label_new(label);
	gtk_misc_set_alignment(GTK_MISC(wlabel), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), wlabel,
			 0, 1, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);
	gtk_widget_show(wlabel);

	entry = gnome_pixmap_entry_new(history_id,_("Browse"),TRUE);
	gnome_pixmap_entry_set_preview_size(GNOME_PIXMAP_ENTRY(entry),pw,ph);
	t = gnome_pixmap_entry_gtk_entry (GNOME_PIXMAP_ENTRY (entry));
	if (text)
		gtk_entry_set_text(GTK_ENTRY(t), text);
	gtk_table_attach(GTK_TABLE(table), entry,
			 1, 2, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);

	gtk_signal_connect_object_while_alive (GTK_OBJECT (t), "changed",
					       GTK_SIGNAL_FUNC(gnome_property_box_changed), 
					       GTK_OBJECT(w));
	return entry;
}

GtkWidget *
create_icon_entry(GtkWidget *table,
		  char *history_id,
		  int row,
		  char *label,
		  char *text,
		  GtkWidget *w)
{
	GtkWidget *wlabel;
	GtkWidget *entry;
	GtkWidget *t;

	wlabel = gtk_label_new(label);
	gtk_misc_set_alignment(GTK_MISC(wlabel), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), wlabel,
			 0, 1, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);
	gtk_widget_show(wlabel);

	entry = gnome_icon_entry_new(history_id,_("Browse"));
	t = gnome_icon_entry_gtk_entry (GNOME_ICON_ENTRY (entry));
	if (text)
		gtk_entry_set_text(GTK_ENTRY(t), text);
	gtk_table_attach(GTK_TABLE(table), entry,
			 1, 2, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);

	gtk_signal_connect_object_while_alive (GTK_OBJECT (t), "changed",
					       GTK_SIGNAL_FUNC(gnome_property_box_changed), 
					       GTK_OBJECT(w));
	return entry;
}

GtkWidget *
create_file_entry(GtkWidget *table,
		  char *history_id,
		  int row,
		  char *label,
		  char *text,
		  GtkWidget *w)
{
	GtkWidget *wlabel;
	GtkWidget *entry;
	GtkWidget *t;

	wlabel = gtk_label_new(label);
	gtk_misc_set_alignment(GTK_MISC(wlabel), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), wlabel,
			 0, 1, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);
	gtk_widget_show(wlabel);

	entry = gnome_file_entry_new(history_id,_("Browse"));
	t = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (entry));
	if (text)
		gtk_entry_set_text(GTK_ENTRY(t), text);
	gtk_table_attach(GTK_TABLE(table), entry,
			 1, 2, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);

	gtk_signal_connect_object_while_alive (GTK_OBJECT (t), "changed",
					       GTK_SIGNAL_FUNC(gnome_property_box_changed), 
					       GTK_OBJECT(w));
	return entry;
}


GList *
my_g_list_swap_next(GList *list, GList *dl)
{
	GList *t;

	if(!dl->next)
		return list;
	if(dl->prev)
		dl->prev->next = dl->next;
	t = dl->prev;
	dl->prev = dl->next;
	dl->next->prev = t;
	if(dl->next->next)
		dl->next->next->prev = dl;
	t = dl->next->next;
	dl->next->next = dl;
	dl->next = t;

	if(list == dl)
		return dl->prev;
	return list;
}

GList *
my_g_list_swap_prev(GList *list, GList *dl)
{
	GList *t;

	if(!dl->prev)
		return list;
	if(dl->next)
		dl->next->prev = dl->prev;
	t = dl->next;
	dl->next = dl->prev;
	dl->prev->next = t;
	if(dl->prev->prev)
		dl->prev->prev->next = dl;
	t = dl->prev->prev;
	dl->prev->prev = dl;
	dl->prev = t;

	if(list == dl->next)
		return dl;
	return list;
}

/*maybe this should be a glib function?
 it resorts a single item in the list*/
GList *
my_g_list_resort_item(GList *list, gpointer data, GCompareFunc func)
{
	GList *dl;

	if(!list)
		return NULL;

	dl = g_list_find(list,data);

	g_return_val_if_fail(dl!=NULL,list);

	while(dl->next &&
	      (*func)(dl->data,dl->next->data)>0)
		list=my_g_list_swap_next(list,dl);
	while(dl->prev &&
	      (*func)(dl->data,dl->prev->data)<0)
		list=my_g_list_swap_prev(list,dl);
	return list;
}

/*this is used to do an immediate move instead of set_uposition, which
queues one*/
void
move_resize_window(GtkWidget *widget, int x, int y, int w, int h)
{
	/*printf("%d x %d x %d x %d\n",x,y,w,h);*/
	gdk_window_set_hints(widget->window, x, y, w, h, w, h,
			     GDK_HINT_POS|GDK_HINT_MIN_SIZE|GDK_HINT_MAX_SIZE);
	gdk_window_move_resize(widget->window, x, y, w, h);
	/* FIXME: this should draw only the newly exposed area! */
	gtk_widget_draw(widget, NULL);
}

/*this is used to do an immediate resize instead of set_usize, which
queues one*/
void
resize_window(GtkWidget *widget, int w, int h)
{
	/*printf("%d x %d x %d x %d\n",x,y,w,h);*/
	gdk_window_set_hints(widget->window, 0, 0, w, h, w, h,
			     GDK_HINT_MIN_SIZE|GDK_HINT_MAX_SIZE);
	gdk_window_resize(widget->window, w, h);
	/* FIXME: this should draw only the newly exposed area! */
	gtk_widget_draw(widget, NULL);
}

/*following code shamelessly stolen from gtk*/
static void
rgb_to_hls (gdouble *r,
	    gdouble *g,
	    gdouble *b)
{
  gdouble min;
  gdouble max;
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble h, l, s;
  gdouble delta;

  red = *r;
  green = *g;
  blue = *b;

  if (red > green)
    {
      if (red > blue)
	max = red;
      else
	max = blue;

      if (green < blue)
	min = green;
      else
	min = blue;
    }
  else
    {
      if (green > blue)
	max = green;
      else
	max = blue;

      if (red < blue)
	min = red;
      else
	min = blue;
    }

  l = (max + min) / 2;
  s = 0;
  h = 0;

  if (max != min)
    {
      if (l <= 0.5)
	s = (max - min) / (max + min);
      else
	s = (max - min) / (2 - max - min);

      delta = max -min;
      if (red == max)
	h = (green - blue) / delta;
      else if (green == max)
	h = 2 + (blue - red) / delta;
      else if (blue == max)
	h = 4 + (red - green) / delta;

      h *= 60;
      if (h < 0.0)
	h += 360;
    }

  *r = h;
  *g = l;
  *b = s;
}

static void
hls_to_rgb (gdouble *h,
	    gdouble *l,
	    gdouble *s)
{
  gdouble hue;
  gdouble lightness;
  gdouble saturation;
  gdouble m1, m2;
  gdouble r, g, b;

  lightness = *l;
  saturation = *s;

  if (lightness <= 0.5)
    m2 = lightness * (1 + saturation);
  else
    m2 = lightness + saturation - lightness * saturation;
  m1 = 2 * lightness - m2;

  if (saturation == 0)
    {
      *h = lightness;
      *l = lightness;
      *s = lightness;
    }
  else
    {
      hue = *h + 120;
      while (hue > 360)
	hue -= 360;
      while (hue < 0)
	hue += 360;

      if (hue < 60)
	r = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
	r = m2;
      else if (hue < 240)
	r = m1 + (m2 - m1) * (240 - hue) / 60;
      else
	r = m1;

      hue = *h;
      while (hue > 360)
	hue -= 360;
      while (hue < 0)
	hue += 360;

      if (hue < 60)
	g = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
	g = m2;
      else if (hue < 240)
	g = m1 + (m2 - m1) * (240 - hue) / 60;
      else
	g = m1;

      hue = *h - 120;
      while (hue > 360)
	hue -= 360;
      while (hue < 0)
	hue += 360;

      if (hue < 60)
	b = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
	b = m2;
      else if (hue < 240)
	b = m1 + (m2 - m1) * (240 - hue) / 60;
      else
	b = m1;

      *h = r;
      *l = g;
      *s = b;
    }
}

static void
gtk_style_shade (GdkColor *a,
		 GdkColor *b,
		 gdouble   k)
{
  gdouble red;
  gdouble green;
  gdouble blue;

  red = (gdouble) a->red / 65535.0;
  green = (gdouble) a->green / 65535.0;
  blue = (gdouble) a->blue / 65535.0;

  rgb_to_hls (&red, &green, &blue);

  green *= k;
  if (green > 1.0)
    green = 1.0;
  else if (green < 0.0)
    green = 0.0;

  blue *= k;
  if (blue > 1.0)
    blue = 1.0;
  else if (blue < 0.0)
    blue = 0.0;

  hls_to_rgb (&red, &green, &blue);

  b->red = red * 65535.0;
  b->green = green * 65535.0;
  b->blue = blue * 65535.0;
}

#define LIGHTNESS_MULT  1.3
#define DARKNESS_MULT   0.7

void
set_frame_colors(PanelWidget *panel, GtkWidget *frame,
		 GtkWidget *but1, GtkWidget *but2, GtkWidget *but3, GtkWidget *but4)
{
	if(panel->back_type == PANEL_BACK_COLOR) {
		GtkStyle *ns;
		int i;

		ns = gtk_style_copy(GTK_WIDGET(panel)->style);
		gtk_style_ref(ns);

		ns->bg[GTK_STATE_NORMAL] =
			panel->back_color;
		gtk_style_shade (&panel->back_color,
				 &ns->bg[GTK_STATE_PRELIGHT],1.5);
		gtk_style_shade (&panel->back_color,
				 &ns->bg[GTK_STATE_ACTIVE],0.8);
		ns->bg[GTK_STATE_INSENSITIVE] = 
			panel->back_color;

		for (i = 0; i < 5; i++) {
			gtk_style_shade (&ns->bg[i], &ns->light[i], LIGHTNESS_MULT);
			gtk_style_shade (&ns->bg[i], &ns->dark[i], DARKNESS_MULT);

			ns->mid[i].red = (ns->light[i].red + ns->dark[i].red) / 2;
			ns->mid[i].green = (ns->light[i].green + ns->dark[i].green) / 2;
			ns->mid[i].blue = (ns->light[i].blue + ns->dark[i].blue) / 2;
		}
		gtk_widget_set_style(frame, ns);
		gtk_widget_set_style(but1, ns);
		gtk_widget_set_style(but2, ns);
		gtk_widget_set_style(but3, ns);
		gtk_widget_set_style(but4, ns);
		gtk_style_unref(ns);
	} else {
		GtkStyle *ns;

		ns = gtk_rc_get_style(frame);
		if(!ns) ns = gtk_style_new();

		gtk_style_ref(ns);
		gtk_widget_set_style(frame, ns);
		gtk_widget_set_style(but1, ns);
		gtk_widget_set_style(but2, ns);
		gtk_widget_set_style(but3, ns);
		gtk_widget_set_style(but4, ns);
		gtk_style_unref(ns);
	}
}


void
remove_directory(char *dirname, int just_clean)
{
	DIR *dir;
	struct dirent *dent;
	char *oldcwd;

	dir = opendir (dirname);
	if(!dir) return;
	oldcwd = g_get_current_dir();

	chdir(dirname);
	while((dent = readdir (dir)) != NULL) {
		if(strcmp(dent->d_name,".")==0 ||
		   strcmp(dent->d_name,"..")==0)
			continue;
		if(g_file_test(dent->d_name,G_FILE_TEST_ISDIR))
			remove_directory(dent->d_name, FALSE);
		else
			unlink(dent->d_name);
	}
	closedir(dir);
	chdir(oldcwd);

	if(!just_clean)
		rmdir(dirname);
	g_free(oldcwd);
}

