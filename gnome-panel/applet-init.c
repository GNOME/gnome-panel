#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>

#include "applet-init.h"

int
applet_factory_main (int                     argc,
		     char                  **argv,
		     const gchar            *iid,
		     const gchar            *name,
		     const gchar            *version,
		     BonoboFactoryCallback   callback,
		     gpointer                data)
{
	GnomeProgram *program;

	program = gnome_program_init (name, version,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_NONE);

	g_object_set (G_OBJECT (program), 
		      GNOME_CLIENT_PARAM_SM_CONNECT,
		      FALSE, NULL);

	return bonobo_generic_factory_main (iid, callback, data);
}
