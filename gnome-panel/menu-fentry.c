/*
 * GNOME panel menu module.
 * (C) 1997 The Free Software Foundation
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

static GSList *dir_list = NULL;

static GMemChunk *file_chunk = NULL;
static GMemChunk *dir_chunk = NULL;

void
init_fr_chunks ()
{
	file_chunk = g_mem_chunk_create (FileRec, 64, G_ALLOC_AND_FREE);
	dir_chunk  = g_mem_chunk_create (DirRec,  16, G_ALLOC_AND_FREE);
}

/*reads in the order file and makes a list*/
static GSList *
get_presorted_from(char *dir)
{
	char buf[PATH_MAX+1];
	GSList *list = NULL;
	char *fname = g_concat_dir_and_file(dir,".order");
	FILE *fp = fopen(fname,"r");
	
	if(!fp) {
		g_free(fname);
		return NULL;
	}
	while(fgets(buf,PATH_MAX+1,fp)!=NULL) {
		char *p = strchr(buf,'\n');
		if(p) *p = '\0';
		list = g_slist_prepend(list,g_strdup(buf));
	}
	fclose(fp);
	g_free(fname);
	return g_slist_reverse(list);
}

GSList *
get_files_from_menudir(char *menudir)
{
	struct dirent *dent;
	DIR *dir;
	GSList *out = NULL;
	GSList *pres = NULL;
	
	dir = opendir (menudir);
	if (dir == NULL)
		return NULL;
	
	pres = get_presorted_from(menudir);
	
	while((dent = readdir (dir)) != NULL) {
		/* Skip over dot files */
		if (dent->d_name [0] == '.')
			continue;
		if(!string_is_in_list(pres,dent->d_name))
			out = g_slist_prepend(out,g_strdup(dent->d_name));
	}

	closedir(dir);
	return g_slist_concat(pres,g_slist_reverse(out));
}

char *
get_applet_goad_id_from_dentry(GnomeDesktopEntry *ii)
{
	int i;
	int constantlen = strlen("--activate-goad-server");

	g_return_val_if_fail(ii!=NULL,NULL);

	if (!ii->exec || !ii->type)
		return NULL;
	
	if(strcmp(ii->type,"PanelApplet")==0) {
		return g_strjoinv(" ",ii->exec);
	} else {
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
fr_free(FileRec *fr, int free_fr)
{
	if(!fr) return;
	g_free(fr->name);
	g_free(fr->fullname);
	g_free(fr->icon);
	g_free(fr->goad_id);
	if(fr->parent && free_fr)
		fr->parent->recs = g_slist_remove(fr->parent->recs,fr);
	if(fr->type == FILE_REC_DIR) {
		DirRec *dr = (DirRec *)fr;
		GSList *li;
		for(li = dr->mfl; li!=NULL; li=g_slist_next(li))
			((MenuFinfo *)li->data)->fr = NULL;
		g_slist_free(dr->mfl);
		for(li = dr->recs; li!=NULL; li=g_slist_next(li)) {
			FileRec *ffr = li->data;
			ffr->parent = NULL;
			fr_free(ffr,TRUE);
		}
		g_slist_free(dr->recs);
	}
	if(free_fr) {
		dir_list = g_slist_remove(dir_list,fr);
		if (fr->type == FILE_REC_DIR)
			g_chunk_free (fr, dir_chunk);
		else
			g_chunk_free (fr, file_chunk);
	} else  {
		if(fr->type == FILE_REC_DIR)
			memset(fr,0,sizeof(DirRec));
		else
			memset(fr,0,sizeof(FileRec));
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
	
	g_return_if_fail(dr->recs==NULL);
	g_return_if_fail(fr!=NULL);
	g_return_if_fail(fr->name!=NULL);

	ffr = g_chunk_new0 (FileRec, file_chunk);
	ffr->type = FILE_REC_EXTRA;
	ffr->name = g_concat_dir_and_file(fr->name,".order");
	ffr->parent = dr;
	if (stat (ffr->name, &s) != -1)
		ffr->mtime = s.st_mtime;
	ffr->last_stat = curtime;
	dr->recs = g_slist_prepend(dr->recs,ffr);

	flist = get_files_from_menudir(fr->name);
	while(flist) {
		char *name = g_concat_dir_and_file(fr->name,flist->data);
		GSList *tmp = flist;
		g_free(flist->data);
		flist = flist->next;
		g_slist_free_1(tmp);
		
		if (stat (name, &s) == -1) {
			g_free(name);
			continue;
		}

		if (S_ISDIR (s.st_mode)) {
			ffr = fr_read_dir(NULL,name,&s,sublevels-1);
			g_free(name);
			if(ffr)
				dr->recs = g_slist_prepend(dr->recs,ffr);
		} else {
			GnomeDesktopEntry *dentry;
			char *p = strrchr(name,'.');
			if (!p || (strcmp(p, ".desktop") != 0 &&
				   strcmp(p, ".kdelnk") != 0)) {
				g_free(name);
				continue;
			}

			dentry = gnome_desktop_entry_load(name);
			if(dentry) {
				ffr = g_chunk_new0 (FileRec, file_chunk);
				ffr->type = FILE_REC_FILE;
				ffr->name = name;
				ffr->mtime = s.st_mtime;
				ffr->last_stat = curtime;
				ffr->parent = dr;
				ffr->icon = dentry->icon;
				dentry->icon = NULL;
				ffr->fullname = dentry->name;
				dentry->name = NULL;
				ffr->goad_id =
					get_applet_goad_id_from_dentry(dentry);
				gnome_desktop_entry_free(dentry);

				dr->recs = g_slist_prepend(dr->recs,ffr);
			} else
				g_free(name);
		}
	}
	dr->recs = g_slist_reverse(dr->recs);
}

FileRec *
fr_read_dir(DirRec *dr, char *mdir, struct stat *dstat, int sublevels)
{
	char *fname;
	struct stat s;
	FileRec *fr;
	time_t curtime = time(NULL);
	
	g_return_val_if_fail(mdir!=NULL,NULL);

	/*this will zero all fields*/
	if(!dr) {
		dr = g_chunk_new0 (DirRec, dir_chunk);
		/* this must be set otherwise we may messup on
		   fr_free */
		dr->frec.type = FILE_REC_DIR;
	}
	fr = (FileRec *)dr;

	if(fr->last_stat < curtime-1) {
		if(!dstat) {
			if (stat (mdir, &s) == -1) {
				fr_free(fr, TRUE);
				return NULL;
			}

			fr->mtime = s.st_mtime;
		} else
			fr->mtime = dstat->st_mtime;
		fr->last_stat = curtime;
	}

	fr->type = FILE_REC_DIR;
	g_free(fr->name);
	fr->name = g_strdup(mdir);

	s.st_mtime = 0;
	fname = g_concat_dir_and_file(mdir,".directory");
	if (dr->dentrylast_stat >= curtime-1 ||
	    stat (fname, &s) != -1) {
		GnomeDesktopEntry *dentry;
		dentry = gnome_desktop_entry_load(fname);
		if(dentry) {
			g_free(fr->icon);
			fr->icon = dentry->icon;
			dentry->icon = NULL;
			g_free(fr->fullname);
			fr->fullname = dentry->name;
			dentry->name = NULL;
			gnome_desktop_entry_free(dentry);
		} else {
			g_free(fr->icon);
			fr->icon = NULL;
			g_free(fr->fullname);
			fr->fullname = NULL;
		}
		/*if we statted*/
		if(s.st_mtime)
			dr->dentrylast_stat = curtime;
		dr->dentrymtime = s.st_mtime;
	}
	g_free(fname);
	
	dir_list = g_slist_prepend(dir_list, fr);
	
	/*if this is a fake structure, so we don't actually look into
	  the directory*/
	if(sublevels>0)
		fr_fill_dir(fr, sublevels);

	return fr;
}


FileRec *
fr_replace(FileRec *fr)
{
	char *tmp = fr->name;
	DirRec *par = fr->parent;
	
	g_assert(fr->type == FILE_REC_DIR);

	fr->parent = NULL;
	fr->name = NULL;
	fr_free(fr,FALSE);
	fr = fr_read_dir((DirRec *)fr,tmp,NULL,1);
	if(fr)
		fr->parent = par;
	return fr;
}


FileRec *
fr_check_and_reread(FileRec *fr)
{
	DirRec *dr = (DirRec *)fr;
	FileRec *ret = fr;
	time_t curtime = time(NULL);
	g_return_val_if_fail(fr!=NULL,fr);
	g_return_val_if_fail(fr->type == FILE_REC_DIR,fr);
	if(!dr->recs) {
		fr_fill_dir(fr,1);
	} else {
		int reread = FALSE;
		int any_change = FALSE;
		struct stat ds;
		GSList *li;
		if (fr->last_stat < curtime-1) {
			if(stat(fr->name,&ds)==-1) {
				fr_free(fr,TRUE);
				return NULL;
			}
			if(ds.st_mtime != fr->mtime)
				reread = TRUE;
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
				if (ddr->dentrylast_stat >= curtime-1) {
					g_free (p);
					break;
				}
				if(stat(p,&s)==-1) {
					if(dr->dentrymtime) {
						g_free(ffr->icon);
						ffr->icon = NULL;
						g_free(ffr->fullname);
						ffr->fullname = NULL;
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
						dentry->name = NULL;
						gnome_desktop_entry_free(dentry);
					} else {
						g_free(ffr->icon);
						ffr->icon = NULL;
						g_free(ffr->fullname);
						ffr->fullname = NULL;
					}
					ddr->dentrymtime = s.st_mtime;
					dr->dentrylast_stat = curtime;
					any_change = TRUE;
				}
				g_free(p);
				break;
			case FILE_REC_FILE:
				if (ffr->last_stat >= curtime-1)
					break;
				if(stat(ffr->name,&s)==-1) {
					reread = TRUE;
					break;
				}
				if(ffr->mtime != s.st_mtime) {
					GnomeDesktopEntry *dentry;
					dentry = gnome_desktop_entry_load(ffr->name);
					if(dentry) {
						g_free(ffr->icon);
						ffr->icon = dentry->icon;
						dentry->icon = NULL;
						g_free(ffr->fullname);
						ffr->fullname = dentry->name;
						dentry->name = NULL;
						gnome_desktop_entry_free(dentry);
					} else {
						reread = TRUE;
						break;
					}
					ffr->mtime = s.st_mtime;
					ffr->last_stat = curtime;
					any_change = TRUE;
				}
				break;
			case FILE_REC_EXTRA:
				if (ffr->last_stat >= curtime-1)
					break;
				r = stat(ffr->name,&s);
				if((r==-1 && ffr->mtime) ||
				   (r!=-1 && ffr->mtime != s.st_mtime))
					reread = TRUE;
				break;
			}
		}
		if(reread) {
			ret = fr_replace(fr);
		} else if(any_change) {
			GSList *li;
			for(li = dr->mfl; li!=NULL; li=g_slist_next(li))
				((MenuFinfo *)li->data)->fr = NULL;
			g_slist_free(dr->mfl);
			dr->mfl = NULL;
		}
	}
	return ret;
}

FileRec *
fr_get_dir(char *mdir)
{
	GSList *li;
	g_return_val_if_fail(mdir!=NULL,NULL);
	for(li=dir_list;li!=NULL;li=g_slist_next(li)) {
		FileRec *fr = li->data;
		g_assert(fr!=NULL);
		g_assert(fr->name!=NULL);
		if(strcmp(fr->name,mdir)==0)
			return fr_check_and_reread(fr);
	}
	return fr_read_dir(NULL,mdir,NULL,1);
}
