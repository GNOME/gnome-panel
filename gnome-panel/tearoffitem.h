#ifndef TEAROFF_ITEM_H
#define TEAROFF_ITEM_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define TYPE_TEAROFF_ITEM	(tearoff_item_get_type())
#define TEAROFF_ITEM(obj)	(GTK_CHECK_CAST ((obj), tearoff_item_get_type(), TearoffItem))
#define TEAROFF_ITEM_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), tearoff_item_get_type(), TearoffItemClass))
#define IS_TEAROFF_ITEM(obj)	(GTK_CHECK_TYPE ((obj), tearoff_item_get_type()))
#define IS_TEAROFF_ITEM_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), tearoff_item_get_type()))


typedef struct _TearoffItem       TearoffItem;
typedef struct _TearoffItemClass  TearoffItemClass;

struct _TearoffItem
{
	GtkTearoffMenuItem tearoff_menu_item;
};

struct _TearoffItemClass
{
	GtkTearoffMenuItemClass parent_class;
};


GtkType		tearoff_item_get_type	(void);
GtkWidget*	tearoff_item_new	(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* TEAROFF_ITEM_H */
