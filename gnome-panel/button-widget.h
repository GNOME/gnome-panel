#ifndef BUTTON_WIDGET_H
#define BUTTON_WIDGET_H

#include <gtk/gtk.h>
#include <gnome.h>
#include "panel-types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
	
#define TYPE_BUTTON_WIDGET	    (button_widget_get_type ())
#define BUTTON_WIDGET(obj)          GTK_CHECK_CAST (obj, button_widget_get_type (), ButtonWidget)
#define BUTTON_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, button_widget_get_type (), ButtonWidgetClass)
#define IS_BUTTON_WIDGET(obj)       GTK_CHECK_TYPE (obj, button_widget_get_type ())

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
	
	GdkWindow               *event_window;
	
	char			*text;
	
	guint			pressed:1; /*true if the button is pressed*/
	guint			in_button:1;

	guint			ignore_leave:1; /*ignore the leave notify,
						  if you do this remember to
						  set the in_button properly
						  later!*/
	
	guint			tile:2; /*the tile number, only used if tiles
					  are on*/
	guint			arrow:1; /*0 no arrow, 1 simple arrow, more
					   to do*/
	guint			dnd_highlight:1;

	guint			no_alpha:1; /*we don't have any alpha to speak
					      of, so we don't have to dump the
					      cache all the time*/
	
	PanelOrientType		orient;

	guint			pressed_timeout;
};

struct _ButtonWidgetClass
{
	GtkWidgetClass parent_class;

	void (* clicked) (ButtonWidget *button);
	void (* pressed) (ButtonWidget *button);
	void (* unpressed) (ButtonWidget *button);
};

guint		button_widget_get_type		(void) G_GNUC_CONST;

GtkWidget*	button_widget_new		(const char *pixmap,
						 int size,
						 int tile,
						 gboolean arrow,
						 PanelOrientType orient,
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
						 int tile,
						 gboolean arrow,
						 PanelOrientType orient);

void		button_widget_set_dnd_highlight	(ButtonWidget *button,
						 gboolean highlight);

void		button_widget_clicked		(ButtonWidget *button);
void		button_widget_down		(ButtonWidget *button);
void		button_widget_up		(ButtonWidget *button);

/*load a tile of a given type/class, note that depth applies to a class
  wheather or not there is a tile or not, so this is basically a class
  initialization function*/
void		button_widget_load_tile		(int tile,
						 const char *tile_up,
						 const char *tile_down,
						 int border,
						 int depth);

void		button_widget_set_flags		(int tile,
						 gboolean tiles_enabled,
						 gboolean pixmaps_enabled,
						 gboolean always_text);

void		button_widget_redo_all		(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __BUTTON_WIDGET_H__ */
