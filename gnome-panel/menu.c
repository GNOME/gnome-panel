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
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <gnome.h>

#include "panel-include.h"
#include "panel-widget.h"

#define SMALL_ICON_SIZE 20
#define BIG_ICON_SIZE   48

#define PANEL_DEBUG 1;

#define MENU_PATH "menu_path"

#define MENU_PROPERTIES "menu_properties"

#define REDHAT_MENUDIR "/etc/X11/wmconfig"
#define DEBIAN_MENUDIR "/var/lib/gnome/Debian/."

#define MENU_TYPES "types_menu"
#define MENU_TYPE_EDGE "Edge panel"
#define MENU_TYPE_ALIGNED "Aligned panel"
#define MENU_TYPE_SLIDING "Sliding panel"

#define MENU_MODES "modes_menu"
#define MENU_MODE_EXPLICIT_HIDE "Explicit hide"
#define MENU_MODE_AUTO_HIDE "Auto hide"

#define MENU_HIDEBUTTONS "hidebuttons_menu"
#define MENU_HIDEBUTTONS_PIXMAP "With pixmap arrow"
#define MENU_HIDEBUTTONS_PLAIN "Without pixmap"
#define MENU_HIDEBUTTONS_NONE "None"

/* perhaps into basep-widget.h? */
enum {
	HIDEBUTTONS_PIXMAP,
	HIDEBUTTONS_PLAIN,
	HIDEBUTTONS_NONE
};

#define MENU_SIZES "sizes_menu"
#define MENU_SIZE_TINY "Tiny (24 pixels)"
#define MENU_SIZE_STANDARD "Standard (48 pixels)"
#define MENU_SIZE_LARGE "Large (64 pixels)"
#define MENU_SIZE_HUGE "Huge (80 pixels"

#define MENU_BACKS "background_menu"
#define MENU_BACK_NONE "Standard"
#define MENU_BACK_PIXMAP "Pixmap"
#define MENU_BACK_COLOR "Color"

static char *gnome_folder = NULL;

/*for AnotherLevel menus to trigger rereading on directory writes*/
static time_t rhsysdir_mtime = 0;
static time_t rhuserdir_mtime = 0;

extern GSList *applets;
extern GSList *applets_last;
extern int applet_count;

/*list of all panel widgets created*/
extern GSList *panel_list;

extern GlobalConfig global_config;

extern int config_sync_timeout;
extern int panels_to_sync;
extern int applets_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

/*???? this might be ugly, but I guess we can safely assume that we can only
  have one menu open and that nothing weird will happen to the panel that
  opened that menu whilethe user is looking over the choices*/
PanelWidget *current_panel = NULL;

typedef struct _MenuFinfo MenuFinfo;
typedef struct _FileRec FileRec;
typedef struct _DirRec DirRec;

struct _MenuFinfo {
	char *menudir;
	int applets;
	char *dir_name;
	char *pixmap_name;	
	int fake_menu;
	FileRec *fr;
};

enum {
	FILE_REC_FILE, /*.desktop file record*/
	FILE_REC_DIR,  /*directory*/
	FILE_REC_EXTRA /*just check the mtime*/
};

struct _FileRec {
	int type;
	char *name;
	char *fullname;
	char *icon;
	char *goad_id;
	DirRec *parent;
	time_t mtime;
	time_t last_stat;
};


struct _DirRec {
	FileRec frec;
	time_t dentrymtime;
	time_t dentrylast_stat;
	GSList *recs; /*records for directories*/
	GSList *mfl;  /*records of menus using this record*/
};

static GSList *dir_list = NULL;

static void create_rh_menu(int dofork);

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

static GSList *
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

static char *
get_applet_goad_id_from_dentry(GnomeDesktopEntry *ii)
{
	int i;
	int constantlen = strlen("--activate-goad-server");
	char *goad_id=NULL;
	/*FIXME:
	  this is a horrible horrible hack and should be taken out
	  and shot, once we add proper way to do this*/
	for(i=1;ii->exec[i];i++) {
		if(strncmp("--activate-goad-server",
			   ii->exec[i],constantlen)==0) {
			if(strlen(ii->exec[i])>constantlen)
				goad_id = g_strdup(&ii->exec[i][constantlen+1]);
			else
				goad_id = g_strdup(ii->exec[i+1]);
		}
	}
	return goad_id;
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
		g_free(fr);
	} else  {
		if(fr->type == FILE_REC_DIR)
			memset(fr,0,sizeof(DirRec));
		else
			memset(fr,0,sizeof(FileRec));
	}
}

static FileRec * fr_read_dir(DirRec *dr, char *mdir, struct stat *dstat, int sublevels);

static void
fr_fill_dir(FileRec *fr, int sublevels)
{
	GSList *flist;
	struct stat s;
	DirRec *dr = (DirRec *)fr;
	FileRec *ffr;
	time_t curtime = time(NULL);
	
	g_return_if_fail(dr->recs==NULL);

	ffr = g_new0(FileRec,1);
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
				ffr = g_new0(FileRec,1);
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

static FileRec *
fr_read_dir(DirRec *dr, char *mdir, struct stat *dstat, int sublevels)
{
	char *fname;
	struct stat s;
	FileRec *fr;
	time_t curtime = time(NULL);

	/*this will zero all fields*/
	if(!dr)
		dr = g_new0(DirRec,1);
	fr = (FileRec *)dr;

	if(fr->last_stat < curtime-1) {
		if(!dstat) {
			if (stat (mdir, &s) == -1) {
				fr_free(fr,TRUE);
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
	
	dir_list = g_slist_prepend(dir_list,fr);
	
	/*if this is a fake structure, so we don't actually look into
	  the directory*/
	if(sublevels>0)
		fr_fill_dir(fr,sublevels);

	return fr;
}


static FileRec *
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


static FileRec *
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
				if (ddr->dentrylast_stat >= curtime-1)
					break;
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

static FileRec *
fr_get_dir(char *mdir)
{
	GSList *li;
	for(li=dir_list;li!=NULL;li=g_slist_next(li)) {
		FileRec *fr = li->data;
		if(strcmp(fr->name,mdir)==0)
			return fr_check_and_reread(fr);
	}
	return fr_read_dir(NULL,mdir,NULL,1);
}

/*to be called on startup to load in some of the directories,
  this makes the startup a little bit slower, and take up slightly
  more ram, but it also speeds up later operation*/
void
init_menus(void)
{
	/*just load the menus from disk, don't make the widgets
	  this just reads the .desktops of the top most directory
	  and a level down*/
	char *menu = gnome_datadir_file("gnome/apps");
	if(menu)
		fr_read_dir(NULL,menu,NULL,2);
	g_free(menu);
	menu = gnome_datadir_file("applets");
	if(menu)
		fr_read_dir(NULL,menu,NULL,2);
	g_free(menu);
	menu = gnome_util_home_file("apps");
	if(menu)
		fr_read_dir(NULL,menu,NULL,2);
	g_free(menu);
	/*if redhat menus, use the fork version to read*/
	if(g_file_exists(REDHAT_MENUDIR))
		create_rh_menu(TRUE);
}

/*the most important dialog in the whole application*/
static void
about_cb (GtkWidget *widget, gpointer data)
{
	GtkWidget *about;
	char *authors[] = {
	  "George Lebl (jirka@5z.com)",
	  "Miguel de Icaza (miguel@kernel.org)",
	  "Federico Mena (quartic@gimp.org)",
	  "Tom Tromey (tromey@cygnus.com)",
	  "Ian Main (slow@intergate.bc.ca)",
	  "Elliot Lee (sopwith@redhat.com)",
	  "Owen Taylor (otaylor@redhat.com)",
	  "Jacob Berkman (jberkman@andrew.cmu.edu)",
	  "Many others ...",
	  "and finally, The Knights Who Say ... NI!",
	  NULL
	  };

	about = gnome_about_new ( _("The GNOME Panel"), VERSION,
			"(C) 1998 the Free Software Foundation",
			(const gchar **)authors,
			_("This program is responsible for launching "
			"other applications, embedding small applets "
			"within itself, world peace, and random X crashes."),
			"gnome-gegl.png");
	gtk_widget_show (about);

	return;
}

static void
about_gnome_cb(GtkObject *object, char *program_path)
{
	gnome_execute_async(NULL, 1, &program_path);
}

static void
activate_app_def (GtkWidget *widget, char *item_loc)
{
	GnomeDesktopEntry *item = gnome_desktop_entry_load(item_loc);
	if(item) {
		gnome_desktop_entry_launch (item);
		gnome_desktop_entry_free(item);
	} else {
		g_warning(_("Can't load entry"));
	}
}

static void
add_app_to_personal (GtkWidget *widget, char *item_loc)
{
	char *s;
	char *p;
	p = gnome_util_home_file("apps");
	s = g_strdup_printf("cp -r -f %s %s",item_loc,p);
	g_free(p);
	system(s);
	g_free(s);
}

/* returns a g_strdup'd string with filesystem reserved chars replaced */
/* again from gmenu */
static char *
validate_filename(char *file)
{
	char *ret;
	char *ptr;

	g_return_val_if_fail(file != NULL, NULL);
	
	ret = g_strdup(file);
	ptr = ret;
	while (*ptr != '\0') {
		if (*ptr == '/') *ptr = '_';
		ptr++;
	}

	return ret;
}

static void
really_add_new_menu_item (GtkWidget *d, int button, gpointer data)
{
	GnomeDEntryEdit *dedit = GNOME_DENTRY_EDIT(data);
	char *file, *dir = gtk_object_get_data(GTK_OBJECT(d),"dir");
	GnomeDesktopEntry *dentry;
	FILE *fp;
	
	if(button == 0) {
		dentry = gnome_dentry_get_dentry(dedit);

		if(!dentry->name || !(*(dentry->name)))
			dentry->name=g_strdup(_("untitled"));
		
		
		/* assume we are making a new file */
		if (!dentry->location) {
			int i=2;
			char *tmp=NULL;

			tmp = validate_filename(dentry->name);

			file = g_strdup_printf("%s.desktop", tmp);
			dentry->location = g_concat_dir_and_file(dir, file);
				
			while (g_file_exists(dentry->location)) {
				g_free(dentry->location);
				g_free(file);
				file = g_strdup_printf("%s%d", tmp, i++);
				dentry->location = g_concat_dir_and_file(dir, file);
			}
			g_free(tmp);
			g_free(file);
		}

		file = g_concat_dir_and_file(dir, ".order");
		fp = fopen(file, "a");
		if (fp) {
			char *file2 = g_filename_pointer(dentry->location);
			if (file2)
				fprintf(fp, "%s\n", file2);
			else
				g_warning(_("Could not get file from path: %s"), 
					  dentry->location);
			fclose(fp);
		} else
			g_warning(_("Could not open .order file: %s"), file);
		g_free(file);

		gnome_desktop_entry_save(dentry);
		gnome_desktop_entry_free(dentry);

	}
	gtk_widget_destroy(d);
}

static void
add_new_app_to_menu (GtkWidget *widget, char *item_loc)
{
	GtkWidget *d;
	GtkWidget *notebook;
	GnomeDEntryEdit *dee;

	d = gnome_dialog_new(_("Create menu item"),
			     GNOME_STOCK_BUTTON_OK,
			     GNOME_STOCK_BUTTON_CANCEL,
			     NULL);
	gtk_window_set_wmclass(GTK_WINDOW(d),
			       "create_menu_item","Panel");
	gtk_window_set_policy(GTK_WINDOW(d), FALSE, FALSE, TRUE);
	
	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox),notebook,
			   TRUE,TRUE,GNOME_PAD_SMALL);
	dee = GNOME_DENTRY_EDIT(gnome_dentry_edit_new_notebook(GTK_NOTEBOOK(notebook)));
	
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(dee->type_combo)->entry),
			   "Application");

	gtk_object_set_data(GTK_OBJECT(d),"dir", g_strdup(item_loc));
	
	gtk_signal_connect(GTK_OBJECT(d),"clicked",
			   GTK_SIGNAL_FUNC(really_add_new_menu_item),
			   dee);

	gnome_dialog_close_hides(GNOME_DIALOG(d),FALSE);

	gnome_dialog_set_default(GNOME_DIALOG(d),0);

	gtk_widget_show_all(d);	
}

static void
remove_menuitem (GtkWidget *widget, char *item_loc)
{
	char *file, *dir, buf[256], *order_in_name, *order_out_name;
	FILE *order_in_file, *order_out_file;

	g_return_if_fail (item_loc);
	if (unlink(item_loc) < 0) {
		g_warning(_("Could not remove the menu item %s: %s\n"), 
			  item_loc, g_strerror(errno));
		return;
	}

	file = g_filename_pointer(item_loc);
	if (!file) {
		g_warning(_("Could not get file name from path: %s"),
			  item_loc);
		return;
	}

	dir = g_strdup(item_loc);
	dir[g_filename_index(dir)] = '\0';
	
	order_in_name = g_concat_dir_and_file(dir, ".order");
	order_in_file = fopen(order_in_name, "r");

	if (!order_in_file) {
		/*no .order file so we can just leave*/
		g_free(order_in_name);
		return;
	}

	order_out_name = g_concat_dir_and_file(dir, ".order.tmp");
	order_out_file = fopen(order_out_name, "w");

	if (!order_out_file) {
		g_warning(_("Could not open .order file: %s"),
			  order_in_name);
		g_free(order_in_name);
		g_free(order_out_name);
		fclose(order_in_file);
		return;
	}

	while (fgets(buf, 255, order_in_file)) {
		g_strchomp (buf);  /* remove trailing '\n' */
		if (strcmp(buf, file))
			fprintf(order_out_file, "%s\n", buf);
	}

	fclose(order_out_file);
	fclose(order_in_file);

	if (rename(order_out_name, order_in_name) == -1)
		g_warning(_("Could not rename tmp file %s"),
			  order_out_name);

	g_free(order_out_name);
	g_free(order_in_name);
	g_free(file);
}

static void
add_app_to_panel (GtkWidget *widget, char *item_loc)
{
	load_launcher_applet(item_loc, current_panel,0);
}


static void
add_drawers_from_dir(char *dirname, char *name, int pos, PanelWidget *panel)
{
	AppletInfo *info;
	Drawer *drawer;
	PanelWidget *newpanel;
	GnomeDesktopEntry *item_info;
	char *dentry_name;
	char *subdir_name;
	char *pixmap_name;
	GSList *list, *li;

	if(!g_file_exists(dirname))
		return;

	dentry_name = g_concat_dir_and_file (dirname,
					     ".directory");
	item_info = gnome_desktop_entry_load (dentry_name);
	g_free (dentry_name);

	if(!name)
		subdir_name = item_info?item_info->name:NULL;
	else
		subdir_name = name;
	pixmap_name = item_info?item_info->icon:NULL;

	load_drawer_applet(-1,pixmap_name,subdir_name,
			   panel,pos);
	
	g_return_if_fail(applets_last!=NULL);
	info = applets_last->data;
	g_return_if_fail(info!=NULL);
	
	drawer = info->data;
	g_return_if_fail(drawer);
	newpanel = PANEL_WIDGET(BASEP_WIDGET(drawer->drawer)->panel);
	
	list = get_files_from_menudir(dirname);
	for(li = list; li!= NULL; li = g_slist_next(li)) {
		char *filename = g_concat_dir_and_file(dirname, li->data);
		struct stat s;
		g_free(li->data);
		if (stat (filename, &s) == 0) {
			if (S_ISDIR (s.st_mode)) {
				add_drawers_from_dir(filename,NULL,INT_MAX/2,
						     newpanel);
			} else {
				char *p = strrchr(filename,'.');
				if (p &&
				    (strcmp(p,".desktop")==0 || 
				     strcmp(p,".kdelnk")==0))
					/*we load the applet at the right
					  side, that is end of the drawer*/
					load_launcher_applet(filename,
							     newpanel,
							     INT_MAX/2);
			}
		}
		g_free(filename);
	}
	g_slist_free(list);
}

/*add a drawer with the contents of a menu to the panel*/
static void
add_menudrawer_to_panel(GtkWidget *w, gpointer data)
{
	MenuFinfo *mf = data;
	g_return_if_fail(mf);
	
	add_drawers_from_dir(mf->menudir,mf->dir_name,0,
			     current_panel);
}

static void
add_menu_to_panel (GtkWidget *widget, gpointer data)
{
	MenuFinfo *mf = data;
	int flags = MAIN_MENU_SYSTEM|MAIN_MENU_USER|
		MAIN_MENU_SYSTEM_SUB|MAIN_MENU_USER_SUB;
	/*guess redhat menus*/
	if(g_file_exists(REDHAT_MENUDIR))
		flags |= MAIN_MENU_REDHAT|MAIN_MENU_REDHAT_SUB;

	/*guess KDE menus*/
	if(g_file_exists(KDE_MENUDIR))
		flags |= MAIN_MENU_KDE|MAIN_MENU_KDE_SUB;

	/*guess debian menus*/
	if (g_file_exists(DEBIAN_MENUDIR))
		flags |= MAIN_MENU_DEBIAN|MAIN_MENU_DEBIAN_SUB;

	if(mf)
		load_menu_applet(mf->menudir,flags, current_panel, 0);
	else
		load_menu_applet(NULL,flags, current_panel, 0);
}


/*most of this function stolen from the real gtk_menu_popup*/
static void
restore_grabs(GtkWidget *w, gpointer data)
{
	GtkWidget *menu_item = data;
	GtkMenu *menu = GTK_MENU(menu_item->parent); 
	GtkWidget *xgrab_shell;
	GtkWidget *parent;
	/* Find the last viewable ancestor, and make an X grab on it
	 */
	parent = GTK_WIDGET (menu);
	xgrab_shell = NULL;
	while (parent) {
		gboolean viewable = TRUE;
		GtkWidget *tmp = parent;

		while (tmp) {
			if (!GTK_WIDGET_MAPPED (tmp)) {
				viewable = FALSE;
				break;
			}
			tmp = tmp->parent;
		}

		if (viewable)
			xgrab_shell = parent;

		parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
	}

	/*only grab if this HAD a grab before*/
	if (xgrab_shell && (GTK_MENU_SHELL (xgrab_shell)->have_xgrab)) {
		GdkCursor *cursor = gdk_cursor_new (GDK_ARROW);

		GTK_MENU_SHELL (xgrab_shell)->have_xgrab = 
			(gdk_pointer_grab (xgrab_shell->window, TRUE,
					   GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
					   GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK,
					   NULL, cursor, 0) == 0);
		gdk_cursor_destroy (cursor);
	}
	
	gtk_grab_add (GTK_WIDGET (menu));
}

static void
dentry_apply_callback(GtkWidget *widget, int page, gpointer data)
{
	GnomeDesktopEntry *dentry;

	if (page != -1)
		return;
	
	g_return_if_fail(data!=NULL);
	g_return_if_fail(GNOME_IS_DENTRY_EDIT(data));

	dentry = gnome_dentry_get_dentry(GNOME_DENTRY_EDIT(data));
	dentry->location = g_strdup(gtk_object_get_data(data,"location"));
	gnome_desktop_entry_save(dentry);
	gnome_desktop_entry_free(dentry);
}

static void
edit_dentry(GtkWidget *widget, char *item_loc)
{
	GtkWidget *dialog;
	GtkObject *o;
	GnomeDesktopEntry *dentry;
	
	g_return_if_fail(item_loc!=NULL);

	dentry = gnome_desktop_entry_load(item_loc);
	/* We'll screw up a KDE menu entry if we edit it */
	if (dentry && dentry->is_kde) {
		gnome_desktop_entry_free (dentry);
		return;
	}

	dialog = gnome_property_box_new();
	gtk_window_set_wmclass(GTK_WINDOW(dialog),
			       "desktop_entry_properties","Panel");
	gtk_window_set_title(GTK_WINDOW(dialog), _("Desktop entry properties"));
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	o = gnome_dentry_edit_new_notebook(GTK_NOTEBOOK(GNOME_PROPERTY_BOX(dialog)->notebook));
	/*item loc will be alive all this time*/
	gtk_object_set_data(o,"location",item_loc);

	if(dentry) {
		gnome_dentry_edit_set_dentry(GNOME_DENTRY_EDIT(o),dentry);
		gnome_desktop_entry_free(dentry);
	}

	gtk_signal_connect_object(GTK_OBJECT(o), "changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(dialog));

	gtk_signal_connect(GTK_OBJECT(dialog), "apply",
			   GTK_SIGNAL_FUNC(dentry_apply_callback),
			   o);
	gtk_widget_show(dialog);
}

static void
edit_direntry(GtkWidget *widget, MenuFinfo *mf)
{
	GtkWidget *dialog;
	GtkObject *o;
	char *dirfile = g_concat_dir_and_file(mf->menudir, ".directory");
	GnomeDesktopEntry *dentry;

	dentry = gnome_desktop_entry_load_unconditional(dirfile);
	/* We'll screw up a KDE menu entry if we edit it */
	if (dentry && dentry->is_kde) {
		gnome_desktop_entry_free (dentry);
		return;
	}

	dialog = gnome_property_box_new();
	gtk_window_set_wmclass(GTK_WINDOW(dialog),
			       "desktop_entry_properties","Panel");
	gtk_window_set_title(GTK_WINDOW(dialog), _("Desktop entry properties"));
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	o = gnome_dentry_edit_new_notebook(GTK_NOTEBOOK(GNOME_PROPERTY_BOX(dialog)->notebook));

	if (dentry) {
		gnome_dentry_edit_set_dentry(GNOME_DENTRY_EDIT(o), dentry);
		gtk_object_set_data_full(o,"location",
					 g_strdup(dentry->location),
					 (GtkDestroyNotify)g_free);
		gnome_desktop_entry_free(dentry);
		g_free(dirfile);
	} else {
		dentry = g_new0(GnomeDesktopEntry, 1);
		dentry->name =
			mf->dir_name?g_strdup(mf->dir_name):g_strdup("Menu");
		dentry->type = g_strdup("Directory");
		/*we don't have to free dirfile here it will be freed as if
		  we had strduped it here*/
		gtk_object_set_data_full(o,"location",dirfile,
					 (GtkDestroyNotify)g_free);
		gnome_dentry_edit_set_dentry(GNOME_DENTRY_EDIT(o), dentry);
		gnome_desktop_entry_free(dentry);
	}

	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->exec_entry, FALSE);
	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->tryexec_entry, FALSE);
	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->doc_entry, FALSE);
	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->type_combo, FALSE);
	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->terminal_button, FALSE);

	gtk_signal_connect_object(o, "changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(dialog));

	gtk_signal_connect(GTK_OBJECT(dialog), "apply",
			   GTK_SIGNAL_FUNC(dentry_apply_callback),
			   o);
	gtk_widget_show(dialog);
}

typedef struct _ShowItemMenu ShowItemMenu;
struct _ShowItemMenu {
	int type;
	char *item_loc;
	MenuFinfo *mf;
	GtkWidget *menu;
	GtkWidget *prop_item;
};

static int
is_ext(char *f, char *ext)
{
	char *p = strrchr(f,'.');
	if(!p) return FALSE;
	else if(strcmp(p,ext) == 0) return TRUE;
	else return FALSE;
}

static void
show_item_menu(GtkWidget *item, GdkEventButton *bevent, ShowItemMenu *sim)
{
	GtkWidget *menuitem;
	
	if(!sim->menu) {
		sim->menu = gtk_menu_new ();
		
		gtk_signal_connect(GTK_OBJECT(sim->menu),"deactivate",
				   GTK_SIGNAL_FUNC(restore_grabs),
				   item);

		if(sim->type == 1) {
			char *tmp;
			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add this launcher to panel"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_app_to_panel),
					   sim->item_loc);
			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add this to personal menu"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_app_to_personal),
					   sim->item_loc);
			/*ummmm slightly ugly but should work 99% of time*/
			if(strstr(sim->item_loc,"/.gnome/apps/"))
				gtk_widget_set_sensitive(menuitem,FALSE);

			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Remove this item"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
					    GTK_SIGNAL_FUNC (remove_menuitem),
					    sim->item_loc);
			tmp = g_dirname(sim->item_loc);
			if(access(tmp,W_OK)!=0)
				gtk_widget_set_sensitive(menuitem,FALSE);
			g_free(tmp);
			gtk_signal_connect_object(GTK_OBJECT(menuitem),
						  "activate",
						  GTK_SIGNAL_FUNC(gtk_menu_shell_deactivate),
						  GTK_OBJECT(item->parent));
		} else {
			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add this as drawer to panel"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
				   GTK_SIGNAL_FUNC(add_menudrawer_to_panel),
				   sim->mf);

			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add this as menu to panel"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_menu_to_panel),
					   sim->mf);
			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add this to personal menu"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_app_to_personal),
					   sim->mf->menudir);
			/*ummmm slightly ugly but should work 99% of time*/
			if(strstr(sim->mf->menudir,"/.gnome/apps"))
				gtk_widget_set_sensitive(menuitem,FALSE);

			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add new item to this menu"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			/*when activated we must pop down the first menu*/
			gtk_signal_connect_object(GTK_OBJECT(menuitem),
						  "activate",
						  GTK_SIGNAL_FUNC(gtk_menu_shell_deactivate),
						  GTK_OBJECT(item->parent));
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_new_app_to_menu),
					   sim->mf->menudir);
			if(access(sim->mf->menudir,W_OK)!=0)
				gtk_widget_set_sensitive(menuitem,FALSE);
		}

		sim->prop_item = gtk_menu_item_new ();
		/*when activated we must pop down the first menu*/
		gtk_signal_connect_object(GTK_OBJECT(sim->prop_item),
					  "activate",
					  GTK_SIGNAL_FUNC(gtk_menu_shell_deactivate),
					  GTK_OBJECT(item->parent));
		if(sim->type == 1)
			gtk_signal_connect(GTK_OBJECT(sim->prop_item),
					   "activate",
					   GTK_SIGNAL_FUNC(edit_dentry),
					   sim->item_loc);
		else
			gtk_signal_connect(GTK_OBJECT(sim->prop_item),
					   "activate",
					   GTK_SIGNAL_FUNC(edit_direntry),
					   sim->mf);
		gtk_object_set_data(GTK_OBJECT(item),"prop_item",
				    sim->prop_item);
		setup_menuitem (sim->prop_item, 0, _("Properties..."));
		gtk_menu_append (GTK_MENU (sim->menu), sim->prop_item);
	}
	
	gtk_widget_set_sensitive(sim->prop_item,FALSE);
	if(sim->item_loc &&
	   /*A HACK: but it works, don't have it edittable if it's redhat
	     menus as they are auto generated!*/
	   !strstr(sim->item_loc,"/.gnome/apps-redhat/") &&
	   /*if it's a kdelnk file, don't let it be editted*/
	   !is_ext(sim->item_loc,".kdelnk") &&
	   access(sim->item_loc,W_OK)==0) {
#ifdef PANEL_DEBUG
		puts(sim->item_loc);
#endif
		/*file exists and is writable, we're in bussines*/
		gtk_widget_set_sensitive(sim->prop_item,TRUE);
	} else if(!sim->item_loc || errno==ENOENT) {
		/*the dentry isn't there, check if we can write the
		  directory*/
		if(access(sim->mf->menudir,W_OK)==0 &&
		   /*A HACK: but it works, don't have it edittable if it's redhat
		     menus as they are auto generated!*/
		   !strstr(sim->mf->menudir,".gnome/apps-redhat/"))
			gtk_widget_set_sensitive(sim->prop_item,TRUE);
	}
	
	gtk_menu_popup (GTK_MENU (sim->menu),
			NULL,
			NULL,
			NULL,
			NULL,
			bevent->button,
			bevent->time);
}

static int
show_item_menu_b_cb(GtkWidget *w, GdkEvent *event, ShowItemMenu *sim)
{
	GdkEventButton *bevent = (GdkEventButton *)event;
	GtkWidget *item;
	
	if(event->type!=GDK_BUTTON_PRESS)
		return FALSE;
	
	item = w->parent->parent;
	show_item_menu(item,bevent,sim);
	
	return TRUE;
}

static int
show_item_menu_mi_cb(GtkWidget *w, GdkEvent *event, ShowItemMenu *sim)
{
	GdkEventButton *bevent = (GdkEventButton *)event;
	
	if(event->type==GDK_BUTTON_PRESS && bevent->button==3)
		show_item_menu(w,bevent,sim);
	
	return FALSE;
}

static void
destroy_item_menu(GtkWidget *w, ShowItemMenu *sim)
{
	/*NOTE: don't free item_loc or mf.. it's not ours and will be free'd
	  elsewhere*/
	if(sim->menu)
		gtk_widget_destroy(sim->menu);
	g_free(sim);
}

/* This is a _horrible_ hack to have this here. This needs to be added to the
 * GTK+ menuing code in some manner.
 */
static void  
drag_end_menu_cb (GtkWidget *widget, GdkDragContext     *context)
{
  GtkWidget *xgrab_shell;
  GtkWidget *parent;

  /* Find the last viewable ancestor, and make an X grab on it
   */
  parent = widget->parent;
  xgrab_shell = NULL;
  while (parent)
    {
      gboolean viewable = TRUE;
      GtkWidget *tmp = parent;
      
      while (tmp)
	{
	  if (!GTK_WIDGET_MAPPED (tmp))
	    {
	      viewable = FALSE;
	      break;
	    }
	  tmp = tmp->parent;
	}
      
      if (viewable)
	xgrab_shell = parent;
      
      parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
    }
  
  if (xgrab_shell)
    {
      GdkCursor *cursor = gdk_cursor_new (GDK_ARROW);

      if ((gdk_pointer_grab (xgrab_shell->window, TRUE,
			     GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			     GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
			     GDK_POINTER_MOTION_MASK,
			     NULL, cursor, GDK_CURRENT_TIME) == 0))
	{
	  if (gdk_keyboard_grab (xgrab_shell->window, TRUE,
				 GDK_CURRENT_TIME) == 0)
	    GTK_MENU_SHELL (xgrab_shell)->have_xgrab = TRUE;
	  else
	    {
	      gdk_pointer_ungrab (GDK_CURRENT_TIME);
	    }
	}

      gdk_cursor_destroy (cursor);
    }
}

static void  
drag_data_get_menu_cb (GtkWidget *widget, GdkDragContext     *context,
		       GtkSelectionData   *selection_data, guint info,
		       guint time, char *item_loc)
{
	gchar *uri_list = g_strconcat ("file:", item_loc, "\r\n", NULL);
	gtk_selection_data_set (selection_data,
				selection_data->target, 8, uri_list,
				strlen(uri_list));
	g_free(uri_list);
}

static void  
drag_data_get_string_cb (GtkWidget *widget, GdkDragContext     *context,
		      GtkSelectionData   *selection_data, guint info,
		      guint time, char *string)
{
	gtk_selection_data_set (selection_data,
				selection_data->target, 8, string,
				strlen(string));
}

static void
setup_title_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title,
		      MenuFinfo *mf)
{
	GtkWidget *label, *hbox=NULL, *align;

	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);

	if(global_config.show_small_icons ||
	   global_config.show_dot_buttons) {
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (hbox);
		gtk_container_add (GTK_CONTAINER (menuitem), hbox);
	} else
		gtk_container_add (GTK_CONTAINER (menuitem), label);
	
	if(global_config.show_small_icons) {
		align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
		gtk_widget_show (align);
		gtk_container_set_border_width (GTK_CONTAINER (align), 1);

		if (pixmap) {
			gtk_container_add (GTK_CONTAINER (align), pixmap);
			gtk_widget_set_usize (align, 22, 16);
			gtk_widget_show (pixmap);
		} else
			gtk_widget_set_usize (align, 22, 16);

		gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);
	}

	if(global_config.show_small_icons ||
	   global_config.show_dot_buttons)
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
	if(mf) {
		ShowItemMenu *sim = g_new0(ShowItemMenu,1);
		sim->mf = mf;/*make sure you don't free this,
			       it's not ours!*/
		sim->type = 0;
		gtk_signal_connect(GTK_OBJECT(menuitem),"event",
				   GTK_SIGNAL_FUNC(show_item_menu_mi_cb),
				   sim);
		gtk_signal_connect(GTK_OBJECT(menuitem),"destroy",
				   GTK_SIGNAL_FUNC(destroy_item_menu),
				   sim);
		if(global_config.show_dot_buttons) {
			GtkWidget *w = gtk_button_new_with_label(_("..."));
			gtk_signal_connect(GTK_OBJECT(w),"event",
					   GTK_SIGNAL_FUNC(show_item_menu_b_cb),
					   sim);
			gtk_widget_show(w);
			gtk_box_pack_end (GTK_BOX (hbox), w, FALSE, FALSE, 0);

			/*this is not really a problem for large fonts but it
			  makes the button smaller*/
			gtk_widget_set_usize(w,0,16);
		}
	}

	gtk_widget_show (menuitem);

	gtk_signal_connect_object(GTK_OBJECT(menuitem),"select",
				  GTK_SIGNAL_FUNC(gtk_item_deselect),
				  GTK_OBJECT(menuitem));
}

static void
setup_full_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title,
		     char *item_loc)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "text/uri-list", 0, 0 }
	};

	GtkWidget *label, *hbox=NULL, *align;

	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);
	
	if(global_config.show_small_icons ||
	   global_config.show_dot_buttons) {
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (hbox);
		gtk_container_add (GTK_CONTAINER (menuitem), hbox);
	} else
		gtk_container_add (GTK_CONTAINER (menuitem), label);
	
	if(global_config.show_small_icons) {
		align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
		gtk_widget_show (align);
		gtk_container_set_border_width (GTK_CONTAINER (align), 1);

		if (pixmap) {
			gtk_container_add (GTK_CONTAINER (align), pixmap);
			gtk_widget_set_usize (align, 22, 16);
			gtk_widget_show (pixmap);
		} else
			gtk_widget_set_usize (align, 22, 16);

		gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);
	}

	if(global_config.show_small_icons ||
	   global_config.show_dot_buttons)
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
	if(item_loc) {
		ShowItemMenu *sim = g_new0(ShowItemMenu,1);
		sim->item_loc = item_loc; /*make sure you don't free this,
					    it's not ours!*/
		sim->type = 1;
		gtk_signal_connect(GTK_OBJECT(menuitem),"event",
				   GTK_SIGNAL_FUNC(show_item_menu_mi_cb),
				   sim);
		gtk_signal_connect(GTK_OBJECT(menuitem),"destroy",
				   GTK_SIGNAL_FUNC(destroy_item_menu),
				   sim);
		if(global_config.show_dot_buttons) {
			GtkWidget *w = gtk_button_new_with_label(_("..."));
			gtk_signal_connect(GTK_OBJECT(w),"event",
					   GTK_SIGNAL_FUNC(show_item_menu_b_cb),
					   sim);
			gtk_widget_show(w);
			gtk_box_pack_end (GTK_BOX (hbox), w, FALSE, FALSE, 0);
			/*this is not really a problem for large fonts but it
			  makes the button smaller*/
			gtk_widget_set_usize(w,0,16);
		}

		gtk_drag_source_set(menuitem,
				    GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
				    menu_item_targets, 1,
				    GDK_ACTION_COPY);

		gtk_signal_connect(GTK_OBJECT(menuitem), "drag_data_get",
				   drag_data_get_menu_cb, item_loc);
		gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
				   drag_end_menu_cb, NULL);
	}

	gtk_widget_show (menuitem);
}

void
setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title)
{
	setup_full_menuitem(menuitem,pixmap,title,NULL);
}

static void
setup_directory_drag (GtkWidget *menuitem, char *directory)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-directory", 0, 0 }
	};

	gtk_drag_source_set(menuitem,
			    GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			    menu_item_targets, 1,
			    GDK_ACTION_COPY);
	
	gtk_signal_connect_full(GTK_OBJECT(menuitem), "drag_data_get",
			   GTK_SIGNAL_FUNC (drag_data_get_string_cb), NULL,
			   g_strdup (directory), (GtkDestroyNotify)g_free,
			   FALSE, FALSE);
	gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
			   GTK_SIGNAL_FUNC (drag_end_menu_cb), NULL);
}

static void
setup_internal_applet_drag (GtkWidget *menuitem, char *applet_type)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};
	
	if(!applet_type)
		return;
	
	gtk_drag_source_set(menuitem,
			    GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			    menu_item_targets, 1,
			    GDK_ACTION_COPY);
	
	gtk_signal_connect_full(GTK_OBJECT(menuitem), "drag_data_get",
			   GTK_SIGNAL_FUNC (drag_data_get_string_cb), NULL,
			   g_strdup (applet_type), (GtkDestroyNotify)g_free,
			   FALSE, FALSE);
	gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
			   GTK_SIGNAL_FUNC (drag_end_menu_cb), NULL);

}

static void
setup_applet_drag (GtkWidget *menuitem, char *goad_id)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet", 0, 0 }
	};
	
	if(!goad_id)
		return;
	
	gtk_drag_source_set(menuitem,
			    GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			    menu_item_targets, 1,
			    GDK_ACTION_COPY);
	
	/*note: goad_id should be alive long enough!!*/
	gtk_signal_connect(GTK_OBJECT(menuitem), "drag_data_get",
			   GTK_SIGNAL_FUNC (drag_data_get_string_cb),
			   goad_id);
	gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
			   GTK_SIGNAL_FUNC (drag_end_menu_cb), NULL);

}

static void
add_menu_separator (GtkWidget *menu)
{
	GtkWidget *menuitem;
	
	menuitem = gtk_menu_item_new ();
	gtk_widget_set_sensitive (menuitem, FALSE);
	gtk_widget_show (menuitem);
	gtk_menu_append (GTK_MENU (menu), menuitem);
}

static void
add_drawer_to_panel (GtkWidget *widget, gpointer data)
{
	load_drawer_applet(-1,NULL,NULL, current_panel, 0);
}

static void
add_logout_to_panel (GtkWidget *widget, gpointer data)
{
	load_logout_applet(current_panel, 0);
}

static void
add_lock_to_panel (GtkWidget *widget, gpointer data)
{
	load_lock_applet(current_panel, 0);
}

static void
add_applet (GtkWidget *w, char *item_loc)
{
	GnomeDesktopEntry *ii;
	char *goad_id;

	ii = gnome_desktop_entry_load(item_loc);
	if(!ii) {
		g_warning(_("Can't load entry"));
		return;
	}

	goad_id = get_applet_goad_id_from_dentry(ii);
	gnome_desktop_entry_free(ii);
	
	if(!goad_id) {
		g_warning(_("Can't get goad_id from desktop entry!"));
		return;
	}
	load_extern_applet(goad_id,NULL,current_panel,0,FALSE);

	g_free(goad_id);
}

static void
destroy_mf(MenuFinfo *mf)
{
	if(mf->fr) {
		DirRec *dr = (DirRec *)mf->fr;
		dr->mfl = g_slist_remove(dr->mfl,mf);
	}
	if(mf->menudir) g_free(mf->menudir);
	if(mf->dir_name) g_free(mf->dir_name);
	if(mf->pixmap_name) g_free(mf->pixmap_name);
	g_free(mf);
}


static void
menu_destroy(GtkWidget *menu, gpointer data)
{
	GSList *mfl = gtk_object_get_data(GTK_OBJECT(menu),"mf");
	GSList *li;
	for(li=mfl;li!=NULL;li=g_slist_next(li)) {
		MenuFinfo *mf = li->data;
		destroy_mf(mf);
	}
	g_slist_free(mfl);
	gtk_object_set_data(GTK_OBJECT(menu),"mf",NULL);
}

static void add_menu_widget (Menu *menu, GSList *menudirl,
			     int main_menu, int fake_subs);
static GtkWidget * create_menu_at (GtkWidget *menu, char *menudir, 
				   int applets, char *dir_name,
				   char *pixmap_name, int fake_submenus,
				   int force);
static GtkWidget * create_menu_at_fr (GtkWidget *menu, FileRec *fr,
				      int applets, char *dir_name,
				      char *pixmap_name, int fake_submenus,
				      int force);

/*reread the applet menu, not a submenu*/
static void
check_and_reread_applet(Menu *menu,int main_menu)
{
	GSList *mfl = gtk_object_get_data(GTK_OBJECT(menu->menu), "mf");
	GSList *list;
	int need_reread = FALSE;

	/*we shouldn't warn, this is more for debugging anyway,
	  and nowdays we do have menus that don't have one, this
	  however might be needed for further debugging*/
	/*if(!mfl)
	  g_warning("Weird menu doesn't have mf entry");*/

	/*check if we need to reread this*/
	for(list = mfl; list != NULL; list = g_slist_next(list)) {
		MenuFinfo *mf = list->data;
		if(mf->fake_menu ||
		   mf->fr == NULL) {
			if(mf->fr)
				mf->fr = fr_replace(mf->fr);
			else
				mf->fr = fr_get_dir(mf->menudir);
			need_reread = TRUE;
		} else {
			FileRec *fr;
			fr = fr_check_and_reread(mf->fr);
			if(fr!=mf->fr ||
			   fr == NULL) {
				need_reread = TRUE;
				mf->fr = fr;
			}
		}
	}

	if(need_reread) {
		/*that will be destroyed in add_menu_widget*/
		if(main_menu)
			add_menu_widget(menu,NULL,main_menu,TRUE);
		else {
			GSList *dirlist = NULL;
			for(list = mfl; list != NULL;
			    list = g_slist_next(list)) {
				MenuFinfo *mf = list->data;
				dirlist = g_slist_append(dirlist,
							 g_strdup(mf->menudir));
			}
			add_menu_widget(menu,dirlist, main_menu,TRUE);

			g_slist_foreach(dirlist,(GFunc)g_free,NULL);
			g_slist_free(dirlist);
		}
	}
}

/* XXX: hmmm stolen GTK code, the gtk_menu_reposition only calls
   gtk_menu_position if the widget is drawable, but that's not the
   case when we want to do it*/
static void
gtk_menu_position (GtkMenu *menu)
{
  GtkWidget *widget;
  GtkRequisition requisition;
  gint x, y;
 
  g_return_if_fail (menu != NULL);
  g_return_if_fail (GTK_IS_MENU (menu));

  widget = GTK_WIDGET (menu);

  gdk_window_get_pointer (NULL, &x, &y, NULL);

  /* We need the requisition to figure out the right place to
   * popup the menu. In fact, we always need to ask here, since
   * if one a size_request was queued while we weren't popped up,
   * the requisition won't have been recomputed yet.
   */
  gtk_widget_size_request (widget, &requisition);
      
  if (menu->position_func)
    (* menu->position_func) (menu, &x, &y, menu->position_func_data);
  else
    {
      gint screen_width;
      gint screen_height;
      
      screen_width = gdk_screen_width ();
      screen_height = gdk_screen_height ();
	  
      x -= 2;
      y -= 2;
      
      if ((x + requisition.width) > screen_width)
	x -= ((x + requisition.width) - screen_width);
      if (x < 0)
	x = 0;
      if ((y + requisition.height) > screen_height)
	y -= ((y + requisition.height) - screen_height);
      if (y < 0)
	y = 0;
    }
  
  gtk_widget_set_uposition (GTK_MENU_SHELL (menu)->active ?
			        menu->toplevel : menu->tearoff_window, 
			    x, y);
}


static void
submenu_to_display(GtkWidget *menuw, GtkMenuItem *menuitem)
{
	GSList *mfl = gtk_object_get_data(GTK_OBJECT(menuw), "mf");
	GSList *list;
	int need_reread = FALSE;

	/*if(!mfl)
	  g_warning("Weird menu doesn't have mf entry");*/

	/*check if we need to reread this*/
	for(list = mfl; list != NULL; list = g_slist_next(list)) {
		MenuFinfo *mf = list->data;
		if(mf->fake_menu ||
		   mf->fr == NULL) {
			if(mf->fr)
				mf->fr = fr_replace(mf->fr);
			else
				mf->fr = fr_get_dir(mf->menudir);
			need_reread = TRUE;
		} else {
			FileRec *fr;
			fr = fr_check_and_reread(mf->fr);
			if(fr!=mf->fr ||
			   fr == NULL) {
				need_reread = TRUE;
				mf->fr = fr;
			}
		}
	}

	/*this no longer constitutes a bad hack, now it's purely cool :)*/
	if(need_reread) {
		while(GTK_MENU_SHELL(menuw)->children)
			gtk_widget_destroy(GTK_MENU_SHELL(menuw)->children->data);
		gtk_object_set_data(GTK_OBJECT(menuw), "mf",NULL);
		for(list = mfl; list != NULL;
		    list = g_slist_next(list)) {
			MenuFinfo *mf = list->data;

			menuw = create_menu_at_fr(menuw,
						  mf->fr,
						  mf->applets,
						  mf->dir_name,
						  mf->pixmap_name,
						  TRUE,
						  FALSE);
			destroy_mf(mf);
		}
		g_slist_free(mfl);

		gtk_menu_position(GTK_MENU(menuw));
	}
}

static void
rh_submenu_to_display(GtkWidget *menuw, GtkMenuItem *menuitem)
{
	struct stat s;
	int r;
	int do_read = FALSE;
	char *userrh;
	if(!g_file_exists(REDHAT_MENUDIR))
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



static GtkWidget *
create_fake_menu_at (char *menudir,
		     int applets,
		     char *dir_name,
		     char *pixmap_name)
{	
	MenuFinfo *mf;
	GtkWidget *menu;
	GSList *list;
	
	menu = gtk_menu_new ();
	
	mf = g_new0(MenuFinfo,1);
	mf->menudir = g_strdup(menudir);
	mf->applets = applets;
	mf->dir_name = dir_name?g_strdup(dir_name):NULL;
	mf->pixmap_name = pixmap_name?g_strdup(pixmap_name):NULL;
	mf->fake_menu = TRUE;
	mf->fr = NULL;
	
	list = g_slist_prepend(NULL,mf);
	gtk_object_set_data(GTK_OBJECT(menu),"mf",list);
	
	gtk_signal_connect(GTK_OBJECT(menu),"destroy",
			   GTK_SIGNAL_FUNC(menu_destroy),NULL);
	
	return menu;
}

static void
create_menuitem(GtkWidget *menu,
		FileRec *fr,
		int applets,
		int fake_submenus,
		int *add_separator,
		int *first_item)
{
	GtkWidget *menuitem, *sub, *pixmap;
	char *itemname;
	
	g_return_if_fail(fr != NULL);

	if(fr->type == FILE_REC_EXTRA)
		return;


	if(fr->type == FILE_REC_FILE && applets &&
	   !fr->goad_id) {
		g_warning(_("Can't get goad_id for applet, ignoring it"));
		return;
	}

	sub = NULL;
	if(fr->fullname) {
		itemname = g_strdup(fr->fullname);
	} else {
		char *p;
		itemname = g_strdup(g_basename(fr->name));
		p = strrchr(itemname,'.');
		if(p) *p = '\0';
	}

	if(fr->type == FILE_REC_DIR) {
		if(fake_submenus)
			sub = create_fake_menu_at (fr->name,
						   applets,
						   itemname,
						   fr->icon);
		else
			sub = create_menu_at_fr (NULL, fr,
						 applets,
						 itemname,
						 fr->icon,
						 fake_submenus,
						 FALSE);

		if (!sub) {
			g_free(itemname);
			return;
		}
	}

	menuitem = gtk_menu_item_new ();
	if (sub) {
		gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), sub);
		gtk_signal_connect(GTK_OBJECT(sub),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   menuitem);
	}

	pixmap = NULL;
	if (fr->icon && g_file_exists (fr->icon)) {
		pixmap = gnome_stock_pixmap_widget_at_size (NULL, fr->icon,
							    SMALL_ICON_SIZE,
							    SMALL_ICON_SIZE);
		if (pixmap)
			gtk_widget_show (pixmap);
	}

	if(!sub && applets)
		setup_applet_drag (menuitem, fr->goad_id);
	/*setup the menuitem, pass item_loc if this is not
	  a submenu or an applet, so that the item can be added,
	  we can be sure that the FileRec will live that long,
	  (when it dies, the menu will not be used again, it will
	   be recreated at the next available opportunity)*/
	setup_full_menuitem (menuitem, pixmap,itemname,
			     (applets||sub)?NULL:fr->name);

	if(*add_separator) {
		add_menu_separator(menu);
		(*first_item)++;
		*add_separator = FALSE;
	}
	gtk_menu_append (GTK_MENU (menu), menuitem);

	if(!sub) {
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    applets?
				    GTK_SIGNAL_FUNC(add_applet):
				    GTK_SIGNAL_FUNC(activate_app_def),
				    fr->name);
	}
	g_free(itemname);
}

static GtkWidget *
create_menu_at (GtkWidget *menu,
		char *menudir,
		int applets,
		char *dir_name,
		char *pixmap_name,
		int fake_submenus,
		int force)
{
	return create_menu_at_fr(menu,fr_get_dir(menudir),
				 applets,dir_name,pixmap_name,
				 fake_submenus,force);
}

static GtkWidget *
create_menu_at_fr (GtkWidget *menu,
		   FileRec *fr,
		   int applets,
		   char *dir_name,
		   char *pixmap_name,
		   int fake_submenus,
		   int force)
{	
	GSList *li;
	GSList *mfl = NULL;
	int add_separator = FALSE;
	int first_item = 0;
	GtkWidget *menuitem;
	MenuFinfo *mf = NULL;
	DirRec *dr = (DirRec *)fr;

	g_return_val_if_fail(!(fr&&fr->type!=FILE_REC_DIR),menu);
	
	if(!force && !fr)
		return menu;
	
	/*get this info ONLY if we haven't gotten it already*/
	if(!dir_name)
		dir_name = (fr&&fr->fullname)?fr->fullname:_("Menu");
	if(!pixmap_name)
		pixmap_name = (fr&&fr->icon)?fr->icon:gnome_folder;
	
	if(!menu) {
		menu = gtk_menu_new ();
		gtk_signal_connect(GTK_OBJECT(menu),"destroy",
				   GTK_SIGNAL_FUNC(menu_destroy),NULL);
	} else {
		first_item = g_list_length(GTK_MENU_SHELL(menu)->children);
		mfl = gtk_object_get_data(GTK_OBJECT(menu), "mf");
		if(GTK_MENU_SHELL(menu)->children)
			add_separator = TRUE;
	}
	
	if(fr) {
		for(li = dr->recs; li!=NULL; li=g_slist_next(li)) {
			create_menuitem(menu,li->data,
					applets,fake_submenus,
					&add_separator,
					&first_item);
		}
	}

	mf = g_new0(MenuFinfo,1);
	mf->menudir = g_strdup(fr->name);
	mf->applets = applets;
	mf->dir_name = dir_name?g_strdup(dir_name):NULL;
	mf->pixmap_name = pixmap_name?g_strdup(pixmap_name):NULL;
	mf->fake_menu = FALSE;
	mf->fr = fr;
	if(fr) {
		DirRec *dr = (DirRec *)fr;
		dr->mfl = g_slist_prepend(dr->mfl,mf);
	}
	
	if(!applets) {
		GtkWidget *pixmap;
		char *menu_name;
		/*if we actually added anything*/
		if(first_item < g_list_length(GTK_MENU_SHELL(menu)->children)) {
			menuitem = gtk_menu_item_new();
			gtk_menu_insert(GTK_MENU(menu),menuitem,first_item);
			gtk_widget_show(menuitem);
			menu_name = g_strdup(dir_name?dir_name:_("Menu"));
		} else {
			menu_name = g_strconcat(dir_name?dir_name:_("Menu"),_(" (empty)"),NULL);
		}


		pixmap = NULL;
		if (pixmap_name && g_file_exists (pixmap_name)) {
			pixmap = gnome_stock_pixmap_widget_at_size (NULL, pixmap_name,
								    SMALL_ICON_SIZE,
								    SMALL_ICON_SIZE);
			if (pixmap)
				gtk_widget_show (pixmap);
		} else if (gnome_folder && g_file_exists (gnome_folder)) {
			pixmap = gnome_stock_pixmap_widget_at_size (NULL, gnome_folder,
								    SMALL_ICON_SIZE,
								    SMALL_ICON_SIZE);
			if (pixmap)
				gtk_widget_show (pixmap);
		}

		menuitem = gtk_menu_item_new();
		setup_title_menuitem(menuitem,pixmap,menu_name,mf);
		gtk_menu_insert(GTK_MENU(menu),menuitem,first_item);
		
		g_free(menu_name);

		setup_directory_drag (menuitem, mf->menudir);

		/*add separator*/
		if(add_separator) {
			menuitem = gtk_menu_item_new();
			gtk_menu_insert(GTK_MENU(menu),menuitem,first_item);
			gtk_widget_show(menuitem);
			add_separator = FALSE;
		}
	}

	mfl = g_slist_append(mfl,mf);

	gtk_object_set_data(GTK_OBJECT(menu),"mf",mfl);
	
	return menu;
}

static void
destroy_menu (GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	GtkWidget *prop_dialog = gtk_object_get_data(GTK_OBJECT(menu->button),
						     MENU_PROPERTIES);
	if(prop_dialog)
		gtk_widget_destroy(prop_dialog);
	if(menu->menu)
		gtk_widget_destroy(menu->menu);
	g_free(menu->path);
	g_free(menu);
}

static void
menu_deactivate(GtkWidget *w, gpointer data)
{
	Menu *menu = data;
	GtkWidget *panel = get_panel_parent(menu->button);
	/* allow the panel to hide again */
	if(IS_BASEP_WIDGET(panel))
		BASEP_WIDGET(panel)->autohide_inhibit = FALSE;
	BUTTON_WIDGET(menu->button)->in_button = FALSE;
	BUTTON_WIDGET(menu->button)->ignore_leave = FALSE;
	button_widget_up(BUTTON_WIDGET(menu->button));
	menu->age = 0;
}

static char *
get_real_menu_path(char *arguments, char *menu_base)
{
	char *this_menu;
	
	g_return_val_if_fail(menu_base!=NULL,NULL);

	/*if null, let's put the main menu up*/
	if (!arguments || !*arguments)
		arguments = ".";

	if (*arguments == '/')
		this_menu = g_strdup (arguments);
	else
		this_menu = g_concat_dir_and_file (menu_base, arguments);

	if (!g_file_exists (this_menu)) {
		g_warning("menu %s does not exist "
			  "(base is %s, arguments are %s)",
			  this_menu, menu_base, arguments);
		g_free (this_menu);
		return NULL;
	}
	return this_menu;
}

static GtkWidget *
create_applets_menu(int fake_submenus)
{
	GtkWidget *applet_menu;
	char *menu_base = gnome_unconditional_datadir_file ("applets");
	char *menudir;

	menudir = g_concat_dir_and_file (menu_base, ".");
	g_free (menu_base);
	if (!g_file_exists (menudir)) {
		g_free (menudir);
		return NULL;
	}

	applet_menu = create_menu_at(NULL,menudir,TRUE,_("Applets"),NULL,
				     fake_submenus,FALSE);
	g_free (menudir);
	return applet_menu;
}

static void
find_empty_pos_array (int posscore[3][3])
{
	GSList *li;
	int i,j;
	PanelData *pd;
	BasePWidget *basep;
	
	gint16 tx, ty;
	guint16 w, h;
	gfloat sw, sw2, sh, sh2;

	sw2 = 2 * (sw = gdk_screen_width () / 3);
	sh2 = 2 * (sh = gdk_screen_height () / 3);
	
	for(li=panel_list;li!=NULL;li=g_slist_next(li)) {
		pd = li->data;
		basep = BASEP_WIDGET (pd->panel);

		if(IS_DRAWER_WIDGET(basep))
			continue;

		basep_widget_get_pos (basep, &tx, &ty);
		basep_widget_get_size (basep, &w, &h);

		if (PANEL_WIDGET (basep->panel)->orient == PANEL_HORIZONTAL) {
			j = MIN (ty / sh, 2);
			ty = tx + w;
			if (tx < sw) posscore[0][j]++;
			if (tx < sw2 && ty > sw) posscore[1][j]++;
			if (ty > sw2) posscore[2][j]++;
		} else {
			i = MIN (tx / sw, 2);
			tx = ty + h;
			if (ty < sh) posscore[i][0]++;
			if (ty < sh2 && tx > sh) posscore[i][1]++;
			if (tx > sh2) posscore[i][2]++;
		}
	}
}

static void
find_empty_pos (gint16 *x, gint16 *y)
{
	int posscore[3][3] = { {0,0,0}, {0,512,0}, {0,0,0}};
	int i, j, lowi= 0, lowj = 0;

	find_empty_pos_array (posscore);

	for(j=2;j>=0;j--) {
		for (i=0;i<3;i++) {
			if(posscore[i][j]<posscore[lowi][lowj]) {
				lowi = i;
				lowj = j;
			}
		}
	}

	*x = ((float)lowi * gdk_screen_width ()) / 2.0;
	*y = ((float)lowj * gdk_screen_height ()) / 2.0;
}

static BorderEdge
find_empty_edge ()
{
	int posscore[3][3] = { {0,0,0}, {0,512,0}, {0,0,0}};
	int escore [4] = { 0, 0, 0, 0};
	BorderEdge edge = BORDER_BOTTOM;
	int low=512, i;

	find_empty_pos_array (posscore);

	escore[BORDER_TOP] = posscore[0][0] + posscore[1][0] + posscore[2][0];
	escore[BORDER_RIGHT] = posscore[2][0] + posscore[2][1] + posscore[2][2];
	escore[BORDER_BOTTOM] = posscore[0][2] + posscore[1][2] + posscore[2][2];
	escore[BORDER_LEFT] = posscore[0][0] + posscore[0][1] + posscore[0][2];
	
	for (i=0; i<4; i++) {
		if (escore[i] < low) {
			edge = i;
			low = escore[i];
		}
	}
	return edge;
}

static void
create_new_panel(GtkWidget *w,gpointer data)
{
	PanelType type = GPOINTER_TO_INT(data);
	GdkColor bcolor = {0,0,0,1};
	gint16 x, y;
	GtkWidget *panel=NULL;
    
	g_return_if_fail (type != DRAWER_PANEL);

	switch(type) {
	case ALIGNED_PANEL: 
		find_empty_pos (&x, &y);
		panel = aligned_widget_new(ALIGNED_LEFT,
					   BORDER_TOP,
					   BASEP_EXPLICIT_HIDE,
					   BASEP_SHOWN,
					   SIZE_STANDARD,
					   TRUE,
					   TRUE,
					   PANEL_BACK_NONE,
					   NULL,
					   TRUE,
					   &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);
		break;
	case EDGE_PANEL: 
		panel = edge_widget_new(find_empty_edge (),
					BASEP_EXPLICIT_HIDE,
					BASEP_SHOWN,
					SIZE_STANDARD,
					TRUE,
					TRUE,
					PANEL_BACK_NONE,
					NULL,
					TRUE,
					&bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);	
		break;
	case SLIDING_PANEL:
		find_empty_pos (&x, &y);
		panel = sliding_widget_new (SLIDING_ANCHOR_LEFT, 0,
					    BORDER_TOP,
					    BASEP_EXPLICIT_HIDE,
					    BASEP_SHOWN,
					    SIZE_STANDARD,
					    TRUE, TRUE,
					    PANEL_BACK_NONE,
					    NULL, TRUE,
					    &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);	
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);
		break;
	default: break;
	}

	if (panel == NULL)
		return;
		
	panels_to_sync = TRUE;
}

static GtkWidget *
create_add_panel_submenu (void)
{
	GtkWidget *menu, *menuitem;

	menu = gtk_menu_new ();
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Edge panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(EDGE_PANEL));

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Aligned panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(ALIGNED_PANEL));

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Sliding panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(SLIDING_PANEL));
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Free panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(FREE_PANEL));

	return menu;
}

static GtkWidget *
create_system_menu(GtkWidget *menu, int fake_submenus, int fake)
{
	char *menu_base = gnome_unconditional_datadir_file ("gnome/apps");
	char *menudir;
	
	menudir = g_concat_dir_and_file (menu_base, ".");
	g_free (menu_base);
	if (g_file_exists (menudir)) {
		if(!fake || menu) {
			menu = create_menu_at (menu,menudir,FALSE,_("Programs"),
					       gnome_pixmap_file ("gnome-logo-icon-transparent.png"),
					       fake_submenus, FALSE);
		} else {
			menu = create_fake_menu_at (menudir, FALSE,
						    _("Programs"),
						    gnome_pixmap_file ("gnome-logo-icon-transparent.png"));
		}
		g_return_val_if_fail(menu,NULL);
		g_free (menudir);
	} else {
		g_warning("No system menus found!");
	}
	
	return menu;
}

static GtkWidget *
create_user_menu(char *title, char *dir, GtkWidget *menu, int fake_submenus,
		 int force, int fake)
{
	char *menu_base = gnome_util_home_file (dir);
	char *menudir = g_concat_dir_and_file (menu_base, ".");
	
	if (!g_file_exists (menudir))
		mkdir (menu_base, 0755);
	g_free (menu_base);
	if(!fake || menu) {
		menu = create_menu_at (menu,menudir, FALSE,
				       title,
				       NULL,fake_submenus,
				       force);
	} else {
		menu = create_fake_menu_at (menudir, FALSE,
					    title,NULL);
	}
	g_free (menudir); 
	return menu;
}

static GtkWidget *
create_debian_menu(GtkWidget *menu, int fake_submenus, int fake)
{
	if (!fake || menu) {
		menu = create_menu_at (menu, DEBIAN_MENUDIR, FALSE,
				       _("Debian menus"), NULL,
				       fake_submenus, FALSE);
	} else {
		menu = create_fake_menu_at (DEBIAN_MENUDIR, FALSE,
					    _("Debian menus"), NULL);
	}

	return menu;
}

static GtkWidget *
create_kde_menu(GtkWidget *menu, int fake_submenus,
		int force, int fake)
{
	char *pixmap_name = g_concat_dir_and_file (KDE_ICONDIR, "exec.xpm");
	if(!fake || menu) {
		menu = create_menu_at (menu, 
				       KDE_MENUDIR, FALSE,
				       _("KDE menus"), 
				       pixmap_name,
				       fake_submenus,
				       force);
	} else {
		menu = create_fake_menu_at (KDE_MENUDIR, FALSE,
					    _("KDE menus"),
					    pixmap_name);
	}
	g_free (pixmap_name);
	return menu;
}

GtkWidget *
create_panel_root_menu(GtkWidget *panel)
{
	GtkWidget *menuitem;
	GtkWidget *panel_menu;
	GtkWidget *menu;

	panel_menu = gtk_menu_new();

	menu = create_system_menu(NULL,TRUE,TRUE);
	if(menu) {
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, 
				gnome_pixmap_new_from_file_at_size (
					gnome_pixmap_file ("gnome-logo-icon-transparent.png"),
					SMALL_ICON_SIZE, SMALL_ICON_SIZE),
				_("Programs"));
		gtk_menu_append (GTK_MENU (panel_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),menu);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   menuitem);
	}

	menu = create_user_menu(_("Favorites"),"apps",NULL,TRUE,TRUE,TRUE);
	if(menu) {
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, 0, _("Favorites"));
		gtk_menu_append (GTK_MENU (panel_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),menu);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   menuitem);
	}

	if(g_file_exists(REDHAT_MENUDIR)) {
		menu = create_user_menu(_("AnotherLevel menus"),"apps-redhat",
					NULL,TRUE,TRUE,TRUE);
		if(menu) {
			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0, _("AnotherLevel menus"));
			gtk_menu_append (GTK_MENU (panel_menu), menuitem);
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
						   menu);
			gtk_signal_connect(GTK_OBJECT(menu),"show",
					   GTK_SIGNAL_FUNC(rh_submenu_to_display),
					   menuitem);
			gtk_signal_connect(GTK_OBJECT(menu),"show",
					   GTK_SIGNAL_FUNC(submenu_to_display),
					   menuitem);
		}
	}

	if (g_file_exists (DEBIAN_MENUDIR)) {
		menu = create_debian_menu(NULL,TRUE,TRUE);
		if(menu) {
			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0, _("Debian menus"));
			gtk_menu_append (GTK_MENU (panel_menu), menuitem);
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),menu);
			gtk_signal_connect(GTK_OBJECT(menu),"show",
					   GTK_SIGNAL_FUNC(submenu_to_display),
					   menuitem);
		}
	}

	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	make_panel_submenu(panel_menu,TRUE);

	add_menu_separator (panel_menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_stock_pixmap_widget (panel_menu,
						   GNOME_STOCK_PIXMAP_REMOVE),
			_("Remove this panel"));
	gtk_menu_append (GTK_MENU (panel_menu), menuitem);
	gtk_signal_connect_object (GTK_OBJECT (menuitem), "activate",
				   GTK_SIGNAL_FUNC(gtk_widget_destroy),
				   GTK_OBJECT(panel));
	gtk_object_set_data(GTK_OBJECT(panel_menu),"remove_item",menuitem);

#if 0
	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);
#endif

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(panel_menu,
						  GNOME_STOCK_PIXMAP_QUIT),
			_("Log out"));
	gtk_menu_append (GTK_MENU (panel_menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_quit),
			    NULL);

	return panel_menu;
}


static void
current_panel_config(GtkWidget *w, gpointer data)
{
	GtkWidget *parent = gtk_object_get_data(GTK_OBJECT(current_panel),
						PANEL_PARENT);
	panel_config(parent);
}

static void
ask_about_launcher_cb(GtkWidget *w, gpointer data)
{
	ask_about_launcher(NULL,current_panel,0);
}

static void
ask_about_swallowing_cb(GtkWidget *w, gpointer data)
{
	ask_about_swallowing(current_panel,0);
}

static void
convert_setup (BasePWidget *basep, GtkType type)
{
	basep->pos = gtk_type_new (type);
	basep->pos->basep = basep;
	basep_widget_pre_convert_hook (basep);
	basep_pos_connect_signals (basep);
}

static void
convert_to_panel(GtkWidget *widget, gpointer data)
{
	PanelType type = GPOINTER_TO_INT(data);
	PanelData *pd;
	gint16 x, y;
	guint16 w, h;
	BasePWidget *basep;
	BasePPos *old_pos;
	PanelWidget *panel;

	g_return_if_fail(current_panel != NULL);
	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;
	basep = gtk_object_get_data(GTK_OBJECT(current_panel),"panel_parent");
	
	g_return_if_fail (IS_BASEP_WIDGET (basep));
	panel = PANEL_WIDGET (basep->panel);

	pd = gtk_object_get_user_data (GTK_OBJECT (basep));
	if (pd->type == type)
		return;

	basep_widget_get_pos (basep, &x, &y);
	basep_widget_get_size (basep, &w, &h);

	old_pos = basep->pos;
	old_pos->basep = NULL;
	pd->type = type;

	/* for now, just ignore non-border types */
	switch (type) {
	case EDGE_PANEL: 
	{
		BorderEdge edge = BORDER_BOTTOM;
		convert_setup (basep, EDGE_POS_TYPE);

		if (IS_BORDER_POS (old_pos))
			edge = BORDER_POS (old_pos)->edge;

		border_widget_change_edge (BORDER_WIDGET (basep), edge);
		break;
	}
	case ALIGNED_PANEL: 
	{
		gint mid, max;
		BorderEdge edge = BORDER_BOTTOM;
		AlignedAlignment align;

		convert_setup (basep, ALIGNED_POS_TYPE);

		if (IS_BORDER_POS (old_pos))
			edge = BORDER_POS (old_pos)->edge;
		if (PANEL_WIDGET (panel)->orient == PANEL_HORIZONTAL) {
			mid = x + w / 2;
			max = gdk_screen_width ();
		} else {
			mid = y + h / 2;
			max = gdk_screen_height ();
		}
	
		if (mid < max / 3)
			align = ALIGNED_LEFT;
		else if (mid < 2 * (max / 3))
			align = ALIGNED_CENTER;
		else
			align = ALIGNED_RIGHT;
		aligned_widget_change_align_edge (
			ALIGNED_WIDGET (basep), align, edge);
		break;
	}
	case SLIDING_PANEL:
	{
		gint val, max;
		BorderEdge edge = BORDER_BOTTOM;
		SlidingAnchor anchor;
		gint16 offset;
		
		convert_setup (basep, SLIDING_POS_TYPE);
		
		if (IS_BORDER_POS (old_pos))
			edge = BORDER_POS (old_pos)->edge;
		
		if (PANEL_WIDGET (panel)->orient == PANEL_HORIZONTAL) {
			val = x;
			max = gdk_screen_width ();
		} else {
			val = y;
			max = gdk_screen_height ();
		}
		
		if (val > 0.9 * max) {
			offset = max - val;
			anchor = SLIDING_ANCHOR_RIGHT;
		} else {
			offset = val;
			anchor = SLIDING_ANCHOR_LEFT;
		}

		sliding_widget_change_anchor_offset_edge (
			SLIDING_WIDGET (basep), anchor, offset, edge);
		
		break;
	}
	default:
		g_assert_not_reached ();
		break;
	}

	gtk_object_unref (GTK_OBJECT (old_pos));
	gtk_widget_queue_resize (GTK_WIDGET (basep));
}

static void
change_hiding_mode (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep;

	g_return_if_fail(current_panel != NULL);
	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	basep = gtk_object_get_data(GTK_OBJECT(current_panel),"panel_parent");
	g_return_if_fail (IS_BASEP_WIDGET (basep));
	
	basep_widget_change_params (basep,
				    current_panel->orient,
				    current_panel->sz,
				    GPOINTER_TO_INT (data),
				    basep->state,
				    basep->hidebuttons_enabled,
				    basep->hidebutton_pixmaps_enabled,
				    current_panel->back_type,
				    current_panel->back_pixmap,
				    current_panel->fit_pixmap_bg,
				    &current_panel->back_color);
}

static void
change_size (GtkWidget *widget, gpointer data)
{
	g_return_if_fail(current_panel != NULL);
	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	panel_widget_change_params (current_panel,
				    current_panel->orient,
				    GPOINTER_TO_INT (data),
				    current_panel->back_type,
				    current_panel->back_pixmap,
				    current_panel->fit_pixmap_bg,
				    &current_panel->back_color);
}

static void
change_background (GtkWidget *widget, gpointer data)
{
	g_return_if_fail(current_panel != NULL);

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	panel_widget_change_params (current_panel,
				    current_panel->orient,
				    current_panel->sz,
				    GPOINTER_TO_INT (data),
				    current_panel->back_pixmap,
				    current_panel->fit_pixmap_bg,
				    &current_panel->back_color);
}

static void
change_hidebuttons (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep;
	gboolean hidebutton_pixmaps_enabled, hidebuttons_enabled;
	g_return_if_fail(current_panel != NULL);

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	basep = gtk_object_get_data(GTK_OBJECT(current_panel),"panel_parent");
	g_return_if_fail (IS_BASEP_WIDGET (basep));

	hidebuttons_enabled = basep->hidebuttons_enabled;
	hidebutton_pixmaps_enabled = basep->hidebutton_pixmaps_enabled;

	switch (GPOINTER_TO_INT (data)) {
	case HIDEBUTTONS_NONE:
		hidebuttons_enabled = FALSE;
		break;
	case HIDEBUTTONS_PLAIN:
		hidebutton_pixmaps_enabled = FALSE;
		hidebuttons_enabled = TRUE;
		break;
	case HIDEBUTTONS_PIXMAP:
		hidebutton_pixmaps_enabled = TRUE;
		hidebuttons_enabled = TRUE;
		break;
	}

	basep_widget_change_params (basep,
				    current_panel->orient,
				    current_panel->sz,
				    basep->mode,
				    basep->state,
				    hidebuttons_enabled,
				    hidebutton_pixmaps_enabled,
				    current_panel->back_type,
				    current_panel->back_pixmap,
				    current_panel->fit_pixmap_bg,
				    &current_panel->back_color);
}

#if 0
void
show_x_on_panels(GtkWidget *menu)
{
	GtkWidget *pw;
	GtkWidget *convcorn = gtk_object_get_data(GTK_OBJECT(menu),"convcorn");
	GtkWidget *convsnap = gtk_object_get_data(GTK_OBJECT(menu),"convsnap");
	g_return_if_fail(current_panel != NULL);
	g_return_if_fail(convcorn != NULL);
	g_return_if_fail(convsnap != NULL);
	
	pw = gtk_object_get_data(GTK_OBJECT(current_panel),"panel_parent");
	g_return_if_fail(pw != NULL);
	
	if(IS_SNAPPED_WIDGET(pw)) {
		gtk_widget_show(convcorn);
		gtk_widget_hide(convsnap);
	} else if(IS_CORNER_WIDGET(pw)) {
		gtk_widget_hide(convcorn);
		gtk_widget_show(convsnap);
	} else {
		gtk_widget_hide(convcorn);
		gtk_widget_hide(convsnap);
	}
}
#endif

static void
update_type_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	GtkWidget *basep = gtk_object_get_data(GTK_OBJECT(current_panel),
					       PANEL_PARENT);
	if (IS_EDGE_WIDGET (basep))
		s = MENU_TYPE_EDGE;
	else if (IS_ALIGNED_WIDGET (basep))
		s = MENU_TYPE_ALIGNED;
	else if (IS_SLIDING_WIDGET (basep))
		s = MENU_TYPE_SLIDING;
	else 
		return;
	
	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);				 
	
	if (menuitem)
		gtk_check_menu_item_set_active (
			GTK_CHECK_MENU_ITEM (menuitem), TRUE);
}

static void
update_size_menu (GtkWidget *menu, gpointer data)
{
	GtkWidget *menuitem = NULL;
	char *s = NULL;
	switch (current_panel->sz) {
	case SIZE_TINY:
		s = MENU_SIZE_TINY;
		break;
	case SIZE_STANDARD:
		s = MENU_SIZE_STANDARD;
		break;
	case SIZE_LARGE:
		s = MENU_SIZE_LARGE;
		break;
	case SIZE_HUGE:
		s = MENU_SIZE_HUGE;
		break;
	default:
		return;
	}

	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
}


static void
update_back_menu (GtkWidget *menu, gpointer data)
{
	GtkWidget *menuitem = NULL;
	char *s = NULL;
	switch (current_panel->back_type) {
	case PANEL_BACK_NONE:
		s = MENU_BACK_NONE;
		break;
	case PANEL_BACK_COLOR:
		s = MENU_BACK_COLOR;
		break;
	case PANEL_BACK_PIXMAP:
		s = MENU_BACK_PIXMAP;
		break;
	default:
		return;
	}

	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
	menuitem = gtk_object_get_data (GTK_OBJECT (menu), MENU_BACK_PIXMAP);
	gtk_widget_set_sensitive (menuitem, current_panel->back_pixmap != NULL);
}

static void
update_hidebutton_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	BasePWidget *basep = gtk_object_get_data(GTK_OBJECT(current_panel),
						 "panel_parent");

	if (!basep->hidebuttons_enabled)
		s = MENU_HIDEBUTTONS_NONE;
	else if (basep->hidebutton_pixmaps_enabled)
		s = MENU_HIDEBUTTONS_PIXMAP;
	else 
		s = MENU_HIDEBUTTONS_PLAIN;
	
	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
}

static void
update_hiding_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	BasePWidget *basep = gtk_object_get_data(GTK_OBJECT(current_panel),
					       "panel_parent");
	s =  (basep->mode == BASEP_EXPLICIT_HIDE)
		? MENU_MODE_EXPLICIT_HIDE
		: MENU_MODE_AUTO_HIDE;

	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
}

typedef struct {
	char *s;
	int i;
} StringEnumPair;

static void
add_radios_to_menu (GtkWidget *menu, StringEnumPair *items,
		    GtkSignalFunc func)
{
	int i;
	GSList *radio_group = NULL;
	GtkWidget *menuitem;

	for (i=0; items[i].s; i++) {
		menuitem = gtk_radio_menu_item_new (radio_group);
		radio_group = gtk_radio_menu_item_group (
			GTK_RADIO_MENU_ITEM (menuitem));
		setup_menuitem (menuitem, NULL, _(items[i].s));
		gtk_menu_append (GTK_MENU (menu), 
				 menuitem);
		gtk_object_set_data (GTK_OBJECT (menu),
				     items[i].s, menuitem);
		gtk_check_menu_item_set_show_toggle (
			GTK_CHECK_MENU_ITEM (menuitem), TRUE);
		gtk_check_menu_item_set_active (
			GTK_CHECK_MENU_ITEM (menuitem), FALSE);
		gtk_signal_connect (GTK_OBJECT (menuitem), "toggled",
				    GTK_SIGNAL_FUNC (func),
				    GINT_TO_POINTER (items[i].i));
	}
}

static void
add_radio_menu (GtkWidget *menu, char *menutext, 
		StringEnumPair *items, char *menu_key,
		GtkSignalFunc change_func,
		GtkSignalFunc update_func)
{
	GtkWidget *menuitem;
	GtkWidget *submenu;

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, NULL, menutext);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_object_set_data (GTK_OBJECT (menu), menu_key, menuitem);

	submenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
	add_radios_to_menu (submenu, items, change_func);
	gtk_signal_connect (GTK_OBJECT (submenu), "show",
			    GTK_SIGNAL_FUNC (update_func),
			    NULL);
	
}

static void
make_properties_submenu (GtkWidget *menu)
{
	StringEnumPair types[] = { { N_(MENU_TYPE_EDGE), EDGE_PANEL },
				   { N_(MENU_TYPE_ALIGNED), ALIGNED_PANEL },
				   { N_(MENU_TYPE_SLIDING), SLIDING_PANEL },
				   { NULL, -1 } };
	
	StringEnumPair modes[] = { { N_(MENU_MODE_EXPLICIT_HIDE), BASEP_EXPLICIT_HIDE },
				   { N_(MENU_MODE_AUTO_HIDE), BASEP_AUTO_HIDE },
				   { NULL, -1 } };

	StringEnumPair hidebuttons[] = { { N_(MENU_HIDEBUTTONS_PIXMAP), HIDEBUTTONS_PIXMAP },
				       { N_(MENU_HIDEBUTTONS_PLAIN), HIDEBUTTONS_PLAIN },
				       { N_(MENU_HIDEBUTTONS_NONE), HIDEBUTTONS_NONE },
				       { NULL, -1 } };

	StringEnumPair sizes[] = { { N_(MENU_SIZE_TINY), SIZE_TINY },
				   { N_(MENU_SIZE_STANDARD), SIZE_STANDARD },
				   { N_(MENU_SIZE_LARGE), SIZE_LARGE },
				   { N_(MENU_SIZE_HUGE), SIZE_HUGE },
				   { NULL, -1 } };

	StringEnumPair backgrounds[] = { { N_(MENU_BACK_NONE), PANEL_BACK_NONE },
					 { N_(MENU_BACK_COLOR), PANEL_BACK_COLOR },
					 { N_(MENU_BACK_PIXMAP), PANEL_BACK_PIXMAP },
					 { NULL, -1 } };

	add_radio_menu (menu, _("Type"), types, MENU_TYPES,
			convert_to_panel, update_type_menu);

	add_radio_menu (menu, _("Hiding policy"), modes, MENU_MODES,
			change_hiding_mode, update_hiding_menu);

	add_radio_menu (menu, _("Hide buttons"), hidebuttons, MENU_HIDEBUTTONS,
			change_hidebuttons, update_hidebutton_menu);

	add_radio_menu (menu, _("Size"), sizes, MENU_SIZES,
			change_size, update_size_menu);

	add_radio_menu (menu, _("Background type"), backgrounds, MENU_BACKS,
			change_background, update_back_menu);
}

static void
make_add_submenu (GtkWidget *menu, int fake_submenus)
{
	GtkWidget *menuitem, *submenu, *submenuitem, *m;

	/* Add Menu */
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_stock_pixmap_widget (menu,
						   GNOME_STOCK_MENU_EXEC),
			_("Applet"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	m = create_applets_menu(fake_submenus);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),m);
	gtk_signal_connect(GTK_OBJECT(m),"show",
			   GTK_SIGNAL_FUNC(submenu_to_display),
			   menuitem);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_pixmap_new_from_file_at_size (
				gnome_pixmap_file ("gnome-gmenu.png"),
				SMALL_ICON_SIZE, SMALL_ICON_SIZE),
			_("Menu"));
	gtk_menu_append (GTK_MENU (menu), menuitem);

	submenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);

	submenuitem = gtk_menu_item_new ();
	setup_menuitem (submenuitem, 0, _("Main menu"));
	gtk_menu_append (GTK_MENU (submenu), submenuitem);
	gtk_signal_connect(GTK_OBJECT(submenuitem), "activate",
			   GTK_SIGNAL_FUNC(add_menu_to_panel),
			   NULL);
	setup_internal_applet_drag(submenuitem, "MENU:MAIN");


	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_pixmap_new_from_file_at_size (
					gnome_pixmap_file ("launcher-program.png"),
					SMALL_ICON_SIZE, SMALL_ICON_SIZE),
			_("Launcher"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(ask_about_launcher_cb),NULL);
	setup_internal_applet_drag(menuitem, "LAUNCHER:ASK");

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_pixmap_new_from_file_at_size (
				gnome_pixmap_file ("panel-drawer.png"),
				SMALL_ICON_SIZE, SMALL_ICON_SIZE),
			_("Drawer"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_drawer_to_panel,
			   NULL);
	setup_internal_applet_drag(menuitem, "DRAWER:NEW");

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_pixmap_new_from_file_at_size (
				gnome_pixmap_file ("gnome-term-night.png"),
				SMALL_ICON_SIZE, SMALL_ICON_SIZE),
			_("Log out button"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(add_logout_to_panel),
			   NULL);
	setup_internal_applet_drag(menuitem, "LOGOUT:NEW");
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_pixmap_new_from_file_at_size (
				gnome_pixmap_file ("gnome-lockscreen.png"),
				SMALL_ICON_SIZE, SMALL_ICON_SIZE),
			_("Lock button"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(add_lock_to_panel),
			   NULL);
	setup_internal_applet_drag(menuitem, "LOCK:NEW");

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Swallowed app"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(ask_about_swallowing_cb),NULL);
	setup_internal_applet_drag(menuitem, "SWALLOW:ASK");
}

void
make_panel_submenu (GtkWidget *menu, int fake_submenus)
{
	GtkWidget *menuitem, *submenu, *submenuitem;

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget (menu,
						   GNOME_STOCK_PIXMAP_ADD),
			_("Add to panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);

	submenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   submenu);
	make_add_submenu (submenu, fake_submenus);

        menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_stock_pixmap_widget (menu,
						   GNOME_STOCK_MENU_NEW),
			_("Create panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   create_add_panel_submenu());

	add_menu_separator(menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_stock_pixmap_widget (menu,
						   GNOME_STOCK_MENU_PROP),
			_("Properties"));
	gtk_menu_append (GTK_MENU (menu), menuitem);

	submenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);

	submenuitem = gtk_menu_item_new ();
	setup_menuitem (submenuitem,
			gnome_stock_pixmap_widget(submenu,
						  GNOME_STOCK_MENU_PROP),
			_("Panel..."));

	gtk_menu_append (GTK_MENU (submenu), submenuitem);
	gtk_signal_connect (GTK_OBJECT (submenuitem), "activate",
			    GTK_SIGNAL_FUNC(current_panel_config), 
			    NULL);

	add_menu_separator (submenu);
	make_properties_submenu (submenu);
	

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_MENU_PREF),
			_("Global Preferences..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_config_global), 
			    NULL);

#if 0
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Convert to corner panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) convert_to_panel,
			   GINT_TO_POINTER(CORNER_PANEL));
	gtk_object_set_data(GTK_OBJECT(menu),"convcorn",menuitem);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Convert to edge panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) convert_to_panel,
			   GINT_TO_POINTER(SNAPPED_PANEL));
	gtk_object_set_data(GTK_OBJECT(menu),"convsnap",menuitem);

	add_menu_separator(menu);
#endif
}

void
panel_lock (GtkWidget *widget, gpointer data)
{
	gboolean lock = gnome_config_get_bool_with_default ("Screensaver/Default/password", FALSE);
	/* we want to default to something safe.
	 * Have we started xscreensaver in a locked state or not?*/
	if (lock)
		system ("(xscreensaver-command -activate&)");
	else
		system ("(xscreensaver-command -lock&)");
}

static void
add_special_entries (GtkWidget *menu, int fake_submenus)
{
	GtkWidget *menuitem;
	GtkWidget *panel_menu;
	char *char_tmp;

	/* Panel entry */

	add_menu_separator (menu);

	panel_menu = gtk_menu_new();
	make_panel_submenu(panel_menu,fake_submenus);
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),panel_menu);
	
#if 0
	gtk_signal_connect_object(GTK_OBJECT(menu),"show",
				  GTK_SIGNAL_FUNC(show_x_on_panels),
				  GTK_OBJECT(panel_menu));
#endif
	add_menu_separator (menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_PIXMAP_ABOUT),
			_("About..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(about_cb),
			    NULL);

	char_tmp = gnome_is_program_in_path("guname");
	if(char_tmp) {
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem,
				gnome_stock_pixmap_widget(menu,
							  GNOME_STOCK_PIXMAP_ABOUT),
				_("About GNOME..."));
		gtk_menu_append (GTK_MENU (menu), menuitem);
		gtk_signal_connect_full(GTK_OBJECT (menuitem), "activate",
					GTK_SIGNAL_FUNC(about_gnome_cb),NULL,
					char_tmp, (GtkDestroyNotify)g_free,
					FALSE,TRUE);
	}

	add_menu_separator (menu);

	char_tmp = gnome_is_program_in_path ("xscreensaver");
	if (char_tmp) {	
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, 
				gnome_pixmap_new_from_file_at_size (
					gnome_pixmap_file ("gnome-lockscreen.png"),
					SMALL_ICON_SIZE, SMALL_ICON_SIZE),
				_("Lock screen"));
		gtk_menu_append (GTK_MENU (menu), menuitem);
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC(panel_lock), 0);
	}
	g_free (char_tmp);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_MENU_QUIT),
			_("Log out"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_quit), 0);

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
				if(g_file_exists(s))
					rh->u.item.icon = s;
				else {
					g_free(s);
					rh->u.item.icon = NULL;
				}
			} else {
				rh->u.item.icon = g_concat_dir_and_file("/usr/share/icons", s);
				if(!g_file_exists(rh->u.item.icon)) {
					g_free(rh->u.item.icon);
					rh->u.item.icon = gnome_pixmap_file(s);
				}
				g_free(s);
			}
		} else if(strcmp(w,"mini-icon")==0) {
			char *s = read_word(&p);
			g_free(rh->u.item.mini_icon);
			if(*s == '/') {
				if(g_file_exists(s))
					rh->u.item.mini_icon = s;
				else {
					g_free(s);
					rh->u.item.mini_icon = NULL;
				}
			} else {
				rh->u.item.mini_icon = g_concat_dir_and_file("/usr/share/icons/mini", s);
				if(!g_file_exists(rh->u.item.mini_icon)) {
					g_free(rh->u.item.mini_icon);
					rh->u.item.mini_icon = gnome_pixmap_file(s);
				}
				g_free(s);
			}
		} else if(strcmp(w,"exec")==0) {
			g_free(rh->u.item.exec);
			rh->u.item.exec = read_word(&p);
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
			if(fp) fprintf(fp,"%s\n",g_basename(p));
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


static void
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
			if(fork()==0) {
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

static GtkWidget *
create_root_menu(int fake_submenus, int flags)
{
	GtkWidget *root_menu;
	GtkWidget *menu;
	GtkWidget *menuitem;
	int need_separ = FALSE;
	
	root_menu = NULL;
	
	if(flags&MAIN_MENU_SYSTEM && !(flags&MAIN_MENU_SYSTEM_SUB)) {
		root_menu = create_system_menu(root_menu,fake_submenus,FALSE);
		need_separ = TRUE;
	}
	if(flags&MAIN_MENU_USER && !(flags&MAIN_MENU_USER_SUB)) {
		root_menu = create_user_menu(_("Favorites"), "apps",
					     root_menu, fake_submenus, FALSE,
					     FALSE);
		need_separ = TRUE;
	}
	if(flags&MAIN_MENU_REDHAT && !(flags&MAIN_MENU_REDHAT_SUB)) {
		rh_submenu_to_display(NULL,NULL);
		root_menu = create_user_menu(_("AnotherLevel menus"), "apps-redhat",
					     root_menu, fake_submenus, FALSE,
					     FALSE);
		need_separ = TRUE;
	}
	if(flags&MAIN_MENU_DEBIAN && !(flags&MAIN_MENU_DEBIAN_SUB)) {
		root_menu = create_debian_menu(root_menu, fake_submenus, FALSE);
		need_separ = TRUE;
	}
	if(flags&MAIN_MENU_KDE && !(flags&MAIN_MENU_KDE_SUB)) {
		root_menu = create_kde_menu(root_menu,fake_submenus,FALSE, FALSE);
		need_separ = TRUE;
	}
	/*others here*/
	
	if(!root_menu)
		root_menu = gtk_menu_new();

	if(flags&MAIN_MENU_SYSTEM && flags&MAIN_MENU_SYSTEM_SUB) {
		if(need_separ)
			add_menu_separator(root_menu);
		need_separ = FALSE;
		menu = create_system_menu(NULL,fake_submenus, TRUE);
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, 
				gnome_pixmap_new_from_file_at_size (
					gnome_pixmap_file ("gnome-logo-icon-transparent.png"),
					SMALL_ICON_SIZE, SMALL_ICON_SIZE),
				_("Programs"));
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		if(menu) {
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
						   menu);
			gtk_signal_connect(GTK_OBJECT(menu),"show",
					   GTK_SIGNAL_FUNC(submenu_to_display),
					   menuitem);
		}
	}
	if(flags&MAIN_MENU_USER && flags&MAIN_MENU_USER_SUB) {
		if(need_separ)
			add_menu_separator(root_menu);
		need_separ = FALSE;
		menu = create_user_menu(_("Favorites"), "apps", 
					NULL, fake_submenus, TRUE, TRUE);
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, 0, _("Favorites"));
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   menuitem);
	}
	if(flags&MAIN_MENU_REDHAT && flags&MAIN_MENU_REDHAT_SUB) {
		GtkWidget *pixmap = NULL;
		if(need_separ)
			add_menu_separator(root_menu);
		need_separ = FALSE;
		menu = create_user_menu(_("AnotherLevel menus"), "apps-redhat", 
					NULL, fake_submenus, TRUE, TRUE);
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, pixmap, _("AnotherLevel menus"));
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(rh_submenu_to_display),
				   menuitem);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   menuitem);
	}
	if(flags&MAIN_MENU_DEBIAN && flags&MAIN_MENU_DEBIAN_SUB) {
		if(need_separ)
			add_menu_separator(root_menu);
		need_separ = FALSE;
		menu = create_debian_menu(NULL,fake_submenus, TRUE);
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, 0, _("Debian menus"));
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		if(menu) {
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
						   menu);
			gtk_signal_connect(GTK_OBJECT(menu),"show",
					   GTK_SIGNAL_FUNC(submenu_to_display),
					   menuitem);
		}
	}
	if(flags&MAIN_MENU_KDE && flags&MAIN_MENU_KDE_SUB) {
		GtkWidget *pixmap = NULL;
		char *pixmap_path;
		if(need_separ)
			add_menu_separator(root_menu);
		need_separ = FALSE;
		menu = create_kde_menu(NULL, fake_submenus, TRUE, TRUE);
		pixmap_path = g_concat_dir_and_file (KDE_MINI_ICONDIR, "exec.xpm");
		if (g_file_exists(pixmap_path)) {
			pixmap = gnome_stock_pixmap_widget_at_size (NULL, pixmap_path,
								    SMALL_ICON_SIZE,
								    SMALL_ICON_SIZE);
			if (pixmap)
				gtk_widget_show (pixmap);
		}
		g_free (pixmap_path);
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, pixmap, _("KDE menus"));
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   menuitem);
	}
	add_special_entries (root_menu, fake_submenus);
	
	return root_menu;
}

static void
add_menu_widget (Menu *menu, GSList *menudirl, int main_menu, int fake_subs)
{
	GSList *li;
	if(menu->menu)
		gtk_widget_destroy(menu->menu);

	if (main_menu)
		menu->menu = create_root_menu(fake_subs, menu->main_menu_flags);
	else {
		menu->menu = NULL;
		for(li=menudirl;li!=NULL;li=g_slist_next(li))
			menu->menu = create_menu_at (menu->menu,li->data,
						     FALSE, NULL, NULL,
						     fake_subs, FALSE);
		if(!menu->menu) {
			g_warning("Can't create menu, using main menu!");
			menu->menu = create_root_menu(fake_subs,
						      menu->main_menu_flags);
		}
	}
	gtk_signal_connect (GTK_OBJECT (menu->menu), "deactivate",
			    GTK_SIGNAL_FUNC (menu_deactivate), menu);
}

static void
menu_button_pressed(GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	GdkEventButton *bevent = (GdkEventButton*)gtk_get_current_event();
	GtkWidget *wpanel = get_panel_parent(menu->button);
	int main_menu = (strcmp (menu->path, ".") == 0);

	/*this HAS to be set everytime we popup the menu*/
	current_panel = PANEL_WIDGET(menu->button->parent);

	if(!menu->menu) {
		char *menu_base = gnome_unconditional_datadir_file ("gnome/apps");
		char *this_menu = get_real_menu_path(menu->path,menu_base);
		GSList *list = g_slist_append(NULL,this_menu);
		
		add_menu_widget(menu,list, strcmp(menu->path,".")==0, TRUE);
		
		g_free(menu_base);
		g_free(this_menu);

		g_slist_free(list);
	} else {
		if(menu->main_menu_flags&MAIN_MENU_REDHAT &&
		   !(menu->main_menu_flags&MAIN_MENU_REDHAT_SUB))
			rh_submenu_to_display(NULL,NULL);

		check_and_reread_applet(menu,main_menu);
	}

	/*so that the panel doesn't pop down until we're
	  done with the menu */
	if(IS_BASEP_WIDGET(wpanel)) {
		BASEP_WIDGET(wpanel)->autohide_inhibit = TRUE;
		basep_widget_autohide(BASEP_WIDGET(wpanel));
	}

	BUTTON_WIDGET(menu->button)->ignore_leave = TRUE;
	gtk_grab_remove(menu->button);

	menu->age = 0;
	gtk_menu_popup(GTK_MENU(menu->menu), 0,0, 
		       applet_menu_position,
		       menu->info, bevent->button, bevent->time);
	gdk_event_free((GdkEvent *)bevent);
}

static char *
get_pixmap(char *menudir, int main_menu)
{
	char *pixmap_name;
	if (main_menu) {
		pixmap_name = gnome_unconditional_pixmap_file("gnome-logo-icon-transparent.png");
	} else {
		char *dentry_name;
		GnomeDesktopEntry *item_info;

		dentry_name = g_concat_dir_and_file (menudir,
						     ".directory");
		item_info = gnome_desktop_entry_load (dentry_name);
		g_free (dentry_name);

		if(item_info && item_info->icon)
		  pixmap_name = g_strdup(item_info->icon);
		else
		  pixmap_name =
		    gnome_unconditional_pixmap_file ("gnome-folder.png");

		if(item_info)
		  gnome_desktop_entry_free(item_info);
	}
	return pixmap_name;
}

static Menu *
create_panel_menu (char *menudir, int main_menu,
		   PanelOrientType orient, int main_menu_flags)
{
	Menu *menu;
	
	char *pixmap_name;

	menu = g_new0(Menu,1);

	pixmap_name = get_pixmap(menudir,main_menu);

	menu->main_menu_flags = main_menu_flags;

	/*make the pixmap*/
	menu->button = button_widget_new_from_file (pixmap_name,-1,
						    MENU_TILE,
						    TRUE,orient,
						    _("Menu"));
	gtk_signal_connect_after (GTK_OBJECT (menu->button), "pressed",
				  GTK_SIGNAL_FUNC (menu_button_pressed), menu);
	gtk_signal_connect (GTK_OBJECT (menu->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_menu), menu);
	gtk_widget_show(menu->button);

	/*if we are allowed to be pigs and load all the menus to increase
	  speed, load them*/
	if(global_config.hungry_menus) {
		GSList *list = g_slist_append(NULL,menudir);
		add_menu_widget(menu,list,main_menu,TRUE);
		g_slist_free(list);
	}

	g_free (pixmap_name);

	return menu;
}

static Menu *
create_menu_applet(char *arguments, PanelOrientType orient,
		   int main_menu_flags)
{
	Menu *menu;
	int main_menu;

	char *menu_base = gnome_unconditional_datadir_file ("gnome/apps");
	char *this_menu = get_real_menu_path(arguments,menu_base);

	if (!this_menu) {
		g_free (menu_base);
		return NULL;
	}

	if(!gnome_folder) {
		gnome_folder =
			gnome_unconditional_pixmap_file("gnome-folder.png");
		if (!g_file_exists (gnome_folder)) {
			g_free (gnome_folder);
			gnome_folder = NULL;
		}
	}

	main_menu = (!arguments || !*arguments || (strcmp (arguments, ".") == 0));

	menu = create_panel_menu (this_menu, main_menu, orient,main_menu_flags);
	menu->path=g_strdup((arguments && *arguments)?arguments:".");

	gtk_object_set_user_data(GTK_OBJECT(menu->button),menu);

	g_free(menu_base);
	g_free (this_menu);
	return menu;
}

void
set_menu_applet_orient(Menu *menu, PanelOrientType orient)
{
	g_return_if_fail(menu!=NULL);

	button_widget_set_params(BUTTON_WIDGET(menu->button),
				 MENU_TILE,TRUE,orient);
}

static void
properties_apply_callback(GtkWidget *widget, int page, gpointer data)
{
	Menu *menu = data;
	GtkWidget *main_menu = gtk_object_get_data(GTK_OBJECT(widget), "main_menu");
	GtkWidget *system_off = gtk_object_get_data(GTK_OBJECT(widget), "system_off");
	GtkWidget *system_sub = gtk_object_get_data(GTK_OBJECT(widget), "system_sub");
	GtkWidget *user_off = gtk_object_get_data(GTK_OBJECT(widget), "user_off");
	GtkWidget *user_sub = gtk_object_get_data(GTK_OBJECT(widget), "user_sub");
	GtkWidget *redhat_off = gtk_object_get_data(GTK_OBJECT(widget), "redhat_off");
	GtkWidget *redhat_sub = gtk_object_get_data(GTK_OBJECT(widget), "redhat_sub");
 	GtkWidget *kde_off = gtk_object_get_data(GTK_OBJECT(widget), "kde_off");
 	GtkWidget *kde_sub = gtk_object_get_data(GTK_OBJECT(widget), "kde_sub");
	GtkWidget *debian_off = gtk_object_get_data(GTK_OBJECT(widget), "debian_off");
	GtkWidget *debian_sub = gtk_object_get_data(GTK_OBJECT(widget), "debian_sub");
	GtkWidget *pathentry = gtk_object_get_data(GTK_OBJECT(widget), "path");
	char *s;

	if (page != -1)
		return;
	
	applet_remove_callback(menu->info,"edit_menus");
	if(GTK_TOGGLE_BUTTON(main_menu)->active) {
		char *tmp;
		g_free(menu->path);
		menu->path = g_strdup(".");
		if((tmp = gnome_is_program_in_path("gmenu")))  {
			g_free(tmp);
			applet_add_callback(menu->info,"edit_menus",
					    NULL,
					    _("Edit menus..."));
		}
	} else {
		g_free(menu->path);
		s = gnome_file_entry_get_full_path(GNOME_FILE_ENTRY(pathentry),
						   TRUE);
		if(!s) {
			g_warning(_("Can't open directory, using main menu!"));
			menu->path = g_strdup(".");
		} else if(!*s)
			menu->path = g_strdup(".");
		else
			menu->path = g_strdup(s);
	}
	if(GTK_TOGGLE_BUTTON(system_off)->active)
		menu->main_menu_flags &=~ (MAIN_MENU_SYSTEM|MAIN_MENU_SYSTEM_SUB);
	else if(GTK_TOGGLE_BUTTON(system_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_SYSTEM|MAIN_MENU_SYSTEM_SUB;
	else {
		menu->main_menu_flags |= MAIN_MENU_SYSTEM;
		menu->main_menu_flags &=~ MAIN_MENU_SYSTEM_SUB;
	}
	if(GTK_TOGGLE_BUTTON(user_off)->active)
		menu->main_menu_flags &=~ (MAIN_MENU_USER|MAIN_MENU_USER_SUB);
	else if(GTK_TOGGLE_BUTTON(user_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_USER|MAIN_MENU_USER_SUB;
	else {
		menu->main_menu_flags |= MAIN_MENU_USER;
		menu->main_menu_flags &=~ MAIN_MENU_USER_SUB;
	}
	if(GTK_TOGGLE_BUTTON(redhat_off)->active)
		menu->main_menu_flags &=~ (MAIN_MENU_REDHAT|MAIN_MENU_REDHAT_SUB);
	else if(GTK_TOGGLE_BUTTON(redhat_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_REDHAT|MAIN_MENU_REDHAT_SUB;
	else {
		menu->main_menu_flags |= MAIN_MENU_REDHAT;
		menu->main_menu_flags &=~ MAIN_MENU_REDHAT_SUB;
	}
	if(GTK_TOGGLE_BUTTON(debian_off)->active)
		menu->main_menu_flags &=~ (MAIN_MENU_DEBIAN|MAIN_MENU_DEBIAN_SUB);
	else if(GTK_TOGGLE_BUTTON(debian_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_DEBIAN|MAIN_MENU_DEBIAN_SUB;
	else {
		menu->main_menu_flags |= MAIN_MENU_DEBIAN;
		menu->main_menu_flags &=~ MAIN_MENU_DEBIAN_SUB;
	}
	if(GTK_TOGGLE_BUTTON(kde_off)->active)
		menu->main_menu_flags &=~ (MAIN_MENU_KDE|MAIN_MENU_KDE_SUB);
	else if(GTK_TOGGLE_BUTTON(kde_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_KDE|MAIN_MENU_KDE_SUB;
	else {
		menu->main_menu_flags |= MAIN_MENU_KDE;
		menu->main_menu_flags &=~ MAIN_MENU_KDE_SUB;
	}

	if(menu->menu)
		gtk_widget_destroy(menu->menu);	
	menu->menu = NULL;

	{
		char *menu_base = gnome_unconditional_datadir_file ("gnome/apps");
		char *this_menu = get_real_menu_path(menu->path,menu_base);
		GSList *list = g_slist_append(NULL,this_menu);
		char *pixmap_name = get_pixmap(this_menu,
					       strcmp(menu->path,".")==0);
		/*make the pixmap*/
		button_widget_set_pixmap_from_file (BUTTON_WIDGET(menu->button),
						    pixmap_name, -1);
		g_free(pixmap_name);
		
		add_menu_widget(menu,list, strcmp(menu->path,".")==0, TRUE);
		
		g_free(menu_base);
		g_free(this_menu);

		g_slist_free(list);
	}
}

static void
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	gtk_object_set_data(GTK_OBJECT(menu->button),
			    MENU_PROPERTIES,NULL);
}

static void
toggle_prop(GtkWidget *widget, gpointer data)
{
	GnomePropertyBox *box = GNOME_PROPERTY_BOX (data);

	if(GTK_TOGGLE_BUTTON(widget)->active)
		gnome_property_box_changed (box);
}

static void
toggle_main_menu(GtkWidget *widget, gpointer data)
{
	GnomePropertyBox *box = GNOME_PROPERTY_BOX (data);
	GtkWidget *main_frame = gtk_object_get_data(GTK_OBJECT(box),
						    "main_frame");
	GtkWidget *normal_frame = gtk_object_get_data(GTK_OBJECT(box),
						      "normal_frame");
	if(GTK_TOGGLE_BUTTON(widget)->active) {
		gtk_widget_set_sensitive(main_frame,TRUE);
		gtk_widget_set_sensitive(normal_frame,FALSE);
		gnome_property_box_changed (box);
	}
}
static void
toggle_normal_menu(GtkWidget *widget, gpointer data)
{
	GnomePropertyBox *box = GNOME_PROPERTY_BOX (data);
	GtkWidget *main_frame = gtk_object_get_data(GTK_OBJECT(box),
						    "main_frame");
	GtkWidget *normal_frame = gtk_object_get_data(GTK_OBJECT(box),
						      "normal_frame");
	if(GTK_TOGGLE_BUTTON(widget)->active) {
		gtk_widget_set_sensitive(main_frame,FALSE);
		gtk_widget_set_sensitive(normal_frame,TRUE);
		gnome_property_box_changed (box);
	}
}

static void
add_menu_type_options(GtkObject *dialog, GtkTable *table, int row,
		      char *title,char *ident, int on, int sub)
{
	char *p;
	GtkWidget *w;
	GtkWidget *rb;

	/*sanity checks*/
	if(!on)
		sub = FALSE;
	
	w = gtk_label_new(title);
	gtk_table_attach_defaults(table,w,0,1,row,row+1);
	
	rb = w = gtk_radio_button_new_with_label (NULL, _("Off"));
	gtk_table_attach_defaults(table,w,3,4,row,row+1);
	p = g_strconcat(ident,"_off",NULL);
	gtk_object_set_data(dialog,p,w);
	g_free(p);
	if(!on)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    dialog);
	
	w = gtk_radio_button_new_with_label (gtk_radio_button_group(GTK_RADIO_BUTTON(rb)),
					     _("In a submenu"));
	gtk_table_attach_defaults(table,w,2,3,row,row+1);
	p = g_strconcat(ident,"_sub",NULL);
	gtk_object_set_data(dialog,p,w);
	g_free(p);
	if(sub)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    dialog);
	
	w = gtk_radio_button_new_with_label (gtk_radio_button_group(GTK_RADIO_BUTTON(rb)),
					     _("On the main menu"));
	gtk_table_attach_defaults(table,w,1,2,row,row+1);
	if(on && !sub)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    dialog);
}
	

static GtkWidget *
create_properties_dialog(Menu *menu)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *box;
	GtkWidget *table;
	GtkWidget *w,*w2;
	GtkWidget *f;
	GtkWidget *t;

	dialog = gnome_property_box_new();
	gtk_window_set_wmclass(GTK_WINDOW(dialog),
			       "menu_properties","Panel");
	gtk_window_set_title(GTK_WINDOW(dialog), _("Menu properties"));
	/*gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	vbox = gtk_vbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(vbox),GNOME_PAD_SMALL);

	f = gtk_frame_new(_("Menu type"));
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(box),GNOME_PAD_SMALL);
	gtk_container_add(GTK_CONTAINER(f),box);
	
	w = gtk_radio_button_new_with_label (NULL, _("Main menu"));
	gtk_object_set_data(GTK_OBJECT(dialog),"main_menu",w);
	if(!menu->path || strcmp(menu->path,".")==0)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_main_menu), 
			    dialog);
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);

	w2 = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (w)),
			  _("Normal menu"));
	if(menu->path && strcmp(menu->path,".")!=0)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w2), TRUE);
	gtk_signal_connect (GTK_OBJECT (w2), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_normal_menu), 
			    dialog);
	gtk_box_pack_start(GTK_BOX(box),w2,TRUE,TRUE,0);


	f = gtk_frame_new(_("Main menu"));
	if(menu->path && strcmp(menu->path,".")!=0)
		gtk_widget_set_sensitive(f,FALSE);
	gtk_object_set_data(GTK_OBJECT(dialog),"main_frame",f);
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	table = gtk_table_new(5,4,FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table),GNOME_PAD_SMALL);
	gtk_container_add(GTK_CONTAINER(f),table);

	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),0,
			      _("Programs: "),"system",
			      menu->main_menu_flags&MAIN_MENU_SYSTEM,
			      menu->main_menu_flags&MAIN_MENU_SYSTEM_SUB);
	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),1,
			      _("Favorites: "),"user",
			      menu->main_menu_flags&MAIN_MENU_USER,
			      menu->main_menu_flags&MAIN_MENU_USER_SUB);
	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),2,
			      _("AnotherLevel menu (if found): "),"redhat",
			      menu->main_menu_flags&MAIN_MENU_REDHAT,
			      menu->main_menu_flags&MAIN_MENU_REDHAT_SUB);
 	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),3,
 			      _("KDE menu (if found): "),"kde",
 			      menu->main_menu_flags&MAIN_MENU_KDE,
 			      menu->main_menu_flags&MAIN_MENU_KDE_SUB);
	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),4,
			      _("Debian menu (if found): "),"debian",
			      menu->main_menu_flags&MAIN_MENU_DEBIAN,
			      menu->main_menu_flags&MAIN_MENU_DEBIAN_SUB);
	
	f = gtk_frame_new(_("Normal menu"));
	if(!menu->path || strcmp(menu->path,".")==0)
		gtk_widget_set_sensitive(f,FALSE);
	gtk_object_set_data(GTK_OBJECT(dialog),"normal_frame",f);
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(box),GNOME_PAD_SMALL);
	gtk_container_add(GTK_CONTAINER(f),box);
	
	w = gtk_label_new(_("Menu path"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);

	w = gnome_file_entry_new("menu_path",_("Browse"));
	gnome_file_entry_set_directory(GNOME_FILE_ENTRY(w), TRUE);

	t = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w));
	gtk_object_set_data(GTK_OBJECT(dialog),"path",w);
	if (menu->path)
		gtk_entry_set_text(GTK_ENTRY(t), menu->path);
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);
	gtk_signal_connect_object_while_alive (GTK_OBJECT (t), "changed",
					       GTK_SIGNAL_FUNC(gnome_property_box_changed),
					       GTK_OBJECT(dialog));
	
	gtk_notebook_append_page (GTK_NOTEBOOK(GNOME_PROPERTY_BOX (dialog)->notebook),
				  vbox, gtk_label_new (_("Menu")));
	
	gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
			   (GtkSignalFunc) properties_close_callback,
			   menu);

	gtk_signal_connect(GTK_OBJECT(dialog), "apply",
			   GTK_SIGNAL_FUNC(properties_apply_callback),
			   menu);

	return dialog;
}

void
menu_properties(Menu *menu)
{
	GtkWidget *dialog;

	dialog = gtk_object_get_data(GTK_OBJECT(menu->button),
				     MENU_PROPERTIES);
	if(dialog) {
		gdk_window_raise(dialog->window);
		gtk_widget_show(dialog);
		return;
	}

	dialog = create_properties_dialog(menu);
	gtk_object_set_data(GTK_OBJECT(menu->button),
			    MENU_PROPERTIES,dialog);
	gtk_widget_show_all (dialog);
}

void
load_menu_applet(char *params, int main_menu_flags,
		 PanelWidget *panel, int pos)
{
	Menu *menu;

	menu = create_menu_applet(params, ORIENT_UP,main_menu_flags);

	if(menu) {
		char *tmp;
		register_toy(menu->button,menu,
			     panel,pos,APPLET_MENU);

		menu->info = applets_last->data;

		applet_add_callback(menu->info,"properties",
				    GNOME_STOCK_MENU_PROP,
				    _("Properties..."));
		if(params && strcmp(params,".")==0 &&
		   (tmp = gnome_is_program_in_path("gmenu")))  {
			g_free(tmp);
			applet_add_callback(menu->info,"edit_menus",
					    NULL,
					    _("Edit menus..."));
		}
	}
}

/*menu related utility functions .. also used elswhere*/

void
panel_menu_position (GtkMenu *menu, gint *x, gint *y, gpointer data)
{
	GtkWidget *w = data;
	gint wx, wy;

	g_return_if_fail(w != NULL);

	gdk_window_get_origin (w->window, &wx, &wy);

	gtk_widget_get_pointer(w, x, y);
	if (IS_BASEP_WIDGET (w))
		basep_widget_get_menu_pos(BASEP_WIDGET(w), 
					  GTK_WIDGET(menu),
					  x,y,wx,wy,
					  w->allocation.width,
					  w->allocation.height);
}

void
applet_menu_position (GtkMenu *menu, gint *x, gint *y, gpointer data)
{
	AppletInfo *info = data;
	int wx, wy;
	PanelWidget *panel;
	GtkWidget *w; /*the panel window widget*/

	g_return_if_fail(info != NULL);
	g_return_if_fail(info->widget != NULL);

	panel = PANEL_WIDGET(info->widget->parent);
	g_return_if_fail(panel != NULL);
	
	w = gtk_object_get_data(GTK_OBJECT(panel), PANEL_PARENT);

	gdk_window_get_origin (info->widget->window, &wx, &wy);
	if(GTK_WIDGET_NO_WINDOW(info->widget)) {
		wx += info->widget->allocation.x;
		wy += info->widget->allocation.y;
	}
	*x = *y = 0;
	if (IS_BASEP_WIDGET (w))
		basep_widget_get_menu_pos(BASEP_WIDGET(w),
					  GTK_WIDGET(menu),
					  x,y,wx,wy,
					  info->widget->allocation.width,
					  info->widget->allocation.height);
}

