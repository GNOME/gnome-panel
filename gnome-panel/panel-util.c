#include <config.h>
#include <string.h>
#include <glib.h>
#include <fcntl.h>

#include "panel-util.h"

/* this function might be a slight overkill, but it should work
   perfect, hopefully it should be 100% buffer overrun safe too*/
char *
get_full_path(char *argv0)
{
	char buf[PATH_MAX+2];
	char *cmdbuf;
	int i;
#if 0
	int cmdsize=100;
	int fd[2];
#else
	FILE *fwhich;
#endif

	if(!argv0)
		return NULL;

	if(*argv0 == '/')
		return g_strdup(argv0);


	if(strchr(argv0,'/')) {
		char *curpath = getcwd(NULL,0);
		char *outbuf;

		if(!curpath)
			return NULL;

		outbuf = g_copy_strings(curpath,"/",argv0,NULL);
		free(curpath);

		realpath(outbuf,buf);
	
		return g_strdup(buf);
	}

#if 0
	if(pipe(fd) == -1)
		return NULL;

	/*dynamically reallocates cmdbuf until the command fits*/
	for(;;) {
		cmdbuf = (char *)g_malloc(cmdsize);
		if(g_snprintf(cmdbuf, cmdsize, "sh -c 'which %s > /dev/fd/%d'",
			      argv0,fd[1])>-1)
			break;
	
		g_free(cmdbuf);
		cmdsize*=2;
	}
		
	system(cmdbuf);
	g_free(cmdbuf);

	i=read(fd[0],buf,PATH_MAX+1);
	close(fd[0]);
	close(fd[1]);
	if(i <= 0)
		return NULL;

	buf[i]='\0';
	if(buf[i-1]=='\n')
		buf[i-1]='\0';

	if(buf[0]=='\0')
		return NULL;

#else
	cmdbuf = g_copy_strings("/usr/bin/which ", argv0, NULL);
	fwhich = popen(cmdbuf, "r");
	g_free(cmdbuf);
	
	if (fwhich == NULL)
	  return NULL;
	if (fgets(buf, PATH_MAX+1, fwhich) == NULL) {
	  	pclose(fwhich);
		return NULL;
	}

	pclose(fwhich);

	i = strlen(buf)-1;
	if(buf[i]=='\n')
		buf[i]='\0';

#endif
	return g_strdup(buf);
}

/*this is used to do an immediate move instead of set_uposition, which
queues one*/
void
move_window(GtkWidget *widget, int x, int y)
{
	gdk_window_set_hints(widget->window, x, y, 0, 0, 0, 0, GDK_HINT_POS);
	gdk_window_move(widget->window, x, y);
	/* FIXME: this should draw only the newly exposed area! */
	gtk_widget_draw(widget, NULL);
}



int
string_is_in_list(GList *list,char *text)
{
	for(;list!=NULL;list=g_list_next(list))
		if(strcmp(text,list->data)==0)
			return TRUE;
	return FALSE;
}


/*these ones are used in internal property dialogs:*/
static void
notify_entry_change (GtkWidget *widget, void *data)
{
	GnomePropertyBox *box = GNOME_PROPERTY_BOX (data);

	gnome_property_box_changed (box);
}

/*FIXME: use gnome_entry*/
GtkWidget *
create_text_entry(GtkWidget *table,
		  char *history_id,
		  int row,
		  char *label,
		  char *text,
		  GtkWidget *w)
{
	GtkWidget *wlabel;
	GtkWidget *entry;

	wlabel = gtk_label_new(label);
	gtk_misc_set_alignment(GTK_MISC(wlabel), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), wlabel,
			 0, 1, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);
	gtk_widget_show(wlabel);

	entry = gtk_entry_new();
	if (text)
		gtk_entry_set_text(GTK_ENTRY(entry), text);
	gtk_table_attach(GTK_TABLE(table), entry,
			 1, 2, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);

	gtk_signal_connect (GTK_OBJECT (entry), "changed",
			    GTK_SIGNAL_FUNC(notify_entry_change), w);
	return entry;
}

GtkWidget *
create_file_entry(GtkWidget *table,
		  char *history_id,
		  int row,
		  char *label,
		  char *text,
		  GtkWidget *w)
{
	GtkWidget *wlabel;
	GtkWidget *entry;
	GtkWidget *t;

	wlabel = gtk_label_new(label);
	gtk_misc_set_alignment(GTK_MISC(wlabel), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), wlabel,
			 0, 1, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);
	gtk_widget_show(wlabel);

	entry = gnome_file_entry_new(history_id,_("Browse"));
	t = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (entry));
	if (text)
		gtk_entry_set_text(GTK_ENTRY(t), text);
	gtk_table_attach(GTK_TABLE(table), entry,
			 1, 2, row, row + 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);

	gtk_signal_connect (GTK_OBJECT (t), "changed",
			    GTK_SIGNAL_FUNC(notify_entry_change), w);
	return t;
}


GList *
my_g_list_swap_next(GList *list, GList *dl)
{
	GList *t;

	if(!dl->next)
		return list;
	if(dl->prev)
		dl->prev->next = dl->next;
	t = dl->prev;
	dl->prev = dl->next;
	dl->next->prev = t;
	if(dl->next->next)
		dl->next->next->prev = dl;
	t = dl->next->next;
	dl->next->next = dl;
	dl->next = t;

	if(list == dl)
		return dl->prev;
	return list;
}

GList *
my_g_list_swap_prev(GList *list, GList *dl)
{
	GList *t;

	if(!dl->prev)
		return list;
	if(dl->next)
		dl->next->prev = dl->prev;
	t = dl->next;
	dl->next = dl->prev;
	dl->prev->next = t;
	if(dl->prev->prev)
		dl->prev->prev->next = dl;
	t = dl->prev->prev;
	dl->prev->prev = dl;
	dl->prev = t;

	if(list == dl->next)
		return dl;
	return list;
}

/*maybe this should be a glib function?
 it resorts a single item in the list*/
GList *
my_g_list_resort_item(GList *list, gpointer data, GCompareFunc func)
{
	GList *dl;

	if(!list)
		return NULL;

	dl = g_list_find(list,data);

	g_return_val_if_fail(dl!=NULL,list);

	while(dl->next &&
	      (*func)(dl->data,dl->next->data)>0)
		list=my_g_list_swap_next(list,dl);
	while(dl->prev &&
	      (*func)(dl->data,dl->prev->data)<0)
		list=my_g_list_swap_prev(list,dl);
	return list;
}

/*this is used to do an immediate move instead of set_uposition, which
queues one*/
void
move_resize_window(GtkWidget *widget, int x, int y, int w, int h)
{
	/*printf("%d x %d x %d x %d\n",x,y,w,h);*/
	gdk_window_set_hints(widget->window, x, y, w, h, w, h,
			     GDK_HINT_POS|GDK_HINT_MIN_SIZE|GDK_HINT_MAX_SIZE);
	gdk_window_move_resize(widget->window, x, y, w, h);
	/* FIXME: this should draw only the newly exposed area! */
	gtk_widget_draw(widget, NULL);
}

/*this is used to do an immediate resize instead of set_usize, which
queues one*/
void
resize_window(GtkWidget *widget, int w, int h)
{
	/*printf("%d x %d x %d x %d\n",x,y,w,h);*/
	gdk_window_set_hints(widget->window, 0, 0, w, h, w, h,
			     GDK_HINT_MIN_SIZE|GDK_HINT_MAX_SIZE);
	gdk_window_resize(widget->window, w, h);
	/* FIXME: this should draw only the newly exposed area! */
	gtk_widget_draw(widget, NULL);
}
