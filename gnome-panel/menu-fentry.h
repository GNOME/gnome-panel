#ifndef MENU_FENTRY_H
#define MENU_FENTRY_H

#include <sys/stat.h>
#include <unistd.h>
#include "gnome-desktop-item.h"

G_BEGIN_DECLS

typedef struct _MenuFinfo MenuFinfo;
typedef struct _FileRec FileRec;
typedef struct _DirRec DirRec;
typedef struct _MFile MFile;

struct _MenuFinfo {
	char *menudir;
	gboolean applets;
	gboolean launcher_add;
	char *dir_name;
	char *pixmap_name;	
	gboolean fake_menu;
	FileRec *fr;
};

struct _MFile {
	char *name;
	gboolean verified;
	time_t mtime;
	gboolean is_dir;
};

enum {
	FILE_REC_FILE, /*.desktop file record*/
	FILE_REC_DIR,  /*directory*/
	FILE_REC_EXTRA, /*just check the mtime*/
	FILE_REC_SEP
};

struct _FileRec {
	int type;
	char *name;
	char *comment;
	char *fullname;
	char *icon;
	char *goad_id;
	char *tryexec_path;
	DirRec *parent;
	time_t mtime;
	time_t last_stat;
};

struct _DirRec {
	FileRec frec;
	time_t ditemmtime;
	time_t ditemlast_stat;
	gboolean force_reread;
	GSList *tryexecs;
	GSList *recs; /*records for directories*/
	GSList *mfl;  /*records of menus using this record*/
};

char * get_applet_goad_id_from_dentry(GnomeDesktopItem *ii);

GSList * get_mfiles_from_menudir(const char *menudir);

FileRec * fr_read_dir(DirRec *dr, const char *muri, time_t mtime, int sublevels);
FileRec * fr_replace(FileRec *fr);
FileRec * fr_check_and_reread(FileRec *fr);
FileRec * fr_get_dir(const char *mdir);
void fr_force_reread(void);
GSList* fr_get_all_dirs (void);

void free_mfile (MFile *mfile);
void free_mfile_list (GSList *list);

G_END_DECLS

#endif
