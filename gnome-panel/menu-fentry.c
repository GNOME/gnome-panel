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
#include <gnome.h>

#include "panel-include.h"

/*#define PANEL_DEBUG 1*/

/* the minimum number of seconds between stats of files */
#define STAT_EVERY 3

static GSList *dir_list = NULL;

static GMemChunk *file_chunk = NULL;
static GMemChunk *dir_chunk = NULL;

extern char *merge_main_dir;
extern int merge_main_dir_len;
extern char *merge_merge_dir;

extern GlobalConfig global_config;

void
init_fr_chunks (void)
{
	file_chunk = g_mem_chunk_create (FileRec, 64, G_ALLOC_AND_FREE);
	dir_chunk  = g_mem_chunk_create (DirRec,  16, G_ALLOC_AND_FREE);
}

static GSList *
prepend_mfile (GSList *list, const char *name, gboolean merged,
	       gboolean verified)
{
	MFile *mfile = g_new0 (MFile, 1);

	mfile->name = g_strdup (name);
	mfile->merged = merged;
	mfile->verified = verified;

	return g_slist_prepend (list, mfile);
}

/* merged is 1/0/-1 (-1 is don't care) */
static MFile *
find_mfile (GSList *list, const char *string, int merged)
{
	GSList *li;

	g_return_val_if_fail (string != NULL, NULL);

	for (li = list; li != NULL; li = li->next) {
		MFile *mfile = li->data;

		if (mfile->name != NULL &&
		    strcmp (mfile->name, string) == 0 &&
		    (merged < 0 ||
		     (merged ? 1 : 0) == (mfile->merged ? 1 : 0)))
			return mfile;
	}

	return NULL;
}


/*reads in the order file and makes a list*/
static GSList *
get_presorted_from(GSList *list, const char *dir, gboolean merged)
{
	char buf[PATH_MAX+1];
	char *fname = g_concat_dir_and_file(dir, ".order");
	FILE *fp = fopen(fname, "r");
	
	if(!fp) {
		g_free(fname);
		return list;
	}
	while(fgets(buf, PATH_MAX+1, fp)!=NULL) {
		MFile *mfile;
		char *p = strchr(buf, '\n');
		if(p)
			*p = '\0';

		if(is_ext (buf, ".desktop") ||
		   is_ext (buf, ".kdelnk")) {
			mfile = find_mfile (list, buf, merged ? 1 : 0);
		} else {
			mfile = find_mfile (list, buf, -1);
		}

		if (mfile == NULL)
			list = prepend_mfile (list, buf, merged, FALSE);
	}
	fclose(fp);
	g_free(fname);
	return list;
}

gboolean
fr_is_subdir (const char *dir, const char *superdir, int superdir_len)
{
	if (superdir == NULL || superdir_len == 0)
		return FALSE;

	if (strncmp (dir, superdir, superdir_len-1) == 0 &&
	    (dir[superdir_len-1] == '/' ||
	     dir[superdir_len-1] == '\0')) {
		return TRUE;
	} else {
		return FALSE;
	}
}

char *
fr_get_mergedir (const char *dir)
{
	char *mergedir;

	/* If we never merge, just return NULL */
	if ( ! global_config.merge_menus)
		return NULL;

	if(merge_merge_dir != NULL &&
	   fr_is_subdir(dir, merge_main_dir, merge_main_dir_len)) {
		if (dir[merge_main_dir_len-1] == '/')
			mergedir =
				g_strconcat(merge_merge_dir,
					    &dir[merge_main_dir_len], NULL);
		else
			mergedir =
				g_strconcat(merge_merge_dir,
					    &dir[merge_main_dir_len-1], NULL);
	} else {
		mergedir = NULL;
	}

	return mergedir;
}

static GSList *
read_directory (GSList *list, const char *menudir, gboolean merged)
{
	DIR *dir;
	struct dirent *dent;

	dir = opendir (menudir);
	if (dir != NULL)  {
		while((dent = readdir (dir)) != NULL) {
			MFile *mfile;

			if (dent->d_name[0] == '.')
				continue;

			if(is_ext (dent->d_name, ".desktop") ||
			   is_ext (dent->d_name, ".kdelnk")) {
				mfile = find_mfile (list, dent->d_name,
						    merged ? 1 : 0);
			} else {
				mfile = find_mfile (list, dent->d_name, -1);

				/* if this is a bogus unmerged dir,
				 * then append it again for the merged */
				if (mfile != NULL &&
				    merged &&
				    ! mfile->merged &&
				    ! mfile->verified)
					mfile = NULL;
			}

			if (mfile == NULL)
				list = prepend_mfile (list, dent->d_name,
						      merged, TRUE);
			/* if this is the same merge foo and we've
			 * already foudn it in the presorted, just verify it */
			else if ((mfile->merged ? 1 : 0) == (merged ? 1 : 0))
				mfile->verified = TRUE;
		}

		closedir(dir);
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
get_mfiles_from_menudir (const char *menudir)
{
	GSList *list = NULL;
	char *mergedir;

	mergedir = fr_get_mergedir (menudir);
	
	list = get_presorted_from (list, menudir, FALSE /*merged*/);
	list = read_directory (list, menudir, FALSE /*merged*/);

	if (mergedir != NULL) {
		list = get_presorted_from (list, mergedir, TRUE /*merged*/);
		list = read_directory (list, mergedir, TRUE /*merged*/);

		g_free (mergedir);
	}

	return g_slist_reverse (list);
}

char *
get_applet_goad_id_from_dentry(GnomeDesktopEntry *ii)
{
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
		/*this is here as a horrible hack since that's the way it
		  used to work, but now one should make the .desktop type
		  PanelApplet*/
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
		if (fr->type == FILE_REC_DIR)
			g_chunk_free (fr, dir_chunk);
		else
			g_chunk_free (fr, file_chunk);
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
fr_fill_dir(FileRec *fr, int sublevels)
{
	GSList *flist;
	struct stat s;
	DirRec *dr = (DirRec *)fr;
	FileRec *ffr;
	time_t curtime = time(NULL);
	char *mergedir;
	
	g_return_if_fail (dr->recs == NULL);
	g_return_if_fail (fr != NULL);
	g_return_if_fail (fr->name != NULL);

	ffr = g_chunk_new0 (FileRec, file_chunk);
	ffr->type = FILE_REC_EXTRA;
	ffr->name = g_concat_dir_and_file (fr->name, ".order");
	ffr->parent = dr;
	if (stat (ffr->name, &s) != -1)
		ffr->mtime = s.st_mtime;
	ffr->last_stat = curtime;
	dr->recs = g_slist_prepend(dr->recs, ffr);

	mergedir = fr_get_mergedir (fr->name);

	flist = get_mfiles_from_menudir(fr->name);
	while (flist != NULL) {
		gboolean merged;
		MFile *mfile = flist->data;
		char *name;
		GSList *tmp = flist;
		flist = flist->next;
		g_slist_free_1(tmp);

		if ( ! mfile->merged) {
			name = g_concat_dir_and_file (fr->name, mfile->name);
		} else if (mergedir != NULL) {
			name = g_concat_dir_and_file (mergedir, mfile->name);
		} else {
			free_mfile (mfile);
			continue;
		}
		merged = mfile->merged;

		if (stat (name, &s) == -1) {
			g_free(name);
			free_mfile (mfile);
			continue;
		}
		free_mfile (mfile);

		if (S_ISDIR (s.st_mode)) {
			if (merged)
				ffr = fr_read_dir (NULL, name, NULL, &s, sublevels-1);
			else 
				ffr = fr_read_dir (NULL, name, &s, NULL, sublevels-1);
			g_free(name);
			if(ffr) {
				ffr->merged = merged;
				dr->recs = g_slist_prepend(dr->recs,ffr);
			}
		} else {
			GnomeDesktopEntry *dentry;
			char *tryexec_path;
			char *p = strrchr(name,'.');
			if (p == NULL ||
			    (strcmp (p, ".desktop") != 0 &&
			     strcmp (p, ".kdelnk") != 0)) {
				g_free (name);
				continue;
			}

			tryexec_path = NULL;

			dentry = gnome_desktop_entry_load_unconditional (name);
			if (dentry != NULL &&
			    dentry->tryexec != NULL) {
				tryexec_path = panel_is_program_in_path (dentry->tryexec);
				if (tryexec_path == NULL) {
					dr->tryexecs = g_slist_prepend (dr->tryexecs, dentry->tryexec);
					dentry->tryexec = NULL;
					gnome_desktop_entry_free (dentry);
					dentry = NULL;
				}
			}
			if (dentry != NULL) {
				ffr = g_chunk_new0 (FileRec, file_chunk);
				if (dentry->type != NULL &&
				    strcasecmp_no_locale (dentry->type,
							  "separator") == 0)
					ffr->type = FILE_REC_SEP;
				else
					ffr->type = FILE_REC_FILE;
				ffr->merged = merged;
				ffr->name = name;
				ffr->mtime = s.st_mtime;
				ffr->last_stat = curtime;
				ffr->parent = dr;
				ffr->icon = dentry->icon;
				dentry->icon = NULL;
				ffr->fullname = dentry->name;
				ffr->comment = g_strdup (dentry->comment);
				dentry->name = NULL;
				ffr->tryexec_path = tryexec_path;
				ffr->goad_id =
					get_applet_goad_id_from_dentry (dentry);
				gnome_desktop_entry_free (dentry);

				dr->recs = g_slist_prepend (dr->recs, ffr);
			} else {
				g_free (name);
			}
		}
	}
	dr->recs = g_slist_reverse (dr->recs);

	g_free (mergedir);
}

FileRec *
fr_read_dir (DirRec *dr, const char *mdir, struct stat *dstat,
	     struct stat *merge_dstat, int sublevels)
{
	char *fname;
	struct stat s;
	FileRec *fr;
	time_t curtime = time (NULL);
	char *mergedir;
	
	g_return_val_if_fail (mdir != NULL, NULL);

	mergedir = fr_get_mergedir (mdir);

	/*this will zero all fields*/
	if (dr == NULL) {
		dr = g_chunk_new0 (DirRec, dir_chunk);
		dr->force_reread = FALSE;
		/* this must be set otherwise we may messup on
		   fr_free */
		dr->frec.type = FILE_REC_DIR;
	}
	fr = (FileRec *)dr;

	if (fr->last_stat < curtime-STAT_EVERY) {
		if (dstat == NULL) {
			if (stat (mdir, &s) == -1) {
				fr_free (fr, TRUE);
				g_free (mergedir);
				return NULL;
			}

			fr->mtime = s.st_mtime;
		} else {
			fr->mtime = dstat->st_mtime;
		}

		if (mergedir != NULL) {
			if (merge_dstat == NULL) {
				if (stat (mergedir, &s) == -1) {
					dr->merge_mtime = 0;
				} else {
					dr->merge_mtime = s.st_mtime;
				}
			} else
				dr->merge_mtime = merge_dstat->st_mtime;
		}

		fr->last_stat = curtime;
	}

	g_free (mergedir);

	fr->type = FILE_REC_DIR;
	g_free (fr->name);
	fr->name = g_strdup (mdir);

	s.st_mtime = 0;
	fname = g_concat_dir_and_file (mdir, ".directory");
	if (dr->dentrylast_stat >= curtime-STAT_EVERY ||
	    stat (fname, &s) != -1) {
		GnomeDesktopEntry *dentry;
		dentry = gnome_desktop_entry_load(fname);
		if (dentry != NULL) {
			g_free (fr->icon);
			fr->icon = dentry->icon;
			dentry->icon = NULL;
			g_free (fr->fullname);
			fr->fullname = dentry->name;
			g_free (fr->comment);
			fr->comment = g_strdup (dentry->comment);
			dentry->name = NULL;
			gnome_desktop_entry_free (dentry);
		} else {
			g_free (fr->icon);
			fr->icon = NULL;
			g_free (fr->fullname);
			fr->fullname = NULL;
			g_free (fr->comment);
			fr->comment = NULL;
		}
		/*if we statted*/
		if (s.st_mtime != 0)
			dr->dentrylast_stat = curtime;
		dr->dentrymtime = s.st_mtime;
	}
	g_free (fname);
	
	/* add if missing from list of directories */
	if (g_slist_find (dir_list, fr) == NULL)
		dir_list = g_slist_prepend (dir_list, fr);
	
	/*if this is a fake structure, so we don't actually look into
	  the directory*/
	if (sublevels > 0)
		fr_fill_dir (fr, sublevels);

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

	fr = fr_read_dir ((DirRec *)fr, name, NULL, NULL, 1);
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

	g_return_val_if_fail (fr != NULL, fr);
	g_return_val_if_fail (fr->type == FILE_REC_DIR, fr);

	if (dr->recs == NULL) {
		fr_fill_dir (fr, 1);
	} else {
		gboolean reread = FALSE;
		gboolean any_change = FALSE;
		struct stat ds;
		GSList *li;

		if ( ! global_config.menu_check)
			return ret;

		if (dr->force_reread)
			reread = TRUE;
		dr->force_reread = FALSE;

		/* recheck tryexecs */
		for (li = dr->tryexecs; ! reread && li != NULL; li = li->next) {
			char *tryexec = li->data;
			char *p = panel_is_program_in_path (tryexec);

			if (p != NULL) {
				reread = TRUE;
				g_free (p);
			}
		}

		curtime = time (NULL);

		if ( ! reread &&
		    fr->last_stat < curtime-STAT_EVERY) {
			if(stat(fr->name, &ds)==-1) {
				fr_free (fr, TRUE);
				return NULL;
			}
			if(ds.st_mtime != fr->mtime)
				reread = TRUE;

			if(dr->merge_mtime > 0) {
				char *mergedir = fr_get_mergedir (fr->name);
				if(mergedir != NULL) {
					if(stat(mergedir, &ds) >= 0 &&
					   ds.st_mtime != dr->merge_mtime)
						reread = TRUE;
					g_free(mergedir);
				}
			}
		}
		for(li = dr->recs; !reread && li!=NULL; li=g_slist_next(li)) {
			FileRec *ffr = li->data;
			DirRec *ddr;
			int r;
			char *p;
			struct stat s;

			switch(ffr->type) {
			case FILE_REC_DIR:
				ddr = (DirRec *)ffr;
				p = g_concat_dir_and_file(ffr->name,
							  ".directory");
				if (ddr->dentrylast_stat >= curtime-STAT_EVERY) {
					g_free (p);
					break;
				}
				dr->dentrylast_stat = curtime;
				if(stat(p,&s)==-1) {
					/* perhaps the directory is gone */
					if ( ! panel_file_exists (ffr->name)) {
						reread = TRUE;
						break;
					}
					/* if not, we're just now missing a
					 * desktop file */
					if(dr->dentrymtime) {
						g_free(ffr->icon);
						ffr->icon = NULL;
						g_free(ffr->fullname);
						ffr->fullname = NULL;
						g_free(ffr->comment);
						ffr->comment = NULL;
						ddr->dentrymtime = 0;
						any_change = TRUE;
					}
					dr->dentrylast_stat = 0;
					g_free(p);
					break;
				}
				if(ddr->dentrymtime != s.st_mtime) {
					GnomeDesktopEntry *dentry;
					dentry = gnome_desktop_entry_load(p);
					if(dentry) {
						g_free(ffr->icon);
						ffr->icon = dentry->icon;
						dentry->icon = NULL;
						g_free(ffr->fullname);
						ffr->fullname = dentry->name;
						g_free(ffr->comment);
						ffr->comment = g_strdup (dentry->comment);
						dentry->name = NULL;
						gnome_desktop_entry_free(dentry);
					} else {
						g_free(ffr->icon);
						ffr->icon = NULL;
						g_free(ffr->fullname);
						ffr->fullname = NULL;
						g_free(ffr->comment);
						ffr->comment = NULL;
					}
					ddr->dentrymtime = s.st_mtime;
					any_change = TRUE;
				}
				g_free(p);
				break;
			case FILE_REC_FILE:
				if (ffr->last_stat >= curtime-STAT_EVERY)
					break;
				if(stat(ffr->name,&s)==-1) {
					reread = TRUE;
					break;
				}
				ffr->last_stat = curtime;
				if(ffr->mtime != s.st_mtime) {
					GnomeDesktopEntry *dentry;
					dentry = gnome_desktop_entry_load(ffr->name);
					if (dentry != NULL) {
						/* take over memory */
						g_free (ffr->icon);
						ffr->icon = dentry->icon;
						dentry->icon = NULL;

						/* take over memory */
						g_free (ffr->fullname);
						ffr->fullname = dentry->name;
						dentry->name = NULL;

						/* take over memory */
						g_free (ffr->comment);
						ffr->comment = dentry->comment;
						dentry->comment = NULL;

						gnome_desktop_entry_free (dentry);
					} else {
						reread = TRUE;
						break;
					}
					ffr->mtime = s.st_mtime;
					any_change = TRUE;
				}
				if (ffr->tryexec_path != NULL &&
				    ! panel_file_exists (ffr->tryexec_path)) {
					reread = TRUE;
				}
				break;
			case FILE_REC_EXTRA:
				if (ffr->last_stat >= curtime-STAT_EVERY)
					break;
				r = stat(ffr->name,&s);
				if((r==-1 && ffr->mtime) ||
				   (r!=-1 && ffr->mtime != s.st_mtime))
					reread = TRUE;
				ffr->last_stat = curtime;
				break;
			}
		}
		if(reread) {
			ret = fr_replace(fr);
		} else if(any_change) {
			GSList *li;
			for(li = dr->mfl; li!=NULL; li = g_slist_next(li)) {
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
	return fr_read_dir (NULL, mdir, NULL, NULL, 1);
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
