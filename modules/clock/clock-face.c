/**
 * clock.c
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

#include <gtk/gtk.h>
#include <math.h>
#include <time.h>

#include "clock-applet.h"
#include "clock-face.h"
#include "clock-location.h"
#include "clock-utils.h"

#define CLOCK_FACE_SIZE 48

static void     clock_face_finalize             (GObject *);
static gboolean clock_face_draw                 (GtkWidget     *clock,
                                                 cairo_t       *cr);
static void     clock_face_get_preferred_width  (GtkWidget     *this,
                                                 gint          *minimal_width,
                                                 gint          *natural_width);
static void     clock_face_get_preferred_height (GtkWidget     *this,
                                                 gint          *minimal_height,
                                                 gint          *natural_height);

typedef enum {
	CLOCK_FACE_MORNING,
	CLOCK_FACE_DAY,
	CLOCK_FACE_EVENING,
	CLOCK_FACE_NIGHT,
	CLOCK_FACE_INVALID
} ClockFaceTimeOfDay;

struct _ClockFacePrivate
{
	GDateTime *time; /* the time on the clock face */
        int minute_offset; /* the offset of the minutes hand */

	ClockFaceTimeOfDay timeofday;
        ClockLocation *location;
        cairo_surface_t *face;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClockFace, clock_face, GTK_TYPE_WIDGET)

static void
clock_face_class_init (ClockFaceClass *class)
{
        GObjectClass *obj_class;
        GtkWidgetClass *widget_class;

        obj_class = G_OBJECT_CLASS (class);
        widget_class = GTK_WIDGET_CLASS (class);

        /* GtkWidget signals */
        widget_class->draw = clock_face_draw;
        widget_class->get_preferred_width  = clock_face_get_preferred_width;
        widget_class->get_preferred_height = clock_face_get_preferred_height;

        /* GObject signals */
        obj_class->finalize = clock_face_finalize;
}

static void
clock_face_init (ClockFace *this)
{
        ClockFacePrivate *priv;

        priv = this->priv = clock_face_get_instance_private (this);

        priv->timeofday = CLOCK_FACE_INVALID;
        priv->location = NULL;

        gtk_widget_set_has_window (GTK_WIDGET (this), FALSE);
}

static void
ensure_clock_face (ClockFace *this)
{
        ClockFacePrivate *priv = this->priv;
        const gchar *daytime_string[4] = { "morning", "day", "evening", "night" };
        int scale;
        int width;
        int height;
        char *name;
        GdkPixbuf *pixbuf;

        if (priv->face != NULL)
                return;

        scale = gtk_widget_get_scale_factor (GTK_WIDGET (this));
        width = CLOCK_FACE_SIZE * scale;
        height = CLOCK_FACE_SIZE * scale;

        name = g_strconcat (CLOCK_RESOURCE_PATH "icons/",
                            "clock-face-small-", daytime_string[priv->timeofday], ".svg",
                            NULL);

        pixbuf = gdk_pixbuf_new_from_resource_at_scale (name,
                                                        width,
                                                        height,
                                                        FALSE,
                                                        NULL);
        g_free (name);

        if (pixbuf == NULL) {
                name = g_strdup (CLOCK_RESOURCE_PATH "icons/clock-face-small.svg");
                pixbuf = gdk_pixbuf_new_from_resource_at_scale (name,
                                                                width,
                                                                height,
                                                                FALSE,
                                                                NULL);
                g_free (name);
        }

        if (pixbuf == NULL)
                return;

        priv->face = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, NULL);
        g_clear_object (&pixbuf);
}

static gboolean
clock_face_draw (GtkWidget *this, cairo_t *cr)
{
        ClockFacePrivate *priv = CLOCK_FACE (this)->priv;
        int width, height;
        double x, y;
        double radius;
        int hours, minutes;
        /* Hand lengths as a multiple of the clock radius */
        double hour_length, min_length;

        ensure_clock_face (CLOCK_FACE (this));

        if (GTK_WIDGET_CLASS (clock_face_parent_class)->draw)
                GTK_WIDGET_CLASS (clock_face_parent_class)->draw (this, cr);

        hour_length = 0.5;
        min_length = 0.7;

        width = gtk_widget_get_allocated_width (this);
        height = gtk_widget_get_allocated_height (this);

        x = width / 2;
        y = height / 2;
        radius = MIN (width / 2, height / 2) - 5;

        /* clock back */
        if (priv->face != NULL) {
                double offset_x;
                double offset_y;

                offset_x = (width - CLOCK_FACE_SIZE) / 2.;
                offset_y = (height - CLOCK_FACE_SIZE) / 2.;

                cairo_save (cr);
                cairo_set_source_surface (cr, priv->face, offset_x, offset_y);
                cairo_paint (cr);
                cairo_restore (cr);
        }

        /* clock hands */
        hours = g_date_time_get_hour (priv->time);
        minutes = g_date_time_get_minute (priv->time) + priv->minute_offset;

        cairo_set_line_width (cr, 1);

        /* hour hand:
         * the hour hand is rotated 30 degrees (pi/6 r) per hour +
         * 1/2 a degree (pi/360 r) per minute
         */
        cairo_save (cr);
        cairo_move_to (cr, x, y);
        cairo_line_to (cr, x + radius * hour_length * sin (M_PI / 6 * hours +
                                                           M_PI / 360 * minutes),
                           y + radius * hour_length * -cos (M_PI / 6 * hours +
                                                            M_PI / 360 * minutes));
        cairo_stroke (cr);
        cairo_restore (cr);
        /* minute hand:
         * the minute hand is rotated 6 degrees (pi/30 r) per minute
         */
        cairo_move_to (cr, x, y);
        cairo_line_to (cr, x + radius * min_length * sin (M_PI / 30 * minutes),
                           y + radius * min_length * -cos (M_PI / 30 * minutes));
        cairo_stroke (cr);

        return FALSE;
}

static void
clock_face_redraw_canvas (ClockFace *this)
{
        gtk_widget_queue_draw (GTK_WIDGET (this));
}

static void
clock_face_get_preferred_width (GtkWidget *this,
                                gint      *minimal_width,
                                gint      *natural_width)
{
        *minimal_width = *natural_width = CLOCK_FACE_SIZE;
}

static void
clock_face_get_preferred_height (GtkWidget *this,
                                 gint      *minimal_height,
                                 gint      *natural_height)
{
        *minimal_height = *natural_height = CLOCK_FACE_SIZE;
}

static void
update_time_and_face (ClockFace *this)
{
        ClockFacePrivate *priv;
	ClockFaceTimeOfDay timeofday;
	int hour;

        priv = this->priv;

	if (priv->time)
		g_date_time_unref (priv->time);
        /* update the time */
        if (priv->location)
                priv->time = clock_location_localtime (priv->location);
	else
		priv->time = g_date_time_new_now_local ();

	/* FIXME  this should be a gconf setting
         * Or we could use some code from clock-sun.c?
         * currently we hardcode
         * morning 7-9
         * day 9-17
         * evening 17-22
         * night 22-7
         */
	hour = g_date_time_get_hour (priv->time);
	if (hour < 7)
		timeofday = CLOCK_FACE_NIGHT;
	else if (hour < 9)
		timeofday = CLOCK_FACE_MORNING;
	else if (hour < 17)
		timeofday = CLOCK_FACE_DAY;
	else if (hour < 22)
		timeofday = CLOCK_FACE_EVENING;
	else
		timeofday = CLOCK_FACE_NIGHT;

	if (priv->timeofday != timeofday) {
		priv->timeofday = timeofday;

		g_clear_pointer (&priv->face, cairo_surface_destroy);
	}
}

gboolean
clock_face_refresh (ClockFace *this)
{
        update_time_and_face (this);
        clock_face_redraw_canvas (this);

        return TRUE; /* keep running this event */
}

GtkWidget *
clock_face_new_with_location (ClockLocation *loc)
{
        GObject *obj = g_object_new (INTL_TYPE_CLOCK_FACE, NULL);
        ClockFacePrivate *priv = CLOCK_FACE (obj)->priv;

        priv->location = g_object_ref (loc);

        return GTK_WIDGET (obj);
}

static void
clock_face_finalize (GObject *obj)
{
        ClockFace *face = CLOCK_FACE (obj);
        ClockFacePrivate *priv = face->priv;

        if (priv->location) {
                g_object_unref (priv->location);
                priv->location = NULL;
        }

        g_clear_pointer (&priv->face, cairo_surface_destroy);

        G_OBJECT_CLASS (clock_face_parent_class)->finalize (obj);
}
