#ifndef GDKEXTRA_H
#define GDKEXTRA_H


#include <gdk/gdktypes.h>

#ifdef _cplusplus
extern "C" {
#endif /* _cplusplus */

void gdk_pointer_warp(GdkWindow *src_w,
		      GdkWindow *dest_w,
		      gint       src_x,
		      gint       src_y,
		      guint      src_width,
		      guint      src_height,
		      gint       dest_x,
		      gint       dest_y);

#ifdef _cplusplus
}
#endif /* _cplusplus */


#endif
