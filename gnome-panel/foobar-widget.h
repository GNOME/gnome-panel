/* GNOME panel: foobar widget
 * Copyright 2000 Helix Code, Inc.
 *
 * Author:  Jacob Berkman
 *
 */

#ifndef FOOBAR_WIDGET_H
#define FOOBAR_WIDGET_H

#include <gnome.h>
#include "panel-types.h"
#include "gwmh.h"

BEGIN_GNOME_DECLS

#define TYPE_FOOBAR_WIDGET     (foobar_widget_get_type ())
#define FOOBAR_WIDGET(o)       (GTK_CHECK_CAST ((o), TYPE_FOOBAR_WIDGET, FoobarWidget))
#define FOOBAR_WIDGET_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), TYPE_FOOBAR_WIDGET, FoobarWidgetType))
#define IS_FOOBAR_WIDGET(o)    (GTK_CHECK_TYPE ((o), TYPE_FOOBAR_WIDGET))

typedef struct _FoobarWidget           FoobarWidget;
typedef struct _FoobarWidgetClass      FoobarWidgetClass;

struct _FoobarWidget
{
	GtkWindow window;

	/*< private >*/

	int screen;

	GtkWidget *ebox;
	GtkWidget *hbox;
	GtkWidget *panel;
	GtkWidget *clock_ebox;
	GtkWidget *clock_label;
	guint clock_timeout;

	GtkWidget *programs;
	GtkWidget *favorites;
	GtkWidget *settings;

	GHashTable *tasks;
	GtkWidget *task_item;
	GtkWidget *task_menu;
	GtkWidget *task_pixmap;
	GtkWidget *task_bin;
	GwmhTask *icon_task; /* the task whoose icon is shown,
			      * hopefully should be always OK,
			      * but we only use the pointer value
			      * and never dereference this */

	gboolean compliant_wm;
	char *clock_format;

	guint notify;
};

struct _FoobarWidgetClass
{
	GtkWindowClass panel_class;
};

GtkType		foobar_widget_get_type		(void) G_GNUC_CONST;
GtkWidget *	foobar_widget_new		(int screen);

void		foobar_widget_update_winhints	(FoobarWidget *foo);
void		foobar_widget_redo_window	(FoobarWidget *foo);

void		foobar_widget_set_clock_format	(FoobarWidget *foo,
						 const char *format);


gboolean	foobar_widget_exists		(int screen);
gint		foobar_widget_get_height	(int screen);

void		foobar_widget_force_menu_remake	(void);

void		foobar_widget_global_set_clock_format (const char *format);

END_GNOME_DECLS

#endif
