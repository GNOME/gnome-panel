#ifndef BUTTON_WIDGET_H
#define BUTTON_WIDGET_H

#include <gtk/gtk.h>
#include "panel-types.h"

G_BEGIN_DECLS
	
#define BUTTON_TYPE_WIDGET		(button_widget_get_type ())
#define BUTTON_WIDGET(object)          	(G_TYPE_CHECK_INSTANCE_CAST ((object), BUTTON_TYPE_WIDGET, ButtonWidget))
#define BUTTON_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass, BUTTON_TYPE_WIDGET, ButtonWidgetClass))
#define BUTTON_IS_WIDGET(object)    	(G_TYPE_CHECK_INSTANCE_TYPE ((object), BUTTON_TYPE_WIDGET)) 
#define BUTTON_IS_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), BUTTON_TYPE_WIDGET))

typedef struct _ButtonWidget		ButtonWidget;
typedef struct _ButtonWidgetClass	ButtonWidgetClass;

struct _ButtonWidget {
	GtkButton    parent;
	
	GdkPixbuf   *pixbuf;
	GdkPixbuf   *scaled;
	GdkPixbuf   *scaled_hc;

	/* Invariant: assert (!filename || !stock_id) */
	char        *filename;
	char        *stock_id;

	int          size;

	PanelOrient  orient;

	guint        pressed_timeout;

	guint        ignore_leave  : 1;
	guint        arrow         : 1;
	guint        dnd_highlight : 1;
};

struct _ButtonWidgetClass
{
	GtkButtonClass parent_class;
};

GType      button_widget_get_type          (void) G_GNUC_CONST;

GtkWidget *button_widget_new               (const char   *pixmap,
					    int           size,
					    gboolean      arrow,
					    PanelOrient   orient);
GtkWidget *button_widget_new_from_stock    (const char   *stock_id,
					    int           size,
					    gboolean      arrow,
					    PanelOrient   orient);
void       button_widget_set_pixmap        (ButtonWidget *button,
					    const char   *pixmap);
void       button_widget_set_stock_id      (ButtonWidget *button,
					    const char   *stock_id);
void       button_widget_set_params        (ButtonWidget *button,
					    gboolean      arrow,
					    PanelOrient   orient);
void       button_widget_set_dnd_highlight (ButtonWidget *button,
					    gboolean      highlight);

G_END_DECLS

#endif /* __BUTTON_WIDGET_H__ */
