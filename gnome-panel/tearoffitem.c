#include "config.h"
#include <gnome.h>

#include "panel-include.h"
#include "tearoffitem.h"

static void tearoff_item_class_init	(TearoffItemClass	*klass);
static void tearoff_item_init		(TearoffItem	*tearoff_menu_item);
static void tearoff_item_activate	(GtkMenuItem	*menu_item);

GtkType
tearoff_item_get_type(void)
{
	static GtkType tearoff_menu_item_type = 0;

	if (!tearoff_menu_item_type) {
		static const GtkTypeInfo tearoff_menu_item_info = {
			"TearoffItem",
			sizeof (TearoffItem),
			sizeof (TearoffItemClass),
			(GtkClassInitFunc) tearoff_item_class_init,
			(GtkObjectInitFunc) tearoff_item_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		tearoff_menu_item_type =
			gtk_type_unique(gtk_tearoff_menu_item_get_type(),
					&tearoff_menu_item_info);
	}

	return tearoff_menu_item_type;
}

GtkWidget*
tearoff_item_new(void)
{
	return GTK_WIDGET(gtk_type_new(tearoff_item_get_type()));
}

static void
tearoff_item_class_init(TearoffItemClass *klass)
{
	GtkMenuItemClass *menu_item_class;

	menu_item_class = (GtkMenuItemClass*) klass;

	menu_item_class->activate = tearoff_item_activate;
}

static void
tearoff_item_init(TearoffItem *tearoff_menu_item)
{
}

static void
tearoff_item_activate(GtkMenuItem *menu_item)
{
	TearoffItem *tearoff_item;
	GtkTearoffMenuItem *tearoff_menu_item;

	g_return_if_fail (menu_item != NULL);
	g_return_if_fail (IS_TEAROFF_ITEM (menu_item));
	
	tearoff_menu_item = GTK_TEAROFF_MENU_ITEM (menu_item);
	tearoff_menu_item->torn_off = !tearoff_menu_item->torn_off;

	/*if (GTK_IS_MENU (GTK_WIDGET (menu_item)->parent)) {
		GtkMenu *menu = GTK_MENU (GTK_WIDGET (menu_item)->parent);
		gboolean need_connect;

		need_connect = (tearoff_menu_item->torn_off && !menu->tearoff_window);

		gtk_menu_set_tearoff_state (GTK_MENU (GTK_WIDGET (menu_item)->parent),
					    tearoff_menu_item->torn_off);

		if (need_connect)
			gtk_signal_connect_object (GTK_OBJECT (menu->tearoff_window),  
						   "delete_event",
						   GTK_SIGNAL_FUNC (tearoff_item_delete_cb),
						   GTK_OBJECT (menu_item));
	}*/

	gtk_widget_queue_resize (GTK_WIDGET (menu_item));
}

