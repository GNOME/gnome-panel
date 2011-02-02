#include <X11/Xlib.h>

#include "nothing.h"

/* phish code */
#define PHSHFRAMES 8
#define PHSHORIGWIDTH 288
#define PHSHORIGHEIGHT 22
#define PHSHWIDTH (PHSHORIGWIDTH/PHSHFRAMES)
#define PHSHHEIGHT PHSHORIGHEIGHT
#define PHSHCHECKTIMEOUT (g_random_int()%120*1000)
#define PHSHTIMEOUT 120
#define PHSHHIDETIMEOUT 80
#define PHSHXS 5
#define PHSHYS ((g_random_int() % 2) + 1)
#define PHSHXSHIDEFACTOR 2.5
#define PHSHYSHIDEFACTOR 2.5
#define PHSHPIXELSTOREMOVE(a, r, g, b) (a < 128 || (a < 255 && b > r && b > g))

/* Some important code copied from PonG */
typedef struct _AppletContainer AppletContainer;
struct _AppletContainer {
        GdkWindow *win;
        gboolean hide_mode;
        int state;
        int x, y, xs, ys;
        int handler;
        cairo_pattern_t *phsh[PHSHFRAMES];
        cairo_pattern_t *phsh_reverse[PHSHFRAMES];
        cairo_region_t *phsh_shape;
        cairo_region_t *phsh_shape_reverse;
};
static AppletContainer phsh = {NULL};

static void
phsh_kill (void)
{
        int i;

        for (i = 0; i < PHSHFRAMES; i++) {
                if (phsh.phsh[i] != NULL)
                        cairo_pattern_destroy (phsh.phsh[i]);
                phsh.phsh[i] = NULL;
                if (phsh.phsh_reverse[i] != NULL)
                        cairo_pattern_destroy (phsh.phsh_reverse[i]);
                phsh.phsh_reverse[i] = NULL;
        }

        if (phsh.phsh_shape != NULL)
                cairo_region_destroy (phsh.phsh_shape);
        phsh.phsh_shape = NULL;

        if (phsh.phsh_shape_reverse != NULL)
                cairo_region_destroy (phsh.phsh_shape_reverse);
        phsh.phsh_shape_reverse = NULL;

        gdk_window_destroy (phsh.win);

        g_source_remove (phsh.handler);

        memset (&phsh, 0, sizeof (AppletContainer));

        gdk_event_handler_set ((GdkEventFunc)gtk_main_do_event, NULL, NULL);
}

static void
phsh_draw (int orient, int frame)
{
        cairo_pattern_t *pattern;
        cairo_region_t *region;
        cairo_t *cr;
        int shape_offset;

        pattern = orient ? phsh.phsh[frame] : phsh.phsh_reverse[frame];
        region = orient ? phsh.phsh_shape : phsh.phsh_shape_reverse;
        shape_offset = orient ? -frame : frame + 1 - PHSHFRAMES;

        cr = gdk_cairo_create (phsh.win);
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source (cr, pattern);
        cairo_paint (cr);
        cairo_destroy (cr);

        /* Yes. Don't ask. */
        gdk_window_set_background_pattern (phsh.win, pattern);

        gdk_window_shape_combine_region (phsh.win, region,
                                         shape_offset * PHSHWIDTH, 0);
}

static gboolean
phsh_move (gpointer data)
{
        int orient, frame;
        gboolean change = TRUE;

        phsh.x += phsh.xs;
        phsh.y += phsh.ys;
        if (phsh.x <= -PHSHWIDTH ||
            phsh.x >= gdk_screen_width ()) {
                phsh_kill ();
                return FALSE;
        }
        if (phsh.y <= 0 ||
            phsh.y >= gdk_screen_height () - PHSHHEIGHT ||
            g_random_int() % (phsh.hide_mode?10:50) == 0)
                phsh.ys = -phsh.ys;

        phsh.state ++;
        if (phsh.hide_mode) {
                phsh.state ++;
                if (phsh.state >= 2*PHSHFRAMES)
                        phsh.state = 0;
                if (phsh.state % 2 == 0)
                        change = TRUE;
        } else if (phsh.state >= PHSHFRAMES) {
                phsh.state = 0;
                change = TRUE;
        }

        frame = phsh.state / (phsh.hide_mode?2:1);
        orient = phsh.xs >= 0 ? 0 : 1;

        if (change)
                phsh_draw (orient, frame);

        gdk_window_move (phsh.win, phsh.x, phsh.y);
        gdk_window_raise (phsh.win);

        return TRUE;
}

static void
phsh_handle_event (GdkEvent *event)
{
        if (event->any.window != phsh.win)
                goto out;

        if (phsh.hide_mode)
                goto out;

        switch (event->type) {
                case GDK_SCROLL:
                case GDK_BUTTON_PRESS:
                case GDK_2BUTTON_PRESS:
                case GDK_3BUTTON_PRESS:
                        g_source_remove (phsh.handler);
                        phsh.handler = g_timeout_add (PHSHHIDETIMEOUT,
                                                      phsh_move, NULL);
                        phsh.xs *= PHSHXSHIDEFACTOR;
                        phsh.ys *= PHSHYSHIDEFACTOR;
                        phsh.hide_mode = TRUE;
                        if (phsh.xs > 0) {
                                if (phsh.x < (gdk_screen_width () / 2))
                                        phsh.xs *= -1;
                        } else {
                                if (phsh.x > (gdk_screen_width () / 2))
                                        phsh.xs *= -1;
                        }
                        break;
                default:
                        break;
        }

out:
        gtk_main_do_event (event);
}

static char *
get_phsh_file (void)
{
        return g_strdup_printf ("%s/%cand%c.png", ICONDIR, 'w', 'a');
}

static void
phsh_unsea (cairo_surface_t *surface)
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
                        if (PHSHPIXELSTOREMOVE(a, r, g, b))
                                *p = 0;
                }
        }
}

static cairo_pattern_t *
get_phsh_frame (GdkWindow *win, cairo_surface_t *source, int frame)
{
        cairo_pattern_t *pattern;
        cairo_surface_t *surface;
        cairo_t *cr;

        /* We need an Xlib surface for gdk_window_set_background_pattern() */
        surface = gdk_window_create_similar_surface (win,
                                                     CAIRO_CONTENT_COLOR_ALPHA,
                                                     PHSHWIDTH, PHSHHEIGHT);

        cr = cairo_create (surface);
        cairo_set_source_surface (cr, source, - frame * PHSHWIDTH, 0);
        cairo_paint (cr);
        cairo_destroy (cr);

        pattern = cairo_pattern_create_for_surface (surface);
        cairo_surface_destroy (surface);

        return pattern;
}

static cairo_pattern_t *
get_phsh_frame_reverse (GdkWindow *win, cairo_surface_t *source, int frame)
{
        cairo_pattern_t *pattern;
        cairo_surface_t *surface;
        cairo_t *cr;
        cairo_matrix_t matrix;

        /* We need an Xlib surface for gdk_window_set_background_pattern() */
        surface = gdk_window_create_similar_surface (win,
                                                     CAIRO_CONTENT_COLOR_ALPHA,
                                                     PHSHWIDTH, PHSHHEIGHT);

        cr = cairo_create (surface);
        cairo_matrix_init_identity (&matrix);
        cairo_matrix_scale (&matrix, -1.0, 1.0);
        cairo_set_matrix (cr, &matrix);
        cairo_set_source_surface (cr, source, - (frame + 1) * PHSHWIDTH, 0);
        cairo_paint (cr);
        cairo_destroy (cr);

        pattern = cairo_pattern_create_for_surface (surface);
        cairo_surface_destroy (surface);

        return pattern;
}

static cairo_region_t *
get_phsh_shape (cairo_surface_t *surface)
{
        return gdk_cairo_region_create_from_surface (surface);
}

static cairo_region_t *
get_phsh_shape_reverse (cairo_surface_t *surface)
{
        cairo_region_t *region;
        cairo_surface_t *surface_reverse;
        cairo_t *cr;
        cairo_matrix_t matrix;

        surface_reverse = cairo_surface_create_similar (surface,
                                                        CAIRO_CONTENT_COLOR_ALPHA,
                                                        PHSHORIGWIDTH,
                                                        PHSHORIGHEIGHT);

        cr = cairo_create (surface_reverse);
        cairo_matrix_init_identity (&matrix);
        cairo_matrix_scale (&matrix, -1.0, 1.0);
        cairo_set_matrix (cr, &matrix);
        cairo_set_source_surface (cr, surface, -PHSHORIGWIDTH, 0);
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
        char *phsh_file;
        cairo_surface_t *surface;
        int orient;
        int i;

        if (phsh.win != NULL)
                return;

        phsh_file = get_phsh_file ();

        if (phsh_file == NULL)
                return;

        surface = cairo_image_surface_create_from_png (phsh_file);
        g_free (phsh_file);

        if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
                cairo_surface_destroy (surface);
                return;
        }

        if (cairo_image_surface_get_width (surface) != PHSHORIGWIDTH ||
            cairo_image_surface_get_height (surface) != PHSHORIGHEIGHT) {
                cairo_surface_destroy (surface);
                return;
        }

        phsh_unsea (surface);

        orient = g_random_int() % 2;

        phsh.state = 0;
        phsh.hide_mode = FALSE;
        phsh.x = orient ? -PHSHWIDTH : gdk_screen_width ();
        phsh.y = (g_random_int() % (gdk_screen_height() - PHSHHEIGHT - 2)) + 1;
        phsh.xs = orient ? PHSHXS : -PHSHXS;
        phsh.ys = PHSHYS;

        attributes.window_type = GDK_WINDOW_TEMP;
        attributes.x = phsh.x;
        attributes.y = phsh.y;
        attributes.width = PHSHWIDTH;
        attributes.height = PHSHHEIGHT;
        attributes.wclass = GDK_INPUT_OUTPUT;
        attributes.event_mask = GDK_BUTTON_PRESS_MASK;

        phsh.win = gdk_window_new (NULL, &attributes,
                                   GDK_WA_X | GDK_WA_Y);

        for (i = 0; i < PHSHFRAMES; i++) {
                phsh.phsh[i] = get_phsh_frame (phsh.win, surface, i);
                phsh.phsh_reverse[i] = get_phsh_frame_reverse (phsh.win, surface, i);
        }

        phsh.phsh_shape = get_phsh_shape (surface);
        phsh.phsh_shape_reverse = get_phsh_shape_reverse (surface);

        cairo_surface_destroy (surface);

        phsh_draw (0, 0);
        gdk_window_show (phsh.win);

        gdk_event_handler_set ((GdkEventFunc)phsh_handle_event, NULL, NULL);

        phsh.handler = g_timeout_add (PHSHTIMEOUT, phsh_move, NULL);
}

static guint screen_check_id = 0;

static gboolean
check_screen_timeout (gpointer data)
{
        screen_check_id = 0;

        check_screen ();

        screen_check_id = g_timeout_add (PHSHCHECKTIMEOUT,
                                         check_screen_timeout, NULL);
        return FALSE;
}

void
start_screen_check (void)
{
        if (screen_check_id > 0)
                g_source_remove (screen_check_id);

        check_screen_timeout (NULL);
}
