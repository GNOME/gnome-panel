#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>

#include "applet-init.h"

#define APPLET_DEBUG_OUTPUT "/tmp/applet-debug"

static FILE *static_applet_debug_fh = NULL;

void
applet_debug_log (const char *format, ...)
{
	va_list args;

	g_return_if_fail (static_applet_debug_fh);

	va_start (args, format);

	vfprintf (static_applet_debug_fh, format, args);

	fflush (static_applet_debug_fh);

	va_end (args);
}

void
applet_debug_init (void)
{
	g_return_if_fail (!static_applet_debug_fh);

	static_applet_debug_fh = fopen (APPLET_DEBUG_OUTPUT, "w");
}

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
	int           retval;

	program = gnome_program_init (name, version,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_NONE);

	g_object_set (G_OBJECT (program), 
		      GNOME_CLIENT_PARAM_SM_CONNECT,
		      FALSE, NULL);

	applet_debug_log ("applet_factory_main starting '%s' '%s' '%s'.\n",
			  iid, name, version);

	retval = bonobo_generic_factory_main (iid, callback, data);

	applet_debug_log ("applet_factory_main finishing %d.\n", retval);

	return retval;
}
