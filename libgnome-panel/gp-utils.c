/*
 * Copyright (C) 2018 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gp-utils.h"

/**
 * gp_add_text_color_class:
 * @widget: a #GtkWidget
 *
 * Use this function to add css class to widgets that are visible on
 * panel and shows text. Typically this function should be used with
 * #GtkLabel, #GtkMenuItem or #GtkButton as @widget.
 */
void
gp_add_text_color_class (GtkWidget *widget)
{
  GtkStyleContext *context;

  context = NULL;

  if (GTK_IS_MENU_ITEM (widget) || GTK_IS_BUTTON (widget))
    {
      GtkWidget *child;

      child = gtk_bin_get_child (GTK_BIN (widget));
      if (GTK_IS_LABEL (child))
        context = gtk_widget_get_style_context (child);
    }

  if (context == NULL)
    context = gtk_widget_get_style_context (widget);

  gtk_style_context_add_class (context, "gp-text-color");
}
