/*
 * GNOME panel x stuff
 * Copyright 2000,2001 Eazel, Inc.
 *
 * Authors: George Lebl
 */
#include <config.h>
#include <gnome.h>
#include <gdk/gdkx.h>

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "panel-include.h"
#include "gnome-panel.h"
#include "global-keys.h"
#include "gwmh.h"

#include "xstuff.h"
#include "multiscreen-stuff.h"

static GdkAtom KWM_MODULE = 0;
static GdkAtom KWM_MODULE_DOCKWIN_ADD = 0;
static GdkAtom KWM_MODULE_DOCKWIN_REMOVE = 0;
static GdkAtom KWM_DOCKWINDOW = 0;
static GdkAtom NAUTILUS_DESKTOP_WINDOW_ID = 0;
static GdkAtom _NET_WM_DESKTOP = 0;
static GdkAtom GNOME_PANEL_DESKTOP_AREA = 0;

extern GList *check_swallows;

/*list of all panel widgets created*/
extern GSList *panel_list;

static void xstuff_setup_global_desktop_area (int left, int right,
					      int top, int bottom);

static void
steal_statusspot(StatusSpot *ss, Window winid)
{
	GdkDragProtocol protocol;

	gtk_socket_steal(GTK_SOCKET(ss->socket), winid);
	if (gdk_drag_get_protocol (winid, &protocol))
		gtk_drag_dest_set_proxy (GTK_WIDGET (ss->socket),
					 GTK_SOCKET(ss->socket)->plug_window,
					 protocol, TRUE);
}

static void
try_adding_status(guint32 winid)
{
	guint32 *data;
	int size;

	if (status_applet_get_ss (winid))
		return;

	data = get_typed_property_data (GDK_DISPLAY (),
					winid,
					KWM_DOCKWINDOW,
					KWM_DOCKWINDOW,
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

void
xstuff_go_through_client_list (void)
{
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
}

static void
redo_interface (void)
{
	GSList *li;
	for (li = panel_list; li != NULL; li = li->next) {
		PanelData *pd = li->data;
		if (IS_BASEP_WIDGET (pd->panel))
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

void
xstuff_init (void)
{
	KWM_MODULE = gdk_atom_intern ("KWM_MODULE", FALSE);
	KWM_MODULE_DOCKWIN_ADD =
		gdk_atom_intern ("KWM_MODULE_DOCKWIN_ADD", FALSE);
	KWM_MODULE_DOCKWIN_REMOVE =
		gdk_atom_intern ("KWM_MODULE_DOCKWIN_REMOVE", FALSE);
	KWM_DOCKWINDOW = gdk_atom_intern ("KWM_DOCKWINDOW", FALSE);
	NAUTILUS_DESKTOP_WINDOW_ID =
		gdk_atom_intern ("NAUTILUS_DESKTOP_WINDOW_ID", FALSE);
	_NET_WM_DESKTOP =
		gdk_atom_intern ("_NET_WM_DESKTOP", FALSE);
	GNOME_PANEL_DESKTOP_AREA =
		gdk_atom_intern ("GNOME_PANEL_DESKTOP_AREA", FALSE);

	gwmh_init ();

	gwmh_desk_notifier_add (desk_notifier, NULL);
	gwmh_task_notifier_add (task_notifier, NULL);

	/* setup the keys filter */
	gdk_window_add_filter (GDK_ROOT_PARENT(),
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
					NAUTILUS_DESKTOP_WINDOW_ID,
					XA_WINDOW,
					&size, 32);
	if (data != NULL &&
	    *data != 0) {
		guint32 *desktop;
		desktop = get_typed_property_data (GDK_DISPLAY (),
						   *data,
						   _NET_WM_DESKTOP,
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
xstuff_set_simple_hint (GdkWindow *w, GdkAtom atom, long val)
{
	gdk_error_trap_push ();
	XChangeProperty (GDK_DISPLAY (), GDK_WINDOW_XWINDOW (w), atom, atom,
			 32, PropModeReplace, (unsigned char*)&val, 1);
	gdk_flush ();
	gdk_error_trap_pop ();
}

static GdkFilterReturn
status_event_filter (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	XEvent *xevent;

	xevent = (XEvent *)gdk_xevent;

	if(xevent->type == ClientMessage) {
		if(xevent->xclient.message_type == KWM_MODULE_DOCKWIN_ADD &&
		   !status_applet_get_ss(xevent->xclient.data.l[0])) {
			Window w = xevent->xclient.data.l[0];
			StatusSpot *ss;
			ss = new_status_spot ();
			if (ss != NULL)
				steal_statusspot (ss, w);
		} else if(xevent->xclient.message_type ==
			  KWM_MODULE_DOCKWIN_REMOVE) {
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
	xstuff_set_simple_hint (w, KWM_MODULE, 2);
	gdk_window_add_filter (w, status_event_filter, NULL);
	send_client_message_1L (GDK_ROOT_WINDOW (), GDK_WINDOW_XWINDOW (w),
				KWM_MODULE, SubstructureNotifyMask,
				GDK_WINDOW_XWINDOW (w));
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
send_client_message_1L (Window recipient,
			Window event_window,
			Atom   message_type,
			long   event_mask,
			glong  long1)
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
	GwmhDesk *desk;

	desk = gwmh_desk_get_config ();

	return desk->detected_gnome_wm;
}

void
xstuff_set_no_group_and_no_input (GdkWindow *win)
{
	XWMHints *old_wmhints;
	XWMHints wmhints = {0};
	static GdkAtom wm_client_leader_atom = GDK_NONE;

	if (wm_client_leader_atom == GDK_NONE)
		wm_client_leader_atom = gdk_atom_intern ("WM_CLIENT_LEADER",
							 FALSE);

	gdk_property_delete (win, wm_client_leader_atom);

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

	XChangeProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (), 
			 GNOME_PANEL_DESKTOP_AREA, XA_CARDINAL,
			 32, PropModeReplace, (unsigned char*)vals, 4);

	old_left = left;
	old_right = right;
	old_top = top;
	old_bottom = bottom;
}

void
xstuff_setup_desktop_area (int screen, int left, int right, int top, int bottom)
{
	char *screen_atom;
	GdkAtom atom;
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
	atom = gdk_atom_intern (screen_atom, FALSE);
	g_free (screen_atom);

	XChangeProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (), 
			 atom, XA_CARDINAL,
			 32, PropModeReplace, (unsigned char*)vals, 4);

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
	GdkAtom atom;

	gdk_error_trap_push ();

	XDeleteProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
			 GNOME_PANEL_DESKTOP_AREA);

	for (i = 0; i < multiscreen_screens (); i++) {
		screen_atom =
			g_strdup_printf ("GNOME_PANEL_DESKTOP_AREA_%d", i);
		atom = gdk_atom_intern (screen_atom, FALSE);
		g_free (screen_atom);

		XDeleteProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (), atom);
	}

	gdk_flush ();
	gdk_error_trap_pop ();
}

void
xstuff_set_pos_size (GdkWindow *window, int x, int y, int w, int h)
{
	Window win = GDK_WINDOW_XWINDOW (window);
	XSizeHints size_hints;
  
	size_hints.flags = USPosition | PPosition | USSize | PSize | PMaxSize | PMinSize;
	size_hints.x = x;
	size_hints.y = y;
	size_hints.width = w;
	size_hints.height = h;
	size_hints.min_width = w;
	size_hints.min_height = h;
	size_hints.max_width = w;
	size_hints.max_height = h;
  
	gdk_error_trap_push ();

	XSetWMNormalHints (GDK_DISPLAY (), win, &size_hints);

	gdk_window_move_resize (window, x, y, w, h);

	gdk_error_trap_pop ();
}


void
xstuff_set_wmspec_dock_hints (GdkWindow *window)
{
        Atom atom;
        
        atom = XInternAtom (gdk_display,
                            "_NET_WM_WINDOW_TYPE_DOCK",
                            False);

        XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                         GDK_WINDOW_XWINDOW (window),
                         XInternAtom (gdk_display,
                                      "_NET_WM_WINDOW_TYPE",
                                      False),
                         XA_ATOM, 32, PropModeReplace,
                         (guchar *)&atom, 1);
}
