/*
 * Copyright (C) 2014 Alberts Muktupāvels
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *    Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 */

#ifndef CLOCK_LOCATION_EDIT_H
#define CLOCK_LOCATION_EDIT_H

#include "clock-location.h"

G_BEGIN_DECLS

#define CLOCK_TYPE_LOCATION_EDIT         (clock_location_edit_get_type ())
#define CLOCK_LOCATION_EDIT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), \
                                          CLOCK_TYPE_LOCATION_EDIT,        \
                                          ClockLocationEdit))
#define CLOCK_LOCATION_EDIT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    \
                                          CLOCK_TYPE_LOCATION_EDIT,        \
                                          ClockLocationEditClass))
#define CLOCK_IS_LOCATION_EDIT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
                                          CLOCK_TYPE_LOCATION_EDIT))
#define CLOCK_IS_LOCATION_EDIT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    \
                                          CLOCK_TYPE_LOCATION_EDIT))
#define CLOCK_LOCATION_EDIT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o),   \
                                          CLOCK_TYPE_LOCATION_EDIT,        \
                                          ClockLocationEditClass))

typedef struct _ClockLocationEdit        ClockLocationEdit;
typedef struct _ClockLocationEditClass   ClockLocationEditClass;
typedef struct _ClockLocationEditPrivate ClockLocationEditPrivate;

struct _ClockLocationEdit
{
	GtkDialog                 parent;
	ClockLocationEditPrivate *priv;
};

struct _ClockLocationEditClass
{
	GtkDialogClass parent_class;
};

GType      clock_location_edit_get_type (void);

GtkWidget *clock_location_edit_new      (GSettings     *settings,
                                         GtkWindow     *parent,
                                         ClockLocation *clock_location);

G_END_DECLS

#endif
