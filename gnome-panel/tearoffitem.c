#include "config.h"

#include <libgnome/libgnome.h>
#include <gconf/gconf-client.h>

#include "menu.h"
#include "tearoffitem.h"

static void tearoff_item_class_init	(TearoffItemClass	*klass);
static void tearoff_item_instance_init	(TearoffItem	*tearoff_menu_item);

GType
tearoff_item_get_type (void)
{
	static GType object_type = 0;

	if (object_type == 0) {
		static const GTypeInfo object_info = {
                    sizeof (TearoffItemClass),
                    (GBaseInitFunc)         NULL,
                    (GBaseFinalizeFunc)     NULL,
                    (GClassInitFunc)        tearoff_item_class_init,
                    NULL,                   /* class_finalize */
                    NULL,                   /* class_data */
                    sizeof (TearoffItem),
                    0,                      /* n_preallocs */
                    (GInstanceInitFunc)     tearoff_item_instance_init
		};

		object_type = g_type_register_static (GTK_TYPE_TEAROFF_MENU_ITEM, "TearoffItem", &object_info, 0);
	}

	return object_type;
}

GtkWidget*
tearoff_item_new (void)
{
	g_return_val_if_fail (panel_menu_have_tearoff (), NULL);

	return GTK_WIDGET (g_object_new (tearoff_item_get_type (), NULL));
}

static void
tearoff_item_class_init(TearoffItemClass *klass)
{
	GtkMenuItemClass *menu_item_class;

	menu_item_class = (GtkMenuItemClass*) klass;

	menu_item_class->activate = NULL;
}

static void
tearoff_item_instance_init(TearoffItem *tearoff_menu_item)
{
	/* Empty */
}
