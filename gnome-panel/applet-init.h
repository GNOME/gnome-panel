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

#define APPLET_BONOBO_FACTORY(iid, name, version, callback, data)               \
int main (int argc, char *argv [])                                              \
{                                                                               \
        return applet_factory_main (argc, argv, iid, name,                      \
				    version, callback, data);                   \
}

#endif /* __APPLET_INIT_H__ */
