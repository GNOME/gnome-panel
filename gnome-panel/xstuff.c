/*
 * GNOME panel x stuff
 * Copyright 2000 Eazel, Inc.
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

#include "xstuff.h"

GdkAtom KWM_MODULE = 0;
GdkAtom KWM_MODULE_DOCKWIN_ADD = 0;
GdkAtom KWM_MODULE_DOCKWIN_REMOVE = 0;
GdkAtom KWM_DOCKWINDOW = 0;
GdkAtom _WIN_CLIENT_LIST = 0;
GdkAtom _WIN_SUPPORTING_WM_CHECK = 0;

extern GList *check_swallows;

static guint32 *client_list = NULL;
static int client_list_size = 0;

/* if wm_restart is TRUE, then we should reget the wm check */
static gboolean wm_restart = TRUE;
/* cached value */
static gboolean compliant_wm = FALSE;
/* the wm window thingie */
static Window compliant_wm_win = None;

/*list of all panel widgets created*/
extern GSList *panel_list;

gboolean
get_window_id(guint32 window, char *title, guint32 *wid, gboolean depth)
{
	Window win = window;
	Window root_return;
	Window parent_return;
	Window *children = NULL;
	unsigned int nchildren;
	unsigned int i;
	char *tit;
	gboolean ret = FALSE;

	if(XFetchName(GDK_DISPLAY(), win, &tit) && tit) {
		if(strstr(tit, title)!=NULL) {
			if(wid) *wid = win;
			ret = TRUE;
		}
		XFree(tit);
	}

	if(!depth || ret)
		return ret;

	XQueryTree(GDK_DISPLAY(),
		   win,
		   &root_return,
		   &parent_return,
		   &children,
		   &nchildren);
	
	/*otherwise we got a problem*/
	if(children) {
		for(i=0;!ret && i<nchildren;i++)
			ret=get_window_id(children[i], title, wid, depth);
		XFree(children);
	}
	return ret;
}

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

	if(status_applet_get_ss(winid))
		return;

	data = get_typed_property_data (GDK_DISPLAY(),
					winid,
					KWM_DOCKWINDOW,
					KWM_DOCKWINDOW,
					&size, 32);

	if(data && *data) {
		StatusSpot *ss;
		ss = new_status_spot();
		if(ss)
			steal_statusspot(ss, winid);
	}
	g_free(data);
}

static void
try_checking_swallows(guint32 winid)
{
	char *tit = NULL;
	if(XFetchName(GDK_DISPLAY(), winid, &tit) &&
	   tit) {
		GList *li;
		for(li = check_swallows; li;
		    li = g_list_next(li)) {
			Swallow *swallow = li->data;
			if(strstr(tit,swallow->title)!=NULL) {
				swallow->wid = winid;
				gtk_socket_steal(GTK_SOCKET(swallow->socket),
						 swallow->wid);
				check_swallows = 
					g_list_remove(check_swallows, swallow);
				xstuff_reset_need_substructure();
				break;
			}
		}
		XFree(tit);
	}
}

static void
go_through_client_list(void)
{
	int i;
	gdk_error_trap_push ();
	/* just for status dock stuff for now */
	for(i=0;i<client_list_size;i++) {
		if(check_swallows)
			try_checking_swallows(client_list[i]);
		try_adding_status(client_list[i]);
	}
	gdk_flush();
	gdk_error_trap_pop ();
}

static int redo_interface_timeout_handle = 0;

static gboolean
redo_interface_timeout(gpointer data)
{
	gboolean old_compliant = compliant_wm;

	redo_interface_timeout_handle = 0;

	wm_restart = TRUE;
	/* redo the compliancy stuff */
	if(old_compliant != xstuff_is_compliant_wm()) {
		GSList *li;
		for(li = panel_list; li != NULL; li = g_slist_next(li)) {
			PanelData *pd = li->data;
			if(IS_BASEP_WIDGET(pd->panel))
				basep_widget_redo_window(BASEP_WIDGET(pd->panel));
			else if(IS_FOOBAR_WIDGET(pd->panel))
				foobar_widget_redo_window(FOOBAR_WIDGET(pd->panel));
		}
	}

	return FALSE;
}

static void
redo_interface(void)
{
	if(redo_interface_timeout_handle)
		gtk_timeout_remove(redo_interface_timeout_handle);

	redo_interface_timeout_handle = 
		gtk_timeout_add(1000,redo_interface_timeout,NULL);
}

static GdkFilterReturn
wm_event_filter(GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	XEvent *xevent;

	xevent = (XEvent *)gdk_xevent;

	if(xevent->type == DestroyNotify) {
		if(compliant_wm_win != None &&
		   compliant_wm_win == xevent->xdestroywindow.window)
			redo_interface();
	}

	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
event_filter(GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	XEvent *xevent;

	xevent = (XEvent *)gdk_xevent;

	switch(xevent->type) {
	case KeyPress:
	case KeyRelease:
		return panel_global_keys_filter(gdk_xevent, event);
	case PropertyNotify:
		if(xevent->xproperty.atom == _WIN_CLIENT_LIST) {
			g_free(client_list);
			client_list = get_typed_property_data (GDK_DISPLAY(),
							       GDK_ROOT_WINDOW(),
							       _WIN_CLIENT_LIST,
							       XA_CARDINAL,
							       &client_list_size, 32);
			/* size returned is the number of bytes */
			client_list_size /= 4;
			go_through_client_list();
		} else if(xevent->xproperty.atom == _WIN_SUPPORTING_WM_CHECK) {
			redo_interface();
		}
		break;
	case MapNotify:
		if (check_swallows) {
			GList *li;
			int remove; /* counts the number of NULLs we
				       should remove from the
				       check_swallows_list */

			gdk_error_trap_push ();

			remove = 0;
			for(li = check_swallows; li; li = g_list_next(li)) {
				Swallow *swallow = li->data;
				if(get_window_id(xevent->xmap.window,
						 swallow->title,
						 &(swallow->wid),
						 FALSE)) {
					gtk_socket_steal(GTK_SOCKET(swallow->socket),swallow->wid);
					li->data = NULL;
					remove++;
				}
			}
			while(remove--)
				check_swallows = g_list_remove(check_swallows,NULL);
			if(!check_swallows)
				xstuff_reset_need_substructure();

			gdk_flush();
			gdk_error_trap_pop ();
		}
	}
	/*if ((event->any.window) &&
	    (gdk_window_get_type(event->any.window) == GDK_WINDOW_FOREIGN))
		return GDK_FILTER_REMOVE;
	else*/
	return GDK_FILTER_CONTINUE;
}

void
xstuff_reset_need_substructure(void)
{
	XWindowAttributes attribs = { 0 };

	gdk_error_trap_push ();

	/* select events, we need to trap the kde status thingies anyway */
	XGetWindowAttributes (GDK_DISPLAY (),
			      GDK_ROOT_WINDOW (),
			      &attribs);
	if (check_swallows) {
		XSelectInput (GDK_DISPLAY (),
			      GDK_ROOT_WINDOW (),
			      attribs.your_event_mask |
			      SubstructureNotifyMask);
	} else {
		XSelectInput (GDK_DISPLAY (),
			      GDK_ROOT_WINDOW (),
			      attribs.your_event_mask &
			      ~SubstructureNotifyMask);
	}
	gdk_flush ();

	gdk_error_trap_pop ();
}

void
xstuff_init(void)
{
	XWindowAttributes attribs = { 0 };

	KWM_MODULE = gdk_atom_intern("KWM_MODULE",FALSE);
	KWM_MODULE_DOCKWIN_ADD =
		gdk_atom_intern("KWM_MODULE_DOCKWIN_ADD",FALSE);
	KWM_MODULE_DOCKWIN_REMOVE =
		gdk_atom_intern("KWM_MODULE_DOCKWIN_REMOVE",FALSE);
	KWM_DOCKWINDOW = gdk_atom_intern("KWM_DOCKWINDOW", FALSE);
	_WIN_CLIENT_LIST = gdk_atom_intern("_WIN_CLIENT_LIST",FALSE);
	_WIN_SUPPORTING_WM_CHECK = gdk_atom_intern("_WIN_SUPPORTING_WM_CHECK",FALSE);

	/* set up a filter on the root window to get map requests */
	/* we will select the events later when we actually need them */
	gdk_window_add_filter(GDK_ROOT_PARENT(), event_filter, NULL);

	gdk_error_trap_push ();

	/* select events, we need to trap the kde status thingies anyway */
	XGetWindowAttributes (GDK_DISPLAY (),
			      GDK_ROOT_WINDOW (),
			      &attribs);
	XSelectInput (GDK_DISPLAY (),
		      GDK_ROOT_WINDOW (),
		      attribs.your_event_mask |
		      SubstructureNotifyMask |
		      StructureNotifyMask |
		      PropertyChangeMask);
	gdk_flush ();

	gdk_error_trap_pop ();

	client_list = get_typed_property_data (GDK_DISPLAY(),
					       GDK_ROOT_WINDOW(),
					       _WIN_CLIENT_LIST,
					       XA_CARDINAL,
					       &client_list_size, 32);
	/* size returned is the number of bytes */
	client_list_size /= 4;

	go_through_client_list();
}

void
xstuff_set_simple_hint(GdkWindow *w, GdkAtom atom, int val)
{
	gdk_error_trap_push();
	XChangeProperty(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(w), atom, atom,
			32, PropModeReplace, (unsigned char*)&val, 1);
	gdk_flush();
	gdk_error_trap_pop();
}

static GdkFilterReturn
status_event_filter(GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	XEvent *xevent;

	xevent = (XEvent *)gdk_xevent;

	if(xevent->type == ClientMessage) {
		if(xevent->xclient.message_type == KWM_MODULE_DOCKWIN_ADD &&
		   !status_applet_get_ss(xevent->xclient.data.l[0])) {
			Window w = xevent->xclient.data.l[0];
			StatusSpot *ss;
			ss = new_status_spot();
			if(ss)
				steal_statusspot(ss, w);
		} else if(xevent->xclient.message_type ==
			  KWM_MODULE_DOCKWIN_REMOVE) {
			StatusSpot *ss;
			ss = status_applet_get_ss(xevent->xclient.data.l[0]);
			if(ss)
				status_spot_remove(ss,TRUE);
		}
	}

	return GDK_FILTER_CONTINUE;
}

void
xstuff_setup_kde_dock_thingie(GdkWindow *w)
{
	xstuff_set_simple_hint(w,KWM_MODULE,2);
	gdk_window_add_filter(w, status_event_filter, NULL);
	send_client_message_1L(GDK_ROOT_WINDOW(),GDK_WINDOW_XWINDOW(w),
			       KWM_MODULE,SubstructureNotifyMask,
			       GDK_WINDOW_XWINDOW(w));
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

/* also quite stolen from deskguide */
gboolean
xstuff_is_compliant_wm(void)
{
	guint32 *prop_data;
	int size;

	if(!wm_restart)
		return compliant_wm;

	compliant_wm_win = None;

	prop_data = get_typed_property_data (GDK_DISPLAY (),
					     GDK_ROOT_WINDOW (),
					     _WIN_SUPPORTING_WM_CHECK,
					     XA_CARDINAL,
					     &size, 32);
	if(prop_data) {
		Window check_window = prop_data[0];
		guint32 *wm_check_data;

		wm_check_data = get_typed_property_data (GDK_DISPLAY (),
							 check_window,
							 _WIN_SUPPORTING_WM_CHECK,
							 XA_CARDINAL,
							 &size, 32);
		if (wm_check_data &&
		    wm_check_data[0] == check_window)
			compliant_wm_win = check_window;
		g_free (prop_data);
		g_free (wm_check_data);
	}
	compliant_wm = (compliant_wm_win!=None);

	if(compliant_wm_win) {
		XWindowAttributes attribs = { 0 };
		GdkWindow *win;

		win = gdk_window_foreign_new(compliant_wm_win);

		gdk_window_add_filter(win, wm_event_filter, NULL);

		gdk_error_trap_push ();

		XGetWindowAttributes(GDK_DISPLAY (),
				     compliant_wm_win,
				     &attribs);
		XSelectInput(GDK_DISPLAY (),
			     compliant_wm_win,
			     attribs.your_event_mask |
			     StructureNotifyMask);
		gdk_flush ();

		gdk_error_trap_pop ();
	}

	wm_restart = FALSE;

	return compliant_wm;
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
