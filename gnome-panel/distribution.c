/*
 * GNOME panel linux distribution module.
 * (C) 2000 The Free Software Foundation
 *
 * Authors: Martin Baulig <baulig@suse.de>
 */

#include <config.h>
#include <gnome.h>

#include "panel-include.h"

/* Note for distribution vendors:
 *
 * This file - especially the `distribution_info' array - is
 * the only place which you need to custumize in order to make
 * the `Distribution' menu in the panel working.
 *
 */

static void rh_menu_init_func (void);
static void rh_menu_show_func (GtkWidget *, GtkMenuItem *);

DistributionInfo distribution_info [] = {
	{ DISTRIBUTION_DEBIAN, "/etc/debian_version",
	  N_("Debian GNU/Linux"), N_("Debian menus"),
	  "gnome-debian.png", "/var/lib/gnome/Debian/.",
	  NULL, NULL
	},
	{ DISTRIBUTION_REDHAT, "/etc/redhat-release",
	  N_("Red Hat Linux"), N_("Red Hat menus"), NULL,
	  "apps-redhat",
	  rh_menu_init_func, rh_menu_show_func
	},
	{ DISTRIBUTION_SUSE, "/etc/SuSE-release",
	  N_("SuSE Linux"), N_("SuSE menus"), "gnome-suse.png",
	  GNOME_DATADIR "/gnome/distribution-menus/SuSE/.",
	  NULL, NULL
	},
	{ DISTRIBUTION_UNKNOWN, NULL, NULL, NULL, NULL }
};

DistributionType
get_distribution (void)
{
	DistributionInfo *ptr;

	for (ptr = distribution_info; ptr->type != DISTRIBUTION_UNKNOWN; ptr++)
		if (g_file_exists (ptr->version_file))
			return ptr->type;

	return DISTRIBUTION_UNKNOWN;
}

const DistributionInfo *
get_distribution_info (DistributionType type)
{
	DistributionInfo *ptr;

	for (ptr = distribution_info; ptr->type != DISTRIBUTION_UNKNOWN; ptr++)
		if (ptr->type == type)
			return ptr;

	return NULL;
}

/*
 * Distribution specific menu functions.
 */

static void
rh_menu_init_func (void)
{
	/* Use the fork version to create the Red Hat menus. */
	create_rh_menu (TRUE);
}

static void
rh_menu_show_func (GtkWidget *menuw, GtkMenuItem *menuitem)
{
	rh_submenu_to_display (menuw, menuitem);
}

