#ifndef __BUTTON_WIDGET_H__
#define __BUTTON_WIDGET_H__

#include <gtk/gtk.h>
#include <gnome.h>
#include "panel-types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
	
#define MAX_TILES 4

#define BUTTON_WIDGET(obj)          GTK_CHECK_CAST (obj, button_widget_get_type (), ButtonWidget)
#define BUTTON_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, button_widget_get_type (), ButtonWidgetClass)
#define IS_BUTTON_WIDGET(obj)       GTK_CHECK_TYPE (obj, button_widget_get_type ())

typedef struct _ButtonWidget		ButtonWidget;
typedef struct _ButtonWidgetClass	ButtonWidgetClass;

struct _ButtonWidget
{
	GtkWidget		_pixmap;
	
	GdkPixmap		*pixmap; /*this is the one we start from*/
	GdkBitmap		*mask;
	
	int			pressed; /*true if the button is pressed*/
	
	int			tile; /*the tile number, only used if tiles are on*/
	int			arrow; /*0 no arrow, 1 simple arrow, more to do*/
	PanelOrientType		orient;
};

struct _ButtonWidgetClass
{
	GtkWidgetClass parent_class;

	void (* clicked) (ButtonWidget *button);
	void (* pressed) (ButtonWidget *button);
	void (* unpressed) (ButtonWidget *button);
};

guint		button_widget_get_type		(void);

GtkWidget*	button_widget_new		(GdkPixmap *pixmap,
						 GdkBitmap *mask,
						 int tile,
						 int arrow,
						 PanelOrientType orient);
GtkWidget*	button_widget_new_from_file	(char *pixmap,
						 int tile,
						 int arrow,
						 PanelOrientType orient);

void		button_widget_draw		(ButtonWidget *button,
						 GdkPixmap *pixmap);

void		button_widget_set_pixmap	(ButtonWidget *button,
						 GdkPixmap *pixmap,
						 GdkBitmap *mask);
int		button_widget_set_pixmap_from_file(ButtonWidget *button,
						   char *pixmap);

void		button_widget_set_params	(ButtonWidget *button,
						 int tile,
						 int arrow,
						 PanelOrientType orient);

void		button_widget_clicked		(ButtonWidget *button);
void		button_widget_down		(ButtonWidget *button);
void		button_widget_up		(ButtonWidget *button);

void		button_widget_load_tile		(int tile,
						 char *tile_up,
						 char *tile_down);

void		button_widget_tile_enamble	(int enabled);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __BUTTON_WIDGET_H__ */
