/*
 * This is a sample shell to test panel modules.
 * Invoke with: module_test module_name
 *
 * Author: Miguel de Icaza
 */
#include <dlfcn.h>
#include <stdio.h>
#include "gnome.h"

extern GtkWidget *init (GtkWidget *window, char *arguments);
	
int
main (int argc, char *argv [])
{
	GtkWidget *window;
	GtkWidget *thing;
	void *lib_handle;
	char *f;
	void *(*init_ptr)(GtkWidget *, char *);

	gtk_init (&argc, &argv);
	gnome_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	if (argc != 2)
		return printf ("use: module_test module-name\n");

	lib_handle = dlopen (argv [1], RTLD_LAZY);
	if (!lib_handle)
		return printf ("Could not dlopen\n");
	init_ptr = dlsym (lib_handle, "init");
	if (!init_ptr)
		return printf ("Could not find init in the module\n");
	
	thing = (*init_ptr) (window, ".");

	if (!thing)
		return printf ("Module was not initialized\n");
	
	gtk_container_add (GTK_CONTAINER (window), (GtkWidget *) thing);
	gtk_widget_set_usize (window, 48, 48);
	gtk_window_set_policy ((GtkWindow *) window, 0, 0, 1);
	gtk_widget_show (window);
	gtk_widget_realize (window);
	gtk_main ();
	
	return 0;
}
