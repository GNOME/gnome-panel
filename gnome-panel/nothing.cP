/*code snippet, not to be compiled separately*/
static int goat_pages=0;
static GdkPixmap *goat_pix = NULL;
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
	if(!GTK_WIDGET_REALIZED(goat_darea) ||
	   !GTK_WIDGET_DRAWABLE(goat_darea) ||
	   !goat_pix)
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
	
	gdk_draw_pixmap(goat_darea->window,
			goat_darea->style->white_gc,
			goat_pix,
			0,0,goat_x,goat_y,
			goat_width,goat_height);

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
	if(!goat_pix) {
		GdkPixbuf *pb;
		guchar *rgb;
		GdkGC *gc;
		int i;
		gdouble affine[6];
		char *file;

		file = gnome_unconditional_pixmap_file("gnome-gegl.png");
		if(!file || !g_file_exists (file) ||
		   !(pb=gdk_pixbuf_new_from_file (file))) {
			g_warning("Goat is not available!");
			return;
		}
		g_free(file);

		goat_width = pb->art_pixbuf->width+12;
		goat_height = pb->art_pixbuf->height+12;
		rgb = g_new(guchar,goat_width*goat_height*3);
		for(i=0;i<goat_width*goat_height*3;i++)
			rgb[i] = 0xFF;

		art_affine_translate(affine,6,6);
		art_rgb_pixbuf_affine(rgb,
				      0,0,goat_width,goat_height,goat_width*3,
				      pb->art_pixbuf,affine,
				      ART_FILTER_NEAREST,NULL);
		gdk_pixbuf_unref(pb);
		goat_pix = gdk_pixmap_new(widget->window,
					  goat_width,goat_height,
					  gtk_widget_get_visual(GTK_WIDGET(widget))->depth);
		gc = gdk_gc_new(goat_pix);
		gdk_draw_rgb_image(goat_pix,gc,0,0,
				   goat_width,goat_height,
				   GDK_RGB_DITHER_NORMAL,
				   rgb, goat_width*3);
		g_free(rgb);
		gdk_gc_destroy(gc);
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
