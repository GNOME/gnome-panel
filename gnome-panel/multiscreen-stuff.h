/*
 *   multiscreen-stuff: Multiscreen and Xinerama support for the panel.
 *
 *   Copyright (C) 2001 George Lebl <jirka@5z.com>
 *                 2002 Sun Microsystems Inc.
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
 *
 * Authors: George Lebl <jirka@5z.com>,
 *          Mark McLoughlin <mark@skynet.ie>
 */

#ifndef MULTISCREEN_STUFF_H
#define MULTISCREEN_STUFF_H

#include <gtk/gtk.h>

void	multiscreen_init		(void);
void	multiscreen_reinit		(void);

int	multiscreen_screens		(void);
int	multiscreen_monitors		(int        screen);

int	multiscreen_x			(int        screen,
					 int        monitor);
int	multiscreen_y			(int        screen,
					 int        monitor);
int	multiscreen_width		(int        screen,
					 int        monitor);
int	multiscreen_height		(int        screen,
					 int        monitor);
int	multiscreen_locate_coords	(int        screen,
					 int        x,
					 int        y);
int	multiscreen_locate_widget	(int        screen,
					 GtkWidget *widget);

#endif /* MULTISCREEN_STUFF_H */
