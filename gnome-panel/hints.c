#include "config.h"
#include <gnome.h>
#include <stdlib.h>
#include <time.h>

#include "panel-include.h"

extern GlobalConfig global_config;
extern int globals_to_sync;

static char * hints[] = {
#include "hintsdata.c"
	NULL
};

#define NUM_HINTS ((sizeof(hints)/sizeof(char *))-1)

static int current_hint = -1;

static GtkWidget *box = NULL;
static GtkWidget *label = NULL;
static GtkWidget *show_next = NULL;

/*if random is set, then random hint is taken, otherwise,
  the next one is taken*/
static void
set_hint(int is_first, int random)
{
	g_assert(label);
	
	if(is_first) {
		current_hint = 0;
	} else if(!random) {
		current_hint++;
		if(current_hint>=NUM_HINTS)
			current_hint = 0;
	} else {
		srand(time(NULL));
		current_hint = (1000*rand())%NUM_HINTS;
	}
	
	gtk_label_set(GTK_LABEL(label),gettext(hints[current_hint]));
}

static void
clicked(GtkWidget *w, int button_num, gpointer data)
{
	if(button_num == 0)
		set_hint(FALSE,FALSE);
	else
		gtk_widget_destroy(box);
}

static void
on_destroy(GtkWidget *w, gpointer data)
{
	box = NULL;
	if(global_config.show_startup_hints !=
	   GTK_TOGGLE_BUTTON(show_next)->active) {
		global_config.show_startup_hints =
			GTK_TOGGLE_BUTTON(show_next)->active;
		globals_to_sync = TRUE;
		panel_config_sync();
	}
}

void 
show_hint(int is_first)
{
	GtkWidget *hbox, *w;
	
	if(box) {
		gtk_widget_show_now(box);
		gdk_window_raise(box->window);
		set_hint(is_first,TRUE);
	}

	box = gnome_dialog_new(_("Panel hint"),
			       _("Show another hint"),
			       GNOME_STOCK_BUTTON_CLOSE,
			       NULL);
	gtk_signal_connect(GTK_OBJECT(box),"destroy",
			   GTK_SIGNAL_FUNC(on_destroy), NULL);
	gtk_signal_connect(GTK_OBJECT(box),"clicked",
			   GTK_SIGNAL_FUNC(clicked),NULL);
	w = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(box)->vbox),w,TRUE,TRUE,0);

	hbox = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(hbox),20);
	gtk_container_add(GTK_CONTAINER(w),hbox);

	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox),label,TRUE,TRUE,0);
	
	show_next = gtk_check_button_new_with_label(_("Show this dialog on startup"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (show_next),
				      global_config.show_startup_hints);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(box)->vbox),show_next,FALSE,FALSE,0);

	set_hint(is_first,TRUE);
	
	gtk_widget_show_all(box);
}
