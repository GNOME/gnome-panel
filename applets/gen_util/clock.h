/*
 * GNOME time/date display module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 *
 * Feel free to implement new look and feels :-)
 */

#ifndef _CLOCK_H_
#define _CLOCK_H_

#include <glib/gmacros.h>
#include <panel-applet.h>

G_BEGIN_DECLS

gboolean fill_clock_applet(PanelApplet *applet);

G_END_DECLS

#endif
