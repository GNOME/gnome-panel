/* GNOME pop/imap-mail-check-library.
 * (C) 1997, 1998 The Free Software Foundation
 *
 * Author: Lennart Poettering
 *
 */

#include "config.h"

#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <glib.h>

#include "popcheck.h"

#define TIMEOUT 5
static int get_server_port(const char *);
static char* get_server_hostname(const char *);
static int connect_socket(const char *, int);
static char *read_line(int);
static int write_line(int, char *);
static int is_pop3_answer_ok(const char *);
static int is_imap_answer_untagged(const char *);
static int is_imap_answer_ok(const char *);
static char *wait_for_imap_answer(int, char *);

static int get_server_port(const char *h)
 {
  char *x;
  x = strchr(h, ':');
  if (x)
   {
    return atoi(x+1);
   }
  else
   return 0;
 }
 
static char* get_server_hostname(const char *h)
 {
  char *e;
  if (!h) return 0;
  
  e = strchr(h, ':');
  if (e)
   {
    char *x;
    int l = e-h;
    x = g_malloc(l+5);
    strncpy(x, h, l);
    x[l] = 0;
    return x;
   }
  else
   return strcpy((char*) g_malloc(strlen(h)+1), h); 
 }

static int connect_socket(const char *h, int def)
 {
  struct hostent *he;
  struct sockaddr_in peer;
  int fd, p;
  char *hn;
  
  hn = get_server_hostname(h);
  if (!hn)
   return -1;
  
  p = get_server_port(h); 
  if (p == 0) p = def;

  he = gethostbyname(hn);
  g_free(hn);
  
  if (!he) 
   return -1;

  fd = socket(PF_INET, SOCK_STREAM, 0);
  if (fd < 0) 
   return -1;

  peer.sin_family = AF_INET;
  peer.sin_port = htons(p);
  peer.sin_addr = *(struct in_addr*) he->h_addr;

  if (connect(fd, (struct sockaddr*) &peer, sizeof(peer)) < 0) 
   {
    close(fd);
    return -1;
   }
   
  return fd; 
 }  

static char *read_line(int s)
 {
  static char response[1024];
  char *c;
  int m = sizeof(response);
  
  c = response;
  while (m--)
   {
    char ch;
    fd_set fs;
    struct timeval t;
    
    FD_ZERO(&fs);
    FD_SET(s, &fs);
    
    t.tv_sec = TIMEOUT;
    t.tv_usec = 0;
    
    if (select(FD_SETSIZE, &fs, 0, 0, &t) <= 0) 
     return NULL;
     
    if (read(s, &ch, sizeof(ch)) != sizeof(ch)) 
     return NULL;
     
    if (ch == 10)
     {
      *c = 0;
      return response;
     } 
     
    *(c++) = ch;
   }    
   
  return NULL; 
 }

static int write_line(int s, char *p)
 {
  char *p2;
  p2 = g_malloc(strlen(p)+3);
  strcat(strcpy(p2, p), "\r\n");

  if (write(s, p2, strlen(p2)) ==  strlen(p2))
   {
    g_free(p2);
    return 1;
   }
   
  g_free(p2); 
  return 0;
 }


static int is_pop3_answer_ok(const char *p)
 {
  if (p) 
   if (p[0] == '+') return 1;
  
  return 0;
 }
 
int pop3_check(const char *h, const char* n, const char* e)
{
  int s;
  char *c;
  char *x;
  int r = -1, msg = 0, last = 0;
  
  if (!h || !n || !e) return -1;
  
  s = connect_socket(h, 110);
  if (s > 0) {
    if (!is_pop3_answer_ok(read_line(s))) {
      close(s);
      return -1;
    }

    c = g_strdup_printf("USER %s", n);
    if (!write_line(s, c) ||
        !is_pop3_answer_ok(read_line(s))) {
      close(s);
      g_free(c);
      return -1;
    }
    g_free(c);

    c = g_strdup_printf("PASS %s", e);
    if (!write_line(s, c) ||
        !is_pop3_answer_ok(read_line(s))) {
      close(s);
      g_free(c);
      return -1;
    }
    g_free(c);

    if (write_line(s, "STAT") &&
        is_pop3_answer_ok(x = read_line(s)) &&
	x != NULL &&
        sscanf(x, "%*s %d %*d", &msg) == 1)
      r = ((unsigned int)msg & 0x0000FFFFL);

    if (r != -1 &&
        write_line(s, "LAST") &&
        is_pop3_answer_ok(x = read_line(s)) &&
	x != NULL &&
        sscanf(x, "%*s %d", &last) == 1)
      r |= (unsigned int)(msg - last) << 16;

    if (write_line(s, "QUIT"))
      read_line(s);

    close(s);
  }     
   
  return r;
}

static int is_imap_answer_untagged(const char *tag)
 {
  return tag ? *tag == '*' : 0;
 }

static int is_imap_answer_ok(const char *p)
 {
  if (p) 
   {
    const char *b = strchr(p, ' ');
    if (b)
     {
      if (*(++b) == 'O' && *(++b) == 'K') 
       return 1;
     }
   }
  
  return 0;
 }

static char *wait_for_imap_answer(int s, char *tag)
 {
  char *p;
  int i = 10; /* read not more than 10 lines */
  
  while (i--)
   {
    p = read_line(s);
    if (!p) return 0;
    if (strncmp(p, tag, strlen(tag)) == 0) return p;
   }
   
  return 0; 
 }

int 
imap_check(const char *h, const char* n, const char* e, const char* f)
{
	int s;
	char *c = NULL;
	char *x;
	unsigned int r = (unsigned int) -1;
	int total = 0, unseen = 0;
	int count = 0;
	
	if (!h || !n || !e) return -1;
	
	if (f == NULL ||
	    f[0] == '\0')
		f = "INBOX";
	
	s = connect_socket(h, 143);
	
	if (s < 0)
		return r;
	
	x = read_line(s);
	/* The greeting is untagged */
	if (!is_imap_answer_untagged(x))
		goto return_from_imap_check;
	
	if (!is_imap_answer_ok(x))
		goto return_from_imap_check;
	
	
	c = g_strdup_printf("A1 LOGIN \"%s\" \"%s\"", n, e);
	if (!write_line(s, c))
		goto return_from_imap_check;
	
	if (!is_imap_answer_ok(wait_for_imap_answer(s, "A1")))
		goto return_from_imap_check;
	
	
	c = g_strdup_printf("A2 STATUS \"%s\" (MESSAGES UNSEEN)",f);
	if (!write_line(s, c))
		goto return_from_imap_check;
	
	/* We only loop 5 times if more than that this is
	 * probably a bogus reply */
	for (count = 0, x = read_line(s);
	     count < 5 && x != NULL;
	     count++, x = read_line(s)) {
		char tmpstr[4096];
		
		if (sscanf(x, "%*s %*s %*s %*s %d %4095s %d",
			   &total, tmpstr, &unseen) != 3)
			continue;
		if (strcmp(tmpstr, "UNSEEN") == 0)
			break;
		
	}
	
	r = (((unsigned int) unseen ) << 16) | /* lt unseen only */
		((unsigned int) total & 0x0000FFFFL);
	
	if (write_line(s, "A3 LOGOUT"))
		read_line(s);

 return_from_imap_check:

	g_free (c);
	close (s);
	
	return r; 
 }

