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

#ifdef FIXME
static char *goat_filename = NULL;
#endif /* FIXME */

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
#ifdef FIXME	
		if(goat_filename)
			gnome_sound_play (goat_filename);
#endif
	} else if(goat_x<6) {
		goat_accx = rand()%4 +3;
		goat_x = 6;
#ifdef FIXME	
		if(goat_filename)
			gnome_sound_play (goat_filename);
#endif
	}
	if(goat_y>goat_darea->allocation.height-6-goat_height) {
		goat_accy = -(rand()%4 +3);
		goat_y = goat_darea->allocation.height-6-goat_height;
#ifdef FIXME	
		if(goat_filename)
			gnome_sound_play (goat_filename);
#endif
	} else if(goat_y<6) {
		goat_accy = rand()%4 +3;
		goat_y = 6;
#ifdef FIXME	
		if(goat_filename)
			gnome_sound_play (goat_filename);
#endif
	}

	real_goat_frame = goat_frame/2;
	
	gdk_draw_pixmap(goat_darea->window,
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

		file = gnome_pixmap_file (files[frame]);
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
#ifdef FIXME
		if(!goat_filename)
			goat_filename =
				gnome_sound_file ("ricochet.wav");
#endif

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

/* this checks the screen */
static void
check_screen (void)
{
        GdkWindowAttr attributes;
        char *phsh_file;
        char *name;
        GdkPixbuf *gp, *tmp;

        if (phsh.win != NULL)
                return;

        name = g_strdup_printf ("%cish/%cishanim.png",
                                'f', 'f');

        phsh_file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
                                               name, TRUE, NULL);

        g_free (name);

        if (!phsh_file)
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

        gp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 60, 40);
        gdk_pixbuf_copy_area (tmp, 60, 0, 60, 40, gp, 0, 0);

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
