/* GNOME pop/imap-mail-check-library.
 * (C) 1997, 1998 The Free Software Foundation
 *
 * Author: Lennart Poettering
 *
 */

#ifndef _POPCHECK_H_
#define _POPCHECK_H_

/* Returns how many mails are available on POP3-server "h" with username "n" and password "e"
 * The server-name may be given with or without port-number in form "host:port". 
 */
int pop3_check(char *h, char* n, char* e);

/* Returns how many mails are available on IMAP-server "h" with username "n" and password "e"
 * Hi: unseen/recent; Lo: total
 * The server-name may be given with or without port-number in form "host:port".
 */
int imap_check(char *h, char* n, char* e);

#endif

