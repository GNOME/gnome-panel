/* status-docket: the interface for generating a small docklets into the panel
 * status dock applet, THIS IS NOT A WIDGET, it's an object for the purpose of handeling
 * panel restarts or a panel that hasn't started yet etc ...
 * (C) 1999 the Free Software Foundation
 *
 * Author:  George Lebl
 */
#ifndef __STATUS_DOCKLET_H__
#define __STATUS_DOCKLET_H__

#include <gtk/gtk.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>

#include <gnome-panel.h>

BEGIN_GNOME_DECLS

#define TYPE_STATUS_DOCKLET          (status_docklet_get_type ())
#define STATUS_DOCKLET(obj)          GTK_CHECK_CAST (obj, status_docklet_get_type (), StatusDocklet)
#define STATUS_DOCKLET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, status_docklet_get_type (), StatusDockletClass)
#define IS_STATUS_DOCKLET(obj)       GTK_CHECK_TYPE (obj, status_docklet_get_type ())

typedef struct _StatusDocklet		StatusDocklet;
typedef struct _StatusDockletClass	StatusDockletClass;

/*infinite retries just sets the maximum_retries to INT_MAX*/
#define STATUS_DOCKLET_INFINITE INT_MAX
/*retry every n seconds*/
#define STATUS_DOCKLET_RETRY_EVERY 15
/*default number of retries to find the server*/
#define STATUS_DOCKLET_DEFAULT_RETRIES 20 /*retry for 5 minutes before giving up*/

struct _StatusDocklet
{
	GtkObject		object;
	
	/*< public >*/
	GtkWidget		*plug; /* a pointer to the current GtkPlug
					  holding the docklet */
	
	/*< private >*/
	GNOME_StatusSpot	sspot;
	
	int			tries; /*if we are set to trying to find the panel,
					 here is the count of our failed tries, we
					 try every 15 seconds*/
	int			maximum_retries; /*the maximum retries, default is*/
	guint			handle_restarts:1; /*handle panel restarts, by just looking
						     for the panel again for some time*/
	int			timeout_handle; /*handle for the retry timeout*/
};

struct _StatusDockletClass
{
	GtkObjectClass parent_class;

	/* called when we build the plug itself (meaning we did find a panel,
	   or after the panel was killed, we found it again after it's restart) */
	void (* build_plug) (StatusDocklet *docklet,
			     GtkWidget     *plug);
};

guint		status_docklet_get_type		(void) G_GNUC_CONST;

/*just creates a new object, but doesn't yet try to connect to a panel, you
  need to bind the build_plug signal which will make your widget, and then
  call status_docklet_run, which actually starts looking for the panel and
  all those things*/
GtkObject*	status_docklet_new		(void);
GtkObject*	status_docklet_new_full		(int maximum_retries,
						 gboolean handle_restarts);

/* this function will actually start looking for the panel and if it finds it
   calls the build_plug signal with the plug that it has added */
void		status_docklet_run		(StatusDocklet *docklet);

END_GNOME_DECLS

#endif /* __STATUS_DOCKLET_H__ */
