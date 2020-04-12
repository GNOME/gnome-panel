#ifndef BUTTON_WIDGET_H
#define BUTTON_WIDGET_H

#include <gtk/gtk.h>
#include "panel-enums.h"

G_BEGIN_DECLS
	
#define BUTTON_TYPE_WIDGET		(button_widget_get_type ())
#define BUTTON_WIDGET(object)          	(G_TYPE_CHECK_INSTANCE_CAST ((object), BUTTON_TYPE_WIDGET, ButtonWidget))
#define BUTTON_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass, BUTTON_TYPE_WIDGET, ButtonWidgetClass))
#define BUTTON_IS_WIDGET(object)    	(G_TYPE_CHECK_INSTANCE_TYPE ((object), BUTTON_TYPE_WIDGET)) 
#define BUTTON_IS_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), BUTTON_TYPE_WIDGET))

typedef struct _ButtonWidget		ButtonWidget;
typedef struct _ButtonWidgetClass	ButtonWidgetClass;
typedef struct _ButtonWidgetPrivate	ButtonWidgetPrivate;

struct _ButtonWidget {
	GtkButton            parent;

	ButtonWidgetPrivate *priv;
};

struct _ButtonWidgetClass {
	GtkButtonClass parent_class;
};

GType            button_widget_get_type          (void) G_GNUC_CONST;
void             button_widget_set_activatable   (ButtonWidget     *button,
						  gboolean          activatable);
void             button_widget_set_icon_name     (ButtonWidget     *button,
						  const char       *icon_name);
void             button_widget_set_orientation   (ButtonWidget     *button,
						  PanelOrientation  orientation);
void             button_widget_set_dnd_highlight (ButtonWidget     *button,
						  gboolean          dnd_highlight);
GtkIconTheme    *button_widget_get_icon_theme    (ButtonWidget     *button);

G_END_DECLS

#endif /* __BUTTON_WIDGET_H__ */
