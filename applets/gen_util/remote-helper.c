/* GNOME remote helper, forks to run imap or pop checks
 * (C) 2001 Eazel, Inc.
 *
 * Author: George Lebl
 *
 * Utterly ugly, make this use corba at some point in the future.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgnome/libgnome.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>

#include "popcheck.h"

#include "remote-helper.h"

#define POLLTIMEOUT 5000 /* 5 milliseconds */

typedef struct {
	pid_t pid;
	int fd;
	guint timeout;
	RemoteHandler handler;
	RemoteHandler error_handler;
       	gpointer data;
	GDestroyNotify destroy_notify;
} RemoteHandlerData;

static gboolean
try_reading (gpointer data)
{
	int retval;
	int mails;
	RemoteHandlerData *handler = data;
	sigset_t mask, omask;

	sigemptyset (&mask);
	sigaddset (&mask, SIGPIPE);
	sigprocmask (SIG_BLOCK, &mask, &omask); 

	retval = read (handler->fd, &mails, sizeof (mails));

	sigprocmask (SIG_SETMASK, &omask, NULL);

	if (retval == 0 ||
	    (retval < 0 && errno == EAGAIN)) {
		if (kill (handler->pid, 0) != 0) {
			handler->timeout = 0;
			helper_whack_handle (handler);
			return FALSE;
		}
		return TRUE;
	} else if (retval < 0) {
		handler->timeout = 0;
		helper_whack_handle (handler);
		return FALSE;
	}

	if (mails < 0)
		handler->error_handler (mails, handler->data);
	else
		handler->handler (mails, handler->data);

	handler->timeout = 0;
	helper_whack_handle (handler);
	return FALSE;
}

void
helper_whack_handle (gpointer handle)
{
	RemoteHandlerData *handler = handle;

	if (handler->fd >= 0)
		close (handler->fd);
	handler->fd = -1;

	if (handler->pid > 0) {
		kill (handler->pid, SIGTERM);
	}
	handler->pid = 0;

	if (handler->timeout > 0)
		gtk_timeout_remove (handler->timeout);
	handler->timeout = 0;
	
	handler->handler = NULL;
	if (handler->destroy_notify != NULL)
		handler->destroy_notify (handler->data);
	handler->data = NULL;
	handler->destroy_notify = NULL;

	g_free (handler);
}

static RemoteHandlerData *
fork_new_handler (RemoteHandler handler, RemoteHandler error_handler,
		  gpointer data, GDestroyNotify destroy_notify)
{
	pid_t pid;
	int fd[2];
	RemoteHandlerData *handler_data;

	if (pipe (fd) != 0)
		return NULL;

	handler_data = g_new0 (RemoteHandlerData, 1);

	pid = fork ();
	if (pid < 0) {
		close (fd[0]);
		close (fd[1]);
		g_free (handler_data);
		return NULL;
	} else if (pid == 0) {
		/*child*/
		pid = fork ();
		if (pid != 0) {
			write (fd[1], &pid, sizeof (pid));
			_exit (0);
		} else {
			/* grand child */

			/* Make sure that the pid is written first */

			struct pollfd poll_list[1];

			poll_list[0].fd = fd[0];
			poll_list[0].events = POLLIN;
			poll (poll_list, 1, POLLTIMEOUT);

			close (fd [0]);

			if (((poll_list[0].revents&POLLHUP) == POLLHUP) ||
			    ((poll_list[0].revents&POLLERR) == POLLERR) ||
			    ((poll_list[0].revents&POLLERR) == POLLNVAL)) {
				g_free (handler_data);
				return NULL;
			}

			handler_data->pid = 0;
			handler_data->fd = fd[1];
			return handler_data;
		}
	} else {
		/*parent*/
		close (fd[1]);
		while ((waitpid (pid, 0, 0) == -1) && errno == EINTR);
		read (fd[0], &pid, sizeof (pid));
		
		if (pid <= 0) {
			close (fd[0]);
			g_free (handler_data);
			return NULL;
		}

		/* set to nonblocking */
		fcntl(fd[0], F_SETFL, O_NONBLOCK);

		handler_data->pid = pid;
		handler_data->fd = fd[0];
		handler_data->handler = handler;
		handler_data->error_handler = error_handler;
		handler_data->data = data;
		handler_data->destroy_notify = destroy_notify;
		handler_data->timeout = gtk_timeout_add (500, try_reading,
							 handler_data);

		return handler_data;
	}
}


gpointer
helper_pop3_check (RemoteHandler handler, RemoteHandler error_handler,
		   gpointer data,
		   GDestroyNotify destroy_notify,
		   const char *command,
		   const char *h, const char* n, const char* e)
{
	RemoteHandlerData *handler_data;

	handler_data = fork_new_handler (handler, error_handler, data,
					 destroy_notify);

	if (handler_data == NULL) {
		int mails;

		mails = pop3_check (h, n, e);

		if (mails < 0)
			error_handler (mails, data);
		else
			handler (mails, data);

		if (destroy_notify != NULL)
			destroy_notify (data);
		return NULL;
	}

	if (handler_data->pid == 0) {
		int mails;

		if (command != NULL &&
		    command[0] != '\0')
			system (command);
	       
		mails = pop3_check (h, n, e);

		write (handler_data->fd, &mails, sizeof (mails));

		_exit (0);
	}

	return handler_data;
}

gpointer
helper_imap_check (RemoteHandler handler, RemoteHandler error_handler,
		   gpointer data,
		   GDestroyNotify destroy_notify,
		   const char *command,
		   const char *h, const char* n, const char* e, const char *f)
{
	RemoteHandlerData *handler_data;

	handler_data = fork_new_handler (handler, error_handler, data,
					 destroy_notify);

	if (handler_data == NULL) {
		int mails;

		mails = imap_check (h, n, e, f);

		if (mails < 0)
			error_handler (mails, data);
		else
			handler (mails, data);

		return NULL;
	}

	if (handler_data->pid == 0) {
		int mails;

		if (command != NULL &&
		    command[0] != '\0')
			system (command);
	       
		mails = imap_check (h, n, e, f);

		write (handler_data->fd, &mails, sizeof (mails));

		_exit (0);
	}

	return handler_data;
}
