/*
 *   multiscreen-stuff: Xinerama (and in the future multidisplay)
 *   support for the panel
 *
 *   Copyright (C) 2001 George Lebl <jirka@5z.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef MULTISCREEN_STUFF_H
#define MULTISCREEN_STUFF_H

void		multiscreen_init		(void);

int		multiscreen_screens		(void) G_GNUC_CONST;

/* information about a screen */
int		multiscreen_x			(int screen);
int		multiscreen_y			(int screen);
int		multiscreen_width		(int screen);
int		multiscreen_height		(int screen);
int		multiscreen_screen_from_pos	(int x,
						 int y);
int		multiscreen_screen_from_panel	(GtkWidget *widget);

#endif /* MULTISCREEN_STUFF_H */
