/*
 * panel-enums-gsettings.h:
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.

 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_ENUMS_GSETTINGS_H__
#define __PANEL_ENUMS_GSETTINGS_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	PANEL_OBJECT_PACK_START  = 0,
	PANEL_OBJECT_PACK_CENTER = 1,
	PANEL_OBJECT_PACK_END    = 2
} PanelObjectPackType;

typedef enum { /*< flags=0 >*/
	PANEL_ORIENTATION_TOP    = 1 << 0,
	PANEL_ORIENTATION_RIGHT  = 1 << 1,
	PANEL_ORIENTATION_BOTTOM = 1 << 2,
	PANEL_ORIENTATION_LEFT   = 1 << 3
} PanelOrientation;

#define PANEL_HORIZONTAL_MASK (PANEL_ORIENTATION_TOP | PANEL_ORIENTATION_BOTTOM)
#define PANEL_VERTICAL_MASK (PANEL_ORIENTATION_LEFT | PANEL_ORIENTATION_RIGHT)

typedef enum {
	PANEL_ALIGNMENT_START  = 0,
	PANEL_ALIGNMENT_CENTER = 1,
	PANEL_ALIGNMENT_END    = 2
} PanelAlignment;

typedef enum {
	PANEL_ANIMATION_SLOW   = 0,
	PANEL_ANIMATION_MEDIUM = 1,
	PANEL_ANIMATION_FAST   = 2
} PanelAnimationSpeed;

typedef enum {
	PANEL_BACKGROUND_IMAGE_STYLE_NONE    = 0,
	PANEL_BACKGROUND_IMAGE_STYLE_STRETCH = 1,
	PANEL_BACKGROUND_IMAGE_STYLE_FIT     = 2
} PanelBackgroundImageStyle;

typedef enum {
	PANEL_THEME_VARIANT_SYSTEM,
	PANEL_THEME_VARIANT_LIGHT,
	PANEL_THEME_VARIANT_DARK
} PanelThemeVariant;

G_END_DECLS

#endif /* __PANEL_ENUMS_GSETTINGS_H__ */
