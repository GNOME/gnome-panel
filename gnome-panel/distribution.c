/*
 * GNOME panel linux distribution module.
 * (C) 2000 The Free Software Foundation
 *
 * Authors: Martin Baulig <baulig@suse.de>
 */

#include <config.h>

#include <libgnome/libgnome.h>

#include "distribution.h"
#include "menu.h"
#include "menu-fentry.h"

/* Note for distribution vendors:
 *
 * This file - especially the `distribution_info' array - is
 * the only place which you need to custumize in order to make
 * the `Distribution' menu in the panel working.
 *
 */

static DistributionInfo distribution_info [] = {
	{ DISTRIBUTION_DEBIAN, "/etc/debian_version",
	  N_("Debian GNU/Linux"), N_("Debian menus"),
	  "gnome-debian.png", "/var/lib/gnome/Debian/.",
	  NULL, NULL
	},
	{ DISTRIBUTION_SUSE, "/etc/SuSE-release",
	  N_("SuSE Linux"), N_("SuSE menus"), "gnome-suse.png",
	  "gnome/distribution-menus/SuSE/.",
	  NULL, NULL
	},
	{ DISTRIBUTION_SOLARIS, "/var/sadm/pkg/SUNWdtcor",
	 N_("Solaris"), N_("CDE Menus"), "gnome-gmenu.png", "cdemenu:/",
	 NULL, NULL
	},
	{ DISTRIBUTION_UNKNOWN, NULL, NULL, NULL, NULL }
};

static DistributionType
internal_get_distribution_type (void)
{
	DistributionInfo *ptr;

	for (ptr = distribution_info; ptr->type != DISTRIBUTION_UNKNOWN; ptr++)
		if (g_file_test (ptr->version_file, G_FILE_TEST_EXISTS))
			return ptr->type;

	return DISTRIBUTION_UNKNOWN;
}

static const DistributionInfo *
internal_get_distribution_info (DistributionType type)
{
	DistributionInfo *ptr;

	for (ptr = distribution_info; ptr->type != DISTRIBUTION_UNKNOWN; ptr++)
		if (ptr->type == type)
			return ptr;

	return NULL;
}

/* note that this function is marked G_GNUC_CONST in distribution.h */
DistributionType
get_distribution_type (void)
{
	static gboolean cached = FALSE;
	static DistributionType cache = DISTRIBUTION_UNKNOWN;

	if (cached) {
		return cache;
	}

	cache = internal_get_distribution_type ();
	cached = TRUE;

	return cache;
}

/* note that this function is marked G_GNUC_CONST in distribution.h */
const DistributionInfo *
get_distribution_info (void)
{
	static gboolean cached = FALSE;
	static const DistributionInfo *cache = NULL;
	DistributionType type;

	if (cached) {
		return cache;
	}

	type = get_distribution_type ();

	cache = internal_get_distribution_info (type);
	cached = TRUE;

	if (cache && cache->menu_path && !g_path_is_absolute (cache->menu_path)) {
		char *full_path;

		full_path = gnome_program_locate_file (
				NULL, GNOME_FILE_DOMAIN_DATADIR, cache->menu_path, TRUE, NULL);
		if (full_path)
			cache->menu_path = full_path;
	}

	return cache;
}
