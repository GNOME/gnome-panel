/*
 * GNOME panel menu module. redhat menu reading code
 * (C) 1998 The Free Software Foundation
 *
 * Author: George Lebl
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

/*for AnotherLevel menus to trigger rereading on directory writes*/
static time_t rhsysdir_mtime = 0;
static time_t rhuserdir_mtime = 0;

void
rh_submenu_to_display(GtkWidget *menuw, GtkMenuItem *menuitem)
{
	struct stat s;
	int r;
	int do_read = FALSE;
	char *userrh;
	if(!panel_file_exists(REDHAT_MENUDIR))
		return;
	userrh = gnome_util_prepend_user_home(".wmconfig/");

	stat(REDHAT_MENUDIR,&s);

	if(rhsysdir_mtime != s.st_mtime)
		do_read = TRUE;

	r = stat(userrh,&s);
	if((r == -1 && rhuserdir_mtime) ||
	   (r != -1 && rhuserdir_mtime != s.st_mtime))
		do_read = TRUE;

	g_free(userrh);

	if(do_read) 
		create_rh_menu(FALSE);
}

enum { RH_MENU_ITEM,RH_MENU_GROUP };
typedef struct _RHMenuItem RHMenuItem;
struct _RHMenuItem {
	int type;
	char *name;
	union {
		struct {
			char *description;
			char *icon;
			char *mini_icon;
			char *exec;
		} item;
		GSList *items;
	} u;
};

static void
free_rh_item(RHMenuItem *rh)
{
	if(!rh) return;
	g_free(rh->name);
	if(rh->type == RH_MENU_ITEM) {
		g_free(rh->u.item.description);
		g_free(rh->u.item.icon);
		g_free(rh->u.item.mini_icon);
		g_free(rh->u.item.exec);
	} else {
		g_slist_foreach(rh->u.items,(GFunc)free_rh_item,NULL);
		g_slist_free(rh->u.items);
	}
	g_free(rh);
}

/*most likely one of the ugliest functions in existence*/
static char *
read_word(char **p)
{
	char *buf;
	char *w;
	int quoted;
	int len;
	if(!p) return NULL;
	if(!*p) return NULL;
	while(*(*p) == ' ' || *(*p) == '\t') (*p)++;
	if(!(*(*p))) return NULL;
	quoted = FALSE;
	for(w=*p,len=0;
	    *w && (quoted || (*w!=' ' && *w!='\t'));
	    w++) {
		if(*w=='"')
			quoted = !quoted;
		else
			len++;
	}
	buf = g_malloc(len+1);
	quoted = FALSE;
	for(w=buf;*(*p) && (quoted || (*(*p)!=' ' && *(*p)!='\t'));(*p)++) {
		if(*(*p)=='"')
			quoted = !quoted;
		else {
			*w++ = *(*p);
		}
	}
	*w='\0';
	return buf;
}

static int
sort_rh(RHMenuItem *a, RHMenuItem *b)
{
	if(a->type > b->type) return -1;
	else if(a->type < b->type) return 1;
	return strcmp(a->name,b->name);
}

static GSList *
insert_rh_into_tree(GSList *list,RHMenuItem *rh,GSList *group)
{
	GSList *li;
	RHMenuItem *ri;
	if(!group) 
		return g_slist_insert_sorted(list,rh,(GCompareFunc)sort_rh);
	for(li=list;li!=NULL;li=g_slist_next(li)) {
		ri = li->data;
		if(ri->type == RH_MENU_GROUP &&
		   strcmp(ri->name,(char *)group->data)==0) {
			ri->u.items = insert_rh_into_tree(ri->u.items, rh,
							  group->next);
			break;
		}
	}
	/*we inserted it already*/
	if(li) return list;
	
	ri = g_new0(RHMenuItem,1);
	ri->type = RH_MENU_GROUP;
	ri->name = g_strdup(group->data);
	ri->u.items = insert_rh_into_tree(ri->u.items, rh,
					  group->next);
	return g_slist_insert_sorted(list,ri,(GCompareFunc)sort_rh);
}

static void
kill_amp (char *str)
{
	int len;
	
	if (str == NULL)
		return;

	len = strlen (str);

	if (len > 0 &&
	    str[len-1] == '&') {
		str[len-1] = '\0';

		if (len > 1 &&
		    str[len-2] == ' ')
			str[len-2] = '\0';
	}
}

static GSList *
add_redhat_entry(GSList *list, char *file)
{
	FILE *fp;
	char *p,*w;
	char buf[256];
	GSList *group = NULL;
	
	RHMenuItem *rh = NULL;
	
	fp = fopen(file,"r");
	if(!fp) return list;
	
	rh = g_new0(RHMenuItem,1);
	while(fgets(buf,256,fp)) {
		p = strchr(buf,'\n');
		if(p) *p='\0';
		p = buf;
		g_free(read_word(&p)); /*XXX:ignore??*/
		w = read_word(&p);
		if (!w) ;
		else if(strcmp(w,"name")==0) {
			g_free(rh->name);
			rh->name = read_word(&p);
		} else if(strcmp(w,"description")==0) {
			g_free(rh->u.item.description);
			rh->u.item.description = read_word(&p);
		} else if(strcmp(w,"icon")==0) {
			char *s = read_word(&p);
			g_free(rh->u.item.icon);
			if(*s == '/') {
				if(panel_file_exists(s))
					rh->u.item.icon = s;
				else {
					g_free(s);
					rh->u.item.icon = NULL;
				}
			} else {
				rh->u.item.icon = g_concat_dir_and_file("/usr/share/icons", s);
				if(!panel_file_exists(rh->u.item.icon)) {
					g_free(rh->u.item.icon);
					rh->u.item.icon = gnome_pixmap_file(s);
				}
				g_free(s);
			}
		} else if(strcmp(w,"mini-icon")==0) {
			char *s = read_word(&p);
			g_free(rh->u.item.mini_icon);
			if(*s == '/') {
				if(panel_file_exists(s))
					rh->u.item.mini_icon = s;
				else {
					g_free(s);
					rh->u.item.mini_icon = NULL;
				}
			} else {
				rh->u.item.mini_icon = g_concat_dir_and_file("/usr/share/icons/mini", s);
				if(!panel_file_exists(rh->u.item.mini_icon)) {
					g_free(rh->u.item.mini_icon);
					rh->u.item.mini_icon = gnome_pixmap_file(s);
				}
				g_free(s);
			}
		} else if(strcmp(w,"exec")==0) {
			g_free(rh->u.item.exec);
			rh->u.item.exec = read_word(&p);
			kill_amp (rh->u.item.exec);
		} else if(strcmp(w,"group")==0) {
			char *sc;
			char *s = read_word(&p);

			if(group) {
				g_slist_foreach(group,(GFunc)g_free,NULL);
				g_slist_free(group);
				group = NULL;
			}
			sc = strtok(s,"/");
			while(sc) {
				group = g_slist_append(group, g_strdup(sc));
				sc = strtok(NULL,"/");
			}
			g_free(s);
		}
		g_free(w);
	}
	fclose(fp);
	if(!rh->name || !rh->u.item.exec) {
		if(group) {
			g_slist_foreach(group,(GFunc)g_free,NULL);
			g_slist_free(group);
		}
		free_rh_item(rh);
		return list;
	}
	list = insert_rh_into_tree(list,rh,group);
	g_slist_foreach(group,(GFunc)g_free,NULL);
	g_slist_free(group);
	return list;
}


static void
make_rh_submenu(char *dir, GSList *rhlist)
{
	GSList *li;
	FILE *fp;
	char *order_file;
	order_file = g_concat_dir_and_file(dir,".order");
	fp = fopen(order_file,"w");
	g_free(order_file);
	for(li = rhlist;li!=NULL;li = g_slist_next(li)) {
		RHMenuItem *ri = li->data;
		GnomeDesktopEntry dentry = {0};
		dentry.name = ri->name;
		if(ri->type == RH_MENU_GROUP) {
			char *p;
			char *s;
			s = g_strdup(ri->name);
			dentry.type = "Directory";
			while((p=strchr(s,' '))) *p='_';

			p = g_concat_dir_and_file(dir,s);
			g_free(s);
			if(fp) fprintf(fp,"%s\n",sure_string(g_basename(p)));
			mkdir(p,0755);
			dentry.location = g_concat_dir_and_file(p,".directory");
			
			make_rh_submenu(p,ri->u.items);
			/* free up the strings */
			g_free(p);
		} else {
			char *p;
			char *s;
			s = g_strconcat(ri->name,".desktop",NULL);
			while((p=strchr(s,' '))) *p='_';

			dentry.type = "Application";
			dentry.comment = ri->u.item.description;
			dentry.icon = ri->u.item.icon?
				ri->u.item.icon:
				ri->u.item.mini_icon;
			gnome_config_make_vector(ri->u.item.exec,
						 &dentry.exec_length,
						 &dentry.exec);
			dentry.location = g_concat_dir_and_file(dir,s);
			if(fp) fprintf(fp,"%s\n",s);
			/* free up the location */
			g_free(s);
		}
		gnome_desktop_entry_save(&dentry);
		if(dentry.exec) g_strfreev(dentry.exec);
		g_free(dentry.location);
	}
	if(fp) fclose(fp);
}


void
create_rh_menu(int dofork)
{
	char *userrh = gnome_util_prepend_user_home(".wmconfig");
	char *rhdir = gnome_util_home_file("apps-redhat");
	GSList *rhlist = NULL;
	int i;
	char *dirs[3] = {REDHAT_MENUDIR,NULL,NULL};
	struct stat s;
	g_return_if_fail(userrh!=NULL);
	g_return_if_fail(rhdir!=NULL);
	
        dirs[1] = userrh;
	rhsysdir_mtime = rhuserdir_mtime = 0;
	if(stat(REDHAT_MENUDIR,&s)!=-1)
		rhsysdir_mtime = s.st_mtime;
	else if(stat(userrh,&s)!=-1)
		rhuserdir_mtime = s.st_mtime;

	/*slightly hackish, but since this thing eats up a lot of
	  ram, and it leaves nothing in memory that we need, we
	  just fork a process to do it unless we need the data
	  synchroniously*/
	if(dofork) {
		int i = fork();
		if(i>0) {
			g_free (userrh);
			g_free (rhdir);
			wait(NULL);
			return;
		/*if fork failed, no need to worry we'll just do it
		  in this process*/
		} else if(i<0) {
			dofork = FALSE;
		} else {
			/*we're in a child, so fork once more and
			  exit the parent to not leave any zombies
			  around*/
			if(fork()>0) {
				_exit(0);
			}
		}
	}

	remove_directory(rhdir,FALSE);

	/*read redhat wmconfig files*/
	for(i=0;dirs[i];i++) {
		DIR *dir;
		struct dirent *dent;

		dir = opendir (dirs[i]);
		if(!dir) continue;
		while((dent = readdir (dir)) != NULL) {
			char *p;
			if(dent->d_name[0] == '.' &&
			   (dent->d_name[1] == '\0' ||
			    (dent->d_name[1] == '.' &&
			     dent->d_name[2] == '\0')))
				continue;
			p = g_strconcat(dirs[i],"/",dent->d_name,NULL);
			rhlist = add_redhat_entry(rhlist,p);
			/* free the list. */
			g_free(p);
		}
		closedir(dir);
	}
	g_free(userrh);
	mkdir(rhdir,0755);
	if(rhlist) {
		make_rh_submenu(rhdir,rhlist);

		g_slist_foreach(rhlist,(GFunc)free_rh_item,NULL);
		g_slist_free(rhlist);
	}
	g_free(rhdir);
	if(dofork)
		_exit(0);
}
