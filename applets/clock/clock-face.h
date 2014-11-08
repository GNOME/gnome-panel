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

#ifndef CLOCK_FACE_H
#define CLOCK_FACE_H

#include <gtk/gtk.h>

#include "clock-location.h"
#include "clock-time.h"

G_BEGIN_DECLS

#define CLOCK_TYPE_FACE         (clock_face_get_type ())
#define CLOCK_FACE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), \
                                 CLOCK_TYPE_FACE,                 \
                                 ClockFace))
#define CLOCK_FACE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    \
                                 CLOCK_TYPE_FACE,                 \
                                 ClockFaceClass))
#define CLOCK_IS_FACE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
                                 CLOCK_TYPE_FACE))
#define CLOCK_IS_FACE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    \
                                 CLOCK_TYPE_FACE))
#define CLOCK_FACE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o),   \
                                 CLOCK_TYPE_FACE,                 \
                                 ClockFaceClass))

typedef struct _ClockFace        ClockFace;
typedef struct _ClockFaceClass   ClockFaceClass;
typedef struct _ClockFacePrivate ClockFacePrivate;

struct _ClockFace
{
	GtkImage          parent;
	ClockFacePrivate *priv;
};

struct _ClockFaceClass
{
	GtkImageClass parent_class;
};

GType      clock_face_get_type (void);

GtkWidget *clock_face_new      (ClockLocation *location,
                                ClockTime     *time,
                                gboolean       show_seconds);

G_END_DECLS

#endif
