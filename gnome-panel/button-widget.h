#ifndef BUTTON_WIDGET_H
#define BUTTON_WIDGET_H

#include <gtk/gtk.h>
#include "panel-types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
	
#define BUTTON_TYPE_WIDGET		(button_widget_get_type ())
#define BUTTON_WIDGET(object)          	(G_TYPE_CHECK_INSTANCE_CAST ((object), BUTTON_TYPE_WIDGET, ButtonWidget))
#define BUTTON_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass, BUTTON_TYPE_WIDGET, ButtonWidgetClass))
#define BUTTON_IS_WIDGET(object)    	(G_TYPE_CHECK_INSTANCE_TYPE ((object), BUTTON_TYPE_WIDGET)) 
#define BUTTON_IS_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), BUTTON_TYPE_WIDGET))

typedef struct _ButtonWidget		ButtonWidget;
typedef struct _ButtonWidgetClass	ButtonWidgetClass;

struct _ButtonWidget
{
	GtkButton		parent;
	
	GdkPixbuf		*pixbuf;
  
	GdkPixbuf		*scaled;
	GdkPixbuf		*scaled_hc;

	char			*filename;
	int			size;
	
	char			*text;
	
	guint			ignore_leave:1; /*ignore the leave notify,
						  if you do this remember to
						  set the in_button properly
						  later!*/

	guint			arrow:1; /*0 no arrow, 1 simple arrow, more
					   to do*/
	guint			dnd_highlight:1;

	PanelOrient		orient;

	guint			pressed_timeout;
};

struct _ButtonWidgetClass
{
	GtkButtonClass parent_class;

	void (* push_move) (ButtonWidget	*button,
                            GtkDirectionType	 dir);
	void (* switch_move) (ButtonWidget	*button,
                              GtkDirectionType	 dir);
	void (* free_move) (ButtonWidget	*button,
                            GtkDirectionType	 dir);
};

GType		button_widget_get_type		(void) G_GNUC_CONST;

GtkWidget*	button_widget_new		(const char *pixmap,
						 int size,
						 gboolean arrow,
						 PanelOrient orient,
						 const char *text);

gboolean	button_widget_set_pixmap	(ButtonWidget *button,
						 const char *pixmap,
						 int size);

void		button_widget_set_text		(ButtonWidget *button,
						 const char *text);

void		button_widget_set_params	(ButtonWidget *button,
						 gboolean arrow,
						 PanelOrient orient);

void		button_widget_set_dnd_highlight	(ButtonWidget *button,
						 gboolean highlight);
#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __BUTTON_WIDGET_H__ */
