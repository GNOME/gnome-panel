/*code snippet, not to be compiled separately*/
static int goat_pages=0;
static int goat_frame=0;
static GdkPixmap *goat_pix[2] = {NULL,NULL};
static GdkPixmap *goat_pix_rev[2] = {NULL,NULL};
static GtkWidget *goat_darea = NULL;
static char *goat_filename = NULL;
static int goat_width = 0,goat_height = 0;
static int goat_timeout = 0;
static int goat_x = -1, goat_y = -1;
static int goat_accx = -1, goat_accy = -1;

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
		if(goat_filename)
			gnome_sound_play (goat_filename);
	} else if(goat_x<6) {
		goat_accx = rand()%4 +3;
		goat_x = 6;
		if(goat_filename)
			gnome_sound_play (goat_filename);
	}
	if(goat_y>goat_darea->allocation.height-6-goat_height) {
		goat_accy = -(rand()%4 +3);
		goat_y = goat_darea->allocation.height-6-goat_height;
		if(goat_filename)
			gnome_sound_play (goat_filename);
	} else if(goat_y<6) {
		goat_accy = rand()%4 +3;
		goat_y = 6;
		if(goat_filename)
			gnome_sound_play (goat_filename);
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

		file = gnome_pixmap_file(files[frame]);
		if(!file || 
		   !(pb=gdk_pixbuf_new_from_file (file))) {
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
		if(!goat_filename)
			goat_filename =
				gnome_sound_file ("ricochet.wav");

		gtk_widget_push_visual (gdk_rgb_get_visual ());
		gtk_widget_push_colormap (gdk_rgb_get_cmap ());

		goat_darea = gtk_drawing_area_new();

		gtk_widget_pop_colormap ();
		gtk_widget_pop_visual ();

		gtk_signal_connect_after(GTK_OBJECT(goat_darea),"realize",
					 GTK_SIGNAL_FUNC(goat_realize),NULL);
		
		gtk_widget_show(goat_darea);
		goat_timeout = gtk_timeout_add(60,goat_timeout_func,NULL);
		/*the GEGL shall not be translated*/
		gtk_notebook_append_page (nbook, goat_darea,
					  gtk_label_new ("GEGL"));
		gtk_notebook_set_page(nbook,-1);
		goat_pages = 1;
		gtk_signal_connect(GTK_OBJECT(goat_darea),"destroy",
				   GTK_SIGNAL_FUNC(destroy_egg),NULL);
		gtk_signal_connect(GTK_OBJECT(goat_darea),"expose_event",
				   GTK_SIGNAL_FUNC(goat_expose),NULL);
	} else {
		gtk_notebook_set_page(nbook,-1);
	}
	return FALSE;
}
