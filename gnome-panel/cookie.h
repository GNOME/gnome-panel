#ifndef COOKIE_H
#define COOKIE_H

BEGIN_GNOME_DECLS

extern char* cookie;
char * create_cookie (void);

#define CHECK_COOKIE() { \
	if (strcmp (cookie, ccookie)) { \
		g_warning("BAD COOKIE file: %s line: %d", \
		__FILE__,__LINE__); \
		return; \
	} \
}
#define CHECK_COOKIE_V(x) { \
	if (strcmp (cookie, ccookie)) { \
		g_warning("BAD COOKIE file: %s line: %d", \
		__FILE__,__LINE__); \
		return x; \
	} \
}

END_GNOME_DECLS

#endif
