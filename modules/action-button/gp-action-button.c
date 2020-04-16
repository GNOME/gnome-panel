/*
 * Copyright (C) 2020 Alberts Muktupāvels
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
#include "gp-action-button.h"

struct _GpActionButton
{
  GtkButton parent;
};

G_DEFINE_TYPE (GpActionButton, gp_action_button, GTK_TYPE_BUTTON)

static void
gp_action_button_class_init (GpActionButtonClass *self_class)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (self_class);

  gtk_widget_class_set_css_name (widget_class, "gp-action-button");
}

static void
gp_action_button_init (GpActionButton *self)
{
}

GtkWidget *
gp_action_button_new (void)
{
  return g_object_new (GP_TYPE_ACTION_BUTTON, NULL);
}
