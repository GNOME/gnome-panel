/*
 * tz-sel-dialog.c: timezone selection dialog
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
#include <time.h>

#include <glib/gi18n.h>

#include <glade/glade.h>

#include "tz-sel-dialog.h"

#include "clock.h"
#include "clock-utils.h"
#include "e-map/e-map.h"
#include "tz.h"
#include "tz-list.h"

#define TZ_MAP_POINT_NORMAL_RGBA 0xc070a0ff
#define TZ_MAP_POINT_HOVER_RGBA 0xffff60ff
#define TZ_MAP_POINT_SELECTED_1_RGBA 0xff60e0ff
#define TZ_MAP_POINT_SELECTED_2_RGBA 0x000000ff

//TODO closing the expander doesn't resize the dialog

typedef struct _TzSelDialogData TzSelDialogData;

struct _TzSelDialogData {
	GtkWidget *dialog;

	TzList    *tz_list;

	GtkWidget *hint_label;
	GtkWidget *location_combo;
	GtkWidget *add_button;
	GtkWidget *up_button;
	GtkWidget *down_button;
	GtkWidget *remove_button;
	GtkWidget *tree_view;

	GtkListStore *combo_store;
	GtkListStore *list_store;

	EMapPoint *point_selected;
	EMapPoint *point_hover;
	EMap      *map;
	glong      correction;

	guint      timeout;

	gulong     added_id;
	gulong     changed_id;
	gulong     removed_id;
};

enum {
  COMBO_NAME_COLUMN,
  COMBO_LOCATION_COLUMN,
  COMBO_NUM_COLUMNS
};

enum {
  ZONE_COLUMN,
  NAME_COLUMN,
  SORT_COLUMN,
  EDITABLE_COLUMN,
  TZ_LIST_ITEM_COLUMN,
  TREE_NUM_COLUMNS
};

static void
tz_sel_dialog_data_free (TzSelDialogData *data)
{
	if (data->timeout)
		g_source_remove (data->timeout);
	data->timeout = 0;

	if (data->added_id != 0)
		g_signal_handler_disconnect (data->tz_list,
					     data->added_id);
	data->added_id = 0;
	if (data->changed_id != 0)
		g_signal_handler_disconnect (data->tz_list,
					     data->changed_id);
	data->changed_id = 0;
	if (data->removed_id != 0)
		g_signal_handler_disconnect (data->tz_list,
					     data->removed_id);
	data->removed_id = 0;

	g_slice_free (TzSelDialogData, data);
}

static void
tz_sel_dialog_help (GtkWidget *widget,
		    gpointer   data)
{
	clock_utils_display_help (widget, "clock", "clock", "clock-usage");
}

static void
tz_sel_dialog_close (GtkWidget       *widget,
		     TzSelDialogData *data)
{
	gtk_widget_destroy (data->dialog);
}

static TzLocation *
tz_location_from_point (TzSelDialogData *data,
			EMapPoint       *point)
{
	TzLocation *tz_loc = NULL;
	GPtrArray *locs;
	double p_longitude, p_latitude;
	double l_longitude, l_latitude;
	int i;

	locs = tz_get_locations ();
	e_map_point_get_location (point, &p_longitude, &p_latitude);

	for (i = 0; i < locs->len; i++) {
		tz_location_get_position (g_ptr_array_index (locs, i),
					  &l_longitude, &l_latitude);

		if (l_longitude - 0.005 <= p_longitude &&
				l_longitude + 0.005 >= p_longitude &&
				l_latitude - 0.005 <= p_latitude &&
				l_latitude + 0.005 >= p_latitude) {
			tz_loc = g_ptr_array_index (locs, i);
			break;
		}
	}

	return (tz_loc);
}

static void
tz_sel_dialog_set_hint (TzSelDialogData *data)
{
	const char *zone;
	char       *location;
	char       *text;
	char        hour[256];

	/* e_tz_map_location_from_point() can in theory return NULL, but in
	 * practice there are no reasons why it should */

	if (data->point_hover)
		zone = tz_location_get_zone (tz_location_from_point (data,
								     data->point_hover));
	else
		zone = NULL;

	if (zone) {
		time_t now;
		struct tm *tm;

		time (&now);
		tm = tz_get_localtime_at (zone, &now);

		location = NULL;

		//TODO: %X contains seconds, and we don't update every second...
		if (strftime (hour, sizeof (hour), "%X", tm)) {
			char *utf8;

			utf8 = g_locale_to_utf8 (hour, -1, NULL, NULL, NULL);

			if (utf8) {
				/* Translators: the first %s is a place and the
				 * second is a time. Result looks like this:
				 * Europe/Paris - 15:45 */
				location = g_strdup_printf (_("%s - %s"),
							    _(zone), utf8);
				g_free (utf8);
			};
		}

		if (location == NULL)
			location = g_strdup (_(zone));
	} else
		location = g_strdup_printf ("<i>%s</i>", _("move the mouse on the map"));

	text = g_strdup_printf (_("<b>Location:</b> %s"), location);
	g_free (location);

	gtk_label_set_markup (GTK_LABEL (data->hint_label), text);
	g_free (text);
}

typedef struct {
	TzLocation  *tz_loc;
	GtkTreeIter  iter;
	gboolean     found;
} TzSelDialogComboFind;

static gboolean
find_iter (GtkTreeModel         *model,
	   GtkTreePath          *path,
	   GtkTreeIter          *iter,
	   TzSelDialogComboFind *data)
{
	TzLocation *tz_loc;

	gtk_tree_model_get (model, iter, 
			    COMBO_LOCATION_COLUMN, &tz_loc, 
			    -1);

	if (tz_loc != data->tz_loc)
		return FALSE;

	data->found = TRUE;
	data->iter = *iter;
	return TRUE;
}

static void
tz_sel_dialog_set_selected_tz_in_combo (TzSelDialogData *data,
					TzLocation      *tz_loc)
{
	GtkTreeIter           iter;
	TzLocation           *old_tz_loc;
	TzSelDialogComboFind  find_data;

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (data->location_combo),
					   &iter))
		gtk_tree_model_get (GTK_TREE_MODEL (data->combo_store), &iter, 
				    COMBO_LOCATION_COLUMN, &old_tz_loc, 
				    -1);
	else
		old_tz_loc = NULL;

	if (old_tz_loc == tz_loc)
		return;

	find_data.tz_loc = tz_loc;
	find_data.found = FALSE;

	gtk_tree_model_foreach (GTK_TREE_MODEL (data->combo_store),
				(GtkTreeModelForeachFunc) find_iter,
				&find_data);

	if (find_data.found)
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (data->location_combo),
					       &find_data.iter);
}

static void
tz_sel_dialog_set_selected_tz_on_map (TzSelDialogData *data,
				      TzLocation      *tz_loc)
{
	double l_longitude, l_latitude;

	l_longitude = 0.0;
	l_latitude = 0.0;

	tz_location_get_position (tz_loc, &l_longitude, &l_latitude);

	if (data->point_selected)
		e_map_point_set_color_rgba (data->map, data->point_selected,
				            TZ_MAP_POINT_NORMAL_RGBA);
	data->point_selected = e_map_get_closest_point (data->map,
							l_longitude, l_latitude,
							FALSE);

	if (!e_map_point_is_in_view (data->map, data->point_selected)
	    && GTK_WIDGET_REALIZED (data->map))
		e_map_zoom_to_location (data->map, l_longitude, l_latitude);
}

static gboolean
tz_sel_dialog_motion (GtkWidget       *widget,
		      GdkEventMotion  *event,
		      TzSelDialogData *data)
{
	double longitude, latitude;

	e_map_window_to_world (data->map, (double) event->x, (double) event->y,
			       &longitude, &latitude);

	if (data->point_hover && data->point_hover != data->point_selected)
		e_map_point_set_color_rgba (data->map, data->point_hover,
					    TZ_MAP_POINT_NORMAL_RGBA);

	data->point_hover = e_map_get_closest_point (data->map,
						     longitude, latitude, TRUE);

	if (data->point_hover != data->point_selected)
		e_map_point_set_color_rgba (data->map, data->point_hover,
					    TZ_MAP_POINT_HOVER_RGBA);

	data->correction = tz_location_get_utc_offset (
				tz_location_from_point (data, data->point_hover));

	tz_sel_dialog_set_hint (data);

	return TRUE;
}

static gboolean
tz_sel_dialog_button_pressed (GtkWidget       *widget,
			      GdkEventButton  *event,
			      TzSelDialogData *data)
{
	double longitude, latitude;
	TzLocation *tz_loc;

	if (event->button != 1) {
		e_map_zoom_out (data->map);
		return TRUE;
	}

	e_map_window_to_world (data->map, (double) event->x, (double) event->y,
			       &longitude, &latitude);

	if (e_map_get_magnification (data->map) <= 1.0)
		e_map_zoom_to_location (data->map, longitude, latitude);

	if (data->point_selected)
		e_map_point_set_color_rgba (data->map, data->point_selected,
					    TZ_MAP_POINT_NORMAL_RGBA);
	data->point_selected = data->point_hover;

	tz_loc = tz_location_from_point (data, data->point_selected);
	tz_sel_dialog_set_selected_tz_in_combo (data, tz_loc);

	return TRUE;
}

static gboolean
tz_sel_dialog_leave (GtkWidget        *widget,
		     GdkEventCrossing *event,
		     TzSelDialogData  *data)
{
	if (event->mode != GDK_CROSSING_NORMAL)
		return FALSE;

	if (data->point_hover && data->point_hover != data->point_selected)
		e_map_point_set_color_rgba (data->map, data->point_hover,
					    TZ_MAP_POINT_NORMAL_RGBA);

	data->point_hover = NULL;

	tz_sel_dialog_set_hint (data);

	return TRUE;
}

static gboolean
tz_sel_dialog_flash_selected_point (TzSelDialogData *data)
{
	if (!IS_E_MAP (data->map))
		return FALSE;

	if (!data->point_selected)
		return TRUE;

	if (e_map_point_get_color_rgba (data->point_selected) ==
	    TZ_MAP_POINT_SELECTED_1_RGBA)
		e_map_point_set_color_rgba (data->map, data->point_selected,
					    TZ_MAP_POINT_SELECTED_2_RGBA);
	else
		e_map_point_set_color_rgba (data->map, data->point_selected,
					    TZ_MAP_POINT_SELECTED_1_RGBA);

	return TRUE;
}

static gboolean
tz_sel_dialog_combo_changed (GtkWidget       *widget,
			     TzSelDialogData *data)
{
	GtkTreeIter  iter;
	TzLocation  *tz_loc;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (data->location_combo),
					    &iter)) {
		gtk_widget_set_sensitive (data->add_button, FALSE);
		return TRUE;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (data->combo_store), &iter, 
			    COMBO_LOCATION_COLUMN, &tz_loc, 
			    -1);

	tz_sel_dialog_set_selected_tz_on_map (data, tz_loc);

	gtk_widget_set_sensitive (data->add_button,
				  !tz_list_contains (data->tz_list, tz_loc));

	return TRUE;
}

static void
tz_sel_dialog_update_add_sensitivity (TzSelDialogData *data)
{
	GtkTreeIter  iter;
	TzLocation  *tz_loc;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (data->location_combo),
					    &iter)) {
		gtk_widget_set_sensitive (data->add_button, FALSE);
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (data->combo_store), &iter, 
			    COMBO_LOCATION_COLUMN, &tz_loc, 
			    -1);

	gtk_widget_set_sensitive (data->add_button,
				  !tz_list_contains (data->tz_list, tz_loc));
}

static void
tz_sel_dialog_name_edited (GtkCellRendererText *cell,
			   char                *path_string,
			   char                *new_text,
			   TzSelDialogData     *data)
{
	GtkTreeIter  iter;
	GtkTreePath *path;
	TzListItem  *item;

	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (data->list_store),
				 &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (GTK_TREE_MODEL (data->list_store), &iter, 
			    TZ_LIST_ITEM_COLUMN, &item, 
			    -1);

	tz_list_rename (data->tz_list, item, new_text);
}

static void
tz_sel_dialog_selection_changed (GtkTreeSelection *selection,
				 TzSelDialogData  *data)
{
	GtkTreeIter iter;
	gboolean    selected;
	gboolean    first;
	gboolean    last;

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (data->list_store),
					   &iter))
		first = gtk_tree_selection_iter_is_selected (selection, &iter);
	else
		first = FALSE;

	selected = gtk_tree_selection_get_selected (selection, NULL, &iter);

	if (selected && gtk_tree_model_iter_next (GTK_TREE_MODEL (data->list_store),
						  &iter))
		last = FALSE;
	else
		last = selected;

	gtk_widget_set_sensitive (data->up_button, selected && !first);
	gtk_widget_set_sensitive (data->down_button, selected && !last);
	gtk_widget_set_sensitive (data->remove_button, selected);
}

static void
tz_sel_dialog_add_clicked (GtkWidget       *widget,
			   TzSelDialogData *data)
{
	GtkTreeIter  iter;
	TzLocation  *tz_loc;
	const char  *zone;
	const char  *name;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (data->location_combo),
					    &iter))
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (data->combo_store), &iter, 
			    COMBO_LOCATION_COLUMN, &tz_loc, 
			    -1);

	zone = tz_location_get_zone (tz_loc); 
	if (!zone)
		return;

	name = rindex (zone, '/');
	if (name != NULL)
		name++;
	else
		name = zone;

	tz_list_add (data->tz_list, tz_loc, name);
}

static void
tz_sel_dialog_up_clicked (GtkWidget       *widget,
			  TzSelDialogData *data)
{
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	TzListItem       *item;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data->tree_view));

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (data->list_store), &iter, 
			    TZ_LIST_ITEM_COLUMN, &item, 
			    -1);

	tz_list_move_up (data->tz_list, item);
}

static void
tz_sel_dialog_down_clicked (GtkWidget       *widget,
			    TzSelDialogData *data)
{
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	TzListItem       *item;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data->tree_view));

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (data->list_store), &iter, 
			    TZ_LIST_ITEM_COLUMN, &item, 
			    -1);

	tz_list_move_down (data->tz_list, item);
}

static void
tz_sel_dialog_remove_clicked (GtkWidget       *widget,
			      TzSelDialogData *data)
{
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	TzListItem       *item;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data->tree_view));

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (data->list_store), &iter, 
			    TZ_LIST_ITEM_COLUMN, &item, 
			    -1);

	tz_list_remove (data->tz_list, item);
	//TODO select something
}

static void
tz_sel_dialog_fill_timezones (TzSelDialogData *data)
{
	GPtrArray  *locs;
	int         i;
	TzLocation *tzl;
	gdouble     longitude, latitude;
	GtkTreeIter iter;


	locs = tz_get_locations ();

	for (i = 0; g_ptr_array_index (locs, i); i++) {
		tzl = g_ptr_array_index (locs, i);

		tz_location_get_position (tzl, &longitude, &latitude);
		e_map_add_point (data->map, NULL, longitude, latitude,
				 TZ_MAP_POINT_NORMAL_RGBA);

		gtk_list_store_append (data->combo_store, &iter);
		gtk_list_store_set (data->combo_store, &iter,
				    COMBO_NAME_COLUMN, _(tz_location_get_zone (tzl)),
				    COMBO_LOCATION_COLUMN, tzl,
				    -1); 
	}
}

static void
tz_sel_dialog_fill_tree_view (TzSelDialogData *data)
{
	GList      *l;
	TzListItem *item;
	GtkTreeIter iter;

	for (l = tz_list_get_items (data->tz_list); l != NULL; l = l->next) {
		item = l->data;

		gtk_list_store_append (data->list_store, &iter);
		gtk_list_store_set (data->list_store, &iter,
				    ZONE_COLUMN, _(tz_list_item_get_zone (item)),
				    NAME_COLUMN, tz_list_item_get_name (item),
				    SORT_COLUMN, tz_list_item_get_sort_order (item),
				    EDITABLE_COLUMN, TRUE,
				    TZ_LIST_ITEM_COLUMN, item,
				    -1); 
	}
}

static void
tz_sel_dialog_item_added (TzList          *tz_list,
			  TzListItem      *item,
			  TzSelDialogData *data)
{
	GtkTreeIter iter;

	gtk_list_store_append (data->list_store, &iter);

	gtk_list_store_set (data->list_store, &iter,
			    ZONE_COLUMN, _(tz_list_item_get_zone (item)),
			    NAME_COLUMN, tz_list_item_get_name (item),
			    SORT_COLUMN, tz_list_item_get_sort_order (item),
			    EDITABLE_COLUMN, TRUE,
			    TZ_LIST_ITEM_COLUMN, item,
			    -1); 

	tz_sel_dialog_update_add_sensitivity (data);
	tz_sel_dialog_selection_changed (gtk_tree_view_get_selection (GTK_TREE_VIEW (data->tree_view)),
					 data);
}

static gboolean
change_item (GtkTreeModel *model,
	     GtkTreePath  *path,
	     GtkTreeIter  *iter,
	     TzListItem   *item)
{
	TzListItem *iter_item;

	gtk_tree_model_get (model, iter, 
			    TZ_LIST_ITEM_COLUMN, &iter_item, 
			    -1);

	if (iter_item == item) {
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    ZONE_COLUMN, _(tz_list_item_get_zone (item)),
				    NAME_COLUMN, tz_list_item_get_name (item),
				    SORT_COLUMN, tz_list_item_get_sort_order (item),
				    -1); 
		return TRUE;
	}

	return FALSE;
}

static void
tz_sel_dialog_item_changed (TzList          *tz_list,
			    TzListItem      *item,
			    TzSelDialogData *data)
{
	gtk_tree_model_foreach (GTK_TREE_MODEL (data->list_store),
				(GtkTreeModelForeachFunc) change_item,
				item);

	tz_sel_dialog_update_add_sensitivity (data);
	tz_sel_dialog_selection_changed (gtk_tree_view_get_selection (GTK_TREE_VIEW (data->tree_view)),
					 data);
}

static gboolean
remove_item (GtkTreeModel *model,
	     GtkTreePath  *path,
	     GtkTreeIter  *iter,
	     TzListItem   *item)
{
	TzListItem *iter_item;

	gtk_tree_model_get (model, iter, 
			    TZ_LIST_ITEM_COLUMN, &iter_item, 
			    -1);

	if (iter_item == item) {
		gtk_list_store_remove (GTK_LIST_STORE (model), iter);
		return TRUE;
	}

	return FALSE;
}

static void
tz_sel_dialog_item_removed (TzList          *tz_list,
			    TzListItem      *item,
			    TzSelDialogData *data)
{
	gtk_tree_model_foreach (GTK_TREE_MODEL (data->list_store),
				(GtkTreeModelForeachFunc) remove_item,
				item);

	tz_sel_dialog_update_add_sensitivity (data);
	tz_sel_dialog_selection_changed (gtk_tree_view_get_selection (GTK_TREE_VIEW (data->tree_view)),
					 data);
}

GtkWidget *
tz_sel_dialog_new (TzList *tz_list)
{
	TzSelDialogData  *data;
	GladeXML         *xml;
	GtkWidget        *dialog;
	GtkCellRenderer  *renderer;
	GtkTreeSelection *selection;
	TzLocation       *system_tz_loc;

	xml = glade_xml_new (GLADEDIR "/timezone.glade", NULL, NULL);

	if (xml == NULL) {
		g_warning ("Cannot open \"%s\".\n", GLADEDIR "/timezone.glade");
		return NULL;
	}

	dialog = glade_xml_get_widget (xml, "timezone_dialog");

	data = g_slice_new0 (TzSelDialogData);
	g_object_set_data_full (G_OBJECT (dialog), "tz-sel-dialog-data",
				data, (GDestroyNotify) tz_sel_dialog_data_free);

	data->dialog = dialog;
	data->tz_list = tz_list;
	data->hint_label = glade_xml_get_widget (xml, "hint_label");
	data->location_combo = glade_xml_get_widget (xml, "location_combo");
	data->add_button = glade_xml_get_widget (xml, "addbutton");
	data->up_button = glade_xml_get_widget (xml, "upbutton");
	data->down_button = glade_xml_get_widget (xml, "downbutton");
	data->remove_button = glade_xml_get_widget (xml, "removebutton");
	data->tree_view = glade_xml_get_widget (xml, "tz_treeview");

	gtk_window_set_icon_name (GTK_WINDOW (dialog), CLOCK_ICON);

	g_signal_connect (glade_xml_get_widget (xml, "helpbutton"), "clicked",
			  G_CALLBACK (tz_sel_dialog_help), NULL);
	g_signal_connect (glade_xml_get_widget (xml, "closebutton"), "clicked",
			  G_CALLBACK (tz_sel_dialog_close), data);

	data->map = e_map_new ();

	e_map_set_smooth_zoom (data->map, TRUE);

	gtk_widget_set_events (GTK_WIDGET (data->map),
			       gtk_widget_get_events (GTK_WIDGET (data->map)) |
			       GDK_LEAVE_NOTIFY_MASK | GDK_VISIBILITY_NOTIFY_MASK);

	g_signal_connect (data->map, "motion-notify-event",
			  G_CALLBACK (tz_sel_dialog_motion), data);
	g_signal_connect (data->map, "button-press-event",
			  G_CALLBACK (tz_sel_dialog_button_pressed), data);
	g_signal_connect (data->map, "leave-notify-event",
			  G_CALLBACK (tz_sel_dialog_leave), data);

	gtk_container_add (GTK_CONTAINER (glade_xml_get_widget (xml,
								"map_window")),
			   GTK_WIDGET (data->map));
	gtk_widget_show (GTK_WIDGET (data->map));

	data->combo_store = gtk_list_store_new (COMBO_NUM_COLUMNS,
						G_TYPE_STRING,
						G_TYPE_POINTER);

	gtk_combo_box_set_model (GTK_COMBO_BOX (data->location_combo),
				 GTK_TREE_MODEL (data->combo_store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (data->location_combo),
				    renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (data->location_combo),
				       renderer,
				       "text", COMBO_NAME_COLUMN);

	g_signal_connect (data->location_combo, "changed",
			  G_CALLBACK (tz_sel_dialog_combo_changed), data);

	data->list_store = gtk_list_store_new (TREE_NUM_COLUMNS,
					       G_TYPE_STRING,
					       G_TYPE_STRING,
					       G_TYPE_INT,
					       G_TYPE_BOOLEAN,
					       G_TYPE_POINTER);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (data->list_store),
					      SORT_COLUMN, GTK_SORT_ASCENDING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (data->tree_view),
				 GTK_TREE_MODEL (data->list_store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (data->tree_view),
						     -1, _("Name"),
						     renderer,
						     "text", NAME_COLUMN,
						     "editable", EDITABLE_COLUMN,
						     NULL);
	gtk_tree_view_column_set_sizing (gtk_tree_view_get_column (GTK_TREE_VIEW (data->tree_view), 0),
					 GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	g_signal_connect (renderer, "edited",
			  G_CALLBACK (tz_sel_dialog_name_edited), data);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (data->tree_view),
						     -1, _("Timezone"),
						     renderer,
						     "text", ZONE_COLUMN,
						     NULL);
	gtk_tree_view_column_set_sizing (gtk_tree_view_get_column (GTK_TREE_VIEW (data->tree_view), 1),
					 GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data->tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (selection, "changed",
			  G_CALLBACK (tz_sel_dialog_selection_changed), data);
	tz_sel_dialog_selection_changed (selection, data);

	g_signal_connect (data->add_button, "clicked",
			  G_CALLBACK (tz_sel_dialog_add_clicked), data);
	g_signal_connect (data->up_button, "clicked",
			  G_CALLBACK (tz_sel_dialog_up_clicked), data);
	g_signal_connect (data->down_button, "clicked",
			  G_CALLBACK (tz_sel_dialog_down_clicked), data);
	g_signal_connect (data->remove_button, "clicked",
			  G_CALLBACK (tz_sel_dialog_remove_clicked), data);

	tz_sel_dialog_fill_timezones (data);
	tz_sel_dialog_fill_tree_view (data);

	data->added_id   = g_signal_connect (data->tz_list, "added",
					     G_CALLBACK (tz_sel_dialog_item_added), data);
	data->changed_id = g_signal_connect (data->tz_list, "changed",
					     G_CALLBACK (tz_sel_dialog_item_changed), data);
	data->removed_id = g_signal_connect (data->tz_list, "removed",
					     G_CALLBACK (tz_sel_dialog_item_removed), data);

	system_tz_loc = tz_get_location_by_name (tz_get_system_timezone ());
	if (system_tz_loc) {
		tz_sel_dialog_set_selected_tz_in_combo (data, system_tz_loc);
		tz_sel_dialog_set_selected_tz_on_map (data, system_tz_loc);
	}

	tz_sel_dialog_set_hint (data);

	data->timeout = g_timeout_add (100,
				       (GSourceFunc) tz_sel_dialog_flash_selected_point,
				       data);

	return dialog;
}

void
tz_sel_dialog_present (GtkWindow *window)
{
	TzSelDialogData *data;

	g_return_if_fail (window != NULL);

	data = g_object_get_data (G_OBJECT (window), "tz-sel-dialog-data");
	if (data && GTK_WIDGET_REALIZED (data->map))
		e_map_zoom_out (data->map);

	gtk_window_present (window);
}
