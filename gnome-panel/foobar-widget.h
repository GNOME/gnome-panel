/* GNOME panel: foobar widget
 * Copyright 2000 Helix Code, Inc.
 *
 * Author:  Jacob Berkman
 *
 */

#ifndef __FOOBAR_WIDGET_H__
#define __FOOBAR_WIDGET_H__

#include <gnome.h>
#include "panel-types.h"

BEGIN_GNOME_DECLS

#define FOOBAR_WIDGET_TYPE     (foobar_widget_get_type ())
#define FOOBAR_WIDGET(o)       (GTK_CHECK_CAST (o, FOOBAR_WIDGET_TYPE, FoobarWidget))
#define FOOBAR_WIDGET_CLASS(k) (GTK_CHECK_CLASS_CAST (k, FOOBAR_WIDGET_TYPE, FoobarWidgetType))
#define IS_FOOBAR_WIDGET(o)    (GTK_CHECK_TYPE (o, FOOBAR_WIDGET_TYPE))

typedef struct _FoobarWidget           FoobarWidget;
typedef struct _FoobarWidgetClass      FoobarWidgetClass;

struct _FoobarWidget
{
	GtkWindow window;

	GtkWidget *hbox;
	GtkWidget *panel;

	gboolean compliant_wm;
};

struct _FoobarWidgetClass
{
	GtkWindowClass panel_class;
};



GtkType foobar_widget_get_type (void);
GtkWidget *foobar_widget_new (void);

void foobar_update_winhints (GtkWidget *foo, gpointer ignored);
void foobar_widget_redo_window (FoobarWidget *foo);

gboolean foobar_widget_exists (void);
gint foobar_widget_get_height (void);

END_GNOME_DECLS

#endif
