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
	GtkWidget		pixmap;
	
	GdkPixbuf		*pixbuf;
	GdkPixbuf		*pixbuf_hc;

	GdkPixmap		*cache;
	
	char			*filename;
	int			size;
	
	guint			pobject:2; /* Not quite sure if we need this right now */
	
	GdkWindow               *event_window;
	
	char			*text;
	
	guint			pressed:1; /*true if the button is pressed*/
	guint			in_button:1;

	guint			ignore_leave:1; /*ignore the leave notify,
						  if you do this remember to
						  set the in_button properly
						  later!*/
	
	guint			arrow:1; /*0 no arrow, 1 simple arrow, more
					   to do*/
	guint			dnd_highlight:1;

	guint			no_alpha:1; /*we don't have any alpha to speak
					      of, so we don't have to dump the
					      cache all the time*/
	
	PanelOrient		orient;

	guint			pressed_timeout;
};

struct _ButtonWidgetClass
{
	GtkWidgetClass parent_class;

	void (* clicked) (ButtonWidget *button);
	void (* pressed) (ButtonWidget *button);
	void (* unpressed) (ButtonWidget *button);
};

GType		button_widget_get_type		(void) G_GNUC_CONST;

GtkWidget*	button_widget_new		(const char *pixmap,
						 int size,
						 int pobject,
						 gboolean arrow,
						 PanelOrient orient,
						 const char *text);

void		button_widget_draw		(ButtonWidget *button,
						 guchar *rgb,
						 int rowstride);
/* draw the xlib part (arrow/text) */
void		button_widget_draw_xlib		(ButtonWidget *button,
						 GdkPixmap *pixmap);

gboolean	button_widget_set_pixmap	(ButtonWidget *button,
						 const char *pixmap,
						 int size);

void		button_widget_set_text		(ButtonWidget *button,
						 const char *text);

void		button_widget_set_params	(ButtonWidget *button,
						 int pobject,
						 gboolean arrow,
						 PanelOrient orient);

void		button_widget_set_dnd_highlight	(ButtonWidget *button,
						 gboolean highlight);

void		button_widget_clicked		(ButtonWidget *button);
void		button_widget_down		(ButtonWidget *button);
void		button_widget_up		(ButtonWidget *button);

void		button_widget_redo_all		(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __BUTTON_WIDGET_H__ */
