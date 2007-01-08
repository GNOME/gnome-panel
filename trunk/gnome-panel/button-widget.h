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

struct _ButtonWidget {
	GtkButton         parent;
	
	GtkIconTheme     *icon_theme;
	GdkPixbuf        *pixbuf;
	GdkPixbuf        *pixbuf_hc;

	char             *filename;

	PanelOrientation  orientation;

	int               size;

	GtkWidget        *no_icon_dialog;

	guint             activatable   : 1;
	guint             ignore_leave  : 1;
	guint             arrow         : 1;
	guint             dnd_highlight : 1;
};

struct _ButtonWidgetClass {
	GtkButtonClass parent_class;
};

GType            button_widget_get_type          (void) G_GNUC_CONST;
GtkWidget *      button_widget_new               (const char       *pixmap,
						  gboolean          arrow,
						  PanelOrientation  orientation);
void             button_widget_set_activatable   (ButtonWidget     *button,
						  gboolean          activatable);
gboolean         button_widget_get_activatable   (ButtonWidget     *button);
void             button_widget_set_icon_name     (ButtonWidget     *button,
						  const char       *icon_name);
const char *     button_widget_get_icon_name     (ButtonWidget     *button);
void             button_widget_set_orientation   (ButtonWidget     *button,
						  PanelOrientation  orientation);
PanelOrientation button_widget_get_orientation   (ButtonWidget     *button);
void             button_widget_set_has_arrow     (ButtonWidget     *button,
						  gboolean          has_arrow);
gboolean         button_widget_get_has_arrow     (ButtonWidget     *button);
void             button_widget_set_dnd_highlight (ButtonWidget     *button,
						  gboolean          dnd_highlight);
gboolean         button_widget_get_dnd_highlight (ButtonWidget     *button);

G_END_DECLS

#endif /* __BUTTON_WIDGET_H__ */
