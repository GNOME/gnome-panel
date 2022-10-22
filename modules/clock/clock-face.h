/**
 * clock.h
 *
 * A GTK+ widget that implements a clock face
 *
 * (c) 2007, Peter Teichman
 * (c) 2005-2006, Davyd Madeley
 *
 * Authors:
 *   Davyd Madeley  <davyd@madeley.id.au>
 *   Peter Teichman <peter@novell.com>
 */

#ifndef __INTL_CLOCK_FACE_H__
#define __INTL_CLOCK_FACE_H__

#include <gtk/gtk.h>
#include "clock-location.h"

G_BEGIN_DECLS

#define INTL_TYPE_CLOCK_FACE          (clock_face_get_type ())
#define CLOCK_FACE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), INTL_TYPE_CLOCK_FACE, ClockFace))
#define CLOCK_FACE_CLASS(obj)         (G_TYPE_CHECK_CLASS_CAST ((obj), INTL_CLOCK_FACE, ClockFaceClass))
#define INTL_IS_CLOCK_FACE(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), INTL_TYPE_CLOCK_FACE))
#define INTL_IS_CLOCK_FACE_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((obj), INTL_TYPE_CLOCK_FACE))
#define CLOCK_FACE_GET_CLASS          (G_TYPE_INSTANCE_GET_CLASS ((obj), INTL_TYPE_CLOCK_FACE, ClockFaceClass))

typedef struct _ClockFace           ClockFace;
typedef struct _ClockFacePrivate    ClockFacePrivate;
typedef struct _ClockFaceClass      ClockFaceClass;

struct _ClockFace
{
        GtkWidget parent;

        /* < private > */
        ClockFacePrivate *priv;
};

struct _ClockFaceClass
{
        GtkWidgetClass parent_class;
};

GType clock_face_get_type (void);

GtkWidget *clock_face_new_with_location (ClockLocation *loc);
gboolean clock_face_refresh (ClockFace *this);


G_END_DECLS

#endif
