/*
 * tz-window.c: toplevel window containing timezones
 *
 * Copyright (C) 2007 Vincent Untz <vuntz@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      Vincent Untz <vuntz@gnome.org>
 */

#include <config.h>

#include <string.h>

#include <glib/gi18n.h>

#include "tz-window.h"

#include "clock.h"
#include "tz-list.h"

#define TZ_WINDOW_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TZ_TYPE_WINDOW, TzWindowPrivate))

typedef struct _TzWindowItem TzWindowItem;

struct _TzWindowItem {
	TzListItem *list_item;
	GtkWidget  *name_label;
	GtkWidget  *time_label;
};

struct _TzWindowPrivate {
	GtkWidget  *align;
	GtkWidget  *separator;
	GtkWidget  *vbox_names;
	GtkWidget  *vbox_times;
	TzList     *tz_list;
	GHashTable *tz_hash;

	char       *format;

	gulong      added_id;
	gulong      changed_id;
	gulong      removed_id;
};

G_DEFINE_TYPE (TzWindow, tz_window, GTK_TYPE_WINDOW);

static void
tz_window_item_set_time (TzWindow     *tzwin,
			 TzWindowItem *item,
			 time_t       *now)
{
	struct tm *tm;
	char hour[256];
	char *utf8;

	tm = tz_get_localtime_at (tz_list_item_get_zone (item->list_item),
				  now);

	if (strftime (hour, sizeof (hour), tzwin->priv->format, tm) <= 0)
		strcpy (hour, "???");

	utf8 = g_locale_to_utf8 (hour, -1, NULL, NULL, NULL);
	if (!utf8)
		utf8 = g_strdup (hour);

	gtk_label_set_text (GTK_LABEL (item->time_label), utf8);

	g_free (utf8);
}

static void
tz_window_item_set_tooltip (TzWindowItem *item)
{
	TzLocation *tz_loc;
	TzInfo     *tz;
	char       *text;
	char       *line;

	tz_loc = tz_list_item_get_tz_location (item->list_item);
	tz = tz_info_from_location (tz_loc);

	if (tz->daylight)
		/* Translators: the first string is a timezone name as in "UTC"
		 * the second string is "+" or nothing. The result looks like
		 * this: BST (UTC +01:00) */
		line = g_strdup_printf (_("%s with daylight saving (UTC %s%.2ld:%.2ld)"),
					tz->tzname_normal,
					(tz->utc_offset / 3600) >= 0 ? "+" : "",
					tz->utc_offset / 3600,
					(ABS (tz->utc_offset) % 3600) / 60);
	else
		/* Translators: the first string is a timezone name as in "UTC"
		 * the second string is "+" or nothing. The result looks like
		 * this: BST (UTC +01:00) */
		line = g_strdup_printf (_("%s (UTC %s%.2ld:%.2ld)"),
					tz->tzname_normal,
					(tz->utc_offset / 3600) >= 0 ? "+" : "",
					tz->utc_offset / 3600,
					(ABS (tz->utc_offset) % 3600) / 60);

	/* Translators: the first string is a timezone name (as in
	 * "Europe/Paris"), the second string is a country name abbreviation
	 * (as in "FR") and the last string is another line (it's the one
	 * looking like "BST (UTC +01:00)") */
	text = g_strdup_printf (_("%s - %s\n%s"),
				_(tz_location_get_zone (tz_loc)),
				tz_location_get_country (tz_loc),
				line);
	g_free (line);

	gtk_widget_set_tooltip_text (item->name_label, text);
	gtk_widget_set_tooltip_text (item->time_label, text);

	tz_info_free (tz);
	g_free (text);
}

static void
tz_window_item_free (gpointer data)
{
	g_slice_free (TzWindowItem, data);
}

static void
tz_list_item_added (TzList     *tz_list,
		    TzListItem *item,
		    TzWindow   *tzwin)
{
	TzWindowItem *win_item;
	time_t        now;

	win_item = g_hash_table_lookup (tzwin->priv->tz_hash, item);
	if (win_item)
		return;

	win_item = g_slice_new (TzWindowItem);

	win_item->list_item = item;

	win_item->name_label = gtk_label_new (tz_list_item_get_name (item));
	win_item->time_label = gtk_label_new ("???");

	time (&now);
	tz_window_item_set_tooltip (win_item);
	tz_window_item_set_time (tzwin, win_item, &now);

	g_hash_table_insert (tzwin->priv->tz_hash, item, win_item);

	gtk_misc_set_alignment (GTK_MISC (win_item->name_label), 0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (win_item->time_label), 1, 0.5);
	gtk_widget_show (win_item->name_label);
	gtk_widget_show (win_item->time_label);

	gtk_container_add (GTK_CONTAINER (tzwin->priv->vbox_names),
			   win_item->name_label);
	gtk_container_add (GTK_CONTAINER (tzwin->priv->vbox_times),
			   win_item->time_label);

	gtk_box_reorder_child (GTK_BOX (tzwin->priv->vbox_names),
			       win_item->name_label,
			       tz_list_item_get_sort_order (item));
	gtk_box_reorder_child (GTK_BOX (tzwin->priv->vbox_times),
			       win_item->time_label,
			       tz_list_item_get_sort_order (item));

	gtk_widget_show (tzwin->priv->align);
	gtk_widget_show (tzwin->priv->separator);
}

static void
tz_list_item_changed (TzList     *tz_list,
		      TzListItem *item,
		      TzWindow   *tzwin)
{
	TzWindowItem *win_item;
	time_t        now;

	win_item = g_hash_table_lookup (tzwin->priv->tz_hash, item);
	if (!win_item)
		return;

	gtk_label_set_text (GTK_LABEL (win_item->name_label),
			    tz_list_item_get_name (item));

	time (&now);
	tz_window_item_set_tooltip (win_item);
	tz_window_item_set_time (tzwin, win_item, &now);

	gtk_box_reorder_child (GTK_BOX (tzwin->priv->vbox_names),
			       win_item->name_label,
			       tz_list_item_get_sort_order (item));
	gtk_box_reorder_child (GTK_BOX (tzwin->priv->vbox_times),
			       win_item->time_label,
			       tz_list_item_get_sort_order (item));
}

static void
tz_list_item_removed (TzList     *tz_list,
		      TzListItem *item,
		      TzWindow   *tzwin)
{
	TzWindowItem *win_item;
	GList        *children;

	win_item = g_hash_table_lookup (tzwin->priv->tz_hash, item);
	if (!win_item)
		return;

	gtk_widget_destroy (win_item->name_label);
	gtk_widget_destroy (win_item->time_label);
	g_hash_table_remove (tzwin->priv->tz_hash, item);

	children = gtk_container_get_children (GTK_CONTAINER (tzwin->priv->vbox_names));
	if (g_list_length (children) == 0) {
		gtk_widget_hide (tzwin->priv->align);
		gtk_widget_hide (tzwin->priv->separator);
	}
	g_list_free (children);
}

static void
tz_window_edit_zones (GtkWidget *widget,
		      TzWindow  *tzwin)
{
	tz_list_edit (tzwin->priv->tz_list,
		      gtk_widget_get_screen (GTK_WIDGET (tzwin)));
}

static void
tz_window_fill (TzWindow *tzwin)
{
        GtkWidget *frame;
        GtkWidget *vbox;
        GtkWidget *hbox;
	GtkWidget *button; 

        frame = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
        gtk_container_add (GTK_CONTAINER (tzwin), frame);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new (FALSE, 4);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_widget_show (vbox);

	tzwin->priv->align = gtk_alignment_new (0, 0, 0, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (tzwin->priv->align),
				   4, 0, 4, 4);
	gtk_container_add (GTK_CONTAINER (vbox), tzwin->priv->align);

	//TODO: the 12 pixels here are breaking the tooltip
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_add (GTK_CONTAINER (tzwin->priv->align), hbox);
	gtk_widget_show (hbox);

	tzwin->priv->vbox_names = gtk_vbox_new (TRUE, 4);
	gtk_container_add (GTK_CONTAINER (hbox), tzwin->priv->vbox_names);
	gtk_widget_show (tzwin->priv->vbox_names);

	tzwin->priv->vbox_times = gtk_vbox_new (TRUE, 4);
	gtk_container_add (GTK_CONTAINER (hbox), tzwin->priv->vbox_times);
	gtk_widget_show (tzwin->priv->vbox_times);

	tzwin->priv->separator = gtk_hseparator_new ();
	gtk_container_add (GTK_CONTAINER (vbox), tzwin->priv->separator);

	button = gtk_button_new_with_mnemonic (_("_Edit timezones..."));
	gtk_button_set_alignment (GTK_BUTTON (button), 0, 0.5);
	gtk_container_add (GTK_CONTAINER (vbox), button);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (tz_window_edit_zones), tzwin);
	gtk_widget_show (button);
}

static GObject *
tz_window_constructor (GType                  type,
		       guint                  n_construct_properties,
		       GObjectConstructParam *construct_properties)
{
	GObject  *obj;
	TzWindow *tzwin;

	obj = G_OBJECT_CLASS (tz_window_parent_class)->constructor (type,
								    n_construct_properties,
								    construct_properties);

	tzwin = TZ_WINDOW (obj);

	tz_window_fill (tzwin);

	return obj;
}

static void
tz_window_destroy (GtkObject *object)
{
	TzWindow *tzwin;

	tzwin = TZ_WINDOW (object);

	if (tzwin->priv->tz_list) {
		if (tzwin->priv->added_id != 0)
			g_signal_handler_disconnect (tzwin->priv->tz_list,
						     tzwin->priv->added_id);
		if (tzwin->priv->changed_id != 0)
			g_signal_handler_disconnect (tzwin->priv->tz_list,
						     tzwin->priv->changed_id);
		if (tzwin->priv->removed_id != 0)
			g_signal_handler_disconnect (tzwin->priv->tz_list,
						     tzwin->priv->removed_id);

		g_object_unref (tzwin->priv->tz_list);
	}
	tzwin->priv->tz_list = NULL;
	tzwin->priv->added_id = 0;
	tzwin->priv->changed_id = 0;
	tzwin->priv->removed_id = 0;

	if (tzwin->priv->tz_hash)
		g_hash_table_destroy (tzwin->priv->tz_hash);
	tzwin->priv->tz_hash = NULL;

	if (tzwin->priv->format)
		g_free (tzwin->priv->format);
	tzwin->priv->format = NULL;
}

static void
tz_window_class_init (TzWindowClass *klass)
{
	GObjectClass   *gobject_class   = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtkobject_class = GTK_OBJECT_CLASS (klass);

	gobject_class->constructor = tz_window_constructor;

	gtkobject_class->destroy = tz_window_destroy;

	g_type_class_add_private (klass, sizeof (TzWindowPrivate));
}

static void
tz_window_init (TzWindow *tzwin)
{
	GtkWindow *window;

	tzwin->priv = TZ_WINDOW_GET_PRIVATE (tzwin);

	window = GTK_WINDOW (tzwin);
	gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DOCK);
	gtk_window_set_decorated (window, FALSE);
	gtk_window_set_resizable (window, FALSE);
	gtk_window_stick (window);
	gtk_window_set_title (window, _("Timezones"));
	gtk_window_set_icon_name (window, CLOCK_ICON);

	tzwin->priv->tz_list = NULL;
	tzwin->priv->tz_hash = g_hash_table_new_full (g_direct_hash,
						      g_direct_equal,
						      NULL,
						      tz_window_item_free);

	tzwin->priv->format = g_strdup ("???");
}

static void
tz_window_set_tz_list (TzWindow *tzwin,
		       TzList   *tz_list)
{
	GList *l;

	g_return_if_fail (TZ_IS_WINDOW (tzwin));

	g_assert (tzwin->priv->tz_list == NULL);

	tzwin->priv->tz_list = g_object_ref (tz_list);

	tzwin->priv->added_id   = g_signal_connect (tz_list, "added",
						    G_CALLBACK (tz_list_item_added), tzwin);
	tzwin->priv->changed_id = g_signal_connect (tz_list, "changed",
						    G_CALLBACK (tz_list_item_changed), tzwin);
	tzwin->priv->removed_id = g_signal_connect (tz_list, "removed",
						    G_CALLBACK (tz_list_item_removed), tzwin);

	for (l = tz_list_get_items (tz_list); l != NULL; l = l->next)
		tz_list_item_added (tz_list, l->data, tzwin);
}


GtkWidget *
tz_window_new (TzList *tz_list)
{
	TzWindow *tzwin;

	tzwin = g_object_new (TZ_TYPE_WINDOW, NULL);

	/* It doesn't work if we do this in tz_window_init(), so do it
	 * here */
	GTK_WINDOW (tzwin)->type = GTK_WINDOW_TOPLEVEL;

	tz_window_set_tz_list (tzwin, tz_list);

	return GTK_WIDGET (tzwin);
}

typedef struct {
	TzWindow *tzwin;
	time_t   *now;
} TzWindowItemRefreshData;

static void
tz_window_item_refresh (gpointer                 key,
			TzWindowItem            *item,
			TzWindowItemRefreshData *data)
{
	tz_window_item_set_time (data->tzwin, item, data->now);
}

void
tz_window_refresh (TzWindow   *tzwin,
		   time_t      now,
		   const char *format)
{
	TzWindowItemRefreshData data;
	char *c;

	g_return_if_fail (TZ_IS_WINDOW (tzwin));

	if (tzwin->priv->format)
		g_free (tzwin->priv->format);
	tzwin->priv->format = g_strdup (format);

	c = index (tzwin->priv->format, '\n');
	while (c) {
		*c = ' ';
		c = index (c, '\n');
	}

	data.tzwin = tzwin;
	data.now   = &now;
	g_hash_table_foreach (tzwin->priv->tz_hash,
			      (GHFunc) tz_window_item_refresh, &data);
}
