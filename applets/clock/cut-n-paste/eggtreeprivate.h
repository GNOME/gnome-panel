/* eggtreeprivate.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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
 */

#ifndef __EGG_TREE_PRIVATE_H__
#define __EGG_TREE_PRIVATE_H__


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtk.h>

/* FIXME: do something to make sure we're not relying on gtk+ internals */
  
/* cool ABI compat hack */
#define EGG_CELL_RENDERER_INFO_KEY "gtk-cell-renderer-info"

typedef struct _EggCellRendererInfo EggCellRendererInfo;
struct _EggCellRendererInfo
{
  GdkColor cell_background;

  /* text renderer */
  gulong focus_out_id;

  /* toggle renderer */
  guint inconsistent :1;
};

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __EGG_TREE_PRIVATE_H__ */

