/* GNOME remote helper, forks to run imap or pop checks
 * (C) 2001 Eazel, Inc.
 *
 * Author: George Lebl
 *
 */

#ifndef REMOTE_HELPER_H
#define REMOTE_HELPER_H

typedef void (* RemoteHandler) (int mails, gpointer data);

gpointer helper_pop3_check (RemoteHandler handler, gpointer data,
			    GDestroyNotify destroy_notify,
			    const char *command,
			    const char *h, const char* n, const char* e);
gpointer helper_imap_check (RemoteHandler handler, gpointer data,
			    GDestroyNotify destroy_notify,
			    const char *command,
			    const char *h, const char* n, const char* e,
			    const char *f);
void helper_whack_handle (gpointer handle);

#endif
