#ifndef __APPLET_INIT_H__
#define __APPLET_INIT_H__

#include <libbonobo.h>

int applet_factory_main (int                     argc,
			 char                  **argv,
			 const gchar            *iid,
			 const gchar            *name,
			 const gchar            *version,
			 BonoboFactoryCallback   callback,
			 gpointer                data);

void applet_debug_init  (void);
void applet_debug_log   (const char *format, ...);

#define APPLET_BONOBO_FACTORY(iid, name, version, callback, data)               \
int main (int argc, char *argv [])                                              \
{                                                                               \
	__applet_debug_init ();							\
        return applet_factory_main (argc, argv, iid, name,                      \
				    version, callback, data);                   \
}

#ifndef APPLET_DEBUG
#define __applet_debug_init
#else 
#define __applet_debug_init applet_debug_init
#endif

#endif /* __APPLET_INIT_H__ */
