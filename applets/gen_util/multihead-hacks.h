#ifndef __MULTIHEAD_HACKS__
#define __MULTIHEAD_HACKS__

/* Multihead support:
 *     Remove this when we require gtk+ with multihead.
 */

#include "config.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#ifndef HAVE_GTK_MULTIHEAD

#define gtk_window_set_screen(a,b)

#endif /* HAVE_GTK_MULTIHEAD */

G_END_DECLS

#endif /* __MULTIHEAD_HACKS__ */
