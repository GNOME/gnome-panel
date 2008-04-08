#ifndef __CLOCK_ZONETABLE_H__
#define __CLOCK_ZONETABLE_H__

#include <glib.h>
#include <glib-object.h>

#include "clock-country.h"
#include "clock-zoneinfo.h"

G_BEGIN_DECLS

#define CLOCK_ZONETABLE_TYPE         (clock_zonetable_get_type ())
#define CLOCK_ZONETABLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CLOCK_ZONETABLE_TYPE, ClockZoneTable))
#define CLOCK_ZONETABLE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), CLOCK_ZONETABLE_TYPE, ClockZoneTableClass))
#define IS_CLOCK_ZONETABLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CLOCK_ZONETABLE_TYPE))
#define IS_CLOCK_ZONETABLE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), CLOCK_ZONETABLE_TYPE))
#define CLOCK_ZONETABLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CLOCK_ZONETABLE_TYPE, ClockZoneTableClass))

typedef struct
{
        GObject g_object;
} ClockZoneTable;

typedef struct
{
        GObjectClass g_object_class;
} ClockZoneTableClass;

GType clock_zonetable_get_type (void);

ClockZoneTable *clock_zonetable_new (void);

GList *clock_zonetable_get_zones             (ClockZoneTable *this);
ClockZoneInfo *clock_zonetable_get_zone      (ClockZoneTable *this,
                                              const gchar    *name);
ClockZoneInfo *clock_zonetable_get_l10n_zone (ClockZoneTable *this,
                                              const gchar    *l10n_name);

ClockCountry *clock_zonetable_get_country    (ClockZoneTable *this, 
                                              const gchar    *code);

G_END_DECLS
#endif /* __CLOCK_ZONETABLE_H__ */
