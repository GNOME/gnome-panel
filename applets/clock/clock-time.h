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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *    Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 */

#ifndef CLOCK_TIME_H
#define CLOCK_TIME_H

#include "clock-location.h"

G_BEGIN_DECLS

#define CLOCK_TYPE_TIME         (clock_time_get_type ())
#define CLOCK_TIME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), \
                                 CLOCK_TYPE_TIME,                 \
                                 ClockTime))
#define CLOCK_TIME_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    \
                                 CLOCK_TYPE_TIME,                 \
                                 ClockTimeClass))
#define CLOCK_IS_TIME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
                                 CLOCK_TYPE_TIME))
#define CLOCK_IS_TIME_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    \
                                 CLOCK_TYPE_TIME))
#define CLOCK_TIME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o),   \
                                 CLOCK_TYPE_TIME,                 \
                                 ClockTimeClass))

typedef struct _ClockTime        ClockTime;
typedef struct _ClockTimeClass   ClockTimeClass;
typedef struct _ClockTimePrivate ClockTimePrivate;

struct _ClockTime
{
	GObject           parent;
	ClockTimePrivate *priv;
};

struct _ClockTimeClass
{
	GObjectClass parent_class;

	void (* hour_changed)   (ClockTime *time,
	                         gint       hour);
	void (* minute_changed) (ClockTime *time,
	                         gint       hour,
	                         gint       minute);
	void (* second_changed) (ClockTime *time,
	                         gint       hour,
	                         gint       minute,
	                         gint       second);
};

GType      clock_time_get_type (void);

ClockTime *clock_time_new      (ClockLocation *location);

G_END_DECLS

#endif
