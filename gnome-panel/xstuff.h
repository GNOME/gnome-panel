#ifndef XSTUFF_H

#include <gdk/gdkx.h>

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

extern GdkAtom KWM_MODULE;
extern GdkAtom KWM_MODULE_DOCKWIN_ADD;
extern GdkAtom KWM_MODULE_DOCKWIN_REMOVE;
extern GdkAtom KWM_DOCKWINDOW;
extern GdkAtom _WIN_CLIENT_LIST;
extern GdkAtom _WIN_SUPPORTING_WM_CHECK;

void xstuff_init(void);
void xstuff_set_simple_hint(GdkWindow *w, GdkAtom atom, int val);
void xstuff_setup_kde_dock_thingie(GdkWindow *w);

gboolean xstuff_is_compliant_wm(void);

gpointer get_typed_property_data (Display *xdisplay,
				  Window   xwindow,
				  Atom     property,
				  Atom     requested_type,
				  gint    *size_p,
				  guint    expected_format);
gboolean send_client_message_1L (Window recipient,
				 Window event_window,
				 Atom   message_type,
				 long   event_mask,
				 glong  long1);

gboolean get_window_id(guint32 window, char *title, guint32 *wid,
		       gboolean depth);

/* if we want to select the substructure notify or not as it can get
 * a little expensive, so this should be called whenever we add/remove to
 * check_swallows or anything else that's gonna use it in the future */
void xstuff_reset_need_substructure(void);

void xstuff_set_no_group(GdkWindow *win);

#endif
