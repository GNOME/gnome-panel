#include <string.h>
#include <libgnomecanvas/libgnomecanvas.h>
#include "panel-util.h"
#include "nothing.h"

/*code snippet, not to be compiled separately*/
static int goat_pages=0;
static int goat_frame=0;
static GdkPixmap *goat_pix[2] = {NULL,NULL};
static GdkPixmap *goat_pix_rev[2] = {NULL,NULL};
static GtkWidget *goat_darea = NULL;
static int goat_width = 0,goat_height = 0;
static int goat_timeout = 0;
static int goat_x = -1, goat_y = -1;
static int goat_accx = -1, goat_accy = -1;

static char *goat_filename = NULL;

static void
destroy_egg(GtkWidget *widget, gpointer data)
{
	goat_pages = 0;
	if(goat_timeout) {
		gtk_timeout_remove(goat_timeout);
		goat_timeout = 0;
	}
	goat_x = goat_y = -1;
}

static int
goat_timeout_func(gpointer data)
{
	int real_goat_frame;

	if(!GTK_WIDGET_REALIZED(goat_darea) ||
	   !GTK_WIDGET_DRAWABLE(goat_darea) ||
	   !goat_pix[0])
		return TRUE;
	
	if(goat_x == -1) {
		goat_x = 6;
		goat_y = 6;
		gdk_draw_rectangle(goat_darea->window,
				   goat_darea->style->white_gc,
				   TRUE, 0,0,-1,-1);
		goat_accx = rand()%4 +3;
		goat_accy = rand()%4 +3;
	}
	
	goat_x += goat_accx;
	goat_y += goat_accy;

	if(goat_x>goat_darea->allocation.width-6-goat_width) {
		goat_accx = -(rand()%4 +3);
		goat_x = goat_darea->allocation.width-6-goat_width;
		if (goat_filename != NULL)
			gnome_sound_play (goat_filename);
	} else if(goat_x<6) {
		goat_accx = rand()%4 +3;
		goat_x = 6;
		if (goat_filename != NULL)
			gnome_sound_play (goat_filename);
	}
	if(goat_y>goat_darea->allocation.height-6-goat_height) {
		goat_accy = -(rand()%4 +3);
		goat_y = goat_darea->allocation.height-6-goat_height;
		if (goat_filename != NULL)
			gnome_sound_play (goat_filename);
	} else if(goat_y<6) {
		goat_accy = rand()%4 +3;
		goat_y = 6;
		if (goat_filename != NULL)
			gnome_sound_play (goat_filename);
	}

	real_goat_frame = goat_frame/2;
	
	gdk_draw_drawable (goat_darea->window,
			   goat_darea->style->white_gc,
			   goat_accx>0?goat_pix_rev[real_goat_frame]:
			   goat_pix[real_goat_frame],
			   0,0,goat_x,goat_y,
			   goat_width,goat_height);

	if(++goat_frame == 4)
		goat_frame = 0;

	return TRUE;
}

static int
goat_expose(GtkWidget *widget, GdkEventExpose *event)
{
	if(!GTK_WIDGET_DRAWABLE(widget))
		return FALSE;

	gdk_draw_rectangle(goat_darea->window,
			   goat_darea->style->white_gc,
			   TRUE, event->area.x, event->area.y,
			   event->area.width,event->area.height);
	return FALSE;
}

static void
goat_realize(GtkWidget *widget)
{
	int frame;
	char *files[] = {
		"gnome-gegl2.png",
		"gnome-gegl2-2.png"
	};
	if(goat_pix[0])
		return;
	for(frame=0;frame<2;frame++) {
		GdkPixbuf *pb;
		guchar *rgb;
		GdkGC *gc;
		int i,j;
		gdouble affine[6];
		char *file;

		file = panel_pixmap_discovery (files[frame], FALSE /* fallback */);
		if(!file || 
		   !(pb=gdk_pixbuf_new_from_file (file, NULL))) {
			g_warning("Goat is not available!");
			return;
		}
		g_free(file);

		goat_width = gdk_pixbuf_get_width(pb)+12;
		goat_height = gdk_pixbuf_get_height(pb)+12;
		rgb = g_new(guchar,goat_width*goat_height*3);
		for(i=0;i<goat_width*goat_height*3;i++)
			rgb[i] = 0xFF;

		art_affine_translate(affine,6,6);
		transform_pixbuf(rgb,
			         0,0,goat_width,goat_height,goat_width*3,
			         pb,affine,ART_FILTER_NEAREST,NULL);
		gdk_pixbuf_unref(pb);

		goat_pix[frame] = gdk_pixmap_new(widget->window,
						 goat_width,goat_height,
						 gtk_widget_get_visual(GTK_WIDGET(widget))->depth);
		gc = gdk_gc_new(goat_pix[frame]);
		gdk_draw_rgb_image(goat_pix[frame],gc,0,0,
				   goat_width,goat_height,
				   GDK_RGB_DITHER_NORMAL,
				   rgb, goat_width*3);
		gdk_gc_destroy(gc);

#define GUCHARSWAP(x,y) { guchar tmp=(x); (x)=(y); (y)=tmp; }
		for(i=0;i<goat_width/2;i++)
			for(j=0;j<goat_height;j++) {
				GUCHARSWAP(*(rgb+(j*goat_width*3)+(i*3)),
					*(rgb+(j*goat_width*3)+((goat_width-i-1)*3)))
				GUCHARSWAP(*(rgb+(j*goat_width*3)+(i*3)+1),
					*(rgb+(j*goat_width*3)+((goat_width-i-1)*3)+1))
				GUCHARSWAP(*(rgb+(j*goat_width*3)+(i*3)+2),
					*(rgb+(j*goat_width*3)+((goat_width-i-1)*3)+2))
			}
#undef GUCHARSWAP

		goat_pix_rev[frame] = gdk_pixmap_new(widget->window,
					  goat_width,goat_height,
					  gtk_widget_get_visual(GTK_WIDGET(widget))->depth);
		gc = gdk_gc_new(goat_pix_rev[frame]);
		gdk_draw_rgb_image(goat_pix_rev[frame],gc,0,0,
				   goat_width,goat_height,
				   GDK_RGB_DITHER_NORMAL,
				   rgb, goat_width*3);

		gdk_gc_destroy(gc);
		g_free(rgb);
	}
}

/*thy evil easter egg*/
static int
config_event(GtkWidget *widget,GdkEvent *event,GtkNotebook *nbook)
{
	static int clicks=0;
	GdkEventButton *bevent;
	
	if(event->type != GDK_BUTTON_PRESS)
		return FALSE;
	
	bevent = (GdkEventButton *)event;
	if(bevent->button != 3)
		clicks = 0;
	else
		clicks++;
	
	if(clicks<3)
		return FALSE;
	clicks = 0;
	
	if(goat_pages==0) {
		if (goat_filename == NULL)
			goat_filename =
				gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_SOUND, 
							   "ricochet.wav",
							   TRUE /* only_if_exists */,
							   NULL /* ret_locations */);

		goat_darea = gtk_drawing_area_new();

		g_signal_connect_after (G_OBJECT(goat_darea),"realize",
					G_CALLBACK (goat_realize),NULL);
		
		gtk_widget_show(goat_darea);
		goat_timeout = gtk_timeout_add(60,goat_timeout_func,NULL);
		/*the GEGL shall not be translated*/
		gtk_notebook_append_page (nbook, goat_darea,
					  gtk_label_new ("GEGL"));
		gtk_notebook_set_current_page(nbook,-1);
		goat_pages = 1;
		g_signal_connect (G_OBJECT(goat_darea),"destroy",
				  G_CALLBACK (destroy_egg),NULL);
		g_signal_connect (GTK_OBJECT(goat_darea),"expose_event",
				  G_CALLBACK (goat_expose),NULL);
	} else {
		gtk_notebook_set_current_page(nbook,-1);
	}
	return FALSE;
}

/* phish code */

/* Some important code copied from PonG */
typedef struct _AppletContainer AppletContainer;
struct _AppletContainer {
        GdkWindow *win;
        gboolean hide_mode;
        int state;
        int x, y, xs, ys;
        int handler;
        GdkPixmap *phsh[4];
        GdkBitmap *phsh_mask[4];
};
AppletContainer phsh = {0};

static void
phsh_kill (void)
{
        int i;
        for (i = 0; i < 4; i++) {
                gdk_pixmap_unref (phsh.phsh[i]);
                gdk_bitmap_unref (phsh.phsh_mask[i]);
        }
        gdk_window_destroy (phsh.win);
        gtk_timeout_remove (phsh.handler);
        memset (&phsh, 0, sizeof (AppletContainer));
}

static gboolean
phsh_move (gpointer data)
{
        int orient, state;
        gboolean change = TRUE;

        phsh.x += phsh.xs;
        phsh.y += phsh.ys;
        if (phsh.x <= -60 ||
            phsh.x >= gdk_screen_width ()) {
                phsh_kill ();
                return FALSE;
        }
        if (phsh.y <= 0 ||
            phsh.y >= gdk_screen_height () - 40 ||
            rand() % (phsh.hide_mode?10:50) == 0)
                phsh.ys = -phsh.ys;

        phsh.state ++;
        if (phsh.state % (phsh.hide_mode?2:4) == 0)
                change = TRUE;
        if (phsh.state >= (phsh.hide_mode?4:8))
                phsh.state = 0;

        state = phsh.state >= (phsh.hide_mode?2:4) ? 1 : 0;
        orient = phsh.xs >= 0 ? 0 : 2;

        if (change) {
                gdk_window_set_back_pixmap (phsh.win, phsh.phsh[orient + state], FALSE);
                gdk_window_shape_combine_mask (phsh.win, phsh.phsh_mask[orient + state], 0, 0);
                gdk_window_clear (phsh.win);
        }

        gdk_window_move (phsh.win, phsh.x, phsh.y);
        gdk_window_raise (phsh.win);

        return TRUE;
}

static void
phsh_reverse (GdkPixbuf *gp)
{
        guchar *pixels = gdk_pixbuf_get_pixels (gp);
        int x, y;
        int rs = gdk_pixbuf_get_rowstride (gp);
#define DOSWAP(x,y) tmp = x; x = y; y = tmp;
        for (y = 0; y < 40; y++, pixels += rs) {
                guchar *p = pixels;
                guchar *p2 = pixels + 60*4 - 4;
                for (x = 0; x < 30; x++, p+=4, p2-=4) {
                        guchar tmp;
                        DOSWAP (p[0], p2[0]);
                        DOSWAP (p[1], p2[1]);
                        DOSWAP (p[2], p2[2]);
                        DOSWAP (p[3], p2[3]);
                }
        }
#undef DOSWAP
}

/* This dered's the phsh */
static void
phsh_dered(GdkPixbuf *gp)
{
        guchar *pixels = gdk_pixbuf_get_pixels (gp);
        int x, y;
        int rs = gdk_pixbuf_get_rowstride (gp);
        for (y = 0; y < 40; y++, pixels += rs) {
                guchar *p = pixels;
                for (x = 0; x < 60; x++, p+=4) {
                        if (p[0] < 55 && p[1] > 100)
                               p[3] = 0;
                }
        }
}

static GdkFilterReturn
phsh_filter (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
        XEvent *xevent = (XEvent *)gdk_xevent;

        if (xevent->type == ButtonPress &&
            ! phsh.hide_mode) {
                gtk_timeout_remove (phsh.handler);
                phsh.handler = gtk_timeout_add (90, phsh_move, NULL);
                phsh.xs *= 2.0;
                phsh.ys *= 2.5;
                phsh.hide_mode = TRUE;
                if (phsh.x < (gdk_screen_width () / 2))
                        phsh.xs *= -1;
        }
        return GDK_FILTER_CONTINUE;
}

static char *
get_phsh_file (void)
{
        char *name;
        char *phsh_file;

        name = g_strdup_printf ("%cish/%cishanim.png",
                                'f', 'f');

        phsh_file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
                                               name, TRUE, NULL);

        g_free (name);

	return phsh_file;
}

static GdkPixbuf *
get_phsh_frame (GdkPixbuf *pb, int frame)
{
        GdkPixbuf *newpb;

        newpb = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 60, 40);
        gdk_pixbuf_copy_area (pb, frame * 60, 0, 60, 40, newpb, 0, 0);

	return newpb;
}

/* this checks the screen */
static void
check_screen (void)
{
        GdkWindowAttr attributes;
        char *phsh_file;
        GdkPixbuf *gp, *tmp;

        if (phsh.win != NULL)
                return;

	phsh_file = get_phsh_file ();

        if (phsh_file == NULL)
                return;

        tmp = gdk_pixbuf_new_from_file (phsh_file, NULL);
        if (tmp == NULL)
                return;

        g_free (phsh_file);

        if (gdk_pixbuf_get_width (tmp) != 180 ||
            gdk_pixbuf_get_height (tmp) != 40) {
                gdk_pixbuf_unref (tmp);
                return;
        }

        phsh.state = 0;
        phsh.hide_mode = FALSE;

	gp = get_phsh_frame (tmp, 1);

        phsh_dered (gp);
        gdk_pixbuf_render_pixmap_and_mask (gp, &phsh.phsh[2], &phsh.phsh_mask[2], 128);
        phsh_reverse (gp);
        gdk_pixbuf_render_pixmap_and_mask (gp, &phsh.phsh[0], &phsh.phsh_mask[0], 128);

        gdk_pixbuf_copy_area (tmp, 120, 0, 60, 40, gp, 0, 0);

        phsh_dered (gp);
        gdk_pixbuf_render_pixmap_and_mask (gp, &phsh.phsh[3], &phsh.phsh_mask[3], 128);
        phsh_reverse (gp);
        gdk_pixbuf_render_pixmap_and_mask (gp, &phsh.phsh[1], &phsh.phsh_mask[1], 128);
        gdk_pixbuf_unref (gp);

        gdk_pixbuf_unref (tmp);

        phsh.x = -60;
        phsh.y = (rand() % (gdk_screen_height () - 40 - 2)) + 1;
        phsh.xs = 8;
        phsh.ys = (rand() % 2) + 1;

        attributes.window_type = GDK_WINDOW_TEMP;
        attributes.x = phsh.x;
        attributes.y = phsh.y;
        attributes.width = 60;
        attributes.height = 40;
        attributes.wclass = GDK_INPUT_OUTPUT;
        attributes.visual = gdk_rgb_get_visual();
        attributes.colormap = gdk_rgb_get_cmap();
        attributes.event_mask = GDK_BUTTON_PRESS_MASK;

        phsh.win = gdk_window_new (NULL, &attributes,
                                   GDK_WA_X | GDK_WA_Y |
                                   GDK_WA_VISUAL | GDK_WA_COLORMAP);
        gdk_window_set_back_pixmap (phsh.win, phsh.phsh[0], FALSE);
        gdk_window_shape_combine_mask (phsh.win, phsh.phsh_mask[0], 0, 0);

        /* setup the keys filter */
        gdk_window_add_filter (phsh.win,
                               phsh_filter,
                               NULL);

        gdk_window_show (phsh.win);
        phsh.handler = gtk_timeout_add (150, phsh_move, NULL);
}

static guint screen_check_id = 0;

static gboolean
check_screen_timeout (gpointer data)
{
        screen_check_id = 0;

        check_screen ();

        screen_check_id = gtk_timeout_add (rand()%120*1000,
                                           check_screen_timeout, NULL);
        return FALSE;
}

void
start_screen_check (void)
{
        if (screen_check_id > 0)
                gtk_timeout_remove (screen_check_id);

        screen_check_id = 0;
        check_screen ();

        screen_check_id = gtk_timeout_add (rand()%120*1000, check_screen_timeout, NULL);
}

typedef struct {
	gboolean live;
	int x, y;
} InvGoat;

typedef struct {
	gboolean good;
	int y;
	int x;
} InvShot;


static GtkWidget *geginv = NULL;
static GtkWidget *geginv_canvas = NULL;
static GtkWidget *geginv_label = NULL;
static GdkPixmap *geginv_pixmap = NULL;
static GdkPixmap *inv_goat1 = NULL;
static GdkPixmap *inv_goat2 = NULL;
static GdkPixmap *inv_phsh1 = NULL;
static GdkBitmap *inv_phsh1_mask = NULL;
static GdkPixmap *inv_phsh2 = NULL;
static GdkBitmap *inv_phsh2_mask = NULL;
static int inv_phsh_state = 0;
static int inv_goat_state = 0;
static int inv_width = 0;
static int inv_height = 0;
static int inv_goat_width = 0;
static int inv_goat_height = 0;
static int inv_phsh_width = 0;
static int inv_phsh_height = 0;
#define INV_ROWS 3
#define INV_COLS 5
static InvGoat invs[INV_COLS][INV_ROWS] = { { { FALSE, 0, 0 } } };
static int inv_num = INV_ROWS * INV_COLS;
static double inv_factor = 1.0;
static int inv_our_x = 0;
static int inv_x = 0;
static int inv_y = 0;
static int inv_first_col = 0;
static int inv_last_col = INV_COLS-1;
static int inv_level = 0;
static int inv_lives = 0;
static gboolean inv_do_pause = FALSE;
static gboolean inv_reverse = FALSE;
static gboolean inv_game_over = FALSE;
static gboolean inv_left_pressed = FALSE;
static gboolean inv_right_pressed = FALSE;
static gboolean inv_fire_pressed = FALSE;
static gboolean inv_left_released = FALSE;
static gboolean inv_right_released = FALSE;
static gboolean inv_fire_released = FALSE;
static GSList *inv_shots = NULL;
static guint inv_draw_idle = 0;

static void
inv_show_status (void)
{
	char *s;
	if (geginv == NULL)
		return;

	if (inv_game_over) {
		s = g_strdup_printf ("<big><b>GAME OVER</b> at level %d!</big>   "
				     "Press 'q' to quit   "
				     "<big><b>GAME OVER</b> at level %d!</big>",
				     inv_level+1, inv_level+1);
	} else {
		s = g_strdup_printf ("<big>Level: <b>%d</b>  Lives: <b>%d</b></big>    "
				     "Left/Right to move, Space to fire, 'q' to quit",
				     inv_level+1, inv_lives);
	}
	gtk_label_set_markup (GTK_LABEL (geginv_label), s);

	g_free (s);
}

static gboolean
inv_draw (gpointer data)
{
	GdkPixmap *goat;
	GSList *li;
	int i, j;

	if (data != geginv) {
		inv_draw_idle = 0;
		return FALSE;
	}

	if ( ! GTK_WIDGET_DRAWABLE (geginv_canvas) ||
	    geginv_pixmap == NULL)
		return TRUE;

	gdk_draw_rectangle (geginv_pixmap,
			    geginv_canvas->style->white_gc,
			    TRUE /* filled */,
			    0, 0,
			    inv_width, inv_height);

	if (inv_goat_state == 0)
		goat = inv_goat1;
	else
		goat = inv_goat2;

	for (i = 0; i < INV_COLS; i++) {
		for (j = 0; j < INV_ROWS; j++) {
			int x, y;
			if ( ! invs[i][j].live)
				continue;

			x = invs[i][j].x*inv_factor - inv_goat_width/2,
			y = invs[i][j].y*inv_factor - inv_goat_height/2,

			gdk_draw_drawable (geginv_pixmap,
					   geginv_canvas->style->white_gc,
					   goat,
					   0, 0,
					   x, y,
					   inv_goat_width,
					   inv_goat_height);
		}
	}

	for (li = inv_shots; li != NULL; li = li->next) {
		InvShot *shot = li->data;

		gdk_draw_rectangle (geginv_pixmap,
				    geginv_canvas->style->black_gc,
				    TRUE /* filled */,
				    (shot->x-1)*inv_factor,
				    (shot->y-4)*inv_factor,
				    3, 8);
	}

	if ( ! inv_game_over) {
		GdkPixmap *phsh;
		GdkBitmap *mask;

		if (inv_phsh_state < 5) {
			phsh = inv_phsh1;
			mask = inv_phsh1_mask;
		} else {
			phsh = inv_phsh2;
			mask = inv_phsh2_mask;
		}

		gdk_gc_set_clip_origin (geginv_canvas->style->white_gc,
					inv_our_x*inv_factor - inv_phsh_width/2,
					550*inv_factor - inv_phsh_height/2);

		gdk_gc_set_clip_mask (geginv_canvas->style->white_gc,
				      mask);

		gdk_draw_drawable (geginv_pixmap,
				   geginv_canvas->style->white_gc,
				   phsh,
				   0, 0,
				   inv_our_x*inv_factor - inv_phsh_width/2,
				   550*inv_factor - inv_phsh_height/2,
				   inv_phsh_width,
				   inv_phsh_height);

		gdk_gc_set_clip_origin (geginv_canvas->style->white_gc, 0, 0);

		gdk_gc_set_clip_mask (geginv_canvas->style->white_gc, NULL);
	}

	gdk_draw_drawable (geginv_canvas->window,
			   geginv_canvas->style->white_gc,
			   geginv_pixmap,
			   0, 0,
			   0, 0,
			   inv_width, inv_height);

	gdk_flush ();

	if (inv_do_pause) {
		sleep (1);
		inv_do_pause = FALSE;
	}

	inv_draw_idle = 0;
	return FALSE;
}

static void
inv_queue_draw (GtkWidget *window)
{
	if (inv_draw_idle == 0)
		inv_draw_idle = g_idle_add (inv_draw, window);
}

static void
inv_draw_explosion (int x, int y)
{
	int i;
	GdkColormap *cmap;
	GdkColor red;
	GdkGC *gc;

	if ( ! GTK_WIDGET_DRAWABLE (geginv_canvas))
		return;

	cmap = gdk_drawable_get_colormap (geginv_canvas->window);
	gdk_color_parse ("red", &red);
	gdk_colormap_alloc_color (cmap, &red, FALSE, TRUE);

	gc = gdk_gc_new (geginv_canvas->window);
	gdk_gc_set_foreground (gc, &red);
	gdk_gc_set_background (gc, &red);

	for (i = 5; i < 100; i += 5) {
		gdk_draw_arc (geginv_canvas->window,
			      gc,
			      TRUE /* filled */,
			      x-i, y-i,
			      i*2, i*2,
			      0, 360*64);
		gdk_flush ();
		g_usleep (50000);
	}

	g_object_unref (G_OBJECT (gc));

	for (i = 5; i < 100; i += 5) {
		gdk_draw_arc (geginv_canvas->window,
			      geginv_canvas->style->white_gc,
			      TRUE /* filled */,
			      x-i, y-i,
			      i*2, i*2,
			      0, 360*64);
		gdk_flush ();
		g_usleep (50000);
	}
	
	gdk_colormap_free_colors (cmap, &red, 1);

	inv_queue_draw (geginv);
}


static void
inv_do_game_over (void)
{
	GSList *li;

	inv_game_over = TRUE;
	
	for (li = inv_shots; li != NULL; li = li->next) {
		InvShot *shot = li->data;
		shot->good = FALSE;
	}

	inv_queue_draw (geginv);

	inv_show_status ();
}


static GdkPixbuf *
pb_scale (GdkPixbuf *pb, double scale)
{
	int w, h;
	
	if (scale == 1.0)
		return gdk_pixbuf_ref (pb);

	w = gdk_pixbuf_get_width (pb) * scale;
	h = gdk_pixbuf_get_height (pb) * scale;

	return gdk_pixbuf_scale_simple (pb, w, h,
					GDK_INTERP_BILINEAR);
}

static void
refind_first_and_last (void)
{
	int i, j;

	for (i = 0; i < INV_COLS; i++) {
		gboolean all_null = TRUE;
		for (j = 0; j < INV_ROWS; j++) {
			if (invs[i][j].live) {
				all_null = FALSE;
				break;
			}
		}
		if ( ! all_null) {
			inv_first_col = i;
			break;
		}
	}

	for (i = INV_COLS-1; i >= 0; i--) {
		gboolean all_null = TRUE;
		for (j = 0; j < INV_ROWS; j++) {
			if (invs[i][j].live) {
				all_null = FALSE;
				break;
			}
		}
		if ( ! all_null) {
			inv_last_col = i;
			break;
		}
	}
}

static void
whack_gegl (int i, int j)
{
	if ( ! invs[i][j].live)
		return;

	invs[i][j].live = FALSE;
	inv_num --;

	if (inv_num > 0) {
		refind_first_and_last ();
	} else {
		inv_x = 70;
		inv_y = 70;
		inv_first_col = 0;
		inv_last_col = INV_COLS-1;
		inv_reverse = FALSE;

		g_slist_foreach (inv_shots, (GFunc)g_free, NULL);
		g_slist_free (inv_shots);
		inv_shots = NULL;

		for (i = 0; i < INV_COLS; i++) {
			for (j = 0; j < INV_ROWS; j++) {
				invs[i][j].live = TRUE;
				invs[i][j].x = 70 + i * 100;
				invs[i][j].y = 70 + j * 80;
			}
		}
		inv_num = INV_ROWS * INV_COLS;

		inv_level ++;

		inv_show_status ();
	}

	inv_queue_draw (geginv);
}

static gboolean
geginv_timeout (gpointer data)
{
	int i, j;
	int limitx1;
	int limitx2;
	int speed;
	int shots;
	int max_shots;

	if (geginv != data ||
	    inv_num <= 0 ||
	    inv_y > 700)
		return FALSE;

	limitx1 = 70 - (inv_first_col * 100);
	limitx2 = 800 - 70 - (inv_last_col * 100);

	if (inv_game_over) {
		inv_y += 30;
	} else {
		if (inv_num < (INV_COLS*INV_ROWS)/3)
			speed = 45+2*inv_level;
		else if (inv_num < (2*INV_COLS*INV_ROWS)/3)
			speed = 30+2*inv_level;
		else
			speed = 15+2*inv_level;

		if (inv_reverse) {
			inv_x -= speed;
			if (inv_x < limitx1) {
				inv_reverse = FALSE;
				inv_x = (limitx1 + (limitx1 - inv_x));
				inv_y += 30+inv_level;
			}
		} else {
			inv_x += speed;
			if (inv_x > limitx2) {
				inv_reverse = TRUE;
				inv_x = (limitx2 - (inv_x - limitx2));
				inv_y += 30+inv_level;
			}
		}
	}

	for (i = 0; i < INV_COLS; i++) {
		for (j = 0; j < INV_ROWS; j++) {
			if (invs[i][j].live) {
				invs[i][j].x = inv_x + i * 100;
				invs[i][j].y = inv_y + j * 80;

				if ( ! inv_game_over &&
				    invs[i][j].y >= 570) {
					inv_do_game_over ();
				} else if ( ! inv_game_over &&
					   invs[i][j].y >= 530 &&
					   invs[i][j].x + 40 > inv_our_x - 25 &&
					   invs[i][j].x - 40 < inv_our_x + 25) {
					whack_gegl (i,j);
					inv_lives --;
					inv_draw_explosion (inv_our_x, 550);
					if (inv_lives <= 0) {
						inv_do_game_over ();
					} else {
						g_slist_foreach (inv_shots, (GFunc)g_free, NULL);
						g_slist_free (inv_shots);
						inv_shots = NULL;
						inv_our_x = 400;
						inv_do_pause = TRUE;
						inv_show_status ();
					}
				}
			}
		}
	}

	shots = 0;
	max_shots = (rand () >> 3) % (2+inv_level);
	while ( ! inv_game_over && shots < MIN (max_shots, inv_num)) {
		int i = (rand () >> 3) % INV_COLS;
		for (j = INV_ROWS-1; j >= 0; j--) {
			if (invs[i][j].live) {
				InvShot *shot = g_new0 (InvShot, 1);

				shot->good = FALSE;
				shot->x = invs[i][j].x + (rand () % 6) - 3;
				shot->y = invs[i][j].y + inv_goat_height/2 + (rand () % 3);

				inv_shots = g_slist_prepend (inv_shots, shot);
				shots++;
				break;
			}
		}
	}

	inv_goat_state = (inv_goat_state+1) % 2;

	inv_queue_draw (geginv);

	g_timeout_add (((inv_num/4)+1) * 100, geginv_timeout, geginv);

	return FALSE;
}

static gboolean
find_gegls (int x, int y)
{
	int i, j;

	/* FIXME: this is stupid, we can do better */
	for (i = 0; i < INV_COLS; i++) {
		for (j = 0; j < INV_ROWS; j++) {
			int ix = invs[i][j].x;
			int iy = invs[i][j].y;

			if ( ! invs[i][j].live)
				continue;

			if (y >= iy - 30 &&
			    y <= iy + 30 &&
			    x >= ix - 40 &&
			    x <= ix + 40) {
				whack_gegl (i, j);
				return TRUE;
			}
		}
	}

	return FALSE;
}


static gboolean
geginv_move_timeout (gpointer data)
{
	GSList *li;
	static int shot_inhibit = 0;

	if (geginv != data ||
	    inv_num <= 0 ||
	    inv_y > 700)
		return FALSE;

	inv_phsh_state = (inv_phsh_state+1)%10;

	/* we will be drawing something */
	if (inv_shots != NULL)
		inv_queue_draw (geginv);

	li = inv_shots;
	while (li != NULL) {
		InvShot *shot = li->data;

		if (shot->good) {
			shot->y -= 30;
			if (find_gegls (shot->x, shot->y) ||
			    shot->y <= 0) {
				GSList *list = li;
				/* we were restarted */
				if (inv_shots == NULL)
					return TRUE;
				li = li->next;
				g_free (shot);
				inv_shots = g_slist_delete_link (inv_shots, list);
				continue;
			}
		} else /* bad */ {
			shot->y += 30;
			if ( ! inv_game_over &&
			    shot->y >= 535 &&
			    shot->y <= 565 &&
			    shot->x >= inv_our_x - 25 &&
			    shot->x <= inv_our_x + 25) {
				inv_lives --;
				inv_draw_explosion (inv_our_x, 550);
				if (inv_lives <= 0) {
					inv_do_game_over ();
				} else {
					g_slist_foreach (inv_shots, (GFunc)g_free, NULL);
					g_slist_free (inv_shots);
					inv_shots = NULL;
					inv_our_x = 400;
					inv_do_pause = TRUE;
					inv_show_status ();
					return TRUE;
				}
			}

			if (shot->y >= 600) {
				GSList *list = li;
				li = li->next;
				g_free (shot);
				inv_shots = g_slist_delete_link (inv_shots, list);
				continue;
			}
		}

		li = li->next;
	}

	if ( ! inv_game_over) {
		if (inv_left_pressed && inv_our_x > 100) {
			inv_our_x -= 20;
			inv_queue_draw (geginv);
		} else if (inv_right_pressed && inv_our_x < 700) {
			inv_our_x += 20;
			inv_queue_draw (geginv);
		}
	}

	if (shot_inhibit > 0)
		shot_inhibit--;

	if ( ! inv_game_over && inv_fire_pressed && shot_inhibit == 0) {
		InvShot *shot = g_new0 (InvShot, 1);

		shot->good = TRUE;
		shot->x = inv_our_x;
		shot->y = 540;

		inv_shots = g_slist_prepend (inv_shots, shot);

		shot_inhibit = 5;

		inv_queue_draw (geginv);
	}

	if (inv_left_released)
		inv_left_pressed = FALSE;
	if (inv_right_released)
		inv_right_pressed = FALSE;
	if (inv_fire_released)
		inv_fire_pressed = FALSE;

	return TRUE;
}

static gboolean
inv_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	switch (event->keyval) {
	case GDK_Left:
	case GDK_KP_Left:
	case GDK_Pointer_Left:
		inv_left_pressed = TRUE;
		inv_left_released = FALSE;
		return TRUE;
	case GDK_Right:
	case GDK_KP_Right:
	case GDK_Pointer_Right:
		inv_right_pressed = TRUE;
		inv_right_released = FALSE;
		return TRUE;
	case GDK_space:
	case GDK_KP_Space:
		inv_fire_pressed = TRUE;
		inv_fire_released = FALSE;
		return TRUE;
	default:
		break;
	}
	return FALSE;
}

static gboolean
inv_key_release (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	switch (event->keyval) {
	case GDK_Left:
	case GDK_KP_Left:
	case GDK_Pointer_Left:
		inv_left_released = TRUE;
		return TRUE;
	case GDK_Right:
	case GDK_KP_Right:
	case GDK_Pointer_Right:
		inv_right_released = TRUE;
		return TRUE;
	case GDK_space:
	case GDK_KP_Space:
		inv_fire_released = TRUE;
		return TRUE;
	case GDK_q:
	case GDK_Q:
		gtk_widget_destroy (widget);
		return TRUE;
	default:
		break;
	}
	return FALSE;
}

static gboolean
ensure_creatures (void)
{
	GdkPixbuf *pb, *pb1, *phsh1, *phsh2, *goat1, *goat2;
	char *phsh_file;
	char *goat_file;

	if (inv_goat1 != NULL)
		return TRUE;

	phsh_file = get_phsh_file ();

        if (phsh_file == NULL)
                return FALSE;

	pb = gdk_pixbuf_new_from_file (phsh_file, NULL);
	g_free (phsh_file);
	if (pb == NULL)
		return FALSE;

	pb1 = get_phsh_frame (pb, 1);
	phsh1 = pb_scale (pb1, inv_factor);
	gdk_pixbuf_unref (pb1);
	phsh_dered (phsh1);

	pb1 = get_phsh_frame (pb, 2);
	phsh2 = pb_scale (pb1, inv_factor);
	gdk_pixbuf_unref (pb1);
	phsh_dered (phsh2);

	gdk_pixbuf_unref (pb);

	goat_file = panel_pixmap_discovery ("gnome-gegl2.png",
					    FALSE /* fallback */);
	if (goat_file == NULL) {
		gdk_pixbuf_unref (phsh1);
		gdk_pixbuf_unref (phsh2);
		return FALSE;
	}

	pb = gdk_pixbuf_new_from_file (goat_file, NULL);
	g_free (goat_file);
	if (pb == NULL) {
		gdk_pixbuf_unref (phsh1);
		gdk_pixbuf_unref (phsh2);
		return FALSE;
	}

	goat1 = pb_scale (pb, inv_factor * 0.66);
	gdk_pixbuf_unref (pb);

	goat_file = panel_pixmap_discovery ("gnome-gegl2-2.png",
					    FALSE /* fallback */);
	if (goat_file == NULL) {
		gdk_pixbuf_unref (goat1);
		gdk_pixbuf_unref (phsh1);
		gdk_pixbuf_unref (phsh2);
		return FALSE;
	}

	pb = gdk_pixbuf_new_from_file (goat_file, NULL);
	g_free (goat_file);
	if (pb == NULL) {
		gdk_pixbuf_unref (goat1);
		gdk_pixbuf_unref (phsh1);
		gdk_pixbuf_unref (phsh2);
		return FALSE;
	}

	goat2 = pb_scale (pb, inv_factor * 0.66);
	gdk_pixbuf_unref (pb);

	inv_goat_width = gdk_pixbuf_get_width (goat1);
	inv_goat_height = gdk_pixbuf_get_height (goat1);
	inv_phsh_width = gdk_pixbuf_get_width (phsh1);
	inv_phsh_height = gdk_pixbuf_get_height (phsh1);

	gdk_pixbuf_render_pixmap_and_mask (goat1, &inv_goat1, NULL, 127);
	gdk_pixbuf_unref (goat1);
	gdk_pixbuf_render_pixmap_and_mask (goat2, &inv_goat2, NULL, 127);
	gdk_pixbuf_unref (goat2);
	gdk_pixbuf_render_pixmap_and_mask (phsh1, &inv_phsh1, &inv_phsh1_mask, 127);
	gdk_pixbuf_unref (phsh1);
	gdk_pixbuf_render_pixmap_and_mask (phsh2, &inv_phsh2, &inv_phsh2_mask, 127);
	gdk_pixbuf_unref (phsh2);

	return TRUE;
}

static void
geginv_destroyed (GtkWidget *w, gpointer data)
{
	geginv = NULL;
	if (geginv_pixmap != NULL)
		gdk_pixmap_unref (geginv_pixmap);
	geginv_pixmap = NULL;
}

static void
geginv_realized (GtkWidget *w, gpointer data)
{
	if (geginv_pixmap == NULL)
		geginv_pixmap = gdk_pixmap_new (w->window,
						inv_width, inv_height,
						gtk_widget_get_visual (w)->depth);
}

static gboolean
inv_expose (GtkWidget *widget, GdkEventExpose *event)
{
	if ( ! GTK_WIDGET_DRAWABLE (widget) ||
	    geginv_pixmap == NULL)
		return FALSE;

	inv_queue_draw (geginv);
	/*
	gdk_draw_drawable (geginv_canvas->window,
			   geginv_canvas->style->white_gc,
			   geginv_pixmap,
			   event->area.x, event->area.x,
			   event->area.x, event->area.x,
			   event->area.width, event->area.height);
			   */

	return FALSE;
}

void
start_geginv (void)
{
	GtkWidget *vbox;
	int i, j;

	if (geginv != NULL) {
		gtk_window_present (GTK_WINDOW (geginv));
		return;
	}

	inv_width = 800;
	inv_height = 600;

	if (inv_width > gdk_screen_width () * 0.9) {
		inv_width = gdk_screen_width () * 0.9;
		inv_height = inv_width * (600.0/800.0);
	}

	if (inv_height > gdk_screen_height () * 0.9) {
		inv_height = gdk_screen_height () * 0.9;
		inv_width = inv_height * (800.0/600.0);
	}

	inv_factor = (double)inv_width / 800.0;

	if ( ! ensure_creatures ())
		return;

	geginv = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_object_set (G_OBJECT (geginv), "resizable", FALSE, NULL);
	g_signal_connect (G_OBJECT (geginv), "destroy",
			  G_CALLBACK (geginv_destroyed),
			  NULL);

	geginv_canvas = gtk_drawing_area_new ();
	gtk_widget_set_double_buffered (GTK_WIDGET (geginv_canvas), FALSE);
	gtk_widget_set_size_request (geginv_canvas, inv_width, inv_height);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (geginv), vbox);
	gtk_box_pack_start (GTK_BOX (vbox), geginv_canvas, TRUE, TRUE, 0);

	geginv_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (vbox), geginv_label, FALSE, FALSE, 0);

	inv_our_x = 400;
	inv_x = 70;
	inv_y = 70;
	inv_first_col = 0;
	inv_level = 0;
	inv_lives = 3;
	inv_last_col = INV_COLS-1;
	inv_reverse = FALSE;
	inv_game_over = FALSE;
	inv_left_pressed = FALSE;
	inv_right_pressed = FALSE;
	inv_fire_pressed = FALSE;
	inv_left_released = FALSE;
	inv_right_released = FALSE;
	inv_fire_released = FALSE;

	gtk_widget_add_events (geginv, GDK_KEY_RELEASE_MASK);

	g_signal_connect (G_OBJECT (geginv), "key_press_event",
			  G_CALLBACK (inv_key_press), NULL);
	g_signal_connect (G_OBJECT (geginv), "key_release_event",
			  G_CALLBACK (inv_key_release), NULL);
	g_signal_connect (G_OBJECT (geginv), "realize",
			  G_CALLBACK (geginv_realized), NULL);
	g_signal_connect (GTK_OBJECT (geginv_canvas), "expose_event",
			  G_CALLBACK (inv_expose), NULL);

	g_slist_foreach (inv_shots, (GFunc)g_free, NULL);
	g_slist_free (inv_shots);
	inv_shots = NULL;

	for (i = 0; i < INV_COLS; i++) {
		for (j = 0; j < INV_ROWS; j++) {
			invs[i][j].live = TRUE;
			invs[i][j].x = 70 + i * 100;
			invs[i][j].y = 70 + j * 80;
		}
	}
	inv_num = INV_ROWS * INV_COLS;

	g_timeout_add (((inv_num/4)+1) * 100, geginv_timeout, geginv);
	g_timeout_add (90, geginv_move_timeout, geginv);

	inv_show_status ();

	gtk_widget_show_all (geginv);
}
