/*
 * GNOME panel x stuff
 * Copyright 2000,2001 Eazel, Inc.
 *
 * Authors: George Lebl
 */
#include <config.h>
#include <gdk/gdkx.h>
#include <string.h>

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

/* Yes, yes I know, now bugger off ... */
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

#include "xstuff.h"
#include "multiscreen-stuff.h"
#include "basep-widget.h"
#include "foobar-widget.h"
#include "global-keys.h"
#include "status.h"
#include "swallow.h"

extern GList *check_swallows;

/*list of all panel widgets created*/
extern GSList *panel_list;

static void xstuff_setup_global_desktop_area (int left, int right,
					      int top, int bottom);

#define ATOM(name) xstuff_atom_intern(GDK_DISPLAY(),name)
/* Once we have multiple display support we need to only use
 * the below ones */

#define ATOMD(display,name) xstuff_atom_intern(display,name)
#define ATOMEV(event,name) xstuff_atom_intern(((XAnyEvent *)event)->display,name)
#define ATOMGDK(win,name) xstuff_atom_intern(GDK_WINDOW_XDISPLAY(win),name)

Atom
xstuff_atom_intern (Display *display, const char *name)
{
	static GHashTable *cache = NULL;
	char *key;
	Atom atom;

	if (cache == 0)
		cache = g_hash_table_new (g_str_hash, g_str_equal);

	key = g_strdup_printf ("%p %s", display, name);

	atom = (Atom)g_hash_table_lookup (cache, key);
	if (atom == 0) {
		atom = XInternAtom (display, name, False);
		g_hash_table_insert (cache, key, (gpointer)atom);
	} else {
		g_free (key);
	}

	return atom;
}

static void
steal_statusspot(StatusSpot *ss, Window winid)
{
	GdkDragProtocol protocol;

	gtk_socket_add_id (GTK_SOCKET (ss->socket), winid);
	if (gdk_drag_get_protocol (winid, &protocol))
		gtk_drag_dest_set_proxy (GTK_WIDGET (ss->socket),
					 GTK_SOCKET(ss->socket)->plug_window,
					 protocol, TRUE);
}

#ifdef FIXME
static void
try_adding_status(guint32 winid)
{
	guint32 *data;
	int size;

	if (status_applet_get_ss (winid))
		return;

	data = get_typed_property_data (GDK_DISPLAY (),
					winid,
					ATOM ("KWM_DOCKWINDOW"),
					ATOM ("KWM_DOCKWINDOW"),
					&size, 32);

	if(data && *data) {
		StatusSpot *ss;
		ss = new_status_spot ();
		if (ss != NULL)
			steal_statusspot (ss, winid);
	}
	g_free(data);
}

static void
try_checking_swallows (GwmhTask *task)
{
	GList *li;

	if (!task->name)
		return;

	for (li = check_swallows; li; li = li->next) {	     
		Swallow *swallow = li->data;
		if (strstr (task->name, swallow->title) != NULL) {
			swallow->wid = task->xwin;
			gtk_socket_steal (GTK_SOCKET (swallow->socket),
					  swallow->wid);
			check_swallows = 
				g_list_remove (check_swallows, swallow);
			break;
		}
	}
}

#endif /* FIXME */

void
xstuff_go_through_client_list (void)
{
#ifdef FIXME
	GList *li;

	gdk_error_trap_push ();
	/* just for status dock stuff for now */
	for (li = gwmh_task_list_get (); li != NULL; li = li->next) {
		GwmhTask *task = li->data;
		/* skip own windows */
		if (task->name != NULL &&
		    strcmp (task->name, "panel") == 0)
			continue;
		if (check_swallows != NULL)
			try_checking_swallows (task);
		try_adding_status (task->xwin);
	}
	gdk_flush();
	gdk_error_trap_pop ();
#endif
}

#ifdef FIXME

static void
redo_interface (void)
{
	GSList *li;
	for (li = panel_list; li != NULL; li = li->next) {
		PanelData *pd = li->data;
		if (BASEP_IS_WIDGET (pd->panel))
			basep_widget_redo_window (BASEP_WIDGET (pd->panel));
		else if (IS_FOOBAR_WIDGET (pd->panel))
			foobar_widget_redo_window (FOOBAR_WIDGET (pd->panel));
	}
}

/* some deskguide code borrowed */
static gboolean
desk_notifier (gpointer func_data,
	       GwmhDesk *desk,
	       GwmhDeskInfoMask change_mask)
{
	if (change_mask & GWMH_DESK_INFO_BOOTUP)
		redo_interface ();

	/* we should maybe notice desk changes here */

	return TRUE;
}

static gboolean
task_notifier (gpointer func_data,
	       GwmhTask *task,
	       GwmhTaskNotifyType ntype,
	       GwmhTaskInfoMask imask)
{
	/* skip own windows */
	if (task->name != NULL &&
	    strcmp (task->name, "panel") == 0)
		return TRUE;

	switch (ntype) {
	case GWMH_NOTIFY_NEW:
		try_adding_status (task->xwin);
		/* fall through */
	case GWMH_NOTIFY_INFO_CHANGED:
		if (check_swallows != NULL)
			try_checking_swallows (task);
		break;
	default:
		break;
	}

	return TRUE;
}

#endif /* FIXME */

void
xstuff_init (void)
{
#ifdef FIXME
	gwmh_init ();

	gwmh_desk_notifier_add (desk_notifier, NULL);
	gwmh_task_notifier_add (task_notifier, NULL);
#endif

	/* setup the keys filter */
	gdk_window_add_filter (gdk_get_default_root_window (),
			       panel_global_keys_filter,
			       NULL);

	gdk_error_trap_push ();

	xstuff_setup_global_desktop_area (0, 0, 0, 0);

	xstuff_go_through_client_list ();

	/* there is a flush in xstuff_go_through_client_list */
	gdk_error_trap_pop ();
}

gboolean
xstuff_nautilus_desktop_present (void)
{
	gboolean ret = FALSE;
	guint32 *data;
	int size;

	gdk_error_trap_push ();
	data = get_typed_property_data (GDK_DISPLAY (),
					GDK_ROOT_WINDOW (),
					ATOM ("NAUTILUS_DESKTOP_WINDOW_ID"),
					XA_WINDOW,
					&size, 32);

        /* Get random property off the window to see if the window
         * still exists.
         */
	if (data != NULL &&
	    *data != 0) {
		guint32 *desktop;
		desktop = get_typed_property_data (GDK_DISPLAY (),
						   *data,
						   ATOM ("_NET_WM_DESKTOP"),
						   XA_CARDINAL,
						   &size, 32);
		if (size > 0)
			ret = TRUE;
		g_free (desktop);
	}
	g_free (data);

	gdk_flush ();
	gdk_error_trap_pop ();

	return ret;
}

void
xstuff_set_simple_hint (GdkWindow *w, const char *name, long val)
{
	Atom atom = ATOMGDK (w, name);

	gdk_error_trap_push ();

	XChangeProperty (GDK_DISPLAY (),
			 GDK_WINDOW_XWINDOW (w),
			 atom, atom,
			 32, PropModeReplace,
			 (unsigned char*)&val, 1);

	gdk_flush ();
	gdk_error_trap_pop ();
}

static GdkFilterReturn
status_event_filter (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	XEvent *xevent;

	xevent = (XEvent *)gdk_xevent;

	if(xevent->type == ClientMessage) {
		if(xevent->xclient.message_type ==
		   ATOMEV (xevent, "KWM_MODULE_DOCKWIN_ADD") &&
		   !status_applet_get_ss(xevent->xclient.data.l[0])) {
			Window w = xevent->xclient.data.l[0];
			StatusSpot *ss;
			ss = new_status_spot ();
			if (ss != NULL)
				steal_statusspot (ss, w);
		} else if(xevent->xclient.message_type ==
			  ATOMEV (xevent, "KWM_MODULE_DOCKWIN_REMOVE")) {
			StatusSpot *ss;
			ss = status_applet_get_ss (xevent->xclient.data.l[0]);
			if (ss != NULL)
				status_spot_remove (ss, TRUE);
		}
	}

	return GDK_FILTER_CONTINUE;
}

void
xstuff_setup_kde_dock_thingie (GdkWindow *w)
{
	xstuff_set_simple_hint (w, "KWM_MODULE", 2);
	gdk_window_add_filter (w, status_event_filter, NULL);
	send_client_message_3L (GDK_ROOT_WINDOW (), GDK_WINDOW_XWINDOW (w),
				ATOMGDK (w, "KWM_MODULE"),
				SubstructureNotifyMask,
				GDK_WINDOW_XWINDOW (w), 0, 0);
}

/* Stolen from deskguide */
gpointer
get_typed_property_data (Display *xdisplay,
			 Window   xwindow,
			 Atom     property,
			 Atom     requested_type,
			 gint    *size_p,
			 guint    expected_format)
{
  static const guint prop_buffer_lengh = 1024 * 1024;
  unsigned char *prop_data = NULL;
  Atom type_returned = 0;
  unsigned long nitems_return = 0, bytes_after_return = 0;
  int format_returned = 0;
  gpointer data = NULL;
  gboolean abort = FALSE;

  g_return_val_if_fail (size_p != NULL, NULL);
  *size_p = 0;

  gdk_error_trap_push ();

  abort = XGetWindowProperty (xdisplay,
			      xwindow,
			      property,
			      0, prop_buffer_lengh,
			      False,
			      requested_type,
			      &type_returned, &format_returned,
			      &nitems_return,
			      &bytes_after_return,
			      &prop_data) != Success;
  if (gdk_error_trap_pop () ||
      type_returned == None)
    abort++;
  if (!abort &&
      requested_type != AnyPropertyType &&
      requested_type != type_returned)
    {
      g_warning (G_GNUC_PRETTY_FUNCTION "(): Property has wrong type, probably on crack");
      abort++;
    }
  if (!abort && bytes_after_return)
    {
      g_warning (G_GNUC_PRETTY_FUNCTION "(): Eeek, property has more than %u bytes, stored on harddisk?",
		 prop_buffer_lengh);
      abort++;
    }
  if (!abort && expected_format && expected_format != format_returned)
    {
      g_warning (G_GNUC_PRETTY_FUNCTION "(): Expected format (%u) unmatched (%d), programmer was drunk?",
		 expected_format, format_returned);
      abort++;
    }
  if (!abort && prop_data && nitems_return && format_returned)
    {
      switch (format_returned)
	{
	case 32:
	  *size_p = nitems_return * 4;
	  if (sizeof (gulong) == 8)
	    {
	      guint32 i, *mem = g_malloc0 (*size_p + 1);
	      gulong *prop_longs = (gulong*) prop_data;

	      for (i = 0; i < *size_p / 4; i++)
		mem[i] = prop_longs[i];
	      data = mem;
	    }
	  break;
	case 16:
	  *size_p = nitems_return * 2;
	  break;
	case 8:
	  *size_p = nitems_return;
	  break;
	default:
	  g_warning ("Unknown property data format with %d bits (extraterrestrial?)",
		     format_returned);
	  break;
	}
      if (!data && *size_p)
	{
	  guint8 *mem = g_malloc (*size_p + 1);

	  memcpy (mem, prop_data, *size_p);
	  mem[*size_p] = 0;
	  data = mem;
	}
    }

  if (prop_data)
    XFree (prop_data);
  
  return data;
}

/* sorta stolen from deskguide */
gboolean
send_client_message_3L (Window recipient,
			Window event_window,
			Atom   message_type,
			long   event_mask,
			long   long1,
			long   long2,
			long   long3)
{
  XEvent xevent = { 0 };

  xevent.type = ClientMessage;
  xevent.xclient.window = event_window;
  xevent.xclient.message_type = message_type;
  xevent.xclient.format = 32;
  xevent.xclient.data.l[0] = long1;

  gdk_error_trap_push ();

  XSendEvent (GDK_DISPLAY (), recipient, False, event_mask, &xevent);
  gdk_flush ();

  return !gdk_error_trap_pop ();
}

gboolean
xstuff_is_compliant_wm (void)
{
	gpointer data;
	int size;

        /* FIXME this is totally broken; should be using
         * gdk_net_wm_supports() on particular hints when we rely
         * on those particular hints
         */
        
	data = get_typed_property_data (GDK_DISPLAY (),
					GDK_ROOT_WINDOW (),
					ATOM ("_NET_SUPPORTED"),
					XA_ATOM,
					&size, 32);
	if (data != NULL) {
		/* Actually checks for some of these */
		g_free (data);
		return TRUE;
	} else {
		return FALSE;
	}
}

void
xstuff_set_no_group_and_no_input (GdkWindow *win)
{
	XWMHints *old_wmhints;
	XWMHints wmhints = {0};

	XDeleteProperty (GDK_WINDOW_XDISPLAY (win),
			 GDK_WINDOW_XWINDOW (win),
			 ATOMGDK (win, "WM_CLIENT_LEADER"));

	old_wmhints = XGetWMHints (GDK_DISPLAY (), GDK_WINDOW_XWINDOW (win));
	/* General paranoia */
	if (old_wmhints != NULL) {
		memcpy (&wmhints, old_wmhints, sizeof (XWMHints));
		XFree (old_wmhints);

		wmhints.flags &= ~WindowGroupHint;
		wmhints.flags |= InputHint;
		wmhints.input = False;
		wmhints.window_group = 0;
	} else {
		/* General paranoia */
		wmhints.flags = InputHint | StateHint;
		wmhints.window_group = 0;
		wmhints.input = False;
		wmhints.initial_state = NormalState;
	}
	XSetWMHints (GDK_DISPLAY (),
		     GDK_WINDOW_XWINDOW (win),
		     &wmhints);
}

static void
xstuff_setup_global_desktop_area (int left, int right, int top, int bottom)
{
	long vals[4];
	static int old_left = -1, old_right = -1, old_top = -1, old_bottom = -1;

	left = left >= 0 ? left : old_left;
	right = right >= 0 ? right : old_right;
	top = top >= 0 ? top : old_top;
	bottom = bottom >= 0 ? bottom : old_bottom;

	if (old_left == left &&
	    old_right == right &&
	    old_top == top &&
	    old_bottom == bottom)
		return;

	vals[0] = left;
	vals[1] = right;
	vals[2] = top;
	vals[3] = bottom;

	XChangeProperty (GDK_DISPLAY (),
			 GDK_ROOT_WINDOW (), 
			 ATOM ("GNOME_PANEL_DESKTOP_AREA"),
			 XA_CARDINAL,
			 32, PropModeReplace,
			 (unsigned char *)vals, 4);

	old_left = left;
	old_right = right;
	old_top = top;
	old_bottom = bottom;
}

void
xstuff_setup_desktop_area (int screen, int left, int right, int top, int bottom)
{
	char *screen_atom;
	long vals[4];
	static int screen_width = -1, screen_height = -1;

	if (screen_width < 0)
		screen_width = gdk_screen_width ();
	if (screen_height < 0)
		screen_height = gdk_screen_height ();

	vals[0] = left;
	vals[1] = right;
	vals[2] = top;
	vals[3] = bottom;

	gdk_error_trap_push ();

	/* Note, when we do standard multihead and we have per screen
	 * root window, this should just set the GNOME_PANEL_DESKTOP_AREA */
	screen_atom = g_strdup_printf ("GNOME_PANEL_DESKTOP_AREA_%d",
				       screen);
	XChangeProperty (GDK_DISPLAY (),
			 GDK_ROOT_WINDOW (), 
			 ATOM (screen_atom),
			 XA_CARDINAL,
			 32, PropModeReplace,
			 (unsigned char *)vals, 4);

	g_free (screen_atom);

	xstuff_setup_global_desktop_area
		((multiscreen_x (screen)      == 0)             ? left   : -1,
		 (multiscreen_x (screen) +
		  multiscreen_width (screen)  == screen_width)  ? right  : -1,
		 (multiscreen_y (screen)      == 0)             ? top    : -1,
		 (multiscreen_y (screen) +
		  multiscreen_height (screen) == screen_height) ? bottom : -1);

	gdk_flush ();
	gdk_error_trap_pop ();
}

void
xstuff_unsetup_desktop_area (void)
{
	int i;
	char *screen_atom;

	gdk_error_trap_push ();

	XDeleteProperty (GDK_DISPLAY (),
			 GDK_ROOT_WINDOW (),
			 ATOM ("GNOME_PANEL_DESKTOP_AREA"));

	for (i = 0; i < multiscreen_screens (); i++) {
		screen_atom =
			g_strdup_printf ("GNOME_PANEL_DESKTOP_AREA_%d", i);

		XDeleteProperty (GDK_DISPLAY (),
				 GDK_ROOT_WINDOW (),
				 ATOM (screen_atom));

		g_free (screen_atom);
	}

	gdk_flush ();
	gdk_error_trap_pop ();
}

/* This is such a broken stupid function. */   
void
xstuff_set_pos_size (GdkWindow *window, int x, int y, int w, int h)
{
       Window win = GDK_WINDOW_XWINDOW (window);
       XSizeHints size_hints;

       /* Do not add USPosition / USSize here, fix the damn WM */
       size_hints.flags = PPosition | PSize | PMaxSize | PMinSize;
       size_hints.x = 0; /* window managers aren't supposed to and  */
       size_hints.y = 0; /* don't use these fields */
       size_hints.width = w;
       size_hints.height = h;
       size_hints.min_width = w;
       size_hints.min_height = h;
       size_hints.max_width = w;
       size_hints.max_height = h;
  
       gdk_error_trap_push ();

       XSetWMNormalHints (GDK_DISPLAY (), win, &size_hints);

       gdk_window_move_resize (window, x, y, w, h);

       gdk_flush ();
       gdk_error_trap_pop ();
}

void
xstuff_set_wmspec_dock_hints (GdkWindow *window,
			      gboolean autohide)
{
        Atom atoms[2] = { None, None };
        
	if (autohide) {
		atoms[0] = ATOMGDK (window, "_GNOME_WINDOW_TYPE_AUTOHIDE_PANEL");
		atoms[1] = ATOMGDK (window, "_NET_WM_WINDOW_TYPE_DOCK");
	} else {
		atoms[0] = ATOMGDK (window, "_NET_WM_WINDOW_TYPE_DOCK");
	}

        XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                         GDK_WINDOW_XWINDOW (window),
			 ATOMGDK (window, "_NET_WM_WINDOW_TYPE"),
                         XA_ATOM, 32, PropModeReplace,
                         (guchar *)atoms, 
			 autohide ? 2 : 1);
}

void
xstuff_set_wmspec_strut (GdkWindow *window,
			 int left,
			 int right,
			 int top,
			 int bottom)
{
	long vals[4];
        
	vals[0] = left;
	vals[1] = right;
	vals[2] = top;
	vals[3] = bottom;

        XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                         GDK_WINDOW_XWINDOW (window),
			 ATOMGDK (window, "_NET_WM_STRUT"),
                         XA_CARDINAL, 32, PropModeReplace,
                         (guchar *)vals, 4);
}

void
xstuff_delete_property (GdkWindow *window, const char *name)
{
        XDeleteProperty (GDK_WINDOW_XDISPLAY (window),
                         GDK_WINDOW_XWINDOW (window),
			 ATOMGDK (window, name));
}
