/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* quick-desktop-reader.h - Quick .desktop file reader

   Copyright (C) 1999, 2000 Red Hat Inc.
   Copyright (C) 2001 Sid Vicious
   All rights reserved.

   This file is part of the Gnome Library.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */
/*
  @NOTATION@
 */

#ifndef QUICK_DESKTOP_READER_H
#define QUICK_DESKTOP_READER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define TYPE_QUICK_DESKTOP_ITEM         (quick_desktop_item_get_type ())
GType quick_desktop_item_get_type       (void);

typedef struct _QuickDesktopItem QuickDesktopItem;
struct _QuickDesktopItem {
	char *type;

	/* localized and in UTF-8 */
	char *name;
	/* localized and in UTF-8 */
	char *comment;

	char *icon;
	char *tryexec;
};

QuickDesktopItem * quick_desktop_item_load_file (const char  *file,
						 const char  *expected_type,
						 gboolean     run_tryexec);
QuickDesktopItem * quick_desktop_item_load_uri  (const char  *uri,
						 const char  *expected_type,
						 gboolean     run_tryexec);

void		   quick_desktop_item_destroy   (QuickDesktopItem *item);

char *             quick_desktop_item_find_icon (const char  *icon);


G_END_DECLS

#endif /* QUICK_DESKTOP_READER_H */
