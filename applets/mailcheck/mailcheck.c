/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GNOME panel mail check module.
 * (C) 1997, 1998, 1999, 2000 The Free Software Foundation
 * (C) 2001 Eazel, Inc.
 *
 * Authors: Miguel de Icaza
 *          Jacob Berkman
 *          Jaka Mocnik
 *          Lennart Poettering
 *          George Lebl
 *
 */

#include <config.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <gnome.h>
#include <applet-widget.h>
#include <libgnomeui/gnome-window-icon.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "popcheck.h"
#include "remote-helper.h"
#include "mailcheck.h"

GtkWidget *applet = NULL;

typedef enum {
	MAILBOX_LOCAL,
	MAILBOX_LOCALDIR,
	MAILBOX_POP3,
	MAILBOX_IMAP
} MailboxType;


typedef struct _MailCheck MailCheck;
struct _MailCheck {
	char *mail_file;

	/* Does the user have any mail at all? */
	int anymail;

	/* New mail has arrived? */
	int newmail;

	/* Does the user have unread mail? */
	int unreadmail;
	int totalmail;

	guint update_freq;

	/* execute a command when the applet is clicked (launch email prog) */
        char *clicked_cmd;
	gboolean clicked_enabled;

	/* execute a command when new mail arrives (play a sound etc.)
	   FIXME: actually executes the command when mc->newmail 
	   goes from 0 -> 1 (so not every time you get new mail) */
	char *newmail_cmd;
	gboolean newmail_enabled;

	/* execute a command before checking email (fetchmail etc.) */
	char *pre_check_cmd;
	gboolean pre_check_enabled;	

	/* This is the event box for catching events */
	GtkWidget *ebox;

	/* This holds either the drawing area or the label */
	GtkWidget *bin;

	/* The widget that holds the label with the mail information */
	GtkWidget *label;

	/* Points to whatever we have inside the bin */
	GtkWidget *containee;

	/* The drawing area */
	GtkWidget *da;
	GdkPixmap *email_pixmap;
	GdkBitmap *email_mask;

	/* handle for the timeout */
	int mail_timeout;

	/* how do we report the mail status */
	enum {
		REPORT_MAIL_USE_TEXT,
		REPORT_MAIL_USE_BITMAP,
		REPORT_MAIL_USE_ANIMATION
	} report_mail_mode;

	/* current frame on the animation */
	int nframe;

	/* number of frames on the pixmap */
	int frames;

	/* handle for the animation timeout handler */
	int animation_tag;

	/* for the selection routine */
	char *selected_pixmap_name;

	/* The property window */
	GtkWidget *property_window;
	GtkWidget *min_spin, *sec_spin;
	GtkWidget *pre_check_cmd_entry, *pre_check_cmd_check;
	GtkWidget *newmail_cmd_entry, *newmail_cmd_check;
	GtkWidget *clicked_cmd_entry, *clicked_cmd_check;

	/* the about box */
	GtkWidget *about;

	gboolean anim_changed;

	char *mailcheck_text_only;

	char *animation_file;
        
	GtkWidget *mailfile_entry, *mailfile_label, *mailfile_fentry;
	GtkWidget *remote_server_entry, *remote_username_entry, *remote_password_entry, *remote_folder_entry;
	GtkWidget *remote_server_label, *remote_username_label, *remote_password_label, *remote_folder_label;
	GtkWidget *pre_remote_command_label, *pre_remote_command_entry;
	GtkWidget *remote_option_menu;
	GtkWidget *play_sound_check;
        
	char *pre_remote_command, *remote_server, *remote_username, *remote_password, *real_password, *remote_folder;
	MailboxType mailbox_type; /* local = 0; maildir = 1; pop3 = 2; imap = 3 */
        MailboxType mailbox_type_temp;

	gboolean play_sound;

	int type; /*mailcheck = 0; mailbox = 1 */
	
	int size;

	/* see remote-helper.h */
	gpointer remote_handle;
};

static int mail_check_timeout (gpointer data);
static void after_mail_check (MailCheck *mc);

#define WANT_BITMAPS(x) (x == REPORT_MAIL_USE_ANIMATION || x == REPORT_MAIL_USE_BITMAP)

static void close_callback (GtkWidget *widget, void *data);

static char *
mail_animation_filename (MailCheck *mc)
{
	if (!mc->animation_file){
		mc->animation_file =
			gnome_unconditional_pixmap_file ("mailcheck/email.png");
		if (g_file_exists (mc->animation_file))
			return g_strdup (mc->animation_file);
		g_free (mc->animation_file);
		mc->animation_file = NULL;
		return NULL;
	} else if (*mc->animation_file){
		if (g_file_exists (mc->animation_file))
			return g_strdup (mc->animation_file);
		g_free (mc->animation_file);
		mc->animation_file = NULL;
		return NULL;
	} else
		/* we are using text only, since the filename was "" */
		return NULL;
}

static int
calc_dir_contents (char *dir) {
       DIR *dr;
       struct dirent *de;
       int size=0;

       dr = opendir(dir);
       if (dr == NULL)
               return 0;
       while((de = readdir(dr))) {
               if (strlen(de->d_name) < 1 || de->d_name[0] == '.')
                       continue;
               size ++;
       }
       closedir(dr);
       return size;
}

static void
get_password_callback (gchar *string, gpointer data)
{
	gchar **pass = (gchar **)data;

	*pass = string;
}

static gchar *
get_remote_password (void)
{
	gchar *pass = NULL;
	GtkWidget *dialog;

	dialog = gnome_request_dialog(TRUE, _("Password:"), "",
				      256, get_password_callback, &pass, NULL);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));

	return pass;
}


static void
got_remote_answer (int mails, gpointer data)
{
	MailCheck *mc = data;
	int old_unreadmail;

	mc->remote_handle = NULL;
	
	if (mails == -1) {
#if 0
		/* don't notify about an error: think of people with
		 * dial-up connections; keep the current mail status
		 */
		GtkWidget *box = NULL;
		box = gnome_message_box_new (_("Remote-client-error occured. Remote-polling deactivated. Maybe you used a wrong server/username/password?"),
					     GNOME_MESSAGE_BOX_ERROR, GNOME_STOCK_BUTTON_CLOSE, NULL);
		gtk_window_set_modal (GTK_WINDOW(box),TRUE);
		gtk_widget_show (box);
		
		mc->mailbox_type = MAILBOX_LOCAL;
		mc->anymail = mc->newmail = 0;
#endif
	} else {
		old_unreadmail = mc->unreadmail;
		mc->unreadmail = (signed int) (((unsigned int) mails) >> 16);
		if(mc->unreadmail > old_unreadmail) /* lt */
			mc->newmail = 1;
		else
			mc->newmail = 0;
		mc->totalmail = (signed int) (((unsigned int) mails) & 0x0000FFFFL);
		mc->anymail = mc->totalmail ? 1 : 0;

		after_mail_check (mc);
	} 
}

static void
null_remote_handle (gpointer data)
{
	MailCheck *mc = data;

	mc->remote_handle = NULL;
}

/*
 * Get file modification time, based upon the code
 * of Byron C. Darrah for coolmail and reused on fvwm95
 */
static void
check_mail_file_status (MailCheck *mc)
{
	static off_t oldsize = 0;
	off_t newsize;
	struct stat s;
	int status;
	
	if ((mc->mailbox_type == MAILBOX_POP3) || 
	    (mc->mailbox_type == MAILBOX_IMAP)) {
		if (mc->remote_handle != NULL)
			/* check in progress */
			return;

		if (mc->remote_password != NULL &&
		    mc->remote_password[0] != '\0') {
			g_free (mc->real_password);
			mc->real_password = g_strdup (mc->remote_password);
		}
		else if(mc->real_password == NULL) {
			if(mc->mail_timeout != 0)
				gtk_timeout_remove(mc->mail_timeout);
			mc->mail_timeout = 0;
			mc->real_password = get_remote_password();
			mc->mail_timeout = gtk_timeout_add(mc->update_freq,
							   mail_check_timeout,
							   mc);
		}

		if (mc->real_password != NULL &&
		    mc->remote_username != NULL &&
		    mc->remote_server != NULL) {
			if (mc->mailbox_type == MAILBOX_POP3)
				mc->remote_handle =
					helper_pop3_check (got_remote_answer,
							   mc,
							   null_remote_handle,
							   mc->pre_remote_command,
							   mc->remote_server,
							   mc->remote_username,
							   mc->real_password);
			else
					helper_imap_check (got_remote_answer,
							   mc,
							   null_remote_handle,
							   mc->pre_remote_command,
							   mc->remote_server,
							   mc->remote_username,
							   mc->real_password,
							   mc->remote_folder);
		}
	}
	else if (mc->mailbox_type == MAILBOX_LOCAL) {
		status = stat (mc->mail_file, &s);
		if (status < 0) {
			oldsize = 0;
			mc->anymail = mc->newmail = mc->unreadmail = 0;
			return;
		}
		
		newsize = s.st_size;
		mc->anymail = newsize > 0;
		mc->unreadmail = (s.st_mtime >= s.st_atime && newsize > 0);
		
		if (newsize != oldsize && mc->unreadmail)
			mc->newmail = 1;
		else
			mc->newmail = 0;
		
		oldsize = newsize;
	}
	else { /* MAILBOX_LOCALDIR */
		int newmail, oldmail;
		char tmp[1024];
		g_snprintf(tmp, sizeof (tmp), "%s/new", mc->mail_file);
		newmail = calc_dir_contents(tmp);
		g_snprintf(tmp, sizeof (tmp), "%s/cur", mc->mail_file);
		oldmail = calc_dir_contents(tmp);
		mc->newmail = newmail > oldsize;
		mc->unreadmail = newmail;
		oldsize = newmail;
		mc->anymail = newmail || oldmail;
		mc->totalmail = newmail + oldmail;
	}	    
}

static gboolean
mailcheck_load_animation (MailCheck *mc, const char *fname)
{
	int width, height;
	int pbwidth, pbheight;
	GdkPixbuf *pb;

	if (mc->email_pixmap != NULL)
		gdk_pixmap_unref(mc->email_pixmap);
	if (mc->email_mask != NULL)
		gdk_bitmap_unref(mc->email_mask);
	mc->email_pixmap = NULL;
	mc->email_mask = NULL;

	pb = gdk_pixbuf_new_from_file (fname);
	if (pb == NULL)
		return FALSE;

	pbwidth = gdk_pixbuf_get_width(pb);
	pbheight = gdk_pixbuf_get_height(pb);

	if(pbheight != mc->size) {
		GdkPixbuf *pbt;
		height = mc->size;
		width = pbwidth*((double)height/pbheight);

		pbt = gdk_pixbuf_scale_simple(pb, width, height,
					      GDK_INTERP_NEAREST);
		gdk_pixbuf_unref(pb);
		pb = pbt;
	} else {
		width = pbwidth;
		height = pbheight;
	}

	/* yeah, they have to be square, in case you were wondering :-) */
	mc->frames = width / height;
	if (mc->frames < 3)
		return FALSE;
	else if (mc->frames == 3)
		mc->report_mail_mode = REPORT_MAIL_USE_BITMAP;
	mc->nframe = 0;

	gdk_pixbuf_render_pixmap_and_mask(pb,
					  &mc->email_pixmap,
					  &mc->email_mask,
					  128);

	gdk_pixbuf_unref (pb);
	
	return TRUE;
}

static int
next_frame (gpointer data)
{
	MailCheck *mc = data;

	mc->nframe = (mc->nframe + 1) % mc->frames;
	if (mc->nframe == 0)
		mc->nframe = 1;
	gtk_widget_draw (mc->da, NULL);
	return TRUE;
}

static void
after_mail_check (MailCheck *mc)
{
	static const char *supinfo[] = {"mailcheck", "new-mail", NULL};

	if(mc->newmail) {
		if(mc->play_sound)
			gnome_triggers_vdo("", "program", supinfo);

		if (mc->newmail_enabled && 
		    mc->newmail_cmd && 
		    (strlen(mc->newmail_cmd) > 0))
			gnome_execute_shell(NULL, mc->newmail_cmd);
	}

	switch (mc->report_mail_mode) {
	case REPORT_MAIL_USE_ANIMATION:
		if (mc->anymail){
			if (mc->unreadmail){
				if (mc->animation_tag == 0){
					mc->animation_tag = gtk_timeout_add (150, next_frame, mc);
					mc->nframe = 1;
				}
			} else {
				if (mc->animation_tag != 0){
					gtk_timeout_remove (mc->animation_tag);
					mc->animation_tag = 0;
				}
				mc->nframe = 1;
			}
		} else {
			if (mc->animation_tag != 0){
				gtk_timeout_remove (mc->animation_tag);
				mc->animation_tag = 0;
			}
			mc->nframe = 0;
		}
		gtk_widget_draw (mc->da, NULL);
		break;
		
	case REPORT_MAIL_USE_BITMAP:
		if (mc->anymail){
			if (mc->newmail)
				mc->nframe = 2;
			else
				mc->nframe = 1;
		} else
			mc->nframe = 0;
		gtk_widget_draw (mc->da, NULL);
		break;

	case REPORT_MAIL_USE_TEXT: {
		char *text;

		if (mc->anymail){
			if(mc->mailbox_type == MAILBOX_LOCAL) {
				if(mc->newmail)
					text = g_strdup(_("You have new mail."));
				else
					text = g_strdup(_("You have mail."));
			}
			else {
				if(mc->unreadmail)
					text = g_strdup_printf(_("%d/%d messages"), mc->unreadmail, mc->totalmail);
				else
					text = g_strdup_printf(_("%d messages"), mc->totalmail);
			} 
		}
		else
			text = g_strdup_printf(_("No mail."));
		gtk_label_set_text (GTK_LABEL (mc->label), text);
		g_free(text);
		break;
	}
	}
}

static gboolean
mail_check_timeout (gpointer data)
{
	MailCheck *mc = data;

	if (mc->pre_check_enabled &&
	    mc->pre_check_cmd && 
	    (strlen(mc->pre_check_cmd) > 0)){
		/*
		 * if we have to execute a command before checking for mail, we
		 * remove the mail-check timeout and re-add it after the command
		 * returns, just in case the execution takes too long.
		 */
		
		if(mc->mail_timeout != 0)
			gtk_timeout_remove (mc->mail_timeout);
		mc->mail_timeout = 0;
		if (system(mc->pre_check_cmd) == 127)
			g_warning("Couldn't execute command");
		mc->mail_timeout = gtk_timeout_add(mc->update_freq, mail_check_timeout, mc);
	}

	check_mail_file_status (mc);

	after_mail_check (mc);

	return TRUE;
}

/*
 * this gets called when we have to redraw the nice icon
 */
static gint
icon_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	MailCheck *mc = data;
	int h = mc->size;
	gdk_draw_pixmap (mc->da->window, mc->da->style->black_gc,
			 mc->email_pixmap, mc->nframe * h,
			 0, 0, 0, h, h);
	return TRUE;
}

static gint
exec_clicked_cmd (GtkWidget *widget, GdkEvent *evt, gpointer data)
{
         MailCheck *mc = data;
	 if (mc->clicked_enabled && 
	     mc->clicked_cmd && 
	     (strlen(mc->clicked_cmd) > 0) )
	        gnome_execute_shell(NULL, mc->clicked_cmd);
	 return TRUE;
}

static void
mailcheck_destroy (GtkWidget *widget, gpointer data)
{
	MailCheck *mc = data;

	mc->bin = NULL;

	if (mc->property_window != NULL)
		gtk_widget_destroy (mc->property_window);
	if (mc->about != NULL)
		gtk_widget_destroy (mc->about);

	gtk_widget_unref (mc->da);

	g_free (mc->pre_check_cmd);
	g_free (mc->newmail_cmd);
	g_free (mc->clicked_cmd);

	g_free (mc->remote_server);
	g_free (mc->pre_remote_command);
	g_free (mc->remote_username);
	g_free (mc->remote_password);
	g_free (mc->remote_folder);
	g_free (mc->real_password);

	g_free (mc->animation_file);
	g_free (mc->mail_file);

	if (mc->email_pixmap != NULL)
		gdk_pixmap_unref (mc->email_pixmap);
	if (mc->email_mask != NULL)
		gdk_bitmap_unref (mc->email_mask);

	if (mc->mail_timeout != 0)
		gtk_timeout_remove (mc->mail_timeout);

	if (mc->animation_tag != 0)
		gtk_timeout_remove (mc->animation_tag);

	if (mc->remote_handle != NULL)
		helper_whack_handle (mc->remote_handle);

	/* just for sanity */
	memset(mc, 0, sizeof(MailCheck));

	g_free(mc);
}

static GtkWidget *
create_mail_widgets (MailCheck *mc)
{
	char *fname = mail_animation_filename (mc);

	mc->ebox = gtk_event_box_new();
        gtk_widget_set_events(mc->ebox, 
                              gtk_widget_get_events(mc->ebox) |
                              GDK_BUTTON_PRESS_MASK);
	gtk_widget_show (mc->ebox);
	
	/*
	 * This is so that the properties dialog is destroyed if the
	 * applet is removed from the panel while the dialog is
	 * active.
	 */
	gtk_signal_connect (GTK_OBJECT (mc->ebox), "destroy",
			    (GtkSignalFunc) mailcheck_destroy,
			    mc);

	mc->bin = gtk_hbox_new (0, 0);
	gtk_container_add(GTK_CONTAINER(mc->ebox), mc->bin);

	gtk_widget_show (mc->bin);
	
	mc->mail_timeout = gtk_timeout_add (mc->update_freq, mail_check_timeout, mc);

	/* The drawing area */
	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());
	mc->da = gtk_drawing_area_new ();
	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();
	gtk_widget_ref (mc->da);
	gtk_drawing_area_size (GTK_DRAWING_AREA(mc->da),
			       mc->size, mc->size);
	gtk_signal_connect (GTK_OBJECT(mc->da), "expose_event", (GtkSignalFunc)icon_expose, mc);
	gtk_widget_show (mc->da);

	/* The label */
	mc->label = gtk_label_new ("");
	gtk_widget_show (mc->label);
	gtk_widget_ref (mc->label);
	
	if (fname != NULL &&
	    WANT_BITMAPS (mc->report_mail_mode) &&
	    mailcheck_load_animation (mc, fname)) {
		mc->containee = mc->da;
	} else {
		mc->report_mail_mode = REPORT_MAIL_USE_TEXT;
		mc->containee = mc->label;
	}
	g_free (fname);
	gtk_container_add (GTK_CONTAINER (mc->bin), mc->containee);
	return mc->ebox;
}

static void
set_selection (GtkWidget *widget, gpointer data)
{
	MailCheck *mc = gtk_object_get_user_data(GTK_OBJECT(widget));
	mc->selected_pixmap_name = data;
	mc->anim_changed = TRUE;

	gnome_property_box_changed (GNOME_PROPERTY_BOX (mc->property_window));
}

static void
property_box_changed(GtkWidget *widget, gpointer data)
{
	MailCheck *mc = data;
	
	gnome_property_box_changed (GNOME_PROPERTY_BOX (mc->property_window));
}

static void
mailcheck_new_entry (MailCheck *mc, GtkWidget *menu, GtkWidget *item, char *s)
{
	gtk_menu_append (GTK_MENU (menu), item);

	gtk_object_set_user_data (GTK_OBJECT (item), mc);

	gtk_signal_connect_full (GTK_OBJECT (item), "activate",
				 GTK_SIGNAL_FUNC (set_selection),
				 NULL,
				 g_strdup (s),
				 (GtkDestroyNotify)g_free,
				 FALSE, FALSE);
}

static GtkWidget *
mailcheck_get_animation_menu (MailCheck *mc)
{
	GtkWidget *omenu, *menu, *item;
	struct    dirent *e;
	char      *dname = gnome_unconditional_pixmap_file ("mailcheck");
	DIR       *dir;
	char      *basename = NULL;
	int       i = 0, select_item = 0;

	mc->selected_pixmap_name = mc->mailcheck_text_only;
	omenu = gtk_option_menu_new ();
	menu = gtk_menu_new ();

	item = gtk_menu_item_new_with_label (mc->mailcheck_text_only);
	gtk_widget_show (item);
	mailcheck_new_entry (mc, menu, item, mc->mailcheck_text_only);

	if (mc->animation_file != NULL)
		basename = g_basename (mc->animation_file);
	else
		basename = NULL;

	i = 1;
	dir = opendir (dname);
	if (dir){
		while ((e = readdir (dir)) != NULL){
			char *s;
			
			if (! (strstr (e->d_name, ".xpm") ||
			       strstr (e->d_name, ".png") ||
			       strstr (e->d_name, ".gif") ||
			       strstr (e->d_name, ".jpg")))
				continue;

			s = g_strdup (e->d_name);
			if (!mc->selected_pixmap_name)
				mc->selected_pixmap_name = s;
			if (basename && strcmp (basename, e->d_name) == 0)
				select_item = i;
			item = gtk_menu_item_new_with_label (s);
			i++;
			gtk_widget_show (item);
			
			mailcheck_new_entry (mc,menu, item, s);

			g_free (s);
		}
		closedir (dir);
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), select_item);
	gtk_widget_show (omenu);
	g_free (dname);
	return omenu;
}

static void
close_callback (GtkWidget *widget, gpointer data)
{
	MailCheck *mc = data;
	mc->property_window = NULL;
}

static void
load_new_pixmap (MailCheck *mc)
{
	gtk_widget_hide (mc->containee);
	gtk_container_remove (GTK_CONTAINER (mc->bin), mc->containee);
	
	if (mc->selected_pixmap_name == mc->mailcheck_text_only) {
		mc->report_mail_mode = REPORT_MAIL_USE_TEXT;
		mc->containee = mc->label;
		g_free(mc->animation_file);
		mc->animation_file = NULL;
	} else {
		char *fname = g_concat_dir_and_file ("mailcheck",
						     mc->selected_pixmap_name);
		char *full;
		
		full = gnome_pixmap_file (fname);
		free (fname);
		
		if(full != NULL &&
		   mailcheck_load_animation (mc, full)) {
			mc->containee = mc->da;
			g_free(mc->animation_file);
			mc->animation_file = full;
		} else {
			g_free (full);
			mc->report_mail_mode = REPORT_MAIL_USE_TEXT;
			mc->containee = mc->label;
			g_free(mc->animation_file);
			mc->animation_file = NULL;
		}
	}
	mail_check_timeout (mc);
	gtk_widget_set_uposition (GTK_WIDGET (mc->containee), 0, 0);
	gtk_container_add (GTK_CONTAINER (mc->bin), mc->containee);
	gtk_widget_show (mc->containee);
}

static void
apply_properties_callback (GtkWidget *widget, gint page, gpointer data)
{
	char *text;
	MailCheck *mc = (MailCheck *)data;
	
	if(page!=-1) return;
	
	mc->update_freq = 1000 * (guint)(gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (mc->sec_spin)) + 
					 60 * gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (mc->min_spin)));
	/* do this here since we can no longer make the seconds
	 * minimum 1
	 */
	if (mc->update_freq == 0) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON (mc->sec_spin), 0.0);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON (mc->min_spin), 1.0);
		mc->update_freq = 60*1000;
	}

	if(mc->mail_timeout != 0)
		gtk_timeout_remove (mc->mail_timeout);
	mc->mail_timeout = gtk_timeout_add (mc->update_freq, mail_check_timeout, mc);
	
	if (mc->clicked_cmd) {
		g_free(mc->pre_check_cmd);
		mc->pre_check_cmd = NULL;
	}

	text = gtk_entry_get_text (GTK_ENTRY(mc->pre_check_cmd_entry));
	
	if (strlen (text) > 0)
		mc->pre_check_cmd = g_strdup (text);
	mc->pre_check_enabled = GTK_TOGGLE_BUTTON(mc->pre_check_cmd_check)->active;
	
	if (mc->clicked_cmd) {
		g_free(mc->clicked_cmd);
		mc->clicked_cmd = NULL;
        }

        text = gtk_entry_get_text (GTK_ENTRY(mc->clicked_cmd_entry));

        if (strlen(text) > 0)
                mc->clicked_cmd = g_strdup(text);
	mc->clicked_enabled = GTK_TOGGLE_BUTTON(mc->clicked_cmd_check)->active;

	if (mc->newmail_cmd) {
		g_free(mc->newmail_cmd);
		mc->newmail_cmd = NULL;
	}

	text = gtk_entry_get_text (GTK_ENTRY(mc->newmail_cmd_entry));

	if (strlen(text) > 0)
		mc->newmail_cmd = g_strdup(text);
	mc->newmail_enabled = GTK_TOGGLE_BUTTON(mc->newmail_cmd_check)->active;

	if (mc->anim_changed)
		load_new_pixmap (mc);
	
	mc->anim_changed = FALSE;
        
	if (mc->mail_file) {
		g_free(mc->mail_file);
		mc->mail_file = NULL;
	}

	text = gtk_entry_get_text (GTK_ENTRY (mc->mailfile_entry));

	if (strlen(text) > 0)
		mc->mail_file = g_strdup(text);

	if (mc->remote_server) {
		g_free(mc->remote_server);
		mc->remote_server = NULL;
	}

        text = gtk_entry_get_text (GTK_ENTRY(mc->remote_server_entry));
	
	if (strlen(text) > 0)
		mc->remote_server = g_strdup(text);

	if (mc->remote_username) {
		g_free(mc->remote_username);
		mc->remote_username = NULL;
	}

        text = gtk_entry_get_text (GTK_ENTRY(mc->remote_username_entry));

	if (strlen(text) > 0)
		mc->remote_username = g_strdup(text);

	if (mc->remote_password) {
		g_free(mc->remote_password);
		mc->remote_password = NULL;
	}

        text = gtk_entry_get_text (GTK_ENTRY(mc->remote_password_entry));

	if (strlen(text) > 0)
		mc->remote_password = g_strdup(text);

        if (mc->remote_folder) {
                g_free(mc->remote_folder);
                mc->remote_folder = NULL;
        }
 
        text = gtk_entry_get_text (GTK_ENTRY(mc->remote_folder_entry));
 
        if (strlen(text) > 0)
                mc->remote_folder = g_strdup(text);

	if (mc->pre_remote_command) {
		g_free(mc->pre_remote_command);
		mc->pre_remote_command = NULL;
	}

        text = gtk_entry_get_text (GTK_ENTRY(mc->pre_remote_command_entry));
	
	if (strlen(text) > 0)
		mc->pre_remote_command = g_strdup(text);
        
	mc->mailbox_type = mc->mailbox_type_temp;

	mc->play_sound = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mc->play_sound_check));

	if ((mc->mailbox_type != MAILBOX_POP3) &&
	    (mc->mailbox_type != MAILBOX_IMAP) &&
	    (mc->remote_handle != NULL)) {
		helper_whack_handle (mc->remote_handle);
		mc->remote_handle = NULL;
	}
}

static void
make_remote_widgets_sensitive(MailCheck *mc)
{
	gboolean b = mc->mailbox_type_temp != MAILBOX_LOCAL &&
	             mc->mailbox_type_temp != MAILBOX_LOCALDIR;
        gboolean f = mc->mailbox_type_temp == MAILBOX_IMAP;
	
	gtk_widget_set_sensitive (mc->mailfile_fentry, !b);
	gtk_widget_set_sensitive (mc->mailfile_label, !b);
	
	gtk_widget_set_sensitive (mc->remote_server_entry, b);
	gtk_widget_set_sensitive (mc->remote_password_entry, b);
	gtk_widget_set_sensitive (mc->remote_username_entry, b);
        gtk_widget_set_sensitive (mc->remote_folder_entry, f);
	gtk_widget_set_sensitive (mc->remote_server_label, b);
	gtk_widget_set_sensitive (mc->remote_password_label, b);
	gtk_widget_set_sensitive (mc->remote_username_label, b);
        gtk_widget_set_sensitive (mc->remote_folder_label, f);
	gtk_widget_set_sensitive (mc->pre_remote_command_entry, b);
	gtk_widget_set_sensitive (mc->pre_remote_command_label, b);
}

static void 
set_mailbox_selection (GtkWidget *widget, gpointer data)
{
	MailCheck *mc = gtk_object_get_user_data(GTK_OBJECT(widget));
	mc->mailbox_type_temp = GPOINTER_TO_INT(data);
        
        make_remote_widgets_sensitive(mc);
	gnome_property_box_changed (GNOME_PROPERTY_BOX (mc->property_window));
}

static GtkWidget *
mailbox_properties_page(MailCheck *mc)
{
	GtkWidget *vbox, *hbox, *l, *l2, *item;

	mc->type = 1;

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
	gtk_widget_show (vbox);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);        

	l = gtk_label_new(_("Mailbox resides on:"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);

	mc->remote_option_menu = l = gtk_option_menu_new();
        
	l2 = gtk_menu_new();
	item = gtk_menu_item_new_with_label(_("Local mailspool")); 
	gtk_widget_show(item);
	gtk_object_set_user_data(GTK_OBJECT(item), mc);
	gtk_signal_connect (GTK_OBJECT(item), "activate", 
			    GTK_SIGNAL_FUNC(set_mailbox_selection), 
			    GINT_TO_POINTER(MAILBOX_LOCAL));
	gtk_menu_append(GTK_MENU(l2), item);

	item = gtk_menu_item_new_with_label(_("Local maildir")); 
	gtk_widget_show(item);
	gtk_object_set_user_data(GTK_OBJECT(item), mc);
	gtk_signal_connect (GTK_OBJECT(item), "activate", 
			    GTK_SIGNAL_FUNC(set_mailbox_selection), 
			    GINT_TO_POINTER(MAILBOX_LOCALDIR));
	gtk_menu_append(GTK_MENU(l2), item);

	item = gtk_menu_item_new_with_label(_("Remote POP3-server")); 
	gtk_widget_show(item);
	gtk_object_set_user_data(GTK_OBJECT(item), mc);
	gtk_signal_connect (GTK_OBJECT(item), "activate", 
			    GTK_SIGNAL_FUNC(set_mailbox_selection), 
			    GINT_TO_POINTER(MAILBOX_POP3));
        
	gtk_menu_append(GTK_MENU(l2), item);
	item = gtk_menu_item_new_with_label(_("Remote IMAP-server")); 
	gtk_widget_show(item);
	gtk_object_set_user_data(GTK_OBJECT(item), mc);
	gtk_signal_connect (GTK_OBJECT(item), "activate", 
			    GTK_SIGNAL_FUNC(set_mailbox_selection), 
			    GINT_TO_POINTER(MAILBOX_IMAP));
	gtk_menu_append(GTK_MENU(l2), item);
	
	gtk_widget_show(l2);
  
	gtk_option_menu_set_menu(GTK_OPTION_MENU(l), l2);
	gtk_option_menu_set_history(GTK_OPTION_MENU(l), mc->mailbox_type_temp = mc->mailbox_type);
	gtk_widget_show(l);
  
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
  
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	mc->mailfile_label = l = gtk_label_new(_("Mail spool file:"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);

	mc->mailfile_fentry = l = gnome_file_entry_new ("spool file", _("Browse"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, TRUE, TRUE, 0);

	mc->mailfile_entry = l = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY (l));
	gtk_entry_set_text(GTK_ENTRY(l), mc->mail_file);
	gtk_signal_connect(GTK_OBJECT(l), "changed",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);  
  
	mc->remote_server_label = l = gtk_label_new(_("Mail server:"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
  
	mc->remote_server_entry = l = gtk_entry_new();
	if (mc->remote_server)
		gtk_entry_set_text(GTK_ENTRY(l), mc->remote_server);
  	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, TRUE, TRUE, 0);      
	
	gtk_signal_connect(GTK_OBJECT(l), "changed",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);
	
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);  
  
	mc->remote_username_label = l = gtk_label_new(_("Username:"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	
	mc->remote_username_entry = l = gtk_entry_new();
	if (mc->remote_username)
		gtk_entry_set_text(GTK_ENTRY(l), mc->remote_username);
  
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);      
  
	gtk_signal_connect(GTK_OBJECT(l), "changed",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);

	mc->remote_password_label = l = gtk_label_new(_("Password:"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	
	mc->remote_password_entry = l = gtk_entry_new();
	if (mc->remote_password)
		gtk_entry_set_text(GTK_ENTRY(l), mc->remote_password);
	gtk_entry_set_visibility(GTK_ENTRY (l), FALSE);
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);      
	
	gtk_signal_connect(GTK_OBJECT(l), "changed",
                     GTK_SIGNAL_FUNC(property_box_changed), mc);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);  

        mc->remote_folder_label = l = gtk_label_new(_("Folder:"));
        gtk_widget_show(l);
        gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
 
        mc->remote_folder_entry = l = gtk_entry_new();
        if (mc->remote_folder)
                gtk_entry_set_text(GTK_ENTRY(l), mc->remote_folder);
  
        gtk_widget_show(l);
        gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
  
        gtk_signal_connect(GTK_OBJECT(l), "changed",
                           GTK_SIGNAL_FUNC(property_box_changed), mc);
 
        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
        gtk_widget_show (hbox);  

	mc->pre_remote_command_label = l = gtk_label_new(_("Command to run before we check for mail:"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
  
	mc->pre_remote_command_entry = l = gtk_entry_new();
	if (mc->pre_remote_command)
		gtk_entry_set_text(GTK_ENTRY(l), mc->pre_remote_command);
  	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, TRUE, TRUE, 0);      
	
	gtk_signal_connect(GTK_OBJECT(l), "changed",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);
  
	make_remote_widgets_sensitive(mc);
	
	return vbox;
}

static GtkWidget *
mailcheck_properties_page (MailCheck *mc)
{
	GtkWidget *vbox, *hbox, *l, *table, *frame;
	GtkObject *freq_a;

	mc->type = 0;

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
	gtk_widget_show (vbox);

	frame = gtk_frame_new (_("Execute"));
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	table = gtk_table_new (3, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (table), GNOME_PAD/2);
	gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD/2);
	gtk_container_set_border_width (GTK_CONTAINER (table), GNOME_PAD/2);
	gtk_widget_show(table);
	gtk_container_add (GTK_CONTAINER (frame), table);

	l = gtk_check_button_new_with_label(_("Before each update:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l), mc->pre_check_enabled);
	gtk_signal_connect(GTK_OBJECT(l), "toggled",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);
	gtk_widget_show(l);
	mc->pre_check_cmd_check = l;
	
	gtk_table_attach (GTK_TABLE (table), mc->pre_check_cmd_check, 
			  0, 1, 0, 1, GTK_FILL, 0, 0, 0);
				   
	
	mc->pre_check_cmd_entry = gtk_entry_new();
	if(mc->pre_check_cmd)
		gtk_entry_set_text(GTK_ENTRY(mc->pre_check_cmd_entry), 
				   mc->pre_check_cmd);
	gtk_signal_connect(GTK_OBJECT(mc->pre_check_cmd_entry), "changed",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);
	gtk_widget_show(mc->pre_check_cmd_entry);
	gtk_table_attach_defaults (GTK_TABLE (table), mc->pre_check_cmd_entry,
				   1, 2, 0, 1);

	l = gtk_check_button_new_with_label (_("When new mail arrives:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l), mc->newmail_enabled);
	gtk_signal_connect(GTK_OBJECT(l), "toggled",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);
	gtk_widget_show(l);
	gtk_table_attach (GTK_TABLE (table), l, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	mc->newmail_cmd_check = l;

	mc->newmail_cmd_entry = gtk_entry_new();
	if (mc->newmail_cmd) {
		gtk_entry_set_text(GTK_ENTRY(mc->newmail_cmd_entry),
				   mc->newmail_cmd);
	}
	gtk_signal_connect(GTK_OBJECT (mc->newmail_cmd_entry), "changed",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);
	gtk_widget_show(mc->newmail_cmd_entry);
	gtk_table_attach_defaults (GTK_TABLE (table), mc->newmail_cmd_entry,
				    1, 2, 1, 2);

        l = gtk_check_button_new_with_label (_("When clicked:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l), mc->clicked_enabled);
	gtk_signal_connect(GTK_OBJECT(l), "toggled",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);
        gtk_widget_show(l);
	gtk_table_attach (GTK_TABLE (table), l, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	mc->clicked_cmd_check = l;

        mc->clicked_cmd_entry = gtk_entry_new();
        if(mc->clicked_cmd) {
		gtk_entry_set_text(GTK_ENTRY(mc->clicked_cmd_entry), 
				   mc->clicked_cmd);
        }
        gtk_signal_connect(GTK_OBJECT(mc->clicked_cmd_entry), "changed",
                           GTK_SIGNAL_FUNC(property_box_changed), mc);
        gtk_widget_show(mc->clicked_cmd_entry);
	gtk_table_attach_defaults (GTK_TABLE (table), mc->clicked_cmd_entry,
				   1, 2, 2, 3);

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
        gtk_widget_show (hbox); 
        
        l = gtk_label_new (_("Check for mail every"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);

	freq_a = gtk_adjustment_new((float)((mc->update_freq/1000)/60), 0, 1440, 1, 5, 5);
	mc->min_spin = gtk_spin_button_new( GTK_ADJUSTMENT (freq_a), 1, 0);
	gtk_signal_connect(GTK_OBJECT(freq_a), "value_changed",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);
	gtk_signal_connect(GTK_OBJECT(mc->min_spin), "changed",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);
	gtk_box_pack_start (GTK_BOX (hbox), mc->min_spin,  FALSE, FALSE, 0);
	gtk_widget_show(mc->min_spin);
	
	l = gtk_label_new (_("minutes"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	
	freq_a = gtk_adjustment_new((float)((mc->update_freq/1000)%60), 0, 59, 1, 5, 5);
	mc->sec_spin  = gtk_spin_button_new (GTK_ADJUSTMENT (freq_a), 1, 0);
	gtk_signal_connect(GTK_OBJECT(freq_a), "value_changed",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);
	gtk_signal_connect(GTK_OBJECT(mc->sec_spin), "changed",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);
	gtk_box_pack_start (GTK_BOX (hbox), mc->sec_spin,  FALSE, FALSE, 0);
	gtk_widget_show(mc->sec_spin);
	
	l = gtk_label_new (_("seconds"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	
	mc->play_sound_check = gtk_check_button_new_with_label(_("Play a sound when new mail arrives"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mc->play_sound_check), mc->play_sound);
	gtk_signal_connect(GTK_OBJECT(mc->play_sound_check), "toggled",
			   GTK_SIGNAL_FUNC(property_box_changed), mc);
	gtk_widget_show(mc->play_sound_check);
	gtk_box_pack_start(GTK_BOX (vbox), mc->play_sound_check, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);  

	l = gtk_label_new (_("Select animation"));
	gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
	gtk_widget_show (l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), mailcheck_get_animation_menu (mc), FALSE, FALSE, 0);

	return vbox;
}

static void
phelp_cb (GtkWidget *w, gint tab, gpointer data)
{
	GnomeHelpMenuEntry help_entry = { "mailcheck_applet", NULL };

	char *das_names[] =  { "index.html#MAILCHECK-PREFS",
			       "index.html#MAILCHECK-SETTINGS-MAILBOX-FIG" };

	help_entry.path = das_names[((MailCheck *)data)->type];
	gnome_help_display(NULL, &help_entry);
}	

static void
mailcheck_properties (AppletWidget *applet, gpointer data)
{
	GtkWidget *p;

	MailCheck *mc = data;

	if (mc->property_window != NULL) {
		gdk_window_raise(mc->property_window->window);
		return; /* Only one instance of the properties dialog! */
	}
	
	mc->property_window = gnome_property_box_new ();
	gtk_window_set_wmclass (GTK_WINDOW (mc->property_window),
				"mailcheck", "Mailcheck");
	gtk_window_set_title (GTK_WINDOW (mc->property_window),
			      _("Mail check properties"));
	gnome_window_icon_set_from_file (GTK_WINDOW (mc->property_window),
					 GNOME_ICONDIR"/gnome-mailcheck.png");

	p = mailcheck_properties_page (mc);
	gnome_property_box_append_page (GNOME_PROPERTY_BOX(mc->property_window),
					p, gtk_label_new (_("Mail check")));
	p = mailbox_properties_page (mc);
	gnome_property_box_append_page (GNOME_PROPERTY_BOX(mc->property_window),
					p, gtk_label_new (_("Mailbox")));

	gtk_signal_connect (GTK_OBJECT (mc->property_window), "apply",
			    GTK_SIGNAL_FUNC(apply_properties_callback), mc);
	gtk_signal_connect (GTK_OBJECT (mc->property_window), "destroy",
			    GTK_SIGNAL_FUNC(close_callback), mc);
	gtk_signal_connect (GTK_OBJECT (mc->property_window), "help",
			    GTK_SIGNAL_FUNC(phelp_cb), mc);

	gtk_widget_show (mc->property_window);
}

static void
check_callback (AppletWidget *applet, gpointer data)
{
	MailCheck *mc = data;

	mail_check_timeout(mc);
}


static gint
applet_save_session(GtkWidget *w,
		    const char *privcfgpath,
		    const char *globcfgpath,
		    gpointer data)
{
	MailCheck *mc = data;

	gnome_config_push_prefix(privcfgpath);
	gnome_config_set_string("mail/animation_file",
                          mc->animation_file?mc->animation_file:"");
	gnome_config_set_int("mail/update_frequency", mc->update_freq);
	gnome_config_set_string("mail/exec_command",
				mc->pre_check_cmd?mc->pre_check_cmd:"");
	gnome_config_set_bool("mail/exec_enabled",mc->pre_check_enabled);
	gnome_config_set_string("mail/newmail_command",
				mc->newmail_cmd?mc->newmail_cmd:"");
	gnome_config_set_bool("mail/newmail_enabled",mc->newmail_enabled);
	gnome_config_set_string("mail/clicked_command",
				mc->clicked_cmd?mc->clicked_cmd:"");
	gnome_config_set_bool("mail/clicked_enabled",mc->clicked_enabled);
	gnome_config_set_string("mail/mail_file",
				mc->mail_file?mc->mail_file:"");
        gnome_config_private_set_string("mail/remote_server", 
				mc->remote_server?mc->remote_server:"");
        gnome_config_private_set_string("mail/remote_username", 
				mc->remote_username?mc->remote_username:"");
        gnome_config_private_set_string("mail/remote_password", 
				mc->remote_password?mc->remote_password:"");
        gnome_config_set_string("mail/remote_folder",
                                mc->remote_folder?mc->remote_folder:"");
        gnome_config_private_set_string("mail/pre_remote_command", 
				mc->pre_remote_command ? mc->pre_remote_command : "");
        gnome_config_set_int("mail/mailbox_type", (int) mc->mailbox_type);
	gnome_config_set_bool("mail/play_sound", mc->play_sound);

	gnome_config_pop_prefix();

	gnome_config_sync();
	gnome_config_drop_all();

	return FALSE;
}

static void
mailcheck_about(AppletWidget *a_widget, gpointer a_data)
{
	MailCheck *mc = a_data;
	static const gchar     *authors [] =
	{
		"Miguel de Icaza <miguel@kernel.org>",
		"Jacob Berkman <jberkman@andrew.cmu.edu>",
		"Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>",
		"Lennart Poettering <poettering@gmx.net>",
		NULL
	};

	if (mc->about != NULL)
	{
		gtk_widget_show_now(mc->about);
		gdk_window_raise(mc->about->window);
		return;
	}
	
	mc->about = gnome_about_new (_("Mail check Applet"), "1.1",
				     _("(c) 1998-2000 the Free Software Foundation"),
				     authors,
				     _("Mail check notifies you when new mail arrives in your mailbox"),
				     NULL);
	gtk_window_set_wmclass (GTK_WINDOW (mc->about),
				"mailcheck", "Mailcheck");
	gnome_window_icon_set_from_file (GTK_WINDOW (mc->about),
					 GNOME_ICONDIR"/gnome-mailcheck.png");
	gtk_signal_connect( GTK_OBJECT(mc->about), "destroy",
			    GTK_SIGNAL_FUNC(gtk_widget_destroyed), &mc->about );
	gtk_widget_show(mc->about);
}

/*this is when the panel size changes */
static void
applet_change_pixel_size(GtkWidget * w, int size, gpointer data)
{
	MailCheck *mc = data;
	char *fname;

	if(mc->report_mail_mode == REPORT_MAIL_USE_TEXT)
		return;

	mc->size = size;
	fname = mail_animation_filename (mc);

	gtk_drawing_area_size (GTK_DRAWING_AREA(mc->da),size,size);
	gtk_widget_set_usize (GTK_WIDGET(mc->da), size, size);
	
	if (!fname)
		return;

	mailcheck_load_animation (mc, fname);
	g_free (fname);
}

static void
help_callback (AppletWidget *widget, gpointer data)
{
	GnomeHelpMenuEntry help_ref = { "mailcheck_applet", "index.html"};
	gnome_help_display (NULL, &help_ref);
}

GtkWidget *
make_mailcheck_applet(const gchar *goad_id)
{
	GtkWidget *mailcheck;
	MailCheck *mc;
	char *emailfile;
	char *query;

	applet = applet_widget_new(goad_id);
	if (!applet) {
		g_warning(_("Can't create applet!\n"));
		return NULL;
	}

	mc = g_new0(MailCheck, 1);
	mc->animation_file = NULL;
	mc->property_window = NULL;
	mc->anim_changed = FALSE;
	mc->anymail = mc->unreadmail = mc->newmail = FALSE;
	mc->mail_timeout = 0;
	mc->animation_tag = 0;

	/*initial state*/
	mc->report_mail_mode = REPORT_MAIL_USE_ANIMATION;

	gnome_config_push_prefix(APPLET_WIDGET(applet)->privcfgpath);

	mc->mail_file = gnome_config_get_string("mail/mail_file");

	if (mc->mail_file == NULL) {
		mc->mail_file = g_getenv ("MAIL");
		if (mc->mail_file == NULL) {
			char *user = g_getenv ("USER");
			if (user == NULL)
				return NULL;

			mc->mail_file = g_strdup_printf ("/var/spool/mail/%s",
							 user);
		} else
			mc->mail_file = g_strdup (mc->mail_file);
	}

	emailfile = gnome_unconditional_pixmap_file("mailcheck/email.png");
	query = g_strconcat("mail/animation_file=",emailfile,NULL);
	g_free(emailfile);
	mc->animation_file = gnome_config_get_string(query);
	g_free(query);

	mc->update_freq = gnome_config_get_int("mail/update_frequency=120000");
		
	mc->pre_check_cmd = gnome_config_get_string("mail/exec_command");
	mc->pre_check_enabled = gnome_config_get_bool("mail/exec_enabled=0");

	mc->newmail_cmd = gnome_config_get_string("mail/newmail_command");
	mc->newmail_enabled = gnome_config_get_bool("mail/newmail_enabled=0");

        mc->clicked_cmd = gnome_config_get_string("mail/clicked_command");
	mc->clicked_enabled = gnome_config_get_bool("mail/clicked_enabled=0");

	mc->remote_server = gnome_config_private_get_string("mail/remote_server=mail");
	mc->pre_remote_command = gnome_config_private_get_string("mail/pre_remote_command=");
	
	query = g_strconcat("mail/remote_username=", g_getenv("USER"), NULL);
	mc->remote_username = gnome_config_private_get_string(query);
	g_free(query);

	mc->remote_password = gnome_config_private_get_string("mail/remote_password");
        mc->remote_folder = gnome_config_get_string("mail/remote_folder");
	mc->mailbox_type = gnome_config_get_int("mail/mailbox_type=0");

	mc->play_sound = gnome_config_get_bool("mail/play_sound=false");

	gnome_config_pop_prefix();

	mc->mailcheck_text_only = _("Text only");
	
	mc->size = PIXEL_SIZE_STANDARD;

	gtk_signal_connect(GTK_OBJECT(applet), "change_pixel_size",
			   GTK_SIGNAL_FUNC(applet_change_pixel_size),
			   mc);

	mailcheck = create_mail_widgets (mc);
	gtk_widget_show(mailcheck);
	applet_widget_add (APPLET_WIDGET (applet), mailcheck);

        gtk_signal_connect(GTK_OBJECT(mc->ebox), "button_press_event",
                           GTK_SIGNAL_FUNC(exec_clicked_cmd), mc);

	gtk_signal_connect(GTK_OBJECT(applet),"save_session",
			   GTK_SIGNAL_FUNC(applet_save_session),
			   mc);

	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "properties",
					      GNOME_STOCK_MENU_PROP,
					      _("Properties..."),
					      mailcheck_properties,
					      mc);
	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "check_mail",
					      GNOME_STOCK_MENU_MAIL,
					      _("Check for mail"),
					      check_callback,
					      mc);
	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "help",
					      GNOME_STOCK_PIXMAP_HELP,
					      _("Help"),
					      help_callback,
					      NULL);
	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "about",
					      GNOME_STOCK_MENU_ABOUT,
					      _("About..."),
					      mailcheck_about,
					      mc);	

	gtk_widget_show (applet);

	/*
	 * check the mail right now, so we don't have to wait
	 * for the first timeout
	 */
	mail_check_timeout (mc);

	return applet;
}
