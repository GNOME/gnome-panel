#ifndef COOKIE_H
#define COOKIE_H

#include <crypt.h>

BEGIN_GNOME_DECLS

extern char* cookie;
char * create_cookie (void);

#define CHECK_COOKIE() { \
	if (strcmp (crypt(cookie,cookie), ccookie)) { \
		g_warning("BAD COOKIE file: %s line: %d", \
		__FILE__,__LINE__); \
		return; \
	} \
}
#define CHECK_COOKIE_V(x) { \
	if (strcmp (crypt(cookie,cookie), ccookie)) { \
		g_warning("BAD COOKIE file: %s line: %d", \
		__FILE__,__LINE__); \
		return x; \
	} \
}

END_GNOME_DECLS

#endif
