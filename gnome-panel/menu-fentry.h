#ifndef MENU_FENTRY_H
#define MENU_FENTRY_H

#include <sys/stat.h>
#include <unistd.h>

BEGIN_GNOME_DECLS

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

char * get_applet_goad_id_from_dentry(GnomeDesktopEntry *ii);

GSList * get_files_from_menudir(char *menudir);

FileRec * fr_read_dir(DirRec *dr, char *mdir, struct stat *dstat, int sublevels);
FileRec * fr_replace(FileRec *fr);
FileRec * fr_check_and_reread(FileRec *fr);
FileRec * fr_get_dir(char *mdir);


END_GNOME_DECLS

#endif
