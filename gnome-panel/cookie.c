#include <config.h>
#include <gnome.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#define COOKIE_SIZE 12

char *
create_cookie (void)
{
	int i;
	char *cookie, *p;

	srandom (time (NULL));
	p = cookie = g_malloc (COOKIE_SIZE + 1);
	for (i = 0; i < COOKIE_SIZE; i++){
		int v = (random () % 50) + 'A';

		*p++ = v;
	}
	return cookie;
}
