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
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <panel-applet.h>
#include <panel-applet-gconf.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomeui/gnome-window-icon.h>

#include "popcheck.h"
#include "remote-helper.h"
#include "mailcheck.h"

#include "egg-screen-help.h"
#include "egg-screen-exec.h"

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
	gboolean anymail;

	/* whether new mail has arrived */
	gboolean newmail;

	/* number of unread/total mails */
	int unreadmail;
	int totalmail;

	/* whether to automatically check for mails */
	gboolean auto_update;

	/* interval to check for mails in milliseconds */
	guint update_freq;

	/* whether to set mc->newmail and mc->unreadmail to 0 if the applet was clicked */
	gboolean reset_on_clicked;

	/* execute a command when the applet is clicked (launch email prog) */
	char *clicked_cmd;
	gboolean clicked_enabled;

	/* execute a command when new mail arrives (play a sound etc.) */
	char *newmail_cmd;
	gboolean newmail_enabled;

	/* execute a command before checking email (fetchmail etc.) */
	char *pre_check_cmd;
	gboolean pre_check_enabled;	

	PanelApplet *applet;
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

	GtkWidget *password_dialog;

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

	gulong applet_realized_signal;

	/* see remote-helper.h */
	gpointer remote_handle;
};

static int mail_check_timeout (gpointer data);
static void after_mail_check (MailCheck *mc);

static void applet_load_prefs(MailCheck *mc);

static void set_atk_name_description (GtkWidget *widget, const gchar *name,
					const gchar *description);
static void set_atk_relation (GtkWidget *label, GtkWidget *entry, AtkRelationType);
static void got_remote_answer (int mails, gpointer data);
static void null_remote_handle (gpointer data);

#define WANT_BITMAPS(x) (x == REPORT_MAIL_USE_ANIMATION || x == REPORT_MAIL_USE_BITMAP)

static void
set_tooltip (GtkWidget  *applet,
	     const char *tip)
{
	GtkTooltips *tooltips;

	tooltips = g_object_get_data (G_OBJECT (applet), "tooltips");
	if (!tooltips) {
		tooltips = gtk_tooltips_new ();
		g_object_ref (tooltips);
		gtk_object_sink (GTK_OBJECT (tooltips));
		g_object_set_data_full (
			G_OBJECT (applet), "tooltips", tooltips,
			(GDestroyNotify) g_object_unref);
	}

	gtk_tooltips_set_tip (tooltips, applet, tip, NULL);
}

static void
mailcheck_execute_shell (MailCheck  *mailcheck,
			 const char *command)
{
	GError *error = NULL;

	egg_screen_execute_command_line_async (
		gtk_widget_get_screen (GTK_WIDGET (mailcheck->applet)), command, &error);
	if (error) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL,
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("There was an error executing %s: %s"),
						 command,
						 error->message);

		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_widget_get_screen (GTK_WIDGET (mailcheck->applet)));

		gtk_widget_show (dialog);

		g_error_free (error);
	}
}

static G_CONST_RETURN char *
mail_animation_filename (MailCheck *mc)
{
	if (!mc->animation_file) {
		mc->animation_file =
			gnome_program_locate_file (
				NULL, GNOME_FILE_DOMAIN_PIXMAP,
				"mailcheck/email.png", TRUE, NULL);

		return mc->animation_file;

	} else if (mc->animation_file [0]) {
		if (g_file_test (mc->animation_file, G_FILE_TEST_EXISTS))
			return mc->animation_file;

		g_free (mc->animation_file);
		mc->animation_file = NULL;

		return NULL;
	} else
		/* we are using text only, since the filename was "" */
		return NULL;
}

static int
calc_dir_contents (char *dir)
{
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
check_remote_mailbox (MailCheck *mc)
{
	if (!mc->real_password || !mc->remote_username || !mc->remote_server)
		return;

	if (mc->mailbox_type == MAILBOX_POP3)
		mc->remote_handle = helper_pop3_check (got_remote_answer,
						       mc,
						       null_remote_handle,
						       mc->pre_remote_command,
						       mc->remote_server,
						       mc->remote_username,
						       mc->real_password);
	else if (mc->mailbox_type == MAILBOX_IMAP)
		helper_imap_check (got_remote_answer,
				   mc,
				   null_remote_handle,
				   mc->pre_remote_command,
				   mc->remote_server,
				   mc->remote_username,
				   mc->real_password,
				   mc->remote_folder);
}

static void
password_response_cb (GtkWidget  *dialog,
		      int         response_id,
		      MailCheck  *mc)
{

	switch (response_id) {
		GtkWidget *entry;

	case GTK_RESPONSE_OK:
		entry = g_object_get_data (G_OBJECT (dialog), "password_entry");
		mc->real_password = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
		check_remote_mailbox (mc);
		break;
	}

	gtk_widget_destroy (dialog);
	mc->password_dialog = NULL;
}
static void
get_remote_password (MailCheck *mc)
{
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;

	if (mc->password_dialog) {
		gtk_window_set_screen (GTK_WINDOW (mc->password_dialog),
				       gtk_widget_get_screen (GTK_WIDGET (mc->applet)));
		gtk_window_present (GTK_WINDOW (mc->password_dialog));
		return;
	}

	mc->password_dialog = dialog =
		gtk_dialog_new_with_buttons (
			_("Inbox Monitor"), NULL, 0,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	label = gtk_label_new (_("You didn't set a password in the preferences for the Inbox Monitor,\nso you have to enter it each time it starts up."));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, FALSE, FALSE, GNOME_PAD_BIG);
	gtk_widget_show (label);

	hbox = gtk_hbox_new (FALSE, 1);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    hbox, FALSE, FALSE, GNOME_PAD_SMALL);

	label = gtk_label_new_with_mnemonic (_("Please enter your mailserver's _password:"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new ();

	set_atk_name_description (entry, _("Password Entry box"), "");
	set_atk_relation (entry, label, ATK_RELATION_LABELLED_BY);	
	gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
	gtk_widget_show_all (hbox);
	gtk_widget_grab_focus (GTK_WIDGET (entry));

	gtk_window_set_screen (GTK_WINDOW (dialog),
			       gtk_widget_get_screen (GTK_WIDGET (mc->applet)));

	g_signal_connect (dialog, "response",
                          G_CALLBACK (password_response_cb), mc);

	g_object_set_data (G_OBJECT (dialog), "password_entry", entry);
	gtk_widget_show (GTK_WIDGET (dialog));
}

static void
got_remote_answer (int mails, gpointer data)
{
	MailCheck *mc = data;
	int old_unreadmail;

	mc->remote_handle = NULL;
	
	if (mails == -1) {
		GtkWidget *dialog;

		/* Disable automatic updating */
		mc->auto_update = FALSE;

		if(mc->mail_timeout != 0) {
			gtk_timeout_remove(mc->mail_timeout);
			mc->mail_timeout = 0;
		}

		/* Notify about an error and keep the current mail status */
		dialog = gtk_message_dialog_new (NULL,
						 0,/* Flags */
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("The Inbox Monitor failed to check your mails and thus automatic updating has been deactivated for now.\nMaybe you used a wrong server, username or password?")); 

		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_widget_get_screen (GTK_WIDGET (mc->applet)));

		g_signal_connect_swapped (G_OBJECT (dialog), "response",
					  G_CALLBACK (gtk_widget_destroy),
					  dialog);
		gtk_widget_show_all (dialog);

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
applet_realized_cb (GtkWidget *widget, gpointer data)
{
	MailCheck *mc = data;
	mail_check_timeout (mc);
	g_signal_handler_disconnect (G_OBJECT(widget), mc->applet_realized_signal);
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

		} else if (!mc->real_password)
			get_remote_password (mc);

		check_remote_mailbox (mc);
	}
	else if (mc->mailbox_type == MAILBOX_LOCAL) {
		status = stat (mc->mail_file, &s);
		if (status < 0) {
			oldsize = 0;
			mc->anymail = mc->newmail = mc->unreadmail = 0;
			after_mail_check (mc);
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

		after_mail_check (mc);
	}
	else if (mc->mailbox_type == MAILBOX_LOCALDIR) {
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

		after_mail_check (mc);
	}
}

static gboolean
mailcheck_load_animation (MailCheck *mc, const char *fname)
{
	int width, height;
	int pbwidth, pbheight;
	GdkPixbuf *pb;

	if (mc->email_pixmap)
		g_object_unref (mc->email_pixmap);

	if (mc->email_mask)
		g_object_unref (mc->email_mask);

	mc->email_pixmap = NULL;
	mc->email_mask = NULL;

	pb = gdk_pixbuf_new_from_file (fname, NULL);
	if (!pb)
		return FALSE;

	pbwidth = gdk_pixbuf_get_width (pb);
	pbheight = gdk_pixbuf_get_height (pb);

	if(pbheight != mc->size) {
		GdkPixbuf *pbt;
		height = mc->size;
		width = pbwidth*((double)height/pbheight);

		pbt = gdk_pixbuf_scale_simple(pb, width, height,
					      GDK_INTERP_NEAREST);
		g_object_unref (pb);
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
	else
		mc->report_mail_mode = REPORT_MAIL_USE_ANIMATION;
	mc->nframe = 0;

	gdk_pixbuf_render_pixmap_and_mask(pb,
					  &mc->email_pixmap,
					  &mc->email_mask,
					  128);

	g_object_unref (pb);
	
	return TRUE;
}

static int
next_frame (gpointer data)
{
	MailCheck *mc = data;

	mc->nframe = (mc->nframe + 1) % mc->frames;
	if (mc->nframe == 0)
		mc->nframe = 1;

	gtk_widget_queue_draw (mc->da);

	return TRUE;
}

static void
after_mail_check (MailCheck *mc)
{
	static const char *supinfo[] = {"mailcheck", "new-mail", NULL};
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

	if (mc->newmail) {
		if(mc->play_sound)
			gnome_triggers_vdo("You've got new mail!", "program", supinfo);

		if (mc->newmail_enabled &&
		    mc->newmail_cmd && 
		    (strlen(mc->newmail_cmd) > 0))
			mailcheck_execute_shell (mc, mc->newmail_cmd);
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

		gtk_widget_queue_draw (mc->da);

		break;
	case REPORT_MAIL_USE_BITMAP:
		if (mc->anymail){
			if (mc->newmail)
				mc->nframe = 2;
			else
				mc->nframe = 1;
		} else
			mc->nframe = 0;

		gtk_widget_queue_draw (mc->da);

		break;
	case REPORT_MAIL_USE_TEXT:
		gtk_label_set_text (GTK_LABEL (mc->label), text);
		break;
	}

	set_tooltip (GTK_WIDGET (mc->applet), text);
	g_free (text);
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
		
		if(mc->mail_timeout != 0) {
			gtk_timeout_remove (mc->mail_timeout);
			mc->mail_timeout = 0;
		}

		mailcheck_execute_shell (mc, mc->pre_check_cmd);

		mc->mail_timeout = gtk_timeout_add(mc->update_freq, mail_check_timeout, mc);
	}

	check_mail_file_status (mc);
	
	if (mc->auto_update)      
		return TRUE;
	else
		/* This handler should just run once */
		return FALSE;
}

/*
 * this gets called when we have to redraw the nice icon
 */
static gint
icon_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	MailCheck *mc = data;
	int        h = mc->size;

	gdk_draw_drawable (
		mc->da->window, mc->da->style->black_gc,
		mc->email_pixmap, mc->nframe * h,
		0, 0, 0, h, h);

	return TRUE;
}

static gint
exec_clicked_cmd (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	MailCheck *mc = data;
	gboolean retval = FALSE;

	if (event->button == 1) {
		
		if (mc->clicked_enabled && mc->clicked_cmd && (strlen(mc->clicked_cmd) > 0))
			mailcheck_execute_shell (mc, mc->clicked_cmd);
		
		if (mc->reset_on_clicked) {
			mc->newmail = mc->unreadmail = 0;
			after_mail_check (mc);
		}

		retval = TRUE;
	}	
	return(retval);
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

	if (mc->email_pixmap)
		g_object_unref (mc->email_pixmap);

	if (mc->email_mask)
		g_object_unref (mc->email_mask);

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
	const char *fname;

	fname = mail_animation_filename (mc);

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
	g_signal_connect (G_OBJECT (mc->ebox), "destroy",
			    (GtkSignalFunc) mailcheck_destroy,
			    mc);

	mc->bin = gtk_hbox_new (0, 0);
	gtk_container_add(GTK_CONTAINER(mc->ebox), mc->bin);

	gtk_widget_show (mc->bin);
	
	if (mc->auto_update)
		mc->mail_timeout = gtk_timeout_add (mc->update_freq, mail_check_timeout, mc);
	else
		mc->mail_timeout = 0;

	/* The drawing area */
	mc->da = gtk_drawing_area_new ();
	gtk_widget_ref (mc->da);

	gtk_widget_set_size_request (mc->da, mc->size, mc->size);

	g_signal_connect (G_OBJECT(mc->da), "expose_event", (GtkSignalFunc)icon_expose, mc);
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

	gtk_container_add (GTK_CONTAINER (mc->bin), mc->containee);

	return mc->ebox;
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
		char *fname;
		char *full;

		fname = g_build_filename ("mailcheck", mc->selected_pixmap_name, NULL);
		full = gnome_program_locate_file (
				NULL, GNOME_FILE_DOMAIN_PIXMAP,
				fname, TRUE, NULL);
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

	gtk_container_add (GTK_CONTAINER (mc->bin), mc->containee);
	gtk_widget_show (mc->containee);
}

static void
animation_selected (GtkMenuItem *item, gpointer data)
{
	MailCheck *mc = g_object_get_data(G_OBJECT(item), "MailCheck");
	mc->selected_pixmap_name = data;
	
	load_new_pixmap (mc);
	panel_applet_gconf_set_string(mc->applet, "animation_file", 
				      mc->animation_file ? mc->animation_file : "", NULL);
}

static void
mailcheck_new_entry (MailCheck *mc, GtkWidget *menu, GtkWidget *item, char *s)
{
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	g_object_set_data (G_OBJECT (item), "MailCheck", mc);
	
	g_signal_connect_data (item, "activate", G_CALLBACK (animation_selected),
			       g_strdup (s), (GClosureNotify) g_free, 0);
}

static GtkWidget *
mailcheck_get_animation_menu (MailCheck *mc)
{
	GtkWidget *omenu, *menu, *item;
	struct     dirent *e;
	char      *dname;
	DIR       *dir;
	char      *basename = NULL;
	int        i = 0, select_item = 0;

	dname = gnome_program_locate_file (
			NULL, GNOME_FILE_DOMAIN_PIXMAP,
			"mailcheck", FALSE, NULL);

	mc->selected_pixmap_name = mc->mailcheck_text_only;
	omenu = gtk_option_menu_new ();
	menu = gtk_menu_new ();

	item = gtk_menu_item_new_with_label (mc->mailcheck_text_only);
	gtk_widget_show (item);
	mailcheck_new_entry (mc, menu, item, mc->mailcheck_text_only);

	if (mc->animation_file != NULL)
		basename = g_path_get_basename (mc->animation_file);
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
			/* FIXME the string s will be freed in a second so 
			** this should be a strdup */
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
	g_free (basename);

	return omenu;
}

static void
make_check_widgets_sensitive(MailCheck *mc)
{
	gtk_widget_set_sensitive (GTK_WIDGET (mc->min_spin), mc->auto_update);
	gtk_widget_set_sensitive (GTK_WIDGET (mc->sec_spin), mc->auto_update);
}

static void
make_remote_widgets_sensitive(MailCheck *mc)
{
	gboolean b = mc->mailbox_type != MAILBOX_LOCAL &&
	             mc->mailbox_type != MAILBOX_LOCALDIR;
        gboolean f = mc->mailbox_type == MAILBOX_IMAP;
	
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
mail_file_changed (GtkEntry *entry, gpointer data)
{
	MailCheck *mc = data;
	gchar *text;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (!text)
		return;
		
	if (mc->mail_file);
		g_free (mc->mail_file);
		
	mc->mail_file = text;
	panel_applet_gconf_set_string(mc->applet, "mail_file", mc->mail_file, NULL);
}

static void
remote_server_changed (GtkEntry *entry, gpointer data)
{
	MailCheck *mc = data;
	gchar *text;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (!text)
		return;
		
	if (mc->remote_server);
		g_free (mc->remote_server);
		
	mc->remote_server = text;
	panel_applet_gconf_set_string(mc->applet, "remote_server", 
				      mc->remote_server, NULL);
}

static void
remote_username_changed (GtkEntry *entry, gpointer data)
{
	MailCheck *mc = data;
	gchar *text;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (!text)
		return;
		
	if (mc->remote_username);
		g_free (mc->remote_username);
		
	mc->remote_username = text;
	panel_applet_gconf_set_string(mc->applet, "remote_username", 
				      mc->remote_username, NULL);
}

static void
remote_password_changed (GtkEntry *entry, gpointer data)
{
	MailCheck *mc = data;
	gchar *text;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (!text)
		return;
		
	if (mc->remote_password);
		g_free (mc->remote_password);
		
	mc->remote_password = text;
	panel_applet_gconf_set_string(mc->applet, "remote_password", 
				      mc->remote_password, NULL);
}

static void
remote_folder_changed (GtkEntry *entry, gpointer data)
{
	MailCheck *mc = data;
	gchar *text;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (!text)
		return;
		
	if (mc->remote_folder);
		g_free (mc->remote_folder);
		
	mc->remote_folder = text;
	panel_applet_gconf_set_string(mc->applet, "remote_folder", 
				      mc->remote_folder, NULL);
}

static void
pre_remote_command_changed (GtkEntry *entry, gpointer data)
{
	MailCheck *mc = data;
	gchar *text;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (!text)
		return;
		
	if (mc->pre_remote_command);
		g_free (mc->pre_remote_command);
		
	mc->pre_remote_command = text;
	panel_applet_gconf_set_string(mc->applet, "pre_remote_command", 
				      mc->pre_remote_command, NULL);
}	

static void 
set_mailbox_selection (GtkWidget *widget, gpointer data)
{
	MailCheck *mc = g_object_get_data(G_OBJECT(widget), "MailCheck");
	mc->mailbox_type = GPOINTER_TO_INT(data);
        panel_applet_gconf_set_int(mc->applet, "mailbox_type", 
        			  (gint)mc->mailbox_type, NULL);
        make_remote_widgets_sensitive(mc);
        
        if ((mc->mailbox_type != MAILBOX_POP3) &&
	    (mc->mailbox_type != MAILBOX_IMAP) &&
	    (mc->remote_handle != NULL)) {
		helper_whack_handle (mc->remote_handle);
		mc->remote_handle = NULL;
	}
	gtk_label_set_text (GTK_LABEL (mc->label), _("Status not updated"));
	set_tooltip (GTK_WIDGET (mc->applet), _("Status not updated"));
}

static void
pre_check_toggled (GtkToggleButton *button, gpointer data)
{
	MailCheck *mc = data;
	
	mc->pre_check_enabled = gtk_toggle_button_get_active (button);
	panel_applet_gconf_set_bool(mc->applet, "exec_enabled", 
				    mc->pre_check_enabled, NULL);
	gtk_widget_set_sensitive (mc->pre_check_cmd_entry, mc->pre_check_enabled);

}

static void
pre_check_changed (GtkEntry *entry, gpointer data)
{
	MailCheck *mc = data;
	gchar *text;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (!text)
		return;
		
	if (mc->pre_check_cmd)
		g_free (mc->pre_check_cmd);	
	mc->pre_check_cmd = g_strdup (text);
	panel_applet_gconf_set_string(mc->applet, "exec_command", 
				      mc->pre_check_cmd, NULL);
	g_free (text);
	
}

static void
newmail_toggled (GtkToggleButton *button, gpointer data)
{
	MailCheck *mc = data;
	
	mc->newmail_enabled = gtk_toggle_button_get_active (button);
	panel_applet_gconf_set_bool(mc->applet, "newmail_enabled", 
				    mc->newmail_enabled, NULL);
	gtk_widget_set_sensitive (mc->newmail_cmd_entry, mc->newmail_enabled);
				    
}

static void
newmail_changed (GtkEntry *entry, gpointer data)
{
	MailCheck *mc = data;
	gchar *text;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (!text)
		return;
		
	if (mc->newmail_cmd)
		g_free (mc->newmail_cmd);	
	mc->newmail_cmd = g_strdup (text);
	panel_applet_gconf_set_string(mc->applet, "newmail_command", 
				      mc->newmail_cmd, NULL);
	g_free (text);
	
}

static void
clicked_toggled (GtkToggleButton *button, gpointer data)
{
	MailCheck *mc = data;
	
	mc->clicked_enabled = gtk_toggle_button_get_active (button);
	panel_applet_gconf_set_bool(mc->applet, "clicked_enabled", 
				    mc->clicked_enabled, NULL);
	gtk_widget_set_sensitive (mc->clicked_cmd_entry, mc->clicked_enabled);
				    
}

static void
clicked_changed (GtkEntry *entry, gpointer data)
{
	MailCheck *mc = data;
	gchar *text;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (!text)
		return;
		
	if (mc->clicked_cmd)
		g_free (mc->clicked_cmd);	
	mc->clicked_cmd = g_strdup (text);
	panel_applet_gconf_set_string(mc->applet, "clicked_command", mc->clicked_cmd, NULL);
	g_free (text);
	
}

static void
reset_on_clicked_toggled (GtkToggleButton *button, gpointer data)
{
	MailCheck *mc = data;
	
	mc->reset_on_clicked = gtk_toggle_button_get_active (button);
	panel_applet_gconf_set_bool(mc->applet, "reset_on_clicked", 
				    mc->reset_on_clicked, NULL);
				    
}

static void
auto_update_toggled (GtkToggleButton *button, gpointer data)
{
	MailCheck *mc = data;
	
	mc->auto_update = gtk_toggle_button_get_active (button);

	if(mc->mail_timeout != 0) {
		gtk_timeout_remove(mc->mail_timeout);
		mc->mail_timeout = 0;
	}
	if(mc->auto_update)
		mc->mail_timeout = gtk_timeout_add(mc->update_freq, mail_check_timeout, mc);

	make_check_widgets_sensitive(mc);
	panel_applet_gconf_set_bool(mc->applet, "auto_update", mc->auto_update, NULL);

	/*
	 * check the mail right now, so we don't have to wait
	 * for the first timeout
	 */
	mail_check_timeout (mc);
}

static void
update_spin_changed (GtkSpinButton *spin, gpointer data)
{
	MailCheck *mc = data;
	
	mc->update_freq = 1000 * (guint)(gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (mc->sec_spin)) + 60 * gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (mc->min_spin)));
	
	if (mc->update_freq == 0) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON (mc->sec_spin), 0.0);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON (mc->min_spin), 1.0);
		mc->update_freq = 60*1000;
	}
	if(mc->mail_timeout != 0)
		gtk_timeout_remove (mc->mail_timeout);
	mc->mail_timeout = gtk_timeout_add (mc->update_freq, mail_check_timeout, mc);
	panel_applet_gconf_set_int(mc->applet, "update_frequency", mc->update_freq, NULL);
}

static void
sound_toggled (GtkToggleButton *button, gpointer data)
{
	MailCheck *mc = data;
	
	mc->play_sound = gtk_toggle_button_get_active (button);
	panel_applet_gconf_set_bool(mc->applet, "play_sound", mc->play_sound, NULL);
}

static GtkWidget *
mailbox_properties_page(MailCheck *mc)
{
	GtkWidget *vbox, *hbox, *l, *l2, *item, *label, *entry;

	mc->type = 1;

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
	gtk_widget_show (vbox);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);        

	label = gtk_label_new_with_mnemonic(_("Mailbox _resides on:"));
	gtk_widget_show(label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	mc->remote_option_menu = l = gtk_option_menu_new();
	set_atk_relation (mc->remote_option_menu, label, ATK_RELATION_LABELLED_BY);
        
	l2 = gtk_menu_new();
	item = gtk_menu_item_new_with_label(_("Local mailspool")); 
	gtk_widget_show(item);
	g_object_set_data(G_OBJECT(item), "MailCheck", mc);
	g_signal_connect (G_OBJECT(item), "activate", 
			    G_CALLBACK(set_mailbox_selection), 
			    GINT_TO_POINTER(MAILBOX_LOCAL));
	gtk_menu_shell_append (GTK_MENU_SHELL (l2), item);

	item = gtk_menu_item_new_with_label(_("Local maildir")); 
	gtk_widget_show(item);
	g_object_set_data(G_OBJECT(item), "MailCheck", mc);
	g_signal_connect (G_OBJECT(item), "activate", 
			    G_CALLBACK(set_mailbox_selection), 
			    GINT_TO_POINTER(MAILBOX_LOCALDIR));
	gtk_menu_shell_append (GTK_MENU_SHELL (l2), item);

	item = gtk_menu_item_new_with_label(_("Remote POP3-server")); 
	gtk_widget_show(item);
	g_object_set_data(G_OBJECT(item), "MailCheck", mc);
	g_signal_connect (G_OBJECT(item), "activate", 
			    G_CALLBACK(set_mailbox_selection), 
			    GINT_TO_POINTER(MAILBOX_POP3));
        
	gtk_menu_shell_append (GTK_MENU_SHELL (l2), item);
	item = gtk_menu_item_new_with_label(_("Remote IMAP-server")); 
	gtk_widget_show(item);
	g_object_set_data(G_OBJECT(item), "MailCheck", mc);
	g_signal_connect (G_OBJECT(item), "activate", 
			    G_CALLBACK(set_mailbox_selection), 
			    GINT_TO_POINTER(MAILBOX_IMAP));
	gtk_menu_shell_append (GTK_MENU_SHELL (l2), item);
	
	gtk_widget_show(l2);
  
	gtk_option_menu_set_menu(GTK_OPTION_MENU(l), l2);
	gtk_option_menu_set_history(GTK_OPTION_MENU(l), mc->mailbox_type_temp = mc->mailbox_type);
	gtk_widget_show(l);
  
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
  
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	mc->mailfile_label = l = gtk_label_new_with_mnemonic(_("Mail _spool file:"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);

	mc->mailfile_fentry = l = gnome_file_entry_new ("spool_file", _("Browse"));
	entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (mc->mailfile_fentry));
	set_atk_relation (entry, mc->mailfile_label, ATK_RELATION_LABELLED_BY);
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, TRUE, TRUE, 0);

	mc->mailfile_entry = l = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY (l));
	gtk_entry_set_text(GTK_ENTRY(l), mc->mail_file);
	g_signal_connect(G_OBJECT(l), "changed",
			   G_CALLBACK(mail_file_changed), mc);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);  
  
	mc->remote_server_label = l = gtk_label_new_with_mnemonic(_("Mail s_erver:"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
  
	mc->remote_server_entry = l = gtk_entry_new();

	set_atk_name_description (mc->remote_server_entry, _("Mail Server Entry box"), "");
	set_atk_relation (mc->remote_server_entry, mc->remote_server_label, ATK_RELATION_LABELLED_BY);
	if (mc->remote_server)
		gtk_entry_set_text(GTK_ENTRY(l), mc->remote_server);
  	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, TRUE, TRUE, 0);      
	
	g_signal_connect(G_OBJECT(l), "changed",
			   G_CALLBACK(remote_server_changed), mc);
	
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);  
  
	mc->remote_username_label = l = gtk_label_new_with_mnemonic(_("_Username:"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	
	mc->remote_username_entry = l = gtk_entry_new();
	if (mc->remote_username)
		gtk_entry_set_text(GTK_ENTRY(l), mc->remote_username);

	set_atk_name_description (mc->remote_username_entry, _("Username Entry box"), "");
	set_atk_relation (mc->remote_username_entry, mc->remote_username_label, ATK_RELATION_LABELLED_BY);
  
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);      
  
	g_signal_connect(G_OBJECT(l), "changed",
			   G_CALLBACK(remote_username_changed), mc);

	mc->remote_password_label = l = gtk_label_new_with_mnemonic(_("_Password:"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	
	mc->remote_password_entry = l = gtk_entry_new();
	if (mc->remote_password)
		gtk_entry_set_text(GTK_ENTRY(l), mc->remote_password);

	set_atk_name_description (mc->remote_password_entry, _("Password Entry box"), "");
	set_atk_relation (mc->remote_password_entry, mc->remote_password_label, ATK_RELATION_LABELLED_BY);
	gtk_entry_set_visibility(GTK_ENTRY (l), FALSE);
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);      
	
	g_signal_connect(G_OBJECT(l), "changed",
                     G_CALLBACK(remote_password_changed), mc);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);  

        mc->remote_folder_label = l = gtk_label_new_with_mnemonic(_("_Folder:"));
        gtk_widget_show(l);
        gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
 
        mc->remote_folder_entry = l = gtk_entry_new();
        if (mc->remote_folder)
                gtk_entry_set_text(GTK_ENTRY(l), mc->remote_folder);
  
	    set_atk_name_description (mc->remote_folder_entry, _("Folder Entry box"), "");
        set_atk_relation (mc->remote_folder_entry, mc->remote_folder_label, ATK_RELATION_LABELLED_BY);
        gtk_widget_show(l);
        gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
  
        g_signal_connect(G_OBJECT(l), "changed",
                           G_CALLBACK(remote_folder_changed), mc);
 
        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
        gtk_widget_show (hbox);  

	mc->pre_remote_command_label = l = gtk_label_new_with_mnemonic(_("C_ommand to run before checking for mail:"));
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
  
	mc->pre_remote_command_entry = l = gtk_entry_new();
	if (mc->pre_remote_command)
		gtk_entry_set_text(GTK_ENTRY(l), mc->pre_remote_command);

	set_atk_name_description (mc->pre_remote_command_entry, _("Command Entry box"), "");
	set_atk_relation (mc->pre_remote_command_entry, mc->pre_remote_command_label, ATK_RELATION_LABELLED_BY);
  	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, TRUE, TRUE, 0);      
	
	g_signal_connect(G_OBJECT(l), "changed",
			   G_CALLBACK(pre_remote_command_changed), mc);
  
	make_remote_widgets_sensitive(mc);
	
	return vbox;
}

static GtkWidget *
mailcheck_properties_page (MailCheck *mc)
{
	GtkWidget *vbox, *hbox, *l, *table, *frame, *check_box, *animation_option_menu;
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

	l = gtk_check_button_new_with_mnemonic(_("Before each _update:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l), mc->pre_check_enabled);
	g_signal_connect(G_OBJECT(l), "toggled",
			   G_CALLBACK(pre_check_toggled), mc);
	gtk_widget_show(l);
	mc->pre_check_cmd_check = l;
	
	gtk_table_attach (GTK_TABLE (table), mc->pre_check_cmd_check, 
			  0, 1, 0, 1, GTK_FILL, 0, 0, 0);
				   
	
	mc->pre_check_cmd_entry = gtk_entry_new();
	if(mc->pre_check_cmd)
		gtk_entry_set_text(GTK_ENTRY(mc->pre_check_cmd_entry), 
				   mc->pre_check_cmd);
	set_atk_name_description (mc->pre_check_cmd_entry, _("Command to execute before each update"), "");
	set_atk_relation (mc->pre_check_cmd_entry, mc->pre_check_cmd_check, ATK_RELATION_CONTROLLED_BY);
	set_atk_relation (mc->pre_check_cmd_check, mc->pre_check_cmd_entry, ATK_RELATION_CONTROLLER_FOR);
	gtk_widget_set_sensitive (mc->pre_check_cmd_entry, mc->pre_check_enabled);
	g_signal_connect(G_OBJECT(mc->pre_check_cmd_entry), "changed",
			   G_CALLBACK(pre_check_changed), mc);
	gtk_widget_show(mc->pre_check_cmd_entry);
	gtk_table_attach_defaults (GTK_TABLE (table), mc->pre_check_cmd_entry,
				   1, 2, 0, 1);

	l = gtk_check_button_new_with_mnemonic (_("When new mail _arrives:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l), mc->newmail_enabled);
	g_signal_connect(G_OBJECT(l), "toggled",
			   G_CALLBACK(newmail_toggled), mc);
	gtk_widget_show(l);
	gtk_table_attach (GTK_TABLE (table), l, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	mc->newmail_cmd_check = l;

	mc->newmail_cmd_entry = gtk_entry_new();
	if (mc->newmail_cmd) {
		gtk_entry_set_text(GTK_ENTRY(mc->newmail_cmd_entry),
				   mc->newmail_cmd);
	}
	set_atk_name_description (mc->newmail_cmd_entry, _("Command to execute when new mail arrives"), "");
	set_atk_relation (mc->newmail_cmd_entry, mc->newmail_cmd_check, ATK_RELATION_CONTROLLED_BY);
	set_atk_relation (mc->newmail_cmd_check, mc->newmail_cmd_entry, ATK_RELATION_CONTROLLER_FOR);
	gtk_widget_set_sensitive (mc->newmail_cmd_entry, mc->newmail_enabled);
	g_signal_connect(G_OBJECT (mc->newmail_cmd_entry), "changed",
			   G_CALLBACK(newmail_changed), mc);
	gtk_widget_show(mc->newmail_cmd_entry);
	gtk_table_attach_defaults (GTK_TABLE (table), mc->newmail_cmd_entry,
				    1, 2, 1, 2);

        l = gtk_check_button_new_with_mnemonic (_("When clicke_d:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l), mc->clicked_enabled);
	g_signal_connect(G_OBJECT(l), "toggled",
			   G_CALLBACK(clicked_toggled), mc);
        gtk_widget_show(l);
	gtk_table_attach (GTK_TABLE (table), l, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	mc->clicked_cmd_check = l;

        mc->clicked_cmd_entry = gtk_entry_new();
        if(mc->clicked_cmd) {
		gtk_entry_set_text(GTK_ENTRY(mc->clicked_cmd_entry), 
				   mc->clicked_cmd);
        }
		set_atk_name_description (mc->clicked_cmd_entry, _("Command to execute when clicked"), "");
		set_atk_relation (mc->clicked_cmd_entry, mc->clicked_cmd_check, ATK_RELATION_CONTROLLED_BY);
		set_atk_relation (mc->clicked_cmd_check, mc->clicked_cmd_entry, ATK_RELATION_CONTROLLER_FOR);
		gtk_widget_set_sensitive (mc->clicked_cmd_entry, mc->clicked_enabled);
        g_signal_connect(G_OBJECT(mc->clicked_cmd_entry), "changed",
                           G_CALLBACK(clicked_changed), mc);
        gtk_widget_show(mc->clicked_cmd_entry);
	gtk_table_attach_defaults (GTK_TABLE (table), mc->clicked_cmd_entry,
				   1, 2, 2, 3);

	l = gtk_check_button_new_with_mnemonic (_("Set the number of unread mails to _zero"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l), mc->reset_on_clicked);
	g_signal_connect(G_OBJECT(l), "toggled",
			   G_CALLBACK(reset_on_clicked_toggled), mc);
	gtk_widget_show(l);
	gtk_table_attach (GTK_TABLE (table), l, 1, 2, 3, 4, GTK_FILL, 0, 0, 0);

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
        gtk_widget_show (hbox); 
        
	check_box = l = gtk_check_button_new_with_mnemonic (_("Check for mail _every"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l), mc->auto_update);
	g_signal_connect(G_OBJECT(l), "toggled",
			 G_CALLBACK(auto_update_toggled), mc);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	gtk_widget_show(l);

	freq_a = gtk_adjustment_new((float)((mc->update_freq/1000)/60), 0, 1440, 1, 5, 5);
	mc->min_spin = gtk_spin_button_new( GTK_ADJUSTMENT (freq_a), 1, 0);
	g_signal_connect (G_OBJECT (mc->min_spin), "value_changed",
			  G_CALLBACK (update_spin_changed), mc);			  
	gtk_box_pack_start (GTK_BOX (hbox), mc->min_spin,  FALSE, FALSE, 0);
	set_atk_name_description (mc->min_spin, _("minutes"), _("Choose time interval in minutes to check mail"));
	set_atk_relation (mc->min_spin, check_box, ATK_RELATION_CONTROLLED_BY);
	gtk_widget_show(mc->min_spin);
	
	l = gtk_label_new (_("minutes"));
	set_atk_relation (mc->min_spin, l, ATK_RELATION_LABELLED_BY);
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	
	freq_a = gtk_adjustment_new((float)((mc->update_freq/1000)%60), 0, 59, 1, 5, 5);
	mc->sec_spin = gtk_spin_button_new (GTK_ADJUSTMENT (freq_a), 1, 0);
	g_signal_connect (G_OBJECT (mc->sec_spin), "value_changed",
			  G_CALLBACK (update_spin_changed), mc);
	gtk_box_pack_start (GTK_BOX (hbox), mc->sec_spin,  FALSE, FALSE, 0);
	set_atk_name_description (mc->sec_spin, _("seconds"), _("Choose time interval in seconds to check mail"));
	set_atk_relation (mc->sec_spin, check_box, ATK_RELATION_CONTROLLED_BY);
	gtk_widget_show(mc->sec_spin);
	
	l = gtk_label_new (_("seconds"));
	set_atk_relation (mc->sec_spin, l,  ATK_RELATION_LABELLED_BY);
	gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	
	set_atk_relation (check_box, mc->min_spin, ATK_RELATION_CONTROLLER_FOR);
	set_atk_relation (check_box, mc->sec_spin, ATK_RELATION_CONTROLLER_FOR);

	mc->play_sound_check = gtk_check_button_new_with_mnemonic(_("Play a _sound when new mail arrives"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mc->play_sound_check), mc->play_sound);
	g_signal_connect(G_OBJECT(mc->play_sound_check), "toggled",
			   G_CALLBACK(sound_toggled), mc);
	gtk_widget_show(mc->play_sound_check);
	gtk_box_pack_start(GTK_BOX (vbox), mc->play_sound_check, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);  

	l = gtk_label_new_with_mnemonic (_("Select a_nimation"));
	gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
	gtk_widget_show (l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	animation_option_menu = mailcheck_get_animation_menu (mc);
	gtk_box_pack_start (GTK_BOX (hbox), animation_option_menu, FALSE, FALSE, 0);
	set_atk_relation (animation_option_menu, l, ATK_RELATION_LABELLED_BY);
	make_check_widgets_sensitive(mc);

	return vbox;
}

static void
phelp_cb (GtkDialog *w, gint tab, MailCheck *mc)
{
	GError *error = NULL;
	static GnomeProgram *applet_program = NULL;

	if (!applet_program) {
		int argc = 1;
		char *argv[2] = { "mailcheck" };
		applet_program = gnome_program_init ("mailcheck", VERSION,
						      LIBGNOME_MODULE, argc, argv,
     						      GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);
	}

	egg_help_display_desktop_on_screen (
			applet_program, "mailcheck", "mailcheck", "mailcheck-prefs",
			gtk_widget_get_screen (GTK_WIDGET (mc->applet)),
			&error);
	if (error) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new (GTK_WINDOW (w),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						  _("There was an error displaying help: %s"),
						 error->message);

		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_widget_get_screen (GTK_WIDGET (mc->applet)));
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}	

static void
response_cb (GtkDialog *dialog, gint id, MailCheck *mc)
{
	if (id == GTK_RESPONSE_HELP) {
		phelp_cb (dialog, id, mc);
		return;	
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
	mc->property_window = NULL;
}


static void
mailcheck_properties (BonoboUIComponent *uic, MailCheck *mc, const gchar *verbname)
{
	GtkWidget *p;
	GtkWidget *notebook;

	if (mc->property_window) {
		gtk_window_set_screen (GTK_WINDOW (mc->property_window),
				       gtk_widget_get_screen (GTK_WIDGET (mc->applet)));
		gtk_window_present (GTK_WINDOW (mc->property_window));
		return;
	}
	
	mc->property_window = gtk_dialog_new_with_buttons (_("Inbox Monitor Preferences"), 
							   NULL,
						           GTK_DIALOG_DESTROY_WITH_PARENT,
						           GTK_STOCK_CLOSE, 
						           GTK_RESPONSE_CLOSE,
						           GTK_STOCK_HELP, 
						           GTK_RESPONSE_HELP,
						           NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (mc->property_window), GTK_RESPONSE_CLOSE);
	gnome_window_icon_set_from_file (GTK_WINDOW (mc->property_window),
					 GNOME_ICONDIR"/gnome-mailcheck.png");
	gtk_window_set_screen (GTK_WINDOW (mc->property_window),
			       gtk_widget_get_screen (GTK_WIDGET (mc->applet)));
	
	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (mc->property_window)->vbox), notebook,
			    TRUE, TRUE, 0);
	p = mailcheck_properties_page (mc);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), p,
				  gtk_label_new_with_mnemonic (_("_Mail check")));
				  
	p = mailbox_properties_page (mc);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), p,
				  gtk_label_new_with_mnemonic (_("Mail_box")));
	
	g_signal_connect (G_OBJECT (mc->property_window), "response",
			  G_CALLBACK (response_cb), mc);
	gtk_widget_show (GTK_DIALOG (mc->property_window)->vbox);
	gtk_widget_show (mc->property_window);
}

static void
check_callback (BonoboUIComponent *uic, gpointer data, const gchar *verbname)
{
	MailCheck *mc = data;

	mail_check_timeout(mc);
}

static void
applet_load_prefs(MailCheck *mc)
{
	mc->animation_file = panel_applet_gconf_get_string(mc->applet, "animation_file", NULL);
	if(!mc->animation_file) {
		g_free(mc->animation_file);
		mc->animation_file = NULL;
	}

	mc->auto_update = panel_applet_gconf_get_bool(mc->applet, "auto_update", NULL);
	mc->reset_on_clicked = panel_applet_gconf_get_bool (mc->applet, "reset_on_clicked", NULL);
	mc->update_freq = panel_applet_gconf_get_int(mc->applet, "update_frequency", NULL);
	mc->pre_check_cmd = panel_applet_gconf_get_string(mc->applet, "exec_command", NULL);
	mc->pre_check_enabled = panel_applet_gconf_get_bool(mc->applet, "exec_enabled", NULL);
	mc->newmail_cmd = panel_applet_gconf_get_string(mc->applet, "newmail_command", NULL);
	mc->newmail_enabled = panel_applet_gconf_get_bool(mc->applet, "newmail_enabled", NULL);
	mc->clicked_cmd = panel_applet_gconf_get_string(mc->applet, "clicked_command", NULL);
	mc->clicked_enabled = panel_applet_gconf_get_bool(mc->applet, "clicked_enabled", NULL);
	mc->remote_server = panel_applet_gconf_get_string(mc->applet, "remote_server", NULL);
	mc->pre_remote_command = panel_applet_gconf_get_string(mc->applet, "pre_remote_command", NULL);
	mc->remote_username = panel_applet_gconf_get_string(mc->applet, "remote_username", NULL);
	if(!mc->remote_username) {
		g_free(mc->remote_username);
		mc->remote_username = g_strdup(g_getenv("USER"));
	}
	mc->remote_password = panel_applet_gconf_get_string(mc->applet, "remote_password", NULL);
	mc->remote_folder = panel_applet_gconf_get_string(mc->applet, "remote_folder", NULL);
	mc->mailbox_type = panel_applet_gconf_get_int(mc->applet, "mailbox_type", NULL);
	mc->mail_file = panel_applet_gconf_get_string (mc->applet, "mail_file", NULL);
	mc->play_sound = panel_applet_gconf_get_bool(mc->applet, "play_sound", NULL);
}

static void
mailcheck_about(BonoboUIComponent *uic, MailCheck *mc, const gchar *verbname)
{
	GdkPixbuf *pixbuf = NULL;
	gchar *file;

	static const gchar     *authors [] =
	{
		"Miguel de Icaza <miguel@kernel.org>",
		"Jacob Berkman <jberkman@andrew.cmu.edu>",
		"Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>",
		"Lennart Poettering <poettering@gmx.net>",
		NULL
	};
	const char *documenters [] = {
	  NULL
	};
	const char *translator_credits = _("translator_credits");

	if (mc->about) {
		gtk_window_set_screen (GTK_WINDOW (mc->about),
				       gtk_widget_get_screen (GTK_WIDGET (mc->applet)));
		gtk_window_present (GTK_WINDOW (mc->about));
		return;
	}
	
	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, "gnome-mailcheck.png", TRUE, NULL);
	pixbuf = gdk_pixbuf_new_from_file (file, NULL);
	g_free (file);
	
	mc->about = gnome_about_new (_("Inbox Monitor"), "1.1",
				     "Copyright \xc2\xa9 1998-2002 Free Software Foundation, Inc.",
				     _("Inbox Monitor notifies you when new mail arrives in your mailbox"),
				     authors,
				     documenters,
   				     strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
				     pixbuf);
				     
	gtk_window_set_wmclass (GTK_WINDOW (mc->about), "mailcheck", "Mailcheck");
	gtk_window_set_screen (GTK_WINDOW (mc->about),
			       gtk_widget_get_screen (GTK_WIDGET (mc->applet)));

	gnome_window_icon_set_from_file (GTK_WINDOW (mc->about),
					 GNOME_ICONDIR"/gnome-mailcheck.png");

	g_signal_connect( G_OBJECT(mc->about), "destroy",
			    G_CALLBACK(gtk_widget_destroyed), &mc->about );
	gtk_widget_show(mc->about);
}

/*this is when the panel size changes */
static void
applet_change_pixel_size(PanelApplet * w, gint size, gpointer data)
{
	MailCheck *mc = data;
	const char *fname;

	if(mc->report_mail_mode == REPORT_MAIL_USE_TEXT)
		return;

	mc->size = size;
	fname = mail_animation_filename (mc);

	gtk_widget_set_size_request (GTK_WIDGET(mc->da), size, size);
	
	if (!fname)
		return;

	mailcheck_load_animation (mc, fname);
}

static void
help_callback (BonoboUIComponent *uic, MailCheck *mc, const gchar *verbname)
{
	GError *error = NULL;
	static GnomeProgram *applet_program = NULL;

	if (!applet_program) {
		int argc = 1;
		char *argv[2] = { "mailcheck" };
		applet_program = gnome_program_init ("mailcheck", VERSION,
						      LIBGNOME_MODULE, argc, argv,
						      GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);
	}

	egg_help_display_desktop_on_screen (
		applet_program, "mailcheck", "mailcheck",NULL,
		gtk_widget_get_screen (GTK_WIDGET (mc->applet)),
		&error);
	if (error) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new (NULL,
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						  _("There was an error displaying help: %s"),
						 error->message);

		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_widget_get_screen (GTK_WIDGET (mc->applet)));
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

static void
set_atk_name_description (GtkWidget *widget, const gchar *name,
    const gchar *description)
{	
	AtkObject *aobj;
	
	aobj = gtk_widget_get_accessible (widget);
	/* Check if gail is loaded */
	if (GTK_IS_ACCESSIBLE (aobj) == FALSE)
		return; 
	atk_object_set_name (aobj, name);
	atk_object_set_description (aobj, description);
}

static void
set_atk_relation (GtkWidget *widget1, GtkWidget *widget2, AtkRelationType relation_type)
{
	AtkObject *atk_widget1;
	AtkObject *atk_widget2;
	AtkRelationSet *relation_set;
	AtkRelation *relation;
	AtkObject *targets[1];

	atk_widget1 = gtk_widget_get_accessible (widget1);
	atk_widget2 = gtk_widget_get_accessible (widget2);

	/* Set the label-for relation only if label-by is being set */
	if (relation_type == ATK_RELATION_LABELLED_BY) 
		gtk_label_set_mnemonic_widget (GTK_LABEL (widget2), widget1); 

	/* Check if gail is loaded */
	if (GTK_IS_ACCESSIBLE (atk_widget1) == FALSE)
		return;

	/* Set the labelled-by relation */
	relation_set = atk_object_ref_relation_set (atk_widget1);
	targets[0] = atk_widget2;
	relation = atk_relation_new (targets, 1, relation_type);
	atk_relation_set_add (relation_set, relation);
	g_object_unref (G_OBJECT (relation));
}

static const BonoboUIVerb mailcheck_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("Preferences", mailcheck_properties),
	BONOBO_UI_UNSAFE_VERB ("Help",        help_callback),
	BONOBO_UI_UNSAFE_VERB ("About",       mailcheck_about),
	BONOBO_UI_UNSAFE_VERB ("Check",       check_callback),
        BONOBO_UI_VERB_END
};

gboolean
fill_mailcheck_applet(PanelApplet *applet)
{
	GtkWidget *mailcheck;
	MailCheck *mc;

	mc = g_new0(MailCheck, 1);
	mc ->applet = applet;
	mc->animation_file = NULL;
	mc->property_window = NULL;
	mc->anim_changed = FALSE;
	mc->anymail = mc->unreadmail = mc->newmail = FALSE;
	mc->mail_timeout = 0;
	mc->animation_tag = 0;
	mc->password_dialog = NULL;

	/*initial state*/
	mc->report_mail_mode = REPORT_MAIL_USE_ANIMATION;
	
	mc->mail_file = NULL;

	if (mc->mail_file == NULL) {
		const char *mail_file = g_getenv ("MAIL");
		if (mail_file == NULL) {
			const char *user = g_getenv ("USER");
			if (user == NULL)
				return FALSE;

			mc->mail_file = g_strdup_printf ("/var/spool/mail/%s",
							 user);
		} else
			mc->mail_file = g_strdup (mail_file);
	}

	panel_applet_add_preferences (applet, "/schemas/apps/mailcheck_applet/prefs", NULL);
	applet_load_prefs(mc);

	mc->mailcheck_text_only = _("Text only");

	mc->size = panel_applet_get_size (applet);

	g_signal_connect(G_OBJECT(applet), "change_size",
			 G_CALLBACK(applet_change_pixel_size),
			 mc);

	mailcheck = create_mail_widgets (mc);
	gtk_widget_show(mailcheck);

	gtk_container_add (GTK_CONTAINER (applet), mailcheck);

	g_signal_connect(G_OBJECT(mc->ebox), "button_press_event",
			 G_CALLBACK(exec_clicked_cmd), mc);


	panel_applet_setup_menu_from_file (applet,
					   NULL,
					   "GNOME_MailCheckApplet.xml",
					   NULL, 
			        	   mailcheck_menu_verbs,
					   mc);
	
	gtk_label_set_text (GTK_LABEL (mc->label), _("Status not updated"));
	set_tooltip (GTK_WIDGET (mc->applet), _("Status not updated"));
	set_atk_name_description (GTK_WIDGET (mc->applet), _("Mail check"), 
			_("Mail check notifies you when new mail arrives in your mailbox"));
	gtk_widget_show_all (GTK_WIDGET (applet));

	/*
	 * check the mail if the applet is  realized. Checking the mail 
	 * right now (in case the applet is not realized), will give us 
	 * wrong screen value. 
	 */

	if (GTK_WIDGET_REALIZED (GTK_WIDGET (applet)))
		mail_check_timeout (mc);
	else
		mc->applet_realized_signal =
			g_signal_connect (G_OBJECT(applet), "realize",
					  G_CALLBACK(applet_realized_cb), mc);

	return(TRUE);
}
