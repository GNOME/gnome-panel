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

#include <librsvg/rsvg.h>

#include "clock-face.h"
#include "clock-location.h"

#define CLOCK_FACE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), INTL_TYPE_CLOCK_FACE, ClockFacePrivate))

G_DEFINE_TYPE (ClockFace, clock_face, GTK_TYPE_WIDGET);

static void clock_face_finalize (GObject *);
static gboolean clock_face_expose (GtkWidget *clock, GdkEventExpose *event);
static void clock_face_size_request (GtkWidget *clock,
				     GtkRequisition *requisition);
static void clock_face_unmap (GtkWidget *clock);
static void clock_face_load_face (ClockFace *this,
				  gint width, gint height);
typedef struct _ClockFacePrivate ClockFacePrivate;

typedef enum {
	CLOCK_FACE_MORNING,
	CLOCK_FACE_DAY,
	CLOCK_FACE_EVENING,
	CLOCK_FACE_NIGHT
} ClockFaceTimeOfDay;

struct _ClockFacePrivate
{
        struct tm time; /* the time on the clock face */
        int minute_offset; /* the offset of the minutes hand */

        gboolean running;
        ClockFaceSize size;
	ClockFaceTimeOfDay timeofday;
        ClockLocation *location;
        GdkPixbuf *face_pixbuf;
        GtkWidget *size_widget;
};

#if 0
enum
{
        TIME_CHANGED,
        LAST_SIGNAL
};

static guint clock_face_signals[LAST_SIGNAL] = { 0 };
#endif

static void
clock_face_class_init (ClockFaceClass *class)
{
        GObjectClass *obj_class;
        GtkWidgetClass *widget_class;

        obj_class = G_OBJECT_CLASS (class);
        widget_class = GTK_WIDGET_CLASS (class);

        /* GtkWidget signals */
        widget_class->expose_event = clock_face_expose;
        widget_class->size_request = clock_face_size_request;
        widget_class->unmap = clock_face_unmap;

        /* GObject signals */
        obj_class->finalize = clock_face_finalize;

        g_type_class_add_private (obj_class, sizeof (ClockFacePrivate));
}

static void
clock_face_init (ClockFace *this)
{
        ClockFacePrivate *priv = CLOCK_FACE_GET_PRIVATE (this);

        priv->size = CLOCK_FACE_SMALL;
        priv->timeofday = CLOCK_FACE_DAY;
        priv->location = NULL;
        priv->size_widget = NULL;

        priv->running = TRUE;

        GTK_WIDGET_SET_FLAGS (GTK_WIDGET (this), GTK_NO_WINDOW);
}

static void
draw (GtkWidget *this, cairo_t *cr)
{
        ClockFacePrivate *priv;
        double x, y;
        double radius;
        int hours, minutes, seconds;

        /* Hand lengths as a multiple of the clock radius */
        double hour_length, min_length, sec_length;

        priv = CLOCK_FACE_GET_PRIVATE (this);

        if (priv->size == CLOCK_FACE_LARGE) {
                hour_length = 0.45;
                min_length = 0.6;
                sec_length = 0.65;
        } else {
                hour_length = 0.5;
                min_length = 0.7;
                sec_length = 0.8;   /* not drawn currently */
        }


        x = this->allocation.x + this->allocation.width / 2;
        y = this->allocation.y + this->allocation.height / 2;
        radius = MIN (this->allocation.width / 2,
                      this->allocation.height / 2) - 5;

        cairo_save (cr);
        cairo_translate (cr, this->allocation.x, this->allocation.y);

        /* clock back */
        if (priv->face_pixbuf) {
                GdkWindow *window = this->window;
                gdk_pixbuf_render_to_drawable (priv->face_pixbuf,
                                               GDK_DRAWABLE (window),
                                               NULL, 0, 0, this->allocation.x,
                                               this->allocation.y,
                                               this->allocation.width,
                                               this->allocation.height,
                                               GDK_RGB_DITHER_NONE, 0, 0);
        }

        cairo_restore (cr);

        /* clock hands */
        hours = priv->time.tm_hour;
        minutes = priv->time.tm_min + priv->minute_offset;
        seconds = priv->time.tm_sec;

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

        /* seconds hand:
         * operates identically to the minute hand
         */
        if (priv->size == CLOCK_FACE_LARGE) {
                cairo_save (cr);
                cairo_set_source_rgb (cr, 0.937, 0.161, 0.161); /* tango red */
                cairo_move_to (cr, x, y);
                cairo_line_to (cr, x + radius * sec_length * sin (M_PI / 30 * seconds),
                               y + radius * sec_length * -cos (M_PI / 30 * seconds));
                cairo_stroke (cr);
                cairo_restore (cr);
        }
}

static gboolean
clock_face_expose (GtkWidget *this, GdkEventExpose *event)
{
        cairo_t *cr;

        /* get a cairo_t */
        cr = gdk_cairo_create (this->window);

        cairo_rectangle (cr,
			 event->area.x, event->area.y,
			 event->area.width, event->area.height);
        cairo_clip (cr);

        draw (this, cr);

        cairo_destroy (cr);

        return FALSE;
}

static void
clock_face_redraw_canvas (ClockFace *this)
{
        gtk_widget_queue_draw (GTK_WIDGET (this));
}

#if 0
static void
emit_time_changed_signal (ClockFace *this, int x, int y)
{
        ClockFacePrivate *priv;
        double phi;
        int hour, minute;

        priv = CLOCK_FACE_GET_PRIVATE (this);

        /* decode the minute hand */
        /* normalise the coordinates around the origin */
        x -= GTK_WIDGET (this)->allocation.width / 2;
        y -= GTK_WIDGET (this)->allocation.height / 2;

        /* phi is a bearing from north clockwise, use the same geometry as we
         * did to position the minute hand originally */
        phi = atan2 (x, -y);
        if (phi < 0)
                phi += M_PI * 2;

        hour = priv->time.tm_hour;
        minute = phi * 30 / M_PI;

        /* update the offset */
        priv->minute_offset = minute - priv->time.tm_min;
        clock_face_redraw_canvas (this);

        g_signal_emit (this,
                        clock_face_signals[TIME_CHANGED],
                        0,
                        hour, minute);
}
#endif

static void
clock_face_size_request (GtkWidget *this,
			 GtkRequisition *requisition)
{
        ClockFacePrivate *priv = CLOCK_FACE_GET_PRIVATE (this);
        GtkRequisition req;
        int w, h;

        if (priv->face_pixbuf == NULL) {
                /* we couldn't load the svg, so use known dimensions
                 * for the svg files */
                if (priv->size == CLOCK_FACE_LARGE) {
                        requisition->width = 50;
                        requisition->height = 50;
                } else {
                        requisition->width = 36;
                        requisition->height = 36;
                }
                return;
        }

        w = gdk_pixbuf_get_width (GDK_PIXBUF (priv->face_pixbuf));
        h = gdk_pixbuf_get_height (GDK_PIXBUF (priv->face_pixbuf));

        if (priv->size_widget != NULL) {
                /* Tie our size to the height of the size_widget */
                gtk_widget_size_request (GTK_WIDGET (priv->size_widget), &req);

                /* Pad out our height by a little bit - this improves
                   the balance */
                requisition->width = req.height + req.height / 8;
                requisition->height = req.height + req.height / 8;

                if (requisition->width != w || requisition->height != h) {
                        /* resize the pixbuf */
                        clock_face_load_face (CLOCK_FACE (this),
					      requisition->width,
					      requisition->height);
                }

                return;
        }

        /* Otherwise, just use the size of the pixbuf we loaded */
        requisition->width = w;
        requisition->height = h;
}

static void
update_timeofday (ClockFace *this)
{
        ClockFacePrivate *priv;
	ClockFaceTimeOfDay timeofday;

        priv = CLOCK_FACE_GET_PRIVATE (this);

	/* FIXME  this should be a gconf setting
         * currently we hardcode
         * morning 7-9
         * day 9-17
         * evening 17-22
         * night 22-7
         */
	if (priv->time.tm_hour < 7)
		timeofday = CLOCK_FACE_NIGHT;
	else if (priv->time.tm_hour < 9)
		timeofday = CLOCK_FACE_MORNING;
	else if (priv->time.tm_hour < 17)
		timeofday = CLOCK_FACE_DAY;
	else if (priv->time.tm_hour < 22)
		timeofday = CLOCK_FACE_EVENING;
	else
		timeofday = CLOCK_FACE_NIGHT;

	if (priv->timeofday != timeofday) {
		gint width, height;

		priv->timeofday = timeofday;

		width = GTK_WIDGET (this)->requisition.width;
		height = GTK_WIDGET (this)->requisition.height;

		if (width == 0)
			width = -1;
		if (height == 0)
			height = -1;

		clock_face_load_face (this, width, height);
	}
}

gboolean
clock_face_refresh (ClockFace *this)
{
        ClockFacePrivate *priv;

        priv = CLOCK_FACE_GET_PRIVATE (this);

        if (!priv->running) {
                return FALSE;
        }

        /* update the time */
        if (priv->location) {
                clock_location_localtime (priv->location, &priv->time);
        } else {
                time_t timet;
                time (&timet);
                localtime_r (&timet, &priv->time);
        }

	update_timeofday (this);

        clock_face_redraw_canvas (this);

        return TRUE; /* keep running this event */
}

GtkWidget *
clock_face_new (ClockFaceSize size)
{
        GObject *obj = g_object_new (INTL_TYPE_CLOCK_FACE, NULL);
        ClockFacePrivate *priv = CLOCK_FACE_GET_PRIVATE (obj);

        priv->size = size;

        clock_face_load_face (CLOCK_FACE (obj), -1, -1);

        return GTK_WIDGET (obj);
}

GtkWidget *
clock_face_new_with_location (ClockFaceSize size,
			      ClockLocation *loc,
			      GtkWidget *size_widget)
{
        ClockFace *obj = CLOCK_FACE (clock_face_new (size));
        ClockFacePrivate *priv = CLOCK_FACE_GET_PRIVATE (obj);

        priv->location = g_object_ref (loc);
        priv->size_widget = g_object_ref (size_widget);

        return GTK_WIDGET (obj);
}

static void
clock_face_finalize (GObject *obj)
{
        ClockFacePrivate *priv = CLOCK_FACE_GET_PRIVATE (obj);

        if (priv->location) {
                g_object_unref (priv->location);
                priv->location = NULL;
        }

        if (priv->face_pixbuf) {
                gdk_pixbuf_unref (priv->face_pixbuf);
                priv->face_pixbuf = NULL;
        }

        if (priv->size_widget) {
                g_object_unref (priv->size_widget);
                priv->size_widget = NULL;
        }

        G_OBJECT_CLASS (clock_face_parent_class)->finalize (obj);
}

static void
clock_face_unmap (GtkWidget *this)
{
        ClockFacePrivate *priv = CLOCK_FACE_GET_PRIVATE (this);

        priv->running = FALSE;

        GTK_WIDGET_CLASS (clock_face_parent_class)->unmap (this);
}

static void
clock_face_load_face (ClockFace *this, gint width, gint height)
{
        ClockFacePrivate *priv = CLOCK_FACE_GET_PRIVATE (this);
	const gchar *size_string[2] = { "small", "large" };
        const gchar *daytime_string[4] = { "morning", "day", "evening", "night" };
	gchar *name;

        if (priv->face_pixbuf != NULL) {
                gdk_pixbuf_unref (priv->face_pixbuf);
                priv->face_pixbuf = NULL;
        }

	name = g_strconcat (ICONDIR, "/clock-face-", size_string[priv->size], "-", daytime_string[priv->timeofday], ".svg", NULL);

	priv->face_pixbuf = rsvg_pixbuf_from_file_at_size (name, width, height, NULL);
	g_free (name);

	if (priv->face_pixbuf)
		return;

	name = g_strconcat (ICONDIR, "/clock-face-", size_string[priv->size], ".svg", NULL);
	priv->face_pixbuf = rsvg_pixbuf_from_file_at_size (name, width, height, NULL);
	g_free (name);
}
