/*
 * tz-window.h: toplevel window containing timezones
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

#ifndef TZ_WINDOW_H
#define TZ_WINDOW_H

#include <gtk/gtk.h>

#include "tz-list.h"

G_BEGIN_DECLS

#define TZ_TYPE_WINDOW         (tz_window_get_type ())
#define TZ_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TZ_TYPE_WINDOW, TzWindow))
#define TZ_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TZ_TYPE_WINDOW, TzWindowClass))
#define TZ_IS_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TZ_TYPE_WINDOW))
#define TZ_IS_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TZ_TYPE_WINDOW))
#define TZ_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TZ_TYPE_WINDOW, TzWindowClass))

typedef struct _TzWindow        TzWindow;
typedef struct _TzWindowClass   TzWindowClass;
typedef struct _TzWindowPrivate TzWindowPrivate;

struct _TzWindow {
	GtkWindow         parent_instance;

	TzWindowPrivate  *priv;
};

struct _TzWindowClass {
	GtkWindowClass parent_class;
};

GType      tz_window_get_type (void) G_GNUC_CONST;	

GtkWidget *tz_window_new      (TzList     *tz_list);
void       tz_window_refresh  (TzWindow   *tzwin,
			       time_t      now,
			       const char *format);

G_END_DECLS

#endif /* TZ_WINDOW_H */
