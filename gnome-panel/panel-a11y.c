/*
 * panel-a11y.c: panel accessibility support module
 *
 * Copyright (C) 2002, 2003 Sun Microsystems Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 */

#include "panel-a11y.h"

static int
is_gail_loaded (GtkWidget *widget)
{
	static gboolean initialised = FALSE;
	static gboolean gail_loaded = FALSE;

	if (!initialised) {
		gail_loaded = GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (widget));
		initialised = TRUE;
	}

	return gail_loaded;
}

void
panel_a11y_set_atk_name_desc (GtkWidget  *widget,
			      const char *name, 
			      const char *desc)
{
	AtkObject *aobj;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (!is_gail_loaded (widget))
		return;

	aobj = gtk_widget_get_accessible (widget);

	if (name)
		atk_object_set_name (aobj, name);

	if (desc)
		atk_object_set_description (aobj, desc);
}

/**
 * panel_a11y_set_atk_relation
 * @widget : The Gtk widget which is labelled by @label
 * @label : The label for the @widget.
 *
 * Description : This function establishes atk relation
 * between a gtk widget and a label.
 */
void
panel_a11y_set_atk_relation (GtkWidget *widget,
			     GtkLabel  *label)
{
	AtkObject      *aobject;
	AtkRelationSet *relation_set;
	AtkRelation    *relation; 
	AtkObject      *targets [1];

	g_return_if_fail (GTK_IS_WIDGET(widget));
	g_return_if_fail (GTK_IS_LABEL(label));

	if (!is_gail_loaded (widget))
		return;

	aobject = gtk_widget_get_accessible (widget);

	gtk_label_set_mnemonic_widget (label, widget);

	targets [0] = gtk_widget_get_accessible (GTK_WIDGET (label));

	relation_set = atk_object_ref_relation_set (aobject);

	relation = atk_relation_new (targets, 1, ATK_RELATION_LABELLED_BY);
	atk_relation_set_add (relation_set, relation);
	g_object_unref (relation);
}
