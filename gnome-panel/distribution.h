#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include <panel-widget.h>
#include "applet.h"

BEGIN_GNOME_DECLS

typedef enum {
	DISTRIBUTION_UNKNOWN = 0,
	DISTRIBUTION_DEBIAN,
	DISTRIBUTION_MANDRAKE,
	DISTRIBUTION_SUSE,
	DISTRIBUTION_REDHAT
} DistributionType;

typedef struct {
	DistributionType type;
	const gchar *version_file;
	const gchar *distribution_name;
	const gchar *menu_name;
	const gchar *menu_icon;
	const gchar *menu_path;
	void (*menu_init_func) (void);
	void (*menu_show_func) (GtkWidget *, GtkMenuItem *);
} DistributionInfo;

/* Distribution description array. */
extern DistributionInfo distribution_info [];

/* Get distribution type. */
DistributionType get_distribution (void);

/* Get info about a distribution or NULL. */
const DistributionInfo *get_distribution_info (DistributionType type);

END_GNOME_DECLS

#endif
