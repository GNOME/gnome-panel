/*overall include file*/
#ifndef PANEL_INCLUDE_H
#define PANEL_INCLUDE_H

#include "panel-types.h"

#include "panel-widget.h"
#include "snapped-widget.h"
#include "drawer-widget.h"
#include "corner-widget.h"

#include "button-widget.h"

#include "panel.h"
#include "applet.h"
#include "session.h"
#include "main.h"

#include "panel_config.h"
#include "panel_config_global.h"

#include "menu.h"
#include "drawer.h"
#include "swallow.h"
#include "launcher.h"
#include "logout.h"
#include "extern.h"

#include "mulapp.h"
#include "exec-queue.h"

#include "orbit-glue.h"

#include "panel-util.h"
#include "gdkextra.h"

/* Gross backward compatibility hack.  */
#ifndef GPOINTER_TO_INT
# if SIZEOF_INT == SIZEOF_VOID_P
#  define GPOINTER_TO_INT(p)	((gint)(p))
#  define GINT_TO_POINTER(i)    ((gpointer)(i))
# elif SIZEOF_LONG == SIZEOF_VOID_P
#  define GPOINTER_TO_INT(p)	((gint)(glong)(p))
#  define GINT_TO_POINTER(i)	((gpointer)(glong)(i))
# endif /* SIZEOF_INT */
#endif /* GPOINTER_TO_INT */

#endif
