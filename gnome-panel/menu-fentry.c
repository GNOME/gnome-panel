/*
 * GNOME panel menu module.
 * (C) 1997, 1998, 1999, 2000 The Free Software Foundation
 * Copyright 2000 Eazel, Inc.
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <libgnome/libgnome.h>

#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-directory.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "menu-fentry.h"
#include "quick-desktop-reader.h"

#include "panel-util.h"
#include "panel_config_global.h"
#include "session.h"

#undef MENU_FENTRY_DEBUG

/* the minimum number of seconds between stats of files */
#define STAT_EVERY 3

static GSList *dir_list = NULL;

extern GlobalConfig global_config;

static GSList *
prepend_mfile (GSList *list,
	       const char *name,
	       gboolean verified,
	       time_t mtime,
	       gboolean is_dir)
{
	MFile *mfile = g_new0 (MFile, 1);

	mfile->name = g_strdup (name);
	mfile->verified = verified;
	mfile->mtime = mtime;
	mfile->is_dir = is_dir;

	return g_slist_prepend (list, mfile);
}

static MFile *
find_mfile (GSList *list, const char *string)
{
	GSList *li;

	g_return_val_if_fail (string != NULL, NULL);

	for (li = list; li != NULL; li = li->next) {
		MFile *mfile = li->data;

		if (mfile->name != NULL &&
		    strcmp (mfile->name, string) == 0)
			return mfile;
	}

	return NULL;
}


/*reads in the sort order and makes a list*/
static GSList *
get_presorted_from (const char *dir_uri)
{
	char *uri, *sort_order, *p;
	GSList *list = NULL;
	QuickDesktopItem *qitem;
	
	uri = g_build_path ("/", dir_uri, ".directory", NULL);
	qitem = quick_desktop_item_load_uri (uri,
					     NULL /* expected_type */,
					     FALSE /* run_tryexec */);

	g_free (uri);

	if (qitem == NULL)
		return NULL;

	/* steal sort_order, we're gonna mangle it */
	sort_order = qitem->sort_order;
	qitem->sort_order = NULL;

	quick_desktop_item_destroy (qitem);

	if (sort_order == NULL)
		return NULL;

	p = strtok (sort_order, ";");
	while (p != NULL) {
		list = prepend_mfile (list,
				      p /* name */,
				      FALSE /* verified */,
				      0 /* mtime */,
				      /* We don't know if it's
				       * a directory */
				      FALSE /* is_dir */);

		p = strtok (NULL, ";");
	}

	g_free (sort_order);

	return list;
}

static GSList *
read_directory (GSList *list, const char *menuuri)
{
	GnomeVFSDirectoryHandle *handle = NULL;

	if (gnome_vfs_directory_open (&handle, menuuri,
				      GNOME_VFS_FILE_INFO_DEFAULT)
	    == GNOME_VFS_OK) {
		GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();
		while (gnome_vfs_directory_read_next (handle, info)
		       == GNOME_VFS_OK) {
			MFile *mfile;

			if (info->name[0] == '.') {
				gnome_vfs_file_info_clear (info);
				continue;
			}

			mfile = find_mfile (list, info->name);

			/* FIXME: what if there is no mtime in the info? */

			if (mfile == NULL) {
				list = prepend_mfile (list,
						      info->name,
						      TRUE /* verified */,
						      info->mtime,
						      (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY));
			/* if we've already found it in the presorted,
			 * just verify it */
			} else {
				mfile->verified = TRUE;
				mfile->mtime = info->mtime;
				mfile->is_dir = (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY);
			}

			gnome_vfs_file_info_clear (info);
		}

		gnome_vfs_file_info_unref (info);
		gnome_vfs_directory_close (handle);
	}

	return list;
}

void
free_mfile (MFile *mfile)
{
	if (mfile != NULL) {
		g_free (mfile->name);
		mfile->name = NULL;

		g_free (mfile);
	}
}

void
free_mfile_list (GSList *list)
{
	GSList *li;
	for (li = list; li != NULL; li = li->next) {
		free_mfile (li->data);
		li->data = NULL;
	}
	g_slist_free (list);
}

GSList *
get_mfiles_from_menudir (const char *menuuri)
{
	GSList *list = NULL;

	list = get_presorted_from (menuuri);
	list = read_directory (list, menuuri);

	return g_slist_reverse (list);
}

static char *
get_applet_goad_id_from_ditem(QuickDesktopItem *ii)
{
#ifdef MENU_FENTRY_DEBUG
  printf ("GET THAT GOAD OUT OF HERE...menu-fentry.c get_applet_gaod_id_from_ditem\n");
#endif
  return NULL;
  /*
	int i;
	int constantlen = strlen ("--activate-goad-server");

	g_return_val_if_fail (ii != NULL, NULL);

	if (ii->exec == NULL ||
	    ii->type == NULL)
		return NULL;
	
	if (strcmp (ii->type, "PanelApplet") == 0) {
		return g_strjoinv (" ", ii->exec);
	} else {
		if (ii->exec[0] == NULL)
			return NULL;
		//this is here as a horrible hack since that's the way it
		//  used to work, but now one should make the .desktop type
		//  PanelApplet
		for(i=1;ii->exec[i];i++) {
			if(strncmp("--activate-goad-server",
				   ii->exec[i],constantlen)==0) {
				if(strlen(ii->exec[i])>constantlen)
					return g_strdup(&ii->exec[i][constantlen+1]);
				else
					return g_strdup(ii->exec[i+1]);
			}
		}
	}
	return NULL;
*/
}

static void
fr_free (FileRec *fr, gboolean free_fr)
{
	if (fr == NULL)
		return;

	g_free (fr->name);
	fr->name = NULL;
	g_free (fr->fullname);
	fr->fullname = NULL;
	g_free (fr->comment);
	fr->comment = NULL;
	g_free (fr->icon);
	fr->icon = NULL;
	g_free (fr->goad_id);
	fr->goad_id = NULL;
	g_free (fr->tryexec_path);
	fr->tryexec_path = NULL;

	if (fr->parent != NULL &&
	    free_fr)
		fr->parent->recs = g_slist_remove (fr->parent->recs, fr);
	fr->parent = NULL;

	if (fr->type == FILE_REC_DIR) {
		DirRec *dr = (DirRec *)fr;
		GSList *li;

		for (li = dr->mfl; li != NULL; li = li->next) {
			MenuFinfo *mf = li->data;
			li->data = NULL;
			mf->fr = NULL;
		}
		g_slist_free (dr->mfl);
		dr->mfl = NULL;

		for (li = dr->recs; li != NULL; li = li->next) {
			FileRec *ffr = li->data;
			li->data = NULL;
			ffr->parent = NULL;
			fr_free (ffr, TRUE);
		}
		g_slist_free (dr->recs);
		dr->recs = NULL;

		if (dr->tryexecs != NULL) {
			g_slist_foreach (dr->tryexecs, (GFunc) g_free, NULL);
			g_slist_free (dr->tryexecs);
			dr->tryexecs = NULL;
		}
		dir_list = g_slist_remove (dir_list, fr);
	}

	if (free_fr) {
		g_free (fr);
	} else  {
		int type = fr->type;
		if (fr->type == FILE_REC_DIR)
			memset (fr, 0, sizeof(DirRec));
		else
			memset (fr, 0, sizeof(FileRec));
		/* we must reset the type so that we don't crash
		 * if we call fr_free on this again */
		fr->type = type;
	}
}

static void
fr_fill_dir (FileRec *fr, int sublevels)
{
	GSList *flist;
	DirRec *dr = (DirRec *)fr;
	time_t curtime = time (NULL);
	
	g_return_if_fail (dr->recs == NULL);
	g_return_if_fail (fr != NULL);
	g_return_if_fail (fr->name != NULL);

	flist = get_mfiles_from_menudir (fr->name);
	while (flist != NULL) {
		MFile *mfile = flist->data;
		char *name;
		GSList *tmp = flist;
		flist = flist->next;
		g_slist_free_1 (tmp);

		if ( ! mfile->verified) {
			free_mfile (mfile);
			continue;
		}

		g_print ("fr->name = \"%s\" , mfile->name = \"%s\"\n",
			 fr->name, mfile->name);

		name = g_build_path ("/", fr->name, mfile->name, NULL);

		if (mfile->is_dir) {
			FileRec *ffr = fr_read_dir (NULL, name, mfile->mtime,
						     sublevels - 1);
			if (ffr != NULL) {
				dr->recs = g_slist_prepend (dr->recs, ffr);
			}
		} else {
			QuickDesktopItem *qitem;
			char *tryexec_path;
			if ( ! is_ext2 (mfile->name, ".desktop", ".kdelnk")) {
				g_free (name);
				free_mfile (mfile);
				g_print ("BAD EXTENSION\n");
				continue;
			}

			tryexec_path = NULL;

			qitem = quick_desktop_item_load_uri (name /* uri */,
							     NULL /* expected_type */,
							     FALSE /* run_tryexec */);
			if (qitem != NULL &&
			    qitem->tryexec != NULL) {
				tryexec_path = g_find_program_in_path (qitem->tryexec);
				if (tryexec_path == NULL) {
					dr->tryexecs = g_slist_prepend (dr->tryexecs,
									g_strdup (qitem->tryexec));
					quick_desktop_item_destroy (qitem);
					qitem = NULL;
				}
			}

			if (qitem != NULL) {
				FileRec *ffr = g_new0 (FileRec, 1);
				if (qitem->type != NULL &&
				    g_ascii_strcasecmp (qitem->type, "separator") == 0)
					ffr->type = FILE_REC_SEP;
				else
					ffr->type = FILE_REC_FILE;
				ffr->name = name;
				name = NULL;
				ffr->mtime = mfile->mtime;
				ffr->last_stat = curtime;
				ffr->parent = dr;
				ffr->icon = qitem->icon;
				qitem->icon = NULL;
				ffr->fullname = qitem->name;
				qitem->name = NULL;
				ffr->comment = qitem->comment;
				qitem->comment = NULL;
				ffr->tryexec_path = tryexec_path;

				/* FIXME: we don't need this shit I don't think */
				ffr->goad_id =
					get_applet_goad_id_from_ditem (qitem);

				quick_desktop_item_destroy (qitem);

				dr->recs = g_slist_prepend (dr->recs, ffr);
			} else {
				g_print ("NO QITEM\n");
			}
		}
		g_free (name);
		free_mfile (mfile);
	}
	dr->recs = g_slist_reverse (dr->recs);
}

FileRec *
fr_read_dir (DirRec *dr, const char *muri, time_t mtime, int sublevels)
{
	char *furi;
	FileRec *fr;
	time_t curtime = time (NULL);
	GnomeVFSFileInfo *info;
	
	g_return_val_if_fail (muri != NULL, NULL);

	g_print ("fr_read_dir (..., \"%s\", %ld, %d)\n",
		 muri, (long)mtime, sublevels);

	info = gnome_vfs_file_info_new ();

	/*this will zero all fields*/
	if (dr == NULL) {
		dr = g_new0 (DirRec, 1);
		dr->force_reread = FALSE;
		/* this must be set otherwise we may messup on
		   fr_free */
		dr->frec.type = FILE_REC_DIR;
	}
	fr = (FileRec *)dr;

	if (fr->last_stat < curtime-STAT_EVERY) {
		if (mtime <= 0) {
			if (gnome_vfs_get_file_info
			    (muri, info, GNOME_VFS_FILE_INFO_DEFAULT)
			    != GNOME_VFS_OK) {
				gnome_vfs_file_info_unref (info);
				fr_free (fr, TRUE);
				return NULL;
			}

			/* FIXME: what if there is no mtime in the info? */
			fr->mtime = info->mtime;
		} else {
			fr->mtime = mtime;
		}

		fr->last_stat = curtime;
	}

	fr->type = FILE_REC_DIR;
	g_free (fr->name);
	fr->name = g_strdup (muri);

	gnome_vfs_file_info_clear (info);

	/* FIXME: we've already read this when reading the sort_order,
	 * so damnit we should just use that copy, oh well */
	furi = g_build_path ("/", muri, ".directory", NULL);
	if (dr->ditemlast_stat >= curtime-STAT_EVERY ||
	    gnome_vfs_get_file_info (furi, info,
				     GNOME_VFS_FILE_INFO_DEFAULT)
	    == GNOME_VFS_OK) {
		QuickDesktopItem *qitem;
		qitem = quick_desktop_item_load_uri (furi /* uri */,
						     NULL /* expected_type */,
						     TRUE /* run_tryexec */);
		if (qitem != NULL) {
			g_free (fr->icon);
			fr->icon = g_strdup (qitem->icon);
			g_free (fr->fullname);
			fr->fullname = g_strdup (qitem->name);
			g_free (fr->comment);
			fr->comment = g_strdup (qitem->comment);
			quick_desktop_item_destroy (qitem);
		} else {
			g_free (fr->icon);
			fr->icon = NULL;
			g_free (fr->fullname);
			fr->fullname = NULL;
			g_free (fr->comment);
			fr->comment = NULL;
		}
		/*if we statted*/
		if (info->mtime != 0)
			dr->ditemlast_stat = curtime;
		dr->ditemmtime = info->mtime;
	}
	g_free (furi);
	
	/* add if missing from list of directories */
	if (g_slist_find (dir_list, fr) == NULL)
		dir_list = g_slist_prepend (dir_list, fr);
	
	/*if this is a fake structure, so we don't actually look into
	  the directory*/
	if (sublevels > 0)
		fr_fill_dir (fr, sublevels);

	gnome_vfs_file_info_unref (info);

	return fr;
}


FileRec *
fr_replace (FileRec *fr)
{
	char *name = fr->name;
	DirRec *par = fr->parent;
	
	g_assert (fr->type == FILE_REC_DIR);

	/* null these so they don't get freed */
	fr->parent = NULL;
	fr->name = NULL;

	/* don't free the actual structure */
	fr_free (fr, FALSE);

	/* sanity */
	fr->type = FILE_REC_DIR;

	fr = fr_read_dir ((DirRec *)fr, name, 0, 1);
	if (fr != NULL)
		fr->parent = par;
	g_free (name);

	return fr;
}


FileRec *
fr_check_and_reread (FileRec *fr)
{
	DirRec *dr = (DirRec *)fr;
	FileRec *ret = fr;
	time_t curtime;
	GnomeVFSFileInfo *info;

	g_return_val_if_fail (fr != NULL, fr);
	g_return_val_if_fail (fr->type == FILE_REC_DIR, fr);

	info = gnome_vfs_file_info_new ();

	if (dr->recs == NULL) {
		fr_fill_dir (fr, 1);
	} else {
		gboolean reread = FALSE;
		gboolean any_change = FALSE;
		GSList *li;

		if ( ! global_config.menu_check)
			return ret;

		if (dr->force_reread)
			reread = TRUE;
		dr->force_reread = FALSE;

		/* recheck tryexecs */
		for (li = dr->tryexecs; ! reread && li != NULL; li = li->next) {
			char *tryexec = li->data;
			char *p = g_find_program_in_path  (tryexec);

			if (p != NULL) {
				reread = TRUE;
				g_free (p);
			}
		}

		curtime = time (NULL);

		if ( ! reread &&
		    fr->last_stat < curtime - STAT_EVERY) {
			if (gnome_vfs_get_file_info
			    (fr->name, info, GNOME_VFS_FILE_INFO_DEFAULT)
			    != GNOME_VFS_OK) {
				gnome_vfs_file_info_unref (info);
				fr_free (fr, TRUE);
				return NULL;
			}

			if (info->mtime != fr->mtime)
				reread = TRUE;
		}

		for (li = dr->recs;
		     ! reread && li != NULL;
		     li = li->next) {
			FileRec *ffr = li->data;
			DirRec *ddr;
			GnomeVFSResult result;
			char *p;
			struct stat s;

			switch(ffr->type) {
			case FILE_REC_DIR:
				ddr = (DirRec *)ffr;
				p = g_build_path ("/",
						  ffr->name,
						  ".directory",
						  NULL);
				if (ddr->ditemlast_stat >= curtime-STAT_EVERY) {
					g_free (p);
					break;
				}
				dr->ditemlast_stat = curtime;
				gnome_vfs_file_info_clear (info);
				if (gnome_vfs_get_file_info
				    (p, info, GNOME_VFS_FILE_INFO_DEFAULT)
				    != GNOME_VFS_OK) {
					gnome_vfs_file_info_clear (info);
					/* perhaps the directory is gone */
					if (gnome_vfs_get_file_info
					    (ffr->name, info,
					     GNOME_VFS_FILE_INFO_DEFAULT)
					    != GNOME_VFS_OK) {
						g_free (p);
						reread = TRUE;
						break;
					}
					/* if not, we're just now missing a
					 * desktop file */
					if (dr->ditemmtime > 0) {
						g_free (ffr->icon);
						ffr->icon = NULL;
						g_free (ffr->fullname);
						ffr->fullname = NULL;
						g_free (ffr->comment);
						ffr->comment = NULL;
						ddr->ditemmtime = 0;
						any_change = TRUE;
					}
					dr->ditemlast_stat = 0;
					g_free (p);
					break;
				}
				if (ddr->ditemmtime != info->mtime) {
					QuickDesktopItem *qitem;
					qitem = quick_desktop_item_load_file (p /* file */,
									      NULL /* expected_type */,
									      TRUE /* run_tryexec */);
					if (qitem != NULL) {
						g_free (ffr->icon);
						ffr->icon = g_strdup (qitem->icon);
						g_free (ffr->fullname);
						ffr->fullname = g_strdup (qitem->name);
						g_free (ffr->comment);
						ffr->comment = g_strdup (qitem->comment);
						quick_desktop_item_destroy (qitem);
					} else {
						g_free (ffr->icon);
						ffr->icon = NULL;
						g_free (ffr->fullname);
						ffr->fullname = NULL;
						g_free (ffr->comment);
						ffr->comment = NULL;
					}
					ddr->ditemmtime = info->mtime;
					any_change = TRUE;
				}
				g_free(p);
				break;
			case FILE_REC_FILE:
				if (ffr->last_stat >= curtime-STAT_EVERY)
					break;
				gnome_vfs_file_info_clear (info);
				if (gnome_vfs_get_file_info
				    (ffr->name, info, GNOME_VFS_FILE_INFO_DEFAULT)
				    != GNOME_VFS_OK) {
					reread = TRUE;
					break;
				}
				ffr->last_stat = curtime;
				if (ffr->mtime != info->mtime) {
					QuickDesktopItem *qitem;
					qitem = quick_desktop_item_load_file (ffr->name /* file */,
									      NULL /* expected_type */,
									      TRUE /* run_tryexec */);
					if (qitem != NULL) {
						g_free (ffr->icon);
						ffr->icon = g_strdup (qitem->icon);

						g_free (ffr->fullname);
						ffr->fullname = g_strdup (qitem->name);

						g_free (ffr->comment);
						ffr->comment = g_strdup (qitem->comment);

						quick_desktop_item_destroy (qitem);
					} else {
						reread = TRUE;
						break;
					}
					ffr->mtime = s.st_mtime;
					any_change = TRUE;
				}
				if (ffr->tryexec_path != NULL &&
				    ! g_file_test (ffr->tryexec_path,
						   G_FILE_TEST_EXISTS)) {
					reread = TRUE;
				}
				break;
			case FILE_REC_EXTRA:
				if (ffr->last_stat >= curtime-STAT_EVERY)
					break;
				gnome_vfs_file_info_clear (info);
				result = gnome_vfs_get_file_info
				    (ffr->name,
				     info,
				     GNOME_VFS_FILE_INFO_DEFAULT);
				if((result != GNOME_VFS_OK && ffr->mtime > 0) ||
				   (result == GNOME_VFS_OK && ffr->mtime != info->mtime))
					reread = TRUE;
				ffr->last_stat = curtime;
				break;
			}
		}

		if (reread) {
			ret = fr_replace (fr);
		} else if (any_change) {
			GSList *li;
			for (li = dr->mfl; li != NULL; li = li->next) {
				MenuFinfo *mf = li->data;
				li->data = NULL;
				mf->fr = NULL;
			}
			g_slist_free (dr->mfl);
			dr->mfl = NULL;
		}
	}
	return ret;
}

FileRec *
fr_get_dir (const char *mdir)
{
	GSList *li;
	g_return_val_if_fail (mdir!=NULL, NULL);

	for(li = dir_list; li != NULL; li = li->next) {
		FileRec *fr = li->data;
		g_assert(fr != NULL);
		g_assert(fr->name != NULL);
		if (strcmp (fr->name, mdir) == 0)
			return fr_check_and_reread (fr);
	}
	return fr_read_dir (NULL, mdir, 0, 1);
}

/* Get all directories we have in memory */
GSList*
fr_get_all_dirs (void)
{
  return dir_list;
}

void
fr_force_reread (void)
{
	GSList *li;
	for(li = dir_list; li != NULL; li = li->next) {
		DirRec *dr = li->data;
		g_assert (dr != NULL);

		dr->force_reread = TRUE;
	}
}
