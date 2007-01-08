/*
 * panel-shell.h: panel shell interface implementation
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      Jacob Berkman <jacob@ximian.com>
 */

#ifndef __PANEL_SHELL_H__
#define __PANEL_SHELL_H__

#include "GNOME_Panel.h"

#include <bonobo/bonobo-object.h>

#define PANEL_SHELL_TYPE        (panel_shell_get_type ())
#define PANEL_SHELL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_SHELL_TYPE, PanelShell))
#define PANEL_SHELL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST    ((k), PANEL_SHELL_TYPE, PanelShellClass))
#define PANEL_IS_SHELL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_SHELL_TYPE))
#define PANEL_IS_SHELL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE    ((k), PANEL_SHELL_TYPE))

typedef BonoboObject PanelShell;

typedef struct {
	BonoboObjectClass                 parent_class;

	POA_GNOME_Vertigo_PanelShell__epv epv;
} PanelShellClass;

GType     panel_shell_get_type (void) G_GNUC_CONST;

gboolean  panel_shell_register   (void);
void      panel_shell_unregister (void);

#endif /* __PANEL_SHELL_H__ */
