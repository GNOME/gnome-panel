/*
 * panel-applet-shell.h: the panel's interface to the applet.
 *
 * Copyright (C) 2001 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *     Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_APPLET_SHELL_H__
#define __PANEL_APPLET_SHELL_H__

#include <bonobo/bonobo-object.h>

#include <panel-applet.h>
#include <GNOME_Panel.h>

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
	BonoboObjectClass                       base_class;

	POA_GNOME_Vertigo_PanelAppletShell__epv epv;
} PanelAppletShellClass;


GType             panel_applet_shell_get_type  (void) G_GNUC_CONST;

void              panel_applet_shell_construct (PanelAppletShell *shell,
						PanelApplet      *applet);

PanelAppletShell *panel_applet_shell_new       (PanelApplet      *applet);


#endif /* PANEL_APPLET_SHELL_H */
