/*
 * panel-applet-shell.h:
 *
 * Authors:
 *   Mark McLoughlin <mark@skynet.ie>
 *
 * Copyright 2001 Sun Microsystems, Inc.
 */

#ifndef __PANEL_APPLET_SHELL_H__
#define __PANEL_APPLET_SHELL_H__

#include <bonobo/bonobo-object.h>

#include "panel-applet.h"
#include "GNOME_Panel.h"

#define PANEL_APPLET_SHELL_TYPE        (panel_applet_shell_get_type ())
#define PANEL_APPLET_SHELL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_APPLET_SHELL_TYPE, PanelAppletShell))
#define PANEL_APPLET_SHELL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST    ((k), PANEL_APPLET_SHELL_TYPE, PanelAppletShellClass))
#define PANEL_IS_APPLET_SHELL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_APPLET_SHELL_TYPE))
#define PANEL_IS_APPLET_SHELL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE    ((k), PANEL_APPLET_SHELL_TYPE))

typedef struct _PanelAppletShellPrivate PanelAppletShellPrivate;

typedef struct {
	BonoboObject             base;

	PanelAppletShellPrivate *priv;
} PanelAppletShell;

typedef struct {
	BonoboObjectClass               base_class;

	POA_GNOME_PanelAppletShell__epv epv;
} PanelAppletShellClass;


GType             panel_applet_shell_get_type  (void) G_GNUC_CONST;

void              panel_applet_shell_construct (PanelAppletShell *shell,
						PanelApplet      *applet);

PanelAppletShell *panel_applet_shell_new       (PanelApplet      *applet);


#endif /* PANEL_APPLET_SHELL_H */
