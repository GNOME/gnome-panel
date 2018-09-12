/*
 * Copyright (C) 2018 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gp-menu-button.h"

struct _GpMenuButton
{
  GtkButton parent;
};

G_DEFINE_TYPE (GpMenuButton, gp_menu_button, GTK_TYPE_BUTTON)

static void
gp_menu_button_class_init (GpMenuButtonClass *button_class)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (button_class);

  gtk_widget_class_set_css_name (widget_class, "gp-menu-button");
}

static void
gp_menu_button_init (GpMenuButton *button)
{
}

GtkWidget *
gp_menu_button_new (void)
{
  return g_object_new (GP_TYPE_MENU_BUTTON, NULL);
}
