/*
 * panel-applet-private.h:
 *
 * Authors:
 *   Mark McLoughlin <mark@skynet.ie>
 *
 * Copyright 2001 Sun Microsystems, Inc.
 */

#ifndef __PANEL_APPLET_PRIVATE_H__
#define __PANEL_APPLET_PRIVATE_H__

#include "panel-applet.h"

G_BEGIN_DECLS

void panel_applet_change_orient (PanelApplet       *applet,
				 PanelAppletOrient  orient);

void panel_applet_change_size   (PanelApplet       *applet,
				 const gint         size);


void panel_applet_set_background_colour (PanelApplet *applet,
					 GdkColor    *colour);

void panel_applet_set_background_pixmap (PanelApplet *applet,
					 const gchar *pixmap);

void panel_applet_clear_background      (PanelApplet *applet);

G_END_DECLS

#endif /* __PANEL_APPLET_PRIVATE_H__ */
