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

/* Get distribution type. */
DistributionType get_distribution_type (void) G_GNUC_CONST;

/* Get the distribution info, it only checks the first time, it thus won't handle
 * distributions getting changed from under us.  Hmmm ... I think doing that
 * would be really going overboard */
const DistributionInfo *get_distribution_info (void) G_GNUC_CONST;

END_GNOME_DECLS

#endif
