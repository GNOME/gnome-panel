#ifndef TEAROFF_ITEM_H
#define TEAROFF_ITEM_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define TEAROFF_TYPE_ITEM			(tearoff_item_get_type())
#define TEAROFF_ITEM(object)			(G_TYPE_CHECK_INSTANCE_CAST ((object), TEAROFF_TYPE_ITEM, TearoffItem))
#define TEAROFF_ITEM_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), TEAROFF_TYPE_ITEM, TearoffItemClass))
#define TEAROFF_IS_ITEM(object)			(G_TYPE_CHECK_INSTANCE_TYPE ((object), TEAROFF_TYPE_ITEM))
#define TEAROFF_IS_ITEM_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((klass), TEAROFF_TYPE_ITEM))


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


GType		tearoff_item_get_type	(void);

GtkWidget*	tearoff_item_new	(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* TEAROFF_ITEM_H */
