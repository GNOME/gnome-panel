#include <config.h>
#include <gnome.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include "cookie.h"

#define COOKIE_SIZE 12

char *
create_cookie (void)
{
	int i;
	char *cookie, *p;

	srandom (time (NULL));
	p = cookie = g_malloc (COOKIE_SIZE + 1);
	/*the frist two chars must be from the [a-zA-Z0-9./] set since
	  they are used as salt for crypt*/
	for (i = 0; i < 2; i++){
		int v;
		do
			v = (random () % 57) + 'A';
		while(!((v>='a' && v<='z') || (v>='A' && v<='Z') ||
		     (v>='0' && v<='9') || v=='.' || v=='/'));

		*p++ = v;
	}
	for (i = 2; i < COOKIE_SIZE; i++){
		int v = (random () % (126-33)) + 33;

		*p++ = v;
	}
	*p++='\0';
	return cookie;
}
