#include <gtk/gtk.h>
#include <string.h>

#include "free-the-fish.h"

/* free the fish code */
#define FISH_FRAMES 8
#define FISH_ORIG_WIDTH 288
#define FISH_ORIG_HEIGHT 22
#define FISH_WIDTH (FISH_ORIG_WIDTH/FISH_FRAMES)
#define FISH_HEIGHT FISH_ORIG_HEIGHT
#define FISH_CHECK_TIMEOUT (g_random_int()%120*1000)
#define FISH_TIMEOUT 120
#define FISH_HIDE_TIMEOUT 80
#define FISH_XS 5
#define FISH_YS ((g_random_int() % 2) + 1)
#define FISH_XS_HIDE_FACTOR 2.5
#define FISH_YS_HIDE_FACTOR 2.5
#define FISH_PIXEL_STORE_MOVE(a, r, g, b) (a < 128 || (a < 255 && b > r && b > g))

/* Some important code copied from PonG */
typedef struct _FreeTheFish FreeTheFish;
struct _FreeTheFish {
        GdkWindow *win;
        gboolean hide_mode;
        int state;
        int x, y, xs, ys;
        guint handler;
        cairo_pattern_t *fish_pattern[FISH_FRAMES];
        cairo_pattern_t *fish_pattern_reverse[FISH_FRAMES];
        cairo_region_t *fish_shape;
        cairo_region_t *fish_shape_reverse;
};
static FreeTheFish fish = {NULL};

static void
fish_kill (void)
{
        int i;

        for (i = 0; i < FISH_FRAMES; i++) {
                if (fish.fish_pattern[i] != NULL)
                        cairo_pattern_destroy (fish.fish_pattern[i]);
                fish.fish_pattern[i] = NULL;
                if (fish.fish_pattern_reverse[i] != NULL)
                        cairo_pattern_destroy (fish.fish_pattern_reverse[i]);
                fish.fish_pattern_reverse[i] = NULL;
        }

        if (fish.fish_shape != NULL)
                cairo_region_destroy (fish.fish_shape);
        fish.fish_shape = NULL;

        if (fish.fish_shape_reverse != NULL)
                cairo_region_destroy (fish.fish_shape_reverse);
        fish.fish_shape_reverse = NULL;

        gdk_window_destroy (fish.win);

        g_source_remove (fish.handler);

        memset (&fish, 0, sizeof (FreeTheFish));

        gdk_event_handler_set ((GdkEventFunc)gtk_main_do_event, NULL, NULL);
}

static void
fish_draw (int orient, int frame)
{
        cairo_pattern_t *pattern;
        cairo_region_t *region;
        cairo_t *cr;
        int shape_offset;

        pattern = orient ? fish.fish_pattern[frame] : fish.fish_pattern_reverse[frame];
        region = orient ? fish.fish_shape : fish.fish_shape_reverse;
        shape_offset = orient ? -frame : frame + 1 - FISH_FRAMES;

        cr = gdk_cairo_create (fish.win);
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source (cr, pattern);
        cairo_paint (cr);
        cairo_destroy (cr);

        /* Yes. Don't ask. */
        gdk_window_set_background_pattern (fish.win, pattern);

        gdk_window_shape_combine_region (fish.win, region,
                                         shape_offset * FISH_WIDTH, 0);
}

static gboolean
fish_move (gpointer data)
{
        int orient, frame;
        gboolean change = TRUE;

        fish.x += fish.xs;
        fish.y += fish.ys;
        if (fish.x <= -FISH_WIDTH ||
            fish.x >= gdk_screen_width ()) {
                fish_kill ();
                return FALSE;
        }
        if (fish.y <= 0 ||
            fish.y >= gdk_screen_height () - FISH_HEIGHT ||
            g_random_int() % (fish.hide_mode?10:50) == 0)
                fish.ys = -fish.ys;

        fish.state ++;
        if (fish.hide_mode) {
                fish.state ++;
                if (fish.state >= 2* FISH_FRAMES)
                        fish.state = 0;
                if (fish.state % 2 == 0)
                        change = TRUE;
        } else if (fish.state >= FISH_FRAMES) {
                fish.state = 0;
                change = TRUE;
        }

        frame = fish.state / (fish.hide_mode?2:1);
        orient = fish.xs >= 0 ? 0 : 1;

        if (change)
                fish_draw (orient, frame);

        gdk_window_move (fish.win, fish.x, fish.y);
        gdk_window_raise (fish.win);

        return TRUE;
}

static void
fish_handle_event (GdkEvent *event)
{
        if (event->any.window != fish.win)
                goto out;

        if (fish.hide_mode)
                goto out;

        switch (event->type) {
                case GDK_SCROLL:
                case GDK_BUTTON_PRESS:
                case GDK_2BUTTON_PRESS:
                case GDK_3BUTTON_PRESS:
                        g_source_remove (fish.handler);
                        fish.handler = g_timeout_add (FISH_HIDE_TIMEOUT,
                                                      fish_move, NULL);
                        fish.xs *= FISH_XS_HIDE_FACTOR;
                        fish.ys *= FISH_YS_HIDE_FACTOR;
                        fish.hide_mode = TRUE;
                        if (fish.xs > 0) {
                                if (fish.x < (gdk_screen_width () / 2))
                                        fish.xs *= -1;
                        } else {
                                if (fish.x > (gdk_screen_width () / 2))
                                        fish.xs *= -1;
                        }
                        break;
                default:
                        break;
        }

out:
        gtk_main_do_event (event);
}

static void
fish_unsea (cairo_surface_t *surface)
{
        guchar *pixels = cairo_image_surface_get_data (surface);
        int rs = cairo_image_surface_get_stride (surface);
        int w = cairo_image_surface_get_width (surface);
        int h = cairo_image_surface_get_height (surface);
        int x, y;
        guint32 *p;
        guchar a, r, g, b;

        for (y = 0; y < h; y++, pixels += rs) {
                p = (guint32 *) pixels;
                for (x = 0; x < w; x++, p++) {
                        a = ((*p)>>24);
                        r = ((*p)>>16)&0xff;
                        g = ((*p)>> 8)&0xff;
                        b = ((*p)>> 0)&0xff;
                        if (FISH_PIXEL_STORE_MOVE(a, r, g, b))
                                *p = 0;
                }
        }
}

static cairo_pattern_t *
get_fish_frame (GdkWindow *win, cairo_surface_t *source, int frame)
{
        cairo_pattern_t *pattern;
        cairo_surface_t *surface;
        cairo_t *cr;

        /* We need an Xlib surface for gdk_window_set_background_pattern() */
        surface = gdk_window_create_similar_surface (win,
                                                     CAIRO_CONTENT_COLOR_ALPHA,
                                                     FISH_WIDTH, FISH_HEIGHT);

        cr = cairo_create (surface);
        cairo_set_source_surface (cr, source, - frame * FISH_WIDTH, 0);
        cairo_paint (cr);
        cairo_destroy (cr);

        pattern = cairo_pattern_create_for_surface (surface);
        cairo_surface_destroy (surface);

        return pattern;
}

static cairo_pattern_t *
get_fish_frame_reverse (GdkWindow *win, cairo_surface_t *source, int frame)
{
        cairo_pattern_t *pattern;
        cairo_surface_t *surface;
        cairo_t *cr;
        cairo_matrix_t matrix;

        /* We need an Xlib surface for gdk_window_set_background_pattern() */
        surface = gdk_window_create_similar_surface (win,
                                                     CAIRO_CONTENT_COLOR_ALPHA,
                                                     FISH_WIDTH, FISH_HEIGHT);

        cr = cairo_create (surface);
        cairo_matrix_init_identity (&matrix);
        cairo_matrix_scale (&matrix, -1.0, 1.0);
        cairo_set_matrix (cr, &matrix);
        cairo_set_source_surface (cr, source, - (frame + 1) * FISH_WIDTH, 0);
        cairo_paint (cr);
        cairo_destroy (cr);

        pattern = cairo_pattern_create_for_surface (surface);
        cairo_surface_destroy (surface);

        return pattern;
}

static cairo_region_t *
get_fish_shape (cairo_surface_t *surface)
{
        return gdk_cairo_region_create_from_surface (surface);
}

static cairo_region_t *
get_fish_shape_reverse (cairo_surface_t *surface)
{
        cairo_region_t *region;
        cairo_surface_t *surface_reverse;
        cairo_t *cr;
        cairo_matrix_t matrix;

        surface_reverse = cairo_surface_create_similar (surface,
                                                        CAIRO_CONTENT_COLOR_ALPHA,
                                                        FISH_ORIG_WIDTH,
                                                        FISH_ORIG_HEIGHT);

        cr = cairo_create (surface_reverse);
        cairo_matrix_init_identity (&matrix);
        cairo_matrix_scale (&matrix, -1.0, 1.0);
        cairo_set_matrix (cr, &matrix);
        cairo_set_source_surface (cr, surface, -FISH_ORIG_WIDTH, 0);
        cairo_paint (cr);
        cairo_destroy (cr);

        region = gdk_cairo_region_create_from_surface (surface_reverse);
        cairo_surface_destroy (surface_reverse);

        return region;
}

/* this checks the screen */
static void
check_screen (void)
{
        GdkWindowAttr attributes;
        GdkPixbuf *fish_pixbuf;
        int width, height;
        cairo_surface_t *surface;
        cairo_t *cr;
        int orient;
        int i;

        if (fish.win != NULL)
                return;

        fish_pixbuf = gdk_pixbuf_new_from_resource ("/org/gnome/panel/anim/wanda.png", NULL);

        if (fish_pixbuf == NULL)
                return;

        width = gdk_pixbuf_get_width (fish_pixbuf);
        height = gdk_pixbuf_get_height (fish_pixbuf);

        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
        cr = cairo_create (surface);
        gdk_cairo_set_source_pixbuf (cr, fish_pixbuf, 0, 0);
        cairo_rectangle (cr, 0, 0, width, height);
        cairo_fill (cr);
        cairo_destroy (cr);

        g_object_unref (fish_pixbuf);

        if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
                cairo_surface_destroy (surface);
                return;
        }

        if (cairo_image_surface_get_width (surface) != FISH_ORIG_WIDTH ||
            cairo_image_surface_get_height (surface) != FISH_ORIG_HEIGHT) {
                cairo_surface_destroy (surface);
                return;
        }

        fish_unsea (surface);

        orient = g_random_int() % 2;

        fish.state = 0;
        fish.hide_mode = FALSE;
        fish.x = orient ? -FISH_WIDTH : gdk_screen_width ();
        fish.y = (g_random_int() % (gdk_screen_height() - FISH_HEIGHT - 2)) + 1;
        fish.xs = orient ? FISH_XS : -FISH_XS;
        fish.ys = FISH_YS;

        attributes.window_type = GDK_WINDOW_TEMP;
        attributes.x = fish.x;
        attributes.y = fish.y;
        attributes.width = FISH_WIDTH;
        attributes.height = FISH_HEIGHT;
        attributes.wclass = GDK_INPUT_OUTPUT;
        attributes.event_mask = GDK_BUTTON_PRESS_MASK;

        fish.win = gdk_window_new (NULL, &attributes,
                                   GDK_WA_X | GDK_WA_Y);

        for (i = 0; i < FISH_FRAMES; i++) {
                fish.fish_pattern[i] = get_fish_frame (fish.win, surface, i);
                fish.fish_pattern_reverse[i] = get_fish_frame_reverse (fish.win, surface, i);
        }

        fish.fish_shape = get_fish_shape (surface);
        fish.fish_shape_reverse = get_fish_shape_reverse (surface);

        cairo_surface_destroy (surface);

        fish_draw (0, 0);
        gdk_window_show (fish.win);

        gdk_event_handler_set ((GdkEventFunc)fish_handle_event, NULL, NULL);

        fish.handler = g_timeout_add (FISH_TIMEOUT, fish_move, NULL);
}

static guint screen_check_id = 0;

static gboolean
check_screen_timeout (gpointer data)
{
        screen_check_id = 0;

        check_screen ();

        screen_check_id = g_timeout_add (FISH_CHECK_TIMEOUT,
                                         check_screen_timeout, NULL);
        return FALSE;
}

void
free_the_fish (void)
{
        if (screen_check_id > 0)
                g_source_remove (screen_check_id);

        check_screen_timeout (NULL);
}
