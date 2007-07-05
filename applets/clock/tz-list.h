/*
 * Copyright (C) 2007 Vincent Untz
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
 *	Vincent Untz <vuntz@gnome.org>
 */

#ifndef TZ_LIST_H
#define TZ_LIST_H

#include <glib-object.h>
#include <gdk/gdkscreen.h>

#include "tz.h"

G_BEGIN_DECLS

#define TZ_TYPE_LIST			(tz_list_get_type ())
#define TZ_LIST(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), TZ_TYPE_LIST, TzList))
#define TZ_LIST_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), TZ_TYPE_LIST, TzListClass))
#define TZ_IS_LIST(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TZ_TYPE_LIST))
#define TZ_IS_LIST_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), TZ_TYPE_LIST))
#define TZ_LIST_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), TZ_TYPE_LIST, TzListClass))

typedef struct _TzList      TzList;
typedef struct _TzListClass TzListClass;

typedef struct _TzListPrivate TzListPrivate;

typedef struct _TzListItem TzListItem;

struct _TzListClass
{
	GObjectClass parent_class;

	void (* added)   (TzList     *list,
			  TzListItem *item);
	void (* changed) (TzList     *list,
			  TzListItem *item);
	void (* removed) (TzList     *list,
			  TzListItem *item);
};

struct _TzList
{
	GObject parent_instance;

	TzListPrivate *priv;
};

GType       tz_list_get_type (void);

TzList     *tz_list_new       (const char *prefs_dir);
GList      *tz_list_get_items (TzList     *list);
gboolean    tz_list_contains  (TzList     *list,
			       TzLocation *tz_loc);
void        tz_list_edit      (TzList     *list,
			       GdkScreen  *screen);

void        tz_list_add       (TzList     *list,
			       TzLocation *tz_loc,
			       const char *name);
void        tz_list_rename    (TzList     *list,
			       TzListItem *item,
			       const char *new_name);
void        tz_list_move_up   (TzList     *list,
			       TzListItem *item);
void        tz_list_move_down (TzList     *list,
			       TzListItem *item);
void        tz_list_remove    (TzList     *list,
			       TzListItem *item);

TzLocation *tz_list_item_get_tz_location (TzListItem *item);
const char *tz_list_item_get_zone        (TzListItem *item);
const char *tz_list_item_get_name        (TzListItem *item);
int         tz_list_item_get_sort_order  (TzListItem *item);

G_END_DECLS

#endif /* TZ_LIST_H */
