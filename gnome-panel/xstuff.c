/*
 * GNOME panel x stuff
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 *               2002 Sun Microsystems Inc.
 *
 * Authors: George Lebl <jirka@5z.com>
 *          Mark McLoughlin <mark@skynet.ie>
 */
#include <config.h>
#include <string.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "xstuff.h"

#include "multihead-hacks.h"

#include "global-keys.h"

static Atom
panel_atom_get (Display    *display,
		const char *atom_name)
{
	static GHashTable *atom_hash;
	Atom               retval;

	g_return_val_if_fail (display != NULL, None);
	g_return_val_if_fail (atom_name != NULL, None);

	if (!atom_hash)
		atom_hash = g_hash_table_new_full (
				g_str_hash, g_str_equal, g_free, NULL);

	retval = GPOINTER_TO_UINT (g_hash_table_lookup (atom_hash, atom_name));
	if (!retval) {
		retval = XInternAtom (display, atom_name, FALSE);

		if (retval != None)
			g_hash_table_insert (atom_hash, g_strdup (atom_name),
					     GUINT_TO_POINTER (retval));
	}

	return retval;
}

/* Stolen from deskguide */
static gpointer
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

gboolean
xstuff_is_compliant_wm (void)
{
	Display  *xdisplay;
	Window    root_window;
	gpointer  data;
	int       size;

	xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	root_window = GDK_WINDOW_XWINDOW (
				gdk_get_default_root_window ());

        /* FIXME this is totally broken; should be using
         * gdk_net_wm_supports() on particular hints when we rely
         * on those particular hints
         */
	data = get_typed_property_data (
			xdisplay, root_window,
			panel_atom_get (xdisplay ,"_NET_SUPPORTED"),
			XA_ATOM, &size, 32);

	if (!data)
		return FALSE;

	/* Actually checks for some of these */
	g_free (data);
	return TRUE;
}

void
xstuff_set_no_group_and_no_input (GdkWindow *win)
{
	XWMHints *old_wmhints;
	XWMHints wmhints = {0};

	XDeleteProperty (GDK_WINDOW_XDISPLAY (win),
			 GDK_WINDOW_XWINDOW (win),
			 panel_atom_get (GDK_WINDOW_XDISPLAY (win),
					 "WM_CLIENT_LEADER"));

	old_wmhints = XGetWMHints (GDK_WINDOW_XDISPLAY (win),
				   GDK_WINDOW_XWINDOW (win));
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

	XSetWMHints (GDK_WINDOW_XDISPLAY (win),
		     GDK_WINDOW_XWINDOW (win),
		     &wmhints);
}

/* This is such a broken stupid function. */   
void
xstuff_set_pos_size (GdkWindow *window, int x, int y, int w, int h)
{
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

       XSetWMNormalHints (GDK_WINDOW_XDISPLAY (window),
			  GDK_WINDOW_XWINDOW (window),
			  &size_hints);

       gdk_window_move_resize (window, x, y, w, h);

       gdk_flush ();
       gdk_error_trap_pop ();
}

void
xstuff_set_wmspec_dock_hints (GdkWindow *window,
			      gboolean autohide)
{
        Atom atoms [2] = { None, None };
        
	if (!autohide)
		atoms [0] = panel_atom_get (GDK_WINDOW_XDISPLAY (window),
					    "_NET_WM_WINDOW_TYPE_DOCK");
	else {
		atoms [0] = panel_atom_get (GDK_WINDOW_XDISPLAY (window),
					    "_GNOME_WINDOW_TYPE_AUTOHIDE_PANEL");
		atoms [1] = panel_atom_get (GDK_WINDOW_XDISPLAY (window),
					    "_NET_WM_WINDOW_TYPE_DOCK");
	}

        XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                         GDK_WINDOW_XWINDOW (window),
			 panel_atom_get (GDK_WINDOW_XDISPLAY (window),
					 "_NET_WM_WINDOW_TYPE"),
                         XA_ATOM, 32, PropModeReplace,
                         (unsigned char *) atoms, 
			 autohide ? 2 : 1);
}

void
xstuff_set_wmspec_strut (GdkWindow *window,
			 int        left,
			 int        right,
			 int        top,
			 int        bottom)
{
	long vals [4];
        
	vals [0] = left;
	vals [1] = right;
	vals [2] = top;
	vals [3] = bottom;

        XChangeProperty (GDK_WINDOW_XDISPLAY (window),
                         GDK_WINDOW_XWINDOW (window),
			 panel_atom_get (GDK_WINDOW_XDISPLAY (window),
					 "_NET_WM_STRUT"),
                         XA_CARDINAL, 32, PropModeReplace,
                         (unsigned char *) vals, 4);
}

void
xstuff_delete_property (GdkWindow *window, const char *name)
{
	Display *xdisplay = GDK_WINDOW_XDISPLAY (window);
	Window   xwindow  = GDK_WINDOW_XWINDOW (window);

        XDeleteProperty (xdisplay, xwindow,
			 panel_atom_get (xdisplay, name));
}

void
xstuff_init (void)
{
#ifdef HAVE_GTK_MULTIHEAD
	GdkDisplay *display;
	int         screens, i;

	display = gdk_display_get_default ();
	screens = gdk_display_get_n_screens (display);

	for (i = 0; i < screens; i++) {
		GdkScreen *screen;
		GdkWindow *root_window;

		screen = gdk_display_get_screen (display, i);
		root_window = gdk_screen_get_root_window (screen);

		gdk_window_add_filter (
			root_window, panel_global_keys_filter, NULL);
	}
#else
	gdk_window_add_filter (gdk_get_default_root_window (),
			       panel_global_keys_filter, NULL);
#endif
}
