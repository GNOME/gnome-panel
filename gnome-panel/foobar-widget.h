/* GNOME panel: foobar widget
 * Copyright 2000 Helix Code, Inc.
 *
 * Author:  Jacob Berkman
 *
 */

#ifndef FOOBAR_WIDGET_H
#define FOOBAR_WIDGET_H

#include "panel-types.h"

G_BEGIN_DECLS

#define FOOBAR_TYPE_WIDGET     		(foobar_widget_get_type ())
#define FOOBAR_WIDGET(object)       	(G_TYPE_CHECK_INSTANCE_CAST ((object), FOOBAR_TYPE_WIDGET, FoobarWidget))
#define FOOBAR_WIDGET_CLASS(klass) 	(G_TYPE_CHECK_CLASS_CAST ((klass), FOOBAR_TYPE_WIDGET, FoobarWidgetType))
#define FOOBAR_IS_WIDGET(object)    	(G_TYPE_CHECK_INSTANCE_TYPE ((object), FOOBAR_TYPE_WIDGET))

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

	GtkWidget *programs;

	GHashTable *windows;
	GtkWidget *task_item;
	GtkWidget *task_menu;
	GtkWidget *task_image;
	GtkWidget *task_bin;
	gpointer icon_window; /* the task whoose icon is shown,
			       * hopefully should be always OK,
			       * but we only use the pointer value
			       * and never dereference this */

	gboolean compliant_wm;

	guint notify;
};

struct _FoobarWidgetClass
{
	GtkWindowClass panel_class;

	gboolean (* popup_panel_menu) (FoobarWidget	*foobar);
};

GType		foobar_widget_get_type		(void) G_GNUC_CONST;

GtkWidget *	foobar_widget_new		(const char *panel_id, int screen);

void		foobar_widget_update_winhints	(FoobarWidget *foo);
void		foobar_widget_redo_window	(FoobarWidget *foo);

gboolean	foobar_widget_exists		(int screen);
gint		foobar_widget_get_height	(int screen);

void		foobar_widget_force_menu_remake	(void);

G_END_DECLS

#endif
