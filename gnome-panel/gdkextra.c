#include <X11/Xlib.h>
#include <gdk/gdkprivate.h>
#include "gdkextra.h"


void
gdk_pointer_warp(GdkWindow *src_w,
		 GdkWindow *dest_w,
		 gint       src_x,
		 gint       src_y,
		 guint      src_width,
		 guint      src_height,
		 gint       dest_x,
		 gint       dest_y)
{
  GdkWindowPrivate *src_private;
  GdkWindowPrivate *dest_private;
  Window xsrc_w;
  Window xdest_w;

  src_private = (GdkWindowPrivate *) src_w;
  dest_private = (GdkWindowPrivate *) dest_w;

  if (src_private)
    xsrc_w = src_private->xwindow;
  else
    xsrc_w = None;

  if (dest_private)
    xdest_w = dest_private->xwindow;
  else
    xdest_w = None;

  XWarpPointer (gdk_display,
		xsrc_w, xdest_w,
		src_x, src_y, src_width, src_height,
		dest_x, dest_y);
}
