/*
 * Copyright 2002 Sun Microsystems Inc.
 *
 * Gnome Panel Accessibility support module
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include "panel-util.h"

static gint is_gail_loaded (GtkWidget *widget);

/* variable that indicates whether GAIL is loaded or not */
gint gail_loaded = -1;

/* Accessibility support routines for panel */
static gint
is_gail_loaded (GtkWidget *widget)
{
	AtkObject *aobj;

	if (gail_loaded == -1) {
		aobj = gtk_widget_get_accessible (widget);
		if (!GTK_IS_ACCESSIBLE (aobj))
			gail_loaded = 0;
		else
			gail_loaded = 1;
	}
	return gail_loaded;
}

/* routine to add accessible name and description to an atk object */
void
add_atk_name_desc (GtkWidget *widget, gchar *name, gchar *desc)
{
	AtkObject *aobj;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (! is_gail_loaded (widget))
		return;

	aobj = gtk_widget_get_accessible (widget);

	if (name)
		atk_object_set_name (aobj, name);
	if (desc)
		atk_object_set_description (aobj, desc);
}

/**
 * set_relation
 * @widget : The Gtk widget which is labelled by @label
 * @label : The label for the @widget.
 * @set_for : if 1, set ATK_RELATION_LABEL_FOR
 *	      if 0, don't set ATK_RELATION_LABEL_FOR.
 * Description : This function establishes atk relation
 * between a gtk widget and a label.
 */
void
set_relation (GtkWidget *widget, GtkLabel *label, int set_for)
{
	AtkObject *aobject;
	AtkRelationSet *relation_set;
	AtkRelation *relation; 
	AtkObject *targets[1];

	g_return_if_fail (GTK_IS_WIDGET(widget));
	g_return_if_fail (GTK_IS_LABEL(label));

	/* Return if GAIL is not loaded */
	if (! is_gail_loaded (widget))
		return;

	aobject = gtk_widget_get_accessible (widget);

	if (set_for)
		/* Set the ATK_RELATION_LABEL_FOR relation */
		gtk_label_set_mnemonic_widget (label, widget);

	targets[0] = gtk_widget_get_accessible (GTK_WIDGET (label));

	relation_set = atk_object_ref_relation_set (aobject);

	relation = atk_relation_new (targets, 1, ATK_RELATION_LABELLED_BY);
	atk_relation_set_add (relation_set, relation);
	g_object_unref (G_OBJECT (relation));
}
